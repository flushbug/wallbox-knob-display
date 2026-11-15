// Wallbox knob display – standalone firmware
// Shows evcc data on the VIEWE UEDX24240013-MD50E and switches the charge mode
// with the knob. Talks directly to the evcc REST API; settings via a local web page.
//
// Operation: turn = preselect a charge mode (orange), press = send it to evcc.
// An unconfirmed selection expires after 8 s. After the display timeout the
// display goes dark; turning or pressing then only wakes it up.
#include <WiFi.h>
#include "app.h"

constexpr uint32_t PENDING_TIMEOUT_MS = 8000;
constexpr uint32_t WAKE_GUARD_MS = 600;     // knob steps right after waking up are ignored
constexpr uint32_t ANIM_INTERVAL_MS = 400;
constexpr uint32_t WIFI_CONNECT_MS = 20000;
constexpr uint32_t SETUP_RETRY_MS = 10UL * 60 * 1000;  // retry saved WiFi after 10 min in setup

static UiState ui;
static bool displayOn = false;
static uint32_t lastActivity = 0, wokeAt = 0, pendingAt = 0, lastAnim = 0, setupSince = 0;
static uint32_t lastVersion = 0;
static uint8_t appliedBrightness = 0;
static bool dirty = true;

static void wake() {
  displayOn = true;
  backlightSet(cfg.brightness);
  appliedBrightness = cfg.brightness;
  dirty = true;
}

static void sleepDisplay() {
  displayOn = false;
  ui.pending = -1;
  backlightSet(0);
}

// next enabled mode in direction dir (+1 / -1), starting from `from`
static int nextMode(int from, int dir) {
  int m = from;
  for (int i = 0; i < MODE_COUNT; i++) {
    m = (m + dir + MODE_COUNT) % MODE_COUNT;
    if (cfg.modeEnabled[m]) return m;
  }
  return from;
}

static bool connectWifi() {
  if (cfg.wifiSsid.isEmpty()) return false;
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(cfg.hostname.c_str());
  WiFi.setAutoReconnect(true);
  WiFi.begin(cfg.wifiSsid.c_str(), cfg.wifiPass.c_str());
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_CONNECT_MS) delay(100);
  return WiFi.status() == WL_CONNECTED;
}

static void startSetupAp() {
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char ssid[32];
  snprintf(ssid, sizeof ssid, "WallboxKnob-%02X%02X", mac[4], mac[5]);
  WiFi.mode(WIFI_AP);
  WiFi.setTxPower(WIFI_POWER_8_5dBm);     // phone is close during setup; full power makes the C3 AP unstable
  WiFi.softAP(ssid, nullptr, 6);          // open network, fixed channel 6
  WiFi.setTxPower(WIFI_POWER_8_5dBm);
  ui.setupMode = true;
  ui.setupSsid = ssid;
  ui.ip = WiFi.softAPIP().toString();
  setupSince = millis();
}

static void logWifiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_AP_STACONNECTED:
      Serial.printf("[%lu] AP: client connected (aid %d)\n", millis(), info.wifi_ap_staconnected.aid);
      break;
    case ARDUINO_EVENT_WIFI_AP_STADISCONNECTED:
      Serial.printf("[%lu] AP: client disconnected (aid %d)\n", millis(), info.wifi_ap_stadisconnected.aid);
      break;
    case ARDUINO_EVENT_WIFI_AP_STAIPASSIGNED:
      Serial.printf("[%lu] AP: client got IP\n", millis());
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial.printf("[%lu] WiFi: connected, IP %s\n", millis(), WiFi.localIP().toString().c_str());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial.printf("[%lu] WiFi: disconnected, reason %d\n", millis(), info.wifi_sta_disconnected.reason);
      break;
    default:
      break;
  }
}

// survives resets (not power-off): counts boots to spot reset loops
RTC_NOINIT_ATTR static uint32_t bootMagic, bootCount;

static const char *resetName(esp_reset_reason_t r) {
  switch (r) {
    case ESP_RST_POWERON: return "power on";
    case ESP_RST_SW: return "restart";
    case ESP_RST_PANIC: return "crash";
    case ESP_RST_INT_WDT:
    case ESP_RST_TASK_WDT:
    case ESP_RST_WDT: return "watchdog";
    case ESP_RST_BROWNOUT: return "brownout";
    default: return "other";
  }
}

void setup() {
  if (bootMagic != 0xB007C0DE || esp_reset_reason() == ESP_RST_POWERON) {
    bootMagic = 0xB007C0DE;
    bootCount = 0;
  }
  bootCount++;
  ui.bootInfo = "boot " + String(bootCount) + ", " + resetName(esp_reset_reason());
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   // never block when no USB host is reading
  Serial.printf("\nWallbox knob %s, reset reason %d\n", FW_VERSION, (int) esp_reset_reason());
  WiFi.onEvent(logWifiEvent);
  configLoad();
  displayBegin();
  knobBegin();

  bool online = connectWifi();
  if (!online) startSetupAp();
  else ui.ip = WiFi.localIP().toString();

  webBegin(ui.setupMode);
  if (!ui.setupMode) evccBegin();

  lastActivity = millis();
  wake();
}

void loop() {
  webLoop();
  uint32_t now = millis();

  // ---- setup mode: only show the instructions, retry the saved WiFi now and then
  if (ui.setupMode) {
    if (!cfg.wifiSsid.isEmpty() && now - setupSince > SETUP_RETRY_MS) ESP.restart();
    if (dirty) {
      displayRender(EvccState(), ui);
      dirty = false;
    }
    delay(5);
    return;
  }

  EvccState s = evccGet();

  // ---- knob and button
  int steps = knobTakeSteps();
  bool press = knobTakePress();
  if (steps != 0 || press) {
    if (!displayOn) {
      wokeAt = now;
      wake();
    } else {
      if (steps != 0 && now - wokeAt > WAKE_GUARD_MS) {
        int cur = ui.pending >= 0 ? ui.pending : (s.mode >= 0 ? s.mode : MODE_OFF);
        int dir = steps > 0 ? 1 : -1;
        for (int i = 0; i < abs(steps); i++) cur = nextMode(cur, dir);
        ui.pending = cur;
        pendingAt = now;
        dirty = true;
      }
      if (press && ui.pending >= 0) {
        evccSetMode(ui.pending);
        ui.pending = -1;
        dirty = true;
      }
    }
    lastActivity = now;
  }

  // ---- unconfirmed selection expires
  if (ui.pending >= 0 && now - pendingAt > PENDING_TIMEOUT_MS) {
    ui.pending = -1;
    dirty = true;
  }

  // ---- display timeout
  if (displayOn && cfg.timeoutS > 0 && now - lastActivity > (uint32_t) cfg.timeoutS * 1000) sleepDisplay();
  if (displayOn && appliedBrightness != cfg.brightness) {
    backlightSet(cfg.brightness);
    appliedBrightness = cfg.brightness;
  }

  // ---- animation while charging or the home battery is active
  bool active = s.charging || (!isnan(s.battW) && fabsf(s.battW) > 50);
  if (displayOn && ui.pending < 0 && active && now - lastAnim >= ANIM_INTERVAL_MS) {
    lastAnim = now;
    ui.animStep = (ui.animStep + 1) % (ui.animSteps + 3);
    dirty = true;
  }

  // ---- new data
  uint32_t v = evccVersion();
  if (v != lastVersion) {
    lastVersion = v;
    dirty = true;
  }

  if (displayOn && dirty) {
    displayRender(s, ui);
    dirty = false;
  }
  delay(5);
}
