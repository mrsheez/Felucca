#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""EDDA OS: "Powered by COSMIC YOU" at power-on (firmware/src/powered.c), Cosmic You's lockup as Chrome draws it.

  tools/gen_powered.py FONTS_DIR [--check]

FONTS_DIR holds cinzel-latin-900-normal.woff2 and inter-latin-500-normal.woff2 (@fontsource/cinzel and
@fontsource/inter 5.3.0, SIL OFL 1.1: `npm pack @fontsource/cinzel@5.3.0 @fontsource/inter@5.3.0`, the
files/ folder of each). Needs Python's playwright (Chromium), Pillow and numpy; the build does not run it:
its output is kept in the tree.

The lockup is the footer of cosmicyou.me's release pages (CSS below) as Cosmic You's playlist cards set it:
no padding left of COSMIC YOU (its letter spacing's trailing 0.22 em then sits right of the centre, so the
word shows 0.11 em left of "Powered by" and the rule) and the rule 2 px under it. Measured against a card
(Afro House 2026, 1080 x 1920, drawn at 2.25 device pixels a CSS pixel): the letters' widths within a
pixel, "Powered by" and the rule within 0.35 CSS px of their places.

Chrome draws it at SCALE device pixels a CSS pixel, once whole and once a layer at a time, on black; each
layer's alpha is read back from its own colour channel (the layers composited again match the whole within
3.5 of 255 per channel). The paint order is CSS's: "Powered by" (white at 42 %), COSMIC YOU's glows (the
text-shadow listed last first: 0 0 18px rgba(0,240,255,.18), then 0 0 8px rgba(90,169,255,.45)), its letters
(#5AA9FF), the rule (transparent, #F97316, transparent). On the FM-1's 240 x 240 screen the word's ink is
centred across and the whole lockup's ink centred on row CENTRE_Y.

Writes firmware/src/powered_data.h and tests/powered_ref.png (Chrome's whole lockup on a black 240 x 240
screen: tests/powered_check.py holds the firmware's drawing, NIGHT's black, to it within one RGB565 step).
--check: the same, compared with the files in the tree (exit 1 if they differ).
"""
import argparse
import base64
import hashlib
import io
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent
HDR = ROOT / "firmware/src/powered_data.h"
REF = ROOT / "tests/powered_ref.png"

SCALE = 1.3              # device pixels a CSS pixel: COSMIC YOU 202 px across the 240
CENTRE_Y = 116           # the lockup's ink centred on this row (a little above the middle, 120)
G1_STEP, G2_STEP = 4, 10  # the glows' sample spacing (bilinear between): within one RGB565 step of Chrome's

CSS = """
@font-face{font-family:Cinzel;font-weight:900;src:url('%(cinzel)s') format('woff2')}
@font-face{font-family:Inter;font-weight:500;src:url('%(inter)s') format('woff2')}
html,body{margin:0;background:#000}
body{font-family:Inter,sans-serif;-webkit-font-smoothing:antialiased;--fg3:rgba(255,255,255,.42)}
.wrap{position:absolute;left:0;top:0;padding:40px 80px}
.powered{display:inline-flex;flex-direction:column;align-items:center;gap:5px;text-decoration:none;padding:6px 12px;border-radius:10px}
.powered .l{font-size:11px;font-weight:500;color:var(--fg3)}
.powered .brand{font-family:Cinzel,"Times New Roman",serif;font-weight:900;font-size:17px;letter-spacing:.22em;text-transform:uppercase;color:#5AA9FF;text-shadow:0 0 8px rgba(90,169,255,.45),0 0 18px rgba(0,240,255,.18);line-height:1.2;margin-top:2px;padding-left:.22em}
.powered .rule{width:56px;height:2px;margin-top:1px;background:linear-gradient(90deg,transparent,#f97316,transparent)}
.powered .brand{padding-left:0}.powered .rule{margin-top:2px}
"""
OFF_L = ".powered .l{color:transparent!important}"
OFF_RULE = ".powered .rule{background:none!important}"
LAYERS = {   # the layout the same in each: colours and shadows only
    "full": "",
    "pw": ".powered .brand{color:transparent!important;text-shadow:none!important}" + OFF_RULE,
    "g2": OFF_L + ".powered .brand{color:transparent!important;text-shadow:0 0 18px rgba(0,240,255,.18)!important}" + OFF_RULE,
    "g1": OFF_L + ".powered .brand{color:transparent!important;text-shadow:0 0 8px rgba(90,169,255,.45)!important}" + OFF_RULE,
    "text": OFF_L + ".powered .brand{text-shadow:none!important}" + OFF_RULE,
    "rule": OFF_L + ".powered .brand{color:transparent!important;text-shadow:none!important}",
}
HTML = """<!doctype html><html><head><meta charset="utf-8"><style>%s%s</style></head><body><div class="wrap">
<a class="powered"><span class="l">Powered by</span><span class="brand">Cosmic You</span><span class="rule"></span></a>
</div></body></html>"""
COLOUR = {"pw": (255, 255, 255), "g2": (0, 240, 255), "g1": (90, 169, 255), "text": (90, 169, 255), "rule": (249, 115, 22)}
CHANNEL = {"pw": 1, "g2": 2, "g1": 2, "text": 2, "rule": 0}     # the channel each layer's alpha is read from
ORDER = ("pw", "g2", "g1", "text", "rule")


def render(fonts):
    from playwright.sync_api import sync_playwright
    files = {"cinzel": fonts / "cinzel-latin-900-normal.woff2", "inter": fonts / "inter-latin-500-normal.woff2"}
    css = CSS % {k: "data:font/woff2;base64," + base64.b64encode(f.read_bytes()).decode() for k, f in files.items()}
    shots = {}
    with sync_playwright() as p:
        b = p.chromium.launch()
        page = b.new_context(viewport={"width": 520, "height": 220}, device_scale_factor=SCALE).new_page()
        for name, extra in LAYERS.items():
            page.set_content(HTML % (css, extra))
            page.evaluate("document.fonts.ready")
            page.wait_for_timeout(200)
            if not page.evaluate("[...document.fonts].every(f => f.status === 'loaded')"):
                sys.exit("gen_powered: a font did not load")
            png = page.screenshot(clip={"x": 0, "y": 0, "width": 520, "height": 220})
            shots[name] = np.asarray(Image.open(io.BytesIO(png)).convert("RGB")).astype(np.float64)
        b.close()
    return shots, {k: hashlib.sha256(f.read_bytes()).hexdigest()[:16] for k, f in files.items()}


def bbox(a, thr=0.5 / 255):
    ys, xs = np.nonzero(a > thr)
    return int(ys.min()), int(ys.max()) + 1, int(xs.min()), int(xs.max()) + 1


def nibbles(v):
    """values 0..15, two a byte, the first in the high nibble"""
    v = list(v) + ([0] if len(v) % 2 else [])
    return bytes((v[i] << 4) | v[i + 1] for i in range(0, len(v), 2))


def packets(v):
    """W x H coverages, row after row, as packets: a byte (the high nibble a run of 0s, 0..15; the low a count of
    coverages that follow, 0..15), then those coverages two a byte (the first high; an odd count's last byte padded).
    A pair of 0s ends a packet's coverages; a lone 0 among them is one of them"""
    out, i, n = bytearray(), 0, len(v)
    while i < n:
        z = 0
        while i < n and v[i] == 0 and z < 15:
            z += 1
            i += 1
        j = i
        while j < n and j - i < 15 and not (v[j] == 0 and j + 1 < n and v[j + 1] == 0):
            j += 1
        out.append(z << 4 | (j - i))
        out += nibbles(v[i:j])
        i = j
    return bytes(out)


def fnv(vals):
    h = 0x811C9DC5
    for x in vals:
        h = ((h ^ int(x)) * 0x01000193) & 0xFFFFFFFF
    return h


def build(shots):
    al = {n: shots[n][..., CHANNEL[n]] / COLOUR[n][CHANNEL[n]] for n in ORDER}
    back = np.zeros_like(shots["full"])
    for n in ORDER:
        a = np.clip(al[n], 0, 1)[..., None]
        back = back * (1 - a) + np.array(COLOUR[n], float) * a
    err = np.abs(back - shots["full"]).max()
    if err > 4:
        sys.exit(f"gen_powered: the layers do not make the whole ({err:.1f} of 255)")
    # placement: the word's ink centred across, the whole lockup's ink on CENTRE_Y
    ty0, ty1, tx0, tx1 = bbox(al["text"])
    boxes = {n: bbox(al[n]) for n in ORDER}
    uy0 = min(b[0] for b in boxes.values()); uy1 = max(b[1] for b in boxes.values())
    dx = 120 - (tx0 + tx1) // 2
    dy = CENTRE_Y - (uy0 + uy1) // 2
    ux0 = min(b[2] for b in boxes.values()) + dx; ux1 = max(b[3] for b in boxes.values()) + dx
    if ux0 < 0 or ux1 > 240:
        sys.exit(f"gen_powered: the lockup ({ux0}..{ux1}) leaves the screen")
    band_y0, band_h = uy0 + dy, uy1 - uy0
    L = {"dx": dx, "dy": dy, "band_y0": band_y0, "band_h": band_h, "err": err}
    # the reference: Chrome's whole lockup on a black screen, and the RGB565 levels it shows as
    full = shots["full"]
    scr = np.zeros((240, 240, 3))
    xs = slice(min(b[2] for b in boxes.values()), max(b[3] for b in boxes.values()))
    scr[uy0 + dy:uy1 + dy, xs.start + dx:xs.stop + dx] = full[uy0:uy1, xs]
    L["ref"] = Image.fromarray(np.clip(np.round(scr), 0, 255).astype(np.uint8))
    ref = steps(np.asarray(L["ref"]).astype(float))
    # the glows: samples every s px from the box's corner, one past it each way (bilinear reaches the edge)
    for n, s in (("g1", G1_STEP), ("g2", G2_STEP)):
        y0, y1, x0, x1 = boxes[n]
        ny, nx = (y1 - 1 - y0) // s + 2, (x1 - 1 - x0) // s + 2
        P = np.zeros((ny, nx))
        for j in range(ny):
            for i in range(nx):
                y, x = y0 + s * j, x0 + s * i
                P[j, i] = al[n][y, x] if y < al[n].shape[0] and x < al[n].shape[1] else 0
        amax = P.max()
        v = np.round(np.clip(P / amax, 0, 1) * 15).astype(int)
        L[n] = dict(x=x0 + dx, y=y0 + dy - band_y0, w=nx, h=ny, s=s, pw=x1 - x0, ph=y1 - y0, a12=round(amax * 4096),
                    v=v, data=nibbles(v.ravel()), hash=fnv(v.ravel()))
    # the rule: its rows times its columns (Chrome's gradient is separable: within 0.005)
    y0, y1, x0, x1 = boxes["rule"]
    r = np.clip(al["rule"][y0:y1, x0:x1], 0, 1)
    rows, cols = r.max(1), r.max(0)
    rows = rows / rows.max() * 255; cols = cols / cols.max() * 255 * r.max()
    sep = np.abs(np.outer(rows, cols) / 65025 - r).max()
    if sep > 0.01:
        sys.exit(f"gen_powered: the rule is not separable ({sep:.3f})")
    L["rule"] = dict(x=x0 + dx, y=y0 + dy - band_y0, w=x1 - x0, h=y1 - y0,
                     rows=bytes(int(round(x)) for x in rows), cols=bytes(int(round(x)) for x in cols))
    # "Powered by" and COSMIC YOU: each pixel's coverage the 0..15 whose result on the screen, through powered.c's
    # integer arithmetic over everything under and over it (dev_pixel), comes closest to Chrome's in RGB565 steps
    # (the nearest coverage unless another is strictly closer, at most 2 away)
    pa12 = round(al["pw"][slice(*boxes["pw"][:2]), slice(*boxes["pw"][2:])].max() * 4096)
    for n in ("pw", "text"):
        y0, y1, x0, x1 = boxes[n]
        a = np.clip(al[n][y0:y1, x0:x1], 0, 1)
        v0 = np.round(a / (a.max() if n == "pw" else 1.0) * 15).astype(int)
        v = v0.copy()
        for j in range(y1 - y0):
            for i in range(x1 - x0):
                X, Y = x0 + dx + i, y0 + dy + j - band_y0
                def score(k):
                    got = dev_pixel(L, X, Y, **({"pw_v": k, "pw_a12": pa12} if n == "pw" else {"text_v": k}))
                    e = np.abs(np.array(got) - ref[Y + band_y0, X])
                    return (e.max(), e.sum(), abs(k - v0[j, i]))
                best = min(range(max(0, v0[j, i] - 2), min(15, v0[j, i] + 2) + 1), key=score)
                v[j, i] = best
        L[n] = dict(x=x0 + dx, y=y0 + dy - band_y0, w=x1 - x0, h=y1 - y0, v=v, moved=int((v != v0).sum()),
                    hash=fnv(v.ravel()))
        if n == "pw":
            L[n].update(a12=pa12, data=packets(list(v.ravel())))
        else:
            L[n].update(data=packets(list(v.ravel())))
    # the whole band as powered.c draws it, against Chrome's
    dev = np.zeros((240, 240, 3))
    for Y in range(band_h):
        for X in range(240):
            dev[Y + band_y0, X] = dev_pixel(L, X, Y)
    d = np.abs(dev - ref).max(-1)
    L["steps"] = (int((d == 1).sum()), int((d > 1).sum()))
    return L


def steps(rgb):
    """8-bit RGB -> the RGB565 levels it shows as (tests/powered_check.py's)"""
    return np.stack([np.round(rgb[..., 0] * 31 / 255), np.round(rgb[..., 1] * 63 / 255),
                     np.round(rgb[..., 2] * 31 / 255)], -1)


def dev_pixel(L, X, Y, pw_v=None, pw_a12=None, text_v=None):
    """one pixel of the band (X across, Y down it) as powered.c draws it over a black BG, NIGHT's: its RGB565 levels.
    pw_v / text_v: that coverage there instead of L's (L may not have the layer yet)"""
    def glow(g):
        fx, fy, s, gw = X - g["x"], Y - g["y"], g["s"], g["w"]
        if not (0 <= fx < g["pw"] and 0 <= fy < g["ph"]):
            return 0
        i, j = fx // s, fy // s
        rx, ry = fx - i * s, fy - j * s
        P = g["v"].ravel()
        k = j * gw + i
        acc = (int(P[k]) * (s - rx) + int(P[k + 1]) * rx) * (s - ry) + (int(P[k + gw]) * (s - rx) + int(P[k + gw + 1]) * rx) * ry
        return (acc * g["a12"] + 15 * s * s // 2) // (15 * s * s)

    def cov(name, v_here):
        if v_here is not None:
            return v_here
        lay = L.get(name)
        if not lay or "v" not in lay:
            return 0
        i, j = X - lay["x"], Y - lay["y"]
        return int(lay["v"][j, i]) if 0 <= i < lay["w"] and 0 <= j < lay["h"] else 0

    c = [0, 0, 0]

    def over(col, a):
        for k in range(3):
            c[k] += ((col[k] - c[k]) * a + 2048) >> 12

    p = cov("pw", pw_v) if (pw_v is not None or "pw" in L) else 0
    pa = pw_a12 if pw_a12 is not None else L.get("pw", {}).get("a12", 0)
    over((255, 255, 255), (p * pa + 7) // 15)
    over((0, 240, 255), glow(L["g2"]))
    over((90, 169, 255), glow(L["g1"]))
    over((90, 169, 255), (cov("text", text_v) * 4096 + 7) // 15)
    rl = L["rule"]
    i, j = X - rl["x"], Y - rl["y"]
    if 0 <= i < rl["w"] and 0 <= j < rl["h"]:
        over((249, 115, 22), (rl["rows"][j] * rl["cols"][i] * 4096 + 32512) // 65025)
    return ((c[0] * 31 * 257 + 32768) >> 16, (c[1] * 63 * 257 + 32768) >> 16, (c[2] * 31 * 257 + 32768) >> 16)


def c_bytes(name, b, per=24):
    rows = [", ".join(f"0x{x:02X}" for x in b[i:i + per]) for i in range(0, len(b), per)]
    return f"static const uint8_t {name}[{len(b)}] = {{\n    " + ",\n    ".join(rows) + "\n};\n"


def header(L, sums):
    p, t, g1, g2, r = L["pw"], L["text"], L["g1"], L["g2"], L["rule"]
    total = len(p["data"]) + len(t["data"]) + len(g1["data"]) + len(g2["data"]) + len(r["rows"]) + len(r["cols"])
    return f"""/* SPDX-License-Identifier: GPL-3.0-only */
/* generated by tools/gen_powered.py: do not edit. EDDA OS: "Powered by COSMIC YOU" (powered.c), Cosmic You's
 * lockup as Chrome draws it at {SCALE} device pixels a CSS pixel (Cinzel 900 {sums['cinzel']}, Inter 500 {sums['inter']}:
 * @fontsource 5.3.0, SIL OFL 1.1), placed on the 240 x 240 screen; {total} bytes. A layer's coverage is 0..15
 * (15: its A12 alpha, /4096); its X across the screen, its Y down the band (screen rows PW_Y0 .. PW_Y0 + PW_H - 1),
 * painted in this order. */
#define PW_Y0 {L['band_y0']}u
#define PW_H {L['band_h']}u

/* "Powered by", white at 42 %: W x H coverages in packets (powered.c pw_next: a byte, its high nibble a run of 0s and
 * its low a count of coverages that follow, two a byte, the first high) */
#define PW_P_X {p['x']}
#define PW_P_Y {p['y']}
#define PW_P_W {p['w']}u
#define PW_P_H {p['h']}u
#define PW_P_A12 {p['a12']}u
{c_bytes('PW_P', p['data'])}
/* COSMIC YOU's glows, 0 0 18px rgba(0,240,255,.18) (G2) under 0 0 8px rgba(90,169,255,.45) (G1): W x H samples
 * S px apart from (X, Y) (bilinear between), two a byte (the first high); they cover PW x PH */
#define PW_G2_X {g2['x']}
#define PW_G2_Y {g2['y']}
#define PW_G2_W {g2['w']}u
#define PW_G2_H {g2['h']}u
#define PW_G2_S {g2['s']}u
#define PW_G2_PW {g2['pw']}u
#define PW_G2_PH {g2['ph']}u
#define PW_G2_A12 {g2['a12']}u
{c_bytes('PW_G2', g2['data'])}
#define PW_G1_X {g1['x']}
#define PW_G1_Y {g1['y']}
#define PW_G1_W {g1['w']}u
#define PW_G1_H {g1['h']}u
#define PW_G1_S {g1['s']}u
#define PW_G1_PW {g1['pw']}u
#define PW_G1_PH {g1['ph']}u
#define PW_G1_A12 {g1['a12']}u
{c_bytes('PW_G1', g1['data'])}
/* COSMIC YOU, #5AA9FF: W x H coverages in packets, as PW_P */
#define PW_T_X {t['x']}
#define PW_T_Y {t['y']}
#define PW_T_W {t['w']}u
#define PW_T_H {t['h']}u
{c_bytes('PW_T', t['data'])}
/* the rule, #F97316: alpha = ROWS[y] * COLS[x] / 65025 */
#define PW_R_X {r['x']}
#define PW_R_Y {r['y']}
#define PW_R_W {r['w']}u
#define PW_R_H {r['h']}u
{c_bytes('PW_R_ROWS', r['rows'])}{c_bytes('PW_R_COLS', r['cols'])}
/* FNV-1a of each layer's coverages as decoded, row by row (tests/ui_render.c checks powered.c's reading); in
 * decimal: tools/build.py's register lint reads some hex numbers as addresses */
#define PW_HASH_P {p['hash']}u
#define PW_HASH_T {t['hash']}u
#define PW_HASH_G1 {g1['hash']}u
#define PW_HASH_G2 {g2['hash']}u
"""


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("fonts", type=Path)
    ap.add_argument("--check", action="store_true")
    a = ap.parse_args()
    shots, sums = render(a.fonts)
    L = build(shots)
    h = header(L, sums)
    buf = io.BytesIO()
    L["ref"].save(buf, "PNG", optimize=True)
    if a.check:
        same_h = HDR.exists() and HDR.read_text() == h
        same_r = REF.exists() and np.array_equal(np.asarray(Image.open(REF)), np.asarray(L["ref"]))
        print(f"gen_powered: {HDR.name} {'the same' if same_h else 'DIFFERS'}, {REF.name} {'the same' if same_r else 'DIFFERS'}")
        sys.exit(0 if same_h and same_r else 1)
    HDR.write_text(h)
    REF.write_bytes(buf.getvalue())
    n = sum(len(L[k]["data"]) for k in ("pw", "text", "g1", "g2")) + len(L["rule"]["rows"]) + len(L["rule"]["cols"])
    print(f"gen_powered: {HDR.relative_to(ROOT)} ({n} B of data: Powered by {len(L['pw']['data'])}, COSMIC YOU "
          f"{len(L['text']['data'])}, glows {len(L['g1']['data'])} + {len(L['g2']['data'])}, rule "
          f"{len(L['rule']['rows']) + len(L['rule']['cols'])}), band rows {L['band_y0']}..{L['band_y0'] + L['band_h'] - 1}; "
          f"{REF.relative_to(ROOT)}; layers vs whole {L['err']:.1f}/255; coverages set closer to Chrome: Powered by "
          f"{L['pw']['moved']}, COSMIC YOU {L['text']['moved']}; the band as powered.c draws it: {L['steps'][0]} px one "
          f"RGB565 step from Chrome's, {L['steps'][1]} further")


if __name__ == "__main__":
    main()
