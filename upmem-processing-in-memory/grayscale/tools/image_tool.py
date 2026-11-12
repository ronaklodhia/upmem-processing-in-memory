#!/usr/bin/env python3
"""image_tool.py -- small helper around the PPM/PGM files the host program uses.

    python3 tools/image_tool.py make-test images/test.ppm [size]   # build the colour test image
    python3 tools/image_tool.py to-ppm photo.jpg images/photo.ppm  # any picture -> PPM (input format)
    python3 tools/image_tool.py to-png out.pgm out.png             # gray result -> PNG (to look at it)

Needs Pillow and NumPy:  pip3 install pillow numpy
"""
import colorsys
import sys

import numpy as np
from PIL import Image


def make_test(size=512):
    """A picture built to make the three methods disagree: saturated colours,
    a full hue sweep, overlapping light circles and smooth gradients."""
    s = size
    img = np.zeros((s, s, 3), dtype=np.uint8)
    yy, xx = np.mgrid[0:s, 0:s]

    # 1) top quarter: the eight classic colour bars
    bars = [(255, 255, 255), (255, 255, 0), (0, 255, 255), (0, 255, 0),
            (255, 0, 255), (255, 0, 0), (0, 0, 255), (0, 0, 0)]
    h1 = s // 4
    for k, c in enumerate(bars):
        img[0:h1, k * s // 8:(k + 1) * s // 8] = c

    # 2) second quarter: every hue left to right, dark to bright top to bottom
    h2 = s // 2
    for x in range(s):
        for y in range(h1, h2):
            v = 0.35 + 0.65 * (y - h1) / (h2 - h1)
            img[y, x] = [int(255 * c) for c in colorsys.hsv_to_rgb(x / s, 1.0, v)]

    # 3) bottom left: three overlapping circles of red, green and blue light
    half = s // 2
    cx, cy, rad = half // 2, h2 + half // 2, half * 0.30
    for ch, (dx, dy) in enumerate([(-0.45, -0.3), (0.45, -0.3), (0.0, 0.5)]):
        mask = (xx - (cx + dx * rad)) ** 2 + (yy - (cy + dy * rad)) ** 2 < rad ** 2
        mask &= (xx < half) & (yy >= h2)
        img[..., ch][mask] = 255

    # 4) bottom right: smooth two-colour gradients (orange -> teal, pink -> lime)
    gx = (xx - half) / half
    gy = (yy - h2) / half
    region = (xx >= half) & (yy >= h2)
    r = 255 * (1 - gx) * (1 - gy) + 255 * gx * gy
    g = 140 * (1 - gx) + 200 * gx * (1 - gy) + 80 * gy
    b = 30 * (1 - gx) + 190 * gx * (1 - gy) + 200 * gy * (1 - gx)
    for ch, plane in enumerate((r, g, b)):
        img[..., ch][region] = np.clip(plane, 0, 255)[region].astype(np.uint8)
    return Image.fromarray(img, "RGB")


def main():
    if len(sys.argv) < 3:
        sys.exit(__doc__)
    cmd = sys.argv[1]
    if cmd == "make-test":
        size = int(sys.argv[3]) if len(sys.argv) > 3 else 512
        make_test(size).save(sys.argv[2])
    elif cmd == "to-ppm":
        Image.open(sys.argv[2]).convert("RGB").save(sys.argv[3], format="PPM")
    elif cmd == "to-png":
        Image.open(sys.argv[2]).save(sys.argv[3], format="PNG")
    else:
        sys.exit(__doc__)
    print("wrote", sys.argv[3] if cmd != "make-test" else sys.argv[2])


if __name__ == "__main__":
    main()
