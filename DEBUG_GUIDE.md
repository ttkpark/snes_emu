# 자동화 테스트 디버깅 가이드

## 🔴 문제: 테스트 중간에 멈춤

### 원인 분석

1. **pyautogui 버튼 입력이 전달 안 됨**
   - 에뮬레이터 창이 포커스 상태가 아님
   - Windows 보안 정책 차단

2. **타임아웃**
   - 특정 단계에서 예상치 못한 시간 소요

3. **에뮬레이터 응답 없음**
   - 게임이 멈춤 또는 무한 루프

---

## 🔧 추적 방법

### 1단계: 세부 로깅 활성화

```python
# run_automation_debug.py에 추가
import logging
import time
from datetime import datetime

logging.basicConfig(
    filename='automation_debug.log',
    level=logging.DEBUG,
    format='%(asctime)s - %(levelname)s - %(message)s'
)

def press_with_logging(buttons, step_num, test_name):
    """로깅이 포함된 버튼 입력"""
    logging.info(f"[Step {step_num}] {test_name} - 버튼 입력 시작: {buttons}")
    
    start_time = time.time()
    try:
        for btn in buttons:
            pyautogui.press(btn)
            logging.debug(f"  └─ 버튼 {btn} 입력됨 ({time.time()-start_time:.2f}s)")
        
        elapsed = time.time() - start_time
        logging.info(f"  ✅ 완료 ({elapsed:.2f}초)")
        return True
    
    except Exception as e:
        logging.error(f"  ❌ 에러: {e}")
        return False
```

### 2단계: 타임스탬프 기록

```bash
# 실행할 때 시간 기록
echo "[$(date '+%H:%M:%S')] 자동화 시작" >> automation_run.log
python run_automation_debug.py >> automation_run.log 2>&1 &
AUTOPID=$!

# 진행 상황 모니터링
while kill -0 $AUTOPID 2>/dev/null; do
    tail -1 automation_run.log
    sleep 5
done

echo "[$(date '+%H:%M:%S')] 자동화 완료" >> automation_run.log
```

### 3단계: 멈춘 지점 파악

```bash
# 가장 최근 로그 확인
tail -30 automation_debug.log

# 특정 단계 검색
grep "Step 42" automation_debug.log

# 에러 로그만 보기
grep "ERROR\|❌" automation_debug.log
```

---

## 📊 문제별 해결책

### 문제 1: 특정 단계에서 2-3초 이상 멈춤

**증상:**
```
[Step 42] Controller Test - 버튼 입력 시작: ['return', 'return']
[**멈춤**]
[Step 43] Controller Test - 버튼 입력 시작: ['space']
```

**원인:** 에뮬레이터가 응답 없음

**해결:**
```python
# 타임아웃 추가
import signal

def timeout_handler(signum, frame):
    raise TimeoutError("버튼 입력 타임아웃")

signal.signal(signal.SIGALRM, timeout_handler)
signal.alarm(10)  # 10초 타임아웃

try:
    pyautogui.press('space')
finally:
    signal.alarm(0)
```

### 문제 2: Sound Test 음악 재생 3분 동안 멈춤

**정상 현상입니다!** 💡

- 음악 재생 중 정지하지 않음
- 로그에 "자동 진행 (2초 대기)" 표시
- 약 3분 정상 진행

### 문제 3: pyautogui가 어떤 창에 입력하는지 모름

**원인:** 에뮬레이터 창이 포커스 상태가 아님

**해결:**
```python
import pyautogui

# 1. 에뮬레이터 창을 찾아서 포커스
import win32gui
window_name = "SNES Emulator"
hwnd = win32gui.FindWindow(None, window_name)
if hwnd:
    win32gui.SetForegroundWindow(hwnd)
    logging.info(f"✅ 에뮬레이터 창 포커스: {window_name}")
else:
    logging.error(f"❌ 에뮬레이터 창을 찾을 수 없음: {window_name}")

# 2. 마우스를 에뮬레이터 중앙으로 이동
pyautogui.moveTo(640, 360)
```

---

## 🎯 실시간 모니터링

### 방법 1: 터미널에서 실시간 로그 보기

```bash
# 터미널 1: 자동화 실행
python run_automation_debug.py > automation.log 2>&1

# 터미널 2: 실시간 로그 감시
tail -f automation.log | grep -E "(Step|버튼|완료|❌)"
```

### 방법 2: 화면 녹화로 확인

```bash
# ffmpeg로 화면 녹화 (선택)
ffmpeg -f gdigrab -i desktop -t 900 automation_run.mp4 &

# 자동화 실행
python run_automation_debug.py

# 녹화 중지
pkill ffmpeg
```

### 방법 3: 프로세스 추적

```bash
# 실행 중인 프로세스 모니터링
watch -n 1 'ps aux | grep "python\|snes_emu"'

# 또는
top -p $AUTOPID  # AUTOPID는 프로세스 ID
```

---

## 💾 로그 분석 스크립트

```python
# analyze_automation_log.py
import re
from pathlib import Path

def analyze_log(log_file):
    with open(log_file, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()
    
    steps = {}
    for line in lines:
        match = re.search(r'\[Step (\d+)\]', line)
        if match:
            step_num = int(match.group(1))
            if step_num not in steps:
                steps[step_num] = []
            steps[step_num].append(line.strip())
    
    # 마지막 단계 찾기
    if steps:
        last_step = max(steps.keys())
        print(f"마지막 완료 단계: {last_step}/79")
        print("\n마지막 단계 로그:")
        for line in steps[last_step][-5:]:  # 마지막 5줄
            print(f"  {line}")
        
        if last_step < 79:
            print(f"\n🔴 멈춘 지점: Step {last_step} 이후")
            print(f"   다음 단계: Step {last_step + 1}")
    
    # 에러 찾기
    errors = [l for l in lines if 'ERROR' in l or '❌' in l]
    if errors:
        print(f"\n❌ 발견된 에러 ({len(errors)}개):")
        for err in errors[:5]:
            print(f"  {err.strip()}")

if __name__ == '__main__':
    analyze_log('automation_debug.log')
```

---

## 📋 체크리스트

자동화를 다시 실행하기 전 확인:

- [ ] 에뮬레이터 창이 열려있는가?
- [ ] 에뮬레이터 창이 활성화(포커스)되어 있는가?
- [ ] `pyautogui` 설치됨 (`pip list | grep pyautogui`)
- [ ] `tests_src/` 폴더에 79개 스크린샷이 있는가?
- [ ] `SNES Test Program.sfc` ROM 파일이 있는가?
- [ ] 로그 파일을 쓸 디스크 공간이 충분한가?

---

## 🆘 여전히 멈추면

### Step-by-step 실행 방법

```python
# manual_step_test.py
from snes_test_automation import TestSequenceBuilder
import pyautogui
import time

builder = TestSequenceBuilder('tests_src')
steps = builder.build_sequence()

# 특정 단계부터 시작 (예: 42번)
start_step = int(input("시작 단계 (1-79): ")) - 1

for i, step in enumerate(steps[start_step:], start=start_step+1):
    print(f"\n[Step {i}] {step.test_name}")
    input("  → Enter 누르면 실행...")
    
    if step.button_input:
        for btn in step.button_input.buttons:
            print(f"    버튼: {btn}")
            pyautogui.press(btn)
            time.sleep(0.1)
    else:
        print(f"    자동 진행...")
        time.sleep(2)
    
    print(f"  ✅ 완료")
```

**이 방법으로 어느 단계에서 멈추는지 정확히 파악 가능합니다!**

---

## 📞 최종 정리

**멈춘 부분을 추적하려면:**

1. **로그 파일 활성화** → `automation_debug.log` 생성
2. **타임스탬프 기록** → 각 단계별 시간 기록
3. **마지막 로그 확인** → 어디서 멈췄는지 파악
4. **Step-by-step 실행** → 문제 단계를 수동으로 테스트
5. **프로세스 모니터링** → CPU/메모리 사용률 확인

이 방법으로 **정확히 멈춘 위치와 원인을 파악**할 수 있습니다! ✅
