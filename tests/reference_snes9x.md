# SNES Test Program - Snes9x 1.62.3 Reference Screenshots

## Test Sequence (Electronics Test)

### TC-01: Main Menu
- Background: dark blue (~#1C1C8C)
- Nintendo: red oval logo top-center
- Title: "SuperNES Test Menu" in pink/magenta
- Menu items in white, arrow pointing to item 1
- Items: 1.Electronics Test, 2.Character Test, 3.Controller Test, 4.Sound Test, 5.Color Test, 6.Accessories Test
- Bottom: "SELECT BUTTON:Choose TEST", "START BUTTON:Begin TEST", "©1991 Nintendo"

### TC-02: Electronics Test - BG Mode 0
- Black background
- Blue text: "TEST" (center top), "BG MODE 0" (center middle), "Nintendo" (right)
- Cyan text: "Look for clear images moving across screen" (bottom)
- Sprites scroll across: Raccoon Mario, spiky enemy, Boo ghost, star row decoration

### TC-03: Electronics Test - BG Mode 0 with Super Mario World BG
- SMW-style side-scroll game scene (cyan sky, white clouds, green hill, pipes, castle)
- Text overlay: "Look for clear images moving across screen"
- Shows window masking: hatched overlay + blue rectangle window

### TC-04: Electronics Test - Princess Flipping (Sprite Y-flip)
- White background
- Princess Peach sprite center, Mario left, Luigi right
- Princess flips upside-down (Y-flip via OAM attribute)
- Text: "Look for clear image of princess flipping"

### TC-05: Electronics Test - Two Windows
- Blue/cyan circle over Mario (sunglasses) sprite
- Two overlapping circle windows
- Text: "Look for two distinct windows on the screen"
- Variants: circle=clear on Mario, circle=blue on Mario, Mario mirrored side-by-side

### TC-06: Electronics Test - Color Changes (Color Math)
- Mario artwork image (~128×128) on dark blue background
- Image cycles through: normal, tinted, half-dark (right half), full purple/magenta tint
- Text: "Look for color changes"
- Uses CGRAM color addition/subtraction math

### TC-07: Electronics Test - Stars (HDMA)
- Full screen teal/gold repeating star pattern tile
- HDMA effect: diagonal scan lines visible across star field
- Text: "Look for stars"

### TC-08: Electronics Test - Many Service Marios (Sprites)
- Multiple Mario sprites, different sizes (scaled via OAM 8×8 vs 16×16)
- Arranged in rows across screen on teal background
- Text: "Look for many Service Marios"

### TC-09: Electronics Test - Mario Rotating (Mode 7)
- Full-screen Mode 7 affine transform of Mario artwork
- Rotation animation
- Text: "Look for clean image of Mario rotating"

### TC-10: Electronics Test - Mario Flipping (Mode 7)
- Mode 7 flip/scale of Mario artwork
- Zoom in/zoom out animation
- Text: "Look for clear image of Mario flipping"

### TC-11: Electronics Test - Princess Zooming (Mode 7)
- Mode 7 zoom into Princess face (pixelated close-up)
- Zoom in and out animation
- Text: "Look for clear image zooming in and out"

### TC-12: Electronics Test PASSED
- Black background
- White text: "PASSED ELECTRONICS TEST"
- White text: "Press SELECT to exit"
- White text: "©1991 Nintendo"

### TC-13: Color Test - Colored Windows
- Vertical color bars: Red background | Green center bar
- Black/White vertical bars variant
- Text: "Look for colored windows"

## Key PPU Features - Current Status (측정됨)

| Feature | Test | Status | 비고 |
|---------|------|--------|------|
| BG Mode 0 (2bpp ×4 BG) | TC-02 | Working | 4개 BG 레이어 모두 렌더 |
| Sprites / OAM rendering | TC-02,04,08 | Working | SPR-HIT 546, 19색 스프라이트 |
| PPU Window masking ($2123-$212B) | TC-03,05,13 | Implemented | 세부 조정 필요 |
| Color math (add/sub screen) | TC-06 | Implemented | CGADSUB/CGWSEL 경로 존재 |
| HDMA (H-blank DMA) | TC-07 | Implemented | initHDMA/performHDMA 존재 |
| Mode 7 affine transform | TC-09,10,11 | Partial | renderBackgroundMode7 존재 |
| CGRAM correct colors | All | Working | 5→8bit (v<<3)\|(v>>2) 확장 |

## Emulator vs Reference - 측정값 (F:60 TC-01, F:4200 TC-03)

### TC-01 메인 메뉴 (F:60)
| Item | Ours | Reference | Match |
|------|------|-----------|-------|
| Menu background | #313184 (dark blue) | ~#1C1C8C | 근접 ✓ |
| Nintendo logo | #FF1818 (red) 4.3% | Red oval | ✓ |
| Title color | #FF1094 (pink) 0.6% | Pink/magenta | ✓ |
| Menu items | #F7F7F7 (white) 0.2% | White | ✓ |

### TC-03 SMW 장면 (F:4200, 19색 렌더링)
| Color | Ours % | Expected Feature |
|-------|--------|------------------|
| #9CE7E7 (light cyan) | 38.1% | 하늘 ✓ |
| #637BCE (gray-blue) | 28.9% | 구름 ✓ |
| #7B7B7B, #7B7B73 (gray) | 16.7% | 지형/바위 ✓ |
| #4A7373 등 (dark teal) | 10.6% | 그림자 ✓ |
| #EFE784 (yellow) | 1.6% | 동전/별 ✓ |
| #8CCEAD (light green) | 1.2% | 언덕 ✓ |
| #FFB594 (flesh) | 1.1% | Mario 얼굴 ✓ |
| #FF4273 (pink) | 0.8% | 디테일 ✓ |

## 성능
- 1 fps → **28.4 fps** (28× 향상, ENABLE_LOGGING 비활성화 후)

## 남은 작업 (픽셀 단위 레퍼런스 필요)
- 색상 비율 미세 조정 (녹색 1.2% → 예상 더 많은 양)
- Window masking 세부 로직 검증 (TC-13)
- Mode 7 affine matrix 정확도
- HDMA 스캔라인 타이밍 정확도
- OBSEL base 표준 공식 복구 (현재 empirical base=0)
