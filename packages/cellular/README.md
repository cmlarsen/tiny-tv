# Cellular

A zoo of cellular automata and emergent simulations for the NM-TV-154. No WiFi or setup: plug it in
and it runs.

| Mode | Variants | What it does |
|---|---|---|
| Life | Conway, HighLife, Day & Night | 80×80 Life with four competing species: a newborn takes its parents' majority color (QuadLife), new cells flash white, dead cells leave a fading trail. Fresh soup is splashed in when the board goes quiet. |
| Cyclic | Demons, Perfect Spirals, 313, Lava Lamp | 120×120 cyclic cellular automaton: a cell advances to the next color when enough neighbors already have it. Noise organizes into rainbow spirals. |
| Slime Mold | Veins, Web, Bloom | 6000 Physarum agents on a 240×240 trail map. Each agent senses the trail ahead, steers toward it and deposits more; the map blurs and decays. Grows vein networks. |
| Brian's Brain | Soup, Mandala | 120×120 three-state automaton (on → dying → off, born with exactly two on neighbors) that never settles. Mandala starts from a 4-fold symmetric seed. |
| Reaction-Diffusion | Coral, Mitosis, Maze, Worms | Gray–Scott on a 120×120 fixed-point (Q14) grid, upscaled with interpolation. |

Tap the pad for the next mode; it starts a random variant. Nothing changes on its own: a mode runs
until the next tap. The mode name shows for 2.5 s after each change.

## Performance

Drawing a frame is SPI-bound at about 23.5 ms (40 MHz), so the frame budget is 30 fps. The heavy
row loops (cyclic, Gray–Scott, slime diffusion) are split across both cores: the library calls
`ca::forRows`, which runs serially by default, and `main.cpp` installs a runner that hands half the
rows to a worker task pinned to core 0. Measured simulation time per frame:

| Mode | Tick |
|---|---|
| Life, Brian's Brain | ≈9 ms every other frame |
| Cyclic | 5–8 ms (Lava Lamp 21 ms) |
| Slime Mold | 12 ms |
| Reaction-Diffusion | 43 ms for 8 steps (≈15 fps) |

## Building

```bash
../../hud test cellular
../../hud flash cellular
SHOT_WAIT=4 ../../hud shot "spirals=mode 1 1;ticks 400;sleep 3"
```

Serial debug commands: `mode N [V]` (0 Life, 1 Cyclic, 2 Slime Mold, 3 Brian's Brain,
4 Reaction-Diffusion; V picks the variant, random if omitted), `next`, `variant`, `ticks N`
(advance without drawing), `perf` (tick and draw times, free heap), and `shot`. `ticks` on
Reaction-Diffusion takes about 45 ms per tick, so add a `sleep` after it in `hud shot` jobs or the
`shot` request times out.
