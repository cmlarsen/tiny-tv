# micro-hud

Firmware for the NM-TV-154 (ESP32-D0WD-V3, 1.54" 240×240 ST7789, one capacitive touch pad),
built with PlatformIO. Each package is its own PlatformIO project; flash one at a time.

| Package | What it is |
|---|---|
| `packages/piggy-weather` | Weather pig for Zed (Open-Meteo forecast, outfits per weather) |
| `packages/machine-stats` | CPU / memory HUD for the LAN dev machines (`stats-daemon`) |
| `packages/nmtv-common` | Shared board pins, display driver, touch pad and drawing helpers |

```bash
cd packages/<name>
pio test -e native          # logic tests (nmtv-common has touch tests)
pio run -e nmtv -t upload   # build and flash over USB (CH340, /dev/cu.usbserial-210)
```
