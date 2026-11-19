// Display: LilyGO T-Display-S3, ST7789 170x320 (8-bit parallel) via LovyanGFX.
// Landscape layout (320 x 170). The two buttons sit on the short edge next to the
// USB-C socket, one near each corner; hints for them ("Mode", "OK") are drawn in a
// column next to that edge, with arrows pointing at the buttons.
// The whole screen is rendered into one sprite (106 KB, plenty of RAM on the S3)
// and pushed in one go: no flicker.
#include "app.h"
#if defined(BOARD_TDISPLAY_S3)
#include "painter.h"

class LGFX : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 panel_;
  lgfx::Bus_Parallel8 bus_;

 public:
  LGFX() {
    auto b = bus_.config();
    b.freq_write = 20000000;
    b.pin_wr = 8;
    b.pin_rd = 9;
    b.pin_rs = 7;  // D/C
    b.pin_d0 = 39; b.pin_d1 = 40; b.pin_d2 = 41; b.pin_d3 = 42;
    b.pin_d4 = 45; b.pin_d5 = 46; b.pin_d6 = 47; b.pin_d7 = 48;
    bus_.config(b);
    panel_.setBus(&bus_);

    auto p = panel_.config();
    p.pin_cs = 6;
    p.pin_rst = 5;
    p.pin_busy = -1;
    p.panel_width = 170;
    p.panel_height = 320;
    p.offset_x = 35;
    p.offset_y = 0;
    p.readable = false;
    p.invert = true;
    p.rgb_order = false;
    p.dlen_16bit = false;
    p.bus_shared = false;
    panel_.config(p);
    setPanel(&panel_);
  }
};

static LGFX lcd;
static LGFX_Sprite screen(&lcd);
constexpr int W = 320, H = 170;

// Landscape orientation for setting 0°. Which end the buttons are on in that
// orientation and which button is at the top edge:
constexpr int ROT_BASE = 1;          // USB-C and buttons on the right
constexpr bool BUTTONS_RIGHT_AT_0 = true;
constexpr bool KEY_TOP_AT_0 = true;     // KEY (GPIO14) at the top edge, BOOT (GPIO0) at the bottom

// ---------------------------------------------------------------- Backlight (LEDC)
constexpr int BL_CH = 0;

void backlightSet(uint8_t percent) {
  if (percent > 100) percent = 100;
  ledcWrite(BL_CH, (uint32_t) percent * 255 / 100);
}

void displayBegin() {
  pinMode(PIN_LCD_POWER, OUTPUT);
  digitalWrite(PIN_LCD_POWER, HIGH);
  ledcSetup(BL_CH, 5000, 8);
  ledcAttachPin(PIN_BACKLIGHT, BL_CH);
  backlightSet(0);
  lcd.init();
  lcd.setRotation((ROT_BASE + (cfg.rotation & 2)) & 3);
  lcd.fillScreen(TFT_BLACK);
  screen.setColorDepth(16);
  screen.createSprite(W, H);
}

void displaySetRotation(uint8_t quarterTurns) {
  lcd.setRotation((ROT_BASE + (quarterTurns & 2)) & 3);
  lcd.fillScreen(TFT_BLACK);
}

// ---------------------------------------------------------------- Scene
namespace {

struct Painter : PainterBase {
  using PainterBase::PainterBase;

  // where the buttons are on screen for the current rotation and assignment
  bool buttonsRight() const { return BUTTONS_RIGHT_AT_0 == ((cfg.rotation & 2) == 0); }
  bool selectAtTop() const {
    bool keyTop = KEY_TOP_AT_0 == ((cfg.rotation & 2) == 0);
    bool selectIsKey = !cfg.swapButtons;   // default: KEY selects, BOOT confirms
    return keyTop == selectIsKey;
  }


  // hint next to a button: triangle pointing sideways at the button, the text (or,
  // with logo = true, the evcc bolt) in front of it
  void hint(const String &label, bool top, uint32_t col, bool logo = false) {
    const int y = top ? 20 : H - 20;       // buttons sit near the corners of the short edge
    if (buttonsRight()) tri(W - 11, y - 7, W - 11, y + 7, W - 2, y, col);
    else tri(10, y - 7, 10, y + 7, 1, y, col);
    if (logo) {
      evccMark(buttonsRight() ? W - 26 : 26, y - 14, 1.55f);
      return;
    }
    if (buttonsRight()) text(asciiLabel(label), W - 15, y, F_TINY, col, lgfx::textdatum_t::middle_right);
    else text(asciiLabel(label), 15, y, F_TINY, col, lgfx::textdatum_t::middle_left);
  }

  void main(const EvccState &s, const UiState &ui) {
    const int x0 = buttonsRight() ? 0 : 30;   // content area: 290 px wide, hints in the corners
    float anim = (float) min(ui.animStep, ui.animSteps) / ui.animSteps;

    // batteries: home left, car right
    int homeDir = 0;
    if (!isnan(s.battW)) {
      if (s.battW < -50) homeDir = 1;        // evcc: negative = charging
      else if (s.battW > 50) homeDir = -1;
    }
    // without home battery everything left of the values moves into its place
    const int shift = cfg.showHomeBatt ? 0 : 36;
    if (cfg.showHomeBatt) {
      battery(x0 + 10, 26, 24, 74, s.battSoc, homeDir, anim);
      house(x0 + 22, 108);
    }
    battery(x0 + 258, 26, 24, 74, s.carSoc, s.charging ? 1 : 0, anim);
    car(x0 + 270, 108);

    // PV and charge power: icon column + values
    const int ix = x0 + 68 - shift, vx = x0 + 238;  // values right-aligned at vx, before the car battery
    solar(ix, 12);
    text(Painter::kw(s.pvW), vx, 8, F_MID, C_PV, lgfx::textdatum_t::top_right, 0.9f);
    wallbox(ix + 1, 50, s.charging ? C_CHARGE : C_GREY);
    text(Painter::kw(s.chargeW), vx, 48, F_MID, C_CHARGE, lgfx::textdatum_t::top_right, 0.9f);

    // charge power bar (instead of the ring of the round display)
    const int bx = x0 + 46 - shift, bw = 200 + shift, by = 90, bh = 10;
    const int cx = bx + bw / 2;              // phases and mode centred under the bar
    float frac = isnan(s.chargeW) ? 0.0f : s.chargeW / 1000.0f / cfg.maxKw;
    frac = constrain(frac, 0.0f, 1.0f);
    if (s.charging) frac *= anim;
    int lit = (int) (bw * frac + 0.5f);
    rect(bx, by, bw, bh, C_TRACK);
    if (lit > 0) rect(bx, by, lit, bh, C_CHARGE);

    // phases below the bar, charge mode below the phases
    const int py = 108, my = 134, mh = 30, mw = 100, mx = cx - mw / 2;
    if (ui.pending >= 0) {
      frame(mx, my, mw, mh, C_PENDING);
      frame(mx + 1, my + 1, mw - 2, mh - 2, C_PENDING);
      text(asciiLabel(cfg.label[ui.pending]) + " ?", cx, my + mh / 2, F_MODE, C_PENDING,
           lgfx::textdatum_t::middle_center);
    } else {
      uint32_t mc = s.mode >= 0 ? MODE_COLOR[s.mode] : C_GREY;
      rect(mx, my, mw, mh, mc);
      text(s.mode >= 0 ? asciiLabel(cfg.label[s.mode]) : String("?"), cx, my + mh / 2,
           F_MODE, C_WHITE, lgfx::textdatum_t::middle_center);
    }
    if (!s.valid) {
      text("evcc offline", cx, py + 9, F_TINY, C_ERR, lgfx::textdatum_t::middle_center);
    } else {
      int nActive = s.charging ? s.phases : 0;
      for (int p = 0; p < 3; p++) bolt(cx - 26 + p * 20, py, p < nActive);
    }

    // button hints; the confirm hint lights up while a selection waits
    bool top = selectAtTop();
    hint(cfg.labelSelect, top, C_WHITE, cfg.evccLogo);   // evcc bolt instead of the text
    hint(cfg.labelConfirm, !top, ui.pending >= 0 ? C_PENDING : C_GREY);
  }

  void setup(const UiState &ui) {
    const int x = 14;
    text("Setup", x, 8, F_MID, C_PV, lgfx::textdatum_t::top_left, 0.9f);
    text("Connect to WiFi:", x, 50, F_TINY, C_GREY, lgfx::textdatum_t::top_left);
    text(ui.setupSsid, x + 150, 50, F_TINY, C_WHITE, lgfx::textdatum_t::top_left);
    text("then open:", x, 76, F_TINY, C_GREY, lgfx::textdatum_t::top_left);
    text(ui.ip, x + 150, 76, F_TINY, C_WHITE, lgfx::textdatum_t::top_left);
    text(ui.bootInfo, x, 140, F_TINY, C_GREY, lgfx::textdatum_t::top_left);
  }
};

}  // namespace

void displayRender(const EvccState &s, const UiState &ui) {
  screen.fillScreen(TFT_BLACK);
  Painter p(screen, 0);
  if (ui.setupMode) p.setup(ui);
  else p.main(s, ui);
  screen.pushSprite(0, 0);
}

#endif
