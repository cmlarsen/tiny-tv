# micro-hud

Firmwares for the NM-TV-154: an ESP32-D0WD-V3 with a 1.54" 240×240 ST7789 screen and one
capacitive touch pad. Each firmware is its own PlatformIO project under `packages/`; the board
runs one at a time.

| Package | What it is |
|---|---|
| `packages/piggy-weather` | Weather pig for Zed (Open-Meteo forecast, outfits per weather) |
| `packages/machine-stats` | CPU / memory HUD for the LAN dev machines (`stats-daemon`) |
| `packages/nmtv-common` | Shared: board pins, display, touch, WiFi setup portal, band canvas |

## Commands

```bash
./hud list                 # firmwares
./hud new my-thing         # scaffold packages/my-thing from templates/firmware
./hud test [name]          # native tests (all packages when no name)
./hud build <name>
./hud flash <name>         # build and upload over USB
./hud monitor              # serial at 230400
./hud shot "home=" "detail=view detail 0"   # framebuffer PNGs into shots/
```

Needs PlatformIO (`uv tool install platformio`) and `uv` for the screenshot tool.
`HUD_PORT` overrides the serial port (default `/dev/cu.usbserial-210`).

## Adding a firmware

`./hud new <name>` copies `templates/firmware`: display and touch set up, a WiFi setup portal
with one settings field, `shot`/`perf` serial commands, and a native test for `lib/applogic`.
The full walkthrough (settings, drawing, touch, networking, tests, screenshots, checklist) is in
[docs/new-firmware.md](docs/new-firmware.md).

Shared versions (platform, LovyanGFX, WiFiManager, port, baud) live in `platformio/nmtv.ini`.
A package adds its own libraries by extending `${nmtv.lib_deps}`.
