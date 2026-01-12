#include "input.h"
#include <iostream>

Input::Input() 
    : m_gameController(nullptr)
    , m_gamepadConnected(false) {
    
    // Default key mapping setup
    setupDefaultMappings();
    
    // Gamepad initialization
    initializeGamepad();
}

Input::~Input() {
    if (m_gameController) {
        SDL_GameControllerClose(m_gameController);
    }
}

void Input::update() {
    // Save previous state
    for (auto& [button, state] : m_buttonStates) {
        state.previous = state.current;
    }
    
    // Update current state
    updateKeyboardInput();
    updateGamepadInput();
    
    // Detect state changes
    for (auto& [button, state] : m_buttonStates) {
        state.justPressed = state.current && !state.previous;
        state.justReleased = !state.current && state.previous;
    }
}

void Input::handleEvent(const SDL_Event& event) {
    handleKeyboardInput(event);
    handleGamepadInput(event);
}

bool Input::isPressed(uint8_t button) const {
    auto it = m_buttonStates.find(button);
    if (it != m_buttonStates.end()) {
        return it->second.current;
    }
    return false;
}

bool Input::isJustPressed(uint8_t button) const {
    auto it = m_buttonStates.find(button);
    if (it != m_buttonStates.end()) {
        return it->second.justPressed;
    }
    return false;
}

bool Input::isJustReleased(uint8_t button) const {
    auto it = m_buttonStates.find(button);
    if (it != m_buttonStates.end()) {
        return it->second.justReleased;
    }
    return false;
}

void Input::setKeyMapping(uint8_t button, SDL_Scancode key) {
    m_keyMappings[button] = key;
}

SDL_Scancode Input::getKeyMapping(uint8_t button) const {
    auto it = m_keyMappings.find(button);
    if (it != m_keyMappings.end()) {
        return it->second;
    }
    return SDL_SCANCODE_UNKNOWN;
}

void Input::setGamepadMapping(uint8_t button, uint8_t gamepadButton) {
    m_gamepadMappings[button] = gamepadButton;
}

bool Input::isGamepadConnected() const {
    return m_gamepadConnected;
}

void Input::reset() {
    for (auto& [button, state] : m_buttonStates) {
        state.current = false;
        state.previous = false;
        state.justPressed = false;
        state.justReleased = false;
    }
}

void Input::setupDefaultMappings() {
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

void Input::updateButtonState(uint8_t button, bool pressed) {
    auto it = m_buttonStates.find(button);
    if (it != m_buttonStates.end()) {
        it->second.current = pressed;
    } else {
        ButtonState state;
        state.current = pressed;
        state.previous = false;
        state.justPressed = false;
        state.justReleased = false;
        m_buttonStates[button] = state;
    }
}

void Input::updateKeyboardInput() {
    const Uint8* keyboardState = SDL_GetKeyboardState(nullptr);
    
    for (const auto& [button, key] : m_keyMappings) {
        bool pressed = keyboardState[key];
        updateButtonState(button, pressed);
    }
}

void Input::updateGamepadInput() {
    if (!m_gamepadConnected || !m_gameController) return;
    
    for (const auto& [button, gamepadButton] : m_gamepadMappings) {
        bool pressed = SDL_GameControllerGetButton(m_gameController, (SDL_GameControllerButton)gamepadButton);
        updateButtonState(button, pressed);
    }
}

void Input::handleKeyboardInput(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        SDL_Scancode scancode = event.key.keysym.scancode;
        bool pressed = (event.type == SDL_KEYDOWN);
        
        // Find mapped button
        for (const auto& [button, key] : m_keyMappings) {
            if (key == scancode) {
                updateButtonState(button, pressed);
                break;
            }
        }
    }
}

void Input::handleGamepadInput(const SDL_Event& event) {
    if (!m_gamepadConnected) return;
    
    if (event.type == SDL_CONTROLLERBUTTONDOWN || 
        event.type == SDL_CONTROLLERBUTTONUP) {
        SDL_ControllerButtonEvent buttonEvent = event.cbutton;
        bool pressed = (event.type == SDL_CONTROLLERBUTTONDOWN);
        
        // Find mapped button
        for (const auto& [button, gamepadButton] : m_gamepadMappings) {
            if (gamepadButton == buttonEvent.button) {
                updateButtonState(button, pressed);
                break;
            }
        }
    }
}

bool Input::initializeGamepad() {
    if (SDL_NumJoysticks() > 0) {
        m_gameController = SDL_GameControllerOpen(0);
        if (m_gameController) {
            m_gamepadConnected = true;
            std::cout << "Gamepad connected: " << SDL_GameControllerName(m_gameController) << std::endl;
            return true;
        }
    }
    return false;
}
