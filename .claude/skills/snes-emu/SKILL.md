---
name: snes-emu
description: "SNES 에뮬레이터 개발 오케스트레이터. SPC700/65c816 CPU 버그 수정, PPU 렌더링 구현, DMA/HDMA, 시스템 통합, Super Mario World 실행 등 모든 SNES 에뮬레이터 작업을 조율한다. spctest.sfc 실패, cputest 실패, opcode 버그, 트레이스 로그 분석, 렌더링 문제, 메모리 맵, 타이밍 동기화 요청 시 반드시 이 스킬을 사용할 것. '다시 실행', '재테스트', '수정 후 검증', '블록 통과', '마일스톤' 등 후속 작업도 포함."
---

# SNES Emulator Orchestrator

SNES 에뮬레이터의 버그 수정, 기능 구현, 테스트 검증을 전문가 팀으로 조율하는 오케스트레이터.

**실행 모드:** 서브 에이전트 파이프라인 (전문가 수정 → QA 검증)

## 에이전트 팀

| 에이전트 | subagent_type | 담당 영역 |
|---------|---------------|---------|
| APU Specialist | `apu-specialist` | SPC700, spctest.sfc, 오디오 |
| CPU Specialist | `cpu-specialist` | 65c816, cputest, 인터럽트 |
| PPU Specialist | `ppu-specialist` | 렌더링, VRAM, BG 모드 |
| System Specialist | `system-specialist` | 메모리 맵, DMA, 타이밍 |
| QA Tester | `qa-tester` | 빌드, 테스트 실행, 로그 분석 |

## Phase 1: 컨텍스트 확인

작업 시작 전 현재 상태를 파악한다.

1. `stdout_spctest.txt`, `stderr_spctest.txt` — 마지막 spctest 실행 결과 확인
2. `stdout_cputest3.txt`, `stderr_cputest3.txt` — 마지막 cputest 결과 확인
3. 사용자 요청에서 **컴포넌트** 식별: APU / CPU / PPU / System
4. 기존 로그가 없으면 QA Tester에게 먼저 테스트 실행을 요청

**실행 모드 결정:**
- 사용자가 특정 버그를 보고 → 해당 Specialist 직접 호출
- 현재 상태 불명확 → QA Tester 먼저 호출하여 현황 파악
- 후속/부분 수정 요청 → 해당 컴포넌트 Specialist만 재호출

## Phase 2: 라우팅 규칙

사용자 요청을 아래 기준으로 라우팅한다.

| 키워드/증상 | 호출 에이전트 |
|-----------|------------|
| SPC700, spctest, APU, opcode(SPC), 타이머($FA-$FF), 포트 통신 | apu-specialist |
| 65c816, cputest, NMI, IRQ, 인터럽트, CPU opcode | cpu-specialist |
| PPU, 렌더링, VRAM, CGRAM, BG 모드, 스프라이트, 화면 | ppu-specialist |
| DMA, HDMA, 메모리 맵, LoROM/HiROM, 타이밍 동기화, WRAM | system-specialist |
| 빌드 실패, 테스트 결과 확인, 로그 분석 | qa-tester |

**복합 이슈** (예: CPU↔APU 포트 통신): 주 증상 기준으로 1차 specialist 호출 → QA 결과에서 추가 이슈 발견 시 2차 specialist 호출.

## Phase 3: Specialist 호출

해당 specialist를 서브 에이전트로 호출한다.

```
Agent(
  subagent_type: "apu-specialist",  # 또는 cpu/ppu/system
  model: "opus",
  prompt: """
    [작업 요청]
    {사용자가 보고한 증상 전달}
    
    [현재 상태]
    - 마지막 테스트 결과: {stdout/stderr 파일 내용 요약}
    - 실패 지점: {트레이스 로그에서 찾은 실패 opcode/주소}
    
    [요청]
    버그를 수정하고 수정 내용과 이유를 상세히 보고하라.
    빌드는 하지 말고 코드 수정만 완료하라.
  """
)
```

Specialist가 코드를 수정하면 Phase 4로 진행.

## Phase 4: QA 검증

QA Tester를 서브 에이전트로 호출한다.

```
Agent(
  subagent_type: "qa-tester",
  model: "opus",
  prompt: """
    [요청]
    수정된 코드를 빌드하고 {spctest.sfc / cputest / smw} 테스트를 실행하라.
    
    [판정 기준]
    - spctest.sfc: Port 0 = 0xFF, PC 0x0350 루프 = 성공
    - cputest: stdout의 PASS/FAIL 확인
    
    [보고 형식]
    1. 빌드 결과 (성공/실패)
    2. 테스트 결과 (성공/실패 + 실패 시 테스트 번호)
    3. 트레이스 로그 발췌 (실패 전 30줄)
    4. 추정 원인
  """
)
```

## Phase 5: 결과 보고

사용자에게 최종 결과를 보고한다.

```
## 결과 요약
- 수정 내용: {Specialist 보고 요약}
- 테스트 결과: {성공 / 실패 - 실패 테스트 번호}
- 다음 조치: {QA 추정 원인 기반 제안}
```

**성공 시**: 현재 마일스톤 진행 상황 업데이트 (아래 참조)
**실패 시**: 실패 원인을 Phase 3에 피드백하여 재호출

## 마일스톤 추적

| 마일스톤 | 상태 | 완료 기준 |
|---------|-----|---------|
| SPC700 전체 테스트 | 진행 중 (블록 3 0xF6 실패) | spctest.sfc Port 0 = 0xFF |
| PPU 렌더링 | 미시작 | 배경 + 스프라이트 표시 |
| DMA/HDMA | 부분 구현 | 범용 DMA + HDMA 완료 |
| 시스템 통합 | 부분 구현 | NMI/IRQ + 타이밍 동기화 |
| Super Mario World | 미시작 | 타이틀 화면 표시 |

## 에러 핸들링

- **빌드 실패**: QA가 에러 라인 보고 → 해당 Specialist에게 컴파일 에러 전달
- **테스트 크래시**: stderr 분석 → System Specialist 또는 해당 컴포넌트 Specialist 호출
- **무한 루프**: 일정 사이클 후 종료, 마지막 PC 위치 기반 Specialist 라우팅
- **복합 버그**: 1차 수정 후 QA 재실행, 새 실패 지점으로 2차 Specialist 호출

## 테스트 시나리오

**정상 흐름**: "spctest.sfc 블록 3 테스트 0xF6 실패"
1. stdout_spctest.txt 확인 → Port 2 = 0xF6 확인
2. apu-specialist 호출 → opcode 0xF6 관련 버그 수정
3. qa-tester 호출 → 빌드 + spctest.sfc 실행
4. 성공 시 마일스톤 업데이트

**에러 흐름**: 빌드 실패
1. qa-tester가 컴파일 에러 라인 보고
2. 에러 파일(cpu.cpp/apu.cpp 등) 기반으로 해당 Specialist 재호출
3. 컴파일 에러 수정 후 QA 재실행
