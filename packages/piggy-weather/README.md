# Piggy Weather

A weather pig for Zed, running on an NM-TV-154 (ESP32 + 1.54" 240×240 ST7789 + one touch pad).
Forecasts come from [Open-Meteo](https://open-meteo.com) (no API key).

## First-time setup

1. Plug it in. The pig shows a QR code.
2. Scan it with a phone (or join the open WiFi network `Piggy-Weather`), then open http://192.168.4.1 if a setup page doesn't open by itself.
3. Pick your WiFi, enter the password, the kid's name, a city or ZIP code (blank = guess from IP), and `F` or `C`.

Settings can be changed later at `http://piggy.local` (or the IP on the info screen).

## Using it

| Touch | Does |
|---|---|
| Tap | Show tomorrow (tap again or wait 15 s to go back) |
| Hold 3 s | Info screen (place, IP, last update) |
| Hold 3 s, let go, hold 5 s | Forget WiFi and settings, back to setup (the second hold must start within 30 s) |

From 8 pm to 7 am the pig sleeps and the screen dims. Tap to peek at tomorrow.

## Building

```bash
pio test -e native          # weather logic tests (runs on the Mac)
pio run -e nmtv -t upload   # build and flash over USB (CH340, /dev/cu.usbserial-210)
pio device monitor          # serial log at 230400
```

Serial debug commands: `mood <sunny|cloudy|hot|chilly|cold|rain|storm|snow|windy|fog>`,
`mood off`, `view <today|tomorrow|night|info|setup|loading|offline>`, `view off`, `fetch`,
and `shot` (dumps the frame as raw big-endian RGB565 between `SHOT` and `ENDSHOT` markers).
