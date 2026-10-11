#!/usr/bin/env python3
import struct, subprocess, sys, os

svg, ico, styles = sys.argv[1], sys.argv[2], sys.argv[3]
sb = styles.encode()
p256, p32, b32 = ico + ".256.png", ico + ".32.png", ico + ".32.bmp"

def render(h, out):
    subprocess.run(['rsvg-convert', svg, '-s', '/dev/stdin', '-o', out, '-h', str(h)],
                   input=sb, check=True)

def run(*args):
    subprocess.run(args, check=True)

try:
    render(256, p256)
    run('pngquant', '--nofs', '8', '--speed', '1', '--output', p256, '--force', p256)
    run('zopflipng', '-y', '--iterations=15', '--filters=01234mepb',
        '--lossy_8bit', '--lossy_transparent', p256, p256)

    render(32, p32)
    run('pngquant', '--nofs', '8', '--speed', '1', '--output', p32, '--force', p32)
    run('magick', p32, '-colors', '256', '-type', 'palette', '+dither', 'BMP3:' + b32)

    with open(p256, 'rb') as f: png = f.read()
    with open(b32, 'rb') as f: bmp = bytearray(f.read()[14:])
    # ICO stores the DIB with biHeight = 2× real height (XOR pixels + AND mask)
    struct.pack_into('<i', bmp, 8, struct.unpack_from('<i', bmp, 8)[0] * 2)
    bmp += bytes((((32 + 31) // 32) * 4) * 32)

    hdr = struct.pack('<HHH', 0, 1, 2)
    off = 6 + 16 * 2
    ents = struct.pack('<BBBBHHII', 0, 0, 0, 0, 1, 32, len(png), off)
    ents += struct.pack('<BBBBHHII', 32, 32, 0, 0, 1, 8, len(bmp), off + len(png))
    with open(ico, 'wb') as f: f.write(hdr + ents + png + bmp)
finally:
    for f in (p256, p32, b32):
        if os.path.exists(f):
            os.remove(f)
