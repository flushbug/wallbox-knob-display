#!/usr/bin/env python3
"""Preview of the display layouts as PNG, without flashing.

Mirrors the drawing code of src/display_tdisplay.cpp (T-Display-S3, 320 x 170),
src/display_knob.cpp (round 240 x 240) and src/painter.h with the same
coordinates; keep them in sync when a layout changes. Fonts are approximated
with Arial, placed with the metrics of the FreeSans fonts used on the device.

    python tools/preview.py out.png [--board tdisplay|knob] [--pending 2] [--no-home-battery]
"""
import argparse
from PIL import Image, ImageDraw, ImageFont

W, H = 320, 170
FONT_DIR = "C:/Windows/Fonts/"
# Adafruit GFX fonts are converted at 141 dpi: em in px = pt * 141 / 72 * size
def gfx_font(bold, pt, size=1.0):
    return ImageFont.truetype(FONT_DIR + ("arialbd.ttf" if bold else "arial.ttf"), round(pt * 141 / 72 * size))

F_TINY = gfx_font(False, 9)
F_MID = gfx_font(True, 18, 0.9)
F_MODE = gfx_font(True, 12)
# LovyanGFX places GFX fonts by the largest ascent (ab) / descent (bb) over all glyphs
# of the font, scaled by the text size; values from the font headers
GFX_METRICS = {id(F_TINY): (13, 5), id(F_MID): (25 * 0.9, 8 * 0.9), id(F_MODE): (17, 6)}

C = dict(PV=0xFFC107, CHARGE=0x4CAF50, GREY=0x9E9E9E, TRACK=0x2A2A2A, OFF=0x757575, SMART=0x2196F3,
         NOW=0xF44336, PENDING=0xFF9800, BATT_LOW=0xF44336, BATT_MID=0xFFC107, BATT_HIGH=0x4CAF50,
         PHASE=0xFFD600, EVCC=0x0FDE41, WHITE=0xFFFFFF, BLACK=0x000000, ERR=0xF44336)
def rgb(c): return ((c >> 16) & 255, (c >> 8) & 255, c & 255)
MODE_COLOR = [C["OFF"], C["SMART"], C["NOW"]]


class P:
    def __init__(self, img):
        self.d = ImageDraw.Draw(img)

    def tri(self, x0, y0, x1, y1, x2, y2, c): self.d.polygon([(x0, y0), (x1, y1), (x2, y2)], fill=rgb(c))
    def rect(self, x, y, w, h, c): self.d.rectangle([x, y, x + w - 1, y + h - 1], fill=rgb(c))
    def frame(self, x, y, w, h, c): self.d.rectangle([x, y, x + w - 1, y + h - 1], outline=rgb(c))
    def line(self, x0, y0, x1, y1, c): self.d.line([(x0, y0), (x1, y1)], fill=rgb(c))
    def circle(self, x, y, r, c): self.d.ellipse([x - r, y - r, x + r, y + r], fill=rgb(c))
    def text(self, s, x, y, f, c, datum):
        ab, bb = GFX_METRICS[id(f)]
        v, h = datum.split("_")
        base = {"top": y + ab, "middle": y + (ab - bb) / 2, "bottom": y - bb, "baseline": y}[v]
        self.d.text((x, round(base)), s, font=f, fill=rgb(c), anchor={"left": "l", "center": "m", "right": "r"}[h] + "s")

    def house(self, mx, ty):
        self.tri(mx - 9, ty + 7, mx, ty, mx + 9, ty + 7, C["GREY"])
        self.rect(mx - 6, ty + 7, 13, 8, C["GREY"])
        self.rect(mx - 1, ty + 10, 3, 5, C["BLACK"])

    def car(self, mx, ty):
        g = C["GREY"]
        self.rect(mx - 9, ty + 5, 19, 6, g)
        self.tri(mx - 6, ty + 5, mx - 3, ty + 1, mx - 3, ty + 5, g)
        self.rect(mx - 3, ty + 1, 7, 4, g)
        self.tri(mx + 4, ty + 1, mx + 7, ty + 5, mx + 4, ty + 5, g)
        for cx in (mx - 5, mx + 6):
            self.circle(cx, ty + 11, 3, C["BLACK"]); self.circle(cx, ty + 11, 2, g)

    def solar(self, mx, ty):
        tl, tr, bl, br, h = mx - 8, mx + 12, mx - 12, mx + 8, 12
        self.tri(tl, ty, tr, ty, br, ty + h, C["PV"])
        self.tri(tl, ty, br, ty + h, bl, ty + h, C["PV"])
        for k in (1, 2):
            yy = ty + k * h // 3
            xl, xr = tl - (yy - ty) * 4 // h, tr - (yy - ty) * 4 // h
            self.line(xl, yy, xr, yy, C["BLACK"])
            xt, xb = tl + k * (tr - tl) // 3, bl + k * (br - bl) // 3
            self.line(xt, ty, xb, ty + h, C["BLACK"])
        self.line(mx, ty + h, mx, ty + h + 4, C["PV"])
        self.line(mx - 4, ty + h + 4, mx + 4, ty + h + 4, C["PV"])

    def evcc_mark(self, mx, ty, k, col=C["EVCC"]):
        for t in [(0, 0, 5, 0, -5, 10), (5, 0, 1, 7, -5, 10), (-5, 10, 1, 7, 5, 7), (-5, 10, 5, 7, 0, 10),
                  (5, 7, -2, 18, 0, 10)]:
            self.tri(*[(mx if i % 2 == 0 else ty) + int(v * k) for i, v in enumerate(t)], col)

    def wallbox(self, mx, ty, col):
        self.rect(mx - 8, ty, 11, 18, col)
        self.rect(mx - 10, ty + 18, 15, 2, col)
        self.tri(mx - 1, ty + 3, mx - 6, ty + 10, mx - 2, ty + 10, C["BLACK"])
        self.tri(mx - 4, ty + 8, mx + 1, ty + 8, mx - 5, ty + 15, C["BLACK"])
        self.rect(mx + 3, ty + 5, 5, 2, col)
        self.rect(mx + 6, ty + 5, 2, 9, col)
        self.rect(mx + 5, ty + 13, 4, 5, col)

    def battery(self, x, y, w, h, soc):
        cap_w, cap_h = w // 2, 5
        self.rect(x + (w - cap_w) // 2, y - cap_h, cap_w, cap_h, C["GREY"])
        self.frame(x, y, w, h, C["GREY"]); self.frame(x + 1, y + 1, w - 2, h - 2, C["GREY"])
        if soc is not None:
            inner = h - 6
            fill = int(inner * soc / 100 + 0.5)
            c = C["BATT_LOW"] if soc < 20 else (C["BATT_MID"] if soc < 50 else C["BATT_HIGH"])
            if fill > 0: self.rect(x + 3, y + 3 + inner - fill, w - 6, fill, c)

    def bolt(self, x0, y0, filled):
        pts = [(x0 + bx, y0 + by) for bx, by in zip([5, 12, 7, 12, 2, 5, 0], [0, 0, 7, 7, 18, 10, 10])]
        if filled: self.d.polygon(pts, fill=rgb(C["PHASE"]))
        self.d.line(pts + [pts[0]], fill=rgb(C["PHASE"]))

    # ---- mirrors Painter::hint / Painter::main in display_tdisplay.cpp (buttons on the right)
    def hint(self, label, top, col, logo=False):
        y = 20 if top else H - 20
        self.tri(W - 11, y - 7, W - 11, y + 7, W - 2, y, col)
        if logo:
            self.evcc_mark(W - 26, y - 14, 1.55)
            return
        self.text(label, W - 15, y, F_TINY, col, "middle_right")

    def evcc_logo(self, mx, ty):
        self.evcc_mark(mx, ty, 1.0)
        self.text("ev", mx - 6, ty + 16, F_MODE, C["WHITE"], "baseline_right")
        self.text("cc", mx + 8, ty + 16, F_MODE, C["WHITE"], "baseline_left")

    # ---- mirrors Painter::main in display_knob.cpp (round display)
    def knob(self, s, pending=None):
        cx = 120
        frac = min(max(s["charge"] / 1000 / s["max_kw"], 0), 1)
        lit = 135 + 270 * frac
        self.d.arc([1, 1, 239, 239], 135, 405, fill=rgb(C["TRACK"]), width=10)
        if frac > 0: self.d.arc([1, 1, 239, 239], 135, lit, fill=rgb(C["CHARGE"]), width=10)
        if s.get("show_home_batt", True):
            self.battery(30, 80, 22, 60, s["batt_soc"]); self.house(41, 146)
        self.battery(188, 80, 22, 60, s["car_soc"]); self.car(199, 146)
        self.solar(cx, 32)
        self.text(kw(s["pv"]), cx, 56, F_MID, C["PV"], "top_center")
        self.evcc_logo(cx, 98)
        self.text(kw(s["charge"]), cx, 124, F_MID, C["CHARGE"], "top_center")
        labels = ["Off", "Smart", "Fast"]
        if pending is not None:
            self.text(labels[pending] + " ?", cx, 168, F_MODE, C["PENDING"], "top_center")
            self.text("Press = OK", cx, 196, F_TINY, C["GREY"], "top_center")
            return
        self.rect(72, 166, 96, 30, MODE_COLOR[s["mode"]])
        self.text(labels[s["mode"]], cx, 181, F_MODE, C["WHITE"], "middle_center")
        for p in range(3): self.bolt(94 + p * 20, 201, p < (s["phases"] if s["charging"] else 0))

    def main(self, s, pending=None):
        x0 = 0
        shift = 0 if s.get("show_home_batt", True) else 36
        if not shift:
            self.battery(x0 + 10, 26, 24, 74, s["batt_soc"]); self.house(x0 + 22, 108)
        self.battery(x0 + 258, 26, 24, 74, s["car_soc"]); self.car(x0 + 270, 108)
        ix, vx = x0 + 68 - shift, x0 + 238   # values right-aligned at vx, before the car battery
        self.solar(ix, 12)
        self.text(kw(s["pv"]), vx, 8, F_MID, C["PV"], "top_right")
        self.wallbox(ix + 1, 50, C["CHARGE"] if s["charging"] else C["GREY"])
        self.text(kw(s["charge"]), vx, 48, F_MID, C["CHARGE"], "top_right")
        bx, bw, by, bh = x0 + 46 - shift, 200 + shift, 90, 10
        cx = bx + bw // 2            # phases and mode centred under the bar
        lit = int(bw * min(max(s["charge"] / 1000 / s["max_kw"], 0), 1) + 0.5)
        self.rect(bx, by, bw, bh, C["TRACK"])
        if lit > 0: self.rect(bx, by, lit, bh, C["CHARGE"])
        py, my, mh, mw = 108, 134, 30, 100
        mx = cx - mw // 2
        labels = ["Off", "Smart", "Fast"]
        if pending is not None:
            self.frame(mx, my, mw, mh, C["PENDING"]); self.frame(mx + 1, my + 1, mw - 2, mh - 2, C["PENDING"])
            self.text(labels[pending] + " ?", cx, my + mh // 2, F_MODE, C["PENDING"], "middle_center")
        else:
            self.rect(mx, my, mw, mh, MODE_COLOR[s["mode"]])
            self.text(labels[s["mode"]], cx, my + mh // 2, F_MODE, C["WHITE"], "middle_center")
        for p in range(3): self.bolt(cx - 26 + p * 20, py, p < (s["phases"] if s["charging"] else 0))
        self.hint("Mode", True, C["WHITE"], logo=True)
        self.hint("OK", False, C["PENDING"] if pending is not None else C["GREY"])


def kw(w): return "%.1f kW" % (w / 1000)


def board(screen, scale):
    """Screen scaled up inside a simple board outline with the buttons on the right."""
    sw, sh = W * scale, H * scale
    pad, right = 14 * scale // 3, 40 * scale // 3
    img = Image.new("RGB", (sw + pad * 2 + right, sh + pad * 2), (40, 40, 40))
    img.paste(screen.resize((sw, sh), Image.NEAREST), (pad, pad))
    d = ImageDraw.Draw(img)
    for y in (pad + 20 * scale, pad + (H - 20) * scale):
        d.rectangle([img.width - 10 * scale // 3, y - 8 * scale // 3, img.width, y + 8 * scale // 3], fill=(170, 170, 170))
    return img


def round_board(screen, scale):
    """Round screen scaled up in a dark bezel."""
    size = 240 * scale
    big = screen.resize((size, size), Image.NEAREST)
    mask = Image.new("L", (size, size), 0)
    ImageDraw.Draw(mask).ellipse([0, 0, size - 1, size - 1], fill=255)
    pad = 12 * scale
    img = Image.new("RGB", (size + 2 * pad, size + 2 * pad), (255, 255, 255))
    ImageDraw.Draw(img).ellipse([0, 0, img.width - 1, img.height - 1], fill=(60, 60, 60))
    img.paste(big, (pad, pad), mask)
    return img


def render(board_name, state, pending=None, scale=2):
    if board_name == "knob":
        scr = Image.new("RGB", (240, 240), (0, 0, 0))
        P(scr).knob(state, pending)
        return round_board(scr, scale)
    scr = Image.new("RGB", (W, H), (0, 0, 0))
    P(scr).main(state, pending)
    return board(scr, scale)


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("out")
    ap.add_argument("--board", choices=["tdisplay", "knob"], default="tdisplay")
    ap.add_argument("--pending", type=int, default=None, help="show a waiting selection (mode index)")
    ap.add_argument("--no-home-battery", action="store_true")
    ap.add_argument("--scale", type=int, default=3)
    a = ap.parse_args()
    state = dict(pv=7400, charge=11000, batt_soc=65, car_soc=42, charging=True, phases=3, mode=1, max_kw=22,
                 show_home_batt=not a.no_home_battery)
    render(a.board, state, a.pending, a.scale).save(a.out)
