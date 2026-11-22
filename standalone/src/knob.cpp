// Rotary encoder (quadrature, interrupt driven) and push button.
#include "app.h"
#if !defined(BOARD_TDISPLAY_S3)

static volatile int32_t transitions = 0;   // valid quadrature transitions, signed
static volatile uint8_t lastAB = 0;
static volatile bool pressed = false;
static int32_t consumed = 0;

// one detent of this knob = 2 transitions (like ESPHome "resolution: 2")
constexpr int TRANSITIONS_PER_STEP = 2;

static void IRAM_ATTR onEncoder() {
  // state table: index = (previous AB << 2) | current AB
  static const int8_t TABLE[16] = {0, -1, 1, 0, 1, 0, 0, -1, -1, 0, 0, 1, 0, 1, -1, 0};
  uint8_t ab = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  transitions += TABLE[(lastAB << 2) | ab];
  lastAB = ab;
}

// The button is read in two ways: the interrupt latches short presses that happen
// while the main loop is busy rendering, the level is polled in knobTakePress().
static void IRAM_ATTR onButton() { pressed = true; }

constexpr uint32_t BUTTON_GUARD_MS = 150;   // ignore bounces after a press or release
static bool held = false;
static uint32_t buttonChangedAt = 0;
static uint32_t pressCount = 0;

void knobBegin() {
  pinMode(PIN_ENC_A, INPUT_PULLUP);
  pinMode(PIN_ENC_B, INPUT_PULLUP);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  lastAB = (digitalRead(PIN_ENC_A) << 1) | digitalRead(PIN_ENC_B);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_A), onEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_ENC_B), onEncoder, CHANGE);
  attachInterrupt(digitalPinToInterrupt(PIN_BUTTON), onButton, FALLING);
}

int knobTakeSteps() {
  int32_t t = transitions;
  int32_t steps = t / TRANSITIONS_PER_STEP - consumed;
  consumed += steps;
  return steps;
}

bool knobTakePress() {
  bool low = digitalRead(PIN_BUTTON) == LOW;
  bool edge = pressed;
  pressed = false;
  uint32_t now = millis();
  if (now - buttonChangedAt < BUTTON_GUARD_MS) return false;
  if (!held && (low || edge)) {
    held = true;
    buttonChangedAt = now;
    pressCount++;
    return true;
  }
  if (held && !low) {
    held = false;
    buttonChangedAt = now;
  }
  return false;
}

String knobButtonInfo() {
  return String(digitalRead(PIN_BUTTON) == LOW ? "pressed" : "released") + ", " + String(pressCount) + " presses";
}

#endif
