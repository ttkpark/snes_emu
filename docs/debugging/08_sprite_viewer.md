# Sprite Viewer - OAM 스프라이트 시각화

## 📋 목차
1. [개요](#개요)
2. [OAM 구조](#oam-구조)
3. [스프라이트 렌더링](#스프라이트-렌더링)
4. [구현 예제](#구현-예제)

---

## 개요

스프라이트 뷰어는 OAM(Object Attribute Memory)의 모든 스프라이트를 시각화합니다.

---

## OAM 구조

### OAM 테이블

```
Low Table (512바이트):
  각 스프라이트: 4바이트
  [0]: X 위치 (0-255)
  [1]: Y 위치 (0-255)
  [2]: 타일 번호
  [3]: 속성 (우선순위, 팔레트, 뒤집기)

High Table (32바이트):
  각 바이트 = 4개 스프라이트의 상위 비트
  Bit 1-0: Size
  Bit 2: X 9번째 비트
```

---

## 스프라이트 렌더링

### 전체 스프라이트 표시

```cpp
class SpriteViewer {
public:
    void renderAllSprites() {
        for (int i = 0; i < 128; i++) {
            renderSprite(i);
        }
    }
    
    void renderSprite(int spriteNum) {
        // OAM 읽기
        int offset = spriteNum * 4;
        uint8_t x = oam[offset + 0];
        uint8_t y = oam[offset + 1];
        uint8_t tile = oam[offset + 2];
        uint8_t attr = oam[offset + 3];
        
        // High Table
        int highByte = oam[512 + (spriteNum / 4)];
        int shift = (spriteNum % 4) * 2;
        bool xHigh = (highByte >> (shift + 2)) & 1;
        int size = (highByte >> shift) & 3;
        
        // 최종 X 좌표
        int finalX = x | (xHigh ? 0x100 : 0);
        
        // 속성 파싱
        int palette = (attr >> 1) & 7;
        int priority = (attr >> 4) & 3;
        bool hFlip = attr & 0x40;
        bool vFlip = attr & 0x80;
        
        // 크기
        int width, height;
        getSpriteSize(size, width, height);
        
        // 타일 렌더링
        for (int ty = 0; ty < height / 8; ty++) {
            for (int tx = 0; tx < width / 8; tx++) {
                int tileX = finalX + tx * 8;
                int tileY = y + ty * 8;
                renderSpriteTile(tile, tileX, tileY, palette, hFlip, vFlip);
            }
        }
    }
    
    void getSpriteSize(int sizeCode, int& width, int& height) {
        // OBSEL 레지스터에 따라 다름
        // 예: Size 0 = 8×8, Size 1 = 16×16, ...
        const int sizes[][2] = {
            {8, 8}, {16, 16}, {32, 32}, {64, 64}
        };
        width = sizes[sizeCode][0];
        height = sizes[sizeCode][1];
    }
};
```

---

## 구현 예제

### ImGui 스프라이트 뷰어

```cpp
class SpriteViewerWindow {
private:
    int selectedSprite = 0;
    
public:
    void render() {
        ImGui::Begin("Sprite Viewer");
        
        // 스프라이트 선택
        ImGui::SliderInt("Sprite", &selectedSprite, 0, 127);
        
        // 스프라이트 정보
        displaySpriteInfo(selectedSprite);
        
        // 모든 스프라이트 표시
        ImGui::BeginChild("Sprites", ImVec2(0, 0), true);
        for (int i = 0; i < 128; i++) {
            if (i % 16 != 0) ImGui::SameLine();
            
            bool selected = (i == selectedSprite);
            if (ImGui::Selectable(selected ? "##selected" : "##sprite",
                                 selected, 0, ImVec2(16, 16))) {
                selectedSprite = i;
            }
            
            // 스프라이트 미리보기
            renderSpritePreview(i);
        }
        ImGui::EndChild();
        
        ImGui::End();
    }
    
    void displaySpriteInfo(int sprite) {
        int offset = sprite * 4;
        uint8_t x = oam[offset + 0];
        uint8_t y = oam[offset + 1];
        uint8_t tile = oam[offset + 2];
        uint8_t attr = oam[offset + 3];
        
        ImGui::Text("Position: (%d, %d)", x, y);
        ImGui::Text("Tile: 0x%02X", tile);
        ImGui::Text("Palette: %d", (attr >> 1) & 7);
        ImGui::Text("Priority: %d", (attr >> 4) & 3);
        ImGui::Text("H-Flip: %s", (attr & 0x40) ? "Yes" : "No");
        ImGui::Text("V-Flip: %s", (attr & 0x80) ? "Yes" : "No");
    }
};
```

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete










