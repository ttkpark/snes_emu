#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SNES Test Program - 강화된 자동화 (SendInput API 사용)

Windows SendInput API를 직접 사용하여 더 신뢰할 수 있는 키 입력 제공
"""

import sys
import time
import subprocess
from pathlib import Path
import ctypes
from ctypes import wintypes

from snes_test_automation import TestSequenceBuilder

# Windows SendInput API 정의
KEYEVENTF_KEYDOWN = 0x0000
KEYEVENTF_KEYUP = 0x0002

class KeyboardInput(ctypes.Structure):
    _fields_ = [
        ('wVk', wintypes.WORD),
        ('wScan', wintypes.WORD),
        ('dwFlags', wintypes.DWORD),
        ('time', wintypes.DWORD),
        ('dwExtraInfo', ctypes.POINTER(ctypes.c_ulong))
    ]

class InputUnion(ctypes.Union):
    _fields_ = [
        ('ki', KeyboardInput),
    ]

class Input(ctypes.Structure):
    _fields_ = [
        ('type', wintypes.DWORD),
        ('u', InputUnion)
    ]

# 키 코드 매핑
KEY_CODES = {
    'space': 0x20,      # Space
    'return': 0x0D,     # Enter
    'w': 0x57,          # W (UP)
    's': 0x53,          # S (DOWN)
    'a': 0x41,          # A (LEFT)
    'd': 0x44,          # D (RIGHT)
    'z': 0x5A,          # Z (A button)
    'x': 0x58,          # X (B button)
    'c': 0x43,          # C (X button)
    'v': 0x56,          # V (Y button)
}

def send_input(key_code, hold_time=0.05):
    """SendInput API를 사용하여 키 입력 전송"""

    user32 = ctypes.windll.user32

    # 키 누르기 (DOWN)
    ki_down = KeyboardInput(wVk=key_code, wScan=0, dwFlags=KEYEVENTF_KEYDOWN, time=0, dwExtraInfo=None)
    input_down = Input(type=1, u=InputUnion(ki=ki_down))

    user32.SendInput(1, ctypes.byref(input_down), ctypes.sizeof(Input))
    time.sleep(hold_time)

    # 키 떨기 (UP)
    ki_up = KeyboardInput(wVk=key_code, wScan=0, dwFlags=KEYEVENTF_KEYUP, time=0, dwExtraInfo=None)
    input_up = Input(type=1, u=InputUnion(ki=ki_up))

    user32.SendInput(1, ctypes.byref(input_up), ctypes.sizeof(Input))
    time.sleep(hold_time)

def launch_emulator():
    """에뮬레이터 실행"""
    print("\n[1/4] SNES 에뮬레이터 확인 중...\n")

    rom_path = "SNES Test Program.sfc"
    exe_path = "snes_emu_complete.exe"

    if not Path(exe_path).exists():
        print(f"❌ 에뮬레이터 없음: {exe_path}")
        return False

    if not Path(rom_path).exists():
        print(f"❌ ROM 없음: {rom_path}")
        return False

    # 이미 실행 중인지 확인
    import subprocess
    try:
        result = subprocess.run(['tasklist', '/FI', 'IMAGENAME eq snes_emu_complete.exe'],
                              capture_output=True, text=True)
        if 'snes_emu_complete.exe' in result.stdout:
            print("✅ 에뮬레이터가 이미 실행 중입니다")
            print("⏳ 잠시 대기...", end='')
            for i in range(3):
                print(".", end='', flush=True)
                time.sleep(1)
            print(" ✅\n")
            return True
        else:
            # 에뮬레이터 실행
            print(f"✅ 에뮬레이터 시작: {exe_path}")
            subprocess.Popen([exe_path, rom_path])
            print("⏳ 에뮬레이터 로드 중...", end='')
            for i in range(5):
                print(".", end='', flush=True)
                time.sleep(1)
            print(" ✅\n")
            return True
    except Exception as e:
        print(f"⚠️  확인 실패: {e} (계속 진행합니다)\n")
        return True

def run_automation():
    """자동화 실행 (SendInput 사용)"""
    print("[2/4] 테스트 시퀀스 로드 중...")

    root_dir = Path.cwd()
    screenshot_dir = root_dir / "tests_src"

    builder = TestSequenceBuilder(screenshot_dir)
    steps = builder.build_sequence()
    print(f"✅ {len(steps)}개 단계 로드됨\n")

    # 시작 전 준비
    print("[3/4] 자동화 실행 준비")
    print("\n🔴 에뮬레이터 창이 활성화되어 있는지 확인하세요!")
    print("⏰ 3초 후 버튼 입력을 시작합니다...\n")

    for i in range(3, 0, -1):
        print(f"   시작까지: {i}초...", end='\r')
        time.sleep(1)

    print("\n" + "="*70)
    print("▶️  자동화 실행 시작!")
    print("="*70 + "\n")

    # 자동화 실행
    step_count = 0
    button_count = 0
    failed_count = 0

    try:
        for i, step in enumerate(steps, 1):
            step_count += 1

            if step.button_input:
                button_count += 1
                buttons_str = ", ".join(step.button_input.buttons)
                print(f"[{i:2d}/{len(steps)}] {step.test_name:20s} | 버튼: {buttons_str:30s}", end='')

                # 버튼 입력 (SendInput 사용)
                try:
                    for btn in step.button_input.buttons:
                        if btn in KEY_CODES:
                            send_input(KEY_CODES[btn], hold_time=0.05)
                        else:
                            print(f"\n  ⚠️  알 수 없는 버튼: {btn}")
                            failed_count += 1

                    time.sleep(step.button_input.duration)
                    time.sleep(step.button_input.release_wait)
                    print(" ✅")

                except Exception as e:
                    print(f" ❌ ({e})")
                    failed_count += 1
            else:
                print(f"[{i:2d}/{len(steps)}] {step.test_name:20s} | 자동 진행... ", end='')
                time.sleep(2)
                print("✅")

        print("\n" + "="*70)
        print("✅ 자동화 완료!")
        print("="*70)
        print(f"\n📊 통계:")
        print(f"   • 총 단계: {step_count}")
        print(f"   • 버튼 입력: {button_count}")
        print(f"   • 자동 진행: {step_count - button_count}")
        print(f"   • 실패: {failed_count}")
        print()

        return True

    except KeyboardInterrupt:
        print(f"\n\n❌ 사용자 중단 (단계 {step_count}/{len(steps)})")
        return False
    except Exception as e:
        print(f"\n\n❌ 에러: {e}")
        import traceback
        traceback.print_exc()
        return False

def main():
    print("\n" + "="*70)
    print("🎮 SNES TEST PROGRAM - 강화된 자동화 (SendInput API)")
    print("="*70)

    # 에뮬레이터 확인
    if not launch_emulator():
        print("❌ 에뮬레이터 확인 실패")
        return 1

    # 자동화 실행
    if run_automation():
        return 0
    else:
        return 1

if __name__ == '__main__':
    sys.exit(main())
