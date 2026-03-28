# PPU 레지스터 비트맵 완전 가이드

## 개요

SNES PPU는 $2100-$213F 범위에 64개의 레지스터를 가지고 있습니다. 이 문서는 **모든 비트의 기능**을 상세히 설명합니다.

---

## 화면 표시 제어

### $2100 - INIDISP (Screen Display)

```
7  bit  0
---- ----
F... BBBB
|    ||||
|    ++++- Brightness (0-15, 0=black, 15=full)
+--------- Force Blank (1=screen off, 0=screen on)
```

**동작**:
- Force Blank (F=1): 화면을 검은색으로, VRAM/OAM/CGRAM 접근 안전
- Brightness: 0=완전히 어두움, 15=정상 밝기

**예제**:
```asm
LDA #$0F
STA $2100      ; Full brightness, screen on

LDA #$80
STA $2100      ; Force blank (for VRAM uploads)
```

---

## 스프라이트 설정

### $2101 - OBSEL (Object Size and Character Base)

```
7  bit  0
---- ----
SSSN NBBB
|||| ||||
|||| |+++- Character Base Address (in VRAM)
|||+-+---- Name Select (VRAM offset for 2nd tile table)
+++------- Sprite Size (see table below)
```

**Sprite Size Table**:
| SSS | Small | Large |
|-----|-------|-------|
| 000 | 8x8   | 16x16 |
| 001 | 8x8   | 32x32 |
| 010 | 8x8   | 64x64 |
| 011 | 16x16 | 32x32 |
| 100 | 16x16 | 64x64 |
| 101 | 32x32 | 64x64 |
| 110 | 16x32 | 32x64 |
| 111 | 16x32 | 32x32 |

**Character Base Address**:
```
Address = BBB × $2000
```

**Name Select**:
```
Offset = (NN + 1) × $1000
```

**예제**:
```asm
LDA #$02       ; Small=8x8, Large=64x64, Base=$0000
STA $2101
```

### $2102-$2103 - OAMADD (OAM Address)

```
$2102 (Low byte):
7  bit  0
---- ----
AAAA AAAA
++++ ++++- OAM Word Address (Low 8 bits)

$2103 (High byte):
7  bit  0
---- ----
P... ...A
|       |
|       +- OAM Word Address (bit 8)
+--------- Priority Rotation
```

**OAM Address**:
```
Word Address = ($2103:bit0 << 8) | $2102
Byte Address = Word Address × 2
```

**예제**:
```asm
STZ $2102      ; OAM address = $0000
STZ $2103
```

### $2104 - OAMDATA (OAM Data Write)

**Write-twice register**:
- 1st write: Low byte
- 2nd write: High byte, address auto-increments

**OAM Structure**:
```
Low table (512 bytes): Sprites 0-127
  Byte 0: X position (low 8 bits)
  Byte 1: Y position
  Byte 2: Tile number
  Byte 3: VhopppTT
          ||||||++- Palette (0-7)
          ||||++--- Priority (0-3)
          |||+----- Name table select
          ||+------ H-flip
          |+------- V-flip

High table (32 bytes): Sprites 0-127
  2 bits per sprite: X.......
                      |+------ X position (bit 8)
                      +------- Size toggle (0=small, 1=large)
```

**예제**:
```asm
; Write sprite 0
LDA #$00
STA $2102      ; OAM address = 0

LDA #$80       ; X = 128
STA $2104
LDA #$70       ; Y = 112
STA $2104
LDA #$00       ; Tile 0
STA $2104
LDA #$00       ; Palette 0, priority 0
STA $2104
```

---

## 배경 모드 및 캐릭터 크기

### $2105 - BGMODE (BG Mode and Character Size)

```
7  bit  0
---- ----
ABCD EMMM
|||| ||||
|||| |+++- BG Mode (0-7)
|||+-+---- Mode 1 BG3 Priority (1=high)
||+------- BG1 Character Size (0=8x8, 1=16x16)
|+-------- BG2 Character Size
+--------- BG3 Character Size
(BG4 Character Size is always 8x8)
```

**BG Modes**:
| Mode | BG1 | BG2 | BG3 | BG4 | Colors | Priority |
|------|-----|-----|-----|-----|--------|----------|
| 0    | 2   | 2   | 2   | 2   | 4,4,4,4| Normal   |
| 1    | 4   | 4   | 2   | -   | 16,16,4| BG3 opt  |
| 2    | 4   | 4   | -   | -   | 16,16  | Offset   |
| 3    | 8   | 4   | -   | -   | 256,16 | Normal   |
| 4    | 8   | 2   | -   | -   | 256,4  | Offset   |
| 5    | 4   | 2   | -   | -   | 16,4   | HiRes    |
| 6    | 4   | -   | -   | -   | 16     | Offset+Hi|
| 7    | 8   | -   | -   | -   | 256    | Mode7    |

**예제**:
```asm
LDA #$01       ; Mode 1, 8x8 tiles
STA $2105

LDA #$09       ; Mode 1, BG1 16x16 tiles
STA $2105
```

### $2106 - MOSAIC (Mosaic Size and Enable)

```
7  bit  0
---- ----
SSSS 4321
|||| ||||
|||| |||+- BG1 Mosaic Enable
|||| ||+-- BG2 Mosaic Enable
|||| |+--- BG3 Mosaic Enable
|||| +---- BG4 Mosaic Enable
++++------ Mosaic Size (0=1x1, 15=16x16)
```

**예제**:
```asm
LDA #$33       ; Size 3x3, enable BG1 and BG2
STA $2106
```

---

## 배경 타일맵 주소

### $2107-$210A - BGxSC (BG Tilemap Address)

각 배경마다 하나씩 (BG1-4):
```
7  bit  0
---- ----
AAAA AAYX
|||| ||||
|||| |||+- Horizontal Mirroring (1=enabled)
|||| ||+-- Vertical Mirroring (1=enabled)
++++-++--- Tilemap Base Address
```

**Address**:
```
Address = AAAAAA × $400
```

**Mirroring**:
```
YX=00: No mirroring (32x32 tiles)
YX=01: Horizontal (64x32 tiles)
YX=10: Vertical (32x64 tiles)
YX=11: Both (64x64 tiles)
```

**예제**:
```asm
LDA #$00       ; BG1 tilemap at $0000, 32x32
STA $2107

LDA #$04       ; BG2 tilemap at $1000, 32x32
STA $2108
```

---

## 배경 캐릭터 주소

### $210B - BG12NBA (BG1 and BG2 Character Base)

```
7  bit  0
---- ----
BBBB AAAA
|||| ||||
|||| ++++- BG1 Character Base Address
++++------ BG2 Character Base Address
```

**Address**:
```
BG1 Address = AAAA × $1000
BG2 Address = BBBB × $1000
```

### $210C - BG34NBA (BG3 and BG4 Character Base)

Same format as $210B.

**예제**:
```asm
LDA #$00       ; BG1 at $0000, BG2 at $0000
STA $210B

LDA #$21       ; BG3 at $1000, BG4 at $2000
STA $210C
```

---

## 배경 스크롤

### $210D-$210E - BG1HOFS/BG1VOFS (BG1 Scroll)
### $210F-$2110 - BG2HOFS/BG2VOFS (BG2 Scroll)
### $2111-$2112 - BG3HOFS/BG3VOFS (BG3 Scroll)
### $2113-$2114 - BG4HOFS/BG4VOFS (BG4 Scroll)

**Write-twice registers** (M7 uses different format):

```
First write:  Low 8 bits
Second write: High 2 bits (bits 8-9)

Effective value = (current << 8) | (previous & ~7) | (current >> 8)
```

**H-Offset**: 0-1023 (10 bits)
**V-Offset**: 0-1023 (10 bits)

**예제**:
```asm
; Scroll BG1 to (100, 50)
LDA #$64       ; 100
STA $210D      ; BG1HOFS low
LDA #$00
STA $210D      ; BG1HOFS high

LDA #$32       ; 50
STA $210E      ; BG1VOFS low
LDA #$00
STA $210E      ; BG1VOFS high
```

---

## VRAM 접근

### $2115 - VMAIN (VRAM Address Increment)

```
7  bit  0
---- ----
I... RRMM
|    ||||
|    ||++- Address Increment Mode
|    ++--- Address Remapping
+--------- Increment on high/low byte access
```

**Increment Mode**:
```
MM=00: Increment by 1
MM=01: Increment by 32
MM=10: Increment by 128
MM=11: Increment by 128
```

**Increment Timing**:
```
I=0: Increment after $2118 (low byte) write
I=1: Increment after $2119 (high byte) write
```

**Remapping** (for efficient tile data access):
```
RR=00: No remapping
RR=01: Remap aaaaaaaaBBBccccc to aaaaaaaacccccBBB
RR=10: Remap aaaaaaaBBBBcccccc to aaaaaaaccccccBBBB
RR=11: Remap aaaaaaBBBBBccccccc to aaaaaacccccccBBBBB
```

**예제**:
```asm
LDA #$80       ; Increment after high byte, by 1
STA $2115
```

### $2116-$2117 - VMADD (VRAM Address)

```
$2116: Low byte
$2117: High byte

Address = ($2117 << 8) | $2116
Word address (0-$7FFF)
```

**예제**:
```asm
LDA #$00
STA $2116      ; VRAM address = $0000
STA $2117
```

### $2118-$2119 - VMDATA (VRAM Data)

```
$2118: Low byte
$2119: High byte
```

**Write**: 
- Address increments based on $2115 setting

**예제**:
```asm
LDA #$80
STA $2115      ; Increment by 1 after high byte

LDA #$00
STA $2116
STA $2117      ; Address = $0000

LDA #$12
STA $2118      ; Write $12 to VRAM[0] low byte
LDA #$34
STA $2119      ; Write $34 to VRAM[0] high byte, then increment
```

---

## Mode 7 설정

### $211A - M7SEL (Mode 7 Settings)

```
7  bit  0
---- ----
RC.. ..BA
||     ||
||     |+- Horizontal Mirroring
||     +-- Vertical Mirroring
|+-------- Empty Space Fill (0=transparency, 1=tile 0)
+--------- Screen Over (0=wrap, 1=transparent outside)
```

**예제**:
```asm
LDA #$00       ; No mirroring, wrap around
STA $211A
```

### $211B-$211E - M7A-M7D (Mode 7 Matrix)

**Write-twice registers**:
```
Matrix = | A  B |
         | C  D |

First write:  Low 8 bits
Second write: High 8 bits (signed)

Values are 8.8 fixed-point (-128.00 to +127.99609375)
```

**Identity Matrix**:
```asm
; A = 1.0
LDA #$00
STA $211B      ; Low byte
LDA #$01
STA $211B      ; High byte = 0x0100 = 1.0

; B = 0.0
LDA #$00
STA $211C
STA $211C

; C = 0.0
LDA #$00
STA $211D
STA $211D

; D = 1.0
LDA #$00
STA $211E
LDA #$01
STA $211E
```

### $211F-$2120 - M7X/M7Y (Mode 7 Center)

**Write-twice registers**:
```
First write:  Low 8 bits
Second write: High 5 bits (13-bit signed)

Range: -4096 to +4095
```

**예제**:
```asm
; Center at (128, 112)
LDA #$80
STA $211F      ; M7X low
LDA #$00
STA $211F      ; M7X high

LDA #$70
STA $2120      ; M7Y low
LDA #$00
STA $2120      ; M7Y high
```

---

## CGRAM (Color Palette)

### $2121 - CGADD (CGRAM Address)

```
7  bit  0
---- ----
AAAA AAAA
++++ ++++- Color Index (0-255)
```

**예제**:
```asm
LDA #$00
STA $2121      ; Start at color 0
```

### $2122 - CGDATA (CGRAM Data Write)

**Write-twice register** (15-bit BGR format):
```
First write:  Low byte  = gggrrrrr
Second write: High byte = .bbbbbgg

Full color = 0bbbbbgg gggrrrrr
            = %0BBBBGGGRRR (5 bits each)
```

**예제**:
```asm
; Write white color ($7FFF)
LDA #$00
STA $2121      ; Color 0

LDA #$FF       ; Red=31, Green=3 (low)
STA $2122
LDA #$7F       ; Blue=31, Green=28 (high)
STA $2122

; Result: RGB = (31, 31, 31) = White
```

---

## 윈도우 마스크

### $2123-$2125 - W12SEL, W34SEL, WOBJSEL (Window Mask)

```
7  bit  0
---- ----
ABab CDcd
|||| ||||
|||| ||++- BG1/BG3/OBJ Window 1 Enable & Invert
|||| ++--- BG1/BG3/OBJ Window 2 Enable & Invert
||++------ BG2/BG4 Window 1 Enable & Invert
++-------- BG2/BG4 Window 2 Enable & Invert

For each: ab/cd
  a/c: 1=Enable Window 1/2
  b/d: 1=Invert Window 1/2 (inside becomes outside)
```

**예제**:
```asm
LDA #$02       ; BG1: Window 1 enabled, not inverted
STA $2123
```

### $2126-$212B - WH0-WH3, WBGLOG, WOBJLOG

**Window Positions**:
```
$2126 - WH0 (Window 1 Left)
$2127 - WH1 (Window 1 Right)
$2128 - WH2 (Window 2 Left)
$2129 - WH3 (Window 2 Right)
```

**Window Logic**:
```
$212A - WBGLOG
$212B - WOBJLOG

Format: 4433 2211
        |||| ||||
        |||| ||++- BG1/OBJ Mask Logic (00=OR, 01=AND, 10=XOR, 11=XNOR)
        |||| ++--- BG2 Mask Logic
        ||++------ BG3 Mask Logic
        ++-------- BG4 Mask Logic
```

**예제**:
```asm
; Window from X=64 to X=192
LDA #$40
STA $2126      ; Window 1 left = 64
LDA #$C0
STA $2127      ; Window 1 right = 192

LDA #$00
STA $212A      ; All BGs use OR logic
```

---

## 메인/서브 화면 설정

### $212C - TM (Main Screen Designation)

```
7  bit  0
---- ----
...O 4321
   | ||||
   | |||+- BG1 on Main Screen
   | ||+-- BG2 on Main Screen
   | |+--- BG3 on Main Screen
   | +---- BG4 on Main Screen
   +------ OBJ on Main Screen
```

### $212D - TS (Sub Screen Designation)

Same format as $212C.

**예제**:
```asm
LDA #$17       ; Main: BG1, BG2, BG3, OBJ
STA $212C

LDA #$00       ; Sub: Nothing
STA $212D
```

### $212E - TMW (Window Mask Designation - Main)

```
7  bit  0
---- ----
...O 4321
   | ||||
   | |||+- BG1 Window Mask Enable (Main)
   | ||+-- BG2 Window Mask Enable (Main)
   | |+--- BG3 Window Mask Enable (Main)
   | +---- BG4 Window Mask Enable (Main)
   +------ OBJ Window Mask Enable (Main)
```

### $212F - TSW (Window Mask Designation - Sub)

Same format as $212E.

---

## Color Math (색상 연산)

### $2130 - CGWSEL (Color Math Control)

```
7  bit  0
---- ----
MMCC DD.S
|||| || |
|||| || +- Direct Color Mode (Mode 7 only)
|||| ++--- Color Math Enable (Main/Sub screen areas)
||++------ Color Math Clip Mode
++-------- Prevent Color Math (areas)
```

**Color Math Enable**:
```
DD=00: Always
DD=01: Inside window only
DD=10: Outside window only
DD=11: Never
```

**Clip Mode**:
```
CC=00: Never
CC=01: Outside window only
CC=10: Inside window only
CC=11: Always
```

**예제**:
```asm
LDA #$00       ; Color math always, no clipping
STA $2130
```

### $2131 - CGADSUB (Color Math Settings)

```
7  bit  0
---- ----
SHBO 4321
|||| ||||
|||| |||+- BG1 Color Math Enable
|||| ||+-- BG2 Color Math Enable
|||| |+--- BG3 Color Math Enable
|||| +---- BG4 Color Math Enable
|||+------ OBJ Color Math Enable
||+------- Backdrop Color Math Enable
|+-------- Half Color Math (divide result by 2)
+--------- Subtract instead of Add
```

**예제**:
```asm
LDA #$3F       ; All layers, add, full intensity
STA $2131
```

### $2132 - COLDATA (Fixed Color Data)

**Write 1-3 times** to set RGB:
```
7  bit  0
---- ----
BGRC CCCC
|||| ||||
|||+-++++- Color Value (0-31)
||+------- Red Component Select
|+-------- Green Component Select
+--------- Blue Component Select
```

**예제**:
```asm
; Set fixed color to white (31,31,31)
LDA #$3F       ; Red = 31
STA $2132
LDA #$5F       ; Green = 31
STA $2132
LDA #$9F       ; Blue = 31
STA $2132
```

---

## 화면 모드

### $2133 - SETINI (Screen Mode/Video Select)

```
7  bit  0
---- ----
ES.L IIIH
||  | |||+- Mode 7 EXTBG (enable BG2)
||  | +++-- Interlace Mode
||  +------ OBJ Interlace
|+--------- Overscan Mode (239 lines instead of 224)
+---------- External Sync
```

**Interlace Modes**:
```
III=000: No interlace
III=001: Interlace (field alternates)
III=010: Invalid
III=011: Invalid
III=100: No interlace
III=101: Interlace
III=110: Interlace + Hi-res (512 pixels)
III=111: Invalid
```

**예제**:
```asm
LDA #$00       ; Normal mode, no interlace
STA $2133

LDA #$08       ; Overscan enabled (PAL)
STA $2133
```

---

## 에뮬레이터 구현 가이드

### PPU 레지스터 읽기/쓰기

```cpp
class PPU {
private:
    // 레지스터 값
    uint8_t inidisp;      // $2100
    uint8_t obsel;        // $2101
    uint16_t oamadd;      // $2102-2103
    uint8_t bgmode;       // $2105
    // ... (모든 레지스터)
    
    // Write-twice 레지스터용
    struct WriteTwice {
        uint8_t first_write;
        bool is_first;
    };
    
    WriteTwice vram_addr_write;
    WriteTwice scroll_writes[8];  // 4 BGs × 2 (H/V)
    
public:
    void writeRegister(uint16_t addr, uint8_t value) {
        switch (addr) {
            case 0x2100:  // INIDISP
                inidisp = value;
                updateBrightness(value & 0x0F);
                updateForceBlank(value & 0x80);
                break;
                
            case 0x2105:  // BGMODE
                bgmode = value;
                updateBGMode(value & 0x07);
                updateBG3Priority(value & 0x08);
                updateTileSizes(value);
                break;
                
            case 0x210D:  // BG1HOFS
                if (scroll_writes[0].is_first) {
                    scroll_writes[0].first_write = value;
                    scroll_writes[0].is_first = false;
                } else {
                    uint16_t offset = ((value & 0x03) << 8) | 
                                    (scroll_writes[0].first_write);
                    bg1_hofs = offset;
                    scroll_writes[0].is_first = true;
                }
                break;
                
            case 0x2118:  // VMDATAL
                vram[vram_address] = (vram[vram_address] & 0xFF00) | value;
                if ((vmain & 0x80) == 0) {
                    vram_address += getVRAMIncrement();
                }
                break;
                
            case 0x2119:  // VMDATAH
                vram[vram_address] = (vram[vram_address] & 0x00FF) | 
                                    (value << 8);
                if (vmain & 0x80) {
                    vram_address += getVRAMIncrement();
                }
                break;
                
            // ... 나머지 레지스터들
        }
    }
    
private:
    int getVRAMIncrement() {
        switch (vmain & 0x03) {
            case 0: return 1;
            case 1: return 32;
            case 2:
            case 3: return 128;
        }
        return 1;
    }
};
```

---

## 쓰기 시퀀스 예제

### VRAM에 타일 데이터 업로드

```asm
; 1. Force blank
LDA #$80
STA $2100

; 2. VRAM 설정
LDA #$80       ; Increment by 1 after high byte
STA $2115

LDA #$00       ; VRAM address = $0000
STA $2116
STA $2117

; 3. 데이터 전송 (DMA 사용)
LDA #$01       ; DMA mode: 2 registers write once
STA $4300
LDA #$18       ; Destination: $2118 (VMDATAL)
STA $4301
LDA #<TileData
STA $4302
LDA #>TileData
STA $4303
LDA #^TileData
STA $4304
LDA #<TileDataSize
STA $4305
LDA #>TileDataSize
STA $4306

LDA #$01
STA $420B      ; Start DMA channel 0

; 4. 화면 켜기
LDA #$0F
STA $2100
```

### 배경 설정

```asm
; BG Mode 1
LDA #$01
STA $2105

; BG1 tilemap at $0000, 32x32
LDA #$00
STA $2107

; BG1 character data at $1000
LDA #$10
STA $210B

; Enable BG1 on main screen
LDA #$01
STA $212C
```

---

## 참고 자료

- **Fullsnes**: https://problemkaputt.de/fullsnes.htm#snesppu
- **SNESdev Wiki**: https://snes.nesdev.org/wiki/Registers
- **Anomie's Register Doc**: https://www.romhacking.net/documents/196/

---

**최종 업데이트**: 2025-12-14  
**우선순위**: ⭐⭐⭐ CRITICAL  
**상태**: 완성










