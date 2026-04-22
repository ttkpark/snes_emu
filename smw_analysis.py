#!/usr/bin/env python3
"""
SMW Rendering Quality Analyzer
Generates frames at key game milestones and analyzes rendering quality
"""
import subprocess
import struct
import os
from pathlib import Path
from collections import Counter
from dataclasses import dataclass

@dataclass
class FrameInfo:
    frame_num: int
    unique_colors: int
    is_mostly_blank: bool
    dominant_color: tuple
    dominant_pct: float

def analyze_bmp(filepath: str) -> FrameInfo:
    """Analyze a single BMP frame"""
    with open(filepath, 'rb') as f:
        # BMP header = 54 bytes, then pixel data (BGR format)
        f.seek(54)
        pixels = f.read()

    color_freq = Counter()
    for i in range(0, len(pixels), 3):
        if i + 2 < len(pixels):
            color = (pixels[i+2], pixels[i+1], pixels[i])
            color_freq[color] += 1

    total_pixels = len(pixels) // 3
    unique_colors = len(color_freq)

    if color_freq:
        dominant_color, dominant_count = color_freq.most_common(1)[0]
        dominant_pct = (dominant_count / total_pixels) * 100
    else:
        dominant_color = (0, 0, 0)
        dominant_pct = 0

    # Frame is mostly blank if > 95% one color
    is_mostly_blank = dominant_pct > 95

    frame_num = int(Path(filepath).stem.split('_f')[1])
    return FrameInfo(frame_num, unique_colors, is_mostly_blank, dominant_color, dominant_pct)

def generate_smw_frames(target_frames=None):
    """Generate SMW frames at key intervals"""
    if target_frames is None:
        # Key game milestones:
        # 0-300: Title fade-in
        # 300-600: Logo display
        # 600-1200: Logo fade-out
        # 1200+: Gameplay
        target_frames = [30, 100, 300, 450, 600, 900, 1200, 1500, 1800, 2100, 2400]

    # Build a modified main that captures frames at specific intervals
    frame_capture_code = """
    // In main loop, add frame capture:
    if (frameCount == 30 || frameCount == 100 || frameCount == 300 || frameCount == 450 ||
        frameCount == 600 || frameCount == 900 || frameCount == 1200 || frameCount == 1500 ||
        frameCount == 1800 || frameCount == 2100 || frameCount == 2400) {
        char filename[64];
        snprintf(filename, sizeof(filename), "frame_f%04d.bmp", frameCount);
        ppu.captureFrameBMP(filename);
    }
    """

    print("To generate SMW frames at key intervals:")
    print(frame_capture_code)
    print()
    print("Key SMW timeline:")
    print("  0-500:    Fade in from white, logo appears")
    print("  500-700:  Logo display (pinkish Nintendo logo)")
    print("  700-1200: Logo fade to black")
    print("  1200+:    Gameplay (blue/cyan sky, terrain)")
    print()

def analyze_smw_rendering():
    """Analyze available SMW frames"""
    frames_dir = Path('.')
    frame_files = sorted(frames_dir.glob('frame_f*.bmp'),
                        key=lambda x: int(x.stem.split('_f')[1]))

    if not frame_files:
        print("No frame files found. Run: ./snes_emu_complete.exe \"Super Mario World (Europe) (Rev 1).sfc\"")
        return

    print("SMW Rendering Analysis")
    print("=" * 90)
    print(f"{'Frame':<8} {'Colors':<10} {'Dominant':<20} {'Content':<40}")
    print("-" * 90)

    blank_frames = []
    content_frames = []

    for frame_file in frame_files[:50]:  # Analyze first 50 frames
        try:
            info = analyze_bmp(str(frame_file))
            r, g, b = info.dominant_color
            dom_desc = f"RGB({r},{g},{b})"

            if info.is_mostly_blank:
                if (r, g, b) == (255, 255, 255):
                    content = "WHITE FADE-IN"
                    blank_frames.append(("white", info.frame_num))
                elif (r, g, b) == (0, 0, 0):
                    content = "BLACK FADE/TRANSITION"
                    blank_frames.append(("black", info.frame_num))
                else:
                    content = "SOLID COLOR"
                    blank_frames.append(("other", info.frame_num))
            else:
                content = f"{info.unique_colors} colors → Content"
                content_frames.append(info.frame_num)

            print(f"f{info.frame_num:<7} {info.unique_colors:<10} {dom_desc:<20} {content:<40}")
        except Exception as e:
            print(f"Error analyzing {frame_file}: {e}")

    print()
    print("=" * 90)
    print("RENDERING SUMMARY")
    print("-" * 90)

    if blank_frames:
        white_frames = [f for t, f in blank_frames if t == "white"]
        black_frames = [f for t, f in blank_frames if t == "black"]

        if white_frames:
            print(f"* White fade-in detected: frames {min(white_frames)}-{max(white_frames)}")
        if black_frames:
            print(f"* Black transition detected: frames {min(black_frames)}-{max(black_frames)}")

    if content_frames:
        print(f"* Game content starts at frame {min(content_frames)}")
        print(f"✓ Content frames captured: {len(content_frames)}")
        print()
        print("Expected SMW content by frame range:")
        print(f"  450-550:  Logo/intro graphics (magenta/pink tones with blues)")
        print(f"  1800+:    Level gameplay (cyan sky, brown/green terrain)")

if __name__ == "__main__":
    analyze_smw_rendering()
