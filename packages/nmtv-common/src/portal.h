#pragma once

#include <Arduino.h>

namespace nmtv {

struct PortalField {
  const char* id;
  const char* label;
  const char* defaultValue;
  char* value;
  size_t capacity;
};

struct PortalConfig {
  const char* title;
  const char* apName;
  const char* hostname;
  PortalField* fields;
  int fieldCount;
};

namespace portal {

void begin(const PortalConfig& config);
void loop();
bool online();
bool takeSettingsChanged();

const char* apName();
const char* apQr();
String ip();

void resetAll();

}  // namespace portal
}  // namespace nmtv
