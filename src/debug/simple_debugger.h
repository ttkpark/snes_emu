#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <set>
#include <functional>

// Simple interactive debugger
class SimpleDebugger {
public:
    enum class BreakpointType {
        PC,              // PC address
        CYCLE,           // Cycle count
        PORT_WRITE,      // Port write
        TEST_NUMBER,     // Test number
        CONDITION        // User-defined condition
    };
    
    struct Breakpoint {
        int id;
        BreakpointType type;
        uint32_t value;         // PC, cycle, port number, test number
        bool enabled;
        std::string description;
    };
    
    SimpleDebugger();
    
    // Breakpoint management
    int addBreakpoint(BreakpointType type, uint32_t value, const std::string& desc = "");
    void removeBreakpoint(int id);
    void enableBreakpoint(int id, bool enable);
    void listBreakpoints();
    
    // Check
    bool shouldBreak(uint32_t pc, uint64_t cycle);
    bool shouldBreakOnPort(uint8_t port, uint8_t value);
    bool shouldBreakOnTest(uint8_t test_num);
    
    // Interactive mode
    void enterInteractiveMode(uint32_t pc, uint64_t cycle);
    bool isStepping() const { return m_stepping; }
    bool shouldContinue() const { return m_continue; }
    
    // Settings
    void setStepMode(bool enable) { m_stepping = enable; }
    void setMaxCycles(uint64_t max) { m_maxCycles = max; }
    
    // Information output
    void printState(uint32_t pc, uint8_t a, uint8_t x, uint8_t y, 
                    uint8_t sp, uint8_t psw, uint64_t cycle);
    
private:
    std::vector<Breakpoint> m_breakpoints;
    int m_nextId;
    bool m_stepping;
    bool m_continue;
    uint64_t m_maxCycles;
    uint64_t m_stepCount;
    
    void processCommand(const std::string& cmd, uint32_t pc, uint64_t cycle);
    void showHelp();
};










