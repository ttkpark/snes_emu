---
name: system-debug
description: "SNES 시스템 통합 디버깅 스킬. 메모리 맵(LoROM/HiROM/ExHiROM) 라우팅, DMA/HDMA 전송 구현, I/O 레지스터 라우팅, 마스터 클럭 타이밍, CPU/PPU/APU 동기화, NMI/IRQ 전달, WRAM 관리. DMA 버그, 메모리 접근 오류, 타이밍 동기화 문제, I/O 라우팅 오류가 있으면 반드시 이 스킬을 사용할 것."
---

# System Debug — 시스템 통합 디버깅 방법론

메모리 맵, DMA, 타이밍, I/O 라우팅 버그를 찾아 수정하는 체계적 절차.

## Step 1: 증상 분류

| 증상 | 원인 후보 |
|-----|---------|
| 잘못된 주소 읽기/쓰기 | 메모리 라우팅 로직 |
| DMA 전송 후 데이터 없음 | DMA 채널 설정, 방향, 카운트 |
| HDMA 효과 없음 | HDMA 활성화, 엔트리 포맷 |
| CPU가 APU를 못 읽음 | 포트 라우팅 ($2140-$2143) |
| NMI 발생 안 함 | V-blank 감지, NMIEN 비트 |
| 타이밍 불일치 | 마스터 클럭 비율 |

## Step 2: 메모리 맵 라우팅

**소스:** `src/memory/memory.cpp`
**참조:** `docs/hardware/mem_memory_map_complete.md`

### LoROM 주소 변환

```cpp
// LoROM: 뱅크 $00-$7D, $80-$FF
// 주소 $8000-$FFFF → ROM
// 주소 $0000-$7FFF → WRAM/레지스터
uint32_t romAddr = ((addr >> 16) & 0x7F) * 0x8000 + (addr & 0x7FFF);

// I/O 레지스터 범위 (모든 뱅크에서 미러)
// $2100-$21FF: PPU
// $2140-$2143: APU 포트 (CPU측)
// $4200-$43FF: CPU 내부 레지스터 (NMI, DMA 등)
// $4016-$4017: 조이패드
```

### HiROM 주소 변환

```cpp
// HiROM: 뱅크 $C0-$FF에 전체 ROM
// 뱅크 $00-$3F, $80-$BF: $8000-$FFFF = ROM 하위 32KB
uint32_t romAddr = (addr & 0x3FFFFF);
```

### 라우팅 우선순위

```
1. I/O 레지스터 ($2100-$21FF, $2140-$21FF, $4200-$43FF)
2. WRAM ($7E0000-$7FFFFF)
3. SRAM (SaveRAM, 뱅크/주소별 매핑)
4. ROM
```

## Step 3: DMA 디버깅

**레지스터 ($4300-$43FF, 채널 0 기준):**

```
$4300: DMAPx — DMA 파라미터
  bit 7: 전송 방향 (0=A→B, 1=B→A)
  bit 6: HDMA Indirect
  bit 5: A 주소 감소
  bit 4: A 주소 고정
  bit 3: (미사용)
  bit 2-0: 전송 패턴 (0=$xx, 1=$xx/$xx+1, 2=$xx/$xx, ...)

$4301: BBADx — B 버스 주소 (I/O 레지스터 하위 바이트)
$4302-$4304: A1Tx — A 버스 소스 주소 (24비트)
$4305-$4306: DASx — 전송 바이트 수
$4307: A2Ax — HDMA 테이블 주소 (HDMA 전용)
```

**DMA 활성화:** `$420B` — 비트별 채널 활성화

```cpp
// DMA 실행 시 CPU는 정지 (사이클 소비 없음)
// 전송 후 채널 비활성화됨
// HDMA는 매 H-blank에 자동 실행 ($420C로 활성화)
```

### DMA 공통 버그

```cpp
// 방향 혼동: A→B vs B→A
// A 버스 = 메인 메모리 (ROM/RAM)
// B 버스 = I/O 레지스터 ($2100-$21FF)

// VRAM DMA: $2116/$2117로 주소 설정, $2118/$2119로 쓰기
// DASx = 0이면 64KB 전송 (0으로 시작해서 감소)

// 전송 패턴 1 ($2118/$2119 교대): 저/고 바이트 교대 쓰기
```

## Step 4: 타이밍 동기화

**소스:** `src/main_complete.cpp`

```cpp
// 마스터 클럭 기준:
// PPU: 4 마스터 사이클마다 1 PPU 사이클
// CPU: 6 마스터 사이클마다 1 CPU 사이클  
// APU: 24 마스터 사이클마다 1 APU 사이클

// 현재 루프 패턴:
for (int i = 0; i < 4; i++) ppu.tick();     // 4 PPU
for (int i = 0; i < 1; i++) cpu.step();     // 약 6 마스터 = 1 CPU 스텝
for (int i = 0; i < 1; i++) apu.step();     // 약 24 마스터 = 1 APU 스텝
```

### NMI/IRQ 전달

```cpp
// NMI: V-blank 시작 시 발생
// $4210 bit7: V-blank 플래그 (읽으면 클리어)
// $4200 bit7: NMI 활성화 (NMIEN)
// 활성화된 상태에서 V-blank → CPU NMI 핀 High

// NMI 벡터: 네이티브 $FFEA, 에뮬레이션 $FFFA
// IRQ 벡터: 네이티브 $FFEE, 에뮬레이션 $FFFE
```

## Step 5: WRAM 접근

```cpp
// $7E0000-$7FFFFF: 128KB WRAM
// $0000-$1FFF: WRAM 미러 (모든 뱅크)
// $2180: WRAM 데이터 포트
// $2181-$2183: WRAM 주소 ($2181=저바이트, $2182=고바이트, $2183=뱅크비트)
```

## Step 6: 수정 보고 형식

```
## 수정 내용
- 파일: src/memory/memory.cpp (또는 main_complete.cpp)
- 컴포넌트: {메모리 라우팅 / DMA / HDMA / 타이밍 / NMI}
- 수정: {무엇을 어떻게 바꿨는지}
- 이유: {SNES 하드웨어 스펙 어느 부분과 달랐는지}
- 영향받는 컴포넌트: {CPU / PPU / APU 중 영향 있는 것}
```

## 참조 문서

- `docs/hardware/mem_memory_map_complete.md` — 전체 메모리 맵
- `docs/hardware/ppu_registers_bitmap.md` — DMA 관련 레지스터 포함
