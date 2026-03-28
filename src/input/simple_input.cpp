#include "simple_input.h"
#include <iostream>
#include <iomanip>

SimpleInput::SimpleInput() 
    : m_controller1_state(0)
    , m_controller2_state(0)
    , m_strobe_active(false)
    , m_read_index1(0)
    , m_read_index2(0) {
}

SimpleInput::~SimpleInput() {
}

void SimpleInput::update() {
    // Update keyboard state
    updateKeyboardState();
}

void SimpleInput::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        bool pressed = (event.type == SDL_KEYDOWN);
        SDL_Scancode scancode = event.key.keysym.scancode;
        
        // Key mapping: Q=SELECT, E=START, Arrow keys=UDLR
        // Z=B, X=A, A=Y, S=X, D=L, F=R
        const char* buttonName = nullptr;
        switch (scancode) {
            case SDL_SCANCODE_UP:    setButton(BIT_UP, pressed); buttonName = "UP"; break;
            case SDL_SCANCODE_DOWN:  setButton(BIT_DOWN, pressed); buttonName = "DOWN"; break;
            case SDL_SCANCODE_LEFT:  setButton(BIT_LEFT, pressed); buttonName = "LEFT"; break;
            case SDL_SCANCODE_RIGHT: setButton(BIT_RIGHT, pressed); buttonName = "RIGHT"; break;
            case SDL_SCANCODE_Z:     setButton(BIT_B, pressed); buttonName = "B"; break;
            case SDL_SCANCODE_X:     setButton(BIT_A, pressed); buttonName = "A"; break;
            case SDL_SCANCODE_A:     setButton(BIT_Y, pressed); buttonName = "Y"; break;
            case SDL_SCANCODE_S:     setButton(BIT_X, pressed); buttonName = "X"; break;
            case SDL_SCANCODE_D:     setButton(BIT_L, pressed); buttonName = "L"; break;
            case SDL_SCANCODE_F:     setButton(BIT_R, pressed); buttonName = "R"; break;
            case SDL_SCANCODE_Q:     setButton(BIT_SELECT, pressed); buttonName = "SELECT"; break;
            case SDL_SCANCODE_E:     setButton(BIT_START, pressed); buttonName = "START"; break;
            default: break;
        }
        
        if (buttonName) {
            std::cout << "[INPUT] Button " << buttonName << " " << (pressed ? "PRESSED" : "RELEASED") << std::endl;
        }
    }
}

void SimpleInput::updateKeyboardState() {
    const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);
    
    // Update button states based on current keyboard state
    setButton(BIT_UP, keyboardState[SDL_SCANCODE_UP] != 0);
    setButton(BIT_DOWN, keyboardState[SDL_SCANCODE_DOWN] != 0);
    setButton(BIT_LEFT, keyboardState[SDL_SCANCODE_LEFT] != 0);
    setButton(BIT_RIGHT, keyboardState[SDL_SCANCODE_RIGHT] != 0);
    setButton(BIT_B, keyboardState[SDL_SCANCODE_Z] != 0);
    setButton(BIT_A, keyboardState[SDL_SCANCODE_X] != 0);
    setButton(BIT_Y, keyboardState[SDL_SCANCODE_A] != 0);
    setButton(BIT_X, keyboardState[SDL_SCANCODE_S] != 0);
    setButton(BIT_L, keyboardState[SDL_SCANCODE_D] != 0);
    setButton(BIT_R, keyboardState[SDL_SCANCODE_F] != 0);
    setButton(BIT_SELECT, keyboardState[SDL_SCANCODE_Q] != 0);
    setButton(BIT_START, keyboardState[SDL_SCANCODE_E] != 0);
}

void SimpleInput::writeStrobe(uint8_t value) {
    bool new_strobe_active = (value & 0x01) != 0;
    if (new_strobe_active && !m_strobe_active) {
        m_read_index1 = 0;
        m_read_index2 = 0;
    }
    m_strobe_active = new_strobe_active;
}

uint8_t SimpleInput::readController1() {
    if (m_strobe_active) {
        return (m_controller1_state & 0x0100) ? 1 : 0; // A button for controller 1
    } else {
        if (m_read_index1 < 16) {
            uint8_t bit = (m_controller1_state >> m_read_index1) & 0x01;
            m_read_index1++;
            return bit;
        } else {
            return 1; // Always return 1 after all bits are read
        }
    }
}

uint8_t SimpleInput::readController2() {
    if (m_strobe_active) {
        return (m_controller2_state & 0x0100) ? 1 : 0; // A button for controller 2
    } else {
        if (m_read_index2 < 16) {
            uint8_t bit = (m_controller2_state >> m_read_index2) & 0x01;
            m_read_index2++;
            return bit;
        } else {
            return 1; // Always return 1 after all bits are read
        }
    }
}

void SimpleInput::setButton(ButtonBit bit, bool pressed) {
    if (pressed) {
        m_controller1_state |= (1 << bit);
    } else {
        m_controller1_state &= ~(1 << bit);
    }
}