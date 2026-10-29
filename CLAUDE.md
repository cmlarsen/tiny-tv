# micro-hud

Multiple firmwares for one device (NM-TV-154). See README.md for layout and `./hud` commands,
and docs/new-firmware.md before adding a firmware.

## Device facts

- ESP32-D0WD-V3, 4 MB flash, no PSRAM. CH340 USB serial on `/dev/cu.usbserial-210`.
- Display ST7789 240×240 on SPI (MOSI 13, SCLK 14, CS 15, DC 2), reset tied to EN. GPIO21 powers
  the panel and GPIO19 is the backlight, both active LOW. `nmtv::beginDisplay()` handles it.
- One capacitive touch pad on GPIO32. Always go through `TouchPad` (backed by `TouchLogic`,
  natively tested). On this core a touch only drops the reading ~15% (≈101 → 84–89), so naive
  thresholds or integer EMA baselines drift and stop registering taps.

## Gotchas

- Platform is pinned to `espressif32@7.1.3`, which ships Arduino-ESP32 **2.0.17**, not 3.x.
- Opening the serial port resets the board. To catch a bug as it happens, keep one long-running
  reader open, then reproduce.
- CH340 drops bytes at 460800+. Uploads and app serial use 230400; `tools/shot.py` retries.
- ~160 KB heap free with WiFi up: a full 240×240×16-bit sprite does not fit reliably. Render in
  two 240×120 bands (`nmtv::renderBands` / `nmtv::Canvas`, which offsets y per band).
- The WiFiManager captive-portal popup doesn't always open on phones. Setup screens must show
  `192.168.4.1`. The portal runs non-blocking; call `nmtv::portal::loop()` every loop.
- Each firmware's portal `hostname` doubles as its Preferences namespace (≤15 chars) and its
  `<hostname>.local` settings page.

## Verifying UI changes

Every firmware implements the `shot` serial command (raw big-endian RGB565 between `SHOT` and
`ENDSHOT`). `./hud shot NAME=CMD;CMD` sends debug commands then saves `shots/NAME.png`;
look at the PNGs instead of guessing. Use `SHOT_WAIT=20` when the screen needs WiFi data first.
