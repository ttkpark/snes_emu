#pragma once
#include <cstdint>
#include <SDL.h>

class SimpleInput {
public:
    SimpleInput();
    ~SimpleInput();
    
    void update();
    void handleEvent(const SDL_Event& event);
    void writeStrobe(uint8_t value);
    uint8_t readController1();
    uint8_t readController2();
    

private:
    uint16_t m_controller1_state;
    uint16_t m_controller2_state;
    bool m_strobe_active;
    uint8_t m_read_index1;
    uint8_t m_read_index2;
    
    // SNES button bit positions
    enum ButtonBit {
        BIT_B = 0,
        BIT_Y = 1,
        BIT_SELECT = 2,
        BIT_START = 3,
        BIT_UP = 4,
        BIT_DOWN = 5,
        BIT_LEFT = 6,
        BIT_RIGHT = 7,
        BIT_A = 8,
        BIT_X = 9,
        BIT_L = 10,
        BIT_R = 11
    };
    
    void updateKeyboardState();
    void setButton(ButtonBit bit, bool pressed);
};