// Display: GC9A01 240x240 via LovyanGFX.
// The screen is rendered in horizontal bands into a small sprite (240 x 48 px,
// 23 KB) instead of a full frame buffer (115 KB), so the ESP32-C3 keeps enough
// RAM for WiFi, HTTP and JSON. Each band is drawn completely, then pushed:
// no flicker.
#include "app.h"
#if !defined(BOARD_TDISPLAY_S3)
#include "painter.h"

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
  lcd.setRotation(cfg.rotation & 3);
  lcd.fillScreen(TFT_BLACK);
  band.setColorDepth(16);
  band.createSprite(240, BAND_H);
}

void displaySetRotation(uint8_t quarterTurns) {
  lcd.setRotation(quarterTurns & 3);
  lcd.fillScreen(TFT_BLACK);
}

namespace {

struct Painter : PainterBase {
  using PainterBase::PainterBase;

  void main(const EvccState &s, const UiState &ui) {
    const int cx = 120;
    float anim = (float) min(ui.animStep, ui.animSteps) / ui.animSteps;

    // ring: charge power 0..maxKw over 270°, bottom left (135°) clockwise to bottom right
    float frac = isnan(s.chargeW) ? 0.0f : s.chargeW / 1000.0f / cfg.maxKw;
    frac = constrain(frac, 0.0f, 1.0f);
    if (s.charging) frac *= anim;
    float lit = 135.0f + 270.0f * frac;
    arc(120, 120, 110, 119, 135.0f, lit, C_CHARGE);
    arc(120, 120, 110, 119, lit, 405.0f, C_TRACK);

    // batteries: home left, car right
    int homeDir = 0;
    if (!isnan(s.battW)) {
      if (s.battW < -50) homeDir = 1;        // evcc: negative = charging
      else if (s.battW > 50) homeDir = -1;
    }
    if (cfg.showHomeBatt) {
      battery(30, 80, 22, 60, s.battSoc, homeDir, anim);
      house(30 + 11, 146);
    }
    battery(188, 80, 22, 60, s.carSoc, s.charging ? 1 : 0, anim);
    car(188 + 11, 146);

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
    Painter p(band, -y0);
    if (ui.setupMode) p.setup(ui);
    else p.main(s, ui);
    band.pushSprite(0, y0);
  }
}

#endif
