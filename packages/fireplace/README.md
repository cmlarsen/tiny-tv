# Fireplace

A fire for the NM-TV-154. No WiFi or setup: plug it in and it burns.

It plays a looping video clip of a real fire from flash. Tap the pad to switch to a simulated fire
and through its sizes: video → low → steady → roaring → video. With no clip flashed it starts on
the simulated fire.

## The video clip

`tools/clip.py` crops a square from a video, scales it to 240×240, encodes each frame as a JPEG,
crossfades the last 15 frames into the first so the loop has no seam, and packs as many frames as
fit the 2.9 MB `clip` partition into `clip.bin` (gitignored). At quality 75 a 30 fps video gives
about 8 s. The firmware maps the partition, checks the CRC at boot (about 0.6 s), and decodes
frames with JPEGDEC straight into DMA strips. A frame takes about 35 ms, so the clip plays every
frame at about 29 fps.

```bash
uv run --with av --with pillow tools/clip.py make ~/Downloads/fire.mp4 --crop 850,403,1700
uv run --with esptool tools/clip.py flash      # about 2.5 minutes at 230400 baud
```

`--crop X,Y,SIZE` is the square to take, in source pixels; the default suits the 4K clip this was
made with. `--start SEC` and `--quality Q` pick the segment and the size/quality trade-off.

The clip lives in its own partition, so `../../hud flash fireplace` leaves it alone. Another
firmware (with its 3 MB app partition) can overwrite it; the CRC check catches that and the
firmware falls back to the simulated fire until you run `clip.py flash` again.

## The simulated fire

A heat simulation on a 120×120 grid (`lib/fire`): fuel along the bottom, heat rising and
averaging upward, and a scrolling noise map cooling it into tongues. Each frame is upscaled 2×
with interpolation, mapped through a black-body palette, and streamed to the panel in 40-row DMA
bands. It runs at about 35 fps, limited by SPI at 40 MHz.

## Building

```bash
../../hud test fireplace
../../hud flash fireplace
../../hud shot "video=frame 120" "fire=level 1;steps 300"
```

Serial debug commands: `video` (play the clip), `frame N` (pause the clip on frame N for 30 s, or until a tap),
`level <0|1|2>`, `steps N` (advance the simulation), `perf` (step and frame times), and `shot`.
