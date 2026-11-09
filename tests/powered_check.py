#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""EDDA OS: "Powered by COSMIC YOU" as the firmware draws it (powered.c; tests/ui_render.c's ppm in NIGHT, a black BG)
against Chrome's drawing of Cosmic You's own CSS (tests/powered_ref.png, tools/gen_powered.py): every pixel within one
RGB565 step (the screen's own: 1/31 of red or blue, 1/63 of green), but for a few edge pixels of the letters at two
(Chrome rounds its layers to 8 bits as it stacks them; tools/gen_powered.py reports the same count).

  python3 tests/powered_check.py build/ui_new/ppm/NIGHT_powered.ppm [tests/powered_ref.png]
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image


def steps(rgb):                                   # 8-bit RGB -> the RGB565 levels it shows as
    return np.stack([np.round(rgb[..., 0] * 31 / 255), np.round(rgb[..., 1] * 63 / 255),
                     np.round(rgb[..., 2] * 31 / 255)], -1)


def main():
    dev = steps(np.asarray(Image.open(sys.argv[1]).convert("RGB")).astype(float))
    ref_path = sys.argv[2] if len(sys.argv) > 2 else str(Path(__file__).with_name("powered_ref.png"))
    ref = steps(np.asarray(Image.open(ref_path).convert("RGB")).astype(float))
    d = np.abs(dev - ref).max(-1)
    ink = (ref.max(-1) > 0) | (dev.max(-1) > 0)
    ok = d.max() <= 2 and (d > 1).sum() <= 4 and ink.sum() > 2000
    print(f"powered: the firmware's Powered by COSMIC YOU against Chrome's, {int(ink.sum())} px of it: "
          f"{int((d == 0)[ink].sum())} the same, {int((d == 1).sum())} one RGB565 step off, "
          f"{int((d > 1).sum())} at two, none further {'ok' if ok else 'FAIL'}")
    sys.exit(0 if ok else 1)


if __name__ == "__main__":
    main()
