# Tile Viewer - VRAM 타일 시각화

## 📋 목차
1. [개요](#개요)
2. [타일 렌더링](#타일-렌더링)
3. [팔레트 뷰어](#팔레트-뷰어)
4. [구현 예제](#구현-예제)

---

## 개요

타일 뷰어는 VRAM에 저장된 모든 타일을 시각화하여 그래픽 문제를 디버깅합니다.

---

## 타일 렌더링

### 전체 타일 표시

```cpp
class TileViewer {
private:
    int tilesPerRow = 16;
    int bpp = 4;  // 2, 4, 8
    
public:
    void renderAllTiles() {
        int maxTiles = (bpp == 2) ? 512 : (bpp == 4) ? 512 : 256;
        
        for (int i = 0; i < maxTiles; i++) {
            int tx = (i % tilesPerRow) * 8;
            int ty = (i / tilesPerRow) * 8;
            
            renderTile(i, tx, ty);
        }
    }
    
    void renderTile(int tileNum, int x, int y) {
        uint8_t pixels[8][8];
        
        // VRAM에서 타일 디코딩
        if (bpp == 2) {
            decode2bpp(tileNum, pixels);
        } else if (bpp == 4) {
            decode4bpp(tileNum, pixels);
        } else {
            decode8bpp(tileNum, pixels);
        }
        
        // 화면에 그리기
        for (int py = 0; py < 8; py++) {
            for (int px = 0; px < 8; px++) {
                uint8_t colorIndex = pixels[py][px];
                uint16_t rgb555 = cgram[colorIndex];
                drawPixel(x + px, y + py, rgb555);
            }
        }
    }
};
```

---

## 팔레트 뷰어

### CGRAM 표시

```cpp
void renderPalette() {
    for (int p = 0; p < 16; p++) {
        for (int c = 0; c < 16; c++) {
            int x = c * 16;
            int y = p * 16;
            
            uint16_t rgb555 = cgram[p * 16 + c];
            uint32_t rgb888 = convertRGB555toRGB888(rgb555);
            
            // 16×16 사각형 그리기
            fillRect(x, y, 16, 16, rgb888);
        }
    }
}

uint32_t convertRGB555toRGB888(uint16_t rgb555) {
    uint8_t r = (rgb555 & 0x1F) << 3;
    uint8_t g = ((rgb555 >> 5) & 0x1F) << 3;
    uint8_t b = ((rgb555 >> 10) & 0x1F) << 3;
    return (r << 16) | (g << 8) | b;
}
```

---

## 구현 예제

### ImGui 타일 뷰어

```cpp
class TileViewerWindow {
private:
    int bpp = 4;
    int selectedPalette = 0;
    uint32_t* texture = nullptr;
    
public:
    void render() {
        ImGui::Begin("Tile Viewer");
        
        // BPP 선택
        ImGui::RadioButton("2bpp", &bpp, 2); ImGui::SameLine();
        ImGui::RadioButton("4bpp", &bpp, 4); ImGui::SameLine();
        ImGui::RadioButton("8bpp", &bpp, 8);
        
        // 팔레트 선택
        if (bpp != 8) {
            ImGui::SliderInt("Palette", &selectedPalette, 0, 7);
        }
        
        // 타일 렌더링
        updateTexture();
        ImGui::Image((void*)(intptr_t)textureID,
                    ImVec2(128, 256));  // 16×32 타일
        
        ImGui::End();
    }
    
    void updateTexture() {
        int maxTiles = (bpp == 2) ? 512 : (bpp == 4) ? 512 : 256;
        
        for (int i = 0; i < maxTiles; i++) {
            int tx = (i % 16) * 8;
            int ty = (i / 16) * 8;
            renderTileToTexture(i, tx, ty);
        }
    }
};
```

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete










