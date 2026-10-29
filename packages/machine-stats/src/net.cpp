#include "net.h"

#include <ESPmDNS.h>
#include <Preferences.h>
#include <WiFi.h>
#include <WiFiManager.h>

namespace net {
namespace {

constexpr const char* kDefaultMachines = "192.168.68.77,192.168.68.59,192.168.68.68";
constexpr const char* kDefaultTz = "EST5EDT,M3.2.0,M11.1.0";

WiFiManager wm;
Preferences prefs;
Settings current{};
bool changed = false;
bool isOnline = false;

WiFiManagerParameter* machinesParam = nullptr;
WiFiManagerParameter* tzParam = nullptr;

void copyField(char* dst, size_t cap, const char* src) {
  strncpy(dst, src ? src : "", cap - 1);
  dst[cap - 1] = '\0';
}

void loadSettings() {
  prefs.begin("stats", true);
  copyField(current.machines, sizeof current.machines, prefs.getString("machines", kDefaultMachines).c_str());
  copyField(current.tz, sizeof current.tz, prefs.getString("tz", kDefaultTz).c_str());
  prefs.end();
}

void saveParams() {
  copyField(current.machines, sizeof current.machines, machinesParam->getValue());
  copyField(current.tz, sizeof current.tz, tzParam->getValue());
  prefs.begin("stats", false);
  prefs.putString("machines", current.machines);
  prefs.putString("tz", current.tz);
  prefs.end();
  changed = true;
}

void onConnected() {
  isOnline = true;
  changed = false;
  MDNS.begin("stats");
  std::vector<const char*> menu = {"param", "wifi", "info", "restart"};
  wm.setMenu(menu);
  wm.startWebPortal();
  Serial.printf("[net] connected, IP %s\n", WiFi.localIP().toString().c_str());
}

}  // namespace

void begin() {
  loadSettings();
  wm.setDebugOutput(false);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname("stats-hud");

  machinesParam = new WiFiManagerParameter("machines", "Machines (IP or IP:port, comma separated)", current.machines,
                                           sizeof current.machines - 1);
  tzParam = new WiFiManagerParameter("tz", "Time zone (POSIX TZ)", current.tz, sizeof current.tz - 1);
  wm.addParameter(machinesParam);
  wm.addParameter(tzParam);
  wm.setSaveParamsCallback(saveParams);
  wm.setTitle("Stats HUD");
  wm.setConnectTimeout(20);
  wm.setConfigPortalBlocking(false);
  const bool saved = wm.getWiFiIsSaved();
  wm.setConfigPortalTimeout(saved ? 300 : 0);
  Serial.printf("[net] saved WiFi: %s\n", saved ? wm.getWiFiSSID().c_str() : "none");

  if (wm.autoConnect(kApName)) onConnected();
  else Serial.printf("[net] not connected (status %d), setup portal open as %s\n", WiFi.status(), kApName);
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

const Settings& settings() { return current; }

bool takeSettingsChanged() {
  const bool was = changed;
  changed = false;
  return was;
}

String ip() { return WiFi.localIP().toString(); }

void resetAll() {
  wm.resetSettings();
  prefs.begin("stats", false);
  prefs.clear();
  prefs.end();
  delay(200);
  ESP.restart();
}

}  // namespace net
