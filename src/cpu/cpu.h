#pragma once
#include <cstdint>

class Memory;
class PPU;

class CPU {
public:
    CPU(Memory* memory);
    void reset();
    void step();
    
    // NMI (Non-Maskable Interrupt) support
    void triggerNMI();
    void setPPU(PPU* ppu) { m_ppu = ppu; }
    
    uint16_t getPC() const { return m_pc; }
    uint16_t getA() const { return m_a; }
    uint16_t getX() const { return m_x; }
    uint16_t getY() const { return m_y; }
    uint8_t getP() const { return m_p; }
    uint8_t getPBR() const { return m_pbr; }
    uint8_t getDBR() const { return m_dbr; }
    uint16_t getSP() const { return m_sp; }
    uint64_t getCycles() const { return m_cycles; }
    bool getEmulationMode() const { return m_emulationMode; }
    
    void setPC(uint16_t pc) { m_pc = pc; }
    
    // Logging control
    void setLogSuppression(bool enabled) { m_suppressLogging = enabled; }
    bool shouldSuppressLogging() const { return m_suppressLogging; }
    
    bool m_quitEmulation;
private:
    Memory* m_memory;
    PPU* m_ppu;
    uint16_t m_pc;      // Program Counter
    uint16_t m_a;       // Accumulator
    uint16_t m_x, m_y;  // Index registers
    uint16_t m_sp;      // Stack Pointer (16-bit for Native Mode, 8-bit for Emulation Mode)
    volatile uint8_t m_p;        // Status flags
    uint64_t m_cycles;
    
    // Interrupt flags
    bool m_nmiPending;
    bool m_nmiEnabled;
    
    // 65816 mode flags
    bool m_modeM;  // M flag: true = 8-bit A, false = 16-bit A
    bool m_modeX;  // X flag: true = 8-bit X/Y, false = 16-bit X/Y
    bool m_emulationMode;  // E flag: true = 6502 emulation, false = native 65816
    
    // 65816 specific registers
    uint16_t m_d;        // Direct Page register
    uint8_t m_dbr;       // Data Bank register
    uint8_t m_pbr;       // Program Bank register
    
    // Logging control
    bool m_suppressLogging;
    
    // Infinite loop detection
    uint32_t m_lastPC;        // Last PC address (full address: PBR + PC)
    uint32_t m_loopCount;      // Number of times same PC was executed
    static const uint32_t MAX_LOOP_COUNT = 10000;  // Threshold for infinite loop detection
    
    // Short loop detection (for loops between 2-3 instructions)
    static const uint32_t LOOP_HISTORY_SIZE = 10;  // Track last 10 PCs
    uint32_t m_pcHistory[LOOP_HISTORY_SIZE];  // History of recent PCs
    uint32_t m_historyIndex;  // Current index in history
    uint32_t m_shortLoopCount;  // Count for short loop detection
    
    void executeInstruction(uint8_t opcode);
    void handleNMI();
    void updateModeFlags();  // Update M, X flags from P register
    void dumpMemory();       // Dump memory for debugging infinite loops
    
    // Flag status test functions
    bool isCarry() const { return (m_p & 0x01) != 0; }
    bool isZero() const { return (m_p & 0x02) != 0; }
    bool isInterruptDisable() const { return (m_p & 0x04) != 0; }
    bool isDecimal() const { return (m_p & 0x08) != 0; }
    bool isBreak() const { return (m_p & 0x10) != 0; }
    bool isIndex8bit() const { return (m_p & 0x10) != 0; }
    bool isAccumulator8bit() const { return (m_p & 0x20) != 0; }
    bool isOverflow() const { return (m_p & 0x40) != 0; }
    bool isNegative() const { return (m_p & 0x80) != 0; }
    
    // Flag setting functions
    void setZero(bool value) { m_p = (m_p & ~0x02) | (value ? 0x02 : 0); }
    void setNegative(bool value) { m_p = (m_p & ~0x80) | (value ? 0x80 : 0); }
    void setCarry(bool value) { m_p = (m_p & ~0x01) | (value ? 0x01 : 0); }
    void setOverflow(bool value) { m_p = (m_p & ~0x40) | (value ? 0x40 : 0); }
    void setInterruptDisable(bool value) { m_p = (m_p & ~0x04) | (value ? 0x04 : 0); }
    void setDecimal(bool value) { m_p = (m_p & ~0x08) | (value ? 0x08 : 0); }
    void setBreak(bool value) { m_p = (m_p & ~0x10) | (value ? 0x10 : 0); }
    void setAccumulator8bit(bool value) { m_p = (m_p & ~0x20) | (value ? 0x20 : 0); }
    void setIndex8bit(bool value) { m_p = (m_p & ~0x10) | (value ? 0x10 : 0); }
    
    // Combined flag setting for common operations
    void setZeroNegative(uint16_t value) {
        setZero(value == 0);
        setNegative(value & 0x8000);
    }
    void setZeroNegative8(uint8_t value) {
        setZero(value == 0);
        setNegative(value & 0x80);
    }
    void pushStack(uint8_t value);
    uint8_t pullStack();
    void pushStack16(uint16_t value);
    uint16_t pullStack16();
    void stackTrace();
    
    // Helper functions for 16-bit operations
    uint16_t read16bit(uint16_t address);
    void write16bit(uint16_t address, uint16_t value);
    
    // Basic instructions
    void LDA_Immediate();  // 0xA9
    void STA_Absolute();   // 0x8D
    void JMP_Absolute();   // 0x4C
    void NOP();            // 0xEA
};
