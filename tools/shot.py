"""Grab framebuffer screenshots from a running NM-TV-154 firmware over serial.

Usage: shot.py OUT_DIR [NAME=CMD;CMD;sleep N ...]
Each job sends its serial commands, then `shot`, and saves NAME.png (2x scale).
Opening the port resets the board; SHOT_WAIT (seconds, default 3.5) waits for boot.
"""

import os
import sys
import time

import serial
from PIL import Image

W = H = 240


def grab(port, name, out_dir):
    port.reset_input_buffer()
    port.write(b"shot\n")
    buf = b""
    t0 = time.time()
    while b"SHOT\n" not in buf and time.time() - t0 < 5:
        buf += port.read(256)
    if b"SHOT\n" not in buf:
        print(name, "no SHOT marker; got:", buf[-200:])
        return False
    data = buf.split(b"SHOT\n", 1)[1]
    need = W * H * 2
    tail = b"\nENDSHOT\n"
    while len(data) < need + len(tail) and time.time() - t0 < 30:
        data += port.read(need + len(tail) - len(data))
    if not data[need:].startswith(tail):
        print(name, "corrupt transfer (bytes dropped), retrying")
        return False
    img = Image.new("RGB", (W, H))
    px = img.load()
    for i in range(W * H):
        v = (data[2 * i] << 8) | data[2 * i + 1]
        r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
        px[i % W, i // W] = (r * 255 // 31, g * 255 // 63, b * 255 // 31)
    path = os.path.join(out_dir, f"{name}.png")
    img.resize((W * 2, H * 2), Image.NEAREST).save(path)
    print(path)
    return True


def main():
    out_dir = sys.argv[1]
    jobs = sys.argv[2:] or ["screen="]
    os.makedirs(out_dir, exist_ok=True)

    port = serial.Serial()
    port.port = os.environ.get("HUD_PORT", "/dev/cu.usbserial-210")
    port.baudrate = 230400
    port.timeout = 0.5
    port.dtr = False
    port.rts = False
    port.open()
    time.sleep(float(os.environ.get("SHOT_WAIT", "3.5")))

    for job in jobs:
        name, cmds = job.split("=", 1)
        for c in filter(None, cmds.split(";")):
            if c.startswith("sleep "):
                time.sleep(float(c[6:]))
            else:
                port.write((c + "\n").encode())
                time.sleep(0.3)
        for _ in range(3):
            if grab(port, name, out_dir):
                break
    port.close()


if __name__ == "__main__":
    main()
