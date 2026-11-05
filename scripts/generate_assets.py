#!/usr/bin/env python3
"""
Extracts the graphical assets used by the plugin UI from the reference image
(docs/reference/MSC_VOX_2_0_reference.png) and builds the 6 interface themes
by hue-shifting them.

Output goes to Resources/Images (already generated and committed, so this script
is OPTIONAL - run it only if you want to regenerate the art):

    pip install pillow numpy
    python scripts/generate_assets.py
"""
import colorsys
import os
import sys

import numpy as np
from PIL import Image, ImageFilter

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REF = os.path.join(ROOT, "docs", "reference", "MSC_VOX_2_0_reference.png")
OUT = os.path.join(ROOT, "Resources", "Images")

# Crop boxes in the 1536x1024 reference coordinate system (x0, y0, x1, y1)
BANNER_BOX = (0, 55, 1100, 340)
SINGER_BOX = (400, 55, 800, 340)
GUITAR_BOX = (780, 55, 1100, 340)
DECO_BOX = (1425, 55, 1536, 400)
TITLE_BOX = (75, 8, 305, 52)

# (file suffix, display name, hue shift in degrees, saturation mult, value mult)
THEMES = [
    ("01_purple", "Purple Punk", 0, 1.0, 1.0),
    ("02_blue", "Midnight Blue", -60, 1.0, 1.0),
    ("03_green", "Toxic Green", -135, 1.0, 1.0),
    ("04_pink", "Hot Pink", 55, 1.05, 1.05),
    ("05_gold", "Gold Rush", 125, 1.0, 1.05),
    ("06_cyan", "Cyber Cyan", -95, 1.0, 1.05),
]


def hue_shift(img, degrees, sat=1.0, val=1.0):
    """Rotate hue of an RGB(A) PIL image using numpy HSV conversion."""
    arr = np.asarray(img.convert("RGBA")).astype(np.float32) / 255.0
    rgb = arr[..., :3]
    alpha = arr[..., 3:]
    mx = rgb.max(axis=-1)
    mn = rgb.min(axis=-1)
    d = mx - mn
    h = np.zeros_like(mx)
    nz = d > 1e-6
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    m = nz & (mx == r)
    h[m] = ((g - b)[m] / d[m]) % 6.0
    m = nz & (mx == g)
    h[m] = (b - r)[m] / d[m] + 2.0
    m = nz & (mx == b)
    h[m] = (r - g)[m] / d[m] + 4.0
    h = (h / 6.0 + degrees / 360.0) % 1.0
    s = np.where(mx > 1e-6, d / np.maximum(mx, 1e-6), 0.0) * sat
    s = np.clip(s, 0, 1)
    v = np.clip(mx * val, 0, 1)
    # HSV -> RGB
    i = np.floor(h * 6.0).astype(np.int32) % 6
    f = h * 6.0 - np.floor(h * 6.0)
    p = v * (1 - s)
    q = v * (1 - f * s)
    t = v * (1 - (1 - f) * s)
    out = np.zeros_like(rgb)
    for k, (rr, gg, bb) in enumerate(
        [(v, t, p), (q, v, p), (p, v, t), (p, q, v), (t, p, v), (v, p, q)]
    ):
        sel = i == k
        out[..., 0][sel] = rr[sel]
        out[..., 1][sel] = gg[sel]
        out[..., 2][sel] = bb[sel]
    res = np.concatenate([out, alpha], axis=-1)
    return Image.fromarray((res * 255.0 + 0.5).astype(np.uint8), "RGBA")


def feather(img, margin):
    """Return RGBA image whose alpha fades to 0 over `margin` px at every edge."""
    w, h = img.size
    ys = np.minimum(np.arange(h), h - 1 - np.arange(h)).astype(np.float32)
    xs = np.minimum(np.arange(w), w - 1 - np.arange(w)).astype(np.float32)
    ay = np.clip(ys / margin, 0, 1)[:, None]
    ax = np.clip(xs / margin, 0, 1)[None, :]
    a = np.minimum(ay, ax)
    a = a * a * (3 - 2 * a)  # smoothstep
    rgba = np.asarray(img.convert("RGBA")).copy()
    rgba[..., 3] = (a * 255).astype(np.uint8)
    return Image.fromarray(rgba, "RGBA")


def main():
    if not os.path.exists(REF):
        print("Reference image not found:", REF)
        return 1
    os.makedirs(OUT, exist_ok=True)
    ref = Image.open(REF).convert("RGB")
    crops = {
        "banner": ref.crop(BANNER_BOX),
        "singer": ref.crop(SINGER_BOX),
        "guitarist": ref.crop(GUITAR_BOX),
        "deco": ref.crop(DECO_BOX),
        "title": ref.crop(TITLE_BOX),
    }
    for suffix, name, deg, sat, val in THEMES:
        for key, img in crops.items():
            shifted = hue_shift(img, deg, sat, val)
            if key in ("singer", "guitarist"):
                shifted = feather(shifted, 26)
                shifted.save(os.path.join(OUT, f"{key}_{suffix}.png"), optimize=True)
            else:
                shifted.convert("RGB").save(
                    os.path.join(OUT, f"{key}_{suffix}.jpg"), quality=90, optimize=True
                )
        print("theme", suffix, "done")
    return 0


if __name__ == "__main__":
    sys.exit(main())
