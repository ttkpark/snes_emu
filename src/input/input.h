#pragma once
#include <cstdint>
#include <map>
#include <SDL.h>

class Input {
public:
    Input();
    ~Input();
    
    // Input handling
    void update();
    void handleEvent(const SDL_Event& event);
    
    // Button state check
    bool isPressed(uint8_t button) const;
    bool isJustPressed(uint8_t button) const;
    bool isJustReleased(uint8_t button) const;
    
    // Key mapping
    void setKeyMapping(uint8_t button, SDL_Scancode key);
    SDL_Scancode getKeyMapping(uint8_t button) const;
    
    // Gamepad support
    void setGamepadMapping(uint8_t button, uint8_t gamepadButton);
    bool isGamepadConnected() const;
    
    // Reset input state
    void reset();
    
private:
    // SNES button definitions
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
    
    // Button state
    struct ButtonState {
        bool current;
        bool previous;
        bool justPressed;
        bool justReleased;
    };
    
    std::map<uint8_t, ButtonState> m_buttonStates;
    
    // Key mapping
    std::map<uint8_t, SDL_Scancode> m_keyMappings;
    std::map<uint8_t, uint8_t> m_gamepadMappings;
    
    // Gamepad
    SDL_GameController* m_gameController;
    bool m_gamepadConnected;
    
    // Default key mapping setup
    void setupDefaultMappings();
    
    // Button state update
    void updateButtonState(uint8_t button, bool pressed);
    
    // Keyboard input handling
    void handleKeyboardInput(const SDL_Event& event);
    
    // Gamepad input handling
    void handleGamepadInput(const SDL_Event& event);
    
    // Internal update functions
    void updateKeyboardInput();
    void updateGamepadInput();
    bool initializeGamepad();
};
