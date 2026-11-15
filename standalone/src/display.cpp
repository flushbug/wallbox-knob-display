// Display: GC9A01 240x240 via LovyanGFX.
// The screen is rendered in horizontal bands into a small sprite (240 x 48 px,
// 23 KB) instead of a full frame buffer (115 KB), so the ESP32-C3 keeps enough
// RAM for WiFi, HTTP and JSON. Each band is drawn completely, then pushed:
// no flicker.
#define LGFX_USE_V1
#include <LovyanGFX.hpp>
#include "app.h"

#ifndef LCD_INVERT
#define LCD_INVERT true   // the GC9A01 on this board needs inverted colours
#endif

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_GC9A01 panel_;
  lgfx::Bus_SPI bus_;

 public:
  LGFX() {
    auto b = bus_.config();
    b.spi_host = SPI2_HOST;
    b.spi_mode = 0;
    b.freq_write = 40000000;
    b.pin_sclk = PIN_SPI_CLK;
    b.pin_mosi = PIN_SPI_MOSI;
    b.pin_miso = -1;
    b.pin_dc = PIN_LCD_DC;
    b.dma_channel = SPI_DMA_CH_AUTO;
    bus_.config(b);
    panel_.setBus(&bus_);

    auto p = panel_.config();
    p.pin_cs = PIN_LCD_CS;
    p.pin_rst = PIN_LCD_RST;
    p.pin_busy = -1;
    p.panel_width = 240;
    p.panel_height = 240;
    p.invert = LCD_INVERT;
    p.readable = false;
    panel_.config(p);
    setPanel(&panel_);
  }
};

static LGFX lcd;
static LGFX_Sprite band(&lcd);
constexpr int BAND_H = 48;

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

// ---------------------------------------------------------------- Backlight (LEDC, inverted)
constexpr int BL_CH = 0;

void backlightSet(uint8_t percent) {
  if (percent > 100) percent = 100;
  uint32_t duty = (uint32_t) percent * 255 / 100;
  ledcWrite(BL_CH, 255 - duty);
}

void displayBegin() {
  ledcSetup(BL_CH, 5000, 8);
  ledcAttachPin(PIN_BACKLIGHT, BL_CH);
  backlightSet(0);
  lcd.init();
  lcd.setRotation(0);
  lcd.fillScreen(TFT_BLACK);
  band.setColorDepth(16);
  band.createSprite(240, BAND_H);
}

String asciiLabel(const String &in) {
  String s = in;
  s.replace("ä", "ae"); s.replace("ö", "oe"); s.replace("ü", "ue");
  s.replace("Ä", "Ae"); s.replace("Ö", "Oe"); s.replace("Ü", "Ue");
  s.replace("ß", "ss");
  String out;
  for (char c : s) out += (c >= 32 && c < 127) ? c : '?';
  return out;
}

// ---------------------------------------------------------------- Scene
// All drawing functions take screen coordinates; oy shifts them into the band.
namespace {

struct Painter {
  LGFX_Sprite &g;
  int oy;

  int Y(int y) const { return y + oy; }

  // ring segment, angles in degrees (0 = 3 o'clock, clockwise), may exceed 360
  void arc(float a0, float a1, uint32_t col) {
    if (a1 <= a0) return;
    if (a1 > 360.0f && a0 < 360.0f) {
      g.fillArc(120, Y(120), 110, 119, a0, 360.0f, col);
      g.fillArc(120, Y(120), 110, 119, 0.0f, a1 - 360.0f, col);
    } else if (a0 >= 360.0f) {
      g.fillArc(120, Y(120), 110, 119, a0 - 360.0f, a1 - 360.0f, col);
    } else {
      g.fillArc(120, Y(120), 110, 119, a0, a1, col);
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
  void wallbox(int mx, int ty, uint32_t col) {
    rect(mx - 8, ty, 11, 18, col);
    rect(mx - 10, ty + 18, 15, 2, col);
    tri(mx - 1, ty + 3, mx - 6, ty + 10, mx - 2, ty + 10, C_BLACK);
    tri(mx - 4, ty + 8, mx + 1, ty + 8, mx - 5, ty + 15, C_BLACK);
    rect(mx + 3, ty + 5, 5, 2, col);
    rect(mx + 6, ty + 5, 2, 9, col);
    rect(mx + 5, ty + 13, 4, 5, col);
  }
  void battery(int x, float soc, bool isCar, int dir, float anim) {
    const int w = 22, h = 60, y = 80, capW = 10, capH = 5;
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
    if (isCar) car(x + w / 2, y + h + 6);
    else house(x + w / 2, y + h + 6);
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

  void main(const EvccState &s, const UiState &ui) {
    const int cx = 120;
    float anim = (float) min(ui.animStep, ui.animSteps) / ui.animSteps;

    // ring: charge power 0..maxKw over 270°, bottom left (135°) clockwise to bottom right
    float frac = isnan(s.chargeW) ? 0.0f : s.chargeW / 1000.0f / cfg.maxKw;
    frac = constrain(frac, 0.0f, 1.0f);
    if (s.charging) frac *= anim;
    float lit = 135.0f + 270.0f * frac;
    arc(135.0f, lit, C_CHARGE);
    arc(lit, 405.0f, C_TRACK);

    // batteries: home left, car right
    int homeDir = 0;
    if (!isnan(s.battW)) {
      if (s.battW < -50) homeDir = 1;        // evcc: negative = charging
      else if (s.battW > 50) homeDir = -1;
    }
    battery(30, s.battSoc, false, homeDir, anim);
    battery(188, s.carSoc, true, s.charging ? 1 : 0, anim);

    // PV and charge power
    solar(cx, 32);
    text(kw(s.pvW), cx, 56, F_MID, C_PV, lgfx::textdatum_t::top_center, 0.9f);
    if (cfg.evccLogo) evccLogo(cx, 98);
    else wallbox(cx, 97, s.charging ? C_CHARGE : C_GREY);
    text(kw(s.chargeW), cx, 124, F_MID, C_CHARGE, lgfx::textdatum_t::top_center, 0.9f);

    // charge mode
    if (ui.pending >= 0) {
      text(asciiLabel(cfg.label[ui.pending]) + " ?", cx, 168, F_MODE, C_PENDING,
           lgfx::textdatum_t::top_center);
      text(asciiLabel(cfg.labelConfirm), cx, 196, F_TINY, C_GREY, lgfx::textdatum_t::top_center);
      return;
    }
    uint32_t mc = s.mode >= 0 ? MODE_COLOR[s.mode] : C_GREY;
    rect(72, 166, 96, 30, mc);
    text(s.mode >= 0 ? asciiLabel(cfg.label[s.mode]) : String("?"), cx, 181, F_MODE, C_WHITE,
         lgfx::textdatum_t::middle_center);

    if (!s.valid) {  // evcc not reachable: hint instead of the phases
      text("evcc offline", cx, 203, F_TINY, C_ERR, lgfx::textdatum_t::top_center);
      return;
    }
    int nActive = s.charging ? s.phases : 0;
    const int bw = 12, gap = 8, by = 201;
    int bx = cx - (3 * bw + 2 * gap) / 2;
    for (int p = 0; p < 3; p++) bolt(bx + p * (bw + gap), by, p < nActive);
  }

  void setup(const UiState &ui) {
    text("Setup", 120, 62, F_MID, C_PV, lgfx::textdatum_t::top_center, 0.9f);
    text("Connect to WiFi:", 120, 104, F_TINY, C_GREY, lgfx::textdatum_t::top_center);
    text(ui.setupSsid, 120, 126, F_TINY, C_WHITE, lgfx::textdatum_t::top_center);
    text("then open:", 120, 152, F_TINY, C_GREY, lgfx::textdatum_t::top_center);
    text(ui.ip, 120, 174, F_TINY, C_WHITE, lgfx::textdatum_t::top_center);
    text(ui.bootInfo, 120, 204, F_TINY, C_GREY, lgfx::textdatum_t::top_center);
  }
};

}  // namespace

void displayRender(const EvccState &s, const UiState &ui) {
  for (int y0 = 0; y0 < 240; y0 += BAND_H) {
    band.fillScreen(TFT_BLACK);
    Painter p{band, -y0};
    if (ui.setupMode) p.setup(ui);
    else p.main(s, ui);
    band.pushSprite(0, y0);
  }
}
