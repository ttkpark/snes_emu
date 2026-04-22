#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SNES Test Program - 실시간 자동화 실행
실제 버튼 입력을 수행합니다.
"""

import sys
import time
from pathlib import Path

from snes_test_automation import (
    TestSequenceBuilder, AutomationController
)

def main():
    root_dir = Path(__file__).parent
    screenshot_dir = root_dir / "tests_src"

    print("\n" + "="*70)
    print("🎮 SNES TEST PROGRAM - 실시간 자동화 실행 시작!")
    print("="*70)
    print("\n⚠️  주의: 마우스/키보드 자동 제어가 시작됩니다.")
    print("⏰ 5초 후 시작합니다. 창을 포커스하세요...\n")

    for i in range(5, 0, -1):
        print(f"   시작까지 {i}초...", end='\r')
        time.sleep(1)

    print("\n" + "="*70)
    print("▶️  자동화 실행 시작!")
    print("="*70 + "\n")

    # 테스트 시퀀스 빌드
    builder = TestSequenceBuilder(screenshot_dir)
    steps = builder.build_sequence()

    # 자동화 실행
    controller = AutomationController()

    try:
        controller.auto_play_test_sequence(steps, dry_run=False)
        print("\n" + "="*70)
        print("✅ 자동화 완료!")
        print("="*70)
        print("\n📊 테스트 결과:")
        print(f"  • 총 단계: {len(steps)}")
        print(f"  • 버튼 입력: {sum(1 for s in steps if s.button_input)}")
        print(f"  • 자동 진행: {sum(1 for s in steps if not s.button_input)}")

    except KeyboardInterrupt:
        print("\n\n❌ 사용자에 의해 중단됨")
        return 1
    except Exception as e:
        print(f"\n\n❌ 에러 발생: {e}")
        return 1

    return 0

if __name__ == '__main__':
    sys.exit(main())
