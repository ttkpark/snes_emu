#pragma once

#include <memory>
#include <string>
#include <SDL2/SDL.h>

// 전방 선언
class CPU;
class PPU;
class APU;
class Memory;
class Input;
class Debugger;

class SNESEmulator {
public:
    SNESEmulator();
    ~SNESEmulator();
    
    // 메인 실행 루프
    void run();
    void run(const std::string& romPath);
    
    // ROM 로딩
    bool loadROM(const std::string& romPath);
    
    // 에뮬레이터 제어
    void reset();
    void pause();
    void resume();
    
    // 디버그 모드
    void setDebugMode(bool enabled);
    bool isDebugMode() const { return m_debugMode; }
    
private:
    // 초기화
    bool initialize();
    void cleanup();
    
    // 메인 루프
    void mainLoop();
    void handleEvents();
    void update();
    void render();
    
    // 컴포넌트들
    std::unique_ptr<CPU> m_cpu;
    std::unique_ptr<PPU> m_ppu;
    std::unique_ptr<APU> m_apu;
    std::unique_ptr<Memory> m_memory;
    std::unique_ptr<Input> m_input;
    std::unique_ptr<Debugger> m_debugger;
    
    // SDL 관련
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    
    // 상태
    bool m_running;
    bool m_paused;
    bool m_debugMode;
    
    // 타이밍
    uint64_t m_frameCount;
    uint32_t m_lastFrameTime;
};
