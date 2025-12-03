# 폰트 렌더링 조건 분석

## 문제 상황
- Snes9x에서는 "Running tests..." 텍스트가 올바르게 표시됨
- 우리 에뮬레이터에서는 글씨가 깨져서 표시됨
- font.bin은 정상적으로 로드됨

## VRAM 데이터 구조

### 타일 데이터 저장 위치
- **BG12NBA=$04**: BG1 tiles at word $4000 = byte $8000
- **폰트 DMA 복사**: word $4000 (byte $8000)부터 2048 bytes
- **타일 번호 82 (R)**: 0x8000 + (82 * 16) = 0x8520

### VRAM 덤프 분석
```
8520: fc 00 66 00 66 00 7c 00 6c 00 66 00 e6 00 00 00
```

이것은 **word 단위로 저장**된 것으로 보입니다:
- 0x8520: 0x00FC (word)
- 0x8522: 0x0066 (word)
- 0x8524: 0x0066 (word)
- ...

### 2bpp 타일 구조
2bpp 타일은 16 bytes로 구성:
- **Plane 0 (bytes 0-7)**: 각 라인의 하위 비트
- **Plane 1 (bytes 8-15)**: 각 라인의 상위 비트

### VMAIN=$80 설정
```asm
lda #$80
sta $2115  ; VMAIN: inc address by 1 after high byte
```
- **Bit 7 = 1**: Increment after high byte write
- **Bit 0-1 = 00**: Increment by 1

### DMA 설정
```asm
lda #$01
sta $4300  ; DMA0: to ppu, 2 bytes->2 registers, inc by 1
lda #$18
sta $4301  ; Write to PPU 2118/2119 (VMDATA)
```

DMA가 $2118/$2119에 쓸 때:
1. $2118 (low byte)에 쓰기
2. $2119 (high byte)에 쓰기
3. VMAIN=$80이므로 high byte 후에 주소 증가

## 타일 데이터 읽기

### 현재 구현 (renderBGx)
```cpp
uint8_t plane0_byte = m_vram[tileAddr + line_offset];
uint8_t plane1_byte = m_vram[tileAddr + line_offset + 8];
```

이것은 **byte 단위로 읽는 것**이므로 올바릅니다.

### 문제 가능성

#### 1. VRAM 주소 변환
```cpp
writeVRAM(m_vramAddress * 2, value);  // Convert word address to byte address
writeVRAM(m_vramAddress * 2 + 1, value);
```

- `m_vramAddress`는 word 주소
- `* 2`로 byte 주소로 변환
- 이것은 올바릅니다.

#### 2. 타일 데이터 디코딩
현재 구현:
```cpp
uint8_t pixel = ((plane0_byte >> bit) & 1) | (((plane1_byte >> bit) & 1) << 1);
```

이것도 올바릅니다.

#### 3. VRAM 덤프 해석
VRAM 덤프가 word 단위로 표시되어 있지만, 실제로는 byte 단위로 저장되어 있을 것입니다.

예상되는 실제 구조:
- 0x8520: 0xFC (Plane 0, line 0)
- 0x8521: 0x00 (Plane 1, line 0)
- 0x8522: 0x66 (Plane 0, line 1)
- 0x8523: 0x00 (Plane 1, line 1)
- ...

## 글씨가 올바르게 나오는 조건

### 1. 타일맵 엔트리
- **타일맵 주소**: word $0021 (byte $0042)부터
- **타일 번호**: ASCII 코드를 직접 사용 (R=82, u=117, n=110, ...)
- **팔레트**: 0
- **Flip**: 없음
- **Priority**: 0

### 2. 타일 데이터
- **Base 주소**: byte $8000 (word $4000)
- **타일 크기**: 16 bytes (2bpp)
- **Plane 0**: bytes 0-7
- **Plane 1**: bytes 8-15

### 3. 색상 팔레트
- **팔레트 0**:
  - 인덱스 0: 검정 (0x0000)
  - 인덱스 1: 흰색 (0xFFFF)
  - 인덱스 2: 회색 (0x1084)
  - 인덱스 3: 회색 (0x18C6)

### 4. 렌더링 파이프라인
1. **타일맵 읽기**: word $0021부터 각 문자에 대한 타일 번호 읽기
2. **타일 데이터 읽기**: byte $8000 + (타일 번호 * 16)
3. **픽셀 디코딩**: 2bpp 디코딩 (Plane 0 + Plane 1)
4. **색상 조회**: 팔레트 0에서 색상 인덱스로 색상 가져오기
5. **화면 출력**: SDL2로 렌더링

## 확인해야 할 사항

1. **VRAM 덤프 해석**: word 단위로 표시되지만 실제로는 byte 단위로 저장되어 있는지 확인
2. **타일 데이터 디코딩**: Plane 0과 Plane 1이 올바르게 읽히는지 확인
3. **비트 순서**: MSB first (bit 7부터)가 올바른지 확인
4. **색상 변환**: CGRAM 색상이 올바르게 변환되는지 확인
5. **스크롤**: BG1 스크롤이 올바르게 적용되는지 확인

## 다음 단계

1. 실제 VRAM 데이터를 byte 단위로 확인
2. 타일 82 (R)의 픽셀 패턴을 수동으로 디코딩
3. 예상되는 픽셀 패턴과 실제 렌더링 결과 비교
4. 문제가 있는 부분을 수정

