# Memory Viewer - 메모리 검사 도구

## 📋 목차
1. [개요](#개요)
2. [Hex Dump](#hex-dump)
3. [메모리 검색](#메모리-검색)
4. [메모리 감시](#메모리-감시)
5. [구현 예제](#구현-예제)

---

## 개요

메모리 뷰어는 에뮬레이터의 메모리 상태를 실시간으로 검사하는 디버깅 도구입니다.

---

## Hex Dump

### 기본 형식

```
Address  | +0 +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F | ASCII
---------|-----------------------------------------------|----------------
00:8000  | C2 30 A9 00 8F 00 21 7E A2 00 FF 9E 00 00 CA 10 | .0...... ........
00:8010  | FA E2 20 A9 81 8D 00 42 A9 0F 8D 00 21 58 6B 00 | .. ...B. ..!Xk.
```

### C++ 구현

```cpp
void hexDump(uint32_t addr, int lines = 16) {
    printf("Address  | +0 +1 +2 +3 +4 +5 +6 +7 +8 +9 +A +B +C +D +E +F | ASCII\n");
    printf("---------|-----------------------------------------------|----------------\n");
    
    for (int i = 0; i < lines; i++) {
        uint32_t lineAddr = addr + (i * 16);
        printf("%02X:%04X  | ", (lineAddr >> 16) & 0xFF, lineAddr & 0xFFFF);
        
        // Hex
        for (int j = 0; j < 16; j++) {
            uint8_t byte = memory.read(lineAddr + j);
            printf("%02X ", byte);
        }
        
        printf("| ");
        
        // ASCII
        for (int j = 0; j < 16; j++) {
            uint8_t byte = memory.read(lineAddr + j);
            printf("%c", (byte >= 32 && byte < 127) ? byte : '.');
        }
        
        printf("\n");
    }
}
```

---

## 메모리 검색

### 값 검색

```cpp
std::vector<uint32_t> searchMemory(uint8_t value) {
    std::vector<uint32_t> results;
    
    for (uint32_t addr = 0; addr < 0x1000000; addr++) {
        if (memory.read(addr) == value) {
            results.push_back(addr);
        }
    }
    
    return results;
}
```

### 패턴 검색

```cpp
std::vector<uint32_t> searchPattern(const uint8_t* pattern, int length) {
    std::vector<uint32_t> results;
    
    for (uint32_t addr = 0; addr < 0x1000000 - length; addr++) {
        bool match = true;
        for (int i = 0; i < length; i++) {
            if (memory.read(addr + i) != pattern[i]) {
                match = false;
                break;
            }
        }
        if (match) {
            results.push_back(addr);
        }
    }
    
    return results;
}
```

---

## 메모리 감시

### Watchpoint

```cpp
class MemoryWatcher {
private:
    struct Watchpoint {
        uint32_t addr;
        uint8_t lastValue;
        bool enabled;
    };
    
    std::vector<Watchpoint> watchpoints;
    
public:
    void addWatch(uint32_t addr) {
        Watchpoint wp;
        wp.addr = addr;
        wp.lastValue = memory.read(addr);
        wp.enabled = true;
        watchpoints.push_back(wp);
    }
    
    void check() {
        for (auto& wp : watchpoints) {
            if (!wp.enabled) continue;
            
            uint8_t current = memory.read(wp.addr);
            if (current != wp.lastValue) {
                printf("Memory changed: $%06X: 0x%02X → 0x%02X\n",
                       wp.addr, wp.lastValue, current);
                wp.lastValue = current;
            }
        }
    }
};
```

---

## 구현 예제

### 메모리 뷰어 GUI

```cpp
class MemoryViewerWindow {
private:
    uint32_t currentAddr = 0x008000;
    int bytesPerLine = 16;
    int visibleLines = 32;
    
public:
    void render() {
        ImGui::Begin("Memory Viewer");
        
        // 주소 입력
        ImGui::InputScalar("Address", ImGuiDataType_U32, &currentAddr,
                          nullptr, nullptr, "%06X", ImGuiInputTextFlags_CharsHexadecimal);
        
        // Hex Dump
        ImGui::BeginChild("HexDump", ImVec2(0, 0), true, ImGuiWindowFlags_HorizontalScrollbar);
        
        for (int line = 0; line < visibleLines; line++) {
            uint32_t lineAddr = currentAddr + (line * bytesPerLine);
            
            // 주소
            ImGui::Text("%02X:%04X", (lineAddr >> 16) & 0xFF, lineAddr & 0xFFFF);
            ImGui::SameLine();
            
            // Hex
            for (int col = 0; col < bytesPerLine; col++) {
                uint8_t byte = memory.read(lineAddr + col);
                ImGui::Text("%02X", byte);
                ImGui::SameLine();
            }
            
            // ASCII
            ImGui::Text("| ");
            ImGui::SameLine();
            for (int col = 0; col < bytesPerLine; col++) {
                uint8_t byte = memory.read(lineAddr + col);
                ImGui::Text("%c", (byte >= 32 && byte < 127) ? byte : '.');
                ImGui::SameLine();
            }
            
            ImGui::NewLine();
        }
        
        ImGui::EndChild();
        ImGui::End();
    }
};
```

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete










