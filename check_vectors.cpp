#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>

int main() {
    std::ifstream file("Super Mario World (Europe) (Rev 1).sfc", std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Failed to open ROM" << std::endl;
        return -1;
    }
    
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    file.seekg(0, std::ios::beg);
    
    std::vector<uint8_t> rom(size);
    file.read(reinterpret_cast<char*>(rom.data()), size);
    file.close();
    
    std::cout << "ROM Size: " << size << " bytes" << std::endl << std::endl;
    
    // Check interrupt vectors at end of ROM
    // For LoROM, vectors are at ROM offset 0x7FEA-0x7FFF
    size_t vectorBase = 0x7FEA;
    
    if (vectorBase + 0x15 < rom.size()) {
        std::cout << "=== Interrupt Vectors (Native Mode) ===" << std::endl;
        uint16_t nmiNative = rom[vectorBase] | (rom[vectorBase + 1] << 8);
        std::cout << "NMI (0xFFEA): 0x" << std::hex << std::setw(4) << std::setfill('0') << nmiNative << std::endl;
        
        uint16_t resetNative = rom[vectorBase + 0x0C] | (rom[vectorBase + 0x0D] << 8);
        std::cout << "RESET (0xFFFC): 0x" << std::hex << std::setw(4) << std::setfill('0') << resetNative << std::endl;
        
        std::cout << std::endl << "=== Interrupt Vectors (Emulation Mode) ===" << std::endl;
        uint16_t nmiEmu = rom[vectorBase + 0x10] | (rom[vectorBase + 0x11] << 8);
        std::cout << "NMI (0xFFFA): 0x" << std::hex << std::setw(4) << std::setfill('0') << nmiEmu << std::endl;
        
        uint16_t resetEmu = rom[vectorBase + 0x12] | (rom[vectorBase + 0x13] << 8);
        std::cout << "RESET (0xFFFC): 0x" << std::hex << std::setw(4) << std::setfill('0') << resetEmu << std::endl;
        
        uint16_t irqEmu = rom[vectorBase + 0x14] | (rom[vectorBase + 0x15] << 8);
        std::cout << "IRQ/BRK (0xFFFE): 0x" << std::hex << std::setw(4) << std::setfill('0') << irqEmu << std::endl;
    }
    
    return 0;
}


