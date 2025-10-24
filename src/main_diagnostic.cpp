#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "cpu/cpu.h"
#include "memory/memory.h"
#include "ppu/ppu.h"
#include "apu/apu.h"
#include "input/simple_input.h"
#include "debug/logger.h"

int main(int argc, char* argv[]) {
    std::cout << "=== SNES Emulator Diagnostic Mode ===" << std::endl;
    std::cout << "CPU, PPU, APU status will be printed in real-time" << std::endl << std::endl;
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL Init Failed: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    std::string romPath = "SNES Test Program.sfc";
    std::ifstream romFile(romPath, std::ios::binary);
    if (!romFile) {
        std::cout << "ROM file not found: " << romPath << std::endl;
        SDL_Quit();
        return 1;
    }
    
    std::vector<uint8_t> romData;
    romData.assign((std::istreambuf_iterator<char>(romFile)),
                    std::istreambuf_iterator<char>());
    romFile.close();
    
    std::cout << "ROM loaded: " << romData.size() << " bytes" << std::endl;
    
    Memory memory;
    CPU cpu(&memory);
    PPU ppu;
    APU apu;
    SimpleInput input;
    Logger& logger = Logger::getInstance();
    logger.setLoggingEnabled(true);
    
    memory.setCPU(&cpu);
    memory.setPPU(&ppu);
    memory.setAPU(&apu);
    memory.setInput(&input);
    ppu.setCPU(&cpu);
    apu.setCPU(&cpu);
    
    if (!memory.loadROM(romData)) {
        std::cerr << "ROM memory load failed" << std::endl;
        SDL_Quit();
        return 1;
    }
    
    if (!ppu.initVideo()) {
        std::cerr << "PPU video init failed" << std::endl;
        SDL_Quit();
        return 1;
    }
    
    if (!apu.initAudio()) {
        std::cerr << "APU audio init failed" << std::endl;
        ppu.cleanup();
        SDL_Quit();
        return 1;
    }
    
    cpu.reset();
    
    std::cout << "\n=== 초기 상태 ===" << std::endl;
    std::cout << "CPU PC: 0x" << std::hex << cpu.getPC() << std::dec << std::endl;
    std::cout << "CPU PBR: 0x" << std::hex << (int)cpu.getPBR() << std::dec << std::endl;
    std::cout << "\nStarting emulation (ESC to quit)..." << std::endl;
    
    bool running = true;
    uint64_t frameCount = 0;
    SDL_Event event;
    int instructionsExecuted = 0;
    
        while (running && frameCount < 10) { // Run only 10 frames
        Uint32 frameStart = SDL_GetTicks();
        
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT || 
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                running = false;
            }
        }
        
        std::cout << "\n=== Frame " << frameCount << " ===" << std::endl;
        
        // Execute limited instructions per frame
        const int MAX_INSTRUCTIONS = 1000;
        
        for (int i = 0; i < MAX_INSTRUCTIONS && running; i++) {
            uint16_t pc_before = cpu.getPC();
            
            cpu.step();
            instructionsExecuted++;
            
            // Print status every 100 instructions
            if (i % 100 == 0) {
                std::cout << "  [" << i << "] PC: 0x" << std::hex << cpu.getPC() 
                          << " | Cycles: " << std::dec << cpu.getCycles()
                          << " | PBR: 0x" << std::hex << (int)cpu.getPBR() << std::dec
                          << std::endl;
            }
            
            // If PC doesn't change, infinite loop detected
            if (pc_before == cpu.getPC() && i > 10) {
                std::cout << "  WARNING: PC not changing! Infinite loop detected!" << std::endl;
                std::cout << "  Current PC: 0x" << std::hex << cpu.getPC() << std::dec << std::endl;
                running = false;
                break;
            }
            
            ppu.step();
            apu.step();
        }
        
        std::cout << "  PPU Scanline: " << ppu.getScanline() << std::endl;
        std::cout << "  Total Instructions: " << instructionsExecuted << std::endl;
        
        if (ppu.isFrameReady()) {
            ppu.renderFrame();
            ppu.clearFrameReady();
            std::cout << "  Frame rendered" << std::endl;
        }
        
        frameCount++;
        
        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < 16) {
            SDL_Delay(16 - frameTime);
        }
    }
    
    std::cout << "\n=== 최종 상태 ===" << std::endl;
    std::cout << "총 프레임: " << frameCount << std::endl;
    std::cout << "총 명령어: " << instructionsExecuted << std::endl;
    std::cout << "CPU Cycles: " << cpu.getCycles() << std::endl;
    std::cout << "최종 PC: 0x" << std::hex << cpu.getPC() << std::dec << std::endl;
    std::cout << "최종 PBR: 0x" << std::hex << (int)cpu.getPBR() << std::dec << std::endl;
    
    std::cout << "\nCheck log files:" << std::endl;
    std::cout << "  - cpu_trace.log" << std::endl;
    std::cout << "  - ppu_trace.log" << std::endl;
    std::cout << "  - apu_trace.log" << std::endl;
    
    apu.cleanup();
    ppu.cleanup();
    SDL_Quit();
    
    return 0;
}

