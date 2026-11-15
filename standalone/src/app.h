// Shared types and functions of the standalone wallbox knob firmware.
#pragma once
#include <Arduino.h>

// ---------------------------------------------------------------- Pins (VIEWE UEDX24240013-MD50E)
constexpr int PIN_SPI_CLK = 1;
constexpr int PIN_SPI_MOSI = 0;
constexpr int PIN_LCD_CS = 10;
constexpr int PIN_LCD_DC = 4;
constexpr int PIN_LCD_RST = 2;
constexpr int PIN_BACKLIGHT = 8;   // PWM, inverted (low = bright)
constexpr int PIN_ENC_A = 6;
constexpr int PIN_ENC_B = 7;
constexpr int PIN_BUTTON = 9;      // low = pressed

// ---------------------------------------------------------------- Charge modes
// Index into the evcc mode names. The knob cycles through the enabled ones.
enum Mode : uint8_t { MODE_OFF = 0, MODE_SMART, MODE_NOW, MODE_COUNT };

// ---------------------------------------------------------------- Settings (stored in flash)
struct Config {
  String wifiSsid;
  String wifiPass;
  String hostname = "wallbox-knob";
  String adminPass;                 // web UI (user "admin") and OTA; empty = no protection
  String evccUrl = "http://evcc.local:7070";
  uint8_t loadpoint = 1;            // evcc loadpoint number, 1-based
  String evccPass;                  // evcc admin password, only needed if evcc requires login
  String evccToken;                 // alternatively an evcc API key (Bearer), takes precedence
  uint16_t pollMs = 2000;
  uint16_t timeoutS = 120;          // display timeout, 0 = never off
  float maxKw = 22.0f;              // full scale of the charge ring
  bool modeEnabled[MODE_COUNT] = {true, true, true};
  String label[MODE_COUNT] = {"Off", "Smart", "Fast"};
  // evcc mode names; current evcc: off / smart / now (older versions: off / pv / now, plus minpv)
  String modeApi[MODE_COUNT] = {"off", "smart", "now"};
  String labelConfirm = "Press = OK";
  bool evccLogo = true;             // true = evcc logo, false = charging station icon
  uint8_t brightness = 100;         // %
};
extern Config cfg;
void configLoad();
void configSave();
void configReset();

// ---------------------------------------------------------------- evcc data
struct EvccState {
  bool valid = false;               // last poll succeeded
  float pvW = NAN;
  float chargeW = NAN;
  float battSoc = NAN;
  float battW = NAN;                // evcc: positive = discharging, negative = charging
  float carSoc = NAN;
  bool charging = false;
  bool connected = false;
  int phases = 0;
  int mode = -1;                    // Mode index or -1 = unknown
  String error;                     // last error message, empty if ok
  uint32_t updatedAt = 0;
};
void evccBegin();                   // starts the background poll task
EvccState evccGet();                // thread-safe copy of the latest state
void evccSetMode(int mode);         // queues a mode change
uint32_t evccVersion();             // increments on every new state

// ---------------------------------------------------------------- Display
struct UiState {
  int pending = -1;                 // preselected mode or -1
  int animStep = 0;
  int animSteps = 8;
  bool setupMode = false;           // WiFi access point active
  String bootInfo;                  // boot counter and last reset reason (diagnostics)
  String setupSsid;
  String ip;
};
void displayBegin();
void displayRender(const EvccState &s, const UiState &ui);
void backlightSet(uint8_t percent); // 0 = off
String asciiLabel(const String &s); // ä -> ae etc. (display fonts are ASCII only)

// ---------------------------------------------------------------- Knob
void knobBegin();
int knobTakeSteps();                // signed steps since last call
bool knobTakePress();               // true once per press

// ---------------------------------------------------------------- Web UI + OTA
void webBegin(bool setupMode);
void webLoop();
