# SNES 에뮬레이터 Agent 개발 가이드

## 📋 개요

이 프로젝트는 완전한 SNES (Super Nintendo Entertainment System) 에뮬레이터입니다. AI Agent가 효율적으로 개발하고 디버깅할 수 있도록 **터미널 기반 도구**와 **구조화된 문서**를 제공합니다.

---

## 🚀 빠른 시작

### 1. 빌드
```powershell
.\build_complete.bat
```

### 2. 실행
```powershell
# 기본 실행
.\snes_emu_complete.exe spctest.sfc

# 모니터링과 함께 실행
.\monitor_execution.ps1 -TimeoutSeconds 60 -DetectLoop
```

### 3. 디버깅
```powershell
# 무한 루프 분석
.\analyze_loop.ps1

# 로그 비교
.\compare_traces.ps1 -GoldenTrace golden.log -CurrentTrace apu_trace.log

# 마지막 상태 확인
Get-Content apu_trace.log -Tail 20
```

---

## 📁 프로젝트 구조

```
snes_emu/
├── src/                      # 소스 코드
│   ├── cpu/                  # 65C816 CPU
│   ├── apu/                  # SPC700 + DSP
│   ├── ppu/                  # 비디오
│   ├── memory/               # 메모리 + DMA
│   ├── input/                # 입력
│   └── debug/                # 디버깅
│
├── docs/                     # 📚 문서 (Agent용)
│   ├── AGENT_DEVELOPMENT_GUIDE.md     # 종합 가이드
│   ├── QUICK_REFERENCE.md             # 빠른 참조
│   ├── hardware/
│   │   └── spc700_instructions.md     # SPC700 명령어 상세
│   └── test_roms/
│       └── spctest_expected.md        # 테스트 ROM 문서
│
├── monitor_execution.ps1     # 🔧 실행 모니터링 스크립트
├── analyze_loop.ps1          # 🔧 무한 루프 분석
├── compare_traces.ps1        # 🔧 로그 비교
│
├── build_complete.bat        # 빌드 스크립트
├── snes_emu_complete.exe     # 실행 파일
│
└── Test ROMs/
    ├── cputest-basic.sfc     # ✅ CPU 테스트 (통과)
    ├── cputest-full.sfc      # ✅ CPU 테스트 (통과)
    └── spctest.sfc           # ❌ SPC700 테스트 (진행 중)
```

---

## 🎯 현재 상태

### ✅ 완료된 기능
- **CPU (65C816)**: 100+ 명령어, 인터럽트 처리
- **메모리**: LoROM 매핑, 8채널 DMA
- **PPU**: SDL2 비디오, NMI 시스템
- **APU**: SPC700 CPU, DSP, 오디오 출력
- **입력**: SDL2 키보드/게임패드
- **로깅**: 상세한 실행 추적

### 🔄 진행 중
- **SPC700 테스트**: spctest.sfc 디버깅
  - **문제**: PC:0x0357에서 무한 루프 (fail 루틴)
  - **원인**: 포트 통신 또는 CMP 명령어 플래그 계산 문제
  - **상태**: 디버깅 도구 완성, 원인 분석 중

---

## 🐛 현재 디버깅 중인 문제

### 증상
```
[Cyc:0000885366] SPC700 PC:0x0357 | 2f fe | BRA rel | operand=0xfe
A:0x56 | X:0x34 | Y:0x56 | SP:0xef | PSW:0x01
```

- **PC**: 0x0357 (fail 루틴의 무한 루프)
- **명령어**: `BRA -2` (자기 자신으로 점프)
- **PSW**: 0x01 (Carry 플래그만 설정)

### 추정 원인
1. CPU가 포트 0에 0xCC를 쓰지 않음
2. SPC가 포트 값을 잘못 읽음
3. CMP 명령어 플래그 계산 오류
4. BNE 분기 조건 오류

### 디버깅 단계
```powershell
# 1. 포트 통신 확인
Get-Content port_comm.log | Select-String "port"

# 2. CMP 명령어 검증
Get-Content apu_trace.log | Select-String "cmp" | Select-Object -First 20

# 3. 분기 명령어 추적
Get-Content apu_trace.log | Select-String "bne\|beq" -CaseSensitive

# 4. 무한 루프 분석
.\analyze_loop.ps1
```

---

## 📚 Agent를 위한 문서

### 핵심 문서 (필독)

#### 1. **AGENT_DEVELOPMENT_GUIDE.md**
- Agent 개발 효율화 전략
- 터미널 기반 디버깅 시스템
- 자료 구조화 전략
- 자동 무한 루프 감지
- PowerShell 스크립트 사용법

#### 2. **QUICK_REFERENCE.md**
- 빠른 명령어 참조
- 주요 주소 및 상수
- 디버깅 체크리스트
- 자주 발생하는 버그 패턴
- PSW 플래그 계산 예제

#### 3. **hardware/spc700_instructions.md**
- 모든 SPC700 opcode 상세 설명
- 플래그 계산 공식
- 명령어별 예제
- 사이클 카운트
- 디버깅 팁

#### 4. **test_roms/spctest_expected.md**
- spctest.sfc 테스트 프로토콜
- CPU-APU 포트 통신
- 성공/실패 판정 기준
- 예상 실행 흐름
- 자주 발생하는 버그

---

## 🔧 Agent 친화적 도구

### 1. **monitor_execution.ps1**
실시간 실행 모니터링 + 자동 중단

```powershell
.\monitor_execution.ps1 `
    -RomFile "spctest.sfc" `
    -TimeoutSeconds 60 `
    -DetectLoop
```

**기능**:
- 실행 진행 상황 실시간 표시
- 무한 루프 자동 감지 (PC 100회 반복)
- 로그 파일 정지 감지 (3초 이상 변화 없음)
- 테스트 실패 자동 감지 (포트 2 = 0x02/0x03)
- 타임아웃 자동 종료

### 2. **analyze_loop.ps1**
무한 루프 패턴 분석

```powershell
.\analyze_loop.ps1 -ThresholdCount 100
```

**기능**:
- 가장 많이 실행된 PC 주소 Top 10
- 각 PC의 고유 상태 카운트
- 반복 패턴 감지
- 명령어 디스어셈블리
- 해결 방안 제안

### 3. **compare_traces.ps1**
로그 파일 비교

```powershell
.\compare_traces.ps1 `
    -GoldenTrace "golden_apu_trace.log" `
    -CurrentTrace "apu_trace.log" `
    -ContextLines 10
```

**기능**:
- 첫 번째 차이점 찾기
- 전후 컨텍스트 표시
- PSW 플래그 비트별 비교
- PC 주소 차이 하이라이트
- 통계 출력

---

## 💡 Agent 개발 효율화 전략

### 문제: Agent는 GUI를 사용할 수 없음
**해결책**: 모든 디버깅을 **터미널 기반**으로 제공

### 문제: 무한 루프 감지 어려움
**해결책**: 
- `monitor_execution.ps1`의 자동 감지
- `analyze_loop.ps1`의 패턴 분석
- 타임아웃 강제 종료

### 문제: 로그 파일이 너무 큼
**해결책**:
- `Get-Content -Tail N`으로 마지막만 읽기
- `Select-String`으로 필터링
- 조건부 로깅 (특정 주소 범위만)

### 문제: 하드웨어 스펙 불명확
**해결책**:
- `docs/hardware/` - 모든 명령어 상세 문서
- 플래그 계산 공식 명시
- 예제 코드 제공

### 문제: 테스트 ROM 동작 불명확
**해결책**:
- `docs/test_roms/` - 기대 동작 문서화
- 포트 프로토콜 명세
- 성공/실패 판정 기준

---

## 🎯 다음 단계

### 우선순위 1: 포트 초기화 확인
```cpp
// src/main_complete.cpp
memory->write(0x2140, 0xCC);
memory->write(0x2141, 0x01);
```

### 우선순위 2: CMP 명령어 검증
```cpp
// src/apu/apu.cpp
setFlag(FLAG_C, a >= operand);  // NOT (a < operand)!
```

### 우선순위 3: 분기 명령어 검증
```cpp
// BNE: Z=0일 때 분기
// BEQ: Z=1일 때 분기
```

### 우선순위 4: 포트 읽기/쓰기 로깅
```cpp
fprintf(port_log, "[Cyc:%lu] SPC read port %d = 0x%02X\n", ...);
```

---

## 📊 테스트 결과

| 테스트 ROM | 상태 | 비고 |
|-----------|------|------|
| cputest-basic.sfc | ✅ 통과 | CPU 기본 명령어 |
| cputest-full.sfc | ✅ 통과 | CPU 전체 명령어 |
| spctest.sfc | ❌ 진행 중 | SPC700 명령어 |
| SNES Test Program.sfc | ✅ 통과 | 통합 테스트 |

---

## 🔍 디버깅 워크플로우

### 1. 문제 발생
```powershell
.\snes_emu_complete.exe spctest.sfc
# 무한 루프 발생...
```

### 2. 자동 분석
```powershell
.\analyze_loop.ps1
# 출력: PC:0x0357에서 무한 루프 감지
```

### 3. 로그 확인
```powershell
Get-Content apu_trace.log -Tail 50
Get-Content port_comm.log | Select-String "port"
```

### 4. 문서 참조
```powershell
# PC:0x0357 = fail 루틴
# docs/test_roms/spctest_expected.md 참조
```

### 5. 코드 수정
```cpp
// src/apu/apu.cpp
// CMP 플래그 계산 수정
```

### 6. 재빌드 및 테스트
```powershell
.\build_complete.bat
.\monitor_execution.ps1 -TimeoutSeconds 30
```

### 7. 로그 비교
```powershell
.\compare_traces.ps1 -GoldenTrace old.log -CurrentTrace apu_trace.log
```

---

## 📖 추가 자료

### 외부 참조
- **Fullsnes**: https://problemkaputt.de/fullsnes.htm
- **SPC700 Instruction Set**: http://www.romhacking.net/documents/226/
- **SNES Dev Wiki**: https://wiki.superfamicom.org/

### 프로젝트 문서
- `README.md` - 본 문서
- `docs/AGENT_DEVELOPMENT_GUIDE.md` - 종합 개발 가이드
- `docs/QUICK_REFERENCE.md` - 빠른 참조
- `APU_COMPONENTS.md` - APU 아키텍처

---

## 🤝 Agent 개발 모범 사례

### DO ✅
1. **항상 타임아웃 설정** - 무한 루프 방지
2. **로그 백업** - 이전 실행 로그 유지
3. **증분 테스트** - 한 번에 하나씩 수정
4. **문서 참조** - `docs/` 디렉토리 활용
5. **스크립트 사용** - PowerShell 도구 활용

### DON'T ❌
1. **GUI 디버거 의존** - Agent는 사용 불가
2. **전체 로그 읽기** - `-Tail` 사용
3. **여러 변경 동시** - 원인 분석 어려움
4. **문서 없이 추측** - 명세 확인
5. **무한 대기** - 항상 타임아웃 설정

---

## 📞 Agent 지원

### Agent가 막혔을 때
1. `docs/QUICK_REFERENCE.md` - 빠른 해결책
2. `docs/AGENT_DEVELOPMENT_GUIDE.md` - 상세 가이드
3. `analyze_loop.ps1` - 자동 분석
4. `Get-Content apu_trace.log -Tail 100` - 최근 로그

### 버그 패턴 확인
1. `docs/QUICK_REFERENCE.md` - 자주 발생하는 버그
2. `docs/test_roms/spctest_expected.md` - 알려진 문제
3. `docs/hardware/spc700_instructions.md` - 명령어 스펙

---

**프로젝트**: SNES 에뮬레이터  
**버전**: 1.0  
**날짜**: 2025-12-14  
**대상**: AI Agent 개발자

**현재 목표**: spctest.sfc 통과 → SPC700 완전 검증










