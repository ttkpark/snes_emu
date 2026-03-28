#include "debugger.h"
#include "../cpu/cpu.h"
#include "../memory/memory.h"
#include <iostream>
#include <iomanip>
#include <sstream>

Debugger::Debugger(CPU* cpu, Memory* memory) 
    : m_cpu(cpu)
    , m_memory(memory)
    , m_enabled(false)
    , m_paused(false)
    , m_windowX(10)
    , m_windowY(10)
    , m_windowWidth(400)
    , m_windowHeight(300)
    , m_showCPUState(true)
    , m_showRegisters(true)
    , m_showFlags(true)
    , m_showDisassembly(true)
    , m_showMemory(false)
    , m_showMemoryMap(false) {
}

Debugger::~Debugger() {
}

void Debugger::setEnabled(bool enabled) {
    m_enabled = enabled;
    if (enabled) {
        std::cout << "Debug mode enabled" << std::endl;
    } else {
        std::cout << "Debug mode disabled" << std::endl;
    }
}

void Debugger::render(SDL_Renderer* renderer) {
    if (!m_enabled) return;
    
    int y = m_windowY;
    
    if (m_showCPUState) {
        renderCPUState(renderer, m_windowX, y);
        y += 200;
    }
    
    if (m_showRegisters) {
        renderRegisters(renderer, m_windowX, y);
        y += 150;
    }
    
    if (m_showFlags) {
        renderFlags(renderer, m_windowX, y);
        y += 100;
    }
    
    if (m_showDisassembly) {
        renderDisassembly(renderer, m_windowX, y);
        y += 200;
    }
    
    if (m_showMemory) {
        renderMemory(renderer, m_windowX, y);
        y += 200;
    }
    
    if (m_showMemoryMap) {
        renderMemoryMap(renderer, m_windowX, y);
    }
}

void Debugger::showCPUState() {
    m_showCPUState = true;
}

void Debugger::showRegisters() {
    m_showRegisters = true;
}

void Debugger::showFlags() {
    m_showFlags = true;
}

void Debugger::showDisassembly() {
    m_showDisassembly = true;
}

void Debugger::showMemory(uint32_t start, uint32_t end) {
    m_showMemory = true;
    // TODO: Set memory range
}

void Debugger::showMemoryMap() {
    m_showMemoryMap = true;
}

void Debugger::addBreakpoint(uint16_t address) {
    if (std::find(m_breakpoints.begin(), m_breakpoints.end(), address) == m_breakpoints.end()) {
        m_breakpoints.push_back(address);
        std::cout << "Breakpoint added: $" << this->formatHex(address) << std::endl;
    }
}

void Debugger::removeBreakpoint(uint16_t address) {
    auto it = std::find(m_breakpoints.begin(), m_breakpoints.end(), address);
    if (it != m_breakpoints.end()) {
        m_breakpoints.erase(it);
        std::cout << "Breakpoint removed: $" << this->formatHex(address) << std::endl;
    }
}

void Debugger::clearBreakpoints() {
    m_breakpoints.clear();
    std::cout << "All breakpoints removed" << std::endl;
}

bool Debugger::isBreakpoint(uint16_t address) const {
    return std::find(m_breakpoints.begin(), m_breakpoints.end(), address) != m_breakpoints.end();
}

void Debugger::step() {
    if (m_paused) {
        m_cpu->step();
        checkBreakpoints();
    }
}

void Debugger::stepOver() {
    if (m_paused) {
        // Check if JSR instruction
        uint8_t opcode = m_memory->read8(m_cpu->getPC());
        if (opcode == 0x20) { // JSR
            // Execute until end of subroutine
            executeUntilReturn();
        } else {
            step();
        }
    }
}

void Debugger::stepOut() {
    if (m_paused) {
        // Execute until RTS instruction
        executeUntilReturn();
    }
}

void Debugger::continueExecution() {
    m_paused = false;
    std::cout << "Resume execution" << std::endl;
}

void Debugger::pauseExecution() {
    m_paused = true;
    std::cout << "Pause execution" << std::endl;
}

std::string Debugger::disassemble(uint16_t address) const {
    return m_cpu->getDisassembly(address);
}

std::vector<std::string> Debugger::disassembleRange(uint16_t start, uint16_t end) const {
    std::vector<std::string> result;
    for (uint16_t address = start; address <= end; address++) {
        result.push_back(disassemble(address));
    }
    return result;
}

void Debugger::renderCPUState(SDL_Renderer* renderer, int x, int y) {
    std::string text = "CPU Status:";
    this->renderText(renderer, text, x, y);
    y = y + 20;
    
    text = "PC: $" + this->formatHex(m_cpu->getPC());
    this->renderText(renderer, text, x, y);
    y = y + 20;
    
    text = "A:  $" + this->formatHex(m_cpu->getA());
    this->renderText(renderer, text, x, y);
    y = y + 20;
    
    text = "X:  $" + this->formatHex(m_cpu->getX());
    this->renderText(renderer, text, x, y);
    y = y + 20;
    
    text = "Y:  $" + this->formatHex(m_cpu->getY());
    this->renderText(renderer, text, x, y);
    y = y + 20;
    
    text = "SP: $" + this->formatHex(m_cpu->getSP());
    this->renderText(renderer, text, x, y);
    y = y + 20;
    
    text = "P:  $" + this->formatHex(m_cpu->getP());
    this->renderText(renderer, text, x, y);
}

void Debugger::renderRegisters(SDL_Renderer* renderer, int x, int y) {
    std::string text = "Registers:";
    this->renderText(renderer, text, x, y);
    y += 20;
    
    text = "PC: $" + this->formatHex(m_cpu->getPC());
    this->renderText(renderer, text, x, y);
    y += 20;
    
    text = "A:  $" + this->formatHex(m_cpu->getA());
    this->renderText(renderer, text, x, y);
    y += 20;
    
    text = "X:  $" + this->formatHex(m_cpu->getX());
    this->renderText(renderer, text, x, y);
    y += 20;
    
    text = "Y:  $" + this->formatHex(m_cpu->getY());
    this->renderText(renderer, text, x, y);
    y += 20;
    
    text = "SP: $" + this->formatHex(m_cpu->getSP());
    this->renderText(renderer, text, x, y);
    y += 20;
    
    text = "P:  $" + this->formatHex(m_cpu->getP());
    this->renderText(renderer, text, x, y);
}

void Debugger::renderFlags(SDL_Renderer* renderer, int x, int y) {
    // TODO: Implement renderFlags
}

void Debugger::renderDisassembly(SDL_Renderer* renderer, int x, int y) {
    std::string text = "Disassembly:";
    this->renderText(renderer, text, x, y);
    y += 20;
    
    uint16_t startAddress = m_cpu->getPC() - 10;
    for (int i = 0; i < 20; i++) {
        uint16_t address = startAddress + i;
        std::string disasm = this->disassemble(address);
        
        // Highlight current PC
        if (address == m_cpu->getPC()) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 0, 255);
            SDL_Rect rect = {x - 5, y - 2, 400, 16};
            SDL_RenderFillRect(renderer, &rect);
        }
        
        this->renderText(renderer, disasm, x, y);
        y += 16;
    }
}

void Debugger::renderMemory(SDL_Renderer* renderer, int x, int y) {
    std::string text = "Memory Dump:";
    this->renderText(renderer, text, x, y);
    y += 20;
    
    uint32_t startAddress = 0x000000;
    for (int i = 0; i < 16; i++) {
        std::string line = this->formatHex(startAddress + i * 16) + ": ";
        
        for (int j = 0; j < 16; j++) {
            uint8_t value = m_memory->read8(startAddress + i * 16 + j);
            line += this->formatHex(value) + " ";
        }
        
        this->renderText(renderer, line, x, y);
        y += 16;
    }
}

void Debugger::renderMemoryMap(SDL_Renderer* renderer, int x, int y) {
    std::string text = "Memory Map:";
    this->renderText(renderer, text, x, y);
    y += 20;
    
    // TODO: Display memory map information
    text = "Work RAM: 0x000000-0x01FFFF";
    this->renderText(renderer, text, x, y);
    y += 16;
    
    text = "Save RAM: 0x700000-0x70FFFF";
    this->renderText(renderer, text, x, y);
    y += 16;
    
    text = "ROM: 0x800000-0xFFFFFF";
    this->renderText(renderer, text, x, y);
}

void Debugger::renderText(SDL_Renderer* renderer, const std::string& text, int x, int y) {
    SDL_Color color = {255, 255, 255, 255};
    renderText(renderer, text, x, y, color);
}

void Debugger::renderText(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color) {
    // Basic text rendering (using SDL2 default functionality)
    // TODO: Implement better text rendering
    std::cout << "Debug: " << text << std::endl;
}

std::string Debugger::formatHex(uint16_t value, int width) const {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(width) << value;
    return ss.str();
}

std::string Debugger::formatHex(uint8_t value, int width) const {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(width) << (int)value;
    return ss.str();
}

std::string Debugger::formatHex(uint32_t value, int width) const {
    std::stringstream ss;
    ss << std::hex << std::uppercase << std::setfill('0') << std::setw(width) << value;
    return ss.str();
}

void Debugger::checkBreakpoints() {
    if (isBreakpoint(m_cpu->getPC())) {
        m_paused = true;
        std::cout << "Stopped at breakpoint: $" << this->formatHex(m_cpu->getPC()) << std::endl;
    }
}

void Debugger::executeUntilReturn() {
    // Execute until RTS instruction
    while (!m_paused) {
        uint8_t opcode = m_memory->read8(m_cpu->getPC());
        if (opcode == 0x60) { // RTS
            step();
            break;
        }
        step();
    }
}
