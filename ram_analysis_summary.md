# RAM 데이터 종합 분석 결과

## 1. VRAM 타일맵 (0x0000-0x0800)
- **Entry @0x0042**: `0x0052`
  - Tile number: **82** (ASCII 'R')
  - Palette: **0**
  - Expected address: `0x8000 + 82*16 = 0x8520` ✅

- **Entry @0x0044**: `0x0075`
  - Tile number: **117** (ASCII 'u')
  - Palette: **0**
  - Expected address: `0x8000 + 117*16 = 0x8750` ✅

## 2. VRAM 타일 데이터

### Tile 82 @0x8520
```
Data: fc 00 66 00 66 00 7c 00 6c 00 66 00 e6 00 00 00
Format: 2bpp (16 bytes)
Plane 0: bytes 0-7   = fc 66 66 7c 6c 66 e6 00
Plane 1: bytes 8-15  = 00 00 00 00 00 00 00 00
```

**첫 번째 행 디코딩 (y=0):**
- Plane 0 byte 0: `0xfc` = `11111100`
- Plane 1 byte 8: `0x00` = `00000000`
- 픽셀 인덱스 (왼쪽→오른쪽): **1, 1, 1, 1, 1, 1, 0, 0**
- ✅ 픽셀 0-5는 인덱스 1 (색상 있음)
- ✅ 픽셀 6-7는 인덱스 0 (투명)

## 3. CGRAM 팔레트 (2bpp)

### Palette 0 (2bpp = 4 colors)
- **Color 0**: CGRAM[0]|CGRAM[1] = `0x0000` (검은색)
- **Color 1**: CGRAM[2]|CGRAM[3] = `0x1084` (어두운 녹색)
  - RGB: R=0x04, G=0x10, B=0x00
- **Color 2**: CGRAM[4]|CGRAM[5] = `0x2108` (회색)
- **Color 3**: CGRAM[6]|CGRAM[7] = `0x294a` (회색)

### getColor 계산 (palette=0, colorIndex=1, bpp=2)
```
cgramIndex = (0*4 + 1)*2 = 2
snesColor = CGRAM[2] | (CGRAM[3] << 8) = 0x84 | (0x10 << 8) = 0x1084
RGB: R=0x04, G=0x10, B=0x00
```

## 4. PPU 레지스터 상태
- **INIDISP**: `$0F` (Forced blank OFF, brightness=15) ✅
- **BG12NBA**: `$04` (BG1 tiles at 0x4000 word = 0x8000 byte) ✅
- **BG1SC**: `$00` (BG1 map at 0x0000) ✅
- **TM**: `$01` (BG1 enabled) ✅
- **Mode**: 0 (2bpp) ✅

## 5. 문제 분석

### 현재 상태
- ✅ 타일맵: 타일 82, 117 참조 (데이터 있음)
- ✅ 타일 데이터: 0x8520, 0x8750에 데이터 있음
- ✅ CGRAM: 팔레트 0 설정됨
- ✅ BG1: 활성화됨
- ✅ 타일 디코딩: 픽셀 인덱스 1 생성됨

### 가능한 문제
1. **타일이 렌더링되지 않음**: `renderBGx`가 호출되지 않거나 투명으로 처리됨
2. **CGRAM 색상 계산 오류**: `getColor` 함수의 색상 변환 문제
3. **배경색이 빨간색**: CGRAM[0]이 빨간색으로 설정됨
4. **렌더링 루프 문제**: `renderBackgroundMode0`가 제대로 동작하지 않음

## 6. 다음 단계
1. `renderBGx` 호출 여부 확인
2. 픽셀 인덱스가 0이 아닌데도 투명으로 처리되는지 확인
3. `getColor` 함수의 색상 변환 확인
4. 배경색(CGRAM[0]) 확인

