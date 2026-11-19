// Two push buttons (LilyGO T-Display-S3): one selects the next charge mode,
// the other confirms it. Mapped onto the knob interface used by main.cpp.
// Polled from the main loop (every few ms) with a stable-state debounce: a press
// counts once the pin has been low for DEBOUNCE_MS after being released.
#include "app.h"
#if defined(BOARD_TDISPLAY_S3)

constexpr uint32_t DEBOUNCE_MS = 30;

struct Button {
  int pin;
  const char *name;
  bool stable = false;     // debounced state, true = pressed
  bool raw = false;
  uint32_t changedAt = 0;

  Button(int p, const char *n) : pin(p), name(n) {}

  // true once per press
  bool update() {
    bool now = digitalRead(pin) == LOW;
    uint32_t t = millis();
    if (now != raw) {
      raw = now;
      changedAt = t;
    }
    if (raw != stable && t - changedAt >= DEBOUNCE_MS) {
      stable = raw;
      if (stable) {
        Serial.printf("[%lu] button %s pressed\n", t, name);
        return true;
      }
    }
    return false;
  }
};

static Button btnBoot{PIN_BTN_BOOT, "BOOT (GPIO0)"};
static Button btnKey{PIN_BTN_KEY, "KEY (GPIO14)"};
static int pendingSelect = 0;
static bool pendingConfirm = false;

void knobBegin() {
  pinMode(PIN_BTN_BOOT, INPUT_PULLUP);
  pinMode(PIN_BTN_KEY, INPUT_PULLUP);
}

// default: KEY (GPIO14) selects, BOOT (GPIO0) confirms; swapButtons exchanges them
static void poll() {
  bool b = btnBoot.update(), k = btnKey.update();
  if (cfg.swapButtons ? b : k) pendingSelect++;
  if (cfg.swapButtons ? k : b) pendingConfirm = true;
}

int knobTakeSteps() {
  poll();
  int n = pendingSelect;
  pendingSelect = 0;
  return n;
}

bool knobTakePress() {
  poll();
  bool p = pendingConfirm;
  pendingConfirm = false;
  return p;
}

#endif
