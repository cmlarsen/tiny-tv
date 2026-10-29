# Machine Stats HUD

Live CPU and memory for the dev machines on the LAN, on an NM-TV-154.
Each machine runs `stats-daemon` on port 9110 (`GET /` returns JSON); the HUD polls every 3 s.

## First-time setup

1. Plug it in. The screen shows a QR code.
2. Scan it (or join the open WiFi network `Stats-HUD`), then open http://192.168.4.1.
3. Pick your WiFi. The machine list defaults to deltron, caleb-work2 and shop16 by IP.

Change the machine list later at `http://stats.local` (IP or IP:port, comma separated, up to 6).
Use IPs: the Tailscale hostnames don't resolve from the ESP32.

## Using it

| Touch | Does |
|---|---|
| Tap | Step through each machine's detail screen, then back to the list |
| Hold 3 s | Info screen (IP, machine count, free heap) |
| Hold 10 s | Forget WiFi and settings, back to setup |

Bars and gauges are green below 60%, amber below 85%, red above. A machine shows as offline
after two failed polls in a row (1.5 s timeout each).

## Building

```bash
pio test -e native          # stats logic tests
pio run -e nmtv -t upload   # build and flash
```

Serial (230400) logs touch events and screen changes. Debug commands: `shot`, `perf`,
`view <overview|detail N|info|setup|connecting>`, `view off`, and `machines <list>` (until reboot).
Opening the serial port resets the board.
