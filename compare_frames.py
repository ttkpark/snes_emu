#!/usr/bin/env python3
"""Compare emulator output with Snes9x reference frames."""

from PIL import Image
import numpy as np
from collections import Counter
import os
import sys

def analyze_frame(filename):
    """Get color distribution of a frame."""
    if not os.path.exists(filename):
        return None

    img = Image.open(filename).convert('RGB')
    arr = np.array(img)
    pixels = arr.reshape(-1, 3)

    color_counts = Counter(map(tuple, pixels))
    return color_counts

def compare_frames(emulator_file, reference_file, tolerance=3):
    """Compare emulator and reference frames."""
    if not os.path.exists(emulator_file):
        return f"[SKIP] {emulator_file} not found"

    if not os.path.exists(reference_file):
        return f"[SKIP] {reference_file} not found"

    # Load and normalize to same size (reference may be taller)
    emu = Image.open(emulator_file).convert('RGB')
    ref = Image.open(reference_file).convert('RGB')

    # Crop reference to emulator size if needed
    if ref.size != emu.size:
        ref = ref.crop((0, 0, emu.size[0], emu.size[1]))

    emu_arr = np.array(emu)
    ref_arr = np.array(ref)

    # Count pixel-by-pixel matches (with tolerance)
    matches = 0
    for i in range(emu_arr.shape[0]):
        for j in range(emu_arr.shape[1]):
            emu_px = emu_arr[i,j]
            ref_px = ref_arr[i,j]
            if np.all(np.abs(emu_px.astype(int) - ref_px.astype(int)) <= tolerance):
                matches += 1

    total = emu_arr.shape[0] * emu_arr.shape[1]
    match_pct = 100.0 * matches / total

    return match_pct

# Test specific frames
tests = [
    ('frame_f1845.bmp', 'tests/snes9x_ref/color_red.png', 'RED'),
    ('frame_f1900.bmp', 'tests/snes9x_ref/color_green.png', 'GREEN'),
    ('frame_f2050.bmp', 'tests/snes9x_ref/color_bar.png', 'COLOR_BAR'),
]

print("Frame Comparison Results:")
print("-" * 60)

for emu_file, ref_file, name in tests:
    result = compare_frames(emu_file, ref_file, tolerance=0)

    if isinstance(result, str):
        print(f"{name}: {result}")
    else:
        status = "[PASS]" if result >= 95.0 else "[FAIL]"
        print(f"{name}: {status} {result:.1f}% match")

        # Show color breakdown
        emu_colors = analyze_frame(emu_file)
        if emu_colors:
            print(f"  Top colors:")
            for color, count in emu_colors.most_common(3):
                pct = 100.0 * count / (256 * 224)
                print(f"    {color}: {pct:.1f}%")

