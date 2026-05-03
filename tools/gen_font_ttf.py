#!/usr/bin/env python3
"""
Convert a TTF/OTF font to screen-ui Font bitmap format.
Uses Pillow — no external tools required.

Usage:
  python3 tools/gen_font_ttf.py <font.ttf> <pixel_size> <symbol_name> [threshold]

  threshold: binarization threshold 0-255 (default 96; lower = thicker strokes)

Examples:
  python3 tools/gen_font_ttf.py /usr/share/fonts/google-noto-vf/NotoSans[wght].ttf 12 font_noto12
  python3 tools/gen_font_ttf.py /usr/share/fonts/google-roboto/Roboto-Regular.ttf 16 font_roboto16
"""
import sys, os, math
from PIL import Image, ImageDraw, ImageFont

CHARS = range(32, 127)

def main():
    if len(sys.argv) < 4:
        print(__doc__)
        sys.exit(1)

    ttf_path  = sys.argv[1]
    size      = int(sys.argv[2])
    sym       = sys.argv[3]
    threshold = int(sys.argv[4]) if len(sys.argv) > 4 else 96

    font = ImageFont.truetype(ttf_path, size)

    # ── Measure glyph pixel extents ──────────────────────────────────────────
    # font.getbbox(ch) → (left, top, right, bottom) pixels from draw origin
    min_top    =  9999
    max_bottom = -9999
    cell_w     = 0

    for code in CHARS:
        ch = chr(code)
        try:
            bb = font.getbbox(ch)
        except Exception:
            continue
        if not bb or bb[2] <= bb[0]:   # skip empty glyphs (e.g. space)
            continue
        if bb[1] < min_top:    min_top    = bb[1]
        if bb[3] > max_bottom: max_bottom = bb[3]
        try:
            adv = math.ceil(font.getlength(ch))
        except AttributeError:
            adv = bb[2] - bb[0]
        if adv > cell_w:
            cell_w = adv

    cell_h = max_bottom - min_top
    if cell_h <= 0 or cell_w <= 0:
        sys.exit("Failed to measure font — check font path")

    stride         = (cell_w + 7) // 8
    bytes_per_char = stride * cell_h

    # ── Render each glyph into a cell ────────────────────────────────────────
    # Draw each char shifted so pixel row 0 = topmost ascender line.
    glyphs = {}
    for code in CHARS:
        ch = chr(code)
        img  = Image.new('L', (cell_w + 4, cell_h + 4), 0)
        draw = ImageDraw.Draw(img)
        try:
            bb   = font.getbbox(ch)
            x_off = -bb[0] if bb[0] < 0 else 0
            y_off = -min_top
        except Exception:
            x_off, y_off = 0, -min_top

        draw.text((x_off, y_off), ch, font=font, fill=255)
        crop   = img.crop((0, 0, cell_w, cell_h))
        pixels = crop.load()

        data = []
        for row in range(cell_h):
            for byte_idx in range(stride):
                byte = 0
                for bit in range(8):
                    col = byte_idx * 8 + bit
                    if col < cell_w and pixels[col, row] >= threshold:
                        byte |= (1 << bit)
                data.append(byte)
        glyphs[code] = data

    empty = [0] * bytes_per_char
    table = [glyphs.get(code, empty) for code in range(128)]

    # ── Write C source ────────────────────────────────────────────────────────
    outfile  = f"{sym}.c"
    basename = os.path.basename(ttf_path)
    with open(outfile, 'w') as out:
        out.write(f'// Auto-generated from {basename} at {size}px\n')
        out.write(f'// {cell_w}x{cell_h} bitmap font, LSB = leftmost pixel\n')
        out.write(f'// To regenerate: python3 tools/gen_font_ttf.py "{ttf_path}" {size} {sym}\n\n')
        out.write('#include <stdint.h>\n')
        out.write('#include "fonts.h"\n\n')
        out.write(f'static const uint8_t {sym}_data[128][{bytes_per_char}] = {{\n')
        for code, glyph in enumerate(table):
            hexvals = ','.join(f'0x{b:02X}' for b in glyph)
            if 0x20 <= code <= 0x7E and code != 0x5C:
                ch_disp = chr(code)
            else:
                ch_disp = f'0x{code:02X}'
            out.write(f'    [0x{code:02X}] = {{{hexvals}}}, // {ch_disp}\n')
        out.write('};\n\n')
        out.write(f'const Font {sym} = {{\n')
        out.write(f'    .w      = {cell_w},\n')
        out.write(f'    .h      = {cell_h},\n')
        out.write(f'    .stride = {stride},\n')
        out.write(f'    .data   = (const uint8_t *){sym}_data,\n')
        out.write('};\n')

    print(f"Generated {outfile}: {cell_w}x{cell_h}, stride={stride}, "
          f"{bytes_per_char} bytes/char, threshold={threshold}")

main()
