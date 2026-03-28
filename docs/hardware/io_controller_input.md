# SNES Controller Input - 컨트롤러 입력 시스템

## 📋 목차
1. [개요](#개요)
2. [컨트롤러 하드웨어](#컨트롤러-하드웨어)
3. [레지스터](#레지스터)
4. [시리얼 통신 프로토콜](#시리얼-통신-프로토콜)
5. [Auto-Joypad Read](#auto-joypad-read)
6. [수동 읽기](#수동-읽기)
7. [Multi-tap](#multi-tap)
8. [특수 컨트롤러](#특수-컨트롤러)
9. [구현 가이드](#구현-가이드)
10. [디버깅](#디버깅)

---

## 개요

SNES 컨트롤러는 **시리얼 통신**을 사용하여 버튼 상태를 전송합니다. CPU는 레지스터 $4016/$4017을 통해 컨트롤러 데이터를 읽습니다.

### 핵심 특징

| 특징 | 스펙 |
|------|------|
| **포트** | 2개 (컨트롤러 1, 컨트롤러 2) |
| **버튼 수** | 12개 (D-Pad 4 + ABXY 4 + L/R + Select/Start) |
| **통신 방식** | 시리얼 (16비트) |
| **읽기 방법** | Auto-Joypad (자동) 또는 수동 |
| **Multi-tap** | 최대 4개 컨트롤러 (Port 2) |

---

## 컨트롤러 하드웨어

### 표준 컨트롤러 (SNES Gamepad)

```
┌─────────────────────────────────┐
│         [L]         [R]          │
│                                  │
│     ┌───┐                 (Y)   │
│     │ ↑ │            (X)     (A)│
│  ┌──┼───┼──┐           (B)      │
│  │← │   │ →│                    │
│  └──┼───┼──┘                    │
│     │ ↓ │                       │
│     └───┘                        │
│                                  │
│   [Select] [Start]               │
└─────────────────────────────────┘

16비트 버튼 맵:
Bit 15: B
Bit 14: Y
Bit 13: Select
Bit 12: Start
Bit 11: Up
Bit 10: Down
Bit 9:  Left
Bit 8:  Right
Bit 7:  A
Bit 6:  X
Bit 5:  L
Bit 4:  R
Bit 3-0: (항상 0)
```

---

## 레지스터

### CPU 레지스터

**$4016 (JOYSER0)** - Joypad Serial Port 1:
```
쓰기 (Write):
  Bit 0: Latch (1=샘플링 시작, 0=읽기 모드)

읽기 (Read):
  Bit 0: Controller 1 데이터 (시리얼)
  Bit 1-7: Open Bus (사용 안 함)
```

**$4017 (JOYSER1)** - Joypad Serial Port 2:
```
읽기 (Read):
  Bit 0: Controller 2 데이터 (시리얼)
  Bit 1-4: 추가 컨트롤러 (Multi-tap)
  Bit 5-7: Open Bus
```

### Auto-Joypad 레지스터

**$4200 (NMITIMEN)** - NMI/IRQ Enable:
```
Bit 0: Auto-Joypad Read (1=자동, 0=수동)
```

**$4212 (HVBJOY)** - Status:
```
Bit 0: Auto-Joypad Busy (1=읽기 중, 0=완료)
Bit 6: H-Blank
Bit 7: V-Blank
```

**$4218-$421F** - Auto-Joypad Data (읽기 전용):
```
$4218-$4219: Controller 1 (16비트, Low/High)
$421A-$421B: Controller 2
$421C-$421D: Controller 3 (Multi-tap)
$421E-$421F: Controller 4 (Multi-tap)
```

---

## 시리얼 통신 프로토콜

### 읽기 시퀀스

```
1. Latch (샘플링)
   $4016에 1 쓰기 → 컨트롤러가 버튼 상태 저장

2. Unlatch
   $4016에 0 쓰기 → 시리얼 읽기 모드

3. 16번 읽기
   $4016 (또는 $4017) 읽기 → Bit 0이 버튼 상태
   각 읽기마다 다음 버튼으로 이동

순서:
  Read 0: B
  Read 1: Y
  Read 2: Select
  Read 3: Start
  Read 4: Up
  Read 5: Down
  Read 6: Left
  Read 7: Right
  Read 8: A
  Read 9: X
  Read 10: L
  Read 11: R
  Read 12-15: (항상 0)
```

### 타이밍

```
Latch Pulse: 최소 12 master cycles (약 5.6μs)
Read Delay: 각 읽기 사이 최소 6 master cycles
```

---

## Auto-Joypad Read

### 개념

**Auto-Joypad Read**는 VBlank 시작 시 자동으로 컨트롤러를 읽어 $4218-$421F에 저장합니다.

### 장점/단점

**장점**:
- 간단함 (레지스터 읽기만 하면 됨)
- 타이밍 정확
- NMI에서 즉시 사용 가능

**단점**:
- VBlank 중 ~134 사이클 소비 (HDMA/DMA 시간 감소)
- 특수 컨트롤러 지원 제한

### 활성화

```asm
; Auto-Joypad 활성화
LDA #$81     ; Bit7=NMI, Bit0=Auto-Joypad
STA $4200    ; NMITIMEN

; NMI 핸들러에서 읽기
NMI_Handler:
    ; Auto-Joypad 완료 대기
:wait
    LDA $4212    ; HVBJOY
    AND #$01     ; Bit 0 = Busy
    BNE :wait
    
    ; Controller 1 읽기
    LDA $4218    ; Low byte
    STA Joy1Low
    LDA $4219    ; High byte
    STA Joy1High
    
    RTI
```

### C++ 구현

```cpp
class Input {
private:
    uint16_t joypad[4];  // 4개 컨트롤러
    bool autoJoypadEnabled = false;
    
public:
    void setAutoJoypad(bool enable) {
        autoJoypadEnabled = enable;
    }
    
    // VBlank 시작 시 호출
    void autoJoypadRead() {
        if (!autoJoypadEnabled) return;
        
        // 컨트롤러에서 버튼 상태 읽기 (에뮬레이터별 구현)
        joypad[0] = readController(0);
        joypad[1] = readController(1);
        joypad[2] = readController(2);  // Multi-tap
        joypad[3] = readController(3);
        
        // 읽기 완료까지 ~134 사이클 소요
        // (실제 하드웨어 시뮬레이션)
    }
    
    uint8_t readJoypad(int controller, bool highByte) {
        if (controller < 4) {
            return highByte ? (joypad[controller] >> 8) : (joypad[controller] & 0xFF);
        }
        return 0;
    }
};
```

---

## 수동 읽기

### 언제 사용?

- VBlank 시간을 최대한 활용하고 싶을 때
- 특수 컨트롤러 지원 (Super Scope, Mouse 등)
- 프레임 중간에 입력 확인

### 구현

```asm
; 수동 컨트롤러 읽기
ReadController1:
    ; 1. Latch
    LDA #$01
    STA $4016
    LDA #$00
    STA $4016
    
    ; 2. 16비트 읽기
    LDX #16      ; 16번 반복
.loop:
    LDA $4016    ; Bit 0 = 버튼 상태
    LSR A        ; Bit 0 → Carry
    ROL Joy1Data ; Carry → Joy1Data (오른쪽부터 채움)
    DEX
    BNE .loop
    
    RTS

Joy1Data: .dw 0
```

### C++ 구현

```cpp
class SerialInput {
private:
    uint16_t shiftRegister[2];  // 컨트롤러 1, 2
    bool latched = false;
    int bitIndex = 0;
    
public:
    // $4016 쓰기
    void writeLatch(uint8_t value) {
        if (value & 0x01) {
            // Latch: 현재 버튼 상태 샘플링
            shiftRegister[0] = readController(0);
            shiftRegister[1] = readController(1);
            latched = true;
            bitIndex = 0;
        } else {
            // Unlatch: 읽기 모드
            latched = false;
        }
    }
    
    // $4016 읽기 (Controller 1)
    uint8_t readJOYSER0() {
        if (latched) {
            // Latch 중: 첫 번째 비트 반복
            return (shiftRegister[0] >> 15) & 1;
        } else {
            // 시리얼 읽기
            uint8_t bit = (shiftRegister[0] >> 15) & 1;
            shiftRegister[0] <<= 1;  // 다음 비트로
            bitIndex++;
            
            if (bitIndex >= 16) {
                shiftRegister[0] = 0x0000;  // 16비트 후: 0
            }
            
            return bit | 0x40;  // Bit 0 = 데이터, Bit 6 = 1 (일부 게임 호환)
        }
    }
    
    // $4017 읽기 (Controller 2, 동일 로직)
    uint8_t readJOYSER1() {
        // Controller 2에 대해 동일 처리
        // ...
    }
};
```

---

## Multi-tap

### 개념

**Super Multitap**는 Port 2에 연결하여 최대 4개 컨트롤러를 지원합니다.

```
SNES Console
  ├─ Port 1: Controller 1
  └─ Port 2: Multi-tap
       ├─ Controller 2
       ├─ Controller 3
       ├─ Controller 4
       └─ Controller 5 (일부 게임)
```

### 읽기 방법

**Auto-Joypad**: $421C-$421F에 Controller 3, 4 데이터

**수동 읽기**: $4017의 Bit 1-4에서 추가 컨트롤러 데이터

```asm
; Multi-tap 읽기 (수동)
ReadMultitap:
    ; Latch
    LDA #$01
    STA $4016
    LDA #$00
    STA $4016
    
    ; 16비트 × 4개 컨트롤러
    LDX #16
.loop:
    LDA $4017
    ; Bit 0: Controller 2
    ; Bit 1: Controller 3
    ; Bit 2: Controller 4
    ; Bit 3: Controller 5 (드물게 사용)
    
    ; 각 비트를 별도로 처리
    LSR A
    ROL Joy2Data
    LSR A
    ROL Joy3Data
    LSR A
    ROL Joy4Data
    
    DEX
    BNE .loop
    
    RTS
```

---

## 특수 컨트롤러

### Super Scope (Light Gun)

**특징**:
- 광선총 (CRT TV 전용)
- Trigger, Cursor, Turbo, Pause 버튼
- X/Y 위치 정보

**읽기**:
```
$4016 (수동 읽기):
  Bit 0: 시리얼 데이터
  → 버튼 + X/Y 좌표 (복잡한 프로토콜)
```

### SNES Mouse

**특징**:
- 2버튼 마우스 (L/R)
- 상대 좌표 (delta X/Y)

**읽기**:
```
32비트 데이터:
  [0-7]: 서명 (0x01)
  [8-15]: Delta Y (부호 있음)
  [16-23]: Delta X (부호 있음)
  [24-30]: 버튼
  [31]: 속도 (0=Slow, 1=Fast)
```

### Super Game Boy

**특징**:
- Game Boy를 SNES에서 실행
- 패킷 통신 프로토콜

---

## 구현 가이드

### 전체 입력 시스템

```cpp
class SNESInput {
private:
    enum ButtonBit {
        B = 15, Y = 14, Select = 13, Start = 12,
        Up = 11, Down = 10, Left = 9, Right = 8,
        A = 7, X = 6, L = 5, R = 4
    };
    
    uint16_t currentState[4];   // 현재 프레임
    uint16_t previousState[4];  // 이전 프레임
    
    bool autoJoypad = false;
    SerialInput serialInput;
    
public:
    // SDL2에서 키보드/게임패드 → SNES 버튼 매핑
    void updateInput() {
        previousState[0] = currentState[0];
        
        currentState[0] = 0;
        
        // SDL2 키보드 상태 읽기
        const uint8_t* keys = SDL_GetKeyboardState(nullptr);
        
        if (keys[SDL_SCANCODE_UP])    currentState[0] |= (1 << Up);
        if (keys[SDL_SCANCODE_DOWN])  currentState[0] |= (1 << Down);
        if (keys[SDL_SCANCODE_LEFT])  currentState[0] |= (1 << Left);
        if (keys[SDL_SCANCODE_RIGHT]) currentState[0] |= (1 << Right);
        if (keys[SDL_SCANCODE_Z])     currentState[0] |= (1 << B);
        if (keys[SDL_SCANCODE_X])     currentState[0] |= (1 << A);
        if (keys[SDL_SCANCODE_A])     currentState[0] |= (1 << Y);
        if (keys[SDL_SCANCODE_S])     currentState[0] |= (1 << X);
        if (keys[SDL_SCANCODE_Q])     currentState[0] |= (1 << L);
        if (keys[SDL_SCANCODE_W])     currentState[0] |= (1 << R);
        if (keys[SDL_SCANCODE_RETURN]) currentState[0] |= (1 << Start);
        if (keys[SDL_SCANCODE_RSHIFT]) currentState[0] |= (1 << Select);
    }
    
    // 버튼 상태 확인
    bool isPressed(int controller, ButtonBit button) {
        return (currentState[controller] & (1 << button)) != 0;
    }
    
    bool isJustPressed(int controller, ButtonBit button) {
        bool current = (currentState[controller] & (1 << button)) != 0;
        bool previous = (previousState[controller] & (1 << button)) != 0;
        return current && !previous;
    }
    
    bool isJustReleased(int controller, ButtonBit button) {
        bool current = (currentState[controller] & (1 << button)) != 0;
        bool previous = (previousState[controller] & (1 << button)) != 0;
        return !current && previous;
    }
    
    // CPU 레지스터 읽기/쓰기
    void write4016(uint8_t value) {
        serialInput.writeLatch(value);
    }
    
    uint8_t read4016() {
        return serialInput.readJOYSER0();
    }
    
    uint8_t read4017() {
        return serialInput.readJOYSER1();
    }
    
    // Auto-Joypad
    void setAutoJoypad(bool enable) {
        autoJoypad = enable;
    }
    
    void performAutoJoypad() {
        if (autoJoypad) {
            // 컨트롤러 상태를 $4218-$421F에 저장
        }
    }
};
```

### 에뮬레이터 통합

```cpp
class SNES {
private:
    SNESInput input;
    
public:
    void runFrame() {
        // 프레임 시작 시 호스트 입력 읽기
        input.updateInput();
        
        // CPU 실행
        while (!reachedVBlank) {
            cpu.step();
        }
        
        // VBlank: Auto-Joypad
        if (inVBlank) {
            input.performAutoJoypad();
        }
    }
    
    // CPU에서 레지스터 액세스
    uint8_t cpuRead(uint16_t addr) {
        if (addr == 0x4016) return input.read4016();
        if (addr == 0x4017) return input.read4017();
        if (addr == 0x4218) return input.readAutoJoypad(0, false);
        // ...
    }
    
    void cpuWrite(uint16_t addr, uint8_t value) {
        if (addr == 0x4016) input.write4016(value);
        // ...
    }
};
```

---

## 디버깅

### 입력 모니터

```cpp
void debugInput() {
    printf("=== Controller 1 ===\n");
    
    const char* buttonNames[] = {
        "R", "L", "X", "A", "Right", "Left", "Down", "Up",
        "Start", "Select", "Y", "B"
    };
    
    for (int i = 4; i <= 15; i++) {
        if (input.isPressed(0, (ButtonBit)i)) {
            printf("%s ", buttonNames[i - 4]);
        }
    }
    printf("\n");
}
```

### 입력 로그

```cpp
void logInput() {
    static uint16_t lastState = 0;
    uint16_t current = input.getCurrentState(0);
    
    if (current != lastState) {
        printf("[Frame %d] Input changed: 0x%04X\n", frameCount, current);
        lastState = current;
    }
}
```

### 입력 재생 (TAS)

```cpp
class InputRecorder {
private:
    std::vector<uint16_t> recording;
    int playbackIndex = 0;
    
public:
    void record(uint16_t state) {
        recording.push_back(state);
    }
    
    uint16_t playback() {
        if (playbackIndex < recording.size()) {
            return recording[playbackIndex++];
        }
        return 0;  // 재생 끝
    }
    
    void saveToFile(const char* filename) {
        // 입력 데이터를 파일로 저장
    }
    
    void loadFromFile(const char* filename) {
        // 파일에서 입력 데이터 로드
    }
};
```

---

## 일반적인 문제

### 1. D-Pad 대각선 입력

```cpp
// 잘못된 예: Up + Down 동시 허용
if (keys[SDL_SCANCODE_UP]) state |= (1 << Up);
if (keys[SDL_SCANCODE_DOWN]) state |= (1 << Down);

// 올바른 예: Up/Down, Left/Right 배타적
if (keys[SDL_SCANCODE_UP] && !keys[SDL_SCANCODE_DOWN]) {
    state |= (1 << Up);
} else if (keys[SDL_SCANCODE_DOWN]) {
    state |= (1 << Down);
}
```

### 2. Auto-Joypad 타이밍

```cpp
// VBlank 시작 직후 읽으면 데이터가 준비 안 됨
// $4212 Bit 0 확인 필수
while (readRegister(0x4212) & 0x01) {
    // Auto-Joypad 완료 대기
}
```

### 3. 입력 지연

```cpp
// 프레임 시작 시 입력 읽기 (최소 지연)
void runFrame() {
    input.updateInput();  // ← 여기
    // ... 프레임 처리 ...
}
```

---

## 최적화

### 1. 입력 버퍼링

```cpp
// 빠른 입력을 놓치지 않도록
std::queue<InputEvent> inputBuffer;

void pollInput() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
            inputBuffer.push({event.type, event.key.keysym.scancode});
        }
    }
}
```

### 2. 핫키 시스템

```cpp
// 에뮬레이터 핫키 (저장, 로드, 리셋 등)
void checkHotkeys() {
    if (keys[SDL_SCANCODE_F5]) saveState();
    if (keys[SDL_SCANCODE_F9]) loadState();
    if (keys[SDL_SCANCODE_R]) reset();
}
```

---

## 참고 자료

- [SnesLab - Controller](https://sneslab.net/wiki/Controller)
- [SnesLab - Auto-Joypad](https://sneslab.net/wiki/Auto-joypad_read)
- [SNESdev - Controllers](https://snes.nesdev.org/wiki/Controllers)
- [Fullsnes - I/O Ports](https://problemkaputt.de/fullsnes.htm#snescontrollersinputdevices)

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete
