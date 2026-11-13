#!/usr/bin/env python3
"""The app's own updater: the newest GitHub release, checked and installed the way BananaOS's Store does it.

  python3 update.py check              the newest release against the installed version
  python3 update.py install [--dry-run]  downloads, checks and installs it (--dry-run stops before anything changes)

It prints key=value lines and keeps the same lines in $STATE (the UI reads that file while it runs):
  state=checking|uptodate|available|downloading|verifying|installing|done|failed|offline
  current=<installed>  latest=<newest>  pct=<0-100>  msg=<why, for failed>

What it reads from the app's manifest (beside this file):
  app_version     the installed version
  update_repo     owner/name on GitHub
  update_asset    the release's zip: one name or several, space-separated, the first one the release has wins.
                  {version}, {bananaos} (this system's VERSION_ID) and {model} (rgds, rgds_plus) are filled in,
                  so a release with one zip per BananaOS version can sit beside one zip for all.
  update_root     the one folder in the zip (the app's own folder)

The install is the Store's: the zip is checked against the sha256 GitHub lists for the asset (else a .sha256 file
or a SHA256SUMS beside it; no checksum, no install), unpacked beside the app as <app>.new, the app.json's "keep"
moved over, the folders swapped with two renames, the app.json's "post_install" run. Anything that fails before the
swap leaves the installed app as it was; a failed swap puts it back.

Debug: PIXEL_UPDATE_API (or api=<url> in update.conf beside this file) points the check at another release JSON
(GitHub's releases/latest format), e.g. a local test server.
"""
import hashlib
import json
import os
import re
import shutil
import stat
import subprocess
import sys
import urllib.error
import urllib.request
import zipfile

APP = os.path.dirname(os.path.abspath(__file__))
API = "https://api.github.com/repos/{repo}/releases/latest"
UA = "pixelado-app-updater"
VER_RE = re.compile(r"^[0-9]+(\.[0-9]+)*$")
STATE = os.environ.get("PIXEL_UPDATE_STATE", "")
_state = {}


class Fail(Exception):
    def __init__(self, msg, state="failed"):
        Exception.__init__(self, msg)
        self.state = state


def say(**kv):
    """the state, on stdout and in $STATE (one rename, so the UI never reads half of it)"""
    _state.update({k: str(v) for k, v in kv.items()})
    for k, v in kv.items():
        print("%s=%s" % (k, v), flush=True)
    if STATE:
        tmp = STATE + ".tmp"
        try:
            with open(tmp, "w") as f:
                for k, v in _state.items():
                    f.write("%s=%s\n" % (k, v.replace("\n", " ")))
            os.replace(tmp, STATE)
        except OSError:
            pass


def manifest(d=APP):
    m = {}
    try:
        with open(os.path.join(d, "manifest"), encoding="utf-8", errors="replace") as f:
            for line in f:
                k, eq, v = line.strip().partition("=")
                if eq and k not in m:
                    m[k] = v.strip()
    except OSError:
        pass
    return m


def app_json(d):
    try:
        with open(os.path.join(d, "app.json"), encoding="utf-8") as f:
            j = json.load(f)
        return j if isinstance(j, dict) else {}
    except (OSError, ValueError):
        return {}


def vkey(v):
    return [int(x) for x in re.findall(r"\d+", v or "")]


def newer(a, b):
    return vkey(a) > vkey(b)


def os_version():
    try:
        with open("/etc/os-release") as f:
            m = re.search(r'^VERSION_ID="?([^"\n]*)"?$', f.read(), re.M)
            return m.group(1) if m else ""
    except OSError:
        return ""


def model():
    try:
        with open("/proc/device-tree/model", "rb") as f:
            s = f.read().replace(b"\0", b"").decode("utf-8", "replace")
    except OSError:
        return ""
    return "rgds_plus" if "Plus" in s else "rgds" if s == "Anbernic RG DS" else ""


def api_url(man):
    u = os.environ.get("PIXEL_UPDATE_API", "")
    if not u:
        try:
            with open(os.path.join(APP, "update.conf")) as f:
                for line in f:
                    if line.startswith("api="):
                        u = line[4:].strip()
        except OSError:
            pass
    if u:
        return u
    repo = man.get("update_repo", "")
    if not re.match(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$", repo):
        raise Fail("No update source in the manifest")
    return API.format(repo=repo)


def get(url, timeout=20):
    req = urllib.request.Request(url, headers={"User-Agent": UA, "Accept": "application/vnd.github+json"})
    try:
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.read()
    except urllib.error.HTTPError as e:
        if e.code == 404:
            raise Fail("No release yet")
        if e.code in (403, 429):
            raise Fail("GitHub is busy, try later")
        raise Fail("Server error %d" % e.code)
    except (urllib.error.URLError, OSError):           # no Wi-Fi, no DNS, no answer
        raise Fail("No network", "offline")


def release(man):
    """{version, name, url, sha256 or sums} of the newest release"""
    body = get(api_url(man))
    try:
        r = json.loads(body)
    except ValueError:
        raise Fail("Bad answer from GitHub")
    if not isinstance(r, dict):
        raise Fail("Bad answer from GitHub")
    tag = str(r.get("tag_name") or "")
    v = tag[1:] if tag[:1] in "vV" else tag
    if not VER_RE.match(v) or r.get("draft") or r.get("prerelease"):
        raise Fail("No release yet")
    assets = {str(a.get("name")): a for a in (r.get("assets") or []) if isinstance(a, dict)}
    pats = (man.get("update_asset") or "").split()
    if not pats:
        raise Fail("No update source in the manifest")
    fill = {"version": v, "bananaos": os_version(), "model": model()}
    name = None
    for p in pats:
        try:
            n = p.format(**fill)
        except (KeyError, IndexError, ValueError):
            continue
        if n in assets:
            name = n
            break
    rel = {"version": v, "name": name}
    if name:
        a = assets[name]
        rel["url"] = str(a.get("browser_download_url") or "")
        rel["size"] = int(a.get("size") or 0)
        d = str(a.get("digest") or "")
        if re.match(r"^sha256:[0-9a-fA-F]{64}$", d):
            rel["sha256"] = d[7:].lower()
        for s in (name + ".sha256", "SHA256SUMS"):
            if s in assets and assets[s].get("browser_download_url"):
                rel["sums"] = (s, str(assets[s]["browser_download_url"]))
                break
    return rel


def cmd_check():
    man = manifest()
    cur = man.get("app_version", "")
    say(state="checking", current=cur, latest="", msg="")
    rel = release(man)
    if not newer(rel["version"], cur):
        say(state="uptodate", latest=rel["version"])
    elif not rel["name"]:
        say(state="failed", latest=rel["version"], msg="Not for this console yet")
    else:
        say(state="available", latest=rel["version"])


def download(url, dest, size):
    req = urllib.request.Request(url, headers={"User-Agent": UA})
    h = hashlib.sha256()
    got, last = 0, -1
    try:
        with urllib.request.urlopen(req, timeout=30) as r, open(dest, "wb") as f:
            total = int(r.headers.get("Content-Length") or size or 0)
            while True:
                b = r.read(1 << 16)
                if not b:
                    break
                f.write(b)
                h.update(b)
                got += len(b)
                pct = got * 100 // total if total else 0
                if pct != last:
                    say(pct=min(pct, 100))
                    last = pct
    except urllib.error.HTTPError as e:
        raise Fail("Download failed (%d)" % e.code)
    except (urllib.error.URLError, OSError):
        raise Fail("Download failed", "offline")
    return h.hexdigest()


def want_sha(rel):
    if rel.get("sha256"):
        return rel["sha256"]
    if rel.get("sums"):
        s, url = rel["sums"]
        body = get(url, 30).decode("ascii", "replace")
        for line in body.splitlines():
            p = line.split()
            if not p:
                continue
            if s.endswith(".sha256") and re.match(r"^[0-9a-fA-F]{64}$", p[0]):
                return p[0].lower()
            if len(p) >= 2 and p[1].lstrip("*") == rel["name"] and re.match(r"^[0-9a-fA-F]{64}$", p[0]):
                return p[0].lower()
    raise Fail("No checksum published for it")


def safe_rel(p):
    p = str(p)
    if not p or p.startswith("/") or "\\" in p or ".." in p.split("/"):
        raise Fail("Package damaged")
    return p


def extract(pkg, into):
    """the Store's unzip: no paths outside, no links, the permissions the zip was made with"""
    try:
        with zipfile.ZipFile(pkg) as z:
            members = z.infolist()
            for m in members:
                safe_rel(m.filename.rstrip("/") or "x")
                if stat.S_ISLNK(m.external_attr >> 16):
                    raise Fail("Package damaged")
            for m in members:
                dest = os.path.join(into, m.filename)
                if m.is_dir():
                    os.makedirs(dest, exist_ok=True)
                    continue
                os.makedirs(os.path.dirname(dest), exist_ok=True)
                with z.open(m) as src, open(dest, "wb") as f:
                    shutil.copyfileobj(src, f, 1 << 20)
                mode = (m.external_attr >> 16) & 0o777
                if mode:
                    os.chmod(dest, mode | 0o600)
    except (zipfile.BadZipFile, EOFError, OSError):
        raise Fail("Package damaged")


def status(d):
    """what the app's status.sh says about a copy of it (supported=, reason=), {} without one"""
    s = os.path.join(d, "status.sh")
    if not os.path.isfile(s):
        return {}
    try:
        out = subprocess.run(["sh", s], cwd=d, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                             timeout=60).stdout.decode("utf-8", "replace")
    except (OSError, subprocess.SubprocessError):
        return {}
    return dict(line.partition("=")[::2] for line in out.splitlines() if "=" in line)


def busy():
    """the installed app's can-update.sh (when it has one): its first line is why not now"""
    s = os.path.join(APP, "can-update.sh")
    if not os.path.isfile(s):
        return ""
    r = subprocess.run(["sh", s], cwd=APP, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if r.returncode == 0:
        return ""
    return (r.stdout.decode("utf-8", "replace").strip().splitlines() or ["Busy, try later"])[0]


def cmd_install(dry):
    man = manifest()
    cur = man.get("app_version", "")
    root = man.get("update_root") or os.path.basename(APP)
    say(state="checking", current=cur, latest="", pct=0, msg="")
    why = busy()
    if why:
        raise Fail(why)
    rel = release(man)
    v = rel["version"]
    say(latest=v)
    if not newer(v, cur) and not os.environ.get("PIXEL_UPDATE_FORCE"):
        say(state="uptodate")
        return
    if not rel["name"] or not rel.get("url"):
        raise Fail("Not for this console yet")
    dest = APP
    parent = os.path.dirname(dest)
    work = os.path.join(parent, "." + os.path.basename(dest) + ".update")
    new, old = dest + ".new", dest + ".old"
    shutil.rmtree(work, ignore_errors=True)
    try:
        os.makedirs(work)
        st = os.statvfs(parent)
        if rel.get("size") and st.f_bavail * st.f_frsize < rel["size"] * 4:
            raise Fail("Not enough space")
        say(state="downloading", pct=0)
        pkg = os.path.join(work, "pkg.zip")
        got = download(rel["url"], pkg, rel.get("size", 0))
        say(state="verifying", pct=100)
        if got != want_sha(rel):
            raise Fail("Download damaged")
        x = os.path.join(work, "x")
        extract(pkg, x)
        src = os.path.join(x, safe_rel(root))
        if not os.path.isdir(src):
            raise Fail("Package incomplete")
        lay = app_json(src)
        check = lay.get("check")
        if check and not os.path.exists(os.path.join(src, safe_rel(check))):
            raise Fail("Package incomplete")
        if manifest(src).get("app_version") != v:
            raise Fail("Wrong version inside")
        for dp, _, fs in os.walk(src):
            for f in fs:
                if f.endswith(".sh"):
                    os.chmod(os.path.join(dp, f), 0o755)
        if check:
            os.chmod(os.path.join(src, check), 0o755)
        vf = os.path.join(src, "VERSION")                 # the Store's installed version, kept in step
        if not os.path.exists(vf):
            with open(vf, "w") as f:
                f.write(v + "\n")
        # a release that wouldn't run here where this one does (another BananaOS): not installed
        was, now = status(dest), status(src)
        if was.get("supported") == "yes" and now.get("supported") == "no":
            raise Fail(now.get("reason") or "Not for this BananaOS")
        if dry:
            say(state="done", msg="dry run: nothing installed")
            return
        say(state="installing")
        shutil.rmtree(new, ignore_errors=True)
        shutil.rmtree(old, ignore_errors=True)
        shutil.move(src, new)
        for k in lay.get("keep") or []:                    # what an update leaves: the app's own settings
            k = safe_rel(k)
            if os.path.lexists(os.path.join(dest, k)):
                shutil.rmtree(os.path.join(new, k), ignore_errors=True)
                if os.path.lexists(os.path.join(new, k)):
                    os.remove(os.path.join(new, k))
                os.makedirs(os.path.dirname(os.path.join(new, k)), exist_ok=True)
                if os.path.isdir(os.path.join(dest, k)):
                    shutil.copytree(os.path.join(dest, k), os.path.join(new, k), symlinks=True)
                else:
                    shutil.copy2(os.path.join(dest, k), os.path.join(new, k), follow_symlinks=False)
        os.sync()
        os.replace(dest, old)
        try:
            os.replace(new, dest)
        except OSError:
            os.replace(old, dest)
            raise Fail("Could not install it")
        os.sync()
        warn = ""
        post = lay.get("post_install")
        if post:
            r = subprocess.run(["sh", os.path.join(dest, safe_rel(post))], cwd=dest, stdout=subprocess.PIPE,
                               stderr=subprocess.STDOUT,
                               env=dict(os.environ, RNDS_STORE_APP=os.path.basename(dest), RNDS_STORE_VERSION=v))
            if r.returncode != 0:
                warn = (r.stdout.decode("utf-8", "replace").strip().splitlines() or ["setup failed"])[-1]
        shutil.rmtree(old, ignore_errors=True)
        say(state="done", current=v, msg=warn)
    finally:
        shutil.rmtree(work, ignore_errors=True)
        shutil.rmtree(new, ignore_errors=True)


def main():
    args = sys.argv[1:]
    if not args or args[0] not in ("check", "install"):
        print("usage: update.py check | install [--dry-run]", file=sys.stderr)
        return 2
    try:
        if args[0] == "check":
            cmd_check()
        else:
            cmd_install("--dry-run" in args)
    except Fail as e:
        say(state=e.state, msg=str(e))
        return 1
    except Exception as e:                                 # never a traceback for the UI: one line it can show
        say(state="failed", msg="%s" % (e.__class__.__name__,))
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
