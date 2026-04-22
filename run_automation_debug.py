#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SNES Test Automation - 디버그 버전
버튼 입력 및 에러 처리를 더 자세히 로깅합니다.
"""

import sys
import time
from pathlib import Path

try:
    import pyautogui
    # 안전성 설정
    pyautogui.FAILSAFE = False  # ESC로 종료 불가
    pyautogui.PAUSE = 0.05  # 명령 간 기본 대기
except ImportError:
    print("❌ pyautogui 설치 필요: pip install pyautogui")
    sys.exit(1)

from snes_test_automation import (
    TestSequenceBuilder, ScreenshotParser
)

def main():
    root_dir = Path(__file__).parent
    screenshot_dir = root_dir / "tests_src"

    print("\n" + "="*70)
    print("🎮 SNES TEST PROGRAM - 디버그 모드 실행")
    print("="*70 + "\n")

    # 테스트 시퀀스 빌드
    print("[1/3] 테스트 시퀀스 파싱 중...")
    builder = TestSequenceBuilder(screenshot_dir)
    steps = builder.build_sequence()
    print(f"  ✅ {len(steps)}개 단계 로드됨\n")

    # pyautogui 상태 확인
    print("[2/3] pyautogui 초기화 중...")
    try:
        # 마우스 위치 확인 (작동하는지 테스트)
        x, y = pyautogui.position()
        print(f"  ✅ 현재 마우스 위치: ({x}, {y})")

        # 키보드 테스트 (음소거된 입력)
        print(f"  ✅ 키보드 입력 준비 완료")
        print(f"  ⚠️  10초 후 실제 입력 시작\n")

        for i in range(10, 0, -1):
            print(f"  시작까지: {i}초...", end='\r')
            time.sleep(1)
        print()

    except Exception as e:
        print(f"  ❌ pyautogui 에러: {e}")
        return 1

    # 자동화 실행
    print("[3/3] 자동화 실행 중...\n")
    print("="*70)

    step_count = 0
    button_count = 0

    try:
        for i, step in enumerate(steps, 1):
            step_count += 1

            # 진행 상황 출력
            if step.button_input:
                button_count += 1
                buttons_str = ", ".join(step.button_input.buttons)
                print(f"[{i:2d}/{len(steps)}] {step.test_name:20s} | 버튼: {buttons_str:30s}", end='')

                # 버튼 입력 수행
                try:
                    for btn in step.button_input.buttons:
                        pyautogui.press(btn)
                        time.sleep(0.05)

                    time.sleep(step.button_input.duration)
                    time.sleep(step.button_input.release_wait)
                    print(" ✅")

                except Exception as e:
                    print(f" ❌ ({e})")
            else:
                print(f"[{i:2d}/{len(steps)}] {step.test_name:20s} | 자동 진행 (2초 대기)... ", end='')
                time.sleep(2)
                print("✅")

        print("="*70)
        print(f"\n✅ 자동화 완료!")
        print(f"  • 총 단계: {step_count}")
        print(f"  • 버튼 입력: {button_count}")
        print(f"  • 자동 진행: {step_count - button_count}")

        return 0

    except KeyboardInterrupt:
        print(f"\n\n❌ 사용자 중단 (단계 {step_count}/{len(steps)})")
        return 1
    except Exception as e:
        print(f"\n\n❌ 에러: {e}")
        import traceback
        traceback.print_exc()
        return 1

if __name__ == '__main__':
    sys.exit(main())
