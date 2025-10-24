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
    std::cout << "=== SNES Real-time Monitor ===" << std::endl;
    
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "SDL Init Failed: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    std::string romPath = "SNES Test Program.sfc";
    std::ifstream romFile(romPath, std::ios::binary);
    if (!romFile) {
        std::cout << "ROM not found: " << romPath << std::endl;
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
    
    memory.setCPU(&cpu);
    memory.setPPU(&ppu);
    memory.setAPU(&apu);
    memory.setInput(&input);
    ppu.setCPU(&cpu);
    apu.setCPU(&cpu);
    
    if (!memory.loadROM(romData)) {
        std::cerr << "ROM load failed" << std::endl;
        SDL_Quit();
        return 1;
    }
    
    if (!ppu.initVideo()) {
        std::cerr << "PPU init failed" << std::endl;
        SDL_Quit();
        return 1;
    }
    
    if (!apu.initAudio()) {
        std::cerr << "APU init failed" << std::endl;
        ppu.cleanup();
        SDL_Quit();
        return 1;
    }
    
    cpu.reset();
    
    std::cout << "\n=== Initial State ===" << std::endl;
    std::cout << "CPU PC: 0x" << std::hex << cpu.getPC() << std::dec << std::endl;
    std::cout << "Starting emulation with real-time monitoring..." << std::endl;
    
    bool running = true;
    uint64_t frameCount = 0;
    uint64_t totalInstructions = 0;
    SDL_Event event;
    
    Uint32 lastStatusTime = SDL_GetTicks();
    
    while (running && frameCount < 300) {  // Run for 5 seconds (300 frames at 60fps)
        Uint32 frameStart = SDL_GetTicks();
        
        // Handle events
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }
        }
        
        // Run one frame worth of cycles
        const int CYCLES_PER_FRAME = 89342; // 21.477 MHz / 60 Hz
        int cyclesThisFrame = 0;
        int instructionsThisFrame = 0;
        
        while (cyclesThisFrame < CYCLES_PER_FRAME && running) {
            cpu.step();
            ppu.step();
            apu.step();
            
            instructionsThisFrame++;
            totalInstructions++;
            cyclesThisFrame++;
        }
        
        // Render frame
        ppu.renderFrame();
        ppu.clearFrameReady();
        frameCount++;
        
        // Print status every second
        Uint32 now = SDL_GetTicks();
        if (now - lastStatusTime >= 1000) {
            std::cout << "\n=== STATUS (Frame " << frameCount << ") ===" << std::endl;
            std::cout << "CPU: PC=0x" << std::hex << cpu.getPC() 
                      << " PBR=0x" << (int)cpu.getPBR()
                      << " P=0x" << (int)cpu.getP()
                      << " SP=0x" << (int)cpu.getSP()
                      << std::dec << std::endl;
            std::cout << "     Cycles=" << cpu.getCycles()
                      << " Instructions=" << totalInstructions << std::endl;
            std::cout << "PPU: Scanline=" << ppu.getScanline()
                      << " FrameReady=" << ppu.isFrameReady() << std::endl;
            std::cout << "FPS: ~" << frameCount / ((now - frameStart + 1000) / 1000) << std::endl;
            lastStatusTime = now;
        }
        
        // Limit to 60 FPS
        Uint32 frameTime = SDL_GetTicks() - frameStart;
        if (frameTime < 16) {
            SDL_Delay(16 - frameTime);
        }
    }
    
    std::cout << "\n=== Final Status ===" << std::endl;
    std::cout << "Total Frames: " << frameCount << std::endl;
    std::cout << "Total Instructions: " << totalInstructions << std::endl;
    std::cout << "Avg Instructions/Frame: " << (totalInstructions / frameCount) << std::endl;
    std::cout << "Final PC: 0x" << std::hex << cpu.getPC() << std::dec << std::endl;
    
    apu.cleanup();
    ppu.cleanup();
    SDL_Quit();
    
    return 0;
}


