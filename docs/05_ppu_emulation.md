# 05. PPU (Picture Processing Unit) 에뮬레이션

## 목표
- SNES PPU 완전 구현
- 7가지 배경 모드 지원
- 스프라이트 렌더링
- 색상 팔레트 시스템

## SNES PPU 특징

### 1. 화면 해상도
- **NTSC**: 256x224 (60 FPS)
- **PAL**: 256x239 (50 FPS)
- **색상**: 15비트 RGB (32,768색)

### 2. 배경 모드
- **Mode 0**: 4개의 2비트 배경
- **Mode 1**: 3개의 2비트 배경 + 1개의 4비트 배경
- **Mode 2**: 2개의 2비트 배경 + 2개의 4비트 배경
- **Mode 3**: 2개의 2비트 배경 + 1개의 8비트 배경
- **Mode 4**: 2개의 2비트 배경 + 1개의 8비트 배경
- **Mode 5**: 2개의 4비트 배경
- **Mode 6**: 1개의 4비트 배경 + 1개의 8비트 배경
- **Mode 7**: 1개의 8비트 배경 (회전/스케일링)

### 3. 스프라이트
- **최대 개수**: 128개
- **스캔라인당**: 최대 32개
- **크기**: 8x8, 16x16, 32x32, 64x64

## 구현 계획

### 1. PPU 클래스 구조
```cpp
class PPU {
    // 레지스터들
    PPURegisters m_registers;
    
    // 메모리
    std::vector<uint8_t> m_vram;    // 64KB
    std::vector<uint8_t> m_cgram;   // 512 bytes
    std::vector<uint8_t> m_oam;     // 544 bytes
    
    // 렌더링
    std::vector<uint32_t> m_screenBuffer;
    int m_scanline;
    int m_cycle;
};
```

### 2. 배경 렌더링
```cpp
void renderBackground(int bg, int scanline) {
    // 배경 모드에 따른 렌더링
    switch (m_registers.BGMODE) {
        case 0: renderMode0(bg, scanline); break;
        case 1: renderMode1(bg, scanline); break;
        case 2: renderMode2(bg, scanline); break;
        case 3: renderMode3(bg, scanline); break;
        case 4: renderMode4(bg, scanline); break;
        case 5: renderMode5(bg, scanline); break;
        case 6: renderMode6(bg, scanline); break;
        case 7: renderMode7(bg, scanline); break;
    }
}
```

### 3. 스프라이트 렌더링
```cpp
void renderSprites(int scanline) {
    // OAM에서 스프라이트 정보 읽기
    // 우선순위 정렬
    // 픽셀별 렌더링
}
```

### 4. 색상 시스템
```cpp
uint32_t convertColor(uint16_t color) const {
    // 15비트 RGB를 32비트 ARGB로 변환
    uint8_t r = (color & 0x1F) << 3;
    uint8_t g = ((color >> 5) & 0x1F) << 3;
    uint8_t b = ((color >> 10) & 0x1F) << 3;
    return (0xFF << 24) | (r << 16) | (g << 8) | b;
}
```

## 렌더링 파이프라인

### 1. 스캔라인 처리
```cpp
void update() {
    if (m_cycle == 0) {
        // 수직 블랭킹
        if (m_scanline >= 224) {
            m_scanline = 0;
            m_frameReady = true;
        }
    } else if (m_cycle < 256) {
        // 배경 렌더링
        renderBackgrounds();
        // 스프라이트 렌더링
        renderSprites();
    }
    
    m_cycle++;
    if (m_cycle >= 341) {
        m_cycle = 0;
        m_scanline++;
    }
}
```

### 2. 픽셀 합성
```cpp
void compositePixel(int x, int y) {
    // 배경 픽셀
    uint32_t bgPixel = getBackgroundPixel(x, y);
    // 스프라이트 픽셀
    uint32_t spritePixel = getSpritePixel(x, y);
    // 우선순위에 따른 합성
    uint32_t finalPixel = compositePixels(bgPixel, spritePixel);
    // 화면 버퍼에 저장
    m_screenBuffer[y * 256 + x] = finalPixel;
}
```

## 최적화 전략

### 1. 렌더링 최적화
- **더티 렉트**: 변경된 영역만 렌더링
- **타일 캐싱**: 자주 사용되는 타일 캐싱
- **스프라이트 정렬**: 스캔라인별 스프라이트 정렬

### 2. 메모리 최적화
- **VRAM 압축**: 타일 데이터 압축
- **팔레트 캐싱**: 색상 팔레트 캐싱
- **스크린 버퍼**: 더블 버퍼링

## 디버그 기능

### 1. 시각적 디버깅
- **타일맵 뷰어**: 배경 타일맵 표시
- **스프라이트 뷰어**: 스프라이트 정보 표시
- **팔레트 뷰어**: 색상 팔레트 표시

### 2. 성능 모니터링
- **렌더링 시간**: 프레임별 렌더링 시간
- **메모리 사용량**: VRAM, CGRAM 사용량
- **스프라이트 통계**: 스캔라인별 스프라이트 수

## 테스트 계획

### 1. 렌더링 테스트
- 각 배경 모드별 렌더링 테스트
- 스프라이트 렌더링 테스트
- 색상 정확성 테스트

### 2. 성능 테스트
- 프레임레이트 테스트
- 메모리 사용량 테스트
- CPU 사용률 테스트

## 예상 소요 시간
3-4일

## 다음 단계
06_apu_emulation.md - APU (Audio Processing Unit) 에뮬레이션
