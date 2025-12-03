# 실제 SNES에서 BPP 설정 방법

## 1. 레지스터 기반 결정

### $2105 (BGMODE) 레지스터
```
Bit 0-2: BG Mode (0-7)
Bit 3:   BG3 Tile Size (0=8x8, 1=16x16)
Bit 4:   BG4 Tile Size (0=8x8, 1=16x16)
Bit 5:   BG1 Tile Size (0=8x8, 1=16x16)
Bit 6:   BG2 Tile Size (0=8x8, 1=16x16)
Bit 7:   Mode 7 ExtBG (Mode 7 전용)
```

### BPP 결정 테이블

| Mode | BG1 | BG2 | BG3 | BG4 | 비고 |
|------|-----|-----|-----|-----|------|
| 0    | 2bpp| 2bpp| 2bpp| 2bpp| 모든 BG 동일 |
| 1    | 4bpp| 4bpp| 2bpp| -   | BG4 미사용 |
| 2    | 4bpp| 4bpp| 2bpp| -   | BG4 미사용 |
| 3    | 8bpp| 4bpp| -   | -   | BG3/BG4 미사용 |
| 4    | 8bpp| 2bpp| -   | -   | BG3/BG4 미사용 |
| 5    | 4bpp| 4bpp| -   | -   | BG3/BG4 미사용 (hires) |
| 6    | 4bpp| -   | -   | -   | BG1만 사용 (hires) |
| 7    | 8bpp| -   | -   | -   | BG1만 사용 (Mode 7 전용) |

## 2. 코드에서의 구현

### PPU::writeRegister(0x2105, value)
```cpp
case 0x2105: { // BGMODE
    m_bgMode = value & 0x07;  // Bit 0-2만 사용
    
    // Mode별 priority 설정 (BPP는 renderBackgroundModeX에서 결정)
    switch (m_bgMode) {
        case 0: // 모든 BG = 2bpp
            // ...
            break;
        case 1: // BG1/BG2=4bpp, BG3=2bpp
            // ...
            break;
    }
}
```

### renderScanline()에서 Mode 확인
```cpp
if (m_bgMode == 0) {
    mainColor = renderBackgroundMode0(x);  // 모든 BG = 2bpp
} else if (m_bgMode == 1) {
    mainColor = renderBackgroundMode1(x);  // BG1/BG2=4bpp, BG3=2bpp
} else if (m_bgMode == 6) {
    mainColor = renderBackgroundMode6(x);  // BG1=4bpp
}
```

### renderBackgroundMode1()에서 BPP 결정
```cpp
// Mode 1: BG1/BG2=4bpp, BG3=2bpp
bgPixels[0] = renderBGx(0, ..., 4);  // BG1 = 4bpp
bgPixels[1] = renderBGx(1, ..., 4);  // BG2 = 4bpp
bgPixels[2] = renderBGx(2, ..., 2);  // BG3 = 2bpp
```

## 3. 실제 SNES 하드웨어 동작

### 하드웨어 레벨
1. **PPU가 $2105 레지스터를 읽음**
   - 렌더링 시마다 BGMODE 레지스터 값을 읽어 Mode 확인
   - Mode에 따라 각 BG의 BPP가 하드웨어적으로 결정됨

2. **타일맵 엔트리 파싱**
   - **2bpp**: Bit 10만 팔레트 비트 (1비트, 팔레트 0-1)
   - **4bpp**: Bit 10-12가 팔레트 비트 (3비트, 팔레트 0-7)
   - **8bpp**: Bit 10-12가 팔레트 비트 (3비트, 팔레트 0-7)

3. **타일 데이터 읽기**
   - **2bpp**: 16바이트 (Plane 0: 8바이트, Plane 1: 8바이트)
   - **4bpp**: 32바이트 (Plane 0-3: 각 8바이트)
   - **8bpp**: 64바이트 (Plane 0-7: 각 8바이트)

4. **CGRAM 인덱스 계산**
   - **2bpp**: `(palette * 4 + colorIndex) * 2`
   - **4bpp**: `(palette * 16 + colorIndex) * 2`
   - **8bpp**: `(palette * 256 + colorIndex) * 2`

## 4. 현재 에뮬레이터 구현

### BPP 결정 흐름
```
CPU: stz $2105 (Mode 0 설정)
  ↓
PPU::writeRegister(0x2105, 0)
  ↓
m_bgMode = 0
  ↓
renderScanline()
  ↓
if (m_bgMode == 0) {
    renderBackgroundMode0(x)
      ↓
    renderBGx(0, ..., 2)  // BG1 = 2bpp
    renderBGx(1, ..., 2)  // BG2 = 2bpp
    renderBGx(2, ..., 2)  // BG3 = 2bpp
    renderBGx(3, ..., 2)  // BG4 = 2bpp
}
```

### Mode 1의 경우
```
CPU: lda #$01; sta $2105 (Mode 1 설정)
  ↓
PPU::writeRegister(0x2105, 1)
  ↓
m_bgMode = 1
  ↓
renderScanline()
  ↓
if (m_bgMode == 1) {
    renderBackgroundMode1(x)
      ↓
    renderBGx(0, ..., 4)  // BG1 = 4bpp
    renderBGx(1, ..., 4)  // BG2 = 4bpp
    renderBGx(2, ..., 2)  // BG3 = 2bpp
}
```

## 5. 요약

**실제 SNES에서 BPP는:**
1. **$2105 (BGMODE) 레지스터의 Bit 0-2 값**에 의해 결정
2. **Mode 값**에 따라 각 BG의 BPP가 **하드웨어적으로 고정**됨
3. **타일맵 엔트리의 팔레트 비트 수**도 BPP에 따라 달라짐
4. **타일 데이터 크기**도 BPP에 따라 달라짐 (16/32/64 바이트)

**에뮬레이터에서는:**
- `m_bgMode` 변수에 Mode 저장
- `renderBackgroundModeX()` 함수에서 각 BG에 적절한 BPP 전달
- `renderBGx(..., bpp)` 함수가 BPP에 따라 타일 디코딩 수행

