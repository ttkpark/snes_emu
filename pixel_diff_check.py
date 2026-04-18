#!/usr/bin/env python3
import sys
from PIL import Image
import os

def compare_frames(frame_file, ref_file):
    """Compare frame_file with reference, return % diff."""
    if not os.path.exists(frame_file):
        print(f"Frame not found: {frame_file}")
        return -1
    if not os.path.exists(ref_file):
        print(f"Reference not found: {ref_file}")
        return -1

    frame = Image.open(frame_file).convert('RGB')
    ref = Image.open(ref_file).convert('RGB')

    if frame.size != ref.size:
        print(f"Size mismatch: {frame.size} vs {ref.size}")
        return -1

    frame_pix = frame.load()
    ref_pix = ref.load()

    w, h = frame.size
    diff_count = 0

    for y in range(h):
        for x in range(w):
            if frame_pix[x, y] != ref_pix[x, y]:
                diff_count += 1

    total = w * h
    diff_pct = 100.0 * diff_count / total
    return diff_pct

# Check key frames
checks = [
    ("frame_f0060.bmp", "tests/snes9x_ref/tc01_menu.png", "TC-01 Menu"),
    ("frame_f0580.bmp", "tests/snes9x_ref/tc_passed.png", "PASSED Electronics Test"),
    ("frame_f0950.bmp", "tests/snes9x_ref/chartest_phase1.png", "Character Test Phase 1"),
]

for frame_f, ref_f, desc in checks:
    diff = compare_frames(frame_f, ref_f)
    if diff >= 0:
        print(f"{desc}: {diff:.2f}% diff")
    else:
        print(f"{desc}: ERROR")
