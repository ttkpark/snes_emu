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
    std::cout << "=== SNES Emulator - Super Mario World ===" << std::endl;
    std::cout << "Version: 1.0.0" << std::endl;
    std::cout << "==========================================" << std::endl << std::endl;
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Load Super Mario World ROM
    std::string romPath = "Super Mario World (Europe) (Rev 1).sfc";
    std::cout << "Loading ROM: " << romPath << std::endl;
    
    std::ifstream romFile(romPath, std::ios::binary);
    if (!romFile) {
        std::cout << "Error: Could not open ROM file: " << romPath << std::endl;
        std::cout << "Please ensure the ROM file is in the same directory as the executable." << std::endl;
        SDL_Quit();
        return 1;
    }
    
    std::vector<uint8_t> romData;
    romData.assign((std::istreambuf_iterator<char>(romFile)),
                    std::istreambuf_iterator<char>());
    romFile.close();
    
    std::cout << "ROM loaded successfully: " << romData.size() << " bytes" << std::endl;
    std::cout << "ROM size: " << (romData.size() / 1024) << " KB" << std::endl << std::endl;
    
    // Initialize components
    std::cout << "Initializing SNES components..." << std::endl;
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
    std::cout << "ROM loaded into memory." << std::endl;
    
    // Initialize PPU video
    std::cout << "Initializing video system..." << std::endl;
    if (!ppu.initVideo()) {
        std::cerr << "Failed to initialize PPU video." << std::endl;
        SDL_Quit();
        return 1;
    }
    std::cout << "Video system initialized (256x224 resolution)." << std::endl;
    
    // Initialize APU audio
    std::cout << "Initializing audio system..." << std::endl;
    if (!apu.initAudio()) {
        std::cerr << "Failed to initialize APU audio." << std::endl;
        ppu.cleanup();
        SDL_Quit();
        return 1;
    }
    std::cout << "Audio system initialized." << std::endl << std::endl;
    
    // Reset CPU
    cpu.reset();
    std::cout << "CPU reset complete." << std::endl;
    std::cout << "Starting emulation..." << std::endl << std::endl;
    
    std::cout << "=== CONTROLS ===" << std::endl;
    std::cout << "D-Pad: Arrow Keys" << std::endl;
    std::cout << "A Button: Z" << std::endl;
    std::cout << "B Button: X" << std::endl;
    std::cout << "Start: Enter" << std::endl;
    std::cout << "Select: Right Shift" << std::endl;
    std::cout << "================" << std::endl << std::endl;
    
    bool running = true;
    uint64_t frameCount = 0;
    SDL_Event event;
    
    Uint32 lastFrameTime = SDL_GetTicks();
    const Uint32 targetFrameTime = 16; // ~60 FPS (16.67ms per frame)
    
    while (running) {
        Uint32 frameStart = SDL_GetTicks();
        
        // Handle SDL events
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
        
        // Emulate one frame (262 scanlines for NTSC)
        for (int scanline = 0; scanline < 262; ++scanline) {
            for (int dot = 0; dot < 341; ++dot) { // 341 dots per scanline
                cpu.step();
                ppu.step();
                apu.step();
            }
        }
        
        frameCount++;
        
        // Render frame
        if (ppu.isFrameReady()) {
            ppu.renderFrame();
            ppu.clearFrameReady();
        }
        
        // Print progress every 5 seconds (~300 frames)
        if (frameCount % 300 == 0) {
            uint64_t cycles = cpu.getCycles();
            std::cout << "Frame: " << frameCount 
                      << " | CPU Cycles: " << cycles 
                      << " | Time: " << (frameCount / 60) << "s" << std::endl;
        }
        
        // Frame rate limiting
        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < targetFrameTime) {
            SDL_Delay(targetFrameTime - frameTime);
        }
    }
    
    std::cout << std::endl << "=== Emulation Finished ===" << std::endl;
    std::cout << "Total Frames: " << frameCount << std::endl;
    std::cout << "Total Time: " << (frameCount / 60) << " seconds" << std::endl;
    std::cout << "Total CPU Cycles: " << cpu.getCycles() << std::endl;
    std::cout << "===========================" << std::endl;
    
    // Cleanup
    apu.cleanup();
    ppu.cleanup();
    SDL_Quit();
    
    return 0;
}

