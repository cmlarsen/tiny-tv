#include "portal.h"

#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>

#include <vector>

namespace nmtv {
namespace portal {
namespace {

WiFiManager wm;
Preferences prefs;
PortalConfig cfg{};
std::vector<WiFiManagerParameter*> params;
char qr[64];
char prefsNamespace[16];
bool changed = false;
bool isOnline = false;

void copyField(char* dst, size_t cap, const char* src) {
  strncpy(dst, src ? src : "", cap - 1);
  dst[cap - 1] = '\0';
}

void loadFields() {
  prefs.begin(prefsNamespace, true);
  for (int i = 0; i < cfg.fieldCount; i++) {
    const PortalField& f = cfg.fields[i];
    copyField(f.value, f.capacity, prefs.getString(f.id, f.defaultValue).c_str());
  }
  prefs.end();
}

void saveFields() {
  prefs.begin(prefsNamespace, false);
  for (int i = 0; i < cfg.fieldCount; i++) {
    const PortalField& f = cfg.fields[i];
    copyField(f.value, f.capacity, params[i]->getValue());
    prefs.putString(f.id, f.value);
  }
  prefs.end();
  changed = true;
}

void onConnected() {
  isOnline = true;
  changed = false;
  MDNS.begin(cfg.hostname);
  std::vector<const char*> menu = {"param", "wifi", "info", "restart"};
  wm.setMenu(menu);
  wm.startWebPortal();
  Serial.printf("[portal] connected, IP %s, settings at http://%s.local\n", WiFi.localIP().toString().c_str(),
                cfg.hostname);
}

}  // namespace

void begin(const PortalConfig& config) {
  cfg = config;
  copyField(prefsNamespace, sizeof prefsNamespace, cfg.hostname);
  snprintf(qr, sizeof qr, "WIFI:T:nopass;S:%s;;", cfg.apName);
  loadFields();

  wm.setDebugOutput(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(cfg.hostname);

  for (int i = 0; i < cfg.fieldCount; i++) {
    const PortalField& f = cfg.fields[i];
    params.push_back(new WiFiManagerParameter(f.id, f.label, f.value, f.capacity - 1));
    wm.addParameter(params.back());
  }
  wm.setSaveParamsCallback(saveFields);
  wm.setTitle(cfg.title);
  wm.setConnectTimeout(20);
  wm.setConfigPortalBlocking(false);
  const bool saved = wm.getWiFiIsSaved();
  wm.setConfigPortalTimeout(saved ? 300 : 0);
  Serial.printf("[portal] saved WiFi: %s\n", saved ? wm.getWiFiSSID().c_str() : "none");

  if (wm.autoConnect(cfg.apName)) onConnected();
  else Serial.printf("[portal] not connected (status %d), setup hotspot %s\n", WiFi.status(), cfg.apName);
}

void loop() {
  const bool connectedNow = wm.process();
  if (isOnline) return;
  if (connectedNow || WiFi.status() == WL_CONNECTED) {
    onConnected();
  } else if (!wm.getConfigPortalActive()) {
    ESP.restart();
  }
}

bool online() { return isOnline; }

bool takeSettingsChanged() {
  const bool was = changed;
  changed = false;
  return was;
}

const char* apName() { return cfg.apName; }
const char* apQr() { return qr; }
String ip() { return WiFi.localIP().toString(); }

void resetAll() {
  wm.resetSettings();
  prefs.begin(prefsNamespace, false);
  prefs.clear();
  prefs.end();
  delay(200);
  ESP.restart();
}

}  // namespace portal
}  // namespace nmtv
