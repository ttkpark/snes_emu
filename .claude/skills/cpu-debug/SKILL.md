---
name: cpu-debug
description: "65c816 CPU 디버깅 스킬. cputest 실패 분석, 65c816 opcode 버그 수정, cpu_trace.log 분석, NMI/IRQ 인터럽트 처리, M/X 플래그 모드 전환 구현. CPU 관련 버그, cputest 실패, 인터럽트 문제, 어드레싱 모드 오류가 있으면 반드시 이 스킬을 사용할 것."
---

# CPU Debug — 65c816 디버깅 방법론

65c816 CPU 버그를 찾아 수정하는 체계적 절차.

## Step 1: 실패 지점 식별

### cputest 실패 분석

```bash
# 테스트 결과 확인
cat stdout_cputest3.txt | tail -30

# CPU 트레이스에서 실패 직전 상태
grep -an "FAIL\|fail\|ERROR" cpu_trace.log | tail -10
```

**실패 판정:**
- stdout에 `FAIL` 또는 `ERROR` 출력된 테스트 번호 확인
- cpu_trace.log에서 해당 테스트 opcode가 실행된 지점 추적

### Super Mario World 부팅 실패 분석

```bash
# 초기화 시퀀스 추적
head -200 cpu_trace.log

# 리셋 벡터 확인 ($FFFC-$FFFD)
grep -a "PC:0xfffc\|RESET\|reset" cpu_trace.log | head -5
```

## Step 2: 하드웨어 스펙 대조

**참조 파일:** `docs/hardware/cpu_65816_opcodes.md`

각 opcode에 대해 확인할 항목:
1. **모드별 사이클 수** — 8비트(M=1) vs 16비트(M=0)
2. **영향받는 플래그** — N, V, M, X, D, I, Z, C
3. **어드레싱 모드** — 바이트 수 계산
4. **에뮬레이션 모드** — 65C02 호환성 차이

## Step 3: 소스 코드 확인

**소스:** `src/cpu/cpu.cpp`

`executeInstruction()` 또는 `step()` 함수에서 해당 opcode case를 찾는다.

### 공통 버그 패턴

**M/X 플래그 모드 전환 누락**
```cpp
// A 레지스터 크기: M=1이면 8비트, M=0이면 16비트
uint16_t val = (m_regs.p & FLAG_M) ? fetch8() : fetch16();

// X 레지스터 크기: X=1이면 8비트, X=0이면 16비트
// X=1로 설정 시 XH, YH는 0으로 클리어됨
```

**Direct Page 기준 주소**
```cpp
// DP 레지스터가 0이 아닐 수 있음
uint32_t dpAddr = m_regs.dp + (uint8_t)m_regs.pc++;
// 에뮬레이션 모드에서 DP가 0이면 제로 페이지 래핑 있음
```

**16비트 오버플로우/언더플로우**
```cpp
// ADC: 16비트 모드에서 캐리 체크
uint32_t result = (uint32_t)a + (uint32_t)val + carry;
m_regs.p = (result > 0xFFFF) ? m_regs.p | FLAG_C : m_regs.p & ~FLAG_C;
// V 플래그: ~(a^val) & (a^result) & 0x8000
```

**인터럽트 벡터 주소**
```cpp
// 네이티브 모드 (E=0)
// NMI: $FFEA-$FFEB
// IRQ: $FFEE-$FFEF
// BRK: $FFE6-$FFE7
// COP: $FFE4-$FFE5

// 에뮬레이션 모드 (E=1)
// NMI: $FFFA-$FFFB
// IRQ/BRK: $FFFE-$FFFF
// COP: $FFF4-$FFF5
```

**BRK vs IRQ 구분**
```cpp
// BRK는 B 플래그를 스택에 세트하고 푸시, IRQ는 클리어
// 에뮬레이션 모드에서 BRK/IRQ 벡터가 같음 (B 플래그로 구분)
```

**PEI/PEA/PHD 등 스택 연산**
```cpp
// PHD: DP 레지스터 전체를 푸시 (16비트)
// PLD: DP 레지스터 풀 (16비트)
// 스택은 항상 뱅크 0
```

## Step 4: 수정 및 검증

1. 스펙과 다른 부분을 수정한다
2. 같은 어드레싱 모드 변형들을 함께 확인한다
   - ADC 수정 → SBC, CMP의 플래그 처리도 점검
   - Direct Page 수정 → Direct Page Indexed, Indirect도 확인
3. 수정 완료 후 QA Tester에게 빌드+cputest 요청

## Step 5: 수정 보고 형식

```
## 수정 내용
- 파일: src/cpu/cpu.cpp
- opcode: 0xXX ({명령어 이름})
- 모드: {에뮬레이션/네이티브, 8비트/16비트}
- 수정: {무엇을 어떻게 바꿨는지}
- 이유: {65816 스펙 어느 부분과 달랐는지}
- 추가 확인: {같은 패턴의 다른 variants}
```

## 참조 문서

- `docs/hardware/cpu_65816_opcodes.md` — opcode 레퍼런스
- `docs/hardware/mem_memory_map_complete.md` — 벡터 주소, 메모리 맵
