#pragma once
#include <cstdint>

class SimpleInput {
public:
    SimpleInput();
    ~SimpleInput();
    
    void update();
    void writeStrobe(uint8_t value);
    uint8_t readController1();
    uint8_t readController2();

private:
    uint16_t m_controller1_state;
    uint16_t m_controller2_state;
    bool m_strobe_active;
    uint8_t m_read_index1;
    uint8_t m_read_index2;
};