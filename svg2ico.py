#!/usr/bin/env python3
import struct, subprocess, sys, os

svg, ico = sys.argv[1], sys.argv[2]
styles = sys.argv[3]
png = ico + ".png"
try:
    h = 256
    # Turning the SVG into a PNG
    subprocess.run(
        ['rsvg-convert', svg, '-s', '/dev/stdin', '-o', png, '-h', str(h)],
        input=styles.encode('utf-8'),
        check=True
    )
    # Quantizing the PNG to 8-bit
    subprocess.run(['pngquant', '--nofs', '8', png, '--speed', '1', '--output', png, '--force'], check=True)
    # Optimizing the PNG
    subprocess.run(['zopflipng', '-y',
                    '--iterations=15', '--filters=01234mepb',
                    '--lossy_8bit', '--lossy_transparent',
                    png, png], check=True)
    # Turning the PNG into an ICO
    data = open(png, 'rb').read()
    s = 0 if h == 256 else h  # 256 -> 0
    open(ico, 'wb').write(
        struct.pack('<HHHBBBBHHII', 0, 1, 1, s, s, 0, 0, 1, 32, len(data), 22) + data)
finally:
    if os.path.exists(png):
        os.remove(png)
