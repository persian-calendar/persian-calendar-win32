#!/usr/bin/env python3
import struct, subprocess, sys, os

svg, ico, styles = sys.argv[1], sys.argv[2], sys.argv[3]
sb = styles.encode()
tmp = []

def render(h):
    p = f"{ico}.{h}.png"; tmp.append(p)
    subprocess.run(['rsvg-convert', svg, '-s', '/dev/stdin', '-o', p, '-h', str(h)],
                   input=sb, check=True)
    return p

def run(*a): subprocess.run(a, check=True)

def and_mask(png, size):
    pgm = png + ".pgm"; tmp.append(pgm)
    run('magick', png, '-alpha', 'extract', '-depth', '8', pgm)
    with open(pgm, 'rb') as f:
        hdr = []
        while len(hdr) < 4: hdr += f.readline().split()
        alpha = f.read(size * size)
    stride = ((size + 31) // 32) * 4
    m = bytearray(stride * size)
    for y in range(size):
        for x in range(size):
            if alpha[(size - 1 - y) * size + x] < 128:
                m[y * stride + x // 8] |= 1 << (7 - (x % 8))
    return bytes(m)

def bmp_frame(size):
    png = render(size)
    run('pngquant', '--nofs', '8', '--speed', '1', '--output', png, '--force', png)
    mask = and_mask(png, size)
    b = png + ".bmp"; tmp.append(b)
    run('magick', png, '-colors', '256', '-type', 'palette', '+dither', 'BMP3:' + b)
    with open(b, 'rb') as f: d = bytearray(f.read()[14:])
    struct.pack_into('<i', d, 8, struct.unpack_from('<i', d, 8)[0] * 2)
    return size, size, 8, bytes(d) + mask

try:
    p256 = render(256)
    run('pngquant', '--nofs', '8', '--speed', '1', '--output', p256, '--force', p256)
    run('zopflipng', '-y', '--iterations=15', '--filters=01234mepb',
        '--lossy_8bit', '--lossy_transparent', p256, p256)
    with open(p256, 'rb') as f: png = f.read()

    frames = [(256, 256, 32, png), bmp_frame(48)]

    hdr = struct.pack('<HHH', 0, 1, len(frames))
    off = 6 + 16 * len(frames)
    ents, body = bytearray(), bytearray()
    for w, h, bpp, d in frames:
        ents += struct.pack('<BBBBHHII', 0 if w >= 256 else w, 0 if h >= 256 else h,
                            0, 0, 1, bpp, len(d), off)
        off += len(d); body += d

    with open(ico, 'wb') as f: f.write(hdr + bytes(ents) + bytes(body))
finally:
    for f in tmp:
        if os.path.exists(f): os.remove(f)
