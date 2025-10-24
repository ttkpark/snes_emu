#include "simple_input.h"
#include <iostream>

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
    // For now, no input processing
    // In a real implementation, this would poll SDL events
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