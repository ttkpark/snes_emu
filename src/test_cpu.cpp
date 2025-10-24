#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>
#include "cpu/cpu.h"
#include "memory/memory.h"
#include "ppu/ppu.h"
#include "apu/apu.h"

bool loadROM(const std::string& path, std::vector<uint8_t>& romData) {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open ROM file: " << path << std::endl;
        return false;
    }
    
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    romData.resize(fileSize);
    file.read(reinterpret_cast<char*>(romData.data()), fileSize);
    file.close();
    
    std::cout << "ROM loaded: " << fileSize << " bytes" << std::endl;
    return true;
}

int main() {
    std::cout << "=== SNES CPU Test ===" << std::endl;
    
    // Load ROM
    std::vector<uint8_t> romData;
    if (!loadROM("Super Mario World (Europe) (Rev 1).sfc", romData)) {
        return -1;
    }
    
    // Initialize APU, PPU, Memory and CPU
    APU apu;
    PPU ppu;
    Memory memory;
    memory.setAPU(&apu);
    memory.setPPU(&ppu);
    memory.loadROM(romData);
    
    CPU cpu(&memory);
    cpu.reset();
    
    std::cout << "\n=== Starting CPU execution ===" << std::endl;
    std::cout << "Initial PC: 0x" << std::hex << cpu.getPC() << std::dec << std::endl;
    
    // Execute first 100 instructions with detailed logging
    for (int i = 0; i < 100; i++) {
        uint16_t pc = cpu.getPC();
        uint16_t a = cpu.getA();
        
        // Read the opcode
        uint8_t opcode = memory.read8(pc);
        
        std::cout << std::setw(3) << i << ": PC=0x" << std::hex << std::setw(4) << std::setfill('0') << pc
                  << " Op=0x" << std::setw(2) << (int)opcode
                  << " A=0x" << std::setw(2) << (a & 0xFF)
                  << " X=0x" << std::setw(2) << (cpu.getX() & 0xFF)
                  << " Y=0x" << std::setw(2) << (cpu.getY() & 0xFF)
                  << std::dec << std::endl;
        
        cpu.step();
        
        // Update APU and PPU
        apu.step();
        ppu.step();
        
        // Check for infinite loop
        if (i > 10 && cpu.getPC() == pc) {
            std::cout << "\n!!! WARNING: PC not advancing! Likely infinite loop." << std::endl;
            break;
        }
    }
    
    std::cout << "\n=== Test Complete ===" << std::endl;
    std::cout << "Final PC: 0x" << std::hex << cpu.getPC() << std::dec << std::endl;
    
    return 0;
}

