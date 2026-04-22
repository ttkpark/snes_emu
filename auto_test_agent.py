#!/usr/bin/env python3
"""
SNES Emulator Automated Test Agent (Main Loop Entry)
지능형 자동 검증 루프 + BMP 기반 분석
"""

import subprocess
import os
import json
import re
import time
from pathlib import Path
from PIL import Image
from datetime import datetime
from collections import Counter
import glob

TEST_PHASES = {
    "BOOT": {"frames": (0, 30), "expected_colors": (1, 5), "description": "부팅 중"},
    "MENU": {"frames": (30, 70), "expected_colors": (50, 200), "description": "메인 메뉴"},
    "ELECTRONICS": {"frames": (70, 150), "expected_colors": (100, 300), "description": "Electronics Test"},
    "CHARACTER": {"frames": (150, 300), "expected_colors": (150, 400), "description": "Character Test"},
    "CONTROLLER_MENU": {"frames": (300, 400), "expected_colors": (100, 250), "description": "Controller Menu"},
    "CONTROLLER_INPUT": {"frames": (400, 800), "expected_colors": (150, 350), "description": "Controller Input"},
    "SOUND": {"frames": (800, 1200), "expected_colors": (200, 400), "description": "Sound Test"},
    "COMPLETED": {"frames": (1200, 99999), "expected_colors": (100, 300), "description": "완료"},
}

class QAExecutor:
    """QA: 에뮬레이터 실행 + BMP 수집"""
    def __init__(self, rom_path="SNES Test Program.sfc"):
        self.rom_path = rom_path
        self.frames = {}

    def run_test(self, timeout=120):
        print("[QA] 에뮬레이터 시작...")
        for f in glob.glob("frame_f*.bmp"):
            try:
                os.remove(f)
            except:
                pass

        cmd = ['./snes_emu_complete.exe', self.rom_path]
        try:
            result = subprocess.run(cmd, timeout=timeout, capture_output=True)
            print(f"[QA] 완료 (code={result.returncode})")
            return result.returncode == 0
        except subprocess.TimeoutExpired:
            print(f"[QA] 타임아웃 ({timeout}초)")
            return False

    def collect_frames(self):
        print("[QA] 프레임 수집...")
        for bmp_file in sorted(glob.glob("frame_f*.bmp")):
            try:
                match = re.search(r'f(\d+)', bmp_file)
                if not match:
                    continue
                frame_num = int(match.group(1))
                img = Image.open(bmp_file)
                pixels = list(img.getdata())
                unique_colors = len(set(pixels))
                self.frames[frame_num] = {'colors': unique_colors, 'image': img, 'path': bmp_file}
            except Exception as e:
                print(f"[QA] 읽기 실패 {bmp_file}: {e}")

        print(f"[QA] {len(self.frames)}개 프레임")
        return self.frames

class Analyzer:
    """분석: 단계 식별 + 이상 탐지"""
    def __init__(self):
        self.analysis = {}
        self.issues = []

    def identify_phase(self, frame_num, color_count):
        for phase_name, phase_info in TEST_PHASES.items():
            f_min, f_max = phase_info["frames"]
            c_min, c_max = phase_info["expected_colors"]
            if f_min <= frame_num <= f_max:
                if c_min <= color_count <= c_max:
                    return phase_name, phase_info["description"]
                else:
                    return phase_name + "_ANOMALY", f"{phase_info['description']} (색상: {color_count})"
        return "UNKNOWN", "알 수 없음"

    def analyze(self, frames):
        print("[ANALYZER] 분석 시작...")
        phase_timeline = {}
        anomalies = []

        for frame_num in sorted(frames.keys()):
            color_count = frames[frame_num]['colors']
            phase, desc = self.identify_phase(frame_num, color_count)
            self.analysis[frame_num] = {'colors': color_count, 'phase': phase, 'description': desc}

            if phase not in phase_timeline:
                phase_timeline[phase] = frame_num
            if "ANOMALY" in phase:
                anomalies.append((frame_num, phase, color_count))

        self._detect_issues(phase_timeline, anomalies)
        print(f"[ANALYZER] 완료: {len(phase_timeline)}개 단계")
        return self.analysis, phase_timeline, anomalies

    def _detect_issues(self, phase_timeline, anomalies):
        if "CONTROLLER_INPUT" not in phase_timeline and "CONTROLLER_MENU" in phase_timeline:
            self.issues.append({
                "type": "INPUT_NOT_RECOGNIZED",
                "severity": "HIGH",
                "message": "입력 미인식"
            })

        for frame_num, phase, colors in anomalies:
            self.issues.append({
                "type": "COLOR_ANOMALY",
                "severity": "MEDIUM",
                "frame": frame_num,
                "colors": colors,
                "message": f"F:{frame_num} {phase} (색상: {colors})"
            })

        expected = ["BOOT", "MENU", "ELECTRONICS", "CHARACTER", "CONTROLLER_MENU"]
        for phase in expected:
            if phase not in phase_timeline and "COMPLETED" not in phase_timeline:
                self.issues.append({
                    "type": "PHASE_MISSING",
                    "severity": "MEDIUM",
                    "phase": phase,
                    "message": f"{phase} 미진행"
                })

        if self.issues:
            print(f"[ANALYZER] WARNING: {len(self.issues)}개 문제")
        else:
            print("[ANALYZER] OK")

class Reporter:
    """리포트: 자동 생성 및 저장"""
    @staticmethod
    def generate(frames, analysis, issues):
        return {
            "timestamp": datetime.now().isoformat(),
            "frames_count": len(frames),
            "phases_detected": len(set(a['phase'] for a in analysis.values())),
            "test_result": "FAIL" if issues else "PASS",
            "analysis_summary": {
                "phase_distribution": dict(Counter(a['phase'] for a in analysis.values())),
                "color_stats": {
                    "min": min(f['colors'] for f in frames.values()) if frames else 0,
                    "max": max(f['colors'] for f in frames.values()) if frames else 0,
                    "avg": sum(f['colors'] for f in frames.values()) / len(frames) if frames else 0
                }
            },
            "issues": issues,
            "sample_frames": {str(k): analysis[k] for k in sorted(analysis.keys())[:5]}
        }

    @staticmethod
    def save(report, filename='test_report_agent.json'):
        with open(filename, 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=2, ensure_ascii=False)
        return filename

def main():
    print("\n" + "="*70)
    print("SNES Automated Test Agent")
    print(f"Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("="*70 + "\n")

    qa = QAExecutor()
    qa.run_test(timeout=120)
    qa.collect_frames()

    if not qa.frames:
        print("[AGENT] 프레임 없음 - 실패")
        return 1

    analyzer = Analyzer()
    analysis, phase_timeline, anomalies = analyzer.analyze(qa.frames)

    report = Reporter.generate(qa.frames, analysis, analyzer.issues)
    report_file = Reporter.save(report)

    print("\n" + "-"*70)
    print(f"Frames: {report['frames_count']}")
    print(f"Phases: {report['phases_detected']}")
    print(f"Colors: min={report['analysis_summary']['color_stats']['min']}, "
          f"max={report['analysis_summary']['color_stats']['max']:.0f}, "
          f"avg={report['analysis_summary']['color_stats']['avg']:.0f}")
    print(f"Result: {report['test_result']}")

    if analyzer.issues:
        print(f"\nWARNING: {len(analyzer.issues)}개 문제")
        for issue in analyzer.issues[:3]:
            print(f"  [{issue['severity']}] {issue['message']}")
    else:
        print("\nOK: 테스트 통과")

    print(f"Report: {report_file}")
    print("-"*70 + "\n")

    return 0 if report['test_result'] == 'PASS' else 1

if __name__ == '__main__':
    exit(main())
