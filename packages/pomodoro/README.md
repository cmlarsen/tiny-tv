# Pomodoro

A Pomodoro timer for the NM-TV-154. No WiFi needed; settings are stored on the device.

The screen fills from the bottom while a pomodoro runs, flashes when it's complete, then drains
from the top through the break. When the break is over it flashes again and the next pomodoro
starts.

## Gestures

| Gesture | Timer | Settings |
|---|---|---|
| Tap | start; pause/resume while running; skip the flash | +5 min, or next theme |
| Double tap | stop and reset | −5 min, or previous theme |
| Long press (0.7 s) | open settings | next page: Focus length → Break length → Color theme → done |

Lengths run 5–120 min in 5 min steps (defaults 25 and 5). Themes: Tomato, Ocean, Forest, Sunset,
Mono, Light. A single tap registers 350 ms after release, once it can't be the start of a double tap.

To restore defaults: hold 3 s, let go, then hold again for 5 s.

## Building

```bash
../../hud test pomodoro
../../hud flash pomodoro
../../hud shot ready= "focus=tap;view focus 0.4" "settings=long"
```

Serial debug commands: `tap`, `double`, `long`, `view <ready|focus|focusdone|break|breakdone> <0-1>`,
`theme <0-5>`, `speed <N>` (run the clock N× faster), `state`, `shot`, `perf`.
