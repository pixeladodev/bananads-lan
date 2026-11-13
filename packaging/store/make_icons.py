#!/usr/bin/env python3
"""The icons of Pixelado Dev's BananaOS apps, drawn pixel by pixel in BananaOS's palette: BananaDS LAN (a dual-screen
   handheld sending Wi-Fi waves) and Direct Link (two handhelds linked). Shared as it is by both repos.
     python3 make_icons.py OUT_DIR [bananads-lan|direct-link]
   writes OUT_DIR/icon.png (32 x 32 RGBA: the Store's icon) and OUT_DIR/tile.png (320 x 288: the Apps shelf's picture,
   the icon 8x), for the app named, else <app>-icon.png and <app>-tile.png for both. Needs Pillow."""
import sys
from PIL import Image

INK = (23, 22, 28, 255)          # the outline (BananaOS's screenBg)
SHELL = (232, 226, 211, 255)     # the handheld (edge)
SHELL_HI = (255, 250, 238, 255)
SHELL_LO = (169, 162, 150, 255)
HINGE = (107, 101, 89, 255)
GLASS = (36, 44, 66, 255)
GLASS_HI = (74, 159, 224, 255)   # the blue accent
YEL = (240, 180, 41, 255)        # the yellow accent
YEL_HI = (255, 230, 138, 255)
YEL_LO = (201, 143, 18, 255)
GREEN = (86, 191, 107, 255)
RED = (232, 87, 77, 255)


class Pix:
    def __init__(self, w=32, h=32):
        self.im = Image.new("RGBA", (w, h), (0, 0, 0, 0))

    def px(self, x, y, c):
        if 0 <= x < self.im.width and 0 <= y < self.im.height:
            self.im.putpixel((x, y), c)

    def rect(self, x, y, w, h, c):
        for yy in range(y, y + h):
            for xx in range(x, x + w):
                self.px(xx, yy, c)

    def box(self, x, y, w, h, fill, hi=None, lo=None):
        """an outlined box with its corners cut (the pixel-art rounding)"""
        self.rect(x + 1, y, w - 2, h, INK)
        self.rect(x, y + 1, w, h - 2, INK)
        self.rect(x + 1, y + 1, w - 2, h - 2, fill)
        if hi:
            self.rect(x + 2, y + 1, w - 4, 1, hi)
        if lo:
            self.rect(x + 2, y + h - 2, w - 4, 1, lo)


def handheld(p, x, y, w, top_h, bot_h, glow):
    """an open dual-screen handheld: x, y its top left"""
    p.box(x, y, w, top_h, SHELL, SHELL_HI, SHELL_LO)
    p.rect(x + 3, y + 2, w - 6, top_h - 4, INK)
    p.rect(x + 4, y + 3, w - 8, top_h - 6, GLASS)
    if glow:
        p.rect(x + 4, y + 3, w - 8, 1, glow)
        p.px(x + 5, y + 5, glow)
    hy = y + top_h
    p.rect(x + 2, hy, w - 4, 1, INK)
    p.rect(x + 3, hy, w - 6, 1, HINGE)
    by = hy + 1
    p.box(x, by, w, bot_h, SHELL, SHELL_HI, SHELL_LO)
    sw = w - 12 if w >= 16 else w - 8
    sx = x + (w - sw) // 2
    p.rect(sx - 1, by + 2, sw + 2, bot_h - 5, INK)
    p.rect(sx, by + 3, sw, bot_h - 7, GLASS)
    if glow:
        p.rect(sx, by + 3, sw, 1, glow)
    # the d-pad and the face buttons (big enough only on the large one)
    if w >= 16:
        cy = by + bot_h // 2
        p.rect(x + 2, cy, 3, 1, INK)
        p.rect(x + 3, cy - 1, 1, 3, INK)
        p.px(x + w - 3, cy - 1, RED)
        p.px(x + w - 4, cy + 1, YEL)


def arc_points(cx, cy, r0, r1):
    """the pixels of a quarter ring (up and right of cx, cy) between radii r0 and r1"""
    pts = set()
    for y in range(cy - r1 - 1, cy + 1):
        for x in range(cx, cx + r1 + 2):
            d = ((x - cx) ** 2 + (y - cy) ** 2) ** 0.5
            if r0 - 0.5 <= d < r1 + 0.5:
                pts.add((x, y))
    return pts


def halo(p, pts):
    """a dark outline around a set of pixels, on the transparent ones only"""
    for (x, y) in pts:
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                q = (x + dx, y + dy)
                if q in pts or not (0 <= q[0] < 32 and 0 <= q[1] < 32):
                    continue
                if p.im.getpixel(q)[3] == 0:
                    p.px(q[0], q[1], INK)


def lan_icon():
    p = Pix()
    handheld(p, 1, 11, 19, 10, 10, GLASS_HI)
    # the wireless waves out of its top right corner: a dot and two rings, yellow, outlined
    cx, cy = 19, 12
    rings = [arc_points(cx, cy, 5, 6), arc_points(cx, cy, 9, 10)]
    dot = {(cx + dx, cy - dy) for dx in range(0, 3) for dy in range(0, 3)}
    allp = set().union(dot, *rings)
    halo(p, allp)
    for pts in rings + [dot]:
        for (x, y) in pts:
            hi = (x - cx) < (cy - y)       # the upper half of each ring a little lighter
            p.px(x, y, YEL_HI if hi else YEL)
    return p.im


def dl_icon():
    p = Pix()
    handheld(p, 0, 7, 13, 9, 9, GLASS_HI)
    handheld(p, 19, 7, 13, 9, 9, GLASS_HI)
    # the link between them: a yellow cable with a spark in the middle
    y = 20
    p.rect(12, y - 1, 8, 4, INK)
    p.rect(13, y, 6, 2, YEL)
    p.rect(13, y, 6, 1, YEL_HI)
    # over the cable, two arrows: the data going both ways
    right = ["X..", "XX.", "XXX", "XX.", "X.."]
    left = ["..X", ".XX", "XXX", ".XX", "..X"]
    pts = set()
    for (shape, ox, oy) in ((right, 16, 7), (left, 13, 12)):
        for j, row in enumerate(shape):
            for i, ch in enumerate(row):
                if ch == "X":
                    pts.add((ox + i, oy + j))
    # each arrow's shaft
    for x in range(13, 16):
        pts.add((x, 9))
    for x in range(16, 19):
        pts.add((x, 14))
    halo(p, pts)
    for (x, y) in pts:
        p.px(x, y, YEL_HI if y < 12 else YEL)
    return p.im


def tile(icon, bg=None):
    """the Apps shelf's picture: the icon 8x (nearest), on a soft panel, 320 x 288"""
    im = Image.new("RGBA", (320, 288), (0, 0, 0, 0))
    big = icon.resize((256, 256), Image.NEAREST)
    im.paste(big, (32, 16), big)
    return im


if __name__ == "__main__":
    out = sys.argv[1] if len(sys.argv) > 1 else "."
    only = sys.argv[2] if len(sys.argv) > 2 else ""
    for name, fn in (("bananads-lan", lan_icon), ("direct-link", dl_icon)):
        if only and only != name:
            continue
        ic = fn()
        icon, pic = (f"{out}/icon.png", f"{out}/tile.png") if only else (f"{out}/{name}-icon.png", f"{out}/{name}-tile.png")
        ic.save(icon, optimize=True)
        tile(ic, None).save(pic, optimize=True)
        print(icon)
        print(pic)
