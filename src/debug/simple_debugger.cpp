#include "simple_debugger.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>

SimpleDebugger::SimpleDebugger()
    : m_nextId(1)
    , m_stepping(false)
    , m_continue(false)
    , m_maxCycles(0)
    , m_stepCount(0)
{
}

int SimpleDebugger::addBreakpoint(BreakpointType type, uint32_t value, const std::string& desc) {
    Breakpoint bp;
    bp.id = m_nextId++;
    bp.type = type;
    bp.value = value;
    bp.enabled = true;
    bp.description = desc;
    
    m_breakpoints.push_back(bp);
    
    std::cout << "Breakpoint " << bp.id << " set";
    switch (type) {
        case BreakpointType::PC:
            std::cout << " at PC:0x" << std::hex << value << std::dec;
            break;
        case BreakpointType::CYCLE:
            std::cout << " at cycle " << value;
            break;
        case BreakpointType::PORT_WRITE:
            std::cout << " on port " << (int)value << " write";
            break;
        case BreakpointType::TEST_NUMBER:
            std::cout << " on test 0x" << std::hex << value << std::dec;
            break;
    }
    if (!desc.empty()) {
        std::cout << " (" << desc << ")";
    }
    std::cout << std::endl;
    
    return bp.id;
}

void SimpleDebugger::removeBreakpoint(int id) {
    auto it = std::find_if(m_breakpoints.begin(), m_breakpoints.end(),
                          [id](const Breakpoint& bp) { return bp.id == id; });
    if (it != m_breakpoints.end()) {
        std::cout << "Breakpoint " << id << " removed" << std::endl;
        m_breakpoints.erase(it);
    }
}

void SimpleDebugger::enableBreakpoint(int id, bool enable) {
    auto it = std::find_if(m_breakpoints.begin(), m_breakpoints.end(),
                          [id](const Breakpoint& bp) { return bp.id == id; });
    if (it != m_breakpoints.end()) {
        it->enabled = enable;
        std::cout << "Breakpoint " << id << (enable ? " enabled" : " disabled") << std::endl;
    }
}

void SimpleDebugger::listBreakpoints() {
    if (m_breakpoints.empty()) {
        std::cout << "No breakpoints set" << std::endl;
        return;
    }
    
    std::cout << "\n=== Breakpoints ===" << std::endl;
    for (const auto& bp : m_breakpoints) {
        std::cout << bp.id << ": ";
        if (!bp.enabled) std::cout << "(disabled) ";
        
        switch (bp.type) {
            case BreakpointType::PC:
                std::cout << "PC:0x" << std::hex << bp.value << std::dec;
                break;
            case BreakpointType::CYCLE:
                std::cout << "Cycle " << bp.value;
                break;
            case BreakpointType::PORT_WRITE:
                std::cout << "Port " << (int)bp.value << " write";
                break;
            case BreakpointType::TEST_NUMBER:
                std::cout << "Test 0x" << std::hex << bp.value << std::dec;
                break;
        }
        
        if (!bp.description.empty()) {
            std::cout << " - " << bp.description;
        }
        std::cout << std::endl;
    }
}

bool SimpleDebugger::shouldBreak(uint32_t pc, uint64_t cycle) {
    // Max cycles check
    if (m_maxCycles > 0 && cycle >= m_maxCycles) {
        std::cout << "\n=== Max cycles (" << m_maxCycles << ") reached ===" << std::endl;
        return true;
    }
    
    // Step mode
    if (m_stepping) {
        return true;
    }
    
    // Breakpoint check
    for (const auto& bp : m_breakpoints) {
        if (!bp.enabled) continue;
        
        bool hit = false;
        switch (bp.type) {
            case BreakpointType::PC:
                hit = (pc == bp.value);
                break;
            case BreakpointType::CYCLE:
                hit = (cycle >= bp.value);
                break;
        }
        
        if (hit) {
            std::cout << "\n=== Breakpoint " << bp.id << " hit ";
            if (bp.type == BreakpointType::PC) {
                std::cout << "at PC:0x" << std::hex << pc << std::dec;
            } else if (bp.type == BreakpointType::CYCLE) {
                std::cout << "at cycle " << cycle;
            }
            std::cout << " ===" << std::endl;
            return true;
        }
    }
    
    return false;
}

bool SimpleDebugger::shouldBreakOnPort(uint8_t port, uint8_t value) {
    for (const auto& bp : m_breakpoints) {
        if (!bp.enabled) continue;
        if (bp.type == BreakpointType::PORT_WRITE && bp.value == port) {
            std::cout << "\n=== Breakpoint " << bp.id << " hit ";
            std::cout << "on port " << (int)port << " write (value=0x" 
                     << std::hex << (int)value << std::dec << ") ===" << std::endl;
            return true;
        }
    }
    return false;
}

bool SimpleDebugger::shouldBreakOnTest(uint8_t test_num) {
    for (const auto& bp : m_breakpoints) {
        if (!bp.enabled) continue;
        if (bp.type == BreakpointType::TEST_NUMBER && bp.value == test_num) {
            std::cout << "\n=== Breakpoint " << bp.id << " hit ";
            std::cout << "on test 0x" << std::hex << (int)test_num << std::dec << " ===" << std::endl;
            return true;
        }
    }
    return false;
}

void SimpleDebugger::printState(uint32_t pc, uint8_t a, uint8_t x, uint8_t y, 
                                 uint8_t sp, uint8_t psw, uint64_t cycle) {
    std::cout << "[Cyc:" << std::setw(10) << std::setfill('0') << cycle << "] ";
    std::cout << "PC:0x" << std::hex << std::setw(4) << std::setfill('0') << pc << " | ";
    std::cout << "A:0x" << std::setw(2) << (int)a << " ";
    std::cout << "X:0x" << std::setw(2) << (int)x << " ";
    std::cout << "Y:0x" << std::setw(2) << (int)y << " ";
    std::cout << "SP:0x" << std::setw(2) << (int)sp << " ";
    std::cout << "PSW:0x" << std::setw(2) << (int)psw << std::dec << std::endl;
}

void SimpleDebugger::showHelp() {
    std::cout << "\n=== Debugger Commands ===" << std::endl;
    std::cout << "s / step       - Execute one instruction" << std::endl;
    std::cout << "c / continue   - Continue until next breakpoint" << std::endl;
    std::cout << "n [count]      - Execute N instructions" << std::endl;
    std::cout << "b <addr>       - Set breakpoint at PC address (hex)" << std::endl;
    std::cout << "bc <cycle>     - Set breakpoint at cycle" << std::endl;
    std::cout << "bt <test>      - Set breakpoint at test number (hex)" << std::endl;
    std::cout << "bp <port>      - Set breakpoint on port write" << std::endl;
    std::cout << "d <id>         - Delete breakpoint" << std::endl;
    std::cout << "list / l       - List all breakpoints" << std::endl;
    std::cout << "r / reg        - Show registers" << std::endl;
    std::cout << "q / quit       - Quit emulator" << std::endl;
    std::cout << "h / help       - Show this help" << std::endl;
}

void SimpleDebugger::enterInteractiveMode(uint32_t pc, uint64_t cycle) {
    m_continue = false;
    
    std::cout << "\n>>> Interactive mode (type 'h' for help)" << std::endl;
    
    while (true) {
        std::cout << "(snes-dbg) ";
        std::string line;
        if (!std::getline(std::cin, line)) {
            break;
        }
        
        if (line.empty()) {
            line = "s";  // Default: step
        }
        
        processCommand(line, pc, cycle);
        
        if (m_continue || m_stepping) {
            break;
        }
    }
}

void SimpleDebugger::processCommand(const std::string& cmd, uint32_t pc, uint64_t cycle) {
    std::istringstream iss(cmd);
    std::string command;
    iss >> command;
    
    if (command == "s" || command == "step") {
        m_stepping = true;
        m_stepCount = 1;
        m_continue = true;
        
    } else if (command == "n") {
        int count = 1;
        iss >> count;
        m_stepping = true;
        m_stepCount = count;
        m_continue = true;
        std::cout << "Stepping " << count << " instructions..." << std::endl;
        
    } else if (command == "c" || command == "continue") {
        m_stepping = false;
        m_continue = true;
        std::cout << "Continuing..." << std::endl;
        
    } else if (command == "b") {
        std::string addr_str;
        iss >> addr_str;
        uint32_t addr = std::stoul(addr_str, nullptr, 16);
        addBreakpoint(BreakpointType::PC, addr);
        
    } else if (command == "bc") {
        uint64_t cycle_val;
        iss >> cycle_val;
        addBreakpoint(BreakpointType::CYCLE, cycle_val, "cycle breakpoint");
        
    } else if (command == "bt") {
        std::string test_str;
        iss >> test_str;
        uint32_t test_num = std::stoul(test_str, nullptr, 16);
        addBreakpoint(BreakpointType::TEST_NUMBER, test_num, "test breakpoint");
        
    } else if (command == "bp") {
        int port;
        iss >> port;
        addBreakpoint(BreakpointType::PORT_WRITE, port, "port breakpoint");
        
    } else if (command == "d") {
        int id;
        iss >> id;
        removeBreakpoint(id);
        
    } else if (command == "list" || command == "l") {
        listBreakpoints();
        
    } else if (command == "r" || command == "reg") {
        std::cout << "Current state:" << std::endl;
        std::cout << "  PC:0x" << std::hex << pc << std::dec << std::endl;
        std::cout << "  Cycle:" << cycle << std::endl;
        
    } else if (command == "q" || command == "quit") {
        std::cout << "Quitting..." << std::endl;
        exit(0);
        
    } else if (command == "h" || command == "help") {
        showHelp();
        
    } else {
        std::cout << "Unknown command: " << command << std::endl;
        std::cout << "Type 'h' for help" << std::endl;
    }
}










