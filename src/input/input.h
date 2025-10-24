#pragma once
#include <cstdint>
#include <map>
#include <SDL.h>

class Input {
public:
    Input();
    ~Input();
    
    // 입력 처리
    void update();
    void handleEvent(const SDL_Event& event);
    
    // 버튼 상태 확인
    bool isPressed(uint8_t button) const;
    bool isJustPressed(uint8_t button) const;
    bool isJustReleased(uint8_t button) const;
    
    // 키 매핑
    void setKeyMapping(uint8_t button, SDL_Scancode key);
    SDL_Scancode getKeyMapping(uint8_t button) const;
    
    // 게임패드 지원
    void setGamepadMapping(uint8_t button, uint8_t gamepadButton);
    bool isGamepadConnected() const;
    
    // 입력 상태 리셋
    void reset();
    
private:
    // SNES 버튼 정의
    enum SNESButton {
        BUTTON_A = 0,
        BUTTON_B = 1,
        BUTTON_X = 2,
        BUTTON_Y = 3,
        BUTTON_L = 4,
        BUTTON_R = 5,
        BUTTON_SELECT = 6,
        BUTTON_START = 7,
        BUTTON_UP = 8,
        BUTTON_DOWN = 9,
        BUTTON_LEFT = 10,
        BUTTON_RIGHT = 11
    };
    
    // 버튼 상태
    struct ButtonState {
        bool current;
        bool previous;
        bool justPressed;
        bool justReleased;
    };
    
    std::map<uint8_t, ButtonState> m_buttonStates;
    
    // 키 매핑
    std::map<uint8_t, SDL_Scancode> m_keyMappings;
    std::map<uint8_t, uint8_t> m_gamepadMappings;
    
    // 게임패드
    SDL_GameController* m_gameController;
    bool m_gamepadConnected;
    
    // 기본 키 매핑 설정
    void setupDefaultMappings();
    
    // 버튼 상태 업데이트
    void updateButtonState(uint8_t button, bool pressed);
    
    // 키보드 입력 처리
    void handleKeyboardInput(const SDL_Event& event);
    
    // 게임패드 입력 처리
    void handleGamepadInput(const SDL_Event& event);
    
    // 내부 업데이트 함수들
    void updateKeyboardInput();
    void updateGamepadInput();
    bool initializeGamepad();
};
