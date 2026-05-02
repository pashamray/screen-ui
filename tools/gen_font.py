#!/usr/bin/env python3
"""
Convert ttf2ugui 1BPP output to our Font struct format.

Usage: python3 gen_font.py <ttf2ugui_output.c> <symbol_name>
Example: python3 gen_font.py LiberationMono-Regular_10X16.c font_mono16
"""
import re, sys, os

def main():
    if len(sys.argv) < 3:
        print(f"Usage: {sys.argv[0]} <input.c> <symbol_name>")
        sys.exit(1)

    infile, sym = sys.argv[1], sys.argv[2]
    with open(infile) as f:
        content = f.read()

    # Parse header to get width, height, bytes_per_char
    header_match = re.search(
        r'// BPP.*?\n\s*(0x[0-9A-Fa-f]+(?:,0x[0-9A-Fa-f]+)+)',
        content)
    if not header_match:
        sys.exit("Cannot find header bytes")
    hbytes = [int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]+', header_match.group(1))]
    # Layout: BPP, Width, Height, Chars_H, Chars_L, Offsets_H, Offsets_L, BPC_H, BPC_L, WidthsPresent
    char_w         = hbytes[1]
    char_h         = hbytes[2]
    bytes_per_char = (hbytes[7] << 8) | hbytes[8]
    stride         = bytes_per_char // char_h

    # Parse bitmap data section — one line per char
    bmp_start = content.find('// Bitmap data')
    if bmp_start == -1:
        sys.exit("Cannot find bitmap data section")

    chars = {}  # char_code -> list[bytes]
    for line in content[bmp_start:].splitlines():
        if '//' not in line:
            continue
        hex_part, comment = line.split('//', 1)
        code_match = re.search(r'0x([0-9A-Fa-f]+)', comment)
        if not code_match:
            continue
        code = int(code_match.group(1), 16)
        vals = [int(x.strip(), 16) for x in hex_part.strip().rstrip(',').split(',') if x.strip()]
        if vals:
            chars[code] = vals

    # Build 128-char table
    empty = [0] * bytes_per_char
    table = [chars.get(code, empty) for code in range(128)]

    # Output C source
    outfile = os.path.splitext(os.path.basename(infile))[0] + '_font.c'
    with open(outfile, 'w') as out:
        out.write(f'// Auto-generated from {os.path.basename(infile)}\n')
        out.write(f'// {char_w}x{char_h} bitmap font, LSB = leftmost pixel\n')
        out.write(f'// To regenerate: python3 tools/gen_font.py <ttf2ugui_output.c> {sym}\n\n')
        out.write('#include <stdint.h>\n')
        out.write('#include "render/render.h"\n\n')
        out.write(f'static const uint8_t {sym}_data[128][{bytes_per_char}] = {{\n')
        for code, glyph in enumerate(table):
            hexvals = ','.join(f'0x{b:02X}' for b in glyph)
            ch_disp = chr(code) if 0x20 <= code <= 0x7E else '.'
            out.write(f'    [0x{code:02X}] = {{{hexvals}}}, // {ch_disp}\n')
        out.write('};\n\n')
        out.write(f'const Font {sym} = {{\n')
        out.write(f'    .w      = {char_w},\n')
        out.write(f'    .h      = {char_h},\n')
        out.write(f'    .stride = {stride},\n')
        out.write(f'    .data   = (const uint8_t *){sym}_data,\n')
        out.write('};\n')

    print(f"Generated {outfile}: {char_w}x{char_h}, stride={stride}, {bytes_per_char} bytes/char")

main()
