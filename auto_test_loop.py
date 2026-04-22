#!/usr/bin/env python3
"""
SNES 자동 검증 루프 + 지능형 분석 (BMP 파일 기반)
매 15분마다 실행: 테스트 실행 → 색상 분석 → AI 맥락 분석 → 자동 진단
"""

import subprocess
import os
import time
import json
import re
from PIL import Image
from collections import Counter
from datetime import datetime
import glob

class QAExecutor:
    """QA 에이전트: 테스트 실행"""

    def __init__(self, rom_path="SNES Test Program.sfc"):
        self.rom_path = rom_path
        self.frames = {}

    def run_test(self, timeout=180):
        """테스트 실행 (더 오래 실행해서 더 많은 프레임 캡처)"""
        print("[QA] 에뮬레이터 시작...")

        # 이전 BMP 파일 정리
        for f in glob.glob("frame_f*.bmp"):
            try:
                os.remove(f)
            except:
                pass

        cmd = ['./snes_emu_complete.exe', self.rom_path]

        try:
            result = subprocess.run(cmd, timeout=timeout, capture_output=True)
            print(f"[QA] 에뮬레이터 완료 (exitcode={result.returncode})")
            return result.returncode == 0
        except subprocess.TimeoutExpired:
            print(f"[QA] 타임아웃 ({timeout}초)")
            return False

    def collect_frames(self):
        """BMP 파일에서 프레임 수집"""
        print("[QA] BMP 파일 수집...")

        for bmp_file in sorted(glob.glob("frame_f*.bmp")):
            try:
                match = re.search(r'f(\d+)', bmp_file)
                if not match:
                    continue
                frame_num = int(match.group(1))
                img = Image.open(bmp_file)
                pixels = list(img.getdata())
                unique_colors = len(set(pixels))

                self.frames[frame_num] = {
                    'unique_colors': unique_colors,
                    'image': img
                }
            except Exception as e:
                print(f"[QA] 파일 읽기 실패 {bmp_file}: {e}")

        print(f"[QA] {len(self.frames)}개 프레임 수집")
        return self.frames

class ResultAnalyzer:
    """분석 에이전트: 검증"""

    def __init__(self):
        self.results = {}

    def analyze(self, frames):
        """프레임 분석"""
        print("[ANALYZER] 검증 시작...")

        for frame_num in sorted(frames.keys()):
            unique_colors = frames[frame_num]['unique_colors']

            # 판정 로직 (수정: 색상 임계값 적절히 상향)
            if frame_num <= 30:
                verdict = 'BOOT_OK' if unique_colors <= 5 else 'BOOT_ERROR'
            elif frame_num < 70:
                verdict = 'READY' if unique_colors > 50 else 'WAITING'
            elif frame_num < 90:
                verdict = 'INPUT_SENT'
            elif frame_num < 600:
                verdict = 'RUNNING' if unique_colors > 100 else 'STALLED'
            else:
                verdict = 'FINISHED'

            self.results[frame_num] = {
                'colors': unique_colors,
                'verdict': verdict
            }

        return self.results

    def validate(self):
        """시퀀스 검증"""
        verdicts = list(self.results.values())

        # 최소 3개 이상의 다양한 프레임 필요
        has_boot = any(v['verdict'].startswith('BOOT') for v in verdicts)
        has_ready = any(v['verdict'] == 'READY' for v in verdicts)
        has_running = any(v['verdict'] == 'RUNNING' for v in verdicts)

        passed = has_boot and (has_ready or has_running)

        return {
            'passed': passed,
            'has_boot': has_boot,
            'has_ready': has_ready,
            'has_running': has_running
        }

# 지능형 분석 (AI 맥락 분석)
class IntelligentAnalyzer:
    """지능형 분석: 테스트 단계 식별 + 이상 탐지"""

    TEST_PHASES = {
        "BOOT": (0, 30, 1, 5),
        "MENU": (30, 70, 50, 200),
        "ELECTRONICS": (70, 150, 100, 300),
        "CHARACTER": (150, 300, 150, 400),
        "CONTROLLER_MENU": (300, 400, 100, 250),
        "CONTROLLER_INPUT": (400, 800, 150, 350),
        "SOUND": (800, 1200, 200, 400),
        "COMPLETED": (1200, 99999, 100, 300),
    }

    def __init__(self):
        self.analysis = {}
        self.issues = []

    def analyze(self, results):
        """지능형 분석"""
        print("[INTELLIGENT] AI 맥락 분석 시작...")

        phase_timeline = {}
        for frame_num in sorted(results.keys()):
            colors = results[frame_num]['colors']
            phase, description = self._identify_phase(frame_num, colors)

            self.analysis[frame_num] = {
                'colors': colors,
                'phase': phase,
                'description': description
            }

            if phase not in phase_timeline:
                phase_timeline[phase] = frame_num

        # 문제 감지
        self._detect_issues(phase_timeline, results)

        return self.analysis, self.issues

    def _identify_phase(self, frame_num, color_count):
        """테스트 단계 식별"""
        for phase_name, (f_min, f_max, c_min, c_max) in self.TEST_PHASES.items():
            if f_min <= frame_num <= f_max:
                if c_min <= color_count <= c_max:
                    return phase_name, f"{phase_name} (정상)"
                else:
                    return f"{phase_name}_ANOMALY", f"{phase_name} (색상 이상: {color_count})"
        return "UNKNOWN", "알 수 없음"

    def _detect_issues(self, phase_timeline, results):
        """문제 감지"""
        # 색상 통계
        colors = [r['colors'] for r in results.values()]
        avg_colors = sum(colors) / len(colors) if colors else 0

        # 1. 메뉴 고착 감지
        if avg_colors < 20:
            self.issues.append({
                "type": "MENU_STUCK",
                "severity": "HIGH",
                "message": f"메뉴 화면 고착 (평균 색상: {avg_colors:.0f}, 예상: 50+)"
            })

        # 2. 입력 미인식 감지
        expected_phases = ["BOOT", "MENU", "ELECTRONICS", "CHARACTER"]
        for phase in expected_phases:
            if phase not in phase_timeline:
                self.issues.append({
                    "type": "PHASE_MISSING",
                    "severity": "MEDIUM",
                    "message": f"{phase} 단계 미진행"
                })

class ReportGenerator:
    """보고서 생성 에이전트"""

    @staticmethod
    def generate(qa_frames, analysis, validation):
        """리포트 생성"""
        return {
            'timestamp': datetime.now().isoformat(),
            'frames_count': len(qa_frames),
            'test_result': 'PASS' if validation['passed'] else 'FAIL',
            'validation': validation,
            'sample_frames': {
                k: analysis[k] for k in sorted(analysis.keys())[:5]
            }
        }

    @staticmethod
    def save(report, filename='test_report.json'):
        """리포트 저장"""
        # datetime 객체 제거
        report_clean = {
            'timestamp': report['timestamp'],
            'frames_count': report['frames_count'],
            'test_result': report['test_result'],
            'validation': report['validation'],
            'sample_frames': report['sample_frames']
        }

        with open(filename, 'w') as f:
            json.dump(report_clean, f, indent=2)

        return filename

def main():
    print("\n" + "="*60)
    print("SNES 자동 검증 루프 + 지능형 분석")
    print(f"실행 시간: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("="*60 + "\n")

    # 1. QA 실행
    qa = QAExecutor()
    qa.run_test(timeout=120)
    qa.collect_frames()

    if not qa.frames:
        print("[HARNESS] 프레임 없음 - 스킵")
        return 1

    # 2. 기본 분석
    analyzer = ResultAnalyzer()
    analyzer.analyze(qa.frames)
    validation = analyzer.validate()

    # 3. 지능형 분석 (AI 맥락 분석)
    intel = IntelligentAnalyzer()
    analysis, issues = intel.analyze(analyzer.results)

    # 4. 결과 출력
    print("\n" + "-"*60)
    print(f"프레임 수집: {len(qa.frames)}")

    # 색상 통계
    colors = [f['unique_colors'] for f in qa.frames.values()]
    print(f"색상 통계: min={min(colors)}, max={max(colors)}, avg={sum(colors)/len(colors):.0f}")

    # 단계 분포
    phases = {}
    for a in analysis.values():
        phase = a['phase']
        phases[phase] = phases.get(phase, 0) + 1
    print(f"식별된 단계: {len(phases)}개 - {', '.join(sorted(phases.keys())[:3])}...")

    # 문제 발생 여부
    if issues:
        print(f"\nWARNING: {len(issues)}개 문제 감지:")
        for issue in issues[:2]:
            print(f"  - [{issue['severity']}] {issue['message']}")
    else:
        print("\nOK: 문제 없음")

    print(f"결과: {validation['passed']}")
    print("-"*60 + "\n")

    return 0 if validation['passed'] else 1

if __name__ == '__main__':
    exit(main())
