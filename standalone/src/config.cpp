// Settings stored in NVS flash (Preferences).
#include <Preferences.h>
#include "app.h"

Config cfg;

static Preferences prefs;

void configLoad() {
  Config d;  // defaults
  prefs.begin("knob", true);
  cfg.wifiSsid = prefs.getString("ssid", d.wifiSsid);
  cfg.wifiPass = prefs.getString("wpass", d.wifiPass);
  cfg.hostname = prefs.getString("host", d.hostname);
  cfg.adminPass = prefs.getString("apass", d.adminPass);
  cfg.evccUrl = prefs.getString("url", d.evccUrl);
  cfg.loadpoint = prefs.getUChar("lp", d.loadpoint);
  cfg.evccPass = prefs.getString("epass", d.evccPass);
  cfg.evccToken = prefs.getString("etoken", d.evccToken);
  cfg.pollMs = prefs.getUShort("poll", d.pollMs);
  cfg.timeoutS = prefs.getUShort("tout", d.timeoutS);
  cfg.maxKw = prefs.getFloat("maxkw", d.maxKw);
  for (int i = 0; i < MODE_COUNT; i++) {
    String k = String("k") + i;   // "k": 3-mode layout (0.1.0 builds used "m" with 4 modes)
    cfg.modeEnabled[i] = prefs.getBool((k + "e").c_str(), d.modeEnabled[i]);
    cfg.label[i] = prefs.getString((k + "l").c_str(), d.label[i]);
    cfg.modeApi[i] = prefs.getString((k + "a").c_str(), d.modeApi[i]);
    if (cfg.modeApi[i].isEmpty()) cfg.modeApi[i] = d.modeApi[i];
  }
  cfg.labelConfirm = prefs.getString("lconf", d.labelConfirm);
  cfg.evccLogo = prefs.getBool("logo", d.evccLogo);
  cfg.brightness = prefs.getUChar("bright", d.brightness);
  prefs.end();

  if (cfg.hostname.isEmpty()) cfg.hostname = d.hostname;
  if (cfg.loadpoint < 1) cfg.loadpoint = 1;
  if (cfg.pollMs < 500) cfg.pollMs = 500;
  if (cfg.maxKw <= 0) cfg.maxKw = d.maxKw;
  if (cfg.brightness < 5 || cfg.brightness > 100) cfg.brightness = 100;
  bool any = false;
  for (bool e : cfg.modeEnabled) any |= e;
  if (!any) for (bool &e : cfg.modeEnabled) e = true;
}

void configSave() {
  prefs.begin("knob", false);
  prefs.putString("ssid", cfg.wifiSsid);
  prefs.putString("wpass", cfg.wifiPass);
  prefs.putString("host", cfg.hostname);
  prefs.putString("apass", cfg.adminPass);
  prefs.putString("url", cfg.evccUrl);
  prefs.putUChar("lp", cfg.loadpoint);
  prefs.putString("epass", cfg.evccPass);
  prefs.putString("etoken", cfg.evccToken);
  prefs.putUShort("poll", cfg.pollMs);
  prefs.putUShort("tout", cfg.timeoutS);
  prefs.putFloat("maxkw", cfg.maxKw);
  for (int i = 0; i < MODE_COUNT; i++) {
    String k = String("k") + i;
    prefs.putBool((k + "e").c_str(), cfg.modeEnabled[i]);
    prefs.putString((k + "l").c_str(), cfg.label[i]);
    prefs.putString((k + "a").c_str(), cfg.modeApi[i]);
  }
  prefs.putString("lconf", cfg.labelConfirm);
  prefs.putBool("logo", cfg.evccLogo);
  prefs.putUChar("bright", cfg.brightness);
  prefs.end();
}

void configReset() {
  prefs.begin("knob", false);
  prefs.clear();
  prefs.end();
  cfg = Config();
}
