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
#include "debug/logger.h"

struct ROMInfo {
    std::string title;
    uint32_t size;
    uint8_t romType;
    uint8_t romSize;
    uint8_t ramSize;
    uint16_t checksum;
    uint16_t checksumComplement;
};

bool loadROM(const std::string& path, std::vector<uint8_t>& romData, ROMInfo& info) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open ROM file: " << path << std::endl;
        return false;
    }
    
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    if (fileSize < 32768) {
        std::cerr << "ROM file too small: " << fileSize << " bytes" << std::endl;
        return false;
    }
    
    romData.resize(fileSize);
    file.read(reinterpret_cast<char*>(romData.data()), fileSize);
    file.close();
    
    // Parse ROM header (at 0x7FC0 for LoROM)
    uint32_t headerOffset = 0x7FC0;
    
    // Extract game title (21 bytes)
    info.title = std::string(reinterpret_cast<char*>(&romData[headerOffset]), 21);
    
    // Extract ROM info
    info.romType = romData[headerOffset + 0x15];
    info.romSize = romData[headerOffset + 0x17];
    info.ramSize = romData[headerOffset + 0x18];
    info.checksum = romData[headerOffset + 0x1E] | (romData[headerOffset + 0x1F] << 8);
    info.checksumComplement = romData[headerOffset + 0x1C] | (romData[headerOffset + 0x1D] << 8);
    info.size = fileSize;
    
    std::cout << "=== ROM Loaded ===" << std::endl;
    std::cout << "Title: " << info.title << std::endl;
    std::cout << "Size: " << info.size << " bytes (" << (info.size / 1024) << " KB)" << std::endl;
    std::cout << "ROM Type: 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)info.romType << std::endl;
    std::cout << "Checksum: 0x" << std::hex << std::setw(4) << std::setfill('0') << info.checksum << std::endl;
    std::cout << std::dec << "==================" << std::endl;
    
    return true;
}

void renderText(SDL_Renderer* renderer, const std::string& text, int x, int y, int fontSize = 2) {
    // Simple pixel font rendering (8x8 per character, scaled by fontSize)
    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    
    for (size_t i = 0; i < text.length(); i++) {
        char c = text[i];
        int charX = x + (i * 8 * fontSize);
        
        // Simple rendering - just draw a rectangle for each character for now
        SDL_Rect rect = {charX, y, 6 * fontSize, 8 * fontSize};
        SDL_RenderDrawRect(renderer, &rect);
    }
}

int main(int argc, char* argv[]) {
    std::cout << "SNES Emulator starting..." << std::endl;
    
    // Load ROM
    std::vector<uint8_t> romData;
    ROMInfo romInfo;
    std::string romPath = "SNES Test Program.sfc";
    
    if (!loadROM(romPath, romData, romInfo)) {
        std::cerr << "Failed to load ROM. Press Enter to exit..." << std::endl;
        std::cin.get();
        return -1;
    }
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_GAMECONTROLLER) < 0) {
        std::cerr << "SDL initialization failed: " << SDL_GetError() << std::endl;
        return -1;
    }
    
    SDL_Window* window = SDL_CreateWindow(
        "SNES Emulator - Test Program",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1024, 768,
        SDL_WINDOW_SHOWN
    );
    
    if (!window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return -1;
    }
    
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    if (!renderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    
    std::cout << "SDL initialized successfully!" << std::endl;
    
    // Initialize APU, PPU, Memory and CPU
    APU apu;
    PPU ppu;
    Memory memory;
    memory.setAPU(&apu);
    memory.setPPU(&ppu);
    memory.loadROM(romData);
    
    CPU cpu(&memory);
    
    // Connect CPU and PPU for NMI communication
    ppu.setCPU(&cpu);
    cpu.setPPU(&ppu);
    
    // Initialize APU audio
    if (!apu.initAudio()) {
        std::cerr << "Warning: Failed to initialize audio, continuing without sound" << std::endl;
    }
    
    cpu.reset();
    
    // Create texture for PPU output (256x224)
    SDL_Texture* ppuTexture = SDL_CreateTexture(
        renderer,
        SDL_PIXELFORMAT_RGBA8888,
        SDL_TEXTUREACCESS_STREAMING,
        256, 224
    );
    
    if (!ppuTexture) {
        std::cerr << "Failed to create PPU texture: " << SDL_GetError() << std::endl;
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    
    std::cout << "Press ESC to exit..." << std::endl;
    
    bool running = true;
    SDL_Event event;
    int frameCount = 0;
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            } else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE) {
                running = false;
            }
        }
        
        // Execute CPU instructions, PPU rendering, and APU processing
        // Run ~1500 CPU cycles per scanline, 262 scanlines per frame
        for (int scanline = 0; scanline < 262; scanline++) {
            // Execute ~5 CPU instructions per scanline (simplified)
            for (int i = 0; i < 5; i++) {
                cpu.step();
            }
            
            // Update PPU for this scanline
            ppu.step();
            
            // Update APU for this scanline (APU runs independently)
            apu.step();
        }
        
        // Print status every 300 frames (5 seconds) to reduce output
        frameCount++;
        if (frameCount % 300 == 0) {
            std::cout << "Frame " << frameCount << " - PC: 0x" << std::hex << std::setw(4) << std::setfill('0') 
                      << cpu.getPC() << " A: 0x" << std::setw(2) << (cpu.getA() & 0xFF) << std::dec << std::endl;
        }
        
        // Clear screen (dark blue background)
        SDL_SetRenderDrawColor(renderer, 20, 20, 60, 255);
        SDL_RenderClear(renderer);
        
        // Get PPU framebuffer and render to SDL
        const std::vector<uint32_t>& framebuffer = ppu.getFramebuffer();
        
        // Debug: Check if framebuffer has valid data
        static int debugCount = 0;
        if (debugCount < 5) {
            std::cout << "Main: First 3 pixels: 0x" << std::hex << framebuffer[0] 
                      << " 0x" << framebuffer[1] << " 0x" << framebuffer[2] << std::dec << std::endl;
            debugCount++;
        }
        
        SDL_UpdateTexture(ppuTexture, nullptr, framebuffer.data(), 256 * sizeof(uint32_t));
        
        // Render PPU texture (scaled to fit screen)
        SDL_Rect ppuRect = {50, 50, 768, 672}; // 256x224 scaled 3x
        SDL_RenderCopy(renderer, ppuTexture, nullptr, &ppuRect);
        
        // Draw border around PPU output
        SDL_SetRenderDrawColor(renderer, 255, 200, 0, 255);
        SDL_Rect ppuBorder = {48, 48, 772, 676};
        SDL_RenderDrawRect(renderer, &ppuBorder);
        
        SDL_RenderPresent(renderer);
    }
    
    SDL_DestroyTexture(ppuTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    
    // Flush all logs before exit
    std::cout << "Flushing logs..." << std::endl;
    Logger::getInstance().flush();
    
    std::cout << "Emulator closed successfully." << std::endl;
    std::cout << "Log files created: cpu_trace.log, apu_trace.log, ppu_trace.log" << std::endl;
    return 0;
}
