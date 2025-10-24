#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>
#include "cpu/cpu.h"
#include "memory/memory.h"
#include "ppu/ppu.h"
#include "apu/apu.h"
#include "debug/logger.h"

int main() {
    std::cout << "SNES Emulator - Console Mode" << std::endl;
    
    // Load ROM
    std::string romPath = "SNES Test Program.sfc";
    std::ifstream romFile(romPath, std::ios::binary);
    if (!romFile) {
        std::cout << "Error: Could not open ROM file: " << romPath << std::endl;
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
    
    // Set CPU pointer for cycle tracking
    ppu.setCPU(&cpu);
    apu.setCPU(&cpu);
    
    // Initialize memory with ROM
    memory.loadROM(romData);
    
    // Initialize logger
    Logger& logger = Logger::getInstance();
    
    std::cout << "Starting emulation..." << std::endl;
    
    // Simple emulation loop
    int frameCount = 0;
    const int maxFrames = 1000; // Limit for testing
    
    while (frameCount < maxFrames) {
        // Run CPU for one frame
        for (int i = 0; i < 262144; i++) { // ~60fps at 21.477MHz
            cpu.step();
            
            // Update other components
            ppu.step();
            apu.step();
        }
        
        frameCount++;
        
        // Render frame (even without SDL)
        ppu.renderFrame();
        ppu.clearFrameReady();
        
        // Print progress every 100 frames
        if (frameCount % 100 == 0) {
            std::cout << "Frame: " << frameCount << std::endl;
        }
    }
    
    std::cout << "Emulation completed. Check log files for details." << std::endl;
    return 0;
}