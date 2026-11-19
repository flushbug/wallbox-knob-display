// Local web UI: settings, status, firmware update. Plus ArduinoOTA and mDNS.
// In setup mode (no WiFi configured or not reachable) the device opens its own
// access point with a captive portal pointing to this page.
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include "app.h"

static WebServer server(80);
static DNSServer dns;
static bool setup_ = false;
static bool rebootPending = false;
static uint32_t rebootAt = 0;

static String esc(const String &s) {
  String o;
  o.reserve(s.length() + 8);
  for (char c : s) {
    switch (c) {
      case '&': o += "&amp;"; break;
      case '<': o += "&lt;"; break;
      case '>': o += "&gt;"; break;
      case '"': o += "&quot;"; break;
      default: o += c;
    }
  }
  return o;
}

// Basic auth with user "admin" if an admin password is set (not in setup mode).
static bool authorized() {
  if (setup_ || cfg.adminPass.isEmpty()) return true;
  if (server.authenticate("admin", cfg.adminPass.c_str())) return true;
  server.requestAuthentication(BASIC_AUTH, "Wallbox Knob");
  return false;
}

static const char HEAD[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1"><title>Wallbox Knob</title>
<style>
:root{color-scheme:light dark;--bg:#f5f5f5;--card:#fff;--fg:#222;--mut:#666;--acc:#2196F3;--bd:#ddd}
@media(prefers-color-scheme:dark){:root{--bg:#121212;--card:#1e1e1e;--fg:#eee;--mut:#aaa;--bd:#333}}
body{font-family:system-ui,sans-serif;background:var(--bg);color:var(--fg);margin:0;padding:16px}
main{max-width:560px;margin:auto}h1{font-size:22px;margin:4px 0 16px}
section{background:var(--card);border:1px solid var(--bd);border-radius:12px;padding:12px 16px;margin-bottom:16px}
h2{font-size:16px;margin:4px 0 12px}label{display:block;font-size:14px;color:var(--mut);margin:10px 0 4px}
input[type=text],input[type=password],input[type=number],input[type=url],select{width:100%;box-sizing:border-box;padding:8px;
border:1px solid var(--bd);border-radius:8px;background:var(--bg);color:var(--fg);font-size:15px}
.row{display:grid;grid-template-columns:auto 1fr;gap:6px 10px;align-items:center}
.row label{margin:0}button{padding:9px 16px;border-radius:8px;border:1px solid var(--acc);background:var(--acc);
color:#fff;font-size:15px;cursor:pointer}button.sec{background:transparent;color:var(--acc)}
small{color:var(--mut)}table{width:100%;font-size:14px}td:last-child{text-align:right}
.err{color:#F44336}.ok{color:#4CAF50}
</style></head><body><main>)HTML";

static String fmt(float v, const char *unit, float div = 1.0f, int dec = 1) {
  if (isnan(v)) return "–";
  return String(v / div, dec) + " " + unit;
}

static void handleRoot() {
  if (!authorized()) return;
  EvccState s = evccGet();
  String h;
  h.reserve(9000);
  h += FPSTR(HEAD);
  h += "<h1>Wallbox Knob</h1>";

  // ---- status
  h += "<section><h2>Status</h2><table>";
  h += "<tr><td>evcc</td><td class='" + String(s.valid ? "ok'>connected" : "err'>" + esc(s.error)) + "</td></tr>";
  if (s.valid) {
    h += "<tr><td>PV power</td><td>" + fmt(s.pvW, "kW", 1000) + "</td></tr>";
    h += "<tr><td>Charge power</td><td>" + fmt(s.chargeW, "kW", 1000) + "</td></tr>";
    h += "<tr><td>Mode</td><td>" + String(s.mode >= 0 ? cfg.modeApi[s.mode].c_str() : "?") + "</td></tr>";
    h += "<tr><td>Vehicle SoC</td><td>" + fmt(s.carSoc, "%", 1, 0) + "</td></tr>";
    h += "<tr><td>Home battery</td><td>" + fmt(s.battSoc, "%", 1, 0) + "</td></tr>";
  }
  h += "<tr><td>IP</td><td>" + (setup_ ? WiFi.softAPIP().toString() : WiFi.localIP().toString()) + "</td></tr>";
  h += "<tr><td>Firmware</td><td>" FW_VERSION "</td></tr>";
  h += "<tr><td>Board</td><td>" + String(BOARD_NAME) + "</td></tr>";
  h += "</table></section>";

  // ---- settings
  h += "<form method='post' action='/save'>";
  h += "<section><h2>WiFi</h2>";
  h += "<label>SSID</label><input type='text' name='ssid' value='" + esc(cfg.wifiSsid) + "'>";
  h += "<label>Password</label><input type='password' name='wpass' placeholder='unchanged'>";
  h += "<label>Hostname</label><input type='text' name='host' value='" + esc(cfg.hostname) + "'>";
  h += "<small>Reachable as http://" + esc(cfg.hostname) + ".local</small></section>";

  h += "<section><h2>evcc</h2>";
  h += "<label>evcc address</label><input type='text' name='url' value='" + esc(cfg.evccUrl) + "' placeholder='http://192.168.1.10:7070'>";
  h += "<label>Loadpoint number</label><input type='number' min='1' max='10' name='lp' value='" + String(cfg.loadpoint) + "'>";
  h += "<label>evcc admin password (only if evcc requires a login)</label><input type='password' name='epass' placeholder='" + String(cfg.evccPass.isEmpty() ? "not set" : "unchanged") + "'>";
  h += "<label>or evcc API key</label><input type='password' name='etoken' placeholder='" + String(cfg.evccToken.isEmpty() ? "not set" : "unchanged") + "'>";
  h += "<label>Poll interval (ms)</label><input type='number' min='500' max='60000' step='100' name='poll' value='" + String(cfg.pollMs) + "'>";
  h += "</section>";

  h += "<section><h2>Display</h2>";
  h += "<label>Display timeout (s, 0 = never off)</label><input type='number' min='0' max='3600' name='tout' value='" + String(cfg.timeoutS) + "'>";
  h += "<label>Brightness (%)</label><input type='number' min='5' max='100' name='bright' value='" + String(cfg.brightness) + "'>";
  h += "<label>Rotation (clockwise, depending on the mounting position)</label><select name='rot'>";
  for (int r = 0; r < 4; r += HAS_KNOB ? 1 : 2)   // rectangular display: landscape only
    h += "<option value='" + String(r) + "'" + String(cfg.rotation == r ? " selected" : "") + ">" + String(r * 90) + "&deg;</option>";
  h += "</select>";
  h += String("<label>Full scale of the charge ") + (HAS_KNOB ? "ring" : "bar") + " (kW)</label><input type='number' min='1' max='100' step='0.1' name='maxkw' value='" + String(cfg.maxKw, 1) + "'>";
  h += HAS_KNOB ? "<label>Charge icon</label><div class='row'>" : "<label>Select button hint</label><div class='row'>";
  h += "<input type='radio' name='logo' value='1' id='l1'" + String(cfg.evccLogo ? " checked" : "") + "><label for='l1'>evcc logo</label>";
  h += "<input type='radio' name='logo' value='0' id='l0'" + String(cfg.evccLogo ? "" : " checked") + "><label for='l0'>" + String(HAS_KNOB ? "charging station" : "text (below)") + "</label></div>";
  h += "<div class='row' style='margin-top:8px'><input type='checkbox' name='hbatt' value='1' id='hb'" + String(cfg.showHomeBatt ? " checked" : "") + "><label for='hb'>show home battery</label></div>";
  h += String("<label>Modes on the ") + (HAS_KNOB ? "knob" : "select button") + " and their texts</label><div class='row'>";
  for (int i = 0; i < MODE_COUNT; i++) {
    h += "<input type='checkbox' name='m" + String(i) + "e' value='1'" + String(cfg.modeEnabled[i] ? " checked" : "") + ">";
    h += "<input type='text' name='m" + String(i) + "l' value='" + esc(cfg.label[i]) + "' placeholder='label'>";
    h += "<input type='text' name='m" + String(i) + "a' value='" + esc(cfg.modeApi[i]) + "' placeholder='evcc name' style='grid-column:2'>";
  }
  h += "</div>";
  if (HAS_KNOB) {
    h += "<label>Confirm hint</label><input type='text' name='lconf' value='" + esc(cfg.labelConfirm) + "'>";
  } else {
    h += "<label>Text next to the select button</label><input type='text' name='lsel' value='" + esc(cfg.labelSelect) + "'>";
    h += "<label>Text next to the confirm button</label><input type='text' name='lconf' value='" + esc(cfg.labelConfirm) + "'>";
    h += "<div class='row' style='margin-top:8px'><input type='checkbox' name='swapbtn' value='1' id='sb'" + String(cfg.swapButtons ? " checked" : "") + "><label for='sb'>swap select and confirm button</label></div>";
  }
  h += "<small>Display texts: letters, digits and simple punctuation; ä ö ü ß become ae oe ue ss.</small></section>";

  h += "<section><h2>Security</h2>";
  h += "<label>Admin password for this page and OTA (user: admin)</label><input type='password' name='apass' placeholder='" + String(cfg.adminPass.isEmpty() ? "not set – page is open in your network" : "unchanged") + "'>";
  h += "<div class='row' style='margin-top:8px'><input type='checkbox' name='aclear' value='1' id='ac'><label for='ac'>remove admin password</label></div>";
  h += "</section><p><button type='submit'>Save</button></p></form>";

  // ---- firmware
  h += "<section><h2>Firmware update</h2><form method='post' action='/update' enctype='multipart/form-data'>";
  h += "<input type='file' name='fw' accept='.bin'><p><button type='submit'>Upload and install</button></p></form>";
  h += "<small>Use firmware.bin from the release or from .pio/build/wallbox-knob/.</small></section>";

  h += "<section><h2>Device</h2><form method='post' action='/reboot' style='display:inline'><button class='sec'>Restart</button></form> ";
  h += "<form method='post' action='/reset' style='display:inline' onsubmit=\"return confirm('Reset all settings?')\"><button class='sec'>Factory reset</button></form></section>";
  h += "</main></body></html>";
  server.send(200, "text/html; charset=utf-8", h);
}

static void message(const String &title, const String &text, int reloadS = 0) {
  String h = FPSTR(HEAD);
  if (reloadS > 0) h.replace("<title>", "<meta http-equiv='refresh' content='" + String(reloadS) + ";url=/'><title>");
  h += "<h1>" + title + "</h1><section><p>" + text + "</p><p><a href='/'>Back</a></p></section></main></body></html>";
  server.send(200, "text/html; charset=utf-8", h);
}

static void scheduleReboot() {
  rebootPending = true;
  rebootAt = millis() + 1500;
}

static void handleSave() {
  if (!authorized()) return;
  String oldSsid = cfg.wifiSsid, oldHost = cfg.hostname;
  bool wifiChanged = false;

  auto arg = [](const char *n) { String v = server.arg(n); v.trim(); return v; };
  if (server.hasArg("ssid")) cfg.wifiSsid = arg("ssid");
  if (!arg("wpass").isEmpty()) { cfg.wifiPass = server.arg("wpass"); wifiChanged = true; }
  if (!arg("host").isEmpty()) cfg.hostname = arg("host");
  if (server.hasArg("url")) cfg.evccUrl = arg("url");
  cfg.loadpoint = constrain(arg("lp").toInt(), 1, 10);
  if (!arg("epass").isEmpty()) cfg.evccPass = server.arg("epass");
  if (!arg("etoken").isEmpty()) cfg.evccToken = arg("etoken");
  cfg.pollMs = constrain(arg("poll").toInt(), 500, 60000);
  cfg.timeoutS = constrain(arg("tout").toInt(), 0, 3600);
  cfg.brightness = constrain(arg("bright").toInt(), 5, 100);
  if (server.hasArg("rot")) cfg.rotation = arg("rot").toInt() & (HAS_KNOB ? 3 : 2);
  float mk = arg("maxkw").toFloat();
  if (mk > 0) cfg.maxKw = mk;
  cfg.evccLogo = arg("logo") != "0";
  cfg.showHomeBatt = server.hasArg("hbatt");
  bool any = false;
  for (int i = 0; i < MODE_COUNT; i++) {
    String k = String("m") + i;
    cfg.modeEnabled[i] = server.hasArg(k + "e");
    any |= cfg.modeEnabled[i];
    if (server.hasArg(k + "l")) cfg.label[i] = arg((k + "l").c_str());
    if (server.hasArg(k + "a") && !arg((k + "a").c_str()).isEmpty()) cfg.modeApi[i] = arg((k + "a").c_str());
  }
  if (!any) for (bool &e : cfg.modeEnabled) e = true;
  if (server.hasArg("lconf")) cfg.labelConfirm = arg("lconf");
  if (server.hasArg("lsel")) cfg.labelSelect = arg("lsel");
  if (!HAS_KNOB) cfg.swapButtons = server.hasArg("swapbtn");
  if (server.hasArg("aclear")) cfg.adminPass = "";
  else if (!arg("apass").isEmpty()) cfg.adminPass = server.arg("apass");
  configSave();

  wifiChanged |= cfg.wifiSsid != oldSsid || cfg.hostname != oldHost;
  if (setup_ || wifiChanged) {
    message("Saved", "The device restarts and connects to <b>" + esc(cfg.wifiSsid) +
            "</b>. Then open http://" + esc(cfg.hostname) + ".local");
    scheduleReboot();
  } else {
    message("Saved", "Settings are active.", 2);
  }
}

static void handleUpdateDone() {
  if (!authorized()) return;
  if (Update.hasError()) {
    message("Update failed", esc(Update.errorString()));
  } else {
    message("Update installed", "The device restarts with the new firmware.", 15);
    scheduleReboot();
  }
}

static void handleUpdateUpload() {
  if (!setup_ && !cfg.adminPass.isEmpty() && !server.authenticate("admin", cfg.adminPass.c_str())) return;
  HTTPUpload &up = server.upload();
  if (up.status == UPLOAD_FILE_START) {
    Update.begin(UPDATE_SIZE_UNKNOWN);
  } else if (up.status == UPLOAD_FILE_WRITE) {
    Update.write(up.buf, up.currentSize);
  } else if (up.status == UPLOAD_FILE_END) {
    Update.end(true);
  } else if (up.status == UPLOAD_FILE_ABORTED) {
    Update.abort();
  }
}

void webBegin(bool setupMode) {
  setup_ = setupMode;
  if (setup_) {
    dns.start(53, "*", WiFi.softAPIP());   // captive portal: every name points to us
  } else {
    MDNS.begin(cfg.hostname.c_str());
    MDNS.addService("http", "tcp", 80);
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/save", HTTP_POST, handleSave);
  server.on("/update", HTTP_POST, handleUpdateDone, handleUpdateUpload);
  server.on("/reboot", HTTP_POST, [] {
    if (!authorized()) return;
    message("Restarting", "The device restarts.", 10);
    scheduleReboot();
  });
  server.on("/reset", HTTP_POST, [] {
    if (!authorized()) return;
    configReset();
    message("Reset", "All settings deleted. The device restarts in setup mode.");
    scheduleReboot();
  });
  server.onNotFound([] {
    // captive portal checks (Apple hotspot-detect, Android generate_204, ...) land here;
    // an absolute URL makes the phone open the settings page instead of retrying
    if (setup_) Serial.printf("[%lu] HTTP %s%s -> portal\n", millis(), server.hostHeader().c_str(), server.uri().c_str());
    String ip = setup_ ? WiFi.softAPIP().toString() : WiFi.localIP().toString();
    server.sendHeader("Location", "http://" + ip + "/", true);
    server.sendHeader("Cache-Control", "no-cache");
    server.send(302, "text/plain", "");
  });
  server.begin();

  if (!setup_) {
    ArduinoOTA.setHostname(cfg.hostname.c_str());
    if (!cfg.adminPass.isEmpty()) ArduinoOTA.setPassword(cfg.adminPass.c_str());
    ArduinoOTA.begin();
  }
}

void webLoop() {
  if (setup_) dns.processNextRequest();
  else ArduinoOTA.handle();
  server.handleClient();
  if (rebootPending && (int32_t) (millis() - rebootAt) > 0) ESP.restart();
}
