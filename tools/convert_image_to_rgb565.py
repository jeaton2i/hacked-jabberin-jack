"""Converts a source image into a raw RGB565 C array for the firmware.

The image is letterboxed (resized to fit, centered on a black background)
to the panel's exact resolution so the firmware can blit it in one shot
with no runtime decoding. The output is mirrored horizontally to match
how the panel actually renders (confirmed against hardware: an unflipped
source appears mirrored left-to-right on screen).

Usage:
    python convert_image_to_rgb565.py <input image> <output .h> <array name>
        [--crop L,T,R,B] [--flatten-checkerboard] [--flood-fill-background]
        [--extra-seed X,Y ...] [--scale FACTOR] [--max-height PX]
        [--max-width PX] [--width W] [--height H]

--extra-seed flood-fills an additional enclosed region to black (e.g. a
letter's counter-hole that doesn't touch the image border, so the corner
seeds in --flood-fill-background never reach it). Coordinates are in the
possibly-cropped image, i.e. relative to --crop's output if both are given.
Repeatable.
"""

import sys

from PIL import Image, ImageDraw

PANEL_WIDTH = 220
PANEL_HEIGHT = 176


def to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def flatten_checkerboard_to_black(im):
    """Some source PNGs bake a light gray/white checkerboard into the RGB
    data itself (no real alpha channel) to represent transparency in an
    editor. Treat any near-white/light-gray, low-saturation pixel as
    background and replace it with black."""
    px = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b = px[x, y]
            if abs(r - g) < 12 and abs(g - b) < 12 and abs(r - b) < 12 and r > 190:
                px[x, y] = (0, 0, 0)
    return im


def flood_fill_background_to_black(im, extra_seeds=()):
    """For images with a solid (not checkerboard) background that touches
    the canvas edges, flood-fill from the corners instead of a global
    threshold, so enclosed light-colored areas (e.g. white bone within a
    skull's outline) are left alone. extra_seeds flood-fills additional
    enclosed regions (e.g. a letter's counter-hole) that don't touch a
    corner and so wouldn't otherwise be reached."""
    im = im.copy()
    w, h = im.size
    seeds = [(0, 0), (w - 1, 0), (0, h - 1), (w - 1, h - 1)] + list(extra_seeds)
    for seed in seeds:
        ImageDraw.floodfill(im, seed, (0, 0, 0), thresh=40)
    return im


def load_rgb_on_black(path):
    im = Image.open(path)
    if "A" in im.getbands():
        im = im.convert("RGBA")
        background = Image.new("RGB", im.size, (0, 0, 0))
        background.paste(im, mask=im.split()[-1])
        return background
    return im.convert("RGB")


def main():
    args = sys.argv[1:]
    if len(args) < 3:
        print(__doc__)
        sys.exit(1)

    in_path, out_path, array_name = args[0], args[1], args[2]
    options = args[3:]

    width, height = PANEL_WIDTH, PANEL_HEIGHT
    crop_box = None
    flatten_checkerboard = False
    flood_fill_background = False
    extra_seeds = []
    scale = 1.0
    max_width = None
    max_height = None

    i = 0
    while i < len(options):
        opt = options[i]
        if opt == "--crop":
            crop_box = tuple(int(v) for v in options[i + 1].split(","))
            i += 2
        elif opt == "--flatten-checkerboard":
            flatten_checkerboard = True
            i += 1
        elif opt == "--flood-fill-background":
            flood_fill_background = True
            i += 1
        elif opt == "--extra-seed":
            extra_seeds.append(tuple(int(v) for v in options[i + 1].split(",")))
            flood_fill_background = True
            i += 2
        elif opt == "--scale":
            scale = float(options[i + 1])
            i += 2
        elif opt == "--max-height":
            max_height = int(options[i + 1])
            i += 2
        elif opt == "--max-width":
            max_width = int(options[i + 1])
            i += 2
        elif opt == "--width":
            width = int(options[i + 1])
            i += 2
        elif opt == "--height":
            height = int(options[i + 1])
            i += 2
        else:
            raise SystemExit(f"unknown option: {opt}")

    src = load_rgb_on_black(in_path)
    if crop_box is not None:
        src = src.crop(crop_box)
    if flatten_checkerboard:
        src = flatten_checkerboard_to_black(src)
    if flood_fill_background:
        src = flood_fill_background_to_black(src, extra_seeds)

    fit_width = max_width if max_width is not None else int(width * scale)
    fit_height = max_height if max_height is not None else int(height * scale)
    fitted = src.copy()
    fitted.thumbnail((fit_width, fit_height), Image.LANCZOS)
    canvas = Image.new("RGB", (width, height), (0, 0, 0))
    canvas.paste(fitted, ((width - fitted.width) // 2, (height - fitted.height) // 2))
    canvas = canvas.transpose(Image.FLIP_LEFT_RIGHT)

    pixels = list(canvas.getdata())
    values = [to_rgb565(r, g, b) for r, g, b in pixels]

    with open(out_path, "w") as f:
        f.write("#pragma once\n\n")
        f.write("#include <stdint.h>\n\n")
        f.write(f"constexpr int16_t {array_name}_width = {width};\n")
        f.write(f"constexpr int16_t {array_name}_height = {height};\n\n")
        f.write(f"const uint16_t {array_name}[] = {{\n")
        for i in range(0, len(values), 16):
            row = values[i:i + 16]
            f.write("  " + ", ".join(f"0x{v:04x}" for v in row) + ",\n")
        f.write("};\n")

    print(f"wrote {out_path}: {width}x{height} ({len(values)} pixels, {len(values)*2} bytes)")


if __name__ == "__main__":
    main()
