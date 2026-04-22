#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
SNES Test Program - 자동 테스트 오토메이션

/tests_src 폴더의 스크린샷 파일명으로부터 테스트 시퀀스를 역구성하고,
각 테스트 단계마다 필요한 버튼 입력을 자동화한다.

버튼 매핑:
  SELECT = Enter
  START = Space
  UP = W
  DOWN = S
  LEFT = A
  RIGHT = D
  A = Z
  B = X
  X = C
  Y = V
"""

import os
import sys
import time
import re
import json
from pathlib import Path
from datetime import datetime
from dataclasses import dataclass, asdict
from typing import List, Dict, Optional, Tuple
import subprocess

# UTF-8 지원
import io
sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')

# 버튼 매핑
BUTTON_MAP = {
    'ST': 'space',      # START
    'SL': 'return',     # SELECT
    'U': 'w',           # UP
    'D': 's',           # DOWN
    'L': 'a',           # LEFT
    'R': 'd',           # RIGHT
    'A': 'z',           # A
    'B': 'x',           # B
    'X': 'c',           # X
    'Y': 'v',           # Y
}

# 조합 버튼 정의
COMBO_BUTTONS = {
    'LU': ['a', 'w'],           # LEFT + UP
    'LD': ['a', 's'],           # LEFT + DOWN
    'RU': ['d', 'w'],           # RIGHT + UP
    'RD': ['d', 's'],           # RIGHT + DOWN
    'DR': ['d', 's'],           # DOWN + RIGHT (same as RD)
}

@dataclass
class ButtonInput:
    """버튼 입력 커맨드"""
    buttons: List[str]
    duration: float = 0.1
    release_wait: float = 0.3

    def __repr__(self) -> str:
        return f"ButtonInput({'+'.join(self.buttons)})"

@dataclass
class TestStep:
    """테스트 단계"""
    timestamp: str
    test_name: str
    step_number: int
    button_input: Optional[ButtonInput]
    screenshot_file: str

    def __repr__(self) -> str:
        if self.button_input:
            return f"{self.test_name}[{self.step_number}]: {self.button_input}"
        else:
            return f"{self.test_name}[{self.step_number}]: Auto"

class ScreenshotParser:
    """스크린샷 파일명 파싱"""

    PATTERN = r'스크린샷 (\d{4}-\d{2}-\d{2}) (\d{2})(\d{2})(\d{2})(?:_([^.]+))?\.png'

    @staticmethod
    def parse_filename(filename: str) -> Tuple[str, str, Optional[str]]:
        """
        파일명에서 타임스탐프와 버튼 정보 추출

        Returns:
            (timestamp, time_hms, button_info_str)
            예: ('2026-04-21', '181858', 'ST')
        """
        match = re.match(ScreenshotParser.PATTERN, filename)
        if not match:
            return None, None, None

        date, h, m, s = match.groups()[:4]
        button_info = match.group(5) if len(match.groups()) >= 5 else None
        time_hms = f"{h}{m}{s}"

        return date, time_hms, button_info

    @staticmethod
    def parse_button_info(button_str: str) -> List[str]:
        """
        버튼 정보 문자열을 실제 버튼 목록으로 변환

        예: 'LD_DR_SL_ST_X' -> ['a', 's', 'd', 's', 'return', 'space', 'c']
        """
        if not button_str:
            return []

        buttons = []
        parts = button_str.split('_')

        for part in parts:
            if part in COMBO_BUTTONS:
                buttons.extend(COMBO_BUTTONS[part])
            elif part in BUTTON_MAP:
                buttons.append(BUTTON_MAP[part])
            elif len(part) == 1 and part.upper() in BUTTON_MAP:
                buttons.append(BUTTON_MAP[part.upper()])
            else:
                print(f"⚠️  Unknown button: {part}")

        return buttons

class TestSequenceBuilder:
    """테스트 시퀀스 빌더"""

    TEST_MARKERS = {
        '181858': ('Electronics Test', 1),  # ST
        '181910': ('Character Test', 2),
        '181950': ('Controller Test', 3),
        '182342': ('Sound Test', 4),
        # Color Test와 Accessories Test는 자동 진행
    }

    def __init__(self, screenshot_dir: Path):
        self.screenshot_dir = Path(screenshot_dir)
        self.steps: List[TestStep] = []

    def build_sequence(self) -> List[TestStep]:
        """스크린샷 파일들로부터 테스트 시퀀스 구성"""

        files = sorted(self.screenshot_dir.glob('*.png'))
        current_test = None
        current_step = 0

        for file in files:
            date, time_hms, button_info = ScreenshotParser.parse_filename(file.name)

            if date is None:
                continue

            # 테스트 구간 감지
            if time_hms in TestSequenceBuilder.TEST_MARKERS:
                test_name, test_num = TestSequenceBuilder.TEST_MARKERS[time_hms]
                current_test = test_name
                current_step = 0
                print(f"📍 시작: {test_name} (시간: {time_hms})")

            # 버튼 입력 파싱
            button_list = ScreenshotParser.parse_button_info(button_info)
            button_input = None

            if button_list:
                button_input = ButtonInput(buttons=button_list)
                print(f"  버튼: {button_info} → {button_list}")
            else:
                print(f"  자동 진행 (버튼 입력 없음)")

            current_step += 1

            step = TestStep(
                timestamp=f"{date} {time_hms}",
                test_name=current_test or "Main Menu",
                step_number=current_step,
                button_input=button_input,
                screenshot_file=file.name
            )

            self.steps.append(step)

        return self.steps

    def get_test_summary(self) -> Dict[str, List[TestStep]]:
        """테스트별 단계 요약"""
        summary = {}
        for step in self.steps:
            if step.test_name not in summary:
                summary[step.test_name] = []
            summary[step.test_name].append(step)
        return summary

class AutomationController:
    """자동 제어 엔진"""

    def __init__(self, emulator_window_title: str = "SNES"):
        self.emulator_title = emulator_window_title
        self.step_count = 0

    def press_buttons(self, buttons: List[str], duration: float = 0.1, release_wait: float = 0.3):
        """버튼 입력 실행"""
        if not buttons:
            return

        try:
            # Windows에서 xdotool 없으면 pyautogui 사용
            import pyautogui

            # 버튼 누르기
            for button in buttons:
                pyautogui.press(button)
                time.sleep(0.05)

            time.sleep(duration)

            # 릴리스 대기
            time.sleep(release_wait)

            self.step_count += 1

        except ImportError:
            print("❌ pyautogui 라이브러리 필요. 설치: pip install pyautogui")

    def auto_play_test_sequence(self, steps: List[TestStep], dry_run: bool = False):
        """테스트 시퀀스 자동 실행"""

        print("\n" + "="*60)
        print("🎮 SNES TEST AUTOMATION STARTED")
        print("="*60 + "\n")

        for i, step in enumerate(steps, 1):
            print(f"\n[{i}/{len(steps)}] {step}")

            if step.button_input:
                if dry_run:
                    print(f"  → DRY RUN: 버튼 입력 {step.button_input.buttons}")
                else:
                    print(f"  → 버튼 입력: {step.button_input.buttons}")
                    self.press_buttons(
                        step.button_input.buttons,
                        duration=step.button_input.duration,
                        release_wait=step.button_input.release_wait
                    )
            else:
                print(f"  → 자동 진행 대기 2초...")
                if not dry_run:
                    time.sleep(2)

        print("\n" + "="*60)
        print("✅ TEST AUTOMATION COMPLETED")
        print("="*60)

    def verify_audio_output(self) -> Dict[str, bool]:
        """오디오 출력 검증"""
        return {
            'left_channel': None,  # 실제 구현 필요
            'right_channel': None,
            'completion_ding': None
        }

class ReportGenerator:
    """테스트 리포트 생성"""

    @staticmethod
    def generate_json_report(steps: List[TestStep], output_path: Path):
        """JSON 형식 리포트"""

        summary = {}
        test_groups = {}

        for step in steps:
            if step.test_name not in test_groups:
                test_groups[step.test_name] = []
            test_groups[step.test_name].append(step)

        report = {
            'timestamp': datetime.now().isoformat(),
            'total_steps': len(steps),
            'tests': {}
        }

        for test_name, test_steps in test_groups.items():
            report['tests'][test_name] = {
                'step_count': len(test_steps),
                'button_inputs': sum(1 for s in test_steps if s.button_input),
                'auto_steps': sum(1 for s in test_steps if not s.button_input),
                'steps': [
                    {
                        'number': s.step_number,
                        'timestamp': s.timestamp,
                        'button_input': s.button_input.buttons if s.button_input else None,
                        'screenshot': s.screenshot_file
                    }
                    for s in test_steps
                ]
            }

        output_path.write_text(json.dumps(report, indent=2, ensure_ascii=False))
        print(f"✅ Report saved: {output_path}")

    @staticmethod
    def generate_markdown_report(steps: List[TestStep], output_path: Path):
        """Markdown 형식 리포트"""

        md = "# SNES Test Automation Sequence\n\n"
        md += f"**Generated**: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}\n\n"
        md += f"**Total Steps**: {len(steps)}\n\n"

        test_groups = {}
        for step in steps:
            if step.test_name not in test_groups:
                test_groups[step.test_name] = []
            test_groups[step.test_name].append(step)

        for test_name, test_steps in test_groups.items():
            md += f"## {test_name}\n\n"
            md += f"Steps: {len(test_steps)} | "
            md += f"Button Inputs: {sum(1 for s in test_steps if s.button_input)}\n\n"
            md += "| # | Time | Button Input | Screenshot |\n"
            md += "|---|------|--------------|------------|\n"

            for step in test_steps:
                buttons = f"`{', '.join(step.button_input.buttons)}`" if step.button_input else "Auto"
                md += f"| {step.step_number} | {step.timestamp} | {buttons} | {step.screenshot_file} |\n"

            md += "\n"

        output_path.write_text(md, encoding='utf-8')
        print(f"✅ Report saved: {output_path}")

def main():
    """메인 실행"""

    # 프로젝트 루트
    root_dir = Path(__file__).parent
    screenshot_dir = root_dir / "tests_src"

    if not screenshot_dir.exists():
        print(f"❌ Screenshot directory not found: {screenshot_dir}")
        return 1

    # 테스트 시퀀스 빌드
    print("📂 Parsing screenshots...")
    builder = TestSequenceBuilder(screenshot_dir)
    steps = builder.build_sequence()

    print(f"\n✅ Total steps parsed: {len(steps)}")

    # 테스트별 요약
    summary = builder.get_test_summary()
    print("\n📊 Test Summary:")
    for test_name, test_steps in summary.items():
        print(f"  • {test_name}: {len(test_steps)} steps")

    # 리포트 생성
    output_dir = root_dir / "test_results"
    output_dir.mkdir(exist_ok=True)

    json_report = output_dir / "test_sequence.json"
    md_report = output_dir / "test_sequence.md"

    ReportGenerator.generate_json_report(steps, json_report)
    ReportGenerator.generate_markdown_report(steps, md_report)

    # 자동화 시뮬레이션 (드라이 런)
    print("\n🎮 Running automation (dry run)...")
    controller = AutomationController()
    controller.auto_play_test_sequence(steps, dry_run=True)

    print("\n" + "="*60)
    print("📝 Usage:")
    print("="*60)
    print("\n1. 드라이 런 (실제 입력 없음):")
    print("   python snes_test_automation.py --dry-run\n")
    print("2. 실제 자동 실행:")
    print("   python snes_test_automation.py --run\n")
    print("3. 특정 테스트만 실행:")
    print("   python snes_test_automation.py --test 'Controller Test'\n")

    return 0

if __name__ == '__main__':
    sys.exit(main())
