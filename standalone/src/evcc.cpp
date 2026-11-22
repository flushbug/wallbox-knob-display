// evcc REST API client, runs in its own FreeRTOS task so that slow HTTP
// requests never block the knob or the display.
//   read:  GET  <url>/api/state
//   write: POST <url>/api/loadpoints/<n>/mode/<mode>
//   auth:  Bearer API key, or POST <url>/api/auth/login {"password": ...} -> cookie "auth"
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "app.h"

static SemaphoreHandle_t lock;
static EvccState state;
static volatile uint32_t version = 0;
static volatile int pendingMode = -1;
static String authCookie;
static String lastCommand = "none yet";

String evccLastCommand() {
  if (!lock) return lastCommand;
  xSemaphoreTake(lock, portMAX_DELAY);
  String c = lastCommand;
  xSemaphoreGive(lock);
  return c;
}

EvccState evccGet() {
  if (!lock) return state;   // setup mode: client not started, nothing polled yet
  xSemaphoreTake(lock, portMAX_DELAY);
  EvccState copy = state;
  xSemaphoreGive(lock);
  return copy;
}

uint32_t evccVersion() { return version; }

void evccSetMode(int mode) { pendingMode = mode; }

static String baseUrl() {
  String u = cfg.evccUrl;
  u.trim();
  while (u.endsWith("/")) u.remove(u.length() - 1);
  if (!u.startsWith("http://") && !u.startsWith("https://")) u = "http://" + u;
  return u;
}

static void addAuth(HTTPClient &http) {
  if (!cfg.evccToken.isEmpty()) http.addHeader("Authorization", "Bearer " + cfg.evccToken);
  else if (!authCookie.isEmpty()) http.addHeader("Cookie", authCookie);
}

static bool login() {
  if (cfg.evccPass.isEmpty()) return false;
  HTTPClient http;
  http.setTimeout(3000);
  if (!http.begin(baseUrl() + "/api/auth/login")) return false;
  const char *keys[] = {"Set-Cookie"};
  http.collectHeaders(keys, 1);
  http.addHeader("Content-Type", "application/json");
  JsonDocument body;
  body["password"] = cfg.evccPass;
  String payload;
  serializeJson(body, payload);
  int code = http.POST(payload);
  String cookie = http.header("Set-Cookie");
  http.end();
  int semi = cookie.indexOf(';');
  if (semi > 0) cookie = cookie.substring(0, semi);
  if (code == 200 && cookie.startsWith("auth=")) {
    authCookie = cookie;
    return true;
  }
  authCookie = "";
  return false;
}

static int modeIndex(const char *m) {
  if (!m) return -1;
  for (int i = 0; i < MODE_COUNT; i++)
    if (cfg.modeApi[i] == m) return i;
  // older evcc: pv and minpv are the solar modes
  if (!strcmp(m, "pv") || !strcmp(m, "minpv")) return MODE_SMART;
  return -1;
}

static float num(JsonVariantConst v) { return v.is<float>() ? v.as<float>() : NAN; }

static void poll() {
  EvccState next;
  if (WiFi.status() != WL_CONNECTED) {
    next.error = "no WiFi";
  } else {
    HTTPClient http;
    http.setTimeout(3000);
    http.useHTTP10(true);  // no chunked encoding, so the body can be streamed into the parser
    if (!http.begin(baseUrl() + "/api/state")) {
      next.error = "invalid evcc URL";
    } else {
      addAuth(http);
      int code = http.GET();
      if (code != 200) {
        next.error = code > 0 ? "HTTP " + String(code) : HTTPClient::errorToString(code);
      } else {
        // only keep the fields we need; the full state is far too large for the C3
        JsonDocument filter;
        for (JsonObject f : {filter.to<JsonObject>(), filter["result"].to<JsonObject>()}) {
          f["pvPower"] = true;
          f["batterySoc"] = true;     // evcc before ~0.300
          f["batteryPower"] = true;
          f["battery"]["soc"] = true;  // current evcc: battery { soc, power }
          f["battery"]["power"] = true;
          JsonObject lp = f["loadpoints"].add<JsonObject>();
          for (const char *k : {"chargePower", "charging", "connected", "mode", "phasesActive", "vehicleSoc"})
            lp[k] = true;
        }
        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
        if (err) {
          next.error = String("JSON: ") + err.c_str();
        } else {
          // evcc before 0.200 wraps the state in "result"
          JsonObjectConst root = doc["result"].is<JsonObject>() ? doc["result"].as<JsonObjectConst>()
                                                                : doc.as<JsonObjectConst>();
          next.pvW = num(root["pvPower"]);
          JsonObjectConst batt = root["battery"];
          next.battSoc = batt ? num(batt["soc"]) : num(root["batterySoc"]);
          next.battW = batt ? num(batt["power"]) : num(root["batteryPower"]);
          JsonArrayConst lps = root["loadpoints"];
          int idx = cfg.loadpoint - 1;
          if (idx < 0 || idx >= (int) lps.size()) {
            next.error = "loadpoint " + String(cfg.loadpoint) + " not found";
          } else {
            JsonObjectConst lp = lps[idx];
            next.chargeW = num(lp["chargePower"]);
            next.charging = lp["charging"] | false;
            next.connected = lp["connected"] | false;
            next.phases = lp["phasesActive"] | 0;
            next.carSoc = next.connected ? num(lp["vehicleSoc"]) : NAN;
            next.mode = modeIndex(lp["mode"]);
            next.valid = true;
          }
        }
      }
      http.end();
    }
  }
  next.updatedAt = millis();
  xSemaphoreTake(lock, portMAX_DELAY);
  state = next;
  xSemaphoreGive(lock);
  version++;
}

static bool postMode(int mode) {
  String url = baseUrl() + "/api/loadpoints/" + String(cfg.loadpoint) + "/mode/" + cfg.modeApi[mode];
  for (int attempt = 0; attempt < 2; attempt++) {
    HTTPClient http;
    http.setTimeout(3000);
    if (!http.begin(url)) return false;
    addAuth(http);
    int code = http.POST("");
    http.end();
    String result = cfg.modeApi[mode] + ": " + (code > 0 ? "HTTP " + String(code) : HTTPClient::errorToString(code));
    Serial.printf("[%lu] mode %s\n", millis(), result.c_str());
    xSemaphoreTake(lock, portMAX_DELAY);
    lastCommand = result;
    xSemaphoreGive(lock);
    if (code == 200) return true;
    if (code == 401 && attempt == 0 && cfg.evccToken.isEmpty() && login()) continue;
    return false;
  }
  return false;
}

static void task(void *) {
  uint32_t lastPoll = 0;
  for (;;) {
    int m = pendingMode;
    if (m >= 0) {
      pendingMode = -1;
      postMode(m);
      lastPoll = 0;  // show the result right away
    }
    if (lastPoll == 0 || millis() - lastPoll >= cfg.pollMs) {
      lastPoll = millis();
      poll();
    }
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void evccBegin() {
  lock = xSemaphoreCreateMutex();
  xTaskCreate(task, "evcc", 8192, nullptr, 1, nullptr);
}
