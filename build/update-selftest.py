#!/usr/bin/env python3
"""app/update.py against a fake GitHub on localhost: no handheld, no network.
   python3 build/update-selftest.py   (from the repo; exit 0 when every case passes)"""
import hashlib
import http.server
import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import zipfile

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "app")


def manifest(d):
    m = {}
    with open(os.path.join(d, "manifest")) as f:
        for line in f:
            k, _, v = line.strip().partition("=")
            m.setdefault(k, v)
    return m


MAN = manifest(SRC)
ID = MAN["update_root"]
CUR = MAN["app_version"]
NEXT = ".".join(CUR.split(".")[:-1] + [str(int(CUR.split(".")[-1]) + 1)])
fails = []


def bad(msg):
    fails.append(msg)
    print("FAIL " + msg)


T = tempfile.mkdtemp()
WEB = os.path.join(T, "web")
os.makedirs(WEB)


class H(http.server.SimpleHTTPRequestHandler):
    def __init__(self, *a, **k):
        super().__init__(*a, directory=WEB, **k)

    def log_message(self, *a):
        pass


srv = http.server.ThreadingHTTPServer(("127.0.0.1", 0), H)
threading.Thread(target=srv.serve_forever, daemon=True).start()
BASE = "http://127.0.0.1:%d/" % srv.server_address[1]


def package(version, name, mutate=None):
    stage = os.path.join(T, "stage")
    shutil.rmtree(stage, ignore_errors=True)
    shutil.copytree(SRC, os.path.join(stage, ID), ignore=shutil.ignore_patterns("ui"))
    os.makedirs(os.path.join(stage, ID, "ui"))
    with open(os.path.join(stage, ID, "ui", "marker"), "w") as f:
        f.write(version)
    lay = json.load(open(os.path.join(stage, ID, "app.json")))
    if lay.get("check"):
        os.makedirs(os.path.dirname(os.path.join(stage, ID, lay["check"])), exist_ok=True)
        open(os.path.join(stage, ID, lay["check"]), "w").write("bin")
    m = open(os.path.join(stage, ID, "manifest")).read().replace("app_version=" + CUR, "app_version=" + version)
    open(os.path.join(stage, ID, "manifest"), "w").write(m)
    if mutate:
        mutate(os.path.join(stage, ID))
    z = os.path.join(WEB, name)
    with zipfile.ZipFile(z, "w") as zf:
        for dp, _, fs in os.walk(stage):
            for f in fs:
                full = os.path.join(dp, f)
                zf.write(full, os.path.relpath(full, stage))
    return hashlib.sha256(open(z, "rb").read()).hexdigest()


def release(version, assets):
    r = {"tag_name": "v" + version, "draft": False, "prerelease": False, "assets": [
        dict({"name": n, "browser_download_url": BASE + n, "size": os.path.getsize(os.path.join(WEB, n)) if os.path.exists(os.path.join(WEB, n)) else 0}, **x)
        for n, x in assets]}
    json.dump(r, open(os.path.join(WEB, "latest.json"), "w"))


def installed():
    apps = os.path.join(T, "apps")
    shutil.rmtree(apps, ignore_errors=True)
    shutil.copytree(SRC, os.path.join(apps, ID), ignore=shutil.ignore_patterns("ui"))
    os.makedirs(os.path.join(apps, ID, "ui"))
    open(os.path.join(apps, ID, "ui", "marker"), "w").write(CUR)
    return os.path.join(apps, ID)


def run(app, *args, api=None):
    env = dict(os.environ, PIXEL_UPDATE_API=api or BASE + "latest.json", PIXEL_UPDATE_STATE=os.path.join(T, "state"),
               BANANADS_MP_CONF=os.path.join(T, "systems.conf"))
    p = subprocess.run([sys.executable, os.path.join(app, "update.py")] + list(args), env=env,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    out = dict(l.partition("=")[::2] for l in p.stdout.decode().splitlines() if "=" in l)
    st = dict(l.partition("=")[::2] for l in open(os.path.join(T, "state")).read().splitlines())
    if st.get("state") != out.get("state"):
        bad("state file says %s, stdout %s" % (st.get("state"), out.get("state")))
    return out


def marker(app):
    return open(os.path.join(app, "ui", "marker")).read()


def leftovers(app):
    p = os.path.dirname(app)
    return sorted(n for n in os.listdir(p) if n != ID)


zipname = "%s-%s.zip" % (ID, NEXT)
try:
    # up to date
    app = installed()
    package(CUR, "%s-%s.zip" % (ID, CUR))
    release(CUR, [("%s-%s.zip" % (ID, CUR), {})])
    o = run(app, "check")
    if o.get("state") != "uptodate":
        bad("same version: %s" % o)

    # newer: available, then installed (GitHub's digest)
    sha = package(NEXT, zipname)
    release(NEXT, [(zipname, {"digest": "sha256:" + sha})])
    o = run(app, "check")
    if o.get("state") != "available" or o.get("latest") != NEXT:
        bad("newer: %s" % o)
    open(os.path.join(app, "update.conf"), "w").write("api=http://127.0.0.1:9/none\n")
    o = run(app, "install", "--dry-run")
    if o.get("state") != "done" or marker(app) != CUR:
        bad("dry run changed something: %s" % o)
    o = run(app, "install")
    if o.get("state") != "done" or marker(app) != NEXT or manifest(app)["app_version"] != NEXT:
        bad("install: %s" % o)
    if not os.path.exists(os.path.join(app, "VERSION")) or open(os.path.join(app, "VERSION")).read().strip() != NEXT:
        bad("VERSION file")
    if leftovers(app):
        bad("left behind: %s" % leftovers(app))

    # damaged download: nothing changes
    app = installed()
    release(NEXT, [(zipname, {"digest": "sha256:" + "0" * 64})])
    o = run(app, "install")
    if o.get("state") != "failed" or o.get("msg") != "Download damaged" or marker(app) != CUR or leftovers(app):
        bad("damaged: %s %s" % (o, leftovers(app)))

    # no digest: SHA256SUMS beside it; no checksum at all: refused
    open(os.path.join(WEB, "SHA256SUMS"), "w").write("%s  %s\n" % (sha, zipname))
    release(NEXT, [(zipname, {}), ("SHA256SUMS", {})])
    o = run(app, "install")
    if o.get("state") != "done" or marker(app) != NEXT:
        bad("SHA256SUMS: %s" % o)
    app = installed()
    release(NEXT, [(zipname, {})])
    o = run(app, "install")
    if o.get("state") != "failed" or "checksum" not in o.get("msg", "") or marker(app) != CUR:
        bad("no checksum: %s" % o)

    # the zip says another version: refused
    sha2 = package(CUR, zipname)
    release(NEXT, [(zipname, {"digest": "sha256:" + sha2})])
    o = run(app, "install")
    if o.get("state") != "failed" or o.get("msg") != "Wrong version inside" or marker(app) != CUR or leftovers(app):
        bad("wrong version: %s" % o)

    # a zip with a path outside: refused
    z = os.path.join(WEB, zipname)
    with zipfile.ZipFile(z, "w") as zf:
        zf.writestr("../evil", "x")
    release(NEXT, [(zipname, {"digest": "sha256:" + hashlib.sha256(open(z, "rb").read()).hexdigest()})])
    o = run(app, "install")
    if o.get("state") != "failed" or marker(app) != CUR or os.path.exists(os.path.join(T, "evil")):
        bad("zip slip: %s" % o)

    # a release for one BananaOS beside one for all: this one's pick is the general one here (no /etc/os-release
    # 0.0.0), and a release without any zip for it says so
    release(NEXT, [("%s-%s-bananaos-0.0.0.zip" % (ID, NEXT), {})])
    o = run(app, "check")
    if o.get("state") != "failed" or o.get("msg") != "Not for this console yet":
        bad("no asset: %s" % o)

    # this BananaOS's own zip wins over the general one
    sys.path.insert(0, SRC)
    sys.dont_write_bytecode = True
    import update as U
    here = U.os_version()
    if here:
        own = "%s-%s-bananaos-%s.zip" % (ID, NEXT, here)
        sha_own = package(NEXT, own, lambda d: open(os.path.join(d, "ui", "marker"), "w").write("own"))
        sha = package(NEXT, zipname)
        release(NEXT, [(zipname, {"digest": "sha256:" + sha}), (own, {"digest": "sha256:" + sha_own})])
        o = run(app, "install")
        if o.get("state") != "done" or marker(app) != "own":
            bad("per-BananaOS zip: %s" % o)
        app = installed()

    # no network
    o = run(app, "check", api="http://127.0.0.1:9/latest.json")
    if o.get("state") != "offline":
        bad("offline: %s" % o)
    o = run(app, "check", api=BASE + "missing.json")
    if o.get("state") != "failed" or o.get("msg") != "No release yet":
        bad("404: %s" % o)
finally:
    srv.shutdown()
    shutil.rmtree(T, ignore_errors=True)

print("update selftest: %s" % ("ok" if not fails else "%d failed" % len(fails)))
sys.exit(1 if fails else 0)
