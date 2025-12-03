# RAM 데이터 종합 분석 결과

## 1. VRAM 타일맵 (0x0000-0x0800)
- **Entry @0x0042**: `0x0052` = tile 82, palette 0 ✅
- **Entry @0x0044**: `0x0075` = tile 117, palette 0 ✅

## 2. VRAM 타일 데이터

### Tile 82 @0x8520
```
Data: fc 00 66 00 66 00 7c 00 6c 00 66 00 e6 00 00 00
Plane 0 (bytes 0-7):   fc 66 66 7c 6c 66 e6 00
Plane 1 (bytes 8-15):  00 00 00 00 00 00 00 00
```

**첫 번째 행 (y=0) 디코딩:**
- Plane 0 byte 0: `0xfc` = `11111100`
- Plane 1 byte 8: `0x00` = `00000000`
- 픽셀 인덱스: **1, 1, 1, 1, 1, 1, 0, 0** ✅
- 픽셀 0-5는 인덱스 1 (색상 있음)
- 픽셀 6-7는 인덱스 0 (투명)

## 3. CGRAM 팔레트

### Palette 0 (2bpp = 4 colors)
- **Color 0**: CGRAM[0]|CGRAM[1] = `0x0000` (검은색) ✅
- **Color 1**: CGRAM[2]|CGRAM[3] = `0x1084` (어두운 녹색)
  - RGB: R=0x04, G=0x10, B=0x00
  - RGBA: `0xFF200820` (어두운 녹색)

### getColor 계산
```
palette=0, colorIndex=1, bpp=2
cgramIndex = (0*4 + 1)*2 = 2
snesColor = CGRAM[2] | (CGRAM[3] << 8) = 0x84 | (0x10 << 8) = 0x1084
RGB: R=0x20, G=0x108 (오버플로우!), B=0x20
```

## 4. PPU 레지스터
- INIDISP: `$0F` (Forced blank OFF) ✅
- BG12NBA: `$04` (BG1 tiles at 0x8000) ✅
- BG1SC: `$00` (BG1 map at 0x0000) ✅
- TM: `$01` (BG1 enabled) ✅
- Mode: 0 (2bpp) ✅

## 5. 문제 발견

### CGRAM 색상 계산 오류
```cpp
uint8_t g = ((snesColor >> 5) & 0x1F) << 3;
```

**0x1084의 경우:**
- `(0x1084 >> 5) & 0x1F` = `0x21 & 0x1F` = `0x01` ✅
- `0x01 << 3` = `0x08` ✅

**하지만 실제 계산:**
- `0x1084 >> 5` = `0x84` (잘못됨!)
- `0x84 & 0x1F` = `0x04`
- `0x04 << 3` = `0x20`

**실제로는:**
- `0x1084 >> 5` = `0x84`가 아니라 `0x21`이어야 함
- `0x21 & 0x1F` = `0x01`
- `0x01 << 3` = `0x08`

### 배경색 확인
- CGRAM[0] = `0x00`, CGRAM[1] = `0x00`
- 배경색 = `0x0000` (검은색) ✅
- 빨간색이 나오는 이유: 타일이 렌더링되지 않아 배경색이 표시되거나, 다른 문제

## 6. 다음 확인 사항
1. `renderBGx`가 실제로 호출되는지
2. `bgPixels[0].color`가 0이 아닌지
3. `finalColor`가 0이 되는 이유
4. 타일이 투명으로 처리되는 이유

