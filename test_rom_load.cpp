#include <iostream>
#include <fstream>
#include <vector>
#include <iomanip>

int main() {
    std::ifstream file("cputest-basic.sfc", std::ios::binary);
    if (!file) {
        std::cout << "Failed to open ROM file!" << std::endl;
        return 1;
    }
    
    std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    
    std::cout << "ROM size: " << rom.size() << " bytes" << std::endl;
    
    std::cout << "Font data at offset 0x4D2C (first 16 bytes): ";
    for (int i = 0; i < 16; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)rom[0x4D2C + i] << " ";
    }
    std::cout << std::dec << std::endl;
    
    std::cout << "Font data at offset 0x502C (char '0', first 16 bytes): ";
    for (int i = 0; i < 16; i++) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)rom[0x502C + i] << " ";
    }
    std::cout << std::dec << std::endl;
    
    return 0;
}

