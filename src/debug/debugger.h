#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <SDL2/SDL.h>

class CPU;
class Memory;

class Debugger {
public:
    Debugger(CPU* cpu, Memory* memory);
    ~Debugger();
    
    // Debug mode control
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }
    
    // Rendering
    void render(SDL_Renderer* renderer);
    
    // CPU status display
    void showCPUState();
    void showRegisters();
    void showFlags();
    void showDisassembly();
    
    // Memory display
    void showMemory(uint32_t start, uint32_t end);
    void showMemoryMap();
    
    // Breakpoints
    void addBreakpoint(uint16_t address);
    void removeBreakpoint(uint16_t address);
    void clearBreakpoints();
    bool isBreakpoint(uint16_t address) const;
    
    // Execution control
    void step();
    void stepOver();
    void stepOut();
    void continueExecution();
    void pauseExecution();
    
    // Disassembly
    std::string disassemble(uint16_t address) const;
    std::vector<std::string> disassembleRange(uint16_t start, uint16_t end) const;
    
    // Breakpoints check
    void checkBreakpoints();
    
    // Execution control
    void executeUntilReturn();
    
private:
    CPU* m_cpu;
    Memory* m_memory;
    
    bool m_enabled;
    bool m_paused;
    
    // Debug window position
    int m_windowX;
    int m_windowY;
    int m_windowWidth;
    int m_windowHeight;
    
    // Information to display
    bool m_showCPUState;
    bool m_showRegisters;
    bool m_showFlags;
    bool m_showDisassembly;
    bool m_showMemory;
    bool m_showMemoryMap;
    
    // Breakpoints
    std::vector<uint16_t> m_breakpoints;
    
    // Disassembly cache
    std::map<uint16_t, std::string> m_disassemblyCache;
    
    
    // UI rendering
    void renderCPUState(SDL_Renderer* renderer, int x, int y);
    void renderRegisters(SDL_Renderer* renderer, int x, int y);
    void renderFlags(SDL_Renderer* renderer, int x, int y);
    void renderDisassembly(SDL_Renderer* renderer, int x, int y);
    void renderMemory(SDL_Renderer* renderer, int x, int y);
    void renderMemoryMap(SDL_Renderer* renderer, int x, int y);
    
    // Text rendering
    void renderText(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color);
    void renderText(SDL_Renderer* renderer, const std::string& text, int x, int y);
    
    // Utilities
    std::string formatHex(uint32_t value, int width = 8) const;
    std::string formatHex(uint16_t value, int width = 4) const;
    std::string formatHex(uint8_t value, int width = 2) const;
    std::string formatBinary(uint8_t value) const;
    
    // Disassembly
    std::string disassembleInstruction(uint16_t address) const;
    std::string getOpcodeName(uint8_t opcode) const;
    std::string getAddressingMode(uint8_t opcode) const;
};
