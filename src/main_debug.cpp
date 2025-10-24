#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "cpu/cpu.h"
#include "memory/memory.h"
#include "ppu/ppu.h"
#include "apu/apu.h"
#include "input/simple_input.h"
#include "debug/logger.h"

int main(int argc, char* argv[]) {
    std::cout << "=== SNES Emulator - Debug Mode ===" << std::endl;
    std::cout << "This version limits CPU execution to prevent freezing" << std::endl;
    std::cout << "======================================" << std::endl << std::endl;
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Load ROM
    std::string romPath = "SNES Test Program.sfc";
    std::cout << "Loading ROM: " << romPath << std::endl;
    
    std::ifstream romFile(romPath, std::ios::binary);
    if (!romFile) {
        std::cout << "Error: Could not open ROM file: " << romPath << std::endl;
        SDL_Quit();
        return 1;
    }
    
    std::vector<uint8_t> romData;
    romData.assign((std::istreambuf_iterator<char>(romFile)),
                    std::istreambuf_iterator<char>());
    romFile.close();
    
    std::cout << "ROM loaded: " << romData.size() << " bytes" << std::endl << std::endl;
    
    // Initialize components
    std::cout << "Initializing components..." << std::endl;
    Memory memory;
    CPU cpu(&memory);
    PPU ppu;
    APU apu;
    SimpleInput input;
    Logger& logger = Logger::getInstance();
    
    // Connect components
    memory.setCPU(&cpu);
    memory.setPPU(&ppu);
    memory.setAPU(&apu);
    memory.setInput(&input);
    ppu.setCPU(&cpu);
    apu.setCPU(&cpu);
    
    // Load ROM into memory
    if (!memory.loadROM(romData)) {
        std::cerr << "Failed to load ROM into memory." << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Initialize PPU video
    if (!ppu.initVideo()) {
        std::cerr << "Failed to initialize PPU video." << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Initialize APU audio
    if (!apu.initAudio()) {
        std::cerr << "Failed to initialize APU audio." << std::endl;
        ppu.cleanup();
        SDL_Quit();
        return 1;
    }
    
    // Reset CPU
    cpu.reset();
    std::cout << "Emulation started!" << std::endl;
    std::cout << "Press ESC to quit" << std::endl << std::endl;
    
    bool running = true;
    uint64_t frameCount = 0;
    SDL_Event event;
    
    //限制每帧的CPU周期数，防止无限循环
    const int MAX_CYCLES_PER_FRAME = 89342; // NTSC: ~1.79 MHz / 60 Hz
    const int CYCLES_PER_SCANLINE = 341;
    const int SCANLINES_PER_FRAME = 262;
    
    Uint32 lastTime = SDL_GetTicks();
    int fpsCounter = 0;
    
    while (running) {
        Uint32 frameStart = SDL_GetTicks();
        
        // Process SDL events FIRST (very important!)
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN) {
                if (event.key.keysym.sym == SDLK_ESCAPE) {
                    running = false;
                }
            }
            input.update();
        }
        
        // Limit CPU cycles per frame to prevent infinite loops
        uint64_t cyclesAtStart = cpu.getCycles();
        int cyclesThisFrame = 0;
        
        // Emulate one frame with cycle limit
        for (int scanline = 0; scanline < SCANLINES_PER_FRAME && running; ++scanline) {
            for (int dot = 0; dot < CYCLES_PER_SCANLINE; ++dot) {
                // Check if we've exceeded the cycle limit
                if (cyclesThisFrame >= MAX_CYCLES_PER_FRAME) {
                    std::cout << "WARNING: Frame " << frameCount 
                              << " exceeded cycle limit! PC: 0x" << std::hex << cpu.getPC() << std::dec << std::endl;
                    break;
                }
                
                cpu.step();
                ppu.step();
                apu.step();
                
                cyclesThisFrame++;
                
                // Process events every scanline to keep responsive
                if (dot == 0 && scanline % 32 == 0) {
                    while (SDL_PollEvent(&event)) {
                        if (event.type == SDL_QUIT) {
                            running = false;
                        } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                            running = false;
                        }
                    }
                }
            }
        }
        
        frameCount++;
        fpsCounter++;
        
        // Render frame
        if (ppu.isFrameReady()) {
            ppu.renderFrame();
            ppu.clearFrameReady();
        }
        
        // Print FPS every second
        Uint32 currentTime = SDL_GetTicks();
        if (currentTime - lastTime >= 1000) {
            std::cout << "FPS: " << fpsCounter 
                      << " | Frame: " << frameCount 
                      << " | Cycles: " << cpu.getCycles()
                      << " | PC: 0x" << std::hex << cpu.getPC() << std::dec
                      << std::endl;
            fpsCounter = 0;
            lastTime = currentTime;
        }
        
        // Frame rate limiting
        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < 16) { // ~60 FPS
            SDL_Delay(16 - frameTime);
        }
    }
    
    std::cout << std::endl << "Emulation stopped." << std::endl;
    std::cout << "Total frames: " << frameCount << std::endl;
    std::cout << "Total cycles: " << cpu.getCycles() << std::endl;
    
    // Cleanup
    apu.cleanup();
    ppu.cleanup();
    SDL_Quit();
    
    return 0;
}



