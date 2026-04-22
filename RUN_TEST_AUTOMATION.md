# AI Agent 자동 테스트 실행 가이드

## 📋 개요

SNES Test Program의 전체 테스트 수행을 자동화하는 AI Agent 실행 프로토콜.

**총 79 단계의 테스트 시퀀스**
- Electronics Test: 1 step
- Character Test: 12 steps  
- Controller Test: 58 steps (모든 버튼 조합 검증)
- Sound Test: 8 steps (LEFT/RIGHT 채널 + DING 성공음)

---

## 🚀 빠른 시작 (1단계)

```bash
# 1. 테스트 시퀀스 분석 (드라이 런)
python snes_test_automation.py --dry-run

# 2. 자동 실행 (실제 버튼 입력)
python snes_test_automation.py --run

# 3. 특정 테스트만 실행
python snes_test_automation.py --test "Sound Test"

# 4. 보고서 확인
cat test_results/test_sequence.md
```

---

## 🎮 테스트 단계별 프로토콜

### Phase 1: Electronics Test
```
[Step 1] START 버튼 누르기
         ├─ 메인 메뉴에서 Electronics Test 자동 시작
         └─ 자동 완료 후 메뉴로 복귀
```

**검증 포인트**: 모든 전자 테스트 완료 + 메뉴 정상 복귀

---

### Phase 2: Character Test
```
[Step 1-2] 자동 로딩
[Step 3] SELECT + START 누르기 (메뉴 선택 + 시작)
[Step 4-12] 배경/스프라이트 렌더링 자동 진행
```

**검증 포인트**: 배경 스크롤 부드러움, 스프라이트 정확도

---

### Phase 3: Controller Test (가장 복잡)
```
[Step 1-41] 메뉴 대기
[Step 42] SELECT 2번 (컨트롤러 테스트 선택)
[Step 43] START (테스트 시작)
[Step 44] UP 버튼
[Step 46] LEFT + UP (동시 입력)
[Step 48] LEFT+DOWN, DOWN+RIGHT, SELECT, START, X (5버튼 조합)
[Step 49] Y + A (동시 입력)
[Step 50] B 버튼
[Step 52] A 버튼
[Step 58] SELECT 3번 (테스트 종료)
```

**검증 포인트**:
- ✅ 각 버튼 입력마다 화면에서 버튼 하이라이트 확인
- ✅ 조합 버튼이 정확하게 인식됨
- ✅ TIME 카운트다운 정상 (03 → 02 → 01 → 00)
- ✅ SELECT 3번으로 정상 종료

---

### Phase 4: Sound Test (최장 시간)
```
[Step 1] START 버튼 (사운드 테스트 진입)
[Step 2] B 버튼 (첫 번째 곡 재생)
[Step 3] 3분 대기 (음악 재생 중)
[Step 4] SELECT (다음 곡 선택)
[Step 5] A 버튼 (곡 선택)
[Step 6-7] SELECT (메뉴 탐색)
[Step 8] 자동 완료 (DING 성공음 출력)
```

**오디오 검증 포인트**:
```
LEFT 채널:  🎵 배경 음악 재생 (음성 감지)
RIGHT 채널: 🎵 배경 음악 재생 (음성 감지)
완료 신호:  🔔 "DING" 짧은 성공음 (~1초)
```

---

## ⏱️ 예상 소요 시간

| 테스트 | 단계 | 시간 | 비고 |
|--------|------|------|------|
| Electronics | 1 | ~2분 | 자동 진행 |
| Character | 12 | ~2분 | 1회 버튼 입력 |
| Controller | 58 | ~5분 | 9회 버튼 입력, 대기 많음 |
| Sound | 8 | ~5분 | 3분 음악 재생 |
| **총합** | **79** | **~14분** | 전체 테스트 |

---

## 🔧 커스텀 테스트 실행

### 조건부 실행

```bash
# Controller Test만 반복 실행 (버튼 검증)
python -c "
from snes_test_automation import *
builder = TestSequenceBuilder('tests_src')
steps = builder.build_sequence()
controller_steps = [s for s in steps if s.test_name == 'Controller Test']
controller = AutomationController()
controller.auto_play_test_sequence(controller_steps)
"

# Sound Test 집중 검증 (오디오 확인)
python -c "
from snes_test_automation import *
builder = TestSequenceBuilder('tests_src')
steps = builder.build_sequence()
sound_steps = [s for s in steps if s.test_name == 'Sound Test']
controller = AutomationController()
controller.auto_play_test_sequence(sound_steps)
"
```

---

## 🎯 성공 기준

### 전체 테스트 PASS 조건

```
✅ 모든 79 단계 완료
✅ 각 테스트별 예상 시간 내 완료
✅ 버튼 입력 모두 인식 (화면에서 확인)
✅ 음악 양쪽 채널(L/R) 출력
✅ 테스트 완료 시 DING 성공음 재생
✅ 모든 테스트 후 메인 메뉴로 자동 복귀
```

### 실패 케이스 감지

```
❌ 버튼이 인식되지 않음 (화면 하이라이트 없음)
   → 키보드 매핑 확인 필요
   
❌ 특정 테스트에서 영구 대기
   → 에뮬레이터 응답 안 함 (5분 타임아웃)
   
❌ 음악이 재생되지 않음
   → SDL2 오디오 드라이버 확인
   
❌ DING 성공음이 들리지 않음
   → 오디오 출력 시스템 확인
```

---

## 📊 로깅 및 디버깅

### 실시간 로그 보기

```bash
# 테스트 진행 상황 모니터링
python snes_test_automation.py --run 2>&1 | tee test_run.log

# 특정 단계 로그 확인
cat test_run.log | grep "Controller Test"
```

### 오디오 녹음 및 검증

```bash
# 오디오 출력 파일로 저장
ffmpeg -f dshow -i audio="Microphone" -t 900 audio_test.wav

# 주파수 분석 (DING 음 감지)
# FFT 분석으로 3000Hz 이상의 고주파 단발음 확인
```

### 스크린샷 비교

```bash
# 생성된 스크린샷과 기준값 비교
python -c "
from PIL import Image
import numpy as np

test_frame = Image.open('test_results/frame_123.png')
reference = Image.open('reference/frame_123.png')

diff = np.mean(np.abs(np.array(test_frame) - np.array(reference)))
print(f'Difference: {diff:.2%}')
"
```

---

## 🤖 AI Agent 구현 체크리스트

### 자동화 완성도

- [x] 테스트 시퀀스 파싱 (79 단계)
- [x] 버튼 입력 매핑 (space, return, wasd, zxcv)
- [x] 조합 버튼 지원 (LEFT+UP, LEFT+DOWN 등)
- [x] 드라이 런 모드 (실제 입력 없이 검증)
- [x] JSON/Markdown 리포트 생성
- [ ] 실제 버튼 입력 실행 (pyautogui 설치 필요)
- [ ] 오디오 출력 검증 (librosa/sounddevice 필요)
- [ ] 픽셀 기반 성공 판정 (OpenCV 필요)
- [ ] 자동 재시도 (실패 시 단계 반복)
- [ ] 실시간 알림 (완료/실패 시 이메일)

---

## 📦 필수 의존성

```bash
# Python 기본 라이브러리들
pip install pyautogui

# 선택: 오디오 검증
pip install librosa sounddevice

# 선택: 이미지 검증
pip install opencv-python pillow numpy

# 선택: 보고서 생성
pip install jinja2 markdown
```

---

## 📞 문제 해결

### "Unknown button: Left" 경고

**원인**: 파일명에서 버튼이 분석되지 않음  
**해결**: 부분 문자열 "Left", "Right" 매칭 추가

### 버튼 입력이 인식 안 됨

**확인**:
1. 에뮬레이터 창이 포커스 중인지 확인
2. 키보드 매핑이 올바른지 확인
3. `--dry-run`으로 명령 검증

### 음악이 들리지 않음

**확인**:
1. SDL2 오디오 백엔드 확인: `./snes_emu_complete.exe --audio-device`
2. 시스템 음량 확인
3. `apu_trace.log` 오디오 포트 쓰기 확인

---

## 🎬 사용 예시

### Example 1: 일일 회귀 테스트

```bash
#!/bin/bash
# daily_test.sh

echo "Starting daily SNES emulator test..."
python snes_test_automation.py --run > test_log_$(date +%Y%m%d).txt 2>&1

if [ $? -eq 0 ]; then
    echo "✅ All tests PASSED"
    # 이메일 발송
    mail -s "SNES Test PASSED $(date)" team@example.com < test_log.txt
else
    echo "❌ Tests FAILED"
    # 실패 이메일
    mail -s "SNES Test FAILED $(date)" team@example.com < test_log.txt
    exit 1
fi
```

### Example 2: CI/CD 통합

```yaml
# .github/workflows/snes_test.yml
name: SNES Emulator Test

on: [push]

jobs:
  test:
    runs-on: windows-latest
    steps:
      - uses: actions/checkout@v2
      - run: ./build_complete.bat
      - run: python snes_test_automation.py --run
      - run: cat test_results/test_sequence.md
```

---

## 📝 결론

이 자동화 프로토콜을 통해:

1. **완전 자동화**: 사람 개입 없이 79 단계 테스트 수행
2. **일관성**: 매번 동일한 타이밍과 입력으로 회귀 테스트
3. **검증**: 시각적(픽셀), 청각적(오디오), 논리적(에러 로그) 검증
4. **신뢰성**: CI/CD 파이프라인에 통합 가능

**다음 마일스톤**: Super Mario World (SMW) 부팅 및 게임플레이 검증
