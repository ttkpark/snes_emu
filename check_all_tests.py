#!/usr/bin/env python3
"""Check all SNES Test Program color distributions against references"""

from PIL import Image
from collections import Counter
import os

def analyze_frame(bmp_path, ref_path=None):
    """Analyze a generated frame and optionally compare to reference"""
    if not os.path.exists(bmp_path):
        return None

    img = Image.open(bmp_path).convert('RGB')
    pixels = list(img.getdata())
    counts = Counter(pixels)
    total = len(pixels)

    result = {
        'path': bmp_path,
        'total': total,
        'colors': counts,
        'top_color': max(counts.items(), key=lambda x: x[1])[0],
        'top_pct': max(counts.values()) / total * 100
    }

    if ref_path and os.path.exists(ref_path):
        ref = Image.open(ref_path).convert('RGB')
        ref_pixels = list(ref.getdata())
        ref_counts = Counter(ref_pixels)
        ref_top = max(ref_counts.items(), key=lambda x: x[1])[0]
        ref_pct = max(ref_counts.values()) / len(ref_pixels) * 100

        result['ref_color'] = ref_top
        result['ref_pct'] = ref_pct
        result['error'] = abs(result['top_pct'] - ref_pct)

    return result

# Test cases: (frame_name, frame_path, ref_path, test_name)
tests = [
    ("WHITE", "frame_f0030.bmp", "tests/snes9x_ref/color_white.png", "WHITE test"),
    ("RED", "frame_f1845.bmp", "tests/snes9x_ref/color_red.png", "RED test"),
    ("GREEN", "frame_f1900.bmp", "tests/snes9x_ref/color_green.png", "GREEN test"),
    ("PRINCESS", "frame_f0450.bmp", "tests/snes9x_ref/chartest_phase2_princess.png", "PRINCESS test"),
    ("COLORBAR", "frame_f2500.bmp", "tests/snes9x_ref/color_bar.png", "COLOR_BAR test"),
    ("MARIO", "frame_f2700.bmp", None, "MARIO BG test"),
]

print("="*80)
print("SNES Test Program Color Distribution Analysis")
print("="*80)

all_match = True
for name, frame_path, ref_path, desc in tests:
    result = analyze_frame(frame_path, ref_path)
    if result is None:
        print(f"\n{name}: WAITING FOR FRAME")
        all_match = False
        continue

    r, g, b = result['top_color']
    print(f"\n{name} ({desc}):")
    print(f"  Generated: {result['top_pct']:6.2f}% ({r},{g},{b})")

    if 'ref_pct' in result:
        r, g, b = result['ref_color']
        print(f"  Reference: {result['ref_pct']:6.2f}% ({r},{g},{b})")
        error = result['error']
        match = "PASS" if error < 0.5 else "FAIL" if error > 1.0 else "CLOSE"
        print(f"  Error: {error:.2f}% [{match}]")
        if error > 0.5:
            all_match = False

print("\n" + "="*80)
if all_match:
    print("ALL TESTS PASS")
else:
    print("SOME TESTS NEED ADJUSTMENT")
print("="*80)
