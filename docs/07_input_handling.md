# 07. 입력 처리 시스템 구현

## 목표
- 키보드 및 게임패드 입력 지원
- SNES 컨트롤러 매핑
- 입력 지연 최소화
- 설정 가능한 키 매핑

## SNES 컨트롤러

### 1. 버튼 구성
- **A, B, X, Y**: 액션 버튼
- **L, R**: 숄더 버튼
- **SELECT, START**: 시스템 버튼
- **D-Pad**: 방향 패드 (상, 하, 좌, 우)

### 2. 입력 타이밍
- **폴링**: 60Hz (프레임당 1회)
- **지연**: 최대 1프레임 (16.67ms)
- **반응성**: 즉시 반응

## 구현 계획

### 1. 입력 클래스 구조
```cpp
class Input {
    // 버튼 상태
    std::map<uint8_t, ButtonState> m_buttonStates;
    
    // 키 매핑
    std::map<uint8_t, SDL_Scancode> m_keyMappings;
    std::map<uint8_t, uint8_t> m_gamepadMappings;
    
    // 게임패드
    SDL_GameController* m_gameController;
    bool m_gamepadConnected;
};
```

### 2. 버튼 상태 관리
```cpp
struct ButtonState {
    bool current;        // 현재 상태
    bool previous;       // 이전 상태
    bool justPressed;    // 방금 눌림
    bool justReleased;   // 방금 떼어짐
};
```

### 3. 입력 업데이트
```cpp
void update() {
    // 이전 상태 저장
    for (auto& [button, state] : m_buttonStates) {
        state.previous = state.current;
    }
    
    // 현재 상태 업데이트
    updateKeyboardInput();
    updateGamepadInput();
    
    // 상태 변화 감지
    for (auto& [button, state] : m_buttonStates) {
        state.justPressed = state.current && !state.previous;
        state.justReleased = !state.current && state.previous;
    }
}
```

## 키보드 입력

### 1. 기본 키 매핑
```cpp
void setupDefaultMappings() {
    m_keyMappings[BUTTON_A] = SDL_SCANCODE_Z;
    m_keyMappings[BUTTON_B] = SDL_SCANCODE_X;
    m_keyMappings[BUTTON_X] = SDL_SCANCODE_A;
    m_keyMappings[BUTTON_Y] = SDL_SCANCODE_S;
    m_keyMappings[BUTTON_L] = SDL_SCANCODE_Q;
    m_keyMappings[BUTTON_R] = SDL_SCANCODE_E;
    m_keyMappings[BUTTON_SELECT] = SDL_SCANCODE_BACKSPACE;
    m_keyMappings[BUTTON_START] = SDL_SCANCODE_RETURN;
    m_keyMappings[BUTTON_UP] = SDL_SCANCODE_UP;
    m_keyMappings[BUTTON_DOWN] = SDL_SCANCODE_DOWN;
    m_keyMappings[BUTTON_LEFT] = SDL_SCANCODE_LEFT;
    m_keyMappings[BUTTON_RIGHT] = SDL_SCANCODE_RIGHT;
}
```

### 2. 키보드 입력 처리
```cpp
void handleKeyboardInput(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        SDL_Scancode scancode = event.key.keysym.scancode;
        bool pressed = (event.type == SDL_KEYDOWN);
        
        // 매핑된 버튼 찾기
        for (const auto& [button, key] : m_keyMappings) {
            if (key == scancode) {
                updateButtonState(button, pressed);
                break;
            }
        }
    }
}
```

## 게임패드 입력

### 1. 게임패드 초기화
```cpp
bool initializeGamepad() {
    if (SDL_NumJoysticks() > 0) {
        m_gameController = SDL_GameControllerOpen(0);
        if (m_gameController) {
            m_gamepadConnected = true;
            setupGamepadMappings();
            return true;
        }
    }
    return false;
}
```

### 2. 게임패드 매핑
```cpp
void setupGamepadMappings() {
    m_gamepadMappings[BUTTON_A] = SDL_CONTROLLER_BUTTON_A;
    m_gamepadMappings[BUTTON_B] = SDL_CONTROLLER_BUTTON_B;
    m_gamepadMappings[BUTTON_X] = SDL_CONTROLLER_BUTTON_X;
    m_gamepadMappings[BUTTON_Y] = SDL_CONTROLLER_BUTTON_Y;
    m_gamepadMappings[BUTTON_L] = SDL_CONTROLLER_BUTTON_LEFTSHOULDER;
    m_gamepadMappings[BUTTON_R] = SDL_CONTROLLER_BUTTON_RIGHTSHOULDER;
    m_gamepadMappings[BUTTON_SELECT] = SDL_CONTROLLER_BUTTON_BACK;
    m_gamepadMappings[BUTTON_START] = SDL_CONTROLLER_BUTTON_START;
    m_gamepadMappings[BUTTON_UP] = SDL_CONTROLLER_BUTTON_DPAD_UP;
    m_gamepadMappings[BUTTON_DOWN] = SDL_CONTROLLER_BUTTON_DPAD_DOWN;
    m_gamepadMappings[BUTTON_LEFT] = SDL_CONTROLLER_BUTTON_DPAD_LEFT;
    m_gamepadMappings[BUTTON_RIGHT] = SDL_CONTROLLER_BUTTON_DPAD_RIGHT;
}
```

### 3. 게임패드 입력 처리
```cpp
void handleGamepadInput(const SDL_Event& event) {
    if (!m_gamepadConnected) return;
    
    if (event.type == SDL_CONTROLLERBUTTONDOWN || 
        event.type == SDL_CONTROLLERBUTTONUP) {
        SDL_ControllerButtonEvent buttonEvent = event.cbutton;
        bool pressed = (event.type == SDL_CONTROLLERBUTTONDOWN);
        
        // 매핑된 버튼 찾기
        for (const auto& [button, gamepadButton] : m_gamepadMappings) {
            if (gamepadButton == buttonEvent.button) {
                updateButtonState(button, pressed);
                break;
            }
        }
    }
}
```

## 입력 설정

### 1. 키 매핑 변경
```cpp
void setKeyMapping(uint8_t button, SDL_Scancode key) {
    m_keyMappings[button] = key;
}

void setGamepadMapping(uint8_t button, uint8_t gamepadButton) {
    m_gamepadMappings[button] = gamepadButton;
}
```

### 2. 설정 저장/로드
```cpp
void saveInputSettings(const std::string& filename) {
    std::ofstream file(filename);
    for (const auto& [button, key] : m_keyMappings) {
        file << "key_" << (int)button << "=" << (int)key << std::endl;
    }
    for (const auto& [button, gamepadButton] : m_gamepadMappings) {
        file << "gamepad_" << (int)button << "=" << (int)gamepadButton << std::endl;
    }
}

void loadInputSettings(const std::string& filename) {
    std::ifstream file(filename);
    std::string line;
    while (std::getline(file, line)) {
        // 설정 파싱 및 적용
    }
}
```

## 최적화 전략

### 1. 입력 지연 최소화
- **폴링 최적화**: 필요한 입력만 폴링
- **상태 캐싱**: 입력 상태 캐싱
- **이벤트 기반**: SDL 이벤트 기반 처리

### 2. 메모리 최적화
- **상태 압축**: 버튼 상태 비트 압축
- **매핑 최적화**: 해시 테이블 활용
- **메모리 풀링**: 입력 버퍼 풀링

## 테스트 계획

### 1. 입력 반응성 테스트
- 키보드 입력 테스트
- 게임패드 입력 테스트
- 입력 지연 측정

### 2. 매핑 테스트
- 키 매핑 변경 테스트
- 게임패드 매핑 테스트
- 설정 저장/로드 테스트

## 예상 소요 시간
1일

## 다음 단계
08_rom_loading.md - ROM 로딩 및 파싱 시스템
