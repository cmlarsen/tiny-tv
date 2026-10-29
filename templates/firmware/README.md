# __TITLE__

Firmware for the NM-TV-154. Created from `templates/firmware`.

## First-time setup

1. Plug it in. The screen shows a QR code.
2. Scan it (or join the open WiFi network `__AP__`), then open http://192.168.4.1.
3. Pick your WiFi. Settings live at `http://__HOST__.local` afterwards.

Tap the pad to count taps. Hold 10 s to forget WiFi and settings.

## Building

```bash
../../hud test __NAME__
../../hud flash __NAME__
../../hud shot
```
