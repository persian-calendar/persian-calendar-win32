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

def bmp_frame(size):
    png = render(size)
    run('pngquant', '--nofs', '8', '--speed', '1', '--output', png, '--force', png)
    rgba = subprocess.check_output(['magick', png, '-depth', '8', 'rgba:-'])
    assert len(rgba) == size * size * 4, f"expected {size*size*4} bytes, got {len(rgba)}"

    # BITMAPINFOHEADER, biHeight doubled to hold XOR + AND
    bih = struct.pack('<IiiHHIIiiII', 40, size, size * 2, 1, 32, 0, 0, 0, 0, 0, 0)

    bgra = bytearray(size * size * 4)
    stride = ((size + 31) // 32) * 4
    mask = bytearray(stride * size)
    for y in range(size):
        for x in range(size):
            src = ((size - 1 - y) * size + x) * 4
            dst = (y * size + x) * 4
            r, g, b, a = rgba[src], rgba[src+1], rgba[src+2], rgba[src+3]
            bgra[dst:dst+4] = bytes((b, g, r, a))
            if a < 128:
                mask[y * stride + x // 8] |= 1 << (7 - (x % 8))

    return size, size, 32, bih + bytes(bgra) + bytes(mask)

try:
    p256 = render(256)
    run('pngquant', '--nofs', '8', '--speed', '1', '--output', p256, '--force', p256)
    run('zopflipng', '-y', '--iterations=15', '--filters=01234mepb',
        '--lossy_8bit', '--lossy_transparent', p256, p256)
    with open(p256, 'rb') as f: png = f.read()

    frames = [(256, 256, 32, png), bmp_frame(32)]

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
