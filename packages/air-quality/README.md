# Air Quality

Outdoor AQI from the nearest EPA AirNow monitor, plus an N-hour average with an hourly chart.

## Data source

AirNow's public hourly observation files (`files.airnowtech.org/airnow/YYYY/YYYYMMDD/HourlyAQObs_YYYYMMDDHH.dat`):
official regulatory monitors run by state and local agencies, no API key. Each file covers every
US (plus some Canadian and Mexican) site for one UTC hour and is about 1 MB. A new hour posts
roughly 40–100 minutes after it ends, so the newest reading is usually 1–2 hours old.

The firmware streams each file line by line and keeps the nearest active site within 80 km for
PM2.5 and for ozone separately.

- **Now**: the AQI AirNow reports for the newest hour (NowCast for PM2.5), the higher of PM2.5
  and ozone.
- **Average**: the mean PM2.5 and ozone concentrations over the last N hours, each converted to
  AQI (PM2.5 with the 2024 breakpoints, ozone with the 8-hour table), the higher one shown. Bars
  are the same calculation per hour. With N = 24 the PM2.5 number is the EPA's 24-hour AQI.

On boot it downloads the newest hour, then backfills the remaining N−1 hours (~5 s each, about
2 minutes for 24). After that it checks every 10 minutes; an unchanged newest file costs a
304 thanks to `If-Modified-Since`.

## First-time setup

1. Plug it in. The screen shows a QR code.
2. Scan it (or join the open WiFi network `Air-Quality`), then open http://192.168.4.1.
3. Pick your WiFi. Settings live at `http://air-quality.local` afterwards:
   - **ZIP code or city**: blank guesses from your IP address.
   - **Average over N hours**: 1–24, default 24.

Tap the pad to switch between Now and the average. Hold 3 s for the info screen (IP, monitor,
distance). To forget WiFi and settings: hold 3 s, let go, then hold again for 5 s.

## Building

```bash
../../hud test air-quality
../../hud flash air-quality
SHOT_WAIT=20 ../../hud shot now= "avg=view avg" "info=view info" "orange=demo 120;view now"
```

Serial commands: `view now|avg|info|setup|connecting|status`, `demo <aqi>` (fake data for
checking colors, `demo -1` to stop), `refresh`, `perf`, `shot`.
