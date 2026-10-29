#include "applogic.h"

#include <stdio.h>

namespace applogic {

void tapLabel(int taps, char* out, size_t len) {
  if (taps == 0) snprintf(out, len, "Tap the pad");
  else snprintf(out, len, "%d tap%s", taps, taps == 1 ? "" : "s");
}

}  // namespace applogic
