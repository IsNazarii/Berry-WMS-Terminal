"""Frames raw README shots for docs/screenshots: scaled down, rounded corners, a hairline
border and a soft shadow on a transparent background, so they sit well on both GitHub
themes and line up in the README's grid (every shot has the same size).

    python3 tools/shots/frame.py <raw dir> <out dir>

Needs Pillow (python3-pil).
"""
import os
import sys

from PIL import Image, ImageChops, ImageDraw, ImageFilter

WIDTH = 1600          # final window width in pixels (the raw shots are 2x of 1280)
RADIUS = 18           # window corner radius
MARGIN = 36           # room for the shadow around the window
SHADOW_BLUR = 16
SHADOW_OFFSET = 8
SHADOW_ALPHA = 70     # 0..255


def rounded_mask(size, radius):
    mask = Image.new("L", size, 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, size[0] - 1, size[1] - 1), radius, fill=255)
    return mask


def frame(path, out_path, dark):
    shot = Image.open(path).convert("RGB")
    height = round(shot.height * WIDTH / shot.width)
    shot = shot.resize((WIDTH, height), Image.LANCZOS)
    size = (WIDTH, height)
    mask = rounded_mask(size, RADIUS)

    canvas = Image.new("RGBA", (WIDTH + 2 * MARGIN, height + 2 * MARGIN), (0, 0, 0, 0))
    shadow = Image.new("RGBA", canvas.size, (0, 0, 0, 0))
    shadow_mask = Image.new("L", canvas.size, 0)
    shadow_mask.paste(mask, (MARGIN, MARGIN + SHADOW_OFFSET))
    shadow_mask = shadow_mask.filter(ImageFilter.GaussianBlur(SHADOW_BLUR))
    shadow.putalpha(ImageChops.multiply(shadow_mask, Image.new("L", canvas.size, SHADOW_ALPHA)))
    canvas = Image.alpha_composite(canvas, shadow)

    window = shot.convert("RGBA")
    window.putalpha(mask)
    canvas.alpha_composite(window, (MARGIN, MARGIN))
    border = (255, 255, 255, 40) if dark else (0, 0, 0, 38)
    ImageDraw.Draw(canvas).rounded_rectangle(
        (MARGIN, MARGIN, MARGIN + WIDTH - 1, MARGIN + height - 1), RADIUS, outline=border, width=2)
    canvas.save(out_path, optimize=True)


def main(raw, out):
    os.makedirs(out, exist_ok=True)
    for name in sorted(os.listdir(raw)):
        if name.endswith(".png"):
            frame(os.path.join(raw, name), os.path.join(out, name), dark=name.endswith("-dark.png"))
            print(name)


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
