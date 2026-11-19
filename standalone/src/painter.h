// Drawing primitives, icons, colours and fonts shared by both display layouts.
// All functions take screen coordinates; oy shifts them into the current band.
#pragma once
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "app.h"

// ---------------------------------------------------------------- Colours (RGB888)
constexpr uint32_t C_PV = 0xFFC107, C_CHARGE = 0x4CAF50, C_GREY = 0x9E9E9E, C_TRACK = 0x2A2A2A;
constexpr uint32_t C_OFF = 0x757575, C_SMART = 0x2196F3, C_NOW = 0xF44336;
constexpr uint32_t C_PENDING = 0xFF9800, C_BATT_LOW = 0xF44336, C_BATT_MID = 0xFFC107;
constexpr uint32_t C_BATT_HIGH = 0x4CAF50, C_PHASE = 0xFFD600, C_EVCC = 0x0FDE41;
constexpr uint32_t C_WHITE = 0xFFFFFF, C_BLACK = 0x000000, C_ERR = 0xF44336;
static const uint32_t MODE_COLOR[MODE_COUNT] = {C_OFF, C_SMART, C_NOW};

// ---------------------------------------------------------------- Fonts (ASCII only)
static const lgfx::IFont *F_TINY = &fonts::FreeSans9pt7b;
static const lgfx::IFont *F_MID = &fonts::FreeSansBold18pt7b;
static const lgfx::IFont *F_MODE = &fonts::FreeSansBold12pt7b;


namespace {

struct PainterBase {
  LGFX_Sprite &g;
  int oy;

  PainterBase(LGFX_Sprite &sprite, int offsetY) : g(sprite), oy(offsetY) {}

  int Y(int y) const { return y + oy; }

  // ring segment around (cx, cy), angles in degrees (0 = 3 o'clock, clockwise), may exceed 360
  void arc(int cx, int cy, int r0, int r1, float a0, float a1, uint32_t col) {
    if (a1 <= a0) return;
    if (a1 > 360.0f && a0 < 360.0f) {
      g.fillArc(cx, Y(cy), r0, r1, a0, 360.0f, col);
      g.fillArc(cx, Y(cy), r0, r1, 0.0f, a1 - 360.0f, col);
    } else if (a0 >= 360.0f) {
      g.fillArc(cx, Y(cy), r0, r1, a0 - 360.0f, a1 - 360.0f, col);
    } else {
      g.fillArc(cx, Y(cy), r0, r1, a0, a1, col);
    }
  }

  void tri(int x0, int y0, int x1, int y1, int x2, int y2, uint32_t c) {
    g.fillTriangle(x0, Y(y0), x1, Y(y1), x2, Y(y2), c);
  }
  void rect(int x, int y, int w, int h, uint32_t c) { g.fillRect(x, Y(y), w, h, c); }
  void frame(int x, int y, int w, int h, uint32_t c) { g.drawRect(x, Y(y), w, h, c); }
  void line(int x0, int y0, int x1, int y1, uint32_t c) { g.drawLine(x0, Y(y0), x1, Y(y1), c); }
  void circle(int x, int y, int r, uint32_t c) { g.fillCircle(x, Y(y), r, c); }
  void text(const String &s, int x, int y, const lgfx::IFont *f, uint32_t c,
            lgfx::textdatum_t d, float size = 1.0f) {
    g.setFont(f);
    g.setTextSize(size);
    g.setTextDatum(d);
    g.setTextColor(c);
    g.drawString(s, x, Y(y));
  }

  // ---- icons, mx = centre, ty = top edge
  void house(int mx, int ty) {
    tri(mx - 9, ty + 7, mx, ty, mx + 9, ty + 7, C_GREY);
    rect(mx - 6, ty + 7, 13, 8, C_GREY);
    rect(mx - 1, ty + 10, 3, 5, C_BLACK);
  }
  void car(int mx, int ty) {
    rect(mx - 9, ty + 5, 19, 6, C_GREY);
    tri(mx - 6, ty + 5, mx - 3, ty + 1, mx - 3, ty + 5, C_GREY);
    rect(mx - 3, ty + 1, 7, 4, C_GREY);
    tri(mx + 4, ty + 1, mx + 7, ty + 5, mx + 4, ty + 5, C_GREY);
    circle(mx - 5, ty + 11, 3, C_BLACK); circle(mx - 5, ty + 11, 2, C_GREY);
    circle(mx + 6, ty + 11, 3, C_BLACK); circle(mx + 6, ty + 11, 2, C_GREY);
  }
  void solar(int mx, int ty) {
    int tl = mx - 8, tr = mx + 12, bl = mx - 12, br = mx + 8, h = 12;
    tri(tl, ty, tr, ty, br, ty + h, C_PV);
    tri(tl, ty, br, ty + h, bl, ty + h, C_PV);
    for (int k = 1; k <= 2; k++) {
      int yy = ty + k * h / 3;
      int xl = tl - (yy - ty) * 4 / h, xr = tr - (yy - ty) * 4 / h;
      line(xl, yy, xr, yy, C_BLACK);
      int xt = tl + k * (tr - tl) / 3, xb = bl + k * (br - bl) / 3;
      line(xt, ty, xb, ty + h, C_BLACK);
    }
    line(mx, ty + h, mx, ty + h + 4, C_PV);
    line(mx - 4, ty + h + 4, mx + 4, ty + h + 4, C_PV);
  }
  void evccLogo(int mx, int ty) {
    tri(mx + 0, ty + 0, mx + 5, ty + 0, mx - 5, ty + 10, C_EVCC);
    tri(mx + 5, ty + 0, mx + 1, ty + 7, mx - 5, ty + 10, C_EVCC);
    tri(mx - 5, ty + 10, mx + 1, ty + 7, mx + 5, ty + 7, C_EVCC);
    tri(mx - 5, ty + 10, mx + 5, ty + 7, mx + 0, ty + 10, C_EVCC);
    tri(mx + 5, ty + 7, mx - 2, ty + 18, mx + 0, ty + 10, C_EVCC);
    text("ev", mx - 6, ty + 16, F_MODE, C_WHITE, lgfx::textdatum_t::baseline_right);
    text("cc", mx + 8, ty + 16, F_MODE, C_WHITE, lgfx::textdatum_t::baseline_left);
  }
  // the bolt of the evcc logo on its own, scaled; (mx, ty) = centre / top edge
  void evccMark(int mx, int ty, float k, uint32_t col = C_EVCC) {
    auto T = [&](float ax, float ay, float bx, float by, float cx, float cy) {
      tri(mx + (int) (ax * k), ty + (int) (ay * k), mx + (int) (bx * k), ty + (int) (by * k),
          mx + (int) (cx * k), ty + (int) (cy * k), col);
    };
    T(0, 0, 5, 0, -5, 10);
    T(5, 0, 1, 7, -5, 10);
    T(-5, 10, 1, 7, 5, 7);
    T(-5, 10, 5, 7, 0, 10);
    T(5, 7, -2, 18, 0, 10);
  }
  void wallbox(int mx, int ty, uint32_t col) {
    rect(mx - 8, ty, 11, 18, col);
    rect(mx - 10, ty + 18, 15, 2, col);
    tri(mx - 1, ty + 3, mx - 6, ty + 10, mx - 2, ty + 10, C_BLACK);
    tri(mx - 4, ty + 8, mx + 1, ty + 8, mx - 5, ty + 15, C_BLACK);
    rect(mx + 3, ty + 5, 5, 2, col);
    rect(mx + 6, ty + 5, 2, 9, col);
    rect(mx + 5, ty + 13, 4, 5, col);
  }
  // vertical battery, (x, y) = top left of the body; dir +1 charging, -1 discharging
  void battery(int x, int y, int w, int h, float soc, int dir, float anim) {
    const int capW = w / 2, capH = 5;
    rect(x + (w - capW) / 2, y - capH, capW, capH, C_GREY);
    frame(x, y, w, h, C_GREY);
    frame(x + 1, y + 1, w - 2, h - 2, C_GREY);
    if (!isnan(soc)) {
      float s = soc < 0 ? 0 : (soc > 100 ? 100 : soc);
      int inner = h - 6;
      int fill = (int) (inner * s / 100.0f + 0.5f);
      if (dir > 0) fill = (int) (fill * anim + 0.5f);
      else if (dir < 0) fill = (int) (fill * (1.0f - anim) + 0.5f);
      uint32_t c = s < 20 ? C_BATT_LOW : (s < 50 ? C_BATT_MID : C_BATT_HIGH);
      if (fill > 0) rect(x + 3, y + 3 + inner - fill, w - 6, fill, c);
    }
  }
  void bolt(int x0, int y0, bool filled) {
    static const int BX[] = {5, 12, 7, 12, 2, 5, 0};
    static const int BY[] = {0, 0, 7, 7, 18, 10, 10};
#define P(i) x0 + BX[i], y0 + BY[i]
    if (filled) {
      tri(P(0), P(1), P(6), C_PHASE);
      tri(P(1), P(2), P(6), C_PHASE);
      tri(P(2), P(5), P(6), C_PHASE);
      tri(P(2), P(3), P(5), C_PHASE);
      tri(P(3), P(4), P(5), C_PHASE);
    }
#undef P
    for (int i = 0; i < 7; i++) {
      int j = (i + 1) % 7;
      line(x0 + BX[i], y0 + BY[i], x0 + BX[j], y0 + BY[j], C_PHASE);
    }
  }

  static String kw(float w) {
    if (isnan(w)) return "--";
    char buf[16];
    snprintf(buf, sizeof buf, "%.1f kW", w / 1000.0f);
    return buf;
  }
};

}  // namespace
