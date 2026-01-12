# SNES 에뮬레이터 디버깅 도구 문서 설계

## 개요

이 문서는 SNES 에뮬레이터 개발 시 필요한 모든 디버깅 도구와 기법을 체계적으로 정리한 마스터 플랜입니다.

---

## 📁 디렉토리 구조

```
docs/
├── debugging/
│   ├── README.md                        # 디버깅 도구 개요
│   │
│   ├── 01_breakpoint_system.md          # 브레이크포인트 시스템
│   ├── 02_loop_detection.md             # 무한 루프 감지
│   ├── 03_trace_logging.md              # 실행 트레이스 로깅
│   ├── 04_memory_inspection.md          # 메모리 검사 도구
│   ├── 05_port_communication.md         # CPU-APU 포트 통신 디버깅
│   ├── 06_timing_analysis.md            # 타이밍 분석
│   ├── 07_performance_profiling.md      # 성능 프로파일링
│   │
│   ├── common_issues/
│   │   ├── cpu_bugs.md                  # CPU 관련 일반 버그
│   │   ├── ppu_bugs.md                  # PPU 관련 일반 버그
│   │   ├── apu_bugs.md                  # APU 관련 일반 버그
│   │   ├── memory_bugs.md               # 메모리 관련 일반 버그
│   │   └── timing_bugs.md               # 타이밍 관련 일반 버그
│   │
│   ├── tools/
│   │   ├── command_line_debugger.md     # CLI 디버거 설계
│   │   ├── scripting_interface.md       # 스크립팅 인터페이스
│   │   ├── visualization_tools.md       # 시각화 도구
│   │   └── automated_testing.md         # 자동화된 테스트
│   │
│   └── case_studies/
│       ├── spctest_debugging.md         # spctest.sfc 디버깅 케이스
│       ├── timing_issues.md             # 타이밍 문제 해결
│       └── graphics_glitches.md         # 그래픽 버그 해결
```

---

## 📖 각 문서의 목적 및 내용

### 핵심 디버깅 도구 (01-07)

#### 01_breakpoint_system.md
**목적**: 브레이크포인트 시스템 구현 및 사용법

**내용**:
- PC 브레이크포인트
- 메모리 읽기/쓰기 브레이크포인트
- 조건부 브레이크포인트
  - 레지스터 값 조건
  - 메모리 값 조건
  - 복합 조건 (AND, OR, NOT)
- 일회성 브레이크포인트
- 카운터 기반 브레이크포인트
- 구현 예제 (C++)
- PowerShell 스크립트 연동

**예제**:
```cpp
// PC 브레이크포인트
debugger->addBreakpoint(0x0357, BreakType::PC);

// 조건부 브레이크포인트
debugger->addConditionalBreakpoint(
    0x0343,
    [](CPUState& state) {
        return state.a == 0x56;
    }
);

// 메모리 쓰기 브레이크포인트
debugger->addMemoryBreakpoint(0x2140, BreakType::WRITE);
```

#### 02_loop_detection.md
**목적**: 무한 루프 자동 감지 및 분석

**내용**:
- PC 방문 빈도 추적
- 상태 변화 감지
- 루프 패턴 인식
- 자동 중단 메커니즘
- 루프 분석 리포트 생성
- 구현 예제
- analyze_loop.ps1 사용법

**알고리즘**:
```cpp
class LoopDetector {
private:
    std::map<uint32_t, int> pc_visit_count;
    std::map<uint32_t, std::vector<State>> pc_states;
    
public:
    bool detectLoop(uint32_t pc, const State& state, int threshold = 100) {
        pc_visit_count[pc]++;
        pc_states[pc].push_back(state);
        
        if (pc_visit_count[pc] > threshold) {
            // 상태가 변화하는지 확인
            auto& states = pc_states[pc];
            std::set<State> unique_states(states.begin(), states.end());
            
            if (unique_states.size() == 1) {
                // 동일한 상태로 반복 = 무한 루프
                return true;
            }
        }
        
        return false;
    }
};
```

#### 03_trace_logging.md
**목적**: 실행 트레이스 로깅 시스템

**내용**:
- 로그 레벨 (DEBUG, INFO, WARN, ERROR)
- 컴포넌트별 필터링 (CPU, PPU, APU, Memory)
- 조건부 로깅
- 로그 포맷
- 성능 영향 최소화
- 로그 분석 도구
- 로그 파일 순환

**로그 포맷**:
```
[Cyc:0000885366] [APU] PC:0x0357 | 2f fe | BRA rel | A:0x56 X:0x34 Y:0x56 SP:0xef PSW:0x01
[Cyc:0000885367] [CPU] PC:0x8024 | ad 40 21 | LDA $2140 | A:0xCC X:0x00 Y:0x00
[Cyc:0000885368] [MEM] Read $2140 = 0xCC (CPU->APU Port 0)
```

#### 04_memory_inspection.md
**목적**: 메모리 검사 및 분석 도구

**내용**:
- 메모리 덤프 (범위 지정)
- 메모리 검색 (값, 패턴)
- 메모리 비교 (스냅샷)
- 메모리 워치 (변경 감지)
- 16진수/ASCII 뷰
- 메모리 맵 시각화
- VRAM, OAM, CGRAM 특수 뷰

**CLI 명령어**:
```bash
# 메모리 덤프
dump 0x7E0000 0x7E01FF

# 값 검색
search 0xCC 0x7E0000 0x7EFFFF

# 메모리 워치
watch 0x2140 0x2143

# 스냅샷 비교
snapshot save state1
# ... 실행 ...
snapshot compare state1
```

#### 05_port_communication.md
**목적**: CPU-APU 포트 통신 디버깅

**내용**:
- 포트 0-3 추적
- 읽기/쓰기 이벤트 로깅
- 타이밍 분석
- 프로토콜 검증
- 동기화 문제 감지
- 일반적인 포트 통신 버그
- spctest 포트 프로토콜

**포트 트레이스**:
```
[Cyc:0000123456] CPU wrote Port 0 = 0xCC
[Cyc:0000123457] CPU wrote Port 1 = 0x01
[Cyc:0000123500] SPC read Port 0 = 0xCC
[Cyc:0000123501] SPC read Port 1 = 0x01
[Cyc:0000123550] SPC wrote Port 0 = 0x00  ← 준비 완료 신호
[Cyc:0000123600] CPU read Port 0 = 0x00   ← 확인
```

#### 06_timing_analysis.md
**목적**: 타이밍 분석 및 동기화 검증

**내용**:
- CPU/PPU/APU 사이클 추적
- 사이클 비율 검증
- NMI/IRQ 타이밍
- DMA/HDMA 타이밍
- 프레임 타이밍 분석
- 타이밍 다이어그램
- 느린/빠른 실행 감지

**타이밍 체크**:
```cpp
// CPU : APU 비율 = 3.58 MHz : 1.024 MHz ≈ 3.5:1
double expected_ratio = 3.5;
double actual_ratio = (double)cpu_cycles / apu_cycles;

if (abs(actual_ratio - expected_ratio) > 0.1) {
    printf("WARNING: CPU/APU timing drift: %.2f (expected %.2f)\n",
           actual_ratio, expected_ratio);
}
```

#### 07_performance_profiling.md
**목적**: 성능 프로파일링 및 최적화

**내용**:
- 함수별 실행 시간 측정
- 핫스팟 식별
- 메모리 사용량 추적
- 사이클 정확도 vs 속도 트레이드오프
- 최적화 전후 비교
- 벤치마크 도구

---

### 일반 버그 패턴 (common_issues/)

#### cpu_bugs.md
**내용**:
- 플래그 계산 오류 (특히 C, V 플래그)
- 주소 지정 모드 버그
- 16비트/8비트 모드 전환 문제
- 인터럽트 처리 오류
- 예제 및 해결책

**예제**:
```markdown
## 버그: ADC Overflow 플래그

### 증상
부호 있는 덧셈 오버플로우가 감지되지 않음

### 원인
V 플래그 계산 오류

### 해결
V = (A^result) & (operand^result) & 0x80
```

#### apu_bugs.md
**내용**:
- SPC700 명령어 구현 오류
- 플래그 계산 (특히 CMP의 C 플래그)
- 포트 동기화 문제
- IPL ROM 부트 실패
- 타이머 구현 버그

#### memory_bugs.md
**내용**:
- 메모리 맵 오류 (LoROM/HiROM)
- DMA 전송 버그
- WRAM 미러링
- 뱅크 래핑
- 읽기 전용 영역 쓰기

---

### 도구 구현 (tools/)

#### command_line_debugger.md
**목적**: CLI 디버거 완전한 설계 및 구현

**내용**:
- 명령어 세트 정의
- REPL 인터페이스
- 스텝 실행 (step, next, continue)
- 레지스터 표시
- 디스어셈블리
- 메모리 검사
- 구현 예제

**명령어 세트**:
```
# 실행 제어
run [rom_file]          - ROM 실행
step [n]                - n 명령어 실행 (기본 1)
continue                - 다음 브레이크포인트까지 실행
reset                   - 리셋

# 브레이크포인트
break <addr>            - 브레이크포인트 설정
break if <condition>    - 조건부 브레이크포인트
break list              - 브레이크포인트 목록
break delete <id>       - 브레이크포인트 삭제

# 검사
reg                     - 레지스터 표시
dis [addr] [count]      - 디스어셈블리
mem <addr> <len>        - 메모리 덤프
watch <addr>            - 메모리 워치

# 트레이스
trace on [filter]       - 트레이스 활성화
trace off               - 트레이스 비활성화
trace save <file>       - 트레이스 저장

# 분석
analyze loop            - 무한 루프 분석
analyze ports           - 포트 통신 분석
analyze timing          - 타이밍 분석
```

#### scripting_interface.md
**목적**: 자동화를 위한 스크립팅 인터페이스

**내용**:
- Lua/Python 바인딩
- 이벤트 훅 (onBreakpoint, onMemoryWrite 등)
- 상태 쿼리 API
- 자동화된 테스트 스크립트
- 리그레션 테스트

**Lua 예제**:
```lua
-- 포트 0이 0xCC인지 확인하는 테스트
function test_port_init()
    -- 브레이크포인트 설정
    emu.addBreakpoint(0x0300)
    
    -- 실행
    emu.run()
    
    -- 포트 값 확인
    local port0 = emu.readPort(0)
    assert(port0 == 0xCC, "Port 0 should be 0xCC")
    
    print("Test passed!")
end

test_port_init()
```

---

### 케이스 스터디 (case_studies/)

#### spctest_debugging.md
**목적**: spctest.sfc 디버깅 실제 사례

**내용**:
- 문제 증상 (PC:0x0357 무한 루프)
- 분석 과정
  1. analyze_loop.ps1 실행
  2. 포트 통신 확인
  3. CMP 명령어 검증
  4. 분기 조건 분석
- 원인 발견
- 해결 방법
- 교훈

**분석 과정**:
```markdown
## Step 1: 무한 루프 감지
```powershell
.\analyze_loop.ps1
# 결과: PC:0x0357에서 무한 루프
```

## Step 2: 주변 코드 분석
```powershell
Get-Content apu_trace.log | Select-String "0x0343|0x0346|0x0357" -Context 5,0
```

## Step 3: 포트 값 확인
```powershell
Get-Content port_comm.log | Select-String "port 0|port 1"
# 발견: 포트 0이 0x00 (기대값: 0xCC)
```

## Step 4: 근본 원인
- CPU가 APU 시작 전에 포트 초기화 안함
- 또는 초기화 타이밍 문제

## Step 5: 수정
```cpp
// src/main_complete.cpp
memory->write(0x2140, 0xCC);
memory->write(0x2141, 0x01);
apu->reset();
```
```

---

## 🔧 구현 우선순위

### Phase 1: 즉시 구현 (현재 디버깅에 필요)
1. ✅ `02_loop_detection.md` + `analyze_loop.ps1` (완료)
2. ✅ `05_port_communication.md` + `analyze_ports.ps1` (완료)
3. ✅ `DEBUGGER_DESIGN.md` (통합 디버거 설계 완료)
4. [ ] `common_issues/apu_bugs.md` (SPC700 버그 패턴)

### Phase 2: 단기 (1주)
1. [ ] `01_breakpoint_system.md`
2. [ ] `03_trace_logging.md`
3. [ ] `04_memory_inspection.md`
4. [ ] `tools/command_line_debugger.md`

### Phase 3: 중기 (1-2주)
1. [ ] `06_timing_analysis.md`
2. [ ] `07_performance_profiling.md`
3. [ ] `tools/scripting_interface.md`
4. [ ] 모든 common_issues 문서

### Phase 4: 장기 (1개월)
1. [ ] `tools/visualization_tools.md`
2. [ ] `tools/automated_testing.md`
3. [ ] 모든 case_studies 문서

---

## 📐 문서 작성 템플릿

각 디버깅 도구 문서는 다음 구조를 따릅니다:

```markdown
# [도구 이름]

## 개요
- 도구의 목적
- 해결하는 문제
- 사용 시기

## 이론적 배경
- 기술적 설명
- 알고리즘
- 데이터 구조

## 구현
### C++ 구현
- 헤더 파일
- 소스 파일
- 사용 예제

### PowerShell 스크립트
- 스크립트 코드
- 사용법
- 옵션

## 사용 예제
### 시나리오 1
- 문제 설명
- 도구 사용법
- 결과 해석

### 시나리오 2
- ...

## 일반적인 문제
### 문제 1
- 증상
- 원인
- 해결

## 참고 자료
- 관련 문서
- 외부 링크
```

---

## 🎯 각 문서의 Agent 친화성

### Agent가 쉽게 사용할 수 있도록:

1. **명확한 명령어**
   ```powershell
   # 좋은 예
   .\analyze_loop.ps1 -ThresholdCount 100
   
   # 나쁜 예
   "무한 루프를 분석해주세요"
   ```

2. **구조화된 출력**
   ```
   === Analysis Results ===
   Loop detected: PC:0x0357
   Visit count: 63
   Recommendation: Check instruction at 0x0357
   ```

3. **자동화 가능**
   ```powershell
   # 빌드 + 실행 + 분석 파이프라인
   .\build_complete.bat && 
   .\snes_emu_complete.exe spctest.sfc && 
   .\analyze_loop.ps1
   ```

4. **명확한 종료 조건**
   ```cpp
   if (loop_detected || max_cycles_reached || test_failed) {
       exit(exit_code);
   }
   ```

---

## 📊 측정 지표

각 디버깅 도구는 다음 지표를 제공해야 합니다:

1. **효율성**: 문제 발견까지 걸리는 시간
2. **정확도**: False positive/negative 비율
3. **오버헤드**: 성능 영향 (%)
4. **사용성**: Agent가 사용하기 쉬운 정도

---

**생성일**: 2025-12-14  
**버전**: 1.0  
**상태**: 설계 완료, 구현 시작










