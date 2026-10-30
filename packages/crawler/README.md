# Crawler

A first-person dungeon crawler for the NM-TV-154 that plays itself. A lone hero explores each
maze, fights what it meets, drinks potions when low and takes the stairs down. Every floor
changes theme (12 of them); after floor 12 they repeat with more blood on the walls each pass.
Hits and deaths spray blood. When the hero dies, a tombstone shows the killer and the best floor
reached, then a new run starts.

Tap the pad to cycle speed (1x, 2x, 4x). Hold it for 3 s to start a new run. No WiFi needed.

## Art (not in git)

The views use the Crawler View Kit and its Deep Places expansion, the monsters use Dungeon
Monsters Remastered 32x32 (all by leckie1989), and the blood comes from a splatter pack. The
licences forbid redistributing the files, and this repo is public, so the art and everything
baked from it are gitignored. Before building, unzip the packs into `assets/`:

```
assets/Crawler-View-Kit/
assets/Crawler-View-Kit-Deep-Places/
assets/Dungeon-Monsters-Remastered-32x32/
assets/Blood-Splatter/            folders 1-5 of the splatter pack
```

The build runs `tools/bake.py` when `src/gen/art.bin` is missing or older than the script. It
crops the kit's 320x200 view to the centre 240 columns, run-length encodes every piece with a
per-theme palette, warps blood stains onto every wall position and writes `src/gen/art.bin`
(embedded in flash, about 2.2 MB of the 3 MB app partition) plus
`src/gen/art.h` (the index).

## Building

```bash
../../hud test crawler
../../hud flash crawler
../../hud shot "fight=pause;spawn 3;step 1"
```

Serial commands: `pause`, `resume`, `seed N`, `floor N`, `spawn KIND`, `step N`, `speed N`,
`card`, `tomb`, `anim KIND*1000+PERMILLE`, `splat KIND`, `best N` (sets the saved deepest floor), `shot`, `perf`.
