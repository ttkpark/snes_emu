#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdlib.h>
#include <windows.h>
#include <iomanip>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include "cpu/cpu.h"
#include "memory/memory.h"
#include "ppu/ppu.h"
#include "apu/apu.h"
#include "input/simple_input.h"
#include "debug/logger.h"

// Function to print vectors in hex viewer format
void printVectorsHexViewer(const std::vector<uint16_t>& vectors, const std::string& title, bool isEmulationMode = false) {
    std::cout << "\n" << title << std::endl;
    std::cout << "Address: ";
    uint16_t baseAddr = isEmulationMode ? 0xFFF4 : 0xFFE4;
    for (int i = 0; i < 6; i++) {
        std::cout << std::setw(8) << std::hex << (baseAddr + i*2) << " ";
    }
    std::cout << std::endl;
    
    std::cout << "Data:    ";
    for (int i = 0; i < 6; i++) {
        std::cout << std::setw(8) << std::hex << vectors[i] << " ";
    }
    std::cout << std::dec << std::endl;
}

// Function to compare vectors
void compareVectors(const std::vector<uint16_t>& original, const std::vector<uint16_t>& current, const std::string& title) {
    std::cout << "\n" << title << std::endl;
    std::cout << "Vector Comparison:" << std::endl;
    std::cout << "Index | Original | Current  | Status" << std::endl;
    std::cout << "------|----------|----------|--------" << std::endl;
    
    for (int i = 0; i < 6; i++) {
        std::cout << "  " << i << "   | 0x" << std::setw(6) << std::hex << original[i] 
                  << " | 0x" << std::setw(6) << current[i] << " | ";
        if (original[i] == current[i]) {
            std::cout << "SAME";
        } else {
            std::cout << "DIFF";
        }
        std::cout << std::dec << std::endl;
    }
}

int main(int argc, char* argv[]) {
    std::cout << "SNES Emulator - Complete SDL2 Version" << std::endl;
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Load ROM
    std::string romPath1 = "SNES Test Program.sfc";
    std::string romPath = "Super Mario World (Europe) (Rev 1).sfc";
    std::ifstream romFile(romPath, std::ios::binary);
    if (!romFile) {
        std::cout << "Error: Could not open ROM file: " << romPath << std::endl;
        SDL_Quit();
        return 1;
    }
    romFile.seekg(0, std::ios::end);
    std::streamoff size = romFile.tellg();
    std::cout << "ROM loaded: " << size << " bytes" << std::endl;
    if ((size%1024) == 512) {
        romFile.seekg(512, std::ios::beg);
    }else
        romFile.seekg(0, std::ios::beg);

    std::vector<uint8_t> romData;
    romData.assign((std::istreambuf_iterator<char>(romFile)),
                    std::istreambuf_iterator<char>());
    romFile.close();
    

    // Initialize components
    std::cout << "Initializing components..." << std::endl;
    Memory memory;
    CPU cpu(&memory);
    PPU ppu;
    APU apu;
    SimpleInput input;
    Logger& logger = Logger::getInstance();
    
    // Connect components
    std::cout << "Connecting components..." << std::endl;
    memory.setCPU(&cpu);
    memory.setPPU(&ppu);
    memory.setAPU(&apu);
    memory.setInput(&input);
    ppu.setCPU(&cpu);
    apu.setCPU(&cpu);
    
    // Load ROM into memory
    std::cout << "Loading ROM into memory..." << std::endl;
    if (!memory.loadROM(romData)) {
        std::cerr << "Failed to load ROM into memory." << std::endl;
        SDL_Quit();
        return 1;
    }
    std::cout << "ROM loaded into memory successfully." << std::endl;
    
    std::vector<uint16_t> originalVectors(6);
    // Check ROM mapping type and interrupt vectors
    std::cout << "\n=== SNES ROM Mapping Analysis ===" << std::endl;
    if (romData.size() >= 0x8000) {
        std::cout << "ROM Size: " << romData.size() << " bytes" << std::endl;
        

        // Check ROM header for additional mapping info
        std::cout << "\n=== ROM Header Analysis ===" << std::endl;
        
        // Check for headered ROM using proper logic
        uint32_t headerOffset = 0;
        uint32_t headerPhysicalAddress = 0;
        uint32_t romSize = romData.size();
        
        // Try to find valid ROM headers at normal locations
        bool foundValidHeader = false;
        
        // Initialize mapping validation variables
        uint8_t availavleROMType = -1;
        bool loROMValid = false;
        bool hiROMValid = false;
        bool exHiROMValid = false;
        bool exLoROMValid = false;
        
        // Check LoROM header at 0x7FC0
        std::cout << "\n=== LoROM Header Analysis ===" << std::endl;
        if (romData.size() >= 0x7FE0) {
            uint32_t loROMHeaderAddr = 0x7FC0 + headerOffset;
            uint8_t romType = romData[loROMHeaderAddr + 0x15]; // $7FD5
            uint8_t romSize = romData[loROMHeaderAddr + 0x17]; // $7FD7

            std::cout << "  LoROM Header at 0x7FC0" << std::endl;
            std::cout << "  ROM Type (0x7FD5): 0x" << std::hex << (int)romType << std::dec;
            if ((romType & 0xEF) == 0x20) std::cout << " (LoROM)";
            else if ((romType & 0xE0) == 0x20) std::cout << " (Other ROM type)";
            else std::cout << " (Not LoROM)";
            std::cout << std::endl;

            if((romType & 0xE0) == 0x20) availavleROMType = (romType & 0x0F);
            
            if ((romType & 0xEF) == 0x20) {
                loROMValid = true;
                std::cout << "  -> LoROM mapping detected" << std::endl;
                headerPhysicalAddress = loROMHeaderAddr;
            }
            
            if(loROMValid){
                std::cout << "  ROM Size      : " << std::dec << (int)(1<<romSize) << "KB" << std::endl;
                std::cout << "  ROM File Size : " << std::dec << (int)(romData.size()/1024) << "KB" << std::endl;
            }
        } else {
            std::cout << "  ROM too small for LoROM header" << std::endl;
        }
        
        // Check HiROM header at 0xFFC0
        std::cout << "\n=== HiROM Header Analysis ===" << std::endl;
        if (romData.size() >= 0xFFE0) {
            uint32_t hiROMHeaderAddr = 0xFFC0 + headerOffset;
            uint8_t romType = romData[hiROMHeaderAddr + 0x15]; // $FFD5
            uint8_t romSize = romData[hiROMHeaderAddr + 0x17]; // $FFD7
            
            std::cout << "  HiROM Header at 0xFFC0" << std::endl;
            std::cout << "  ROM Type (0xFFD5): 0x" << std::hex << (int)romType << std::dec;
            if ((romType & 0xEF) == 0x21) std::cout << " (HiROM)";
            else if ((romType & 0xE0) == 0x20) std::cout << " (Other ROM type)";
            else std::cout << " (Not HiROM)";
            std::cout << std::endl;

            if((romType & 0xE0) == 0x20) availavleROMType = (romType & 0x0F);
            
            if ((romType & 0xE0) == 0x20) {
                hiROMValid = true;
                std::cout << "  -> HiROM mapping detected" << std::endl;
                headerPhysicalAddress = hiROMHeaderAddr;
            }
            
            if(hiROMValid){
                std::cout << "  ROM Size      : " << std::dec << (int)(1<<romSize) << "KB" << std::endl;
                std::cout << "  ROM File Size : " << std::dec << (int)(romData.size()/1024) << "KB" << std::endl;
            }
        } else {
            std::cout << "  ROM too small for HiROM header" << std::endl;
        }
        
        // Check ExHiROM header at 0x40FFC0
        std::cout << "\n=== ExHiROM Header Analysis ===" << std::endl;
        if (romData.size() >= 0x410000) {
            uint32_t exHiROMHeaderAddr = 0x40FFC0 + headerOffset;
            uint8_t romType = romData[exHiROMHeaderAddr + 0x15]; // $40FFD5
            uint8_t romSize = romData[exHiROMHeaderAddr + 0x17]; // $40FFD7
            
            std::cout << "  ExHiROM Header at 0x40FFC0" << std::endl;
            std::cout << "  ROM Type (0x40FFD5): 0x" << std::hex << (int)romType << std::dec;
            if ((romType & 0xEF) == 0x25) std::cout << " (ExHiROM)";
            else if ((romType & 0xE0) == 0x20) std::cout << " (Other ROM type)";
            else std::cout << " (Not ExHiROM)";
            std::cout << std::endl;

            if((romType & 0xE0) == 0x20) availavleROMType = (romType & 0x0F);
            
            if ((romType & 0xE0) == 0x20) {
                exHiROMValid = true;
                std::cout << "  -> ExHiROM mapping detected" << std::endl;
                headerPhysicalAddress = exHiROMHeaderAddr;
            }
            
            if(exHiROMValid){
                std::cout << "  ROM Size      : " << std::dec << (int)(1<<romSize) << "KB" << std::endl;
                std::cout << "  ROM File Size : " << std::dec << (int)(romData.size()/1024) << "KB" << std::endl;
            }
        } else {
            std::cout << "  ROM too small for ExHiROM header" << std::endl;
        }
        
        std::cout << "  ROM Size: " << romSize << " bytes" << std::endl;
        std::cout << "  Headered ROM: " << ((size%1024 == 512) ? "Yes" : "No") << std::endl;
        
        
        // Helper function to calculate checksum
        auto calculateChecksum = [](const std::vector<uint8_t>& rom, uint32_t checksumAddr) -> uint16_t {
            uint16_t cs = 0x0000;
            for (size_t i = 0; i < rom.size(); i++) {
                cs = (cs + rom[i]) & 0xFFFF;
            }
            return cs;
        };
        
        // Helper function to verify checksum
        auto verifyChecksum = [](const std::vector<uint8_t>& rom, uint32_t checksumAddr) -> bool {
            if (rom.size() < checksumAddr + 4) return false;
            
            uint16_t storedChecksum = (rom[checksumAddr + 3] << 8) | rom[checksumAddr + 2];
            uint16_t storedComplement = (rom[checksumAddr + 1] << 8) | rom[checksumAddr + 0];
            
            // Calculate actual checksum
            uint16_t actualChecksum = 0x0000;
            for (size_t i = 0; i < rom.size(); i++) {
                actualChecksum = (actualChecksum + rom[i]) & 0xFFFF;
            }
            
            // Verify checksum and complement
            return (storedChecksum == actualChecksum) && ((storedChecksum ^ storedComplement) == 0xFFFF);
        };
        
        // Check checksums for valid mappings
        std::cout << "\n=== Checksum Verification ===" << std::endl;
        
        // Store original validity before checksum verification
        bool originalLoROMValid = loROMValid;
        bool originalHiROMValid = hiROMValid;
        bool originalExHiROMValid = exHiROMValid;
        
        if (loROMValid) {
            std::cout << "  LoROM Checksum: ";
            if (verifyChecksum(romData, 0x7FDC)) {
                std::cout << "VALID" << std::endl;
            } else {
                std::cout << "INVALID" << std::endl;
                loROMValid = false;
            }
        }
        
        if (hiROMValid) {
            std::cout << "  HiROM Checksum: ";
            if (verifyChecksum(romData, 0xFFDC)) {
                std::cout << "VALID" << std::endl;
            } else {
                std::cout << "INVALID" << std::endl;
                hiROMValid = false;
            }
        }
        
        if (exHiROMValid) {
            std::cout << "  ExHiROM Checksum: ";
            if (verifyChecksum(romData, 0x40FFDC)) {
                std::cout << "VALID" << std::endl;
            } else {
                std::cout << "INVALID" << std::endl;
                exHiROMValid = false;
            }
        }
        
        // If all checksums are invalid, fall back to header-based detection
        if (!loROMValid && !hiROMValid && !exHiROMValid) {
            std::cout << "  All checksums invalid, using header-based detection" << std::endl;


            if(availavleROMType == 0)
                loROMValid = true;
            else if(availavleROMType == 1)
                hiROMValid = true;
            else if(availavleROMType == 5)
                exHiROMValid = true;
        }
        
        // Determine final mapping type
        std::cout << "\n=== Final Mapping Determination ===" << std::endl;
        Memory::ROMMapping detectedMapping = Memory::ROMMapping::Unknown;
        
        // Prioritize LoROM mapping
        if (loROMValid) {
            std::cout << "  -> LoROM mapping detected" << std::endl;
            detectedMapping = Memory::ROMMapping::LoROM;
            //headerPhysicalAddress = 0x7FC0;
        } else if (hiROMValid) {
            std::cout << "  -> HiROM mapping detected" << std::endl;
            detectedMapping = Memory::ROMMapping::HiROM;
            //headerPhysicalAddress = 0xFFC0;
        } else if (exHiROMValid) {
            std::cout << "  -> ExHiROM mapping detected" << std::endl;
            detectedMapping = Memory::ROMMapping::ExHiROM;
            //headerPhysicalAddress = 0x40FFC0;
        }
        
        if((loROMValid + hiROMValid + exHiROMValid) == 0){
            std::cout << "No valid mapping detected" << std::endl;
            std::cout << "  -> Using default mapping: LoROM" << std::endl;
            detectedMapping = Memory::ROMMapping::LoROM;
            loROMValid = true;
        }
        
        // Set the detected mapping in Memory class
        memory.setROMMapping(detectedMapping);
        memory.setHeaderPhysicalAddress(headerPhysicalAddress);
        
        // Check what CPU will actually read for each mode
        std::cout << "\n=== CPU Vector Reading Analysis ===" << std::endl;
        
        // Determine which mapping to use for vector analysis
        std::string mappingType = "Unknown";
        uint32_t vectorBase = headerPhysicalAddress + 0x20;
        
        if (exHiROMValid && !hiROMValid && !loROMValid) {
            mappingType = "ExHiROM";
        } else if (hiROMValid && !loROMValid && !exHiROMValid) {
            mappingType = "HiROM";
        } else if (loROMValid && !hiROMValid && !exHiROMValid) {
            mappingType = "LoROM";
        }
        
        char name[22];
        memcpy(name, romData.data() + headerPhysicalAddress, 21);
        name[21] = '\0';
        std::cout << "ROM Name: " << name << std::endl;
        
        std::cout << "Detected Mapping: " << mappingType << std::endl;
        std::cout << "Vector Base: 0x" << std::hex << vectorBase << std::dec << std::endl;
        uint8_t* vectorBasePtr = romData.data() + vectorBase;
        uint16_t* vectorBasePtr16 = (uint16_t*)vectorBasePtr;
        
        // SNES uses same vectors for both Emulation and Native modes
        std::cout << "\nSNES INTERRUPT VECTORS (native):" << std::endl;
        std::cout << "  COP  :  0xFFE4 -> ";
        std::cout << "0x" << std::hex << vectorBasePtr16[2] << std::dec << std::endl;
        
        std::cout << "  BRK  :  0xFFE6 -> ";
        std::cout << "0x" << std::hex << vectorBasePtr16[3] << std::dec << std::endl;
        
        std::cout << "(Abort):  0xFFE8 -> ";
        std::cout << "0x" << std::hex << vectorBasePtr16[4] << std::dec << std::endl;
        
        std::cout << "  NMI  :  0xFFEA -> ";
        std::cout << "0x" << std::hex << vectorBasePtr16[5] << std::dec << std::endl;
        
        std::cout << " (N/A) :  0xFFEC -> ";
        std::cout << "0x" << std::hex << vectorBasePtr16[6] << std::dec << std::endl;
        
        std::cout << "  IRQ  :  0xFFEE -> ";
        std::cout << "0x" << std::hex << vectorBasePtr16[7] << std::dec << std::endl;

        std::cout << "\nSNES INTERRUPT VECTORS (6502 emulation):" << std::endl;
        std::cout << " COP   :  0xFFF4 -> ";
        std::cout << "0x" << std::hex << vectorBasePtr16[10] << std::dec << std::endl;
        
        std::cout << " (N/A) :  0xFFF6 -> ";
        std::cout << "0x" << std::hex << vectorBasePtr16[11] << std::dec << std::endl;
        
        std::cout << "(Abort):  0xFFF8 -> ";
        std::cout << "0x" << std::hex << vectorBasePtr16[12] << std::dec << std::endl;
        
        std::cout << " NMI   :  0xFFFA -> ";
        std::cout << "0x" << std::hex << vectorBasePtr16[13] << std::dec << std::endl;
        
        std::cout << " RST   :  0xFFFC -> ";
        std::cout << "0x" << std::hex << vectorBasePtr16[14] << std::dec << std::endl;
        
        std::cout << " IRQBRK:  0xFFFE -> ";
        std::cout << "0x" << std::hex << vectorBasePtr16[15] << std::dec << std::endl;
        
        // Store original vectors for comparison
        for (int i = 0; i < 6; i++) {
            originalVectors[i] = vectorBasePtr16[2+i];
        }
        
        std::cout << "m_headerPhysicalAddress : " << std::hex << vectorBase << " = " <<  memory.getROMAddress(0xFFE0, memory.getROMMapping())<< std::dec  << std::endl;
            
        // Check what's at the vector addresses
        std::cout << "\nVector Address Contents:" << std::endl;
        
        // Determine vector base address based on CPU mode
        uint16_t vectorBaseAddr;
        if (cpu.getEmulationMode()) {
            vectorBaseAddr = 0xFFF4;  // Emulation mode vectors
            std::cout << "  Using Emulation Mode vectors (0xFFF4-0xFFFF)" << std::endl;
        } else {
            vectorBaseAddr = 0xFFE4;  // Native mode vectors
            std::cout << "  Using Native Mode vectors (0xFFE4-0xFFEF)" << std::endl;
        }
        
        for (int i = 0; i < 6; i++) {
            uint16_t vectorAddr = memory.read16(vectorBaseAddr + i*2);
            std::cout << "  Vector " << i << ": 0x" << std::hex << vectorAddr;
            
            // Check if vector points to ROM data
            if (vectorAddr >= 0x8000 && vectorAddr <= 0xFFFF) {
                uint32_t romAddr = memory.getROMAddress(vectorAddr, memory.getROMMapping());
                if (romAddr < romData.size()) {
                    uint8_t opcode = romData[romAddr];
                    std::cout << " -> ROM[0x" << romAddr << "] = 0x" << (int)opcode;
                    // Decode common opcodes
                    switch (opcode) {
                        case 0x00: std::cout << " (BRK)"; break;
                        case 0x40: std::cout << " (RTI)"; break;
                        case 0x60: std::cout << " (RTS)"; break;
                        case 0x6B: std::cout << " (RTL)"; break;
                        case 0x4C: std::cout << " (JMP abs)"; break;
                        case 0x5C: std::cout << " (JML long)"; break;
                        case 0x20: std::cout << " (JSR abs)"; break;
                        case 0x22: std::cout << " (JSL long)"; break;
                        default: std::cout << " (Unknown)"; break;
                    }
                } else {
                    std::cout << " -> OUT OF ROM BOUNDS";
                }
            } else {
                std::cout << " -> INVALID ADDRESS";
            }
            std::cout << std::dec << std::endl;
        }
        
        // Check the actual vector data bytes
        std::cout << "\nRaw Vector Data:" << std::endl;
        for (int i = 0; i < 16; i++) {
            std::cout << "  (0x" << std::hex << (0xFFE0 + i) << "): 0x" << (int)memory.read8(0xFFE0 + i) << std::dec;
            if (i % 2 == 1) std::cout << std::endl;
        }
    } else {
        std::cout << "ROM too small to contain vectors (size: " << romData.size() << " bytes)" << std::endl;
    }
    std::cout << "=== End Vector Analysis ===\n" << std::endl;
    
    // Load ROM data into PPU VRAM
    std::cout << "About to call ppu.loadROMData()..." << std::endl;
    ppu.loadROMData(romData);
    std::cout << "ppu.loadROMData() completed." << std::endl;
    
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
    
    // Initialize PPU registers to disable forced blank
    std::cout << "Initializing PPU registers..." << std::endl;
    ppu.writeRegister(0x2100, 0x0F); // INIDISP: Disable forced blank, max brightness
    ppu.writeRegister(0x2101, 0x00); // OBSEL: Object size and character base
    ppu.writeRegister(0x2102, 0x00); // OAMADDL: OAM address low
    ppu.writeRegister(0x2103, 0x00); // OAMADDH: OAM address high
    ppu.writeRegister(0x2105, 0x00); // BGMODE: Background mode and character size
    ppu.writeRegister(0x2106, 0x00); // MOSAIC: Mosaic size and enable
    ppu.writeRegister(0x2107, 0x00); // BG1SC: BG1 screen size and character base
    ppu.writeRegister(0x2108, 0x00); // BG2SC: BG2 screen size and character base
    ppu.writeRegister(0x2109, 0x00); // BG3SC: BG3 screen size and character base
    ppu.writeRegister(0x210A, 0x00); // BG4SC: BG4 screen size and character base
    ppu.writeRegister(0x210B, 0x00); // BG12NBA: BG1/BG2 character base
    ppu.writeRegister(0x210C, 0x00); // BG34NBA: BG3/BG4 character base
    ppu.writeRegister(0x210D, 0x00); // BG1HOFS: BG1 horizontal scroll
    ppu.writeRegister(0x210E, 0x00); // BG1VOFS: BG1 vertical scroll
    ppu.writeRegister(0x210F, 0x00); // BG2HOFS: BG2 horizontal scroll
    ppu.writeRegister(0x2110, 0x00); // BG2VOFS: BG2 vertical scroll
    ppu.writeRegister(0x2111, 0x00); // BG3HOFS: BG3 horizontal scroll
    ppu.writeRegister(0x2112, 0x00); // BG3VOFS: BG3 vertical scroll
    ppu.writeRegister(0x2113, 0x00); // BG4HOFS: BG4 horizontal scroll
    ppu.writeRegister(0x2114, 0x00); // BG4VOFS: BG4 vertical scroll
    ppu.writeRegister(0x2115, 0x00); // VMAIN: VRAM address increment
    ppu.writeRegister(0x2116, 0x00); // VMADDL: VRAM address low
    ppu.writeRegister(0x2117, 0x00); // VMADDH: VRAM address high
    ppu.writeRegister(0x2118, 0x00); // VMDATAL: VRAM data low
    ppu.writeRegister(0x2119, 0x00); // VMDATAH: VRAM data high
    ppu.writeRegister(0x211A, 0x00); // M7SEL: Mode 7 settings
    ppu.writeRegister(0x211B, 0x00); // M7A: Mode 7 matrix A
    ppu.writeRegister(0x211C, 0x00); // M7B: Mode 7 matrix B
    ppu.writeRegister(0x211D, 0x00); // M7C: Mode 7 matrix C
    ppu.writeRegister(0x211E, 0x00); // M7D: Mode 7 matrix D
    ppu.writeRegister(0x211F, 0x00); // M7X: Mode 7 center X
    ppu.writeRegister(0x2120, 0x00); // M7Y: Mode 7 center Y
    ppu.writeRegister(0x2121, 0x00); // CGADD: CGRAM address
    ppu.writeRegister(0x2122, 0x00); // CGDATA: CGRAM data
    ppu.writeRegister(0x2123, 0x00); // W12SEL: Window mask settings
    ppu.writeRegister(0x2124, 0x00); // W34SEL: Window mask settings
    ppu.writeRegister(0x2125, 0x00); // WOBJSEL: Window mask settings
    ppu.writeRegister(0x2126, 0x00); // WH0: Window 1 left position
    ppu.writeRegister(0x2127, 0x00); // WH1: Window 1 right position
    ppu.writeRegister(0x2128, 0x00); // WH2: Window 2 left position
    ppu.writeRegister(0x2129, 0x00); // WH3: Window 2 right position
    ppu.writeRegister(0x212A, 0x00); // WH4: Window 3 left position
    ppu.writeRegister(0x212B, 0x00); // WH5: Window 3 right position
    ppu.writeRegister(0x212C, 0x00); // WH6: Window 4 left position
    ppu.writeRegister(0x212D, 0x00); // WH7: Window 4 right position
    ppu.writeRegister(0x212E, 0x00); // WH8: Window 5 left position
    ppu.writeRegister(0x212F, 0x00); // WH9: Window 5 right position
    ppu.writeRegister(0x2130, 0x00); // WHA: Window 6 left position
    ppu.writeRegister(0x2131, 0x00); // WHB: Window 6 right position
    ppu.writeRegister(0x2132, 0x00); // WHC: Window 7 left position
    ppu.writeRegister(0x2133, 0x00); // WHD: Window 7 right position
    ppu.writeRegister(0x2134, 0x00); // WHE: Window 8 left position
    ppu.writeRegister(0x2135, 0x00); // WHF: Window 8 right position
    ppu.writeRegister(0x2136, 0x00); // TM: Main screen designation
    ppu.writeRegister(0x2137, 0x00); // TS: Sub screen designation
    ppu.writeRegister(0x2138, 0x00); // TMW: Window mask main screen
    ppu.writeRegister(0x2139, 0x00); // TSW: Window mask sub screen
    ppu.writeRegister(0x213A, 0x00); // TMW: Window mask main screen
    ppu.writeRegister(0x213B, 0x00); // TSW: Window mask sub screen
    ppu.writeRegister(0x213C, 0x00); // TMW: Window mask main screen
    ppu.writeRegister(0x213D, 0x00); // TSW: Window mask sub screen
    ppu.writeRegister(0x213E, 0x00); // TMW: Window mask main screen
    ppu.writeRegister(0x213F, 0x00); // TSW: Window mask sub screen
    ppu.writeRegister(0x2140, 0x00); // CGWSEL: Color math settings
    ppu.writeRegister(0x2141, 0x00); // CGADSUB: Color math settings
    ppu.writeRegister(0x2142, 0x00); // COLDATA: Color math settings
    ppu.writeRegister(0x2143, 0x00); // SETINI: Screen mode/color settings
    std::cout << "PPU registers initialized." << std::endl;
    
    // Print initial vectors after initialization
    std::vector<uint16_t> currentVectors(6);
    
    // Determine vector base address based on CPU mode
    uint16_t vectorBaseAddr;
    if (cpu.getEmulationMode()) {
        vectorBaseAddr = 0xFFF4;  // Emulation mode vectors
    } else {
        vectorBaseAddr = 0xFFE4;  // Native mode vectors
    }
    
    for (int i = 0; i < 6; i++) {
        currentVectors[i] = memory.read16(vectorBaseAddr + i*2);
    }
    
    
    printVectorsHexViewer(originalVectors, "=== ORIGINAL ROM HEADER VECTORS ===", cpu.getEmulationMode());
    printVectorsHexViewer(currentVectors, "=== INITIALIZATION COMPLETE - VECTORS ===", cpu.getEmulationMode());
    compareVectors(originalVectors, currentVectors, "=== VECTOR COMPARISON ===");
    
    bool running = true;
    uint64_t frameCount = 0;
    uint64_t cycleCount = 0;
    SDL_Event event;
    
    std::cout << "Starting emulation loop..." << std::endl;
    std::cout << "Controls: Arrow keys, Z/X/A/S for buttons" << std::endl;
    
    while (running) {
        
        // SNES Hardware Clock Synchronization
        // Master Clock: 21.477272 MHz
        // CPU: Master ÷ 6 = 3.579545 MHz (6 cycles per master)
        // PPU: Master ÷ 4 = 5.369318 MHz (4 cycles per master) 
        // APU: Master ÷ 8 = 2.684659 MHz (3 cycles per master)
        
        static int masterCycles = 0;
        static int frameCycles = 0;
        const int CYCLES_PER_FRAME = 1364 * 262; // ~357,368 master cycles per frame
        
        // Run one master clock cycle
        if(++masterCycles >= 24){
            masterCycles = 0;
        }
        frameCycles++;
        
        // CPU runs every 6 master cycles (3.58MHz)
        if (masterCycles == 0 || masterCycles == 6 || masterCycles == 12 || masterCycles == 18) {
            cpu.step();
            cycleCount++;
            
            // Print vectors every 20000 cycles
            if (cycleCount % 20000 == 0) {
                std::vector<uint16_t> runtimeVectors(6);
                
                // Determine vector base address based on CPU mode
                uint16_t vectorBaseAddr;
                if (cpu.getEmulationMode()) {
                    vectorBaseAddr = 0xFFF4;  // Emulation mode vectors
                } else {
                    vectorBaseAddr = 0xFFE4;  // Native mode vectors
                }
                
                for (int i = 0; i < 6; i++) {
                    runtimeVectors[i] = memory.read16(vectorBaseAddr + i*2);
                }
                printVectorsHexViewer(runtimeVectors, "=== CYCLE " + std::to_string(cycleCount) + " - VECTORS ===", cpu.getEmulationMode());
            }
        }
        
        // PPU runs every 4 master cycles (5.37MHz)
        if (masterCycles == 0 || masterCycles == 4 || masterCycles == 8 || masterCycles == 12 || masterCycles == 16 || masterCycles == 20) {
            ppu.step();
        }
        
        // APU runs every 8 master cycles (2.68MHz)
        if (masterCycles == 0 || masterCycles == 8 || masterCycles == 16 || masterCycles == 24) {
            apu.step();
        }
        if(ppu.isFrameReady()){
            
            ppu.renderFrame();
            ppu.clearFrameReady();

            frameCount++;
            frameCycles = 0;
            
            if (frameCount % 60 == 0) {
                std::cout << "Frame: " << frameCount << ", CPU Cycles: " << cpu.getCycles() << std::endl;
            }

            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }
                input.update();
            }
            // Update DSP (only after boot)
            // Always generate audio (test tone will play if no channels enabled)
            apu.generateAudio();
        }
        
    }
    
    std::cout << "Emulation finished." << std::endl;
    system("pause");
    // Cleanup
    apu.cleanup();
    ppu.cleanup();
    SDL_Quit();
    
    return 0;
}
