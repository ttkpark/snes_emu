# SNES Test Program - 자동 테스트 흐름 분석

## 개요
SNES Test Program은 6개의 테스트 모듈을 순차적으로 실행한다.
각 테스트마다 필요한 버튼 입력을 자동화하여 완전 자동 테스트 환경 구축 가능.

## 테스트 메뉴 구조

```
SuperNES Test Menu
1. Electronics Test
2. Character Test
3. Controller Test
4. Sound Test
5. Color Test
6. Accessories Test

SELECT BUTTON: Choose TEST
START BUTTON: Begin TEST
```

## 테스트 자동 시나리오

### 1️⃣ Electronics Test (181858 - 181931)
- **메뉴**: SELECT로 1번 선택 → START 누름 (_ST)
- **테스트 내용**: 자동 진행 (버튼 입력 불필요)
- **진행**: 각 테스트 프레임별로 검증 스크린샷 수집
- **완료**: 메뉴로 자동 복귀

### 2️⃣ Character Test (181931 - 181950)
- **메뉴**: SELECT로 2번 선택 → START 누름
- **테스트 내용**: 자동 진행 (버튼 입력 불필요)
- **진행**: 스크롤/배경/스프라이트 렌더링 검증
- **완료**: 메뉴로 자동 복귀

### 3️⃣ Controller Test (181950 - 182353)
- **메뉴**: SELECT로 3번 선택 → START 누름
- **"PRESS KEY" 화면**: 각 버튼을 순차적으로 입력
  
  **버튼 입력 시퀀스:**
  ```
  Select (SL) → SL SL → Up (U) → Left+Up (LU)
  Left+Down, Down+Right, Select, Start, X 조합 (LD_DR_SL_ST_X)
  Y, A (Y_A) → B (B) → A + Left (A_Left)
  Right (Right) → 3번의 Select (SL_SL_SL)
  START (ST) 누르면 메뉴로 복귀
  ```

- **화면**: "TIME 03" 표시 → 각 버튼 하이라이트 → 시간 감소
- **완료**: 메뉴로 자동 복귀

### 4️⃣ Sound Test (182342 - 182652)
- **메뉴**: SELECT로 4번 선택 → START 누름
- **"PRESS KEY" 화면**: 음악 선택 및 재생
  
  **음악 선택 시퀀스:**
  ```
  B (처음 음악) → A + Select (다음 곡)
  또는 Left/Right로 곡 변경
  ```

- **오디오 검증 포인트**:
  - LEFT 채널: 음악 재생 확인
  - RIGHT 채널: 음악 재생 확인
  - TEST Complete: 띠링 단발 성공음 (DING)
  
- **완료**: 메뉴로 자동 복귀

### 5️⃣ Color Test (자동 진행)
- **메뉴**: SELECT로 5번 선택 → START 누름
- **테스트 내용**: 자동 진행 (버튼 입력 불필요)
- **검증 포인트**:
  - 색상 그라데이션 정확도
  - 팔레트 렌더링
  - 색상 연산 정확도
- **완료**: 메뉴로 자동 복귀

### 6️⃣ Accessories Test (자동 진행)
- **메뉴**: SELECT로 6번 선택 → START 누름
- **테스트 내용**: 자동 진행 (버튼 입력 불필요)
- **완료**: 메뉴로 자동 복귀

---

## 자동화 구현 방식

### 1. 버튼 매핑
```
SELECT = Enter
START = Space
UP = W / Arrow Up
DOWN = S / Arrow Down
LEFT = A / Arrow Left
RIGHT = D / Arrow Right
A = Z
B = X
X = C
Y = V
```

### 2. 타이밍 제어
- 각 스크린샷 간 **0.5-2초 대기** (프레임 안정화)
- Controller/Sound Test는 **버튼당 1초 홀드 + 0.5초 릴리스**
- 메뉴 전환은 **1초 대기** (화면 전환 안정화)

### 3. 검증 포인트
```
Electronics Test:
  ✓ 각 테스트 완료 상태 확인
  ✓ 메뉴로 정상 복귀

Character Test:
  ✓ 배경 스크롤 부드러움
  ✓ 스프라이트 렌더링 정확도
  ✓ 메뉴로 정상 복귀

Controller Test:
  ✓ 각 버튼 입력 인식 (화면 하이라이트)
  ✓ 조합 버튼 지원
  ✓ TIME 카운트다운 정상

Sound Test:
  ✓ LEFT/RIGHT 채널 음악 출력
  ✓ 곡 전환 정상
  ✓ TEST Complete 성공음 (DING)
  ✓ 볼륨/음질 기준값 통과

Color Test:
  ✓ 색상 그라데이션 (픽셀 매칭 < 1%)
  ✓ 팔레트 정확도
  ✓ 메뉴로 정상 복귀

Accessories Test:
  ✓ 하드웨어 호환성 확인
  ✓ 메뉴로 정상 복귀
```

---

## 자동 테스트 Agent 구현 시나리오

### Phase 1: GUI 자동 제어
```python
class SNESTestAutomator:
    def __init__(self, emulator_path, rom_path):
        self.emulator = SNESEmulator(emulator_path, rom_path)
        self.input_manager = InputManager()
        self.screenshot_manager = ScreenshotManager()
    
    def run_full_test_suite(self):
        for test_num in [1, 2, 3, 4, 5, 6]:
            self.select_test(test_num)
            self.run_test(test_num)
            self.capture_results(test_num)
            self.return_to_menu()
    
    def select_test(self, test_num):
        # SELECT 버튼으로 메뉴 이동
        for _ in range(test_num - 1):
            self.input_manager.press('SELECT', duration=0.1)
            time.sleep(0.3)
        
        # START 눌러서 테스트 시작
        time.sleep(0.5)
        self.input_manager.press('START', duration=0.2)
        time.sleep(2)  # 화면 전환 대기
```

### Phase 2: 오디오 검증
```python
class AudioVerifier:
    def __init__(self, audio_output_path):
        self.audio_path = audio_output_path
    
    def verify_stereo_channels(self, test_duration=10):
        """LEFT/RIGHT 채널 각각 음악 재생 확인"""
        left_rms = calculate_rms(self.audio_path, channel='L')
        right_rms = calculate_rms(self.audio_path, channel='R')
        
        if left_rms > THRESHOLD and right_rms > THRESHOLD:
            return PASS
        else:
            return FAIL
    
    def verify_completion_ding(self, audio_sample):
        """TEST Complete 띠링 성공음 감지"""
        # 높은 주파수 단발 음향 감지
        frequency = detect_peak_frequency(audio_sample)
        duration = measure_duration(audio_sample)
        
        if frequency > 3000 and duration < 1.0:
            return PASS
        else:
            return FAIL
```

### Phase 3: 이미지 검증
```python
class PixelVerifier:
    def compare_with_reference(self, captured_frame, reference_frame):
        """Snes9x 기준값과 픽셀 비교"""
        diff_percentage = calculate_diff_percentage(captured_frame, reference_frame)
        
        # 테스트별 허용 오차율
        tolerance = {
            'electronics': 0.01,  # 1%
            'character': 0.05,    # 5%
            'controller': 0.01,   # 1%
            'sound': 0.00,        # 음악 화면은 정적이므로 0%
            'color': 0.01,        # 1%
            'accessories': 0.05   # 5%
        }
        
        test_name = identify_test(captured_frame)
        if diff_percentage <= tolerance[test_name]:
            return PASS
        else:
            return FAIL(diff_percentage)
```

### Phase 4: 자동 리포팅
```python
class TestReport:
    def generate_summary(self, results):
        report = {
            'timestamp': datetime.now(),
            'tests': {
                'electronics': results['electronics'],
                'character': results['character'],
                'controller': results['controller'],
                'sound': results['sound'],
                'color': results['color'],
                'accessories': results['accessories']
            },
            'overall_status': 'PASS' if all_pass else 'FAIL',
            'screenshots_dir': '/tests_results/',
            'audio_samples': '/audio_results/',
            'pixel_diffs': '/diff_analysis/'
        }
        return report
```

---

## 실행 명령어

```bash
# 전체 테스트 스위트 실행
./run_test_automation.py --rom "SNES Test Program.sfc" --full-suite

# 특정 테스트만 실행
./run_test_automation.py --rom "SNES Test Program.sfc" --test 4  # Sound Test only

# 오디오 검증 포함
./run_test_automation.py --rom "SNES Test Program.sfc" --verify-audio --audio-output results/audio.wav

# 상세 레포트 생성
./run_test_automation.py --rom "SNES Test Program.sfc" --full-report --output results/
```

---

## 성공 기준

### 전체 테스트 PASS 조건
- ✅ 6개 테스트 모두 완료
- ✅ 각 테스트 픽셀 오차율 < 1% (electronics, sound, color)
- ✅ Sound Test: 양쪽 채널 음악 출력 + DING 성공음
- ✅ Controller Test: 모든 버튼 인식 확인
- ✅ Character Test: 스크롤/스프라이트 정상 렌더링

### AI Agent 자동화 체크리스트
- [ ] 메뉴 네비게이션 자동화
- [ ] 버튼 입력 타이밍 최적화
- [ ] 스크린샷 캡처 & 프레임 검증
- [ ] 오디오 출력 검증 (LEFT/RIGHT/DING)
- [ ] 자동 리포팅 및 로깅
- [ ] 실패 케이스 자동 분석
- [ ] 반복 실행 & 회귀 테스트 지원
