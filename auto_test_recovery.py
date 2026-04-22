#!/usr/bin/env python3
"""
SNES 자동 복구 루프 - 입력 문제 자동 감지 및 수정
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

class AutoRecovery:
    """자동 복구 에이전트"""

    def __init__(self, rom_path="SNES Test Program.sfc"):
        self.rom_path = rom_path
        self.frames = {}
        self.attempts = 0
        self.max_attempts = 3

    def run_test_with_strategy(self, strategy="standard"):
        """다양한 입력 전략으로 테스트"""
        print(f"\n[RECOVERY] 전략 {self.attempts + 1}: {strategy}")

        # 이전 BMP 정리
        for f in glob.glob("frame_f*.bmp"):
            try: os.remove(f)
            except: pass

        if strategy == "standard":
            # 기본 전략: F:70-90에 START 입력 (0.2초)
            input_commands = [("START", 70, 80, 0.2)]

        elif strategy == "extended":
            # 연장 전략: START 입력을 더 길게 (0.5초)
            input_commands = [("START", 70, 90, 0.5)]

        elif strategy == "delayed":
            # 지연 전략: 더 늦게 입력 (F:100에서)
            input_commands = [("START", 100, 120, 0.3)]

        elif strategy == "multiple":
            # 반복 전략: START를 여러 번
            input_commands = [
                ("START", 70, 75, 0.2),
                ("START", 85, 90, 0.2),
            ]

        elif strategy == "aggressive":
            # 공격적 전략: SELECT + START 조합
            input_commands = [
                ("SELECT", 60, 65, 0.1),
                ("START", 70, 80, 0.3),
            ]

        else:
            input_commands = [("START", 70, 80, 0.2)]

        # 에뮬레이터 실행 + 입력
        print(f"[RECOVERY] 에뮬레이터 시작...")
        cmd = ['./snes_emu_complete.exe', self.rom_path]

        process = subprocess.Popen(
            cmd,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE
        )

        try:
            # 입력 전송 (프레임 기반)
            for button, frame_start, frame_end, duration in input_commands:
                time.sleep(1)  # 에뮬레이터 부팅 대기
                self._send_input_sequence(button, duration)

            # 테스트 완료 대기
            time.sleep(5)
            process.terminate()
            process.wait(timeout=10)

            print(f"[RECOVERY] 에뮬레이터 종료")
            return True

        except Exception as e:
            print(f"[RECOVERY] 에러: {e}")
            process.kill()
            return False

    def _send_input_sequence(self, button, duration):
        """입력 시퀀스 전송"""
        print(f"[RECOVERY] 입력 전송: {button} (지속: {duration}초)")
        # 실제 구현에서는 stdin을 통해 입력 전송
        # 현재는 간단히 sleep으로 시뮬레이션

    def collect_frames(self):
        """프레임 수집"""
        print("[RECOVERY] 프레임 수집 중...")

        for bmp_file in sorted(glob.glob("frame_f*.bmp")):
            try:
                match = re.search(r'f(\d+)', bmp_file)
                if not match: continue
                frame_num = int(match.group(1))

                img = Image.open(bmp_file)
                pixels = list(img.getdata())
                unique_colors = len(set(pixels))

                self.frames[frame_num] = {'colors': unique_colors}
            except:
                pass

        return len(self.frames)

    def diagnose(self):
        """현재 문제 진단"""
        if not self.frames:
            return "NO_FRAMES", "프레임이 생성되지 않음"

        avg_colors = sum(f['colors'] for f in self.frames.values()) / len(self.frames)

        if avg_colors < 20:
            return "MENU_STUCK", f"메뉴 화면 고착 (평균 색상: {avg_colors:.0f})"

        if max(f['colors'] for f in self.frames.values()) > 300:
            return "RUNNING_OK", f"테스트 정상 진행 (평균 색상: {avg_colors:.0f})"

        return "UNKNOWN", f"알 수 없는 상태 (평균 색상: {avg_colors:.0f})"

    def select_next_strategy(self, diagnosis):
        """진단 결과에 따라 다음 전략 선택"""
        strategies = ["standard", "extended", "delayed", "multiple", "aggressive"]

        if self.attempts >= len(strategies):
            return None

        diagnosis_type, message = diagnosis

        if diagnosis_type == "MENU_STUCK":
            # 메뉴 고착 → 입력 강화
            if self.attempts == 0:
                return "extended"  # 입력 시간 연장
            elif self.attempts == 1:
                return "delayed"   # 입력 타이밍 지연
            else:
                return "aggressive"  # 조합 입력

        elif diagnosis_type == "NO_FRAMES":
            # 프레임 없음 → 전혀 다른 접근
            if self.attempts == 0:
                return "extended"
            else:
                return "aggressive"

        # 기본값: 다음 전략
        return strategies[self.attempts % len(strategies)]

def main():
    print("\n" + "="*70)
    print("SNES 자동 복구 루프 (입력 문제 자동 감지 및 해결)")
    print(f"시작 시간: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
    print("="*70)

    recovery = AutoRecovery()

    while recovery.attempts < recovery.max_attempts:
        recovery.attempts += 1

        # 1. 테스트 실행
        strategy = "standard" if recovery.attempts == 1 else recovery.select_next_strategy(None)
        if not strategy:
            break

        success = recovery.run_test_with_strategy(strategy)
        if not success:
            continue

        # 2. 프레임 수집
        frame_count = recovery.collect_frames()
        print(f"[RECOVERY] {frame_count}개 프레임 수집")

        # 3. 진단
        diagnosis = recovery.diagnose()
        diagnosis_type, message = diagnosis
        print(f"[RECOVERY] 진단: {diagnosis_type} - {message}")

        # 4. 성공 판정
        if diagnosis_type == "RUNNING_OK":
            print(f"\n[SUCCESS] 시도 {recovery.attempts}회에 성공!")
            break

        # 5. 복구 전략 선택
        next_strategy = recovery.select_next_strategy(diagnosis)
        if next_strategy:
            print(f"[RECOVERY] 다음 시도 예정: {next_strategy}")
        else:
            print("[RECOVERY] 복구 실패 - 모든 전략 소진")
            break

    print("\n" + "-"*70)
    print(f"최종 결과: {recovery.attempts}회 시도")
    print(f"수집된 총 프레임: {len(recovery.frames)}")
    print("-"*70 + "\n")

if __name__ == '__main__':
    main()
