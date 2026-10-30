"""Turn a video into a looping JPEG clip for the fireplace firmware, and flash it.

  uv run --with av --with pillow tools/clip.py make VIDEO [--crop X,Y,SIZE] [--start SEC] [--quality Q]
  uv run --with esptool tools/clip.py flash

`make` crops a square from the video, scales it to 240x240, encodes each frame as a baseline
JPEG, crossfades the last frames into the first so the loop has no seam, and packs as many
frames as fit the `clip` partition into clip.bin. `flash` writes clip.bin to that partition.
"""

import argparse
import io
import os
import struct
import subprocess
import sys
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
PKG = os.path.dirname(HERE)
SIZE = 240
HEADER = 20


def clip_partition():
    with open(os.path.join(PKG, "partitions.csv")) as f:
        for line in f:
            cols = [c.strip() for c in line.split(",")]
            if cols[0] == "clip":
                return int(cols[3], 16), int(cols[4], 16)
    sys.exit("no clip partition in partitions.csv")


def encode(img, quality):
    buf = io.BytesIO()
    img.save(buf, "JPEG", quality=quality, optimize=True, progressive=False)
    return buf.getvalue()


def pack(jpegs, fps):
    table = b"".join(struct.pack("<I", o) for o in offsets(jpegs))
    body = table + b"".join(jpegs)
    head = b"FCLP" + struct.pack("<HHHHII", SIZE, SIZE, fps, len(jpegs), len(body), zlib.crc32(body))
    return head + body


def offsets(jpegs):
    out, o = [], 0
    for j in jpegs:
        out.append(o)
        o += len(j)
    out.append(o)
    return out


def read_frames(path, crop, start, limit):
    import av

    x, y, s = crop
    with av.open(path) as c:
        stream = c.streams.video[0]
        stream.thread_type = "AUTO"
        fps = round(float(stream.average_rate))
        skip = round(start * fps)
        frames = []
        for i, f in enumerate(c.decode(video=0)):
            if i < skip:
                continue
            img = f.to_image().crop((x, y, x + s, y + s)).resize((SIZE, SIZE), resample=1)
            frames.append(img)
            if len(frames) >= limit:
                break
    return frames, fps


def looped(frames, n, fade):
    from PIL import Image

    out = list(frames[:n])
    for i in range(fade):
        t = (i + 1) / (fade + 1)
        out[i] = Image.blend(frames[n + i], frames[i], t)
    return out


def make(args):
    _, budget = clip_partition()
    crop = tuple(int(v) for v in args.crop.split(","))
    frames, fps = read_frames(args.video, crop, args.start, args.max_frames + args.fade)
    if len(frames) <= args.fade + 1:
        sys.exit("video too short")
    sizes = [len(encode(f, args.quality)) for f in frames]
    n = len(frames) - args.fade
    while n > 1 and HEADER + 4 * (n + 1) + sum(sizes[:n]) > budget * 0.98:
        n -= 1
    while True:
        jpegs = [encode(f, args.quality) for f in looped(frames, n, args.fade)]
        blob = pack(jpegs, fps)
        if len(blob) <= budget:
            break
        n -= 5
    with open(args.out, "wb") as f:
        f.write(blob)
    print(f"{args.out}: {n} frames, {n / fps:.1f} s at {fps} fps, {len(blob) // 1024} KB of {budget // 1024} KB")


def flash(args):
    offset, budget = clip_partition()
    if os.path.getsize(args.clip) > budget:
        sys.exit(f"{args.clip} is larger than the clip partition")
    cmd = [sys.executable, "-m", "esptool", "--chip", "esp32", "--port", args.port, "--baud", "230400",
           "write_flash", hex(offset), args.clip]
    sys.exit(subprocess.call(cmd))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = p.add_subparsers(dest="cmd", required=True)
    m = sub.add_parser("make")
    m.add_argument("video")
    m.add_argument("--crop", default="850,403,1700", help="square in source pixels: X,Y,SIZE")
    m.add_argument("--start", type=float, default=0.0, help="seconds into the video")
    m.add_argument("--quality", type=int, default=75)
    m.add_argument("--fade", type=int, default=15, help="crossfade frames at the loop seam")
    m.add_argument("--max-frames", type=int, default=600)
    m.add_argument("--out", default=os.path.join(PKG, "clip.bin"))
    m.set_defaults(fn=make)
    f = sub.add_parser("flash")
    f.add_argument("--clip", default=os.path.join(PKG, "clip.bin"))
    f.add_argument("--port", default=os.environ.get("HUD_PORT", "/dev/cu.usbserial-210"))
    f.set_defaults(fn=flash)
    args = p.parse_args()
    args.fn(args)


if __name__ == "__main__":
    main()
