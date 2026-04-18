---
name: apu-debug
description: "SPC700 APU 디버깅 스킬. spctest.sfc 실패 분석, SPC700 opcode 버그 수정, apu_trace.log 분석, 포트 통신 검증, 타이머/DSP 레지스터 구현을 수행. APU 또는 SPC700 관련 버그, spctest 테스트 블록 실패, 오디오 문제, IPL ROM 부팅 문제가 있으면 반드시 이 스킬을 사용할 것."
---

# APU Debug — SPC700 디버깅 방법론

SPC700 버그를 찾아 수정하는 체계적 절차.

## Step 1: 실패 지점 식별

### spctest.sfc 실패 분석

```bash
# 마지막 포트 2 기록으로 실패 테스트 번호 확인
grep -a "port.*2.*write\|PORT.*2\|p2=" apu_trace.log | tail -10

# PC 0x033C = 실패 진입, PC 0x0350 = 성공 루프
grep -an "PC:0x033c\|PC:0x0350\|PC:0x0357" apu_trace.log | tail -5

# 실패 직전 실행 흐름 (LINENUM = 0x033c가 나온 줄번호)
# 앞 50줄이 핵심
```

**실패 판정:**
- `Port 2 = 0xXX` → 테스트 번호 0xXX에서 실패
- PC 0x033C 진입 = 실패 루틴, 0x0350 = 성공 루프

### 실패 테스트와 opcode 매핑

spctest.sfc 검증 루틴(0x0322):
- A에 실제값 저장, X에 기대값 저장
- 불일치 시 0x033C로 점프, Port 2 = 테스트번호 기록

실패 직전 `MOVW YA,$XX` 또는 `MOV A,$12` 패턴을 찾으면 해당 테스트의 opcode를 알 수 있다.

## Step 2: 하드웨어 스펙 대조

**참조 파일:** `docs/hardware/apu_spc700_instructions.md`

opcode를 확인할 때 반드시 이 파일을 읽고 다음을 확인한다:
1. **바이트 수** — 1바이트 / 2바이트 / 3바이트
2. **플래그 영향** — 어떤 플래그(N, V, H, Z, C)가 변경되는가
3. **연산 순서** — src/dst 피연산자 순서
4. **특수 동작** — dp,dp 패턴, 페이지 래핑, 사이드이펙트

## Step 3: 소스 코드 확인

**소스:** `src/apu/apu.cpp`

`executeSPC700Instruction()` 함수에서 해당 opcode case를 찾는다.

### 공통 버그 패턴

**dp,dp 명령어 (OR/AND/EOR/ADC/SBC/CMP/MOV)**
```cpp
// 잘못된 패턴
uint8_t src = readARAM(m_regs.pc++);
uint8_t dst = readARAM(m_regs.pc++);
uint8_t valSrc = readARAM(src);  // Direct Page 무시

// 올바른 패턴
uint8_t src = readARAM(m_regs.pc++);
uint8_t dst = readARAM(m_regs.pc++);
uint8_t valSrc = readARAM(getDirectPageAddr(src));  // DP 적용
uint8_t valDst = readARAM(getDirectPageAddr(dst));  // DP 적용
// 결과는 dst에 저장: writeARAM(getDirectPageAddr(dst), result)
```

**플래그 처리 실수**
```cpp
// DBNZ, POP X/Y, RETI 등은 플래그를 변경하지 않는다
// updateNZ() 또는 플래그 업데이트 코드가 있으면 제거

// SBC의 H 플래그 (잘못된 ADC 공식 사용 주의)
// SBC H: (a & 0xF) >= (val & 0xF) + borrow
// SBC V: (a^result) & (a^val)  — ADC와 다름

// RETI = pop PSW 먼저, 그 다음 PC
m_regs.psw = popByte();
uint8_t lo = popByte();
uint8_t hi = popByte();
m_regs.pc = (hi << 8) | lo;
```

**I/O 주소 범위 구분**
```cpp
// I/O 레지스터: 0x00F0-0x00FF (16비트 addr 기준)
// 잘못된 체크: addr >= 0xF0  → 0xFFF0도 매칭됨
// 올바른 체크: addr <= 0x00FF && addr >= 0x00F0
```

**PC 증가 누락**
```cpp
// 잘못됨: readARAM(m_regs.pc)
// 올바름: readARAM(m_regs.pc++)
```

## Step 4: 수정 및 검증

1. 스펙과 다른 부분을 수정한다
2. 같은 패턴을 공유하는 다른 opcode도 확인한다
   - dp,dp 수정 → 같은 그룹의 모든 dp,dp opcode 점검
   - 플래그 수정 → 동일 접미사 패턴의 다른 variants 확인
3. 수정 완료 후 QA Tester에게 빌드+테스트 요청

## Step 5: 수정 보고 형식

```
## 수정 내용
- 파일: src/apu/apu.cpp
- opcode: 0xXX ({명령어 이름})
- 수정: {무엇을 어떻게 바꿨는지}
- 이유: {하드웨어 스펙 어느 부분과 달랐는지}
- 추가 확인: {같은 패턴의 다른 opcode 목록}
```

## 참조 문서

- `docs/hardware/apu_spc700_instructions.md` — opcode 레퍼런스
- `docs/hardware/apu_ipl_rom.md` — IPL ROM 부팅 프로토콜
- `docs/hardware/apu_dsp_registers.md` — DSP 레지스터
- `CLAUDE.md` — 기존 수정 이력 (Key bugs fixed 섹션)
