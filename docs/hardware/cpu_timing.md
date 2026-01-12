# SNES CPU Timing - 65816 사이클 정확도

## 📋 목차
1. [개요](#개요)
2. [Master Clock](#master-clock)
3. [CPU 클럭](#cpu-클럭)
4. [메모리 액세스 타이밍](#메모리-액세스-타이밍)
5. [명령어 사이클 계산](#명령어-사이클-계산)
6. [사이클 페널티](#사이클-페널티)
7. [DMA/HDMA 타이밍](#dmahdma-타이밍)
8. [타이밍 정확도 구현](#타이밍-정확도-구현)
9. [디버깅](#디버깅)

---

## 개요

SNES의 타이밍은 **Master Clock**을 기준으로 모든 컴포넌트가 동기화됩니다. CPU는 일정한 속도로 명령어를 실행하며, 메모리 액세스 속도에 따라 실제 사이클이 결정됩니다.

### 핵심 개념

| 개념 | 값 |
|------|------|
| **Master Clock** | 21.47727 MHz (~21.477 MHz) |
| **CPU Clock** | Master Clock / 6, 8, 12 (속도에 따라) |
| **1 Master Cycle** | ~46.6 ns |
| **Fast Cycle** | 6 master cycles (~280 ns) |
| **Slow Cycle** | 8 master cycles (~373 ns) |
| **Extra Slow** | 12 master cycles (~559 ns) |

---

## Master Clock

### 클럭 주파수

**Master Clock**: 21.47727 MHz (정확히 21477272.727... Hz)

```
1 Master Cycle = 1 / 21477272.727 Hz
               ≈ 46.56 ns
```

**유래**:
```
NTSC 컬러 버스트 주파수 × 6
= 3.579545 MHz × 6
= 21.47727 MHz
```

### 컴포넌트 클럭 분주

```
Master Clock (21.477 MHz)
├─ CPU:  Master / 6, 8, 12 (메모리 속도에 따라)
├─ PPU:  Master / 4 (5.369 MHz)
├─ APU:  Master / 24 (1.024 MHz, SPC700)
└─ DSP:  Master / 32 (32 kHz 샘플링)
```

---

## CPU 클럭

### 클럭 속도

CPU의 실제 동작 속도는 **메모리 액세스 속도**에 따라 결정됩니다.

```
Fast (FastROM):        6 master cycles  = ~3.58 MHz
Slow (SlowROM, 기본):  8 master cycles  = ~2.68 MHz
Extra Slow (WRAM):    12 master cycles  = ~1.79 MHz
```

### FastROM vs SlowROM

**SlowROM** (기본):
```
뱅크 $00-$3F, $80-$BF: 8 master cycles
뱅크 $40-$7F, $C0-$FF: 8 master cycles (로 설정된 경우)
```

**FastROM**:
```
뱅크 $80-$FF: 6 master cycles (ROM 액세스)
```

**FastROM 활성화**:
```asm
LDA #$01
STA $420D    ; MEMSEL: FastROM 활성화
```

**C++ 구현**:
```cpp
class Memory {
private:
    bool fastROM = false;
    
public:
    void writeMemsel(uint8_t value) {
        fastROM = (value & 0x01) != 0;
    }
    
    int getAccessCycles(uint32_t addr) {
        uint8_t bank = addr >> 16;
        
        // WRAM ($7E, $7F)
        if (bank == 0x7E || bank == 0x7F) {
            return 12;  // Extra Slow
        }
        
        // Fast/Slow ROM
        if (bank >= 0x80 && fastROM) {
            return 6;   // Fast
        }
        
        return 8;       // Slow (기본)
    }
};
```

---

## 메모리 액세스 타이밍

### 메모리 영역별 속도

| 주소 범위 | 영역 | Master Cycles | CPU Cycles |
|-----------|------|---------------|------------|
| $00-$3F:$0000-$1FFF | Low RAM (Mirror) | 12 | Slow+4 |
| $00-$3F:$2000-$20FF | PPU (B-Bus) | 6 | Fast |
| $00-$3F:$2100-$21FF | PPU (B-Bus) | 6 | Fast |
| $00-$3F:$4000-$41FF | Internal CPU | 6 | Fast |
| $00-$3F:$4200-$44FF | Internal CPU | 6 | Fast |
| $00-$3F:$8000-$FFFF | ROM (SlowROM) | 8 | Slow |
| $40-$7F:$0000-$FFFF | ROM (SlowROM) | 8 | Slow |
| $7E-$7F:$0000-$FFFF | WRAM | 12 | Extra Slow |
| $80-$BF:$8000-$FFFF | ROM (FastROM) | 6 | Fast |
| $C0-$FF:$0000-$FFFF | ROM (FastROM) | 6 | Fast |

### B-Bus (PPU/APU)

**B-Bus 액세스**: 항상 **6 master cycles** (Fast)

```
$2100-$213F: PPU 레지스터
$2140-$217F: APU 포트
```

**특징**:
- CPU와 독립적인 버스
- 항상 빠른 액세스
- 쓰기 후 즉시 읽을 수 없음 (1 사이클 지연)

---

## 명령어 사이클 계산

### 기본 사이클

각 명령어는 **기본 사이클 수**를 가집니다. 실제 사이클은 여기에 페널티를 더합니다.

```
실제 사이클 = 기본 사이클 + 페널티
```

**예: LDA Immediate**
```
LDA #$42    ; Opcode: $A9
기본 사이클: 2

m=0 (16비트): 3 사이클
m=1 (8비트):  2 사이클
```

### 사이클 카운트 테이블

```cpp
// Opcode별 기본 사이클 (8비트 모드 기준)
static const int baseCycles[256] = {
    // 0x00-0x0F
    7, 6, 7, 4, 5, 3, 5, 6, 3, 2, 2, 4, 6, 4, 6, 5,
    // 0x10-0x1F
    2, 5, 5, 7, 5, 4, 6, 6, 2, 4, 2, 2, 6, 4, 7, 5,
    // ... (나머지 opcode)
};
```

---

## 사이클 페널티

### 1. 16비트 모드 페널티

**m=0 (16비트 Accumulator)**:
```
LDA, STA, ADC, SBC 등 Accumulator 연산: +1 사이클
```

**x=0 (16비트 Index)**:
```
LDX, LDY, STX, STY 등 Index 연산: +1 사이클
```

**예**:
```cpp
int cycles = baseCycles[opcode];

if (isAccumulatorOp(opcode) && !m_flag) {
    cycles += 1;  // 16비트 Accumulator
}

if (isIndexOp(opcode) && !x_flag) {
    cycles += 1;  // 16비트 Index
}
```

### 2. Direct Page 페널티

**Direct Page Register (D) ≠ $0000**:
```
Direct Page 주소 모드: +1 사이클
```

**예**:
```
LDA $10     ; Direct Page
D = $0000:  3 사이클
D = $1200:  4 사이클 (+1)
```

**구현**:
```cpp
if (isDirectPageMode(opcode) && (D & 0xFF) != 0) {
    cycles += 1;
}
```

### 3. 페이지 경계 페널티

**주소 계산 중 페이지 경계를 넘으면**: +1 사이클

**해당 모드**:
- Absolute,X / Absolute,Y
- (Direct),Y
- (Direct,X)

**예**:
```
LDA $20FF,X   ; X = $02
주소 = $2101  (페이지 경계 넘음: $20 → $21)
→ +1 사이클
```

**구현**:
```cpp
uint16_t baseAddr = readWord(PC + 1);
uint16_t effectiveAddr = baseAddr + X;

// 페이지 경계 체크
if ((baseAddr & 0xFF00) != (effectiveAddr & 0xFF00)) {
    cycles += 1;
}
```

### 4. Branch Taken 페널티

**조건 분기가 성공하면**: +1 사이클  
**페이지 경계를 넘으면**: +2 사이클

```cpp
if (branchTaken) {
    cycles += 1;
    
    uint16_t targetAddr = PC + offset;
    if ((PC & 0xFF00) != (targetAddr & 0xFF00)) {
        cycles += 1;  // 페이지 경계
    }
}
```

### 5. Native vs Emulation 모드

**Emulation 모드 (6502 호환)**:
- Stack은 항상 $0100-$01FF
- 일부 명령어 동작 변경

---

## DMA/HDMA 타이밍

### DMA 사이클

**기본 오버헤드**: 8 master cycles (CPU 정지)

**전송 사이클**:
```
1바이트 전송: 8 master cycles

총 사이클 = 8 (오버헤드) + (바이트 수 × 8)
```

**예**:
```
DMA로 1024바이트 전송
= 8 + (1024 × 8)
= 8200 master cycles
= ~382 μs
```

### HDMA 사이클

**스캔라인당**:
```
Direct 모드:   18 master cycles
Indirect 모드: 24 master cycles
```

**8개 채널 모두 활성화**:
```
최대 = 24 × 8 = 192 master cycles/scanline
```

---

## 타이밍 정확도 구현

### Cycle-Accurate 에뮬레이션

```cpp
class CPU {
private:
    int cyclesExecuted = 0;
    int cyclesTarget = 0;
    
public:
    void runFrame() {
        cyclesTarget = CYCLES_PER_FRAME;  // ~89342 사이클/프레임
        
        while (cyclesExecuted < cyclesTarget) {
            int cycles = executeInstruction();
            cyclesExecuted += cycles;
            
            // 다른 컴포넌트 동기화
            ppu.run(cycles);
            apu.run(cycles);
        }
        
        cyclesExecuted -= cyclesTarget;
    }
    
    int executeInstruction() {
        uint8_t opcode = read(PC++);
        int cycles = baseCycles[opcode];
        
        // 명령어 실행
        executeOpcode(opcode);
        
        // 페널티 계산
        cycles += calculatePenalties(opcode);
        
        // 메모리 액세스 타이밍
        cycles = convertToMasterCycles(cycles);
        
        return cycles;
    }
    
    int convertToMasterCycles(int cpuCycles) {
        // CPU 사이클 → Master 사이클
        return cpuCycles * memoryAccessSpeed;  // 6, 8, 12
    }
};
```

### 프레임당 사이클

```
NTSC (60 Hz):
- 총 스캔라인: 262
- 스캔라인당 사이클: 1364 master cycles
- 프레임당 사이클: 357368 master cycles
- CPU 사이클 (SlowROM): ~44671 사이클

PAL (50 Hz):
- 총 스캔라인: 312
- 프레임당 사이클: 425568 master cycles
```

---

## 디버깅

### 사이클 로그

```cpp
void logCycles() {
    printf("[Cycle %lld] PC:%04X Opcode:%02X Cycles:%d\n",
           totalCycles, PC, opcode, lastCycles);
}
```

### 타이밍 검증

```cpp
class TimingValidator {
private:
    int expectedCycles = 0;
    int actualCycles = 0;
    
public:
    void validate() {
        if (actualCycles != expectedCycles) {
            printf("Timing mismatch: Expected %d, Got %d\n",
                   expectedCycles, actualCycles);
        }
    }
};
```

### 사이클 카운터

```cpp
struct CycleStats {
    uint64_t totalCycles = 0;
    uint64_t cpuCycles = 0;
    uint64_t dmaCycles = 0;
    uint64_t idleCycles = 0;
    
    void print() {
        printf("CPU:  %lld (%.1f%%)\n", cpuCycles, 
               100.0 * cpuCycles / totalCycles);
        printf("DMA:  %lld (%.1f%%)\n", dmaCycles,
               100.0 * dmaCycles / totalCycles);
        printf("Idle: %lld (%.1f%%)\n", idleCycles,
               100.0 * idleCycles / totalCycles);
    }
};
```

---

## 최적화

### 1. 사이클 룩업 테이블

```cpp
// 미리 계산된 사이클 테이블
struct CycleTable {
    int cycles[256][4];  // [opcode][m/x 조합]
    
    void init() {
        for (int op = 0; op < 256; op++) {
            for (int flags = 0; flags < 4; flags++) {
                bool m = (flags & 1);
                bool x = (flags & 2);
                cycles[op][flags] = calculateCycles(op, m, x);
            }
        }
    }
    
    int get(uint8_t opcode, bool m, bool x) {
        int flags = (m ? 1 : 0) | (x ? 2 : 0);
        return cycles[opcode][flags];
    }
};
```

### 2. 명령어 캐싱

```cpp
// 자주 실행되는 명령어 최적화
if (opcode == 0xEA) {  // NOP
    cycles = 2;
    return cycles;
}
```

---

## 참고 자료

- [SnesLab - Timing](https://sneslab.net/wiki/Timing)
- [Fullsnes - CPU Timing](https://problemkaputt.de/fullsnes.htm#cpumemoryandio)
- [65816 Datasheet](http://6502.org/documents/datasheets/wdc/wdc_w65c816s_datasheet.pdf)

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete










