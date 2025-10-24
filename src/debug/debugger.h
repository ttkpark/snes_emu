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
    
    // 디버그 모드 제어
    void setEnabled(bool enabled);
    bool isEnabled() const { return m_enabled; }
    
    // 렌더링
    void render(SDL_Renderer* renderer);
    
    // CPU 상태 표시
    void showCPUState();
    void showRegisters();
    void showFlags();
    void showDisassembly();
    
    // 메모리 표시
    void showMemory(uint32_t start, uint32_t end);
    void showMemoryMap();
    
    // 브레이크포인트
    void addBreakpoint(uint16_t address);
    void removeBreakpoint(uint16_t address);
    void clearBreakpoints();
    bool isBreakpoint(uint16_t address) const;
    
    // 실행 제어
    void step();
    void stepOver();
    void stepOut();
    void continueExecution();
    void pauseExecution();
    
    // 디스어셈블리
    std::string disassemble(uint16_t address) const;
    std::vector<std::string> disassembleRange(uint16_t start, uint16_t end) const;
    
    // 브레이크포인트 체크
    void checkBreakpoints();
    
    // 실행 제어
    void executeUntilReturn();
    
private:
    CPU* m_cpu;
    Memory* m_memory;
    
    bool m_enabled;
    bool m_paused;
    
    // 디버그 창 위치
    int m_windowX;
    int m_windowY;
    int m_windowWidth;
    int m_windowHeight;
    
    // 표시할 정보
    bool m_showCPUState;
    bool m_showRegisters;
    bool m_showFlags;
    bool m_showDisassembly;
    bool m_showMemory;
    bool m_showMemoryMap;
    
    // 브레이크포인트
    std::vector<uint16_t> m_breakpoints;
    
    // 디스어셈블리 캐시
    std::map<uint16_t, std::string> m_disassemblyCache;
    
    
    // UI 렌더링
    void renderCPUState(SDL_Renderer* renderer, int x, int y);
    void renderRegisters(SDL_Renderer* renderer, int x, int y);
    void renderFlags(SDL_Renderer* renderer, int x, int y);
    void renderDisassembly(SDL_Renderer* renderer, int x, int y);
    void renderMemory(SDL_Renderer* renderer, int x, int y);
    void renderMemoryMap(SDL_Renderer* renderer, int x, int y);
    
    // 텍스트 렌더링
    void renderText(SDL_Renderer* renderer, const std::string& text, int x, int y, SDL_Color color);
    void renderText(SDL_Renderer* renderer, const std::string& text, int x, int y);
    
    // 유틸리티
    std::string formatHex(uint32_t value, int width = 8) const;
    std::string formatHex(uint16_t value, int width = 4) const;
    std::string formatHex(uint8_t value, int width = 2) const;
    std::string formatBinary(uint8_t value) const;
    
    // 디스어셈블리
    std::string disassembleInstruction(uint16_t address) const;
    std::string getOpcodeName(uint8_t opcode) const;
    std::string getAddressingMode(uint8_t opcode) const;
};
