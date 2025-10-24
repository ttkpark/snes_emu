#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include <SDL.h>
#include "cpu/cpu.h"
#include "memory/memory.h"
#include "ppu/ppu.h"
#include "apu/apu.h"
#include "input/simple_input.h"
#include "debug/logger.h"

int main() {
    std::cout << "SNES Emulator - SDL2 Version" << std::endl;
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Load ROM
    std::string romPath = "SNES Test Program.sfc";
    std::ifstream romFile(romPath, std::ios::binary);
    if (!romFile) {
        std::cout << "Error: Could not open ROM file: " << romPath << std::endl;
        SDL_Quit();
        return 1;
    }
    
    // Read ROM data
    std::vector<uint8_t> romData((std::istreambuf_iterator<char>(romFile)),
                                 std::istreambuf_iterator<char>());
    romFile.close();
    
    std::cout << "ROM loaded: " << romData.size() << " bytes" << std::endl;
    
    // Initialize components
    Memory memory;
    CPU cpu(&memory);
    PPU ppu;
    APU apu;
    Input input;
    
    // Set CPU pointers for cycle tracking
    ppu.setCPU(&cpu);
    apu.setCPU(&cpu);
    memory.setInput(&input);
    
    // Initialize memory with ROM
    memory.loadROM(romData);
    
    // Initialize logger
    Logger& logger = Logger::getInstance();
    
    // Initialize SDL2 video and audio
    if (!ppu.initVideo()) {
        std::cerr << "Failed to initialize video" << std::endl;
        SDL_Quit();
        return 1;
    }
    
    if (!apu.initAudio()) {
        std::cerr << "Failed to initialize audio" << std::endl;
        SDL_Quit();
        return 1;
    }
    
    std::cout << "Starting emulation..." << std::endl;
    
    // Main emulation loop
    bool running = true;
    SDL_Event event;
    int frameCount = 0;
    
    while (running) {
        // Handle SDL events
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE) {
                        running = false;
                    }
                    break;
            }
        }
        
        // Update input
        input.update();
        
        // Emulate one frame (262 scanlines)
        for (int scanline = 0; scanline < 262; scanline++) {
            // CPU step
            cpu.step();
            
            // PPU step
            ppu.step();
            
            // APU step
            apu.step();
        }
        
        // Render frame if ready
        if (ppu.isFrameReady()) {
            ppu.renderFrame();
            ppu.clearFrameReady();
        }
        
        frameCount++;
        
        // Status update every 60 frames
        if (frameCount % 60 == 0) {
            std::cout << "Frame " << frameCount << " - PC: 0x" 
                      << std::hex << cpu.getPC() << std::dec << std::endl;
        }
        
        // Limit to 60 FPS
        SDL_Delay(16);
    }
    
    // Cleanup
    ppu.cleanup();
    SDL_Quit();
    
    std::cout << "Emulation ended after " << frameCount << " frames" << std::endl;
    return 0;
}
