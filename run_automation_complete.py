#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SNES Test Program - 완전 자동화 (에뮬레이터 자동 실행 포함)

1. SNES 에뮬레이터 자동 실행
2. 잠시 대기 (창 로드)
3. 자동화 테스트 실행
"""

import sys
import time
import subprocess
from pathlib import Path

try:
    import pyautogui
except ImportError:
    print("❌ pyautogui 필요: pip install pyautogui")
    sys.exit(1)

from snes_test_automation import TestSequenceBuilder

def launch_emulator():
    """에뮬레이터 실행"""
    print("\n[1/4] SNES 에뮬레이터 실행 중...\n")

    rom_path = "SNES Test Program.sfc"
    exe_path = "snes_emu_complete.exe"

    if not Path(exe_path).exists():
        print(f"❌ 에뮬레이터 없음: {exe_path}")
        return False

    if not Path(rom_path).exists():
        print(f"❌ ROM 없음: {rom_path}")
        return False

    try:
        # 에뮬레이터 실행 (백그라운드)
        subprocess.Popen([exe_path, rom_path])
        print(f"✅ 에뮬레이터 시작: {exe_path} {rom_path}")

        # 창이 나타날 때까지 대기
        print("⏳ 에뮬레이터 로드 중...", end='')
        for i in range(5):
            print(".", end='', flush=True)
            time.sleep(1)
        print(" ✅\n")

        return True

    except Exception as e:
        print(f"❌ 에뮬레이터 실행 실패: {e}")
        return False

def focus_window():
    """에뮬레이터 창 포커스"""
    print("[2/4] 에뮬레이터 창 포커스 중...\n")

    try:
        import win32gui
        import win32con

        # SNES 에뮬레이터 창 찾기
        hwnd = win32gui.FindWindow(None, "SNES Emulator")

        if hwnd:
            # 창을 맨 앞으로
            win32gui.SetForegroundWindow(hwnd)
            print("✅ 에뮬레이터 창 포커스 완료\n")
            return True
        else:
            print("⚠️  에뮬레이터 창을 찾을 수 없음 (계속 진행합니다)\n")
            return False

    except ImportError:
        print("⚠️  win32gui 설치 필요: pip install pywin32")
        print("   (계속 진행합니다)\n")
        return False
    except Exception as e:
        print(f"⚠️  포커스 설정 실패: {e} (계속 진행합니다)\n")
        return False

def run_automation():
    """자동화 실행"""
    print("[3/4] 테스트 시퀀스 로드 중...")

    root_dir = Path.cwd()
    screenshot_dir = root_dir / "tests_src"

    builder = TestSequenceBuilder(screenshot_dir)
    steps = builder.build_sequence()
    print(f"✅ {len(steps)}개 단계 로드됨\n")

    # 시작 전 마지막 준비
    print("[4/4] 자동화 실행 준비")
    print("\n🔴 에뮬레이터 창이 활성화되어 있는지 확인하세요!")
    print("⏰ 5초 후 버튼 입력을 시작합니다...\n")

    for i in range(5, 0, -1):
        print(f"   시작까지: {i}초...", end='\r')
        time.sleep(1)

    print("\n" + "="*70)
    print("▶️  자동화 실행 시작!")
    print("="*70 + "\n")

    # 자동화 실행
    step_count = 0
    button_count = 0

    try:
        for i, step in enumerate(steps, 1):
            step_count += 1

            if step.button_input:
                button_count += 1
                buttons_str = ", ".join(step.button_input.buttons)
                print(f"[{i:2d}/{len(steps)}] {step.test_name:20s} | 버튼: {buttons_str:30s}", end='')

                # 버튼 입력
                for btn in step.button_input.buttons:
                    pyautogui.press(btn)
                    time.sleep(0.05)

                time.sleep(step.button_input.duration)
                time.sleep(step.button_input.release_wait)
                print(" ✅")
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
    print("🎮 SNES TEST PROGRAM - 완전 자동화")
    print("="*70)

    # 1. 에뮬레이터 실행
    if not launch_emulator():
        print("❌ 에뮬레이터 실행 실패")
        return 1

    # 2. 창 포커스
    focus_window()

    # 3. 자동화 실행
    if run_automation():
        return 0
    else:
        return 1

if __name__ == '__main__':
    sys.exit(main())
