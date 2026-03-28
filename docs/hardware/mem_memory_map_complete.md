# SNES 완전한 메모리 맵

## 개요

SNES의 메모리 맵은 복잡하지만 **시스템 구동의 핵심**입니다. 이 문서는 모든 주소 공간을 상세히 설명합니다.

---

## CPU 주소 공간

65C816 CPU는 **24비트 주소 공간** (16MB)을 가집니다:
- **Bank**: $00-$FF (256개 뱅크)
- **Offset**: $0000-$FFFF (각 뱅크당 64KB)
- **전체**: Bank:Offset 형태 (예: $7E:0000)

---

## LoROM 메모리 맵

LoROM은 가장 일반적인 매핑 방식입니다.

### 뱅크 $00-$3F (Shadow Area)

```
$00-$3F:$0000-$1FFF  - System Area
    $0000-$1FFF      - WRAM (Low 8KB) [Mirror of $7E:0000-$1FFF]

$00-$3F:$2000-$20FF  - Unused
$00-$3F:$2100-$21FF  - PPU Registers (B-Bus)
$00-$3F:$2200-$3FFF  - Unused
$00-$3F:$4000-$41FF  - CPU Registers (Old Style Joypad)
$00-$3F:$4200-$43FF  - CPU Registers (DMA, NMI, IRQ)
$00-$3F:$4400-$5FFF  - Unused
$00-$3F:$6000-$7FFF  - Expansion (rarely used)
$00-$3F:$8000-$FFFF  - ROM (32KB per bank)
```

### 뱅크 $40-$6F (LoROM 상위 영역)

```
$40-$6F:$0000-$7FFF  - Unused (mirrors $00-$3F:$0000-$7FFF)
$40-$6F:$8000-$FFFF  - ROM (32KB per bank, continuation)
```

### 뱅크 $70-$7D (SRAM 영역)

```
$70-$7D:$0000-$7FFF  - SRAM (Battery-backed RAM, if present)
$70-$7D:$8000-$FFFF  - ROM (continuation)
```

### 뱅크 $7E-$7F (WRAM)

```
$7E:$0000-$FFFF      - WRAM (64KB, Bank 0)
$7F:$0000-$FFFF      - WRAM (64KB, Bank 1)
Total: 128KB WRAM
```

### 뱅크 $80-$BF (Mirror of $00-$3F)

```
$80-$BF:$0000-$7FFF  - Mirror of $00-$3F:$0000-$7FFF
$80-$BF:$8000-$FFFF  - ROM (mirrors $00-$3F:$8000-$FFFF)
```

### 뱅크 $C0-$FF (Mirror of $40-$7F)

```
$C0-$EF:$0000-$7FFF  - Mirror of $40-$6F:$0000-$7FFF
$C0-$EF:$8000-$FFFF  - ROM (mirrors $40-$6F:$8000-$FFFF)

$F0-$FD:$0000-$7FFF  - SRAM Mirror
$F0-$FD:$8000-$FFFF  - ROM (mirrors $70-$7D:$8000-$FFFF)

$FE-$FF:$0000-$FFFF  - WRAM Mirror (mirrors $7E-$7F)
```

---

## HiROM 메모리 맵

HiROM은 더 큰 ROM을 위한 매핑입니다.

### 뱅크 $00-$3F (Shadow + ROM)

```
$00-$3F:$0000-$1FFF  - WRAM (Low 8KB) [Mirror]
$00-$3F:$2000-$5FFF  - I/O Registers (same as LoROM)
$00-$3F:$6000-$7FFF  - Expansion
$00-$3F:$8000-$FFFF  - ROM (32KB, first half of banks)
```

### 뱅크 $40-$7D (ROM + SRAM)

```
$40-$7D:$0000-$FFFF  - ROM (64KB per bank, full)
```

SRAM는 특정 뱅크에만:
```
$20-$3F:$6000-$7FFF  - SRAM (8KB per bank)
$A0-$BF:$6000-$7FFF  - SRAM Mirror
```

### 뱅크 $7E-$7F (WRAM)

```
$7E-$7F:$0000-$FFFF  - 128KB WRAM (same as LoROM)
```

### 뱅크 $80-$FF (Mirror)

```
$80-$BF:$0000-$7FFF  - Mirror of $00-$3F:$0000-$7FFF
$80-$BF:$8000-$FFFF  - ROM (mirrors $00-$3F:$8000-$FFFF)
$C0-$FF:$0000-$FFFF  - ROM (mirrors $40-$7F:$0000-$FFFF)
```

---

## I/O 레지스터 상세

### PPU 레지스터 ($2100-$21FF)

```
$2100      - INIDISP   - Screen Display
$2101      - OBSEL     - Object Size & Base
$2102-2103 - OAMADD    - OAM Address
$2104      - OAMDATA   - OAM Data Write
$2105      - BGMODE    - BG Mode & Character Size
$2106      - MOSAIC    - Mosaic Size
$2107-210A - BGxSC     - BG Tilemap Address
$210B-210C - BG12NBA   - BG Character Address
$210D-2114 - BGxHOFS/VOFS - BG Scroll
$2115      - VMAIN     - VRAM Address Increment
$2116-2117 - VMADD     - VRAM Address
$2118-2119 - VMDATA    - VRAM Data
$211A      - M7SEL     - Mode 7 Settings
$211B-211E - M7A-M7D   - Mode 7 Matrix
$211F-2120 - M7X/M7Y   - Mode 7 Center
$2121      - CGADD     - CGRAM Address
$2122      - CGDATA    - CGRAM Data
$2123-2125 - WH0-WH2   - Window Mask Settings
$2126-212A - WH0-WH2   - Window Position
$212B      - WBGLOG    - Window Mask Logic (BG)
$212C      - WOBJLOG   - Window Mask Logic (OBJ)
$212D      - TM        - Main Screen Designation
$212E      - TS        - Sub Screen Designation
$212F      - TMW       - Window Mask Designation (Main)
$2130      - TSW       - Window Mask Designation (Sub)
$2131      - CGWSEL    - Color Math Control
$2132      - CGADSUB   - Color Math Sub-screen
$2133      - SETINI    - Screen Mode/Video Select
```

### APU 레지스터 ($2140-$217F)

```
$2140-2143 - APUIO0-3  - APU I/O Ports (CPU side)
$2144-217F - Unused (mirrors $2140-2143)
```

### CPU 레지스터 ($4200-$43FF)

```
$4200      - NMITIMEN  - Interrupt Enable
$4201      - WRIO      - Programmable I/O Port
$4202      - WRMPYA    - Multiplicand A
$4203      - WRMPYB    - Multiplicand B
$4204-4206 - WRDIV     - Dividend
$4207-4208 - HTIME     - H-Count Timer
$4209-420A - VTIME     - V-Count Timer
$420B      - MDMAEN    - DMA Enable
$420C      - HDMAEN    - HDMA Enable
$420D      - MEMSEL    - ROM Access Speed

$4210      - RDNMI     - NMI Flag & Version
$4211      - TIMEUP    - IRQ Flag
$4212      - HVBJOY    - PPU Status
$4213      - RDIO      - Programmable I/O Port Read
$4214-4215 - RDDIV     - Quotient (Divide Result)
$4216-4217 - RDMPY     - Product (Multiply Result)
$4218-421F - JOY1-4    - Controller Data

$4300-437F - DMA Channels 0-7 (8 bytes each)
    $43x0  - DMAPx     - DMA Control
    $43x1  - BBADx     - DMA Destination (B-bus)
    $43x2-3 - A1TxL/H  - DMA Source Address
    $43x4  - A1Bx      - DMA Source Bank
    $43x5-6 - DASxL/H  - DMA Transfer Size
    $43x7  - DASBx     - HDMA Indirect Bank
```

### Controller 레지스터 ($4016-$4017)

```
$4016      - JOYSER0   - Old Style Joypad 1
$4017      - JOYSER1   - Old Style Joypad 2
```

---

## WRAM 액세스

WRAM은 3가지 방법으로 액세스 가능합니다:

### 1. Direct Bank Access
```asm
LDA $7E0000    ; WRAM Bank 0, Offset 0
LDA $7F8000    ; WRAM Bank 1, Offset $8000
```

### 2. Mirror (Low 8KB)
```asm
LDA $000100    ; Mirrors $7E0100
STA $800200    ; Also mirrors $7E0200
```

### 3. WRAM Port ($2180-$2183)
```asm
; $2181-2183: WRAM Address ($00:0000-$01:FFFF)
LDA #$00
STA $2181      ; Low byte
STA $2182      ; High byte
STA $2183      ; Bank byte

LDA $2180      ; Read from WRAM at set address
STA $2180      ; Write to WRAM at set address
; Address auto-increments
```

---

## SRAM 액세스

### LoROM SRAM
```
$70-$7D:$0000-$7FFF  - 최대 56KB
$F0-$FD:$0000-$7FFF  - Mirror
```

예제:
```asm
; Save data
LDA #$01
STA $700000    ; Write to SRAM

; Load data
LDA $700000    ; Read from SRAM
```

### HiROM SRAM
```
$20-$3F:$6000-$7FFF  - 8KB per bank
$A0-$BF:$6000-$7FFF  - Mirror
```

예제:
```asm
LDA #$42
STA $206000    ; HiROM SRAM write
```

---

## Shadowing (미러링) 규칙

### 뱅크 미러링

#### LoROM
```
$00-$3F ≡ $80-$BF  (시스템 영역 + ROM)
$40-$6F ≡ $C0-$EF  (ROM)
$70-$7D ≡ $F0-$FD  (SRAM + ROM)
$7E-$7F ≡ $FE-$FF  (WRAM)
```

#### HiROM
```
$00-$3F ≡ $80-$BF  (시스템 영역 + ROM)
$40-$7D ≡ $C0-$FF  (ROM)
```

### I/O 미러링

I/O 레지스터는 여러 뱅크에 미러링됩니다:
```
$00-$3F:$2100-$21FF ≡ $80-$BF:$2100-$21FF  (PPU)
$00-$3F:$4000-$43FF ≡ $80-$BF:$4000-$43FF  (CPU)
```

---

## 실제 예제

### 예제 1: 메모리 초기화
```asm
; WRAM 클리어 (DMA 사용)
REP #$20           ; 16-bit A
SEP #$10           ; 8-bit X,Y

LDA #$0000
STA $2181          ; WRAM address = $000000
STZ $2183

LDX #$80
STX $2115          ; VRAM increment = 1

; DMA 설정
LDA #$1809         ; CPU -> PPU, fixed source, byte
STA $4300

LDA #$2180         ; Destination: $2180 (WRAM port)
STA $4301

LDA #$0000         ; Source: $000000 (zero byte)
STA $4302
STZ $4304

LDA #$0000         ; Transfer 64KB
STA $4305

LDX #$01
STX $420B          ; Start DMA channel 0
```

### 예제 2: ROM 데이터 읽기 (LoROM)
```asm
; Bank $80, offset $8000 = 첫 번째 ROM 바이트
LDA $808000        ; Read from ROM

; Bank $00, offset $8000도 동일한 데이터
LDA $008000        ; Same as above (mirror)
```

### 예제 3: HiROM vs LoROM 감지
```asm
; ROM 헤더 위치로 감지
LDA $00FFD5        ; HiROM 헤더 위치
CMP #$21           ; HiROM 식별자?
BEQ is_hirom

LDA $007FD5        ; LoROM 헤더 위치
CMP #$20           ; LoROM 식별자?
BEQ is_lorom
```

---

## 에뮬레이터 구현 가이드

### 메모리 맵 클래스 설계

```cpp
class MemoryMap {
public:
    enum class MapMode {
        LoROM,
        HiROM,
        ExHiROM
    };
    
    MemoryMap(MapMode mode);
    
    uint8_t read(uint32_t address);
    void write(uint32_t address, uint8_t value);
    
private:
    MapMode mode;
    
    // 물리 메모리
    std::vector<uint8_t> rom;        // ROM 데이터
    std::vector<uint8_t> wram;       // 128KB WRAM
    std::vector<uint8_t> sram;       // SRAM (크기 가변)
    
    // I/O
    PPU* ppu;
    APU* apu;
    CPU* cpu;
    
    // 주소 변환
    uint32_t mapAddress(uint32_t cpu_address);
    bool isIO(uint32_t address);
    bool isWRAM(uint32_t address);
    bool isSRAM(uint32_t address);
    bool isROM(uint32_t address);
};
```

### 주소 변환 구현 (LoROM)

```cpp
uint8_t MemoryMap::read(uint32_t address) {
    uint8_t bank = (address >> 16) & 0xFF;
    uint16_t offset = address & 0xFFFF;
    
    // WRAM (Direct)
    if (bank >= 0x7E && bank <= 0x7F) {
        uint32_t wram_addr = ((bank - 0x7E) << 16) | offset;
        return wram[wram_addr];
    }
    
    // WRAM (Mirror - Low 8KB)
    if (offset < 0x2000) {
        return wram[offset];
    }
    
    // I/O Registers
    if (offset >= 0x2100 && offset < 0x2200) {
        return ppu->readRegister(offset);
    }
    if (offset >= 0x2140 && offset < 0x2180) {
        return apu->readRegister(offset);
    }
    if (offset >= 0x4000 && offset < 0x4400) {
        return cpu->readRegister(offset);
    }
    
    // ROM (LoROM: $8000-$FFFF in banks $00-$7D, $80-$FF)
    if (offset >= 0x8000) {
        uint8_t rom_bank = bank & 0x7F;  // Remove mirror bit
        if (rom_bank < 0x7E) {
            uint32_t rom_addr = (rom_bank << 15) | (offset & 0x7FFF);
            if (rom_addr < rom.size()) {
                return rom[rom_addr];
            }
        }
    }
    
    // SRAM (LoROM: $70-$7D:$0000-$7FFF)
    if ((bank >= 0x70 && bank < 0x7E) || 
        (bank >= 0xF0 && bank < 0xFE)) {
        if (offset < 0x8000) {
            uint8_t sram_bank = bank & 0x0F;
            uint32_t sram_addr = (sram_bank << 15) | offset;
            if (sram_addr < sram.size()) {
                return sram[sram_addr];
            }
        }
    }
    
    // Unmapped
    return 0x00;  // Open bus
}
```

### 주소 변환 구현 (HiROM)

```cpp
uint8_t MemoryMap::readHiROM(uint32_t address) {
    uint8_t bank = (address >> 16) & 0xFF;
    uint16_t offset = address & 0xFFFF;
    
    // WRAM ($7E-$7F)
    if (bank >= 0x7E && bank <= 0x7F) {
        uint32_t wram_addr = ((bank - 0x7E) << 16) | offset;
        return wram[wram_addr];
    }
    
    // WRAM Mirror (Low 8KB)
    if (offset < 0x2000) {
        return wram[offset];
    }
    
    // I/O (same as LoROM)
    // ... (동일) ...
    
    // SRAM (HiROM: $20-$3F:$6000-$7FFF, $A0-$BF:$6000-$7FFF)
    if (((bank >= 0x20 && bank < 0x40) || 
         (bank >= 0xA0 && bank < 0xC0)) &&
        offset >= 0x6000 && offset < 0x8000) {
        uint8_t sram_bank = (bank & 0x1F) - 0x20;
        uint32_t sram_addr = (sram_bank << 13) | (offset & 0x1FFF);
        if (sram_addr < sram.size()) {
            return sram[sram_addr];
        }
    }
    
    // ROM (HiROM: Full 64KB banks $00-$3F, $40-$7D, $80-$BF, $C0-$FF)
    uint8_t rom_bank = bank;
    
    // Handle mirrors
    if (bank >= 0x80 && bank < 0xC0) {
        rom_bank = bank & 0x3F;  // $80-$BF -> $00-$3F
    } else if (bank >= 0xC0) {
        rom_bank = (bank & 0x3F) + 0x40;  // $C0-$FF -> $40-$7F
    }
    
    if (rom_bank < 0x7E) {
        uint32_t rom_addr;
        if (rom_bank < 0x40) {
            // $00-$3F: Only $8000-$FFFF is ROM
            if (offset >= 0x8000) {
                rom_addr = (rom_bank << 15) | (offset & 0x7FFF);
            } else {
                return 0x00;  // Not ROM
            }
        } else {
            // $40-$7D: Full 64KB
            rom_addr = ((rom_bank - 0x40) << 16) | offset | 0x200000;
        }
        
        if (rom_addr < rom.size()) {
            return rom[rom_addr];
        }
    }
    
    return 0x00;  // Open bus
}
```

---

## 타이밍 고려사항

### 액세스 속도

```
WRAM:       8 cycles (Slow)
ROM (Slow): 8 cycles
ROM (Fast): 6 cycles
I/O:        6 or 12 cycles (레지스터에 따라 다름)
```

### Fast ROM 설정

```asm
LDA #$01
STA $420D      ; Enable FastROM (3.58MHz)
```

---

## 참고 자료

- **Fullsnes**: https://problemkaputt.de/fullsnes.htm#snesmemorymap
- **SNESdev Wiki**: https://snes.nesdev.org/wiki/Memory_mapping
- **SNES Development Manual Book I**: Chapter 1-2 Memory Map

---

**최종 업데이트**: 2025-12-14  
**우선순위**: ⭐⭐⭐ CRITICAL  
**상태**: 완성
