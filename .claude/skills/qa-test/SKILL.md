---
name: qa-test
description: "SNES 에뮬레이터 QA 테스트 스킬. build_complete.bat 빌드, spctest.sfc/cputest/Super Mario World 실행, apu_trace.log/cpu_trace.log/port_comm.log 분석, 회귀 테스트, 빌드 에러 분석. '빌드하고 테스트', '빌드 후 확인', '현재 상태 확인', '회귀 테스트' 요청 시 반드시 이 스킬을 사용할 것."
---

# QA Test — 빌드·테스트·검증 방법론

SNES 에뮬레이터의 빌드, 테스트 실행, 결과 분석을 수행하는 체계적 절차.

## Step 1: 빌드

```bash
./build_complete.bat
```

**빌드 성공 판정:** 마지막 줄에 에러 없이 `.exe` 생성
**빌드 실패 처리:**
1. stderr에서 에러 파일과 라인 번호 추출
2. 에러 유형 분류:
   - `error C2...`: 문법/타입 에러 → 해당 파일의 specialist에게 보고
   - `LNK...`: 링크 에러 → 미구현 함수 또는 중복 정의
3. 에러 메시지 전문을 specialist에게 전달

## Step 2: 테스트 ROM 실행

### spctest.sfc (SPC700 명령어 테스트)

```bash
./snes_emu_complete.exe spctest.sfc > stdout_spctest.txt 2> stderr_spctest.txt
```

**성공 판정:** stdout에서 Port 0 = 0xFF, PC 0x0350 루프 확인
**실패 판정:** Port 2 값이 실패한 테스트 번호

```bash
# Port 2 마지막 기록값으로 실패 테스트 번호 확인
tail -100 stderr_spctest.txt | grep -i "port.*2\|p2="

# VRAM 덤프 확인 (spctest.sfc 결과가 VRAM에 기록되는 경우)
cat vram_spctest.txt
```

### cputest (CPU 명령어 테스트)

```bash
./snes_emu_complete.exe cputest > stdout_cputest.txt 2> stderr_cputest.txt
```

**성공 판정:** stdout에 `PASS` 또는 숫자 증가 패턴
**실패 판정:** stdout에 `FAIL` 또는 특정 테스트 번호에서 멈춤

### Super Mario World

```bash
./snes_emu_complete.exe "Super Mario World (Europe) (Rev 1).sfc" > stdout_smw.txt 2> stderr_smw.txt
```

**성공 판정:** 타이틀 화면 표시, 크래시 없이 일정 시간 실행

## Step 3: 트레이스 로그 분석

### 실패 지점 역추적 (공통 절차)

```bash
# 1. 로그 끝부분에서 마지막 이상 동작 확인
tail -100 apu_trace.log

# 2. 실패 진입 지점 찾기 (spctest: PC 0x033C)
grep -n "PC:0x033c" apu_trace.log | head -3
# 출력: 12345:PC:0x033c ...

# 3. 실패 직전 30줄 추출 (LINENUM에 위 번호 - 30 입력)
sed -n '12315,12345p' apu_trace.log

# 4. 실패 테스트 번호 → 해당 opcode 매핑
# Port 2 기록 찾기
grep -a "write.*port.*2\|PORT2.*=" apu_trace.log | tail -5
```

### 로그 파일 목록

| 로그 파일 | 내용 | 관련 컴포넌트 |
|---------|-----|------------|
| `apu_trace.log` | SPC700 명령어 실행 (PC, opcode, A/X/Y/SP/PSW) | APU |
| `cpu_trace.log` | 65c816 명령어 실행 (PC, opcode, 레지스터) | CPU |
| `port_comm.log` | CPU↔APU 포트 0-3 통신 기록 | System |
| `stderr_*.txt` | 에러 출력 (빌드 에러 포함) | 전체 |
| `stdout_*.txt` | 표준 출력 (테스트 결과) | 전체 |
| `vram_*.txt` | VRAM 덤프 | PPU |
| `cgram_*.txt` | CGRAM 팔레트 덤프 | PPU |

## Step 4: 결과 보고 형식

```
## QA 결과 보고

### 빌드: [성공 / 실패]
{실패 시: 에러 파일:라인 + 에러 메시지}

### 테스트: [성공 / 실패]
- ROM: {spctest.sfc / cputest / SMW}
- 판정: {성공 조건 충족 여부}
- 실패 지점: {테스트 번호 / PC 주소}

### 트레이스 발췌 (실패 전 20줄)
```
{로그 발췌}
```

### 추정 원인
- {실패 opcode 또는 레지스터 상태}

### 다음 조치 제안
- {해당 Specialist에게 전달할 구체적 수정 포인트}
```

## Step 5: 회귀 테스트

수정 후 기존 통과했던 테스트들이 여전히 통과하는지 확인한다:

1. spctest.sfc 전체 블록 1-5 순차 통과 여부
2. 이전에 기록된 stdout_spctest.txt와 새 결과 비교
3. 새로 실패하는 테스트가 있으면 해당 Specialist에게 즉시 보고

```bash
# 이전 결과와 현재 결과 비교
diff stdout_spctest.txt stdout_spctest_prev.txt
```

## 빌드 주의사항

- **MSVC(`cl`) 빌드** — CMakeLists.txt 사용 금지, `build_complete.bat` 사용
- **빌드 전 체크**: `.obj` 파일이 오래된 경우 강제 재빌드 필요
- **SDL2 의존성**: `SDL2.dll`이 실행 디렉토리에 있어야 실행 가능
