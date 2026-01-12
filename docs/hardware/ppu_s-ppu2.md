# SNES S-PPU2 - Color Math & Window Effects

## 📋 목차
1. [개요](#개요)
2. [S-PPU1 vs S-PPU2](#s-ppu1-vs-s-ppu2)
3. [Window Masking](#window-masking)
4. [Color Math](#color-math)
5. [화면 밝기 제어](#화면-밝기-제어)
6. [모자이크 효과](#모자이크-효과)
7. [Sub Screen](#sub-screen)
8. [고해상도 모드](#고해상도-모드)
9. [구현 가이드](#구현-가이드)
10. [실제 게임 예제](#실제-게임-예제)

---

## 개요

**S-PPU2**는 SNES의 두 번째 PPU 칩으로, 색상 연산(Color Math)과 특수 효과를 담당합니다. S-PPU1이 기본 렌더링을 하면, S-PPU2가 최종 색상을 결정합니다.

### 주요 기능

| 기능 | 설명 |
|------|------|
| **Color Math** | 픽셀 간 덧셈/뺄셈/반투명 |
| **Window Masking** | 화면 일부를 마스킹 (2개 윈도우) |
| **Brightness** | 화면 밝기 조절 (페이드 인/아웃) |
| **Mosaic** | 픽셀화 효과 |
| **Sub Screen** | 메인/서브 화면 분리 |
| **Hi-Res** | 512픽셀 고해상도 모드 |

---

## S-PPU1 vs S-PPU2

### 역할 분담

```
┌─────────────────────────────────┐
│ S-PPU1                          │
├─────────────────────────────────┤
│ - 타일 렌더링                    │
│ - 스프라이트 렌더링              │
│ - Mode 7 변환                    │
│ - OAM 관리                       │
│ - 배경 스크롤                    │
│ → 출력: 메인 화면 (Main Screen)  │
└─────────────────────────────────┘
            ↓
┌─────────────────────────────────┐
│ S-PPU2                          │
├─────────────────────────────────┤
│ - Window Masking                │
│ - Color Math (덧셈/뺄셈)        │
│ - 밝기 조절 (Brightness)         │
│ - 모자이크 효과                  │
│ - Sub Screen 처리               │
│ → 출력: 최종 RGB 색상            │
└─────────────────────────────────┘
            ↓
        TV 출력
```

### 관련 레지스터

**S-PPU2 레지스터** ($2100-$2133):
```
$2100: INIDISP   - 화면 밝기 및 강제 공백
$2105: BGMODE    - 모자이크 설정 (일부)
$2106: MOSAIC    - 모자이크 크기

$2123-$2125: W12SEL, W34SEL, WOBJSEL - Window 설정
$2126-$212B: WH0-WH3, WBGLOG, WOBJLOG - Window 위치 및 논리
$212E-$212F: TM, TS - 메인/서브 화면 활성화
$2130: CGWSEL    - Color Math Window
$2131: CGADSUB   - Color Math 설정
$2132: COLDATA   - 고정 색상
$2133: SETINI    - 화면 모드
```

---

## Window Masking

### 개념

**Window**는 화면의 특정 영역을 마스킹하는 사각형입니다. 배경/스프라이트/Color Math를 영역별로 켜거나 끌 수 있습니다.

```
화면 (256 픽셀)
┌─────────────────────────────────┐
│                                 │
│    Window 1    Window 2         │
│    ┌─────┐     ┌──────┐        │
│    │#####│     │######│        │
│    │#####│     │######│        │
│    └─────┘     └──────┘        │
│                                 │
└─────────────────────────────────┘

마스크된 영역(#)에서는 배경/스프라이트가 표시되지 않거나
Color Math가 적용되지 않음
```

### 2개의 윈도우

SNES는 **Window 1**과 **Window 2**를 제공하며, 논리 연산으로 결합할 수 있습니다.

**레지스터**:
```
$2126 (WH0): Window 1 Left Position (0-255)
$2127 (WH1): Window 1 Right Position (0-255)
$2128 (WH2): Window 2 Left Position (0-255)
$2129 (WH3): Window 2 Right Position (0-255)
```

**영역 정의**:
```
Window 1: X가 [WH0, WH1] 범위에 있으면 "안"
Window 2: X가 [WH2, WH3] 범위에 있으면 "안"
```

### Window 활성화

**$2123 (W12SEL)** - BG1/BG2 Window 설정:
```
Bit 7-6: BG2 Window 2 영역 (00=Outside, 01=Inside, 10=Off, 11=On)
Bit 5: BG2 Window 2 Enable
Bit 4: BG2 Window 1 Enable
Bit 3-0: BG1 동일 설정
```

**$2124 (W34SEL)** - BG3/BG4 Window 설정 (동일 구조)

**$2125 (WOBJSEL)** - OBJ/Color Window 설정:
```
Bit 7-4: Color Window
Bit 3-0: OBJ (Sprite) Window
```

### Window 논리 연산

**$212A (WBGLOG)** - BG1/BG2 Window 논리:
```
Bit 3-2: BG2 논리 (00=OR, 01=AND, 10=XOR, 11=XNOR)
Bit 1-0: BG1 논리
```

**$212B (WOBJLOG)** - BG3/BG4/OBJ/Color 논리 (동일 구조)

**논리 연산 설명**:
```
OR:   W1 | W2  (둘 중 하나라도 안이면 마스크)
AND:  W1 & W2  (둘 다 안이어야 마스크)
XOR:  W1 ^ W2  (하나만 안이면 마스크)
XNOR: ~(W1 ^ W2)  (둘 다 같으면 마스크)
```

### 예제 코드

```asm
; Window 1을 X=64-192 범위로 설정
LDA #64
STA $2126    ; WH0 (Left)
LDA #192
STA $2127    ; WH1 (Right)

; BG1에 Window 1 적용 (Inside 영역 마스크)
LDA #$02     ; Enable Window 1, Inside
STA $2123    ; W12SEL

; BG1 Window 논리는 OR (기본값)
LDA #$00
STA $212A    ; WBGLOG
```

### C++ 구현

```cpp
class WindowMask {
private:
    uint8_t w1Left, w1Right;
    uint8_t w2Left, w2Right;
    
    struct LayerWindow {
        bool enable1, enable2;
        bool invert1, invert2;  // Inside vs Outside
        uint8_t logic;  // 0=OR, 1=AND, 2=XOR, 3=XNOR
    };
    
    LayerWindow bg[4];
    LayerWindow obj;
    LayerWindow color;
    
public:
    void setWindowPosition(int window, uint8_t left, uint8_t right) {
        if (window == 1) {
            w1Left = left;
            w1Right = right;
        } else {
            w2Left = left;
            w2Right = right;
        }
    }
    
    bool isInWindow(int x, int window) {
        if (window == 1) {
            return (x >= w1Left && x <= w1Right);
        } else {
            return (x >= w2Left && x <= w2Right);
        }
    }
    
    bool isPixelMasked(int x, int layer) {
        LayerWindow& w = (layer < 4) ? bg[layer] : (layer == 4 ? obj : color);
        
        bool in1 = isInWindow(x, 1);
        bool in2 = isInWindow(x, 2);
        
        // Invert 처리
        if (w.invert1) in1 = !in1;
        if (w.invert2) in2 = !in2;
        
        // 논리 연산
        bool masked = false;
        if (w.enable1 && w.enable2) {
            switch (w.logic) {
                case 0: masked = in1 || in2; break;  // OR
                case 1: masked = in1 && in2; break;  // AND
                case 2: masked = in1 != in2; break;  // XOR
                case 3: masked = in1 == in2; break;  // XNOR
            }
        } else if (w.enable1) {
            masked = in1;
        } else if (w.enable2) {
            masked = in2;
        }
        
        return masked;
    }
};
```

---

## Color Math

### 개념

**Color Math**는 두 화면(Main/Sub)의 색상을 연산하여 반투명, 밝기 변화, 색상 혼합 효과를 만듭니다.

```
메인 화면 (Main Screen)
    +  (또는 -)
서브 화면 (Sub Screen 또는 고정 색상)
    =
최종 색상
```

### 메인/서브 화면

**$212E (TM)** - Main Screen Designation:
```
Bit 4: OBJ (Sprite)
Bit 3: BG4
Bit 2: BG3
Bit 1: BG2
Bit 0: BG1

1 = 해당 레이어를 메인 화면에 표시
```

**$212F (TS)** - Sub Screen Designation (동일 구조):
```
1 = 해당 레이어를 서브 화면에 표시
```

### Color Math 설정

**$2130 (CGWSEL)** - Color Math Window:
```
Bit 7-6: Color Math Window Clip (00=Never, 01=Outside, 10=Inside, 11=Always)
Bit 5-4: Prevent Color Math (00=Never, 01=Outside, 10=Inside, 11=Always)
Bit 1: Direct Color (Mode 3/4/7 전용)
Bit 0: Add Subscreen (0=Backdrop, 1=Subscreen)
```

**$2131 (CGADSUB)** - Color Math Control:
```
Bit 7: Half Color Math (결과 / 2)
Bit 6: Subtract (0=Add, 1=Subtract)
Bit 5: OBJ에 Color Math 적용
Bit 4: BG4에 Color Math 적용
Bit 3: BG3에 Color Math 적용
Bit 2: BG2에 Color Math 적용
Bit 1: BG1에 Color Math 적용
Bit 0: Backdrop에 Color Math 적용
```

**$2132 (COLDATA)** - Fixed Color:
```
고정 색상 (서브 화면 대신 사용 가능)

Bit 7: Blue 수정
Bit 6: Green 수정
Bit 5: Red 수정
Bit 4-0: 색상 값 (0-31)

여러 번 쓰면 R/G/B를 개별 설정 가능
```

### Color Math 알고리즘

```cpp
struct RGB555 {
    uint8_t r, g, b;  // 0-31
    
    RGB555(uint16_t color) {
        r = color & 0x1F;
        g = (color >> 5) & 0x1F;
        b = (color >> 10) & 0x1F;
    }
    
    uint16_t toColor() {
        return r | (g << 5) | (b << 10);
    }
};

RGB555 applyColorMath(RGB555 main, RGB555 sub, bool subtract, bool half) {
    RGB555 result;
    
    if (subtract) {
        // 뺄셈 (클램핑)
        result.r = (main.r > sub.r) ? (main.r - sub.r) : 0;
        result.g = (main.g > sub.g) ? (main.g - sub.g) : 0;
        result.b = (main.b > sub.b) ? (main.b - sub.b) : 0;
    } else {
        // 덧셈 (클램핑)
        result.r = std::min(31, main.r + sub.r);
        result.g = std::min(31, main.g + sub.g);
        result.b = std::min(31, main.b + sub.b);
    }
    
    if (half) {
        result.r >>= 1;
        result.g >>= 1;
        result.b >>= 1;
    }
    
    return result;
}
```

### 반투명 효과 (50% 블렌딩)

```asm
; BG1 (메인)과 BG2 (서브)를 50% 블렌딩
; 1. BG1을 메인 화면에
LDA #$01
STA $212E    ; TM = BG1

; 2. BG2를 서브 화면에
LDA #$02
STA $212F    ; TS = BG2

; 3. Color Math: BG1에 적용, Half, Add
LDA #$82     ; Bit7=Half, Bit1=BG1
STA $2131    ; CGADSUB

; 4. 서브 화면 사용
LDA #$01     ; Add Subscreen
STA $2130    ; CGWSEL

; 결과: BG1과 BG2가 50% 반투명으로 겹침
```

### 페이드 인 효과

```asm
; 화면을 검정색으로 페이드 (서서히 어둡게)
; 고정 색상 = 검정 (0, 0, 0)
LDA #$00
STA $2132    ; COLDATA = 검정

; 모든 레이어에 Color Math 적용 (Subtract)
LDA #$7F     ; Bit6=Subtract, Bit0-5=All layers
STA $2131

; 루프: 서서히 검정색 양 증가
; (페이드 0% → 100%)
```

---

## 화면 밝기 제어

**$2100 (INIDISP)** - Display Control:
```
Bit 7: Force Blank (1=화면 끔, 0=화면 켬)
Bit 3-0: Brightness (0-15)
  0 = 완전히 어두움 (검정)
  15 = 최대 밝기
```

### 밝기 적용

```cpp
uint16_t applyBrightness(uint16_t color, int brightness) {
    if (brightness == 0) return 0;  // 완전히 검정
    
    RGB555 rgb(color);
    
    // 밝기 적용 (0-15 → 0-1.0)
    float factor = brightness / 15.0f;
    
    rgb.r = (uint8_t)(rgb.r * factor);
    rgb.g = (uint8_t)(rgb.g * factor);
    rgb.b = (uint8_t)(rgb.b * factor);
    
    return rgb.toColor();
}
```

### Force Blank

**Force Blank**는 화면을 완전히 끄고 검정 화면을 표시합니다. VBlank 외부에서 VRAM을 안전하게 수정할 수 있습니다.

```asm
; VRAM 업로드 전에 Force Blank
LDA #$80     ; Bit 7 = 1
STA $2100    ; INIDISP

; ... VRAM 쓰기 ...

; Force Blank 해제
LDA #$0F     ; Bit 7 = 0, Brightness = 15
STA $2100
```

---

## 모자이크 효과

**$2106 (MOSAIC)** - Mosaic Control:
```
Bit 7-4: Mosaic Size (0-15, 실제 = Size + 1)
  0 = 1×1 (효과 없음)
  1 = 2×2
  2 = 3×3
  ...
  15 = 16×16

Bit 3: BG4 Mosaic Enable
Bit 2: BG3 Mosaic Enable
Bit 1: BG2 Mosaic Enable
Bit 0: BG1 Mosaic Enable
```

### 모자이크 원리

```
원본 (1×1):
ABCDEFGH
IJKLMNOP
QRSTUVWX

모자이크 (4×4):
AAAA EEEE
AAAA EEEE
AAAA EEEE
AAAA EEEE

(4×4 블록의 왼쪽 위 픽셀로 전체 블록을 채움)
```

### C++ 구현

```cpp
void applyMosaic(uint8_t* framebuffer, int width, int height, int size) {
    int blockSize = size + 1;  // 0 → 1, 1 → 2, ..., 15 → 16
    
    for (int y = 0; y < height; y += blockSize) {
        for (int x = 0; x < width; x += blockSize) {
            // 블록의 첫 픽셀 색상 가져오기
            uint16_t color = getPixel(framebuffer, x, y);
            
            // 블록 전체를 같은 색으로 채움
            for (int by = 0; by < blockSize && (y + by) < height; by++) {
                for (int bx = 0; bx < blockSize && (x + bx) < width; bx++) {
                    setPixel(framebuffer, x + bx, y + by, color);
                }
            }
        }
    }
}
```

### 예제 (전환 효과)

```asm
; 화면 전환 시 모자이크로 흐릿하게
; 1. 모자이크 없음
LDA #$00
STA $2106

; 2. 서서히 증가 (루프)
; Size = 0 → 15
LDA #$10     ; Size=1 (2×2)
STA $2106

; ...

LDA #$F0     ; Size=15 (16×16)
STA $2106

; 3. 새 화면 로드

; 4. 모자이크 감소
LDA #$00
STA $2106
```

---

## Sub Screen

### 메인 vs 서브

**메인 화면** (Main Screen):
- 기본적으로 표시되는 화면
- Color Math의 "기준" 색상

**서브 화면** (Sub Screen):
- Color Math에서 "추가/차감"할 색상
- 메인과 독립적으로 레이어 선택 가능

### 사용 사례

#### 1. 반투명 스프라이트

```asm
; 스프라이트를 배경 위에 50% 반투명으로 표시

; BG1을 메인과 서브 모두에
LDA #$01
STA $212E    ; TM = BG1
STA $212F    ; TS = BG1

; 스프라이트는 메인에만
LDA #$10
STA $212E    ; TM |= OBJ

; Color Math: OBJ에 적용, Half
LDA #$A0     ; Bit7=Half, Bit5=OBJ
STA $2131
```

#### 2. 그림자 효과

```asm
; 스프라이트 아래에 어두운 그림자

; 메인 화면: BG + OBJ
LDA #$1F
STA $212E

; 서브 화면: 검정 (고정 색상)
LDA #$00
STA $2132    ; Black

; Color Math: OBJ에만 적용, Subtract
LDA #$60     ; Bit6=Sub, Bit5=OBJ
STA $2131

; 결과: 스프라이트가 배경을 어둡게 함
```

---

## 고해상도 모드

**$2133 (SETINI)** - Screen Mode:
```
Bit 7: External Sync (항상 0)
Bit 6: Mode 7 EXTBG
Bit 3: Pseudo Hi-Res (512×224)
Bit 2: Overscan (239 scanlines)
Bit 1: OBJ Interlace
Bit 0: Screen Interlace
```

### Pseudo Hi-Res (512픽셀)

**Bit 3 = 1**: 512×224 모드

```
원리:
- 메인 화면: 홀수 픽셀 (0, 2, 4, ...)
- 서브 화면: 짝수 픽셀 (1, 3, 5, ...)
- 최종: 교대로 배치하여 512픽셀

메인: A - B - C - D
서브: - 1 - 2 - 3 - 4
결과: A 1 B 2 C 3 D 4  (512픽셀)
```

**주의**: Color Math를 사용하면 안 됨 (픽셀이 혼합되어 512픽셀 효과 상실)

### 인터레이스 모드

**Bit 0 = 1**: 240p → 480i (격자 무늬 제거)

```
프레임 1: 짝수 라인 (0, 2, 4, ...)
프레임 2: 홀수 라인 (1, 3, 5, ...)

CRT TV에서 480i 해상도로 표시
```

---

## 구현 가이드

### 전체 렌더링 파이프라인

```cpp
class PPU2 {
public:
    uint16_t renderPixel(int x, int y) {
        // 1. S-PPU1에서 메인/서브 화면 픽셀 가져오기
        uint16_t mainColor = ppu1.getMainPixel(x, y);
        uint16_t subColor = ppu1.getSubPixel(x, y);
        
        // 2. Window Masking 적용
        if (windowMask.isPixelMasked(x, currentLayer)) {
            return backdrop;  // 또는 투명
        }
        
        // 3. Color Math 적용
        bool applyColorMath = (cgadsub & (1 << currentLayer));
        if (applyColorMath && !colorMathWindow.isMasked(x)) {
            bool subtract = (cgadsub & 0x40);
            bool half = (cgadsub & 0x80);
            
            RGB555 main(mainColor);
            RGB555 sub = useSubScreen ? RGB555(subColor) : fixedColor;
            
            mainColor = applyColorMath(main, sub, subtract, half).toColor();
        }
        
        // 4. Brightness 적용
        mainColor = applyBrightness(mainColor, brightness);
        
        // 5. Mosaic 적용 (실제로는 렌더링 전에)
        // ...
        
        return mainColor;
    }
};
```

### 레지스터 쓰기 핸들러

```cpp
void PPU::writeRegister(uint16_t addr, uint8_t value) {
    switch (addr) {
        case 0x2100:  // INIDISP
            forceBlank = (value & 0x80) != 0;
            brightness = value & 0x0F;
            break;
            
        case 0x2106:  // MOSAIC
            mosaicSize = (value >> 4) & 0x0F;
            mosaicEnableBG[0] = (value & 0x01) != 0;
            mosaicEnableBG[1] = (value & 0x02) != 0;
            mosaicEnableBG[2] = (value & 0x04) != 0;
            mosaicEnableBG[3] = (value & 0x08) != 0;
            break;
            
        case 0x2126:  // WH0
            window1Left = value;
            break;
        case 0x2127:  // WH1
            window1Right = value;
            break;
        case 0x2128:  // WH2
            window2Left = value;
            break;
        case 0x2129:  // WH3
            window2Right = value;
            break;
            
        case 0x212E:  // TM
            for (int i = 0; i < 5; i++) {
                mainScreenEnable[i] = (value & (1 << i)) != 0;
            }
            break;
            
        case 0x212F:  // TS
            for (int i = 0; i < 5; i++) {
                subScreenEnable[i] = (value & (1 << i)) != 0;
            }
            break;
            
        case 0x2130:  // CGWSEL
            colorMathWindowClip = (value >> 6) & 0x03;
            preventColorMath = (value >> 4) & 0x03;
            directColor = (value & 0x02) != 0;
            addSubscreen = (value & 0x01) != 0;
            break;
            
        case 0x2131:  // CGADSUB
            halfColorMath = (value & 0x80) != 0;
            subtractColor = (value & 0x40) != 0;
            for (int i = 0; i < 6; i++) {
                colorMathEnable[i] = (value & (1 << i)) != 0;
            }
            break;
            
        case 0x2132:  // COLDATA
            if (value & 0x20) fixedColor.r = value & 0x1F;
            if (value & 0x40) fixedColor.g = value & 0x1F;
            if (value & 0x80) fixedColor.b = value & 0x1F;
            break;
            
        case 0x2133:  // SETINI
            pseudoHiRes = (value & 0x08) != 0;
            overscan = (value & 0x04) != 0;
            objInterlace = (value & 0x02) != 0;
            screenInterlace = (value & 0x01) != 0;
            break;
    }
}
```

---

## 실제 게임 예제

### Chrono Trigger - 시간 포털 효과

```
효과: 중앙에서 퍼져나가는 파동

구현:
1. BG1: 일반 배경
2. BG2: 포털 중심
3. Window: 원형 마스크 (HDMA로 매 라인 변경)
4. Color Math: BG2를 50% 반투명으로 BG1에 혼합
```

### Super Metroid - X-Ray 효과

```
효과: 특정 영역만 투시

구현:
1. 메인: 일반 타일
2. 서브: 숨겨진 타일
3. Window: 원형 (캐릭터 중심)
4. Window 안에서만 Color Math 적용
```

### Final Fantasy VI - 페이드 아웃

```
효과: 화면이 서서히 검정으로

구현:
1. 고정 색상 = 검정 (0, 0, 0)
2. 모든 레이어에 Color Math (Subtract)
3. 프레임마다 고정 색상 증가 (0 → 31)
```

### Super Mario World - 반투명 유령

```
효과: 유령 스프라이트가 반투명

구현:
1. 메인: BG + OBJ
2. 서브: BG만 (유령 없음)
3. Color Math: OBJ에만 적용, Half
4. 결과: 유령이 배경과 50% 블렌딩
```

---

## 디버깅

### 화면 분리 디버거

```cpp
void debugScreens() {
    // 메인 화면 (왼쪽 절반)
    for (int y = 0; y < 224; y++) {
        for (int x = 0; x < 128; x++) {
            uint16_t color = ppu1.getMainPixel(x, y);
            drawPixel(x, y, color);
        }
    }
    
    // 서브 화면 (오른쪽 절반)
    for (int y = 0; y < 224; y++) {
        for (int x = 0; x < 128; x++) {
            uint16_t color = ppu1.getSubPixel(x, y);
            drawPixel(128 + x, y, color);
        }
    }
}
```

### Window 시각화

```cpp
void debugWindows() {
    for (int y = 0; y < 224; y++) {
        for (int x = 0; x < 256; x++) {
            bool masked = windowMask.isPixelMasked(x, 0);  // BG1
            drawPixel(x, y, masked ? 0x7FFF : 0x0000);  // White or Black
        }
    }
}
```

---

## 참고 자료

- [SnesLab - Color Math](https://sneslab.net/wiki/Color_math)
- [SnesLab - Window Masking](https://sneslab.net/wiki/Window)
- [SNESdev - PPU](https://snes.nesdev.org/wiki/PPU)
- [Fullsnes - Video](https://problemkaputt.de/fullsnes.htm#snesvideo)

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete










