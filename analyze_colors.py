#!/usr/bin/env python3
"""Analyze colors in reference vs generated frames"""

import os
from PIL import Image
from collections import Counter
import struct

def analyze_png(png_path):
    """Analyze color distribution in PNG"""
    try:
        img = Image.open(png_path)
        img = img.convert('RGB')
        pixels = list(img.getdata())

        # Count unique colors and their frequencies
        color_counts = Counter(pixels)
        total_pixels = len(pixels)

        print(f"\n=== {os.path.basename(png_path)} ===")
        print(f"Total pixels: {total_pixels}")
        print(f"Unique colors: {len(color_counts)}")
        print("\nTop 20 colors (RGB, %):")

        for color, count in color_counts.most_common(20):
            pct = (count / total_pixels) * 100
            r, g, b = color
            hex_color = f"#{r:02X}{g:02X}{b:02X}"
            print(f"  {hex_color} ({r},{g},{b}) - {pct:5.2f}%")

        return color_counts
    except Exception as e:
        print(f"Error analyzing {png_path}: {e}")
        return None

def analyze_bmp(bmp_path):
    """Analyze color distribution in BMP"""
    try:
        img = Image.open(bmp_path)
        img = img.convert('RGB')
        pixels = list(img.getdata())

        # Count unique colors and their frequencies
        color_counts = Counter(pixels)
        total_pixels = len(pixels)

        print(f"\n=== {os.path.basename(bmp_path)} ===")
        print(f"Total pixels: {total_pixels}")
        print(f"Unique colors: {len(color_counts)}")
        print("\nTop 20 colors (RGB, %):")

        for color, count in color_counts.most_common(20):
            pct = (count / total_pixels) * 100
            r, g, b = color
            hex_color = f"#{r:02X}{g:02X}{b:02X}"
            print(f"  {hex_color} ({r},{g},{b}) - {pct:5.2f}%")

        return color_counts
    except Exception as e:
        print(f"Error analyzing {bmp_path}: {e}")
        return None

# Analyze reference princess image
ref_princess = "tests/snes9x_ref/chartest_phase2_princess.png"
if os.path.exists(ref_princess):
    print("\n" + "="*60)
    print("REFERENCE PRINCESS (Snes9x)")
    print("="*60)
    ref_colors = analyze_png(ref_princess)

# Analyze generated frames
print("\n\n" + "="*60)
print("GENERATED FRAMES")
print("="*60)

# Check frames that might be princess test
for frame_num in [450, 540, 580, 900, 1000, 2200, 2500, 2600, 2700, 2800, 2900, 3000]:
    bmp_path = f"frame_f{frame_num:04d}.bmp"
    if os.path.exists(bmp_path):
        analyze_bmp(bmp_path)
