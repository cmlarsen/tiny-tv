#include "net.h"

#include "portal.h"

namespace net {
namespace {

Settings current{};

nmtv::PortalField fields[] = {
    {"machines", "Machines (IP or IP:port, comma separated)", "192.168.68.77,192.168.68.59,192.168.68.68",
     current.machines, sizeof current.machines},
    {"tz", "Time zone (POSIX TZ)", "EST5EDT,M3.2.0,M11.1.0", current.tz, sizeof current.tz},
};

}  // namespace

void begin() { nmtv::portal::begin({"Stats HUD", "Stats-HUD", "stats", fields, sizeof fields / sizeof fields[0]}); }

const Settings& settings() { return current; }

}  // namespace net
