# 09. 디버그 화면 및 개발자 도구 구현

## 목표
- 실시간 디버그 인터페이스
- CPU 상태 모니터링
- 메모리 덤프 및 편집
- 브레이크포인트 시스템

## 디버그 시스템 구조

### 1. 디버거 클래스
```cpp
class Debugger {
    CPU* m_cpu;
    Memory* m_memory;
    
    bool m_enabled;
    bool m_paused;
    
    // 디버그 창 정보
    int m_windowX, m_windowY;
    int m_windowWidth, m_windowHeight;
    
    // 표시할 정보
    bool m_showCPUState;
    bool m_showRegisters;
    bool m_showFlags;
    bool m_showDisassembly;
    bool m_showMemory;
    bool m_showMemoryMap;
    
    // 브레이크포인트
    std::vector<uint16_t> m_breakpoints;
};
```

### 2. 디버그 모드 제어
```cpp
void setEnabled(bool enabled) {
    m_enabled = enabled;
    if (enabled) {
        initializeDebugInterface();
    }
}

void togglePause() {
    m_paused = !m_paused;
    if (m_paused) {
        std::cout << "디버그 모드: 일시정지" << std::endl;
    } else {
        std::cout << "디버그 모드: 재개" << std::endl;
    }
}
```

## CPU 상태 표시

### 1. 레지스터 정보
```cpp
void renderRegisters(SDL_Renderer* renderer, int x, int y) {
    std::string text = "레지스터:";
    renderText(renderer, text, x, y);
    y += 20;
    
    text = "PC: $" + formatHex(m_cpu->getPC());
    renderText(renderer, text, x, y);
    y += 20;
    
    text = "A:  $" + formatHex(m_cpu->getA());
    renderText(renderer, text, x, y);
    y += 20;
    
    text = "X:  $" + formatHex(m_cpu->getX());
    renderText(renderer, text, x, y);
    y += 20;
    
    text = "Y:  $" + formatHex(m_cpu->getY());
    renderText(renderer, text, x, y);
    y += 20;
    
    text = "SP: $" + formatHex(m_cpu->getSP());
    renderText(renderer, text, x, y);
    y += 20;
    
    text = "P:  $" + formatHex(m_cpu->getP());
    renderText(renderer, text, x, y);
}
```

### 2. 상태 플래그
```cpp
void renderFlags(SDL_Renderer* renderer, int x, int y) {
    std::string text = "상태 플래그:";
    renderText(renderer, text, x, y);
    y += 20;
    
    text = "C: " + (m_cpu->getCarry() ? "1" : "0");
    renderText(renderer, text, x, y);
    y += 20;
    
    text = "Z: " + (m_cpu->getZero() ? "1" : "0");
    renderText(renderer, text, x, y);
    y += 20;
    
    text = "I: " + (m_cpu->getInterrupt() ? "1" : "0");
    renderText(renderer, text, x, y);
    y += 20;
    
    text = "D: " + (m_cpu->getDecimal() ? "1" : "0");
    renderText(renderer, text, x, y);
    y += 20;
    
    text = "B: " + (m_cpu->getBreak() ? "1" : "0");
    renderText(renderer, text, x, y);
    y += 20;
    
    text = "V: " + (m_cpu->getOverflow() ? "1" : "0");
    renderText(renderer, text, x, y);
    y += 20;
    
    text = "N: " + (m_cpu->getNegative() ? "1" : "0");
    renderText(renderer, text, x, y);
}
```

## 디스어셈블리

### 1. 명령어 디스어셈블리
```cpp
std::string disassemble(uint16_t address) const {
    uint8_t opcode = m_memory->read8(address);
    std::string instruction = getOpcodeName(opcode);
    std::string addressingMode = getAddressingMode(opcode);
    
    std::string result = formatHex(address) + ": " + instruction;
    
    if (addressingMode == "Immediate") {
        uint8_t operand = m_memory->read8(address + 1);
        result += " #$" + formatHex(operand);
    } else if (addressingMode == "Absolute") {
        uint16_t operand = m_memory->read16(address + 1);
        result += " $" + formatHex(operand);
    } else if (addressingMode == "Zero Page") {
        uint8_t operand = m_memory->read8(address + 1);
        result += " $" + formatHex(operand);
    }
    
    return result;
}
```

### 2. 디스어셈블리 표시
```cpp
void renderDisassembly(SDL_Renderer* renderer, int x, int y) {
    std::string text = "디스어셈블리:";
    renderText(renderer, text, x, y);
    y += 20;
    
    uint16_t startAddress = m_cpu->getPC() - 10;
    for (int i = 0; i < 20; i++) {
        uint16_t address = startAddress + i;
        std::string disasm = disassemble(address);
        
        // 현재 PC 강조
        if (address == m_cpu->getPC()) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_Rect rect = {x - 5, y - 2, 400, 16};
            SDL_RenderFillRect(renderer, &rect);
        }
        
        renderText(renderer, disasm, x, y);
        y += 16;
    }
}
```

## 메모리 덤프

### 1. 메모리 덤프 표시
```cpp
void renderMemory(SDL_Renderer* renderer, int x, int y) {
    std::string text = "메모리 덤프:";
    renderText(renderer, text, x, y);
    y += 20;
    
    uint32_t startAddress = 0x000000;
    for (int i = 0; i < 16; i++) {
        std::string line = formatHex(startAddress + i * 16) + ": ";
        
        for (int j = 0; j < 16; j++) {
            uint8_t value = m_memory->read8(startAddress + i * 16 + j);
            line += formatHex(value) + " ";
        }
        
        renderText(renderer, line, x, y);
        y += 16;
    }
}
```

### 2. 메모리 맵 표시
```cpp
void renderMemoryMap(SDL_Renderer* renderer, int x, int y) {
    std::string text = "메모리 맵:";
    renderText(renderer, text, x, y);
    y += 20;
    
    for (const auto& map : m_memory->getMemoryMaps()) {
        std::string line = formatHex(map.start) + "-" + 
                          formatHex(map.end) + " " + 
                          map.name;
        renderText(renderer, line, x, y);
        y += 16;
    }
}
```

## 브레이크포인트 시스템

### 1. 브레이크포인트 관리
```cpp
void addBreakpoint(uint16_t address) {
    if (std::find(m_breakpoints.begin(), m_breakpoints.end(), address) == m_breakpoints.end()) {
        m_breakpoints.push_back(address);
        std::cout << "브레이크포인트 추가: $" << formatHex(address) << std::endl;
    }
}

void removeBreakpoint(uint16_t address) {
    auto it = std::find(m_breakpoints.begin(), m_breakpoints.end(), address);
    if (it != m_breakpoints.end()) {
        m_breakpoints.erase(it);
        std::cout << "브레이크포인트 제거: $" << formatHex(address) << std::endl;
    }
}

bool isBreakpoint(uint16_t address) const {
    return std::find(m_breakpoints.begin(), m_breakpoints.end(), address) != m_breakpoints.end();
}
```

### 2. 브레이크포인트 체크
```cpp
void checkBreakpoints() {
    if (isBreakpoint(m_cpu->getPC())) {
        m_paused = true;
        std::cout << "브레이크포인트에서 정지: $" << formatHex(m_cpu->getPC()) << std::endl;
    }
}
```

## 실행 제어

### 1. 단계별 실행
```cpp
void step() {
    if (m_paused) {
        m_cpu->step();
        checkBreakpoints();
    }
}

void stepOver() {
    if (m_paused) {
        // JSR 명령어인지 확인
        uint8_t opcode = m_memory->read8(m_cpu->getPC());
        if (opcode == 0x20) { // JSR
            // 서브루틴 끝까지 실행
            executeUntilReturn();
        } else {
            step();
        }
    }
}

void stepOut() {
    if (m_paused) {
        // RTS 명령어까지 실행
        executeUntilReturn();
    }
}
```

### 2. 실행 재개
```cpp
void continueExecution() {
    m_paused = false;
    std::cout << "실행 재개" << std::endl;
}

void pauseExecution() {
    m_paused = true;
    std::cout << "실행 일시정지" << std::endl;
}
```

## 텍스트 렌더링

### 1. 기본 텍스트 렌더링
```cpp
void renderText(SDL_Renderer* renderer, const std::string& text, int x, int y) {
    SDL_Color color = {255, 255, 255, 255};
    renderText(renderer, text, x, y, color);
}

void renderText(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color) {
    // SDL_ttf를 사용한 텍스트 렌더링
    // 또는 비트맵 폰트를 사용한 텍스트 렌더링
}
```

### 2. 유틸리티 함수
```cpp
std::string formatHex(uint16_t value, int width = 4) const {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(width) << value;
    return ss.str();
}

std::string formatHex(uint8_t value, int width = 2) const {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(width) << (int)value;
    return ss.str();
}
```

## 테스트 계획

### 1. 디버그 기능 테스트
- CPU 상태 표시 테스트
- 메모리 덤프 테스트
- 브레이크포인트 테스트

### 2. 성능 테스트
- 디버그 모드 성능 테스트
- 메모리 사용량 테스트
- 렌더링 성능 테스트

## 예상 소요 시간
2일

## 다음 단계
10_testing_integration.md - Super Mario World 테스트 및 최적화
