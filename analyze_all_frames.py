#!/usr/bin/env python3
"""Comprehensive analysis of all test frames."""

from PIL import Image
import numpy as np
from collections import Counter
import os
import glob

def analyze_frame_colors(filename):
    """Get top colors and percentages."""
    if not os.path.exists(filename):
        return None

    try:
        img = Image.open(filename).convert('RGB')
        arr = np.array(img)
        pixels = arr.reshape(-1, 3)

        color_counts = Counter(map(tuple, pixels))
        return color_counts
    except:
        return None

# Check what frames exist
frames = sorted(glob.glob('frame_f*.bmp'))
print(f"Found {len(frames)} generated frames")
print("-" * 70)

# Analyze key test frames
key_tests = {
    'frame_f1845.bmp': ('RED', 'tests/snes9x_ref/color_red.png', (255, 0, 0)),
    'frame_f1900.bmp': ('GREEN', 'tests/snes9x_ref/color_green.png', (0, 255, 0)),
    'frame_f2050.bmp': ('COLOR_BAR', 'tests/snes9x_ref/color_bar.png', None),
}

for emu_file, (name, ref_file, expected_color) in key_tests.items():
    print(f"\n{name}:")
    colors = analyze_frame_colors(emu_file)

    if colors is None:
        print(f"  NOT FOUND")
        continue

    print(f"  Total unique colors: {len(colors)}")
    print(f"  Top 5 colors:")
    for color, count in colors.most_common(5):
        pct = 100.0 * count / (256 * 224)
        print(f"    {color}: {pct:.1f}%")

    # Compare to reference if it exists
    if os.path.exists(ref_file):
        ref_colors = analyze_frame_colors(ref_file)
        # Crop reference to top 224 pixels
        img = Image.open(ref_file)
        ref_crop = img.crop((0, 0, 256, 224)).convert('RGB')
        ref_arr = np.array(ref_crop)
        ref_pixels = ref_arr.reshape(-1, 3)
        ref_color_counts = Counter(map(tuple, ref_pixels))

        print(f"  Reference top 5:")
        for color, count in ref_color_counts.most_common(5):
            pct = 100.0 * count / (256 * 224)
            print(f"    {color}: {pct:.1f}%")

print("\n" + "-" * 70)
print(f"Total frames generated: {len(frames)}")
print(f"Range: {frames[0] if frames else 'none'} to {frames[-1] if frames else 'none'}")

