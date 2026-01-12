# DMA 및 HDMA 완전 가이드

## 개요

SNES의 DMA(Direct Memory Access)와 HDMA(Horizontal-DMA)는 **CPU 개입 없이 메모리를 고속 전송**하는 핵심 기능입니다. 상용 게임의 대부분이 이 기능에 의존합니다.

---

## DMA vs HDMA

| 특징 | DMA | HDMA |
|------|-----|------|
| 전송 시기 | 프로그램이 시작 | 매 스캔라인마다 자동 |
| 용도 | VRAM/OAM/CGRAM 일괄 전송 | 스캔라인별 레지스터 변경 |
| CPU 중단 | 전송 중 중단됨 | 스캔라인마다 짧게 중단 |
| 전송량 | 최대 64KB | 스캔라인당 1-4 바이트 |
| 채널 | 0-7 (8개) | 0-7 (8개, DMA와 공유) |

---

## DMA 레지스터 ($43x0-$43x7)

각 채널(0-7)마다 8개의 레지스터가 있습니다.

### $43x0 - DMAPx (DMA Control)

```
7  bit  0
---- ----
DA.I BTTT
|||| ||||
|||| |+++- Transfer Mode (0-7)
|||+-+---- B-Bus Address Step (for $43x1)
||+------- Unused
|+-------- A-Bus Address Direction (0=increment, 1=decrement, DMA only)
+--------- Transfer Direction (0=A→B, 1=B→A)
```

**Transfer Mode (TTT)**:
| Mode | A→B Pattern | Bytes/Transfer | Use Case |
|------|-------------|----------------|----------|
| 0    | 1 byte      | 1              | Single register |
| 1    | 2 bytes     | 2              | 2 consecutive registers |
| 2    | 2 bytes (AA)| 2              | Same register twice |
| 3    | 4 bytes     | 4              | 2 registers, twice each |
| 4    | 4 bytes     | 4              | 4 consecutive registers |
| 5    | 4 bytes     | 4              | 2 registers, twice (alt) |
| 6    | 2 bytes (AA)| 2              | Same as mode 2 |
| 7    | 4 bytes     | 4              | Same as mode 3 |

**Transfer Patterns**:
```
Mode 0: xx
Mode 1: xx, xx+1
Mode 2: xx, xx
Mode 3: xx, xx, xx+1, xx+1
Mode 4: xx, xx+1, xx+2, xx+3
Mode 5: xx, xx+1, xx, xx+1
Mode 6: xx, xx  (same as 2)
Mode 7: xx, xx, xx+1, xx+1  (same as 3)
```

**B-Bus Step (I)**:
```
I=0: Fixed ($43x1 never changes)
I=1: Increment ($43x1 increases with pattern)
```

**Direction (D)**:
```
D=0: CPU RAM → PPU/APU (most common)
D=1: PPU/APU → CPU RAM (read back)
```

**A-Bus Direction (A)**:
```
A=0: Increment source address
A=1: Decrement source address
```

**예제**:
```asm
; Mode 0, A→B, increment A-Bus
LDA #$00
STA $4300

; Mode 1, A→B, increment A-Bus (for VRAM)
LDA #$01
STA $4300

; Mode 2, A→B, fixed B-Bus (for single register)
LDA #$08
STA $4300
```

### $43x1 - BBADx (B-Bus Address)

```
7  bit  0
---- ----
AAAA AAAA
++++ ++++- B-Bus Register ($21xx)
```

**Address**:
```
Full B-Bus Address = $2100 + BBADx
```

**일반적인 값**:
```
$18 = $2118 (VMDATAL)
$19 = $2119 (VMDATAH)
$22 = $2122 (CGDATA)
$04 = $2104 (OAMDATA)
$80 = $2180 (WMDATA)
```

**예제**:
```asm
LDA #$18       ; Target: $2118 (VRAM data port)
STA $4301
```

### $43x2-$43x3 - A1TxL/A1TxH (A-Bus Address Low/High)

```
$43x2: Low byte  (bits 0-7)
$43x3: High byte (bits 8-15)

Address = ($43x3 << 8) | $43x2
```

**예제**:
```asm
LDA #<TileData
STA $4302      ; Source address low
LDA #>TileData
STA $4303      ; Source address high
```

### $43x4 - A1Bx (A-Bus Bank)

```
7  bit  0
---- ----
BBBB BBBB
++++ ++++- Bank Byte (bits 16-23)
```

**Full Address**:
```
Address = ($43x4 << 16) | ($43x3 << 8) | $43x2
```

**예제**:
```asm
LDA #^TileData
STA $4304      ; Source bank
```

### $43x5-$43x6 - DASxL/DASxH (DMA Size Low/High)

```
$43x5: Low byte
$43x6: High byte

Size = ($43x6 << 8) | $43x5
```

**Size**:
```
0x0000 = 65536 bytes (64KB)
0x0001-0xFFFF = 1-65535 bytes
```

**예제**:
```asm
LDA #<TileSize
STA $4305      ; Transfer size low
LDA #>TileSize
STA $4306      ; Transfer size high
```

### $43x7 - DASBx (HDMA Indirect Bank)

HDMA Indirect 모드에서만 사용:
```
7  bit  0
---- ----
BBBB BBBB
++++ ++++- Indirect Bank
```

---

## DMA 시작

### $420B - MDMAEN (DMA Enable)

```
7  bit  0
---- ----
7654 3210
|||| ||||
|||| |||+- Start DMA Channel 0
|||| ||+-- Start DMA Channel 1
|||| |+--- Start DMA Channel 2
|||| +---- Start DMA Channel 3
|||+------ Start DMA Channel 4
||+------- Start DMA Channel 5
|+-------- Start DMA Channel 6
+--------- Start DMA Channel 7
```

**동작**:
- 1로 설정된 채널이 순서대로 실행 (0→7)
- 전송 중 CPU는 중단됨
- 전송 완료 후 자동으로 0으로 리셋

**예제**:
```asm
LDA #$01
STA $420B      ; Start DMA channel 0 only

LDA #$03
STA $420B      ; Start channels 0 and 1

LDA #$FF
STA $420B      ; Start all 8 channels
```

---

## DMA 사용 예제

### 예제 1: VRAM 데이터 전송

```asm
; 타일 데이터를 VRAM $0000에 전송

; 1. VRAM 준비
LDA #$80
STA $2115      ; VRAM increment = 1 (after high byte)

LDA #$00
STA $2116      ; VRAM address = $0000
STA $2117

; 2. DMA 채널 0 설정
LDA #$01       ; Mode 1 (2 registers write once)
STA $4300      ; $2118, $2119

LDA #$18       ; Target: $2118 (VMDATAL)
STA $4301

LDA #<TileData
STA $4302      ; Source address low
LDA #>TileData
STA $4303      ; Source address high
LDA #^TileData
STA $4304      ; Source bank

LDA #<(TileDataEnd-TileData)
STA $4305      ; Size low
LDA #>(TileDataEnd-TileData)
STA $4306      ; Size high

; 3. 전송 시작
LDA #$01
STA $420B      ; Start DMA channel 0

; 전송 완료!
```

### 예제 2: OAM 데이터 전송

```asm
; OAM 버퍼를 OAM으로 전송

; 1. OAM 준비
STZ $2102      ; OAM address = $0000
STZ $2103

; 2. DMA 설정
LDA #$00       ; Mode 0 (1 register)
STA $4300

LDA #$04       ; Target: $2104 (OAMDATA)
STA $4301

LDA #<OAMBuffer
STA $4302
LDA #>OAMBuffer
STA $4303
LDA #^OAMBuffer
STA $4304

LDA #$20       ; 544 bytes (512 + 32)
STA $4305
LDA #$02
STA $4306

; 3. 전송
LDA #$01
STA $420B
```

### 예제 3: 메모리 채우기 (Fixed Source)

```asm
; WRAM을 0x00으로 채우기

; 1. Source = 고정된 0 값
ZeroValue:
    .byte $00

; 2. DMA 설정
LDA #$08       ; Mode 0, Fixed source (A-Bus no increment)
STA $4300

LDA #$80       ; Target: $2180 (WMDATA)
STA $4301

LDA #<ZeroValue
STA $4302
LDA #>ZeroValue
STA $4303
LDA #^ZeroValue
STA $4304

; 3. WRAM 주소 설정
STZ $2181      ; WRAM address = $000000
STZ $2182
STZ $2183

; 4. 전송 (64KB)
LDA #$00
STA $4305
STA $4306

LDA #$01
STA $420B
```

---

## HDMA (Horizontal DMA)

HDMA는 **매 스캔라인마다** 자동으로 레지스터를 변경합니다.

### HDMA 활성화

### $420C - HDMAEN (HDMA Enable)

```
7  bit  0
---- ----
7654 3210
|||| ||||
|||| |||+- Enable HDMA Channel 0
|||| ||+-- Enable HDMA Channel 1
... (same as MDMAEN)
```

**동작**:
- 1로 설정된 채널이 매 스캔라인마다 실행
- 프레임 내내 유지됨 (자동 리셋 안됨)

**예제**:
```asm
LDA #$01
STA $420C      ; Enable HDMA channel 0
```

### HDMA 테이블 구조

HDMA는 메모리의 **테이블**을 읽어 각 스캔라인에 적용합니다.

#### Direct 모드 ($43x0 bit 6 = 0)

```
Table format:
  [Repeat Count] [Data bytes...]

Repeat Count:
  $00       = End of table
  $01-$7F   = Repeat N scanlines
  $80-$FF   = Repeat (N & $7F) scanlines, then re-read count
```

**예제 테이블**:
```asm
HDMATable:
    .byte $10      ; 16 scanlines
    .byte $F0      ; BG1HOFS = $00F0
    .byte $00
    
    .byte $20      ; 32 scanlines
    .byte $00      ; BG1HOFS = $0000
    .byte $00
    
    .byte $00      ; End
```

#### Indirect 모드 ($43x0 bit 6 = 1)

```
Table format:
  [Repeat Count] [Address Low] [Address High]

Data is fetched from the address specified.
Bank is set by $43x7 (DASBx).
```

**예제**:
```asm
HDMAIndirect:
    .byte $10              ; 16 scanlines
    .word ScrollData1      ; Pointer to data
    
    .byte $20              ; 32 scanlines
    .word ScrollData2
    
    .byte $00              ; End

ScrollData1:
    .byte $F0, $00         ; Data

ScrollData2:
    .byte $00, $00
```

---

## HDMA 사용 예제

### 예제 1: 스캔라인별 스크롤 (Wavy Effect)

```asm
; BG1 H-Scroll을 각 라인마다 변경

; 1. HDMA 테이블 생성
HDMAScrollTable:
    .byte $E0      ; 224 scanlines (entire screen)
    ; Data will be updated each frame

InitHDMA:
    ; HDMA 채널 0 설정
    LDA #$02       ; Mode 1, Absolute
    STA $4300
    
    LDA #$0D       ; Target: $210D (BG1HOFS)
    STA $4301
    
    LDA #<HDMAScrollTable
    STA $4302
    LDA #>HDMAScrollTable
    STA $4303
    LDA #^HDMAScrollTable
    STA $4304
    
    ; HDMA 활성화
    LDA #$01
    STA $420C      ; Enable HDMA channel 0

UpdateHDMATable:
    ; 매 프레임 VBlank에서 테이블 업데이트
    LDX #$00
.loop:
    ; 사인 웨이브 계산 (pseudo-code)
    TXA
    ; ... calculate sine wave offset ...
    STA HDMAScrollTable+1,X  ; Low byte
    STA HDMAScrollTable+2,X  ; High byte
    
    INX
    INX
    INX  ; 3 bytes per entry
    CPX #$E0*3
    BCC .loop
    
    RTS
```

### 예제 2: Window 위치 변경 (Spotlight)

```asm
; Window 1의 위치를 스캔라인마다 변경

HDMAWindowTable:
    .byte $70      ; 112 scanlines
WindowData:
    .res 112*2     ; Left, Right for each line

InitWindowHDMA:
    ; HDMA 채널 1 설정
    LDA #$00       ; Mode 0
    STA $4310
    
    LDA #$26       ; Target: $2126 (WH0)
    STA $4311
    
    LDA #<HDMAWindowTable
    STA $4312
    LDA #>HDMAWindowTable
    STA $4313
    LDA #^HDMAWindowTable
    STA $4314
    
    ; HDMA 활성화
    LDA #$02
    STA $420C      ; Enable HDMA channel 1

UpdateWindowTable:
    ; 원형 스포트라이트 효과
    LDY #$00
    LDX #$00
.loop:
    ; Calculate circle at scanline Y
    ; ... math ...
    
    LDA circle_left
    STA WindowData,X
    INX
    
    LDA circle_right
    STA WindowData,X
    INX
    
    INY
    CPY #112
    BCC .loop
    
    RTS
```

### 예제 3: Mode 7 매트릭스 변경 (Perspective)

```asm
; Mode 7 매트릭스를 각 라인마다 변경하여 원근감 효과

HDMAMode7Table:
    .byte $E0      ; 224 scanlines

Mode7Data:
    .res 224*8     ; A,B,C,D matrix (2 bytes each, 4 values)

InitMode7HDMA:
    ; HDMA 채널 2 설정
    LDA #$42       ; Mode 2 (write same register twice), Indirect
    STA $4320
    
    LDA #$1B       ; Target: $211B (M7A)
    STA $4321
    
    LDA #<HDMAMode7Table
    STA $4322
    LDA #>HDMAMode7Table
    STA $4323
    LDA #^HDMAMode7Table
    STA $4324
    
    LDA #^Mode7Data
    STA $4327      ; Indirect bank
    
    ; HDMA 활성화
    LDA #$04
    STA $420C      ; Enable HDMA channel 2
```

---

## 타이밍 및 우선순위

### DMA 타이밍

```
1 DMA 전송 = 8 master cycles per byte
            = ~2.68 CPU cycles per byte

64KB DMA = ~174,080 master cycles
         = ~20.8ms at 3.58MHz
```

### HDMA 타이밍

```
Overhead per scanline:
  - 18 master cycles (setup)
  - 8 master cycles per byte transferred

Total per frame (224 lines):
  - Minimum: ~4,032 master cycles (no data)
  - With 4 bytes/line: ~7,168 cycles
```

### 채널 우선순위

DMA/HDMA는 **낮은 번호 채널이 우선**합니다:
```
Channel 0 > Channel 1 > Channel 2 > ... > Channel 7
```

---

## 에뮬레이터 구현 가이드

### DMA 구현

```cpp
class DMA {
private:
    struct Channel {
        uint8_t dmap;        // $43x0
        uint8_t bbad;        // $43x1
        uint16_t a1t;        // $43x2-3
        uint8_t a1b;         // $43x4
        uint16_t das;        // $43x5-6
        uint8_t dasb;        // $43x7 (HDMA only)
        
        // HDMA state
        bool hdma_enabled;
        uint16_t hdma_table_addr;
        uint8_t hdma_line_counter;
    };
    
    Channel channels[8];
    
public:
    void startDMA(uint8_t channel_mask) {
        for (int ch = 0; ch < 8; ch++) {
            if (channel_mask & (1 << ch)) {
                executeDMA(ch);
            }
        }
    }
    
private:
    void executeDMA(int ch) {
        Channel& c = channels[ch];
        
        int transfer_size = c.das ? c.das : 65536;
        bool direction = (c.dmap & 0x80) != 0;  // 1=B→A
        bool increment = (c.dmap & 0x40) == 0;  // 0=increment
        int mode = c.dmap & 0x07;
        
        uint32_t a_addr = (c.a1b << 16) | c.a1t;
        uint16_t b_addr = 0x2100 + c.bbad;
        
        // Transfer pattern
        const int patterns[8][4] = {
            {0, -1, -1, -1},  // Mode 0: xx
            {0, 1, -1, -1},   // Mode 1: xx, xx+1
            {0, 0, -1, -1},   // Mode 2: xx, xx
            {0, 0, 1, 1},     // Mode 3: xx, xx, xx+1, xx+1
            {0, 1, 2, 3},     // Mode 4: xx, xx+1, xx+2, xx+3
            {0, 1, 0, 1},     // Mode 5: xx, xx+1, xx, xx+1
            {0, 0, -1, -1},   // Mode 6: same as 2
            {0, 0, 1, 1}      // Mode 7: same as 3
        };
        
        int bytes_per_unit = (mode == 0) ? 1 : (mode < 3) ? 2 : 4;
        int units = transfer_size / bytes_per_unit;
        
        for (int i = 0; i < units; i++) {
            for (int p = 0; patterns[mode][p] != -1; p++) {
                uint16_t curr_b = b_addr + patterns[mode][p];
                
                if (direction) {
                    // B → A
                    uint8_t value = bus->read(curr_b);
                    bus->write(a_addr, value);
                } else {
                    // A → B
                    uint8_t value = bus->read(a_addr);
                    bus->write(curr_b, value);
                }
                
                if (increment) {
                    a_addr++;
                } else {
                    a_addr--;
                }
                
                cycles += 8;  // 8 master cycles per byte
            }
        }
    }
};
```

### HDMA 구현

```cpp
void DMA::executeHDMA() {
    for (int ch = 0; ch < 8; ch++) {
        if (!channels[ch].hdma_enabled) continue;
        
        Channel& c = channels[ch];
        
        // Check if need to read new line counter
        if (c.hdma_line_counter == 0) {
            uint8_t count = bus->read(c.hdma_table_addr);
            c.hdma_table_addr++;
            
            if (count == 0) {
                // End of table
                c.hdma_enabled = false;
                continue;
            }
            
            c.hdma_line_counter = count & 0x7F;
            
            bool indirect = (c.dmap & 0x40) != 0;
            
            if (indirect) {
                // Read indirect address
                uint16_t indirect_addr = bus->read(c.hdma_table_addr);
                c.hdma_table_addr++;
                indirect_addr |= bus->read(c.hdma_table_addr) << 8;
                c.hdma_table_addr++;
                
                // Transfer from indirect address
                transferHDMALine(ch, (c.dasb << 16) | indirect_addr);
            } else {
                // Transfer from table
                transferHDMALine(ch, c.hdma_table_addr);
                int mode = c.dmap & 0x07;
                int bytes = (mode == 0) ? 1 : (mode < 3) ? 2 : 4;
                c.hdma_table_addr += bytes;
            }
        }
        
        c.hdma_line_counter--;
    }
}

void DMA::transferHDMALine(int ch, uint32_t addr) {
    Channel& c = channels[ch];
    int mode = c.dmap & 0x07;
    uint16_t b_addr = 0x2100 + c.bbad;
    
    // Transfer bytes according to mode
    for (int i = 0; i < bytesForMode(mode); i++) {
        uint8_t value = bus->read(addr++);
        bus->write(b_addr + offsetForMode(mode, i), value);
        cycles += 8;
    }
}
```

---

## 일반적인 실수

### 1. DMA 중 레지스터 접근
```
❌ 잘못된 코드:
LDA #$01
STA $420B      ; Start DMA
LDA $2137      ; ← 이미 DMA 시작됨, 접근 불가!

✅ 올바른 코드:
LDA #$01
STA $420B
; DMA 완료될 때까지 기다림 (자동)
LDA $2137      ; OK
```

### 2. HDMA 테이블 끝 없음
```
❌ 잘못된 테이블:
HDMATable:
    .byte $E0
    ; ... 데이터 ...
    ; End marker 없음!

✅ 올바른 테이블:
HDMATable:
    .byte $E0
    ; ... 데이터 ...
    .byte $00      ; End marker
```

### 3. Transfer Mode 혼동
```
Mode 1은 2개의 연속된 레지스터:
  $2118, $2119 (VRAM low, high)

Mode 2는 같은 레지스터 2번:
  $2122, $2122 (CGRAM data)
```

---

## 참고 자료

- **Fullsnes**: https://problemkaputt.de/fullsnes.htm#snesdmaandhdmachannels
- **SNESdev Wiki**: https://snes.nesdev.org/wiki/DMA_and_HDMA
- **Grog's Guide to DMA**: https://wiki.superfamicom.org/grog's-guide-to-dma-and-hdma-on-the-snes

---

**최종 업데이트**: 2025-12-14  
**우선순위**: ⭐⭐⭐ CRITICAL  
**상태**: 완성
