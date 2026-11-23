"""Compares two folders of shots from shots.sh: prints every picture that differs and where.

    python3 tools/shots/compare.py <before> <after>

A refactor should change nothing but the clock (receipt times), which shows up as a few
small boxes; anything larger is worth looking at. Needs Pillow (python3-pil).
"""
import os
import sys

from PIL import Image, ImageChops


def main(before: str, after: str) -> int:
    names = sorted(set(os.listdir(before)) | set(os.listdir(after)))
    changed = 0
    for name in names:
        if not (os.path.exists(os.path.join(before, name)) and os.path.exists(os.path.join(after, name))):
            print(f"{name}: only in one folder")
            changed += 1
            continue
        a = Image.open(os.path.join(before, name)).convert("RGB")
        b = Image.open(os.path.join(after, name)).convert("RGB")
        if a.size != b.size:
            print(f"{name}: size {a.size} -> {b.size}")
            changed += 1
            continue
        box = ImageChops.difference(a, b).getbbox()
        if box:
            print(f"{name}: differs in {box}")
            changed += 1
    print(f"{len(names)} compared, {changed} differ")
    return 1 if changed else 0


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    sys.exit(main(sys.argv[1], sys.argv[2]))
