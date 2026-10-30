"""Bake the Crawler View Kit and Dungeon Monsters art into src/gen/art.bin + art.h.

Usage: bake.py PACKAGE_DIR
Reads PACKAGE_DIR/assets/ (gitignored: the art licences forbid redistributing it).
"""

import json
import os
import struct
import sys

from PIL import Image

KITS = {"base": "Crawler-View-Kit", "deep": "Crawler-View-Kit-Deep-Places"}
THEMES = [
    ("base", "stone-dungeon"),
    ("base", "sewer"),
    ("deep", "mine-shaft"),
    ("base", "crypt"),
    ("deep", "castle-library"),
    ("deep", "desert-tomb"),
    ("base", "overgrown-ruins"),
    ("deep", "fungal-cavern"),
    ("deep", "drowned-temple"),
    ("base", "ice-cave"),
    ("deep", "clockwork-vault"),
    ("base", "lava-forge"),
]
SETS = ["wall", "door-closed", "door-open", "stairs-up", "torch", "grate", "lever"]
POS = {"L3": -3, "L2": -2, "L1": -1, "C": 0, "R1": 1, "R2": 2, "R3": 3}
ANIMS = ["idle", "move", "attack", "hurt", "death"]
CROP_X0, CROP_X1 = 40, 280
SPLAT_SETS = 5
SPLAT_FRAMES = 24
STAINS = [(1, 8, 0.2, 0.25, 0.6, 0.5), (3, 10, 0.1, 0.45, 0.5, 0.5), (5, 9, 0.4, 0.1, 0.5, 0.45)]
DEPTH_SHADE = [1.0, 0.8, 0.6, 0.45]

SKIP, RUN, LITERAL = 0, 1, 2


def encode(pixels, w, h, palette):
    out = bytearray()
    for y in range(h):
        row = [palette.get(pixels[y * w + x]) for x in range(w)]
        x = 0
        while x < w:
            if row[x] is None:
                n = 1
                while x + n < w and row[x + n] is None and n < 64:
                    n += 1
                out.append((SKIP << 6) | (n - 1))
                x += n
                continue
            n = 1
            while x + n < w and row[x + n] == row[x] and n < 64:
                n += 1
            if n >= 3:
                out += bytes([(RUN << 6) | (n - 1), row[x]])
                x += n
                continue
            n = 0
            while x + n < w and row[x + n] is not None and n < 64:
                if x + n + 2 < w and row[x + n] == row[x + n + 1] == row[x + n + 2]:
                    break
                n += 1
            n = max(n, 1)
            out.append((LITERAL << 6) | (n - 1))
            out += bytes(row[x : x + n])
            x += n
    return out


def rgba(img):
    img = img.convert("RGBA")
    return [(r, g, b) if a >= 128 else None for r, g, b, a in img.get_flattened_data()]


def rgb565(c):
    r, g, b = c
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


class Blob:
    def __init__(self):
        self.data = bytearray()

    def add(self, b):
        while len(self.data) % 4:
            self.data.append(0)
        off = len(self.data)
        self.data += b
        return off


def build_palette(images):
    colours = sorted({c for px in images for c in px if c is not None})
    if len(colours) > 256:
        sys.exit(f"palette has {len(colours)} colours, max 256")
    return {c: i for i, c in enumerate(colours)}, colours


def sprite(blob, img, x, y, palette):
    w, h = img.size
    off = blob.add(encode(rgba(img), w, h, palette))
    return (off, x, y, w, h)


def splat_frame(blood, set_no, i):
    return Image.open(os.path.join(blood, str(set_no), f"1_{i}.png")).convert("RGBA")


def stain_sprite(blood, piece_img, piece, stain, depth):
    set_no, frame, u0, v0, su, sv = stain
    src = splat_frame(blood, set_no, frame)
    src = src.crop(src.getbbox())
    sw, sh = src.size
    spx = src.load()
    alpha = piece_img.convert("RGBA").split()[3].load()
    w, h = piece_img.size
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    opx = out.load()
    shade = DEPTH_SHADE[depth]
    for x in range(w):
        ys = [y for y in range(h) if alpha[x, y] > 0]
        if not ys:
            continue
        top, bot = ys[0], ys[-1] + 1
        u = (x + 0.5) / w
        if not (u0 <= u < u0 + su):
            continue
        tx = int((u - u0) / su * sw)
        for y in range(top, bot):
            v = (y + 0.5 - top) / (bot - top)
            if not (v0 <= v < v0 + sv):
                continue
            r, g, b, a = spx[tx, int((v - v0) / sv * sh)]
            if a >= 128:
                opx[x, y] = (int(r * shade) & 0xF8, int(g * shade) & 0xFC, int(b * shade) & 0xF8, 255)
    x0 = max(piece["x"], CROP_X0) - piece["x"]
    x1 = min(piece["x"] + piece["w"], CROP_X1) - piece["x"]
    if x1 <= x0 or out.getbbox() is None:
        return None
    return out.crop((x0, 0, x1, piece["h"])), piece["x"] + x0 - CROP_X0, piece["y"]


def main(pkg):
    assets = os.path.join(pkg, "assets")
    kits = {k: os.path.join(assets, v) for k, v in KITS.items()}
    mon = os.path.join(assets, "Dungeon-Monsters-Remastered-32x32")
    blood = os.path.join(assets, "Blood-Splatter")
    for d in list(kits.values()) + [mon, blood]:
        if not os.path.isdir(d):
            sys.exit(f"missing {d}: unzip the art kits into {assets}")
    kit = kits["base"]
    layout = json.load(open(os.path.join(kit, "layout.json")))
    pieces = layout["pieces"]

    slots = []
    for name in layout["draw_order"]:
        face, depth, pos = name.split("_")
        slots.append((name, int(depth[1:]), POS[pos], face == "side"))
    depths = [s[1] for s in slots]
    if depths != sorted(depths, reverse=True):
        sys.exit("draw_order is not far-to-near by depth")

    blob = Blob()
    themes = []
    for source, theme in THEMES:
        folder = os.path.join(kits[source], theme, "pieces")

        def load(name, p):
            img = Image.open(os.path.join(folder, name + ".png"))
            x0 = max(p["x"], CROP_X0) - p["x"]
            x1 = min(p["x"] + p["w"], CROP_X1) - p["x"]
            if x1 <= x0:
                return None
            return img.crop((x0, 0, x1, p["h"])), p["x"] + x0 - CROP_X0, p["y"]

        loaded = {}
        loaded["background"] = load("background", {"x": 0, "y": 0, "w": 320, "h": 200})
        for name, p in pieces.items():
            loaded[name] = load(name, p)
        palette, colours = build_palette([rgba(v[0]) for v in loaded.values() if v])
        pal_off = blob.add(b"".join(struct.pack("<H", rgb565(c)) for c in colours))

        def spr(name):
            v = loaded.get(name)
            if not v:
                return (0, 0, 0, 0, 0)
            return sprite(blob, v[0], v[1], v[2], palette)

        bg = spr("background")
        sets = [[spr(f"{s}_{slot[0]}") for slot in slots] for s in SETS]
        hatch = [[spr(f"stairs-down_floor_d{d}_{p}") for p in POS] for d in range(4)]
        themes.append((theme, pal_off, len(colours), bg, sets, hatch))

    wall_dir = os.path.join(kit, "stone-dungeon", "pieces")
    stains = []
    for stain in STAINS:
        row = []
        for name, depth, _, _ in slots:
            p = pieces.get("wall_" + name)
            row.append(p and stain_sprite(blood, Image.open(os.path.join(wall_dir, f"wall_{name}.png")), p, stain, depth))
        stains.append(row)
    stain_palette, stain_colours = build_palette([rgba(v[0]) for row in stains for v in row if v])
    stain_pal_off = blob.add(b"".join(struct.pack("<H", rgb565(c)) for c in stain_colours))
    stain_sprites = [[sprite(blob, *v, stain_palette) if v else (0, 0, 0, 0, 0) for v in row] for row in stains]

    splat_frames = []
    for n in range(1, SPLAT_SETS + 1):
        frames = [splat_frame(blood, n, i) for i in range(SPLAT_FRAMES)]
        boxes = [f.getbbox() for f in frames if f.getbbox()]
        box = (min(b[0] for b in boxes), min(b[1] for b in boxes), max(b[2] for b in boxes), max(b[3] for b in boxes))
        splat_frames.append([f.crop(box) for f in frames])
    splat_palette, splat_colours = build_palette([rgba(f) for fs in splat_frames for f in fs])
    splat_pal_off = blob.add(b"".join(struct.pack("<H", rgb565(c)) for c in splat_colours))
    splat_sprites = [[sprite(blob, f, 0, 0, splat_palette) for f in fs] for fs in splat_frames]

    atlas_meta = json.load(open(os.path.join(mon, "atlas.json")))
    atlas = Image.open(os.path.join(mon, "atlas.png"))
    fw, fh = atlas_meta["frame_w"], atlas_meta["frame_h"]
    monsters = list(atlas_meta["monsters"].items())
    frames = []
    for _, anims in monsters:
        per = []
        for a in ANIMS:
            spec = anims[a]
            per.append(
                [
                    atlas.crop(((spec["first_col"] + i) * fw, spec["row"] * fh, (spec["first_col"] + i + 1) * fw, (spec["row"] + 1) * fh))
                    for i in range(spec["frames"])
                ]
            )
        frames.append(per)
    max_frames = max(len(anim) for per in frames for anim in per)
    mpals, msprites = [], []
    for per in frames:
        mpalette, mcolours = build_palette([rgba(f) for anim in per for f in anim])
        mpals.append((blob.add(b"".join(struct.pack("<H", rgb565(c)) for c in mcolours)), len(mcolours)))
        msprites.append([[sprite(blob, f, 0, 0, mpalette) for f in anim] for anim in per])
    first = monsters[0][1]
    anim_frames = [first[a]["frames"] for a in ANIMS]
    anim_fps = [first[a]["fps"] for a in ANIMS]

    gen = os.path.join(pkg, "src", "gen")
    os.makedirs(gen, exist_ok=True)
    with open(os.path.join(gen, "art.bin"), "wb") as f:
        f.write(blob.data)

    def s(t):
        return "{%d, %d, %d, %d, %d}" % t

    L = []
    L.append("#pragma once\n\n#include <stdint.h>\n\nnamespace art {\n")
    L.append("struct Sprite {\n  uint32_t off;\n  int16_t x;\n  int16_t y;\n  uint16_t w;\n  uint16_t h;\n};\n")
    L.append("struct Slot {\n  int8_t depth;\n  int8_t col;\n  bool side;\n};\n")
    L.append(f"constexpr int kThemeCount = {len(THEMES)};")
    L.append(f"constexpr int kSlotCount = {len(slots)};")
    L.append(f"constexpr int kSetCount = {len(SETS)};")
    L.append("enum Set { Wall, DoorClosed, DoorOpen, StairsUp, Torch, Grate, Lever };")
    L.append(f"constexpr int kViewW = {CROP_X1 - CROP_X0};")
    L.append(f"constexpr int kViewH = {layout['view'][1]};")
    L.append(f"constexpr int kCropX = {CROP_X0};")
    L.append(f"constexpr float kFocal = {layout['focal']}f;\n")
    L.append("static const char* const kThemeIds[kThemeCount] = {%s};" % ", ".join(f'"{t}"' for _, t in THEMES))
    L.append("static constexpr Slot kSlots[kSlotCount] = {%s};" % ", ".join("{%d, %d, %s}" % (d, c, "true" if side else "false") for _, d, c, side in slots))
    L.append("static constexpr uint32_t kPalette[kThemeCount] = {%s};" % ", ".join(str(t[1]) for t in themes))
    L.append("static constexpr uint16_t kPaletteSize[kThemeCount] = {%s};" % ", ".join(str(t[2]) for t in themes))
    L.append("static constexpr Sprite kBackground[kThemeCount] = {%s};" % ", ".join(s(t[3]) for t in themes))
    L.append("static constexpr Sprite kPieces[kThemeCount][kSetCount][kSlotCount] = {")
    for t in themes:
        L.append("  {" + ",\n   ".join("{" + ", ".join(s(p) for p in row) + "}" for row in t[4]) + "},")
    L.append("};")
    L.append("static constexpr Sprite kHatch[kThemeCount][4][7] = {")
    for t in themes:
        L.append("  {" + ", ".join("{" + ", ".join(s(p) for p in row) + "}" for row in t[5]) + "},")
    L.append("};\n")
    L.append(f"constexpr int kMonsterCount = {len(monsters)};")
    L.append(f"constexpr int kMaxFrames = {max_frames};")
    L.append("enum Anim { Idle, Move, Attack, Hurt, Death };")
    L.append(f"constexpr int kMonsterSize = {fw};")
    L.append("static const char* const kMonsterIds[kMonsterCount] = {%s};" % ", ".join(f'"{m[0]}"' for m in monsters))
    L.append("static constexpr uint32_t kMonsterPalette[kMonsterCount] = {%s};" % ", ".join(str(p[0]) for p in mpals))
    L.append("static constexpr uint16_t kMonsterPaletteSize[kMonsterCount] = {%s};" % ", ".join(str(p[1]) for p in mpals))
    L.append("static constexpr uint8_t kAnimFrames[5] = {%s};" % ", ".join(map(str, anim_frames)))
    L.append("static constexpr uint8_t kAnimFps[5] = {%s};" % ", ".join(map(str, anim_fps)))
    L.append("static constexpr Sprite kMonsterFrames[kMonsterCount][5][kMaxFrames] = {")
    for per in msprites:
        L.append("  {" + ", ".join("{" + ", ".join(s(p) for p in anim) + "}" for anim in per) + "},")
    L.append("};\n")
    L.append(f"constexpr int kStainCount = {len(STAINS)};")
    L.append(f"static constexpr uint32_t kStainPalette = {stain_pal_off};")
    L.append(f"static constexpr uint16_t kStainPaletteSize = {len(stain_colours)};")
    L.append("static constexpr Sprite kStains[kStainCount][kSlotCount] = {")
    for row in stain_sprites:
        L.append("  {" + ", ".join(s(p) for p in row) + "},")
    L.append("};\n")
    L.append(f"constexpr int kSplatSets = {SPLAT_SETS};")
    L.append(f"constexpr int kSplatFrames = {SPLAT_FRAMES};")
    L.append(f"static constexpr uint32_t kSplatPalette = {splat_pal_off};")
    L.append(f"static constexpr uint16_t kSplatPaletteSize = {len(splat_colours)};")
    L.append("static constexpr Sprite kSplats[kSplatSets][kSplatFrames] = {")
    for row in splat_sprites:
        L.append("  {" + ", ".join(s(p) for p in row) + "},")
    L.append("};\n\n}  // namespace art")
    with open(os.path.join(gen, "art.h"), "w") as f:
        f.write("\n".join(L) + "\n")
    print(f"baked {len(blob.data) / 1024:.0f} KB into {gen}")


if __name__ == "__main__":
    main(os.path.abspath(sys.argv[1]))
