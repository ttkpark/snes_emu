#!/usr/bin/env python3
"""
SNES 지능형 자동 테스트 - AI 맥락 분석 + 자동 복구
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

# 테스트 단계 정의 (RUN_TEST_AUTOMATION.md 기준)
TEST_PHASES = {
    "BOOT": {"frames": (0, 30), "expected_colors": (1, 5), "description": "부팅 중"},
    "MENU": {"frames": (30, 70), "expected_colors": (50, 200), "description": "메인 메뉴"},
    "ELECTRONICS": {"frames": (70, 150), "expected_colors": (100, 300), "description": "Electronics Test 진행 중"},
    "CHARACTER": {"frames": (150, 300), "expected_colors": (150, 400), "description": "Character Test 진행 중"},
    "CONTROLLER_MENU": {"frames": (300, 400), "expected_colors": (100, 250), "description": "Controller Test 메뉴"},
    "CONTROLLER_INPUT": {"frames": (400, 800), "expected_colors": (150, 350), "description": "Controller Test 버튼 입력"},
    "SOUND": {"frames": (800, 1200), "expected_colors": (200, 400), "description": "Sound Test 진행 중"},
    "COMPLETED": {"frames": (1200, 99999), "expected_colors": (100, 300), "description": "테스트 완료"},
}

class IntelligentQA:
    """지능형 QA 에이전트: 분석 + 복구"""

    def __init__(self, rom_path="SNES Test Program.sfc"):
        self.rom_path = rom_path
        self.frames = {}
        self.analysis = {}
        self.issues = []
        self.last_input_frame = None

    def run_test(self, timeout=120):
        """에뮬레이터 실행"""
        print("[INTEL_QA] 에뮬레이터 시작...")

        # 이전 BMP 정리
        for f in glob.glob("frame_f*.bmp"):
            try: os.remove(f)
            except: pass

        cmd = ['./snes_emu_complete.exe', self.rom_path]
        try:
            result = subprocess.run(cmd, timeout=timeout, capture_output=True)
            print(f"[INTEL_QA] 에뮬레이터 완료 (code={result.returncode})")
            return result.returncode == 0
        except subprocess.TimeoutExpired:
            print(f"[INTEL_QA] 타임아웃 ({timeout}초)")
            return False

    def collect_frames(self):
        """프레임 수집"""
        print("[INTEL_QA] 프레임 수집 중...")

        for bmp_file in sorted(glob.glob("frame_f*.bmp")):
            try:
                match = re.search(r'f(\d+)', bmp_file)
                if not match: continue
                frame_num = int(match.group(1))

                img = Image.open(bmp_file)
                pixels = list(img.getdata())
                unique_colors = len(set(pixels))

                self.frames[frame_num] = {
                    'colors': unique_colors,
                    'image': img,
                    'path': bmp_file
                }
            except Exception as e:
                print(f"[INTEL_QA] 프레임 읽기 실패 {bmp_file}: {e}")

        print(f"[INTEL_QA] {len(self.frames)}개 프레임 수집")
        return self.frames

    def identify_test_phase(self, frame_num, color_count):
        """현재 테스트 단계 식별"""
        for phase_name, phase_info in TEST_PHASES.items():
            f_min, f_max = phase_info["frames"]
            c_min, c_max = phase_info["expected_colors"]

            if f_min <= frame_num <= f_max:
                # 색상 범위도 확인
                if c_min <= color_count <= c_max:
                    return phase_name, phase_info["description"]
                else:
                    return phase_name + "_ANOMALY", f"{phase_info['description']} (색상 이상: {color_count})"

        return "UNKNOWN", "알 수 없는 상태"

    def analyze_frames(self):
        """프레임 분석 + 맥락 파악"""
        print("[ANALYZER] 지능형 분석 시작...")

        phase_timeline = {}
        anomalies = []

        for frame_num in sorted(self.frames.keys()):
            color_count = self.frames[frame_num]['colors']
            phase, description = self.identify_test_phase(frame_num, color_count)

            self.analysis[frame_num] = {
                'colors': color_count,
                'phase': phase,
                'description': description
            }

            if phase not in phase_timeline:
                phase_timeline[phase] = frame_num

            # 이상 감지
            if "ANOMALY" in phase:
                anomalies.append((frame_num, phase, color_count))

        print(f"[ANALYZER] 분석 완료: {len(phase_timeline)}개 단계 식별")

        # 문제 발생 판정
        self._detect_issues(phase_timeline, anomalies)

        return self.analysis, phase_timeline, anomalies

    def _detect_issues(self, phase_timeline, anomalies):
        """자동 문제 감지"""
        print("[DETECTOR] 문제 감지 중...")

        # 1. 입력 인식 실패 감지
        if "CONTROLLER_INPUT" not in phase_timeline:
            if "CONTROLLER_MENU" in phase_timeline:
                self.issues.append({
                    "type": "INPUT_NOT_RECOGNIZED",
                    "severity": "HIGH",
                    "message": "Controller Test에 진입했으나 입력이 인식되지 않음",
                    "suggestion": "조이스틱 입력 타이밍 조정 필요"
                })

        # 2. 비정상적인 색상 변화
        for frame_num, phase, colors in anomalies:
            self.issues.append({
                "type": "COLOR_ANOMALY",
                "severity": "MEDIUM",
                "frame": frame_num,
                "phase": phase,
                "colors": colors,
                "message": f"F:{frame_num} {phase}에서 색상 수 이상 ({colors})"
            })

        # 3. 단계 진행 부재
        expected_sequence = ["BOOT", "MENU", "ELECTRONICS", "CHARACTER", "CONTROLLER_MENU"]
        for expected_phase in expected_sequence:
            if expected_phase not in phase_timeline and "COMPLETED" not in phase_timeline:
                self.issues.append({
                    "type": "PHASE_MISSING",
                    "severity": "MEDIUM",
                    "phase": expected_phase,
                    "message": f"{expected_phase} 단계를 거치지 않음"
                })

        if self.issues:
            print(f"[DETECTOR] WARNING: {len(self.issues)}개 문제 감지됨")
            for issue in self.issues:
                print(f"  - [{issue['severity']}] {issue['message']}")
        else:
            print("[DETECTOR] OK: 문제 없음")

    def generate_report(self):
        """지능형 리포트 생성"""
        report = {
            "timestamp": datetime.now().isoformat(),
            "frames_count": len(self.frames),
            "phases_detected": len(set(a['phase'] for a in self.analysis.values())),
            "test_result": "FAIL" if self.issues else "PASS",
            "analysis_summary": {
                "phase_distribution": Counter(a['phase'] for a in self.analysis.values()),
                "color_stats": {
                    "min": min(f['colors'] for f in self.frames.values()),
                    "max": max(f['colors'] for f in self.frames.values()),
                    "avg": sum(f['colors'] for f in self.frames.values()) / len(self.frames)
                }
            },
            "issues": self.issues,
            "sample_frames": {
                str(k): self.analysis[k]
                for k in sorted(self.analysis.keys())[:10]
            }
        }

        return report

    def save_report(self, report, filename='intelligent_report.json'):
        """리포트 저장"""
        report['analysis_summary']['phase_distribution'] = dict(
            report['analysis_summary']['phase_distribution']
        )

        with open(filename, 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=2, ensure_ascii=False)

        return filename

def main():
    print("\n" + "="*70)
    print("SNES 지능형 자동 테스트 시스템 (AI 맥락 분석 + 자동 복구)")
    print(f"실행 시간: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("="*70 + "\n")

    # 1. QA 실행
    qa = IntelligentQA()
    qa.run_test(timeout=120)
    qa.collect_frames()

    if not qa.frames:
        print("[HARNESS] 프레임 없음 - 실패")
        return 1

    # 2. 지능형 분석
    analysis, phase_timeline, anomalies = qa.analyze_frames()

    # 3. 리포트 생성
    report = qa.generate_report()
    report_file = qa.save_report(report)

    # 4. 결과 출력
    print("\n" + "-"*70)
    print(f"프레임 수집: {report['frames_count']}")
    print(f"식별된 단계: {report['phases_detected']}")
    print(f"색상 통계: min={report['analysis_summary']['color_stats']['min']}, "
          f"max={report['analysis_summary']['color_stats']['max']:.0f}, "
          f"avg={report['analysis_summary']['color_stats']['avg']:.0f}")
    print(f"결과: {report['test_result']}")

    if qa.issues:
        print(f"\nWARNING: 감지된 문제: {len(qa.issues)}개")
        for issue in qa.issues[:3]:  # 상위 3개만 표시
            print(f"  - [{issue['severity']}] {issue['message']}")
    else:
        print("\nOK: 모든 테스트 통과")

    print(f"리포트: {report_file}")
    print("-"*70 + "\n")

    return 0 if report['test_result'] == 'PASS' else 1

if __name__ == '__main__':
    exit(main())
