---
name: ppu-debug
description: "PPU 그래픽 디버깅 스킬. PPU 렌더링 구현, VRAM/CGRAM 덤프 분석, BG 모드 0-7 구현, 스프라이트/OAM 처리, 스캔라인 렌더링, 윈도우/색상 수학 구현. 화면이 안 나오거나 깨진 그래픽, VRAM 문제, PPU 레지스터 오류가 있으면 반드시 이 스킬을 사용할 것."
---

# PPU Debug — 그래픽 디버깅 방법론

PPU 렌더링 버그를 찾아 수정하는 체계적 절차.

## Step 1: 증상 분류

| 증상 | 원인 후보 |
|-----|---------|
| 화면 전체가 검정 | INIDISP($2100) Force Blank 비트 확인 |
| 타일이 깨짐 | VRAM 주소 매핑, BPP 설정 |
| 색이 이상함 | CGRAM 팔레트 데이터 |
| 배경 없음 | TM/TS 레지스터($212C/$212D), BG 활성화 |
| 스프라이트 없음 | OAM 데이터, OAMADDL($2102) |
| 스크롤 오류 | BG H/VOFS 레지스터 |
| Mode 7 왜곡 | M7A-M7D 행렬 파라미터 |

## Step 2: VRAM/CGRAM 덤프 분석

### VRAM 덤프 읽기

`vram_spctest.txt`, `vram_cputest.txt` 등 덤프 파일 참조:

```
0000: XX XX XX XX ...  ← 타일 캐릭터 데이터 (BPP에 따라 포맷 다름)
8000: XX XX XX XX ...  ← 타일맵 데이터 (2바이트/타일: VHOPPPCC CCCCCCCC)
```

**타일맵 엔트리 형식 (16비트):**
```
Bit 15: V-flip
Bit 14: H-flip
Bit 13: Priority
Bit 12-10: Palette (팔레트 번호)
Bit 9-0: Character (타일 번호)
```

### CGRAM 덤프 읽기

`cgram_spctest.txt` 등 참조. 색상은 BGR555 포맷:
```
Bit 14-10: Blue (0-31)
Bit 9-5:   Green (0-31)
Bit 4-0:   Red (0-31)
```

## Step 3: PPU 레지스터 상태 확인

**소스:** `src/ppu/ppu.cpp`

핵심 레지스터:
```cpp
// $2100: INIDISP — Force Blank + 밝기
// bit 7 = 1이면 화면 강제 블랭크 (검정)
// $2105: BGMODE — BG 모드(0-7) + BG4 타일 크기
// $2107-$210A: BGxSC — BG1-4 타일맵 기준 주소 + 화면 크기
// $210B-$210C: BG12/34NBA — BG1-4 캐릭터 데이터 기준 주소
// $212C: TM — 메인 화면 레이어 활성화 (bit0=BG1, bit4=OBJ)
// $212D: TS — 서브 화면 레이어 활성화
```

## Step 4: 렌더링 로직 확인

### BG 모드별 BPP

| 모드 | BG1 | BG2 | BG3 | BG4 |
|-----|-----|-----|-----|-----|
| 0 | 2bpp | 2bpp | 2bpp | 2bpp |
| 1 | 4bpp | 4bpp | 2bpp | - |
| 2 | 4bpp | 4bpp | offset | - |
| 3 | 8bpp | 4bpp | - | - |
| 4 | 8bpp | 2bpp | offset | - |
| 5 | 4bpp | 2bpp | - | - |
| 6 | 4bpp | offset | - | - |
| 7 | 8bpp(M7) | EXTBG | - | - |

### VRAM 주소 계산

```cpp
// BGxSC 레지스터에서 타일맵 기준 주소 추출
uint16_t tilemapBase = (bgxsc >> 2) << 10;  // 1KB 단위

// 타일 캐릭터 데이터 기준 주소 (BG12NBA/BG34NBA)
// BG1: bits 3-0 * 0x1000 (4KB 단위)
// BG2: bits 7-4 * 0x1000

// VRAM 주소 인크리먼트 모드 ($2115: VMAIN)
// bit 7 = 0: 저바이트 읽기 후 인크리먼트
// bit 7 = 1: 고바이트 읽기 후 인크리먼트
```

### 공통 버그 패턴

```cpp
// CGRAM 쓰기: $2122 두 번 쓰면 16비트 색상 완성
// 첫 번째 쓰기 = 저바이트, 두 번째 = 고바이트
// 내부 플립플롭으로 추적 필요

// VRAM 쓰기: $2118/$2119 (저/고바이트 분리)
// 인크리먼트는 VMAIN 설정 기준으로 저 또는 고바이트 쓰기 후 발생

// OAM: $2104로 쓰기, 짝수 주소에서 저바이트 버퍼링
// $2102/$2103으로 OAM 주소 설정
```

## Step 5: 스캔라인 타이밍

```cpp
// SNES: 262 스캔라인 (NTSC), 312 (PAL)
// 각 라인: 341 도트 (마스터 클럭 기준)
// H-blank: 256-340 도트
// V-blank: 225-261 라인
// NMI: V-blank 시작 시 ($4210 bit7 세트, 활성화 시 CPU NMI)
```

## Step 6: 수정 보고 형식

```
## 수정 내용
- 파일: src/ppu/ppu.cpp
- 레지스터/기능: {레지스터 주소 or 렌더링 함수명}
- 수정: {무엇을 어떻게 바꿨는지}
- 이유: {PPU 스펙 어느 부분과 달랐는지}
- 시각적 기대 결과: {수정 후 화면 변화}
```

## 참조 문서

- `docs/hardware/ppu_registers_bitmap.md` — PPU 레지스터 비트 필드
- `docs/hardware/mem_memory_map_complete.md` — PPU 레지스터 주소 범위
