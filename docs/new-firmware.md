# Creating a new firmware

This walks through adding a firmware to `packages/`, from scaffold to running on the device.
`packages/machine-stats` is the fuller worked example; `packages/piggy-weather` shows animation.

## 1. Prerequisites

- PlatformIO: `uv tool install platformio`
- `uv` (the screenshot tool runs through it)
- The board on USB as `/dev/cu.usbserial-210` (set `HUD_PORT` if yours differs)

## 2. Scaffold

```bash
./hud new desk-clock
```

This copies `templates/firmware` to `packages/desk-clock` and fills in the names:

| Name | Value for `desk-clock` | Used for |
|---|---|---|
| Title | `Desk Clock` | Settings page title |
| Hotspot | `Desk-Clock` | WiFi network shown during setup |
| Hostname | `desk-clock` | `http://desk-clock.local` settings page and the saved-settings namespace (15 chars max) |

You get:

```
packages/desk-clock/
  platformio.ini            extends platformio/nmtv.ini
  src/main.cpp              display, touch, WiFi portal, render loop, serial debug commands
  lib/applogic/src/         pure logic, no Arduino headers
  test/test_applogic/       native Unity tests for lib/applogic
  README.md
```

## 3. Run it before changing anything

```bash
./hud test desk-clock
./hud flash desk-clock
SHOT_WAIT=12 ./hud shot home=
```

`shots/home.png` should show "Hello!" and "Tap the pad". The WiFi password is stored by the
ESP32 itself, not per firmware, so if the board was on WiFi under another firmware it reconnects
on its own. Otherwise the screen shows a QR code: join the hotspot and open `http://192.168.4.1`.

## 4. Make it yours

### Settings

Settings are fields on the WiFi setup page and at `http://<hostname>.local` later. Declare them in
`main.cpp`; each gets a `char` buffer that is loaded at boot and saved when edited:

```cpp
char city[41];
char units[2];
nmtv::PortalField fields[] = {
    {"city", "City or ZIP code", "", city, sizeof city},
    {"units", "Units: F or C", "F", units, sizeof units},
};
```

Call `nmtv::portal::takeSettingsChanged()` in `loop()` to react when someone saves new values.

### Drawing

The screen is drawn in two 240×120 bands because a full-screen buffer doesn't fit in memory with
WiFi running. `nmtv::renderBands` calls your draw function once per band; `nmtv::Canvas` takes
full-screen coordinates and offsets them for you, so draw as if the screen were one piece:

```cpp
nmtv::renderBands(band, [](nmtv::Canvas& c) {
  c.s.fillScreen(kBg);
  c.font(&fonts::FreeSansBold12pt7b);
  c.text("12:34", 120, 120, kText, middle_center);
  c.roundRect(20, 180, 200, 12, 6, kAccent);
}, dump);
```

Colors are RGB565 via `nmtv::rgb(r, g, b)`. Fonts are LovyanGFX's (`fonts::FreeSansBold9pt7b`,
`12pt`, `18pt`, `24pt`). A full redraw takes 30–55 ms, so 10–20 fps is the practical ceiling;
redraw less often on static screens (the template drops to 1 fps on the setup screen).

### Touch

`touch.poll(now)` returns one event per call:

| Event | Fires | Convention |
|---|---|---|
| `Tap` | on release, if held under 3 s | the main action (next screen, toggle) |
| `Hold` | once at 3 s | info screen (IP, settings URL) |
| `Reset` | 5 s into a second hold that starts within 30 s of releasing a `Hold` | `nmtv::portal::resetAll()` (forget WiFi and settings) |

Keep `Reset` as the reset so every firmware can be recovered the same way. It needs a release
between the two holds so a pad that reads as pressed forever (drift, a moved cable) can't wipe
settings; a press lasting over 60 s is treated as drift and re-baselined. Show
`touch.resetArmed(now)` and `touch.resetProgress(now)` on your info screen.

### Networking

Plain `HTTPClient` works for occasional requests from `loop()`. If a request can hang (a machine
that's asleep, a slow API), run it in a FreeRTOS task so the screen keeps animating. See
`packages/machine-stats/src/fleet.cpp` for a polling task with a mutex-protected snapshot.

### Logic and tests

Put anything that doesn't touch hardware in `lib/` (rename `applogic` to fit): parsing, formatting,
thresholds, state machines. Test it natively with `./hud test desk-clock`; tests run on the Mac in
about a second. Write the test first, then the code.

## 5. Check the screen

Every firmware answers serial debug commands; `shot` dumps the framebuffer and `./hud shot` turns
it into a PNG. Add commands that force the states you want to inspect (see `view` and `mood` in
piggy-weather, or `view detail N` and `machines` in machine-stats):

```bash
./hud shot home= "detail=view detail 0" "offline=machines 10.0.0.250;sleep 8"
```

Opening the serial port resets the board, so set `SHOT_WAIT` long enough for WiFi and the first
data fetch (about 20 s). To capture a bug as it happens, keep one serial reader open
(`./hud monitor`) and reproduce it there.

## 6. Switching firmwares on the device

The board runs one firmware at a time. `./hud flash <name>` replaces whatever is on it. WiFi
credentials survive; each firmware's own settings are kept in its own namespace, so switching back
restores them.

## 7. Checklist

- [ ] `./hud test` passes for every package
- [ ] `./hud build <name>` succeeds
- [ ] Setup screen shows the QR code, hotspot name and `192.168.4.1`
- [ ] The hold, release, hold `Reset` gesture resets WiFi and settings
- [ ] Screenshots of each screen checked with `./hud shot`
- [ ] Row added to the package table in the root README
- [ ] New shared versions go in `platformio/nmtv.ini`, not the package
