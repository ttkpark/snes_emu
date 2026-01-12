# SNES VRAM Data Format - Bitplane 및 타일 포맷

## 📋 목차
1. [개요](#개요)
2. [타일 기본 개념](#타일-기본-개념)
3. [Bitplane 인코딩](#bitplane-인코딩)
4. [2bpp 포맷](#2bpp-포맷-4색)
5. [4bpp 포맷](#4bpp-포맷-16색)
6. [8bpp 포맷](#8bpp-포맷-256색)
7. [VRAM 레이아웃](#vram-레이아웃)
8. [타일 → 픽셀 변환](#타일--픽셀-변환)
9. [구현 가이드](#구현-가이드)
10. [최적화 팁](#최적화-팁)

---

## 개요

SNES의 그래픽 데이터는 **타일(Tile)** 단위로 저장되며, 각 타일은 **Bitplane** 방식으로 인코딩됩니다. 이 문서는 VRAM에 저장된 원시 데이터를 실제 화면 픽셀로 변환하는 과정을 설명합니다.

### 핵심 개념

| 개념 | 설명 |
|------|------|
| **Tile** | 8×8 픽셀 그래픽 블록 (SNES의 기본 그래픽 단위) |
| **Bitplane** | 각 색상 비트를 별도 평면에 저장하는 인코딩 방식 |
| **BPP** | Bits Per Pixel (픽셀당 비트 수 = 색상 깊이) |
| **Palette** | 색상 인덱스 → 실제 15비트 RGB 색상 매핑 |

---

## 타일 기본 개념

### 타일 크기
- **픽셀 크기**: 8×8 = 64픽셀
- **메모리 크기**: 색상 깊이에 따라 다름
  - 2bpp: 16바이트
  - 4bpp: 32바이트
  - 8bpp: 64바이트

### 배경 모드별 색상 깊이

| Mode | BG1 | BG2 | BG3 | BG4 | 설명 |
|------|-----|-----|-----|-----|------|
| 0 | 2bpp | 2bpp | 2bpp | 2bpp | 4개 배경, 각 4색 |
| 1 | 4bpp | 4bpp | 2bpp | - | 2개 16색 + 1개 4색 |
| 2 | 4bpp | 4bpp | - | - | 2개 16색 (Offset-per-tile) |
| 3 | 8bpp | 4bpp | - | - | 1개 256색 + 1개 16색 |
| 4 | 8bpp | 2bpp | - | - | 1개 256색 + 1개 4색 (Offset-per-tile) |
| 5 | 4bpp | 2bpp | - | - | 고해상도 모드 (512×448) |
| 6 | 4bpp | - | - | - | 고해상도 + Offset-per-tile |
| 7 | 8bpp | - | - | - | Mode 7 회전/확대/축소 |

---

## Bitplane 인코딩

### 원리

각 픽셀의 색상 인덱스는 여러 비트로 구성되며, **각 비트가 별도의 평면(Plane)에 저장**됩니다.

**예: 4bpp (4비트 색상)**
```
픽셀 색상 인덱스 = Bit3 << 3 | Bit2 << 2 | Bit1 << 1 | Bit0

각 Bit는 다른 바이트에서 가져옴:
- Bit0: Plane 0
- Bit1: Plane 1
- Bit2: Plane 2
- Bit3: Plane 3
```

### 왜 Bitplane을 사용하나?

1. **압축**: 낮은 색상 깊이에서 메모리 절약
2. **유연성**: 동적으로 색상 깊이 변경 가능
3. **하드웨어 효율**: 병렬 처리에 유리

---

## 2bpp 포맷 (4색)

### 메모리 레이아웃

**타일 크기**: 16바이트 (8행 × 2바이트/행)

```
8×8 타일 = 64픽셀
2비트/픽셀 = 128비트 = 16바이트

구조:
[Plane 0, Row 0] [Plane 1, Row 0]  ← 행 0
[Plane 0, Row 1] [Plane 1, Row 1]  ← 행 1
...
[Plane 0, Row 7] [Plane 1, Row 7]  ← 행 7
```

### 상세 레이아웃

```
Offset | Plane | Row | 설명
-------|-------|-----|---------------------
+0x00  | 0     | 0   | 행 0의 하위 비트 (Bit 0)
+0x01  | 1     | 0   | 행 0의 상위 비트 (Bit 1)
+0x02  | 0     | 1   | 행 1의 하위 비트
+0x03  | 1     | 1   | 행 1의 상위 비트
...
+0x0E  | 0     | 7   | 행 7의 하위 비트
+0x0F  | 1     | 7   | 행 7의 상위 비트
```

### 디코딩 예제

**VRAM 데이터 (16바이트)**:
```
00: FF 00  ← 행 0: Plane0=11111111, Plane1=00000000
02: FF 00  ← 행 1
04: FF 00  ← 행 2
06: FF 00  ← 행 3
08: FF 00  ← 행 4
0A: FF 00  ← 행 5
0C: FF 00  ← 행 6
0E: FF 00  ← 행 7
```

**디코딩 (행 0)**:
```
Plane0[0] = 0xFF = 11111111
Plane1[0] = 0x00 = 00000000

픽셀 0: Bit1=0, Bit0=1 → 색상 인덱스 = 0b01 = 1
픽셀 1: Bit1=0, Bit0=1 → 색상 인덱스 = 0b01 = 1
...
픽셀 7: Bit1=0, Bit0=1 → 색상 인덱스 = 0b01 = 1

결과: 모든 픽셀이 색상 1
```

### C++ 구현

```cpp
// 2bpp 타일 디코딩
void decode2bpp(const uint8_t* tileData, uint8_t output[8][8]) {
    for (int y = 0; y < 8; y++) {
        uint8_t plane0 = tileData[y * 2 + 0];  // Bit 0
        uint8_t plane1 = tileData[y * 2 + 1];  // Bit 1
        
        for (int x = 0; x < 8; x++) {
            int bitPos = 7 - x;  // MSB부터 (왼쪽 픽셀이 MSB)
            uint8_t bit0 = (plane0 >> bitPos) & 1;
            uint8_t bit1 = (plane1 >> bitPos) & 1;
            
            output[y][x] = (bit1 << 1) | bit0;  // 2비트 색상 인덱스
        }
    }
}
```

### ASM 예제 (VRAM 업로드)

```asm
; 2bpp 타일 데이터를 VRAM에 업로드
; A = VRAM 주소 (워드)
LoadTiles2bpp:
    REP #$20        ; 16비트 모드
    STA $2116       ; VRAM 주소 설정
    SEP #$20        ; 8비트 모드
    
    LDX #$0000
.loop:
    LDA TileData,X
    STA $2118       ; VRAM 쓰기 (하위 바이트)
    INX
    CPX #$0010      ; 16바이트 (1타일)
    BNE .loop
    RTS

TileData:
    .db $FF, $00, $FF, $00, $FF, $00, $FF, $00
    .db $FF, $00, $FF, $00, $FF, $00, $FF, $00
```

---

## 4bpp 포맷 (16색)

### 메모리 레이아웃

**타일 크기**: 32바이트 (8행 × 4바이트/행)

```
구조: Plane 0-1이 먼저, Plane 2-3이 나중에 (인터리브됨)

[Plane 0, Row 0] [Plane 1, Row 0]  ← 행 0, 하위 2비트
[Plane 0, Row 1] [Plane 1, Row 1]
...
[Plane 0, Row 7] [Plane 1, Row 7]
[Plane 2, Row 0] [Plane 3, Row 0]  ← 행 0, 상위 2비트
[Plane 2, Row 1] [Plane 3, Row 1]
...
[Plane 2, Row 7] [Plane 3, Row 7]
```

### 상세 레이아웃

```
Offset | Plane | Row | 비트 위치
-------|-------|-----|----------
+0x00  | 0     | 0   | Bit 0
+0x01  | 1     | 0   | Bit 1
+0x02  | 0     | 1   | Bit 0
+0x03  | 1     | 1   | Bit 1
...
+0x0E  | 0     | 7   | Bit 0
+0x0F  | 1     | 7   | Bit 1
+0x10  | 2     | 0   | Bit 2  ← 상위 비트 시작
+0x11  | 3     | 0   | Bit 3
...
+0x1E  | 2     | 7   | Bit 2
+0x1F  | 3     | 7   | Bit 3
```

### C++ 구현

```cpp
// 4bpp 타일 디코딩
void decode4bpp(const uint8_t* tileData, uint8_t output[8][8]) {
    for (int y = 0; y < 8; y++) {
        // 하위 2비트 (Plane 0-1)
        uint8_t plane0 = tileData[y * 2 + 0];
        uint8_t plane1 = tileData[y * 2 + 1];
        
        // 상위 2비트 (Plane 2-3)
        uint8_t plane2 = tileData[16 + y * 2 + 0];
        uint8_t plane3 = tileData[16 + y * 2 + 1];
        
        for (int x = 0; x < 8; x++) {
            int bitPos = 7 - x;
            uint8_t bit0 = (plane0 >> bitPos) & 1;
            uint8_t bit1 = (plane1 >> bitPos) & 1;
            uint8_t bit2 = (plane2 >> bitPos) & 1;
            uint8_t bit3 = (plane3 >> bitPos) & 1;
            
            output[y][x] = (bit3 << 3) | (bit2 << 2) | (bit1 << 1) | bit0;
        }
    }
}
```

### 예제: 그라데이션 타일

```
VRAM 데이터 (32바이트):
00: 00 FF  ← 행 0: Plane0=0, Plane1=1 (모든 픽셀 = 0b01)
02: 00 FF  ← 행 1
04: 00 FF  ← 행 2
06: 00 FF  ← 행 3
08: 00 FF  ← 행 4
0A: 00 FF  ← 행 5
0C: 00 FF  ← 행 6
0E: 00 FF  ← 행 7

10: 00 00  ← 행 0: Plane2=0, Plane3=0 (상위 비트 = 0b00)
12: 01 00  ← 행 1: 왼쪽 픽셀만 Plane2=1
14: 03 00  ← 행 2
16: 07 00  ← 행 3
...

결과:
행 0: 색상 0b0001 = 1 (전부)
행 1: 색상 0b0101 = 5 (왼쪽 1픽셀), 나머지 0b0001 = 1
행 2: 색상 0b0101 = 5 (왼쪽 2픽셀), 나머지 0b0001 = 1
...
```

---

## 8bpp 포맷 (256색)

### 메모리 레이아웃

**타일 크기**: 64바이트 (8행 × 8바이트/행)

```
구조: Plane 0-1, 2-3, 4-5, 6-7 순으로 인터리브

[Plane 0, Row 0] [Plane 1, Row 0]  ← Bit 0-1
...
[Plane 0, Row 7] [Plane 1, Row 7]

[Plane 2, Row 0] [Plane 3, Row 0]  ← Bit 2-3
...
[Plane 2, Row 7] [Plane 3, Row 7]

[Plane 4, Row 0] [Plane 5, Row 0]  ← Bit 4-5
...
[Plane 4, Row 7] [Plane 5, Row 7]

[Plane 6, Row 0] [Plane 7, Row 0]  ← Bit 6-7
...
[Plane 6, Row 7] [Plane 7, Row 7]
```

### 상세 레이아웃

```
Offset | Plane | Row | 비트
-------|-------|-----|-----
+0x00  | 0     | 0   | Bit 0
+0x01  | 1     | 0   | Bit 1
...
+0x0F  | 1     | 7   | Bit 1
+0x10  | 2     | 0   | Bit 2
+0x11  | 3     | 0   | Bit 3
...
+0x1F  | 3     | 7   | Bit 3
+0x20  | 4     | 0   | Bit 4
...
+0x2F  | 5     | 7   | Bit 5
+0x30  | 6     | 0   | Bit 6
...
+0x3F  | 7     | 7   | Bit 7
```

### C++ 구현

```cpp
// 8bpp 타일 디코딩
void decode8bpp(const uint8_t* tileData, uint8_t output[8][8]) {
    for (int y = 0; y < 8; y++) {
        // 모든 8개 Plane 읽기
        uint8_t planes[8];
        for (int p = 0; p < 8; p++) {
            planes[p] = tileData[p * 16 + y * 2 + (p & 1)];
        }
        
        for (int x = 0; x < 8; x++) {
            int bitPos = 7 - x;
            uint8_t color = 0;
            
            for (int p = 0; p < 8; p++) {
                if (planes[p] & (1 << bitPos)) {
                    color |= (1 << p);
                }
            }
            
            output[y][x] = color;  // 8비트 색상 인덱스 (0-255)
        }
    }
}
```

### 최적화된 구현 (Lookup Table)

```cpp
// 8비트를 8개 픽셀로 확장하는 테이블 (초기화 시 1번만)
uint8_t bitExpand[256][8];

void initBitExpand() {
    for (int i = 0; i < 256; i++) {
        for (int bit = 0; bit < 8; bit++) {
            bitExpand[i][bit] = (i >> (7 - bit)) & 1;
        }
    }
}

// 8bpp 디코딩 (최적화)
void decode8bppFast(const uint8_t* tileData, uint8_t output[8][8]) {
    for (int y = 0; y < 8; y++) {
        // Plane 데이터 읽기
        uint8_t p0 = tileData[0x00 + y * 2];
        uint8_t p1 = tileData[0x01 + y * 2];
        uint8_t p2 = tileData[0x10 + y * 2];
        uint8_t p3 = tileData[0x11 + y * 2];
        uint8_t p4 = tileData[0x20 + y * 2];
        uint8_t p5 = tileData[0x21 + y * 2];
        uint8_t p6 = tileData[0x30 + y * 2];
        uint8_t p7 = tileData[0x31 + y * 2];
        
        for (int x = 0; x < 8; x++) {
            output[y][x] = bitExpand[p0][x] |
                          (bitExpand[p1][x] << 1) |
                          (bitExpand[p2][x] << 2) |
                          (bitExpand[p3][x] << 3) |
                          (bitExpand[p4][x] << 4) |
                          (bitExpand[p5][x] << 5) |
                          (bitExpand[p6][x] << 6) |
                          (bitExpand[p7][x] << 7);
        }
    }
}
```

---

## VRAM 레이아웃

### Character (타일) 데이터 저장

VRAM에는 여러 타일이 연속으로 저장됩니다.

```
2bpp (각 타일 16바이트):
Tile 0: $0000-$000F
Tile 1: $0010-$001F
Tile 2: $0020-$002F
...

4bpp (각 타일 32바이트):
Tile 0: $0000-$001F
Tile 1: $0020-$003F
Tile 2: $0040-$005F
...

8bpp (각 타일 64바이트):
Tile 0: $0000-$003F
Tile 1: $0040-$007F
Tile 2: $0080-$00BF
...
```

### 타일 번호 → VRAM 주소 변환

```cpp
// 2bpp
uint16_t getTileAddress2bpp(uint16_t tileNum, uint16_t baseAddr) {
    return baseAddr + (tileNum * 16);
}

// 4bpp
uint16_t getTileAddress4bpp(uint16_t tileNum, uint16_t baseAddr) {
    return baseAddr + (tileNum * 32);
}

// 8bpp
uint16_t getTileAddress8bpp(uint16_t tileNum, uint16_t baseAddr) {
    return baseAddr + (tileNum * 64);
}
```

### 실제 배경 렌더링에서의 사용

```cpp
struct TilemapEntry {
    uint16_t tileNum : 10;   // 타일 번호 (0-1023)
    uint16_t palette : 3;    // 팔레트 번호 (0-7)
    uint16_t priority : 1;   // 우선순위
    uint16_t hFlip : 1;      // 수평 뒤집기
    uint16_t vFlip : 1;      // 수직 뒤집기
};

    void renderTile(int screenX, int screenY, TilemapEntry entry, int bpp) {
        // 1. VRAM에서 타일 데이터 주소 계산
        uint16_t tileAddr;
        if (bpp == 2) {
            tileAddr = getTileAddress2bpp(entry.tileNum, bgCharBase);
        } else if (bpp == 4) {
            tileAddr = getTileAddress4bpp(entry.tileNum, bgCharBase);
        } else {
            tileAddr = getTileAddress8bpp(entry.tileNum, bgCharBase);
        }
        
        // 2. 타일 디코딩
        uint8_t pixels[8][8];
        const uint8_t* tileData = &vram[tileAddr];
        
        if (bpp == 2) {
            decode2bpp(tileData, pixels);
        } else if (bpp == 4) {
            decode4bpp(tileData, pixels);
        } else {
            decode8bpp(tileData, pixels);
        }
        
        // 3. 화면에 그리기 (플립 처리 포함)
        for (int y = 0; y < 8; y++) {
            for (int x = 0; x < 8; x++) {
                // 플립 처리
                int srcX = entry.hFlip ? (7 - x) : x;
                int srcY = entry.vFlip ? (7 - y) : y;
                
                int px = screenX + x;
                int py = screenY + y;
                
                uint8_t colorIndex = pixels[srcY][srcX];
                if (colorIndex == 0) continue;  // 투명 (색상 0)
                
                // 4. 팔레트 적용
                uint16_t cgAddr = (entry.palette * 16) + colorIndex;
                uint16_t rgb555 = cgram[cgAddr];
                
                // 5. 최종 픽셀 쓰기
                drawPixel(px, py, rgb555);
            }
        }
    }
```

---

## 타일 → 픽셀 변환

### 전체 프로세스

```
1. Tilemap Entry 읽기
   ↓
2. 타일 번호 → VRAM 주소 변환
   ↓
3. VRAM에서 타일 데이터 읽기 (16/32/64바이트)
   ↓
4. Bitplane 디코딩 → 8×8 색상 인덱스 배열
   ↓
5. 팔레트 번호 + 색상 인덱스 → CGRAM 주소
   ↓
6. CGRAM에서 15비트 RGB 색상 읽기
   ↓
7. 화면 버퍼에 픽셀 쓰기
```

### 예제: 전체 변환 과정

```cpp
// 실제 게임 화면 렌더링 (간소화)
void renderBackground(int bgNum) {
    // 배경 설정 읽기
    int bpp = getBGColorDepth(bgNum);  // 2, 4, 8
    uint16_t charBase = getBGCharBase(bgNum);  // 타일 데이터 시작 주소
    uint16_t mapBase = getBGMapBase(bgNum);    // 타일맵 시작 주소
    int mapWidth = 32;  // 타일 단위 (32×32, 32×64, 64×32, 64×64)
    int mapHeight = 32;
    
    // 스크롤 오프셋
    int scrollX = getBGScrollX(bgNum);
    int scrollY = getBGScrollY(bgNum);
    
    // 화면에 보이는 타일 범위 (256×224 화면 = 32×28 타일)
    int startTileX = scrollX / 8;
    int startTileY = scrollY / 8;
    
    for (int ty = 0; ty < 29; ty++) {
        for (int tx = 0; tx < 33; tx++) {
            int mapX = (startTileX + tx) % mapWidth;
            int mapY = (startTileY + ty) % mapHeight;
            
            // 타일맵 엔트리 읽기 (2바이트)
            uint16_t mapAddr = mapBase + (mapY * mapWidth + mapX) * 2;
            TilemapEntry entry;
            entry.raw = vram[mapAddr] | (vram[mapAddr + 1] << 8);
            
            // 화면 좌표
            int screenX = tx * 8 - (scrollX % 8);
            int screenY = ty * 8 - (scrollY % 8);
            
            // 타일 렌더링
            renderTile(screenX, screenY, entry, bpp);
        }
    }
}
```

---

## 구현 가이드

### PPU 클래스 구조

```cpp
class PPU {
private:
    uint8_t vram[0x10000];      // 64KB VRAM
    uint16_t cgram[256];        // 256색 팔레트 (15비트 RGB)
    uint8_t oam[544];           // OAM (스프라이트 속성)
    
    // 타일 캐시 (최적화)
    struct TileCache {
        uint8_t pixels[8][8];
        bool valid;
    };
    TileCache tileCache2bpp[512];   // 2bpp: 최대 512타일
    TileCache tileCache4bpp[512];   // 4bpp: 최대 512타일
    TileCache tileCache8bpp[256];   // 8bpp: 최대 256타일
    
public:
    // VRAM 쓰기 (자동 캐시 무효화)
    void writeVRAM(uint16_t addr, uint8_t value) {
        if (vram[addr] == value) return;
        vram[addr] = value;
        
        // 영향받는 타일 캐시 무효화
        invalidateTileCache(addr);
    }
    
    // 타일 캐시 무효화
    void invalidateTileCache(uint16_t addr) {
        // 2bpp: 16바이트 단위
        int tile2bpp = addr / 16;
        if (tile2bpp < 512) tileCache2bpp[tile2bpp].valid = false;
        
        // 4bpp: 32바이트 단위
        int tile4bpp = addr / 32;
        if (tile4bpp < 512) tileCache4bpp[tile4bpp].valid = false;
        
        // 8bpp: 64바이트 단위
        int tile8bpp = addr / 64;
        if (tile8bpp < 256) tileCache8bpp[tile8bpp].valid = false;
    }
    
    // 타일 가져오기 (캐시 활용)
    const uint8_t* getTile(uint16_t tileNum, int bpp) {
        TileCache* cache;
        uint16_t addr;
        
        if (bpp == 2) {
            cache = &tileCache2bpp[tileNum];
            addr = tileNum * 16;
            if (cache->valid) return &cache->pixels[0][0];
            decode2bpp(&vram[addr], cache->pixels);
        } else if (bpp == 4) {
            cache = &tileCache4bpp[tileNum];
            addr = tileNum * 32;
            if (cache->valid) return &cache->pixels[0][0];
            decode4bpp(&vram[addr], cache->pixels);
        } else {
            cache = &tileCache8bpp[tileNum];
            addr = tileNum * 64;
            if (cache->valid) return &cache->pixels[0][0];
            decode8bpp(&vram[addr], cache->pixels);
        }
        
        cache->valid = true;
        return &cache->pixels[0][0];
    }
};
```

### 최적화 전략

#### 1. 타일 캐싱
```cpp
// VRAM이 변경될 때만 재디코딩
// 대부분의 게임은 VRAM을 거의 수정하지 않음
```

#### 2. SIMD 최적화 (SSE/AVX)
```cpp
// 8픽셀을 한 번에 처리 (8×8 타일 = 8번 반복)
void decode4bppSIMD(const uint8_t* tileData, uint8_t output[8][8]) {
    __m128i planes[4];
    
    for (int y = 0; y < 8; y++) {
        // 4개 Plane 로드
        uint8_t p0 = tileData[y * 2 + 0];
        uint8_t p1 = tileData[y * 2 + 1];
        uint8_t p2 = tileData[16 + y * 2 + 0];
        uint8_t p3 = tileData[16 + y * 2 + 1];
        
        // 비트 확장 및 조합 (SIMD 명령어 사용)
        // ... (복잡하므로 생략, 실제로는 pshufb, por 등 사용)
    }
}
```

#### 3. Lookup Table
```cpp
// 자주 사용되는 패턴은 미리 계산
static uint8_t bitReverseTable[256];  // 비트 순서 반전
static uint8_t bitExpandTable[256][8];  // 1바이트 → 8픽셀
```

---

## 최적화 팁

### 1. **타일 캐싱**
- VRAM 쓰기 시 캐시 무효화
- 게임 대부분은 VRAM을 VBlank에서만 수정
- 캐시 히트율 > 99%

### 2. **스캔라인 기반 렌더링**
```cpp
// 전체 화면을 한 번에 렌더링하지 말고,
// 스캔라인별로 필요한 타일만 렌더링
void renderScanline(int line) {
    // 이 라인에 영향을 주는 타일만 디코딩
}
```

### 3. **더티 플래그**
```cpp
bool vramDirty[0x10000 / 64];  // 64바이트 단위로 더티 체크

void writeVRAM(uint16_t addr, uint8_t value) {
    vram[addr] = value;
    vramDirty[addr / 64] = true;
}
```

### 4. **병렬 처리**
```cpp
// 멀티코어 CPU에서 배경/스프라이트를 병렬로 렌더링
std::thread bgThread(renderBackgrounds);
std::thread spriteThread(renderSprites);
bgThread.join();
spriteThread.join();
```

### 5. **GPU 가속**
```cpp
// OpenGL/Vulkan/DirectX로 타일 렌더링을 GPU에 오프로드
// 타일을 텍스처로 업로드하고 Shader에서 디코딩
```

---

## 실제 게임 예제

### 슈퍼 마리오 월드

```
배경 레이어:
- BG1 (하늘): 2bpp, 32×32 타일
- BG2 (지형): 4bpp, 64×32 타일
- BG3 (산): 2bpp, 32×32 타일 (낮은 우선순위)

스프라이트:
- 마리오: 4bpp, 16×16 (2×2 타일)
- 적: 4bpp, 16×16 또는 32×32
- 아이템: 4bpp, 8×8

VRAM 사용:
- $0000-$2FFF: BG 타일
- $3000-$5FFF: 스프라이트 타일
- $6000-$7FFF: 타일맵
```

### Chrono Trigger

```
배경:
- Mode 1 사용 (4bpp + 4bpp + 2bpp)
- BG1: 캐릭터/전경 (4bpp)
- BG2: 배경 (4bpp)
- BG3: 원거리 배경 (2bpp)

특수 효과:
- HDMA로 BG3 스크롤 변경 (물결 효과)
- Color Math로 반투명 처리
- Mode 7 (월드맵, 특정 전투)
```

---

## 디버깅 팁

### 타일 뷰어 구현

```cpp
// 모든 타일을 한 화면에 표시
void debugTileViewer(int bpp) {
    int tilesPerRow = 16;
    int tileSize = (bpp == 2) ? 16 : (bpp == 4) ? 32 : 64;
    int maxTiles = (0x10000 / tileSize);
    
    for (int i = 0; i < maxTiles; i++) {
        int tx = (i % tilesPerRow) * 8;
        int ty = (i / tilesPerRow) * 8;
        
        uint8_t pixels[8][8];
        const uint8_t* tileData = &vram[i * tileSize];
        
        if (bpp == 2) decode2bpp(tileData, pixels);
        else if (bpp == 4) decode4bpp(tileData, pixels);
        else decode8bpp(tileData, pixels);
        
        drawTileDebug(tx, ty, pixels);
    }
}
```

### VRAM 덤프

```cpp
// VRAM을 이미지 파일로 저장
void dumpVRAM(const char* filename, int bpp) {
    // PNG/BMP 형식으로 저장
}
```

### 애니메이션 확인

```cpp
// 프레임마다 타일 변화 추적
std::map<uint16_t, std::vector<uint8_t>> tileHistory;

void recordTileChange(uint16_t tileNum) {
    uint8_t pixels[64];
    // ... 타일 디코딩
    tileHistory[tileNum].push_back(/* ... */);
}
```

---

## 요약

### 핵심 요점

1. **Bitplane 인코딩**: 각 색상 비트가 별도 평면에 저장
2. **타일 크기**: 2bpp=16B, 4bpp=32B, 8bpp=64B
3. **디코딩 순서**: Plane 읽기 → 비트 조합 → 색상 인덱스
4. **렌더링 파이프라인**: Tilemap → VRAM → Decode → Palette → Screen

### 구현 체크리스트

- [ ] 3가지 BPP 디코더 구현 (2/4/8)
- [ ] 타일 캐싱 시스템
- [ ] VRAM 쓰기 시 캐시 무효화
- [ ] 타일맵 엔트리 파싱
- [ ] 팔레트 적용
- [ ] 수평/수직 뒤집기 지원
- [ ] 타일 디버거 (Viewer)

---

## 참고 자료

- [SNESDEV Wiki - VRAM](https://wiki.superfamicom.org/vram)
- [SnesLab - VRAM Tile Format](https://sneslab.net/wiki/VRAM)
- [Super NES Programming Manual (Nintendo)](https://archive.org/details/SNESDevManual) - Chapter 4: Graphics
- Mode 7 타일 포맷 (별도 문서)
- OAM 스프라이트 시스템 (별도 문서)

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete
