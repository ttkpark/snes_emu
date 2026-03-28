#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdlib.h>
#include <windows.h>
#include <iomanip>
#include <chrono>
#include <csignal>
#define SDL_MAIN_HANDLED
#include <SDL.h>

static uint32_t g_lastPC = 0;
static uint64_t g_lastCycle = 0;
void crashHandler(int sig) {
    std::cerr << "CRASH (signal " << sig << ") at PC=0x" << std::hex << g_lastPC
              << " cycle=" << std::dec << g_lastCycle << std::endl;
    exit(1);
}

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
    signal(SIGSEGV, crashHandler);
    signal(SIGABRT, crashHandler);
    std::cout << "SNES Emulator - Complete SDL2 Version" << std::endl;
    
    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }
    
    // Check for cycle limit argument
    int cycleLimit = -1; // -1 means no limit
    if (argc > 2) {
        cycleLimit = std::atoi(argv[2]);
        std::cout << "Cycle limit set to: " << cycleLimit << std::endl;
    }
    
    // Load ROM
    std::string romPath = "spctest.sfc";
    if (argc > 1) {
        romPath = argv[1];
        std::cout << "Loading ROM from command line: " << romPath << std::endl;
    }
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
    cpu.setPPU(&ppu);  // Set PPU reference in CPU for VRAM dumps
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
        
        // If all checksums are invalid, validate by checking reset vector code
        if (!loROMValid && !hiROMValid && !exHiROMValid) {
            std::cout << "  All checksums invalid, validating reset vectors..." << std::endl;

            // Check if LoROM reset vector points to valid code
            uint16_t loResetVec = romData[0x7FFC] | (romData[0x7FFD] << 8);
            uint32_t loCodeAddr = (loResetVec & 0x7FFF); // LoROM: offset $8000+ maps to ROM bank*0x8000
            bool loCodeValid = (loResetVec >= 0x8000 && loCodeAddr < romData.size() && romData[loCodeAddr] != 0x00);

            // Check if HiROM reset vector points to valid code
            uint16_t hiResetVec = romData[0xFFFC] | (romData[0xFFFD] << 8);
            uint32_t hiCodeAddr = hiResetVec; // HiROM: bank $00 offset maps directly
            bool hiCodeValid = (hiCodeAddr < romData.size() && romData[hiCodeAddr] != 0x00);

            std::cout << "  LoROM reset=0x" << std::hex << loResetVec << " code=" << (loCodeValid ? "VALID" : "INVALID") << std::endl;
            std::cout << "  HiROM reset=0x" << hiResetVec << " code=" << (hiCodeValid ? "VALID" : "INVALID") << std::dec << std::endl;

            if (loCodeValid && !hiCodeValid) {
                loROMValid = true;
            } else if (hiCodeValid && !loCodeValid) {
                hiROMValid = true;
            } else {
                // Both valid or both invalid - restore original detection
                loROMValid = originalLoROMValid;
                hiROMValid = originalHiROMValid;
                exHiROMValid = originalExHiROMValid;
            }
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

        // Chipset detection ($FFD6)
        if (headerPhysicalAddress + 0x16 < romData.size()) {
            uint8_t chipset = romData[headerPhysicalAddress + 0x16];
            uint8_t coprocessorNibble = (chipset >> 4) & 0x0F;
            if (coprocessorNibble == 0x1) {
                std::cout << "[WARN] SuperFX/GSU detected (chipset=0x" << std::hex << (int)chipset << std::dec
                          << ") - not implemented; ROM may not run." << std::endl;
            } else if (coprocessorNibble == 0x0) {
                std::cout << "[WARN] DSP-1/2/3/4 detected (chipset=0x" << std::hex << (int)chipset << std::dec
                          << ") - not implemented; ROM may not run." << std::endl;
            } else if (coprocessorNibble == 0x3) {
                std::cout << "[WARN] SA-1 detected (chipset=0x" << std::hex << (int)chipset << std::dec
                          << ") - not implemented; ROM may not run." << std::endl;
            }
        }
        
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
    ppu.writeRegister(0x212A, 0x00); // WBGLOG: Window mask logic for BGs
    ppu.writeRegister(0x212B, 0x00); // WOBJLOG: Window mask logic for OBJs
    ppu.writeRegister(0x212C, 0x00); // TM: Main screen designation
    ppu.writeRegister(0x212D, 0x00); // TS: Sub screen designation
    ppu.writeRegister(0x212E, 0x00); // TMW: Window mask main screen
    ppu.writeRegister(0x212F, 0x00); // TSW: Window mask sub screen
    ppu.writeRegister(0x2130, 0x00); // CGWSEL: Color math settings
    ppu.writeRegister(0x2131, 0x00); // CGADSUB: Color math settings
    ppu.writeRegister(0x2132, 0x00); // COLDATA: Color math settings
    ppu.writeRegister(0x2133, 0x00); // SETINI: Screen mode/color settings
    // Note: 0x2134-0x2135 are read-only (OPHCT/OPVCT)
    // Note: 0x2136-0x2137 are read-only (STAT77/STAT78)
    // Note: 0x2138-0x2139 are read-only (OPHCT/OPVCT)
    // Note: 0x213A-0x213B are read-only (STAT77/STAT78)
    // Note: 0x213C-0x213D are read-only (OPHCT/OPVCT)
    // Note: 0x213E-0x213F are read-only (STAT77/STAT78)
    // Note: 0x2140-0x2143 are APU I/O ports, not PPU registers
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
    
    // Test timer - run for 30 seconds for basic CPU tests
    auto startTime = std::chrono::high_resolution_clock::now();
    const auto testDuration = std::chrono::seconds(60);
    uint64_t cycleCount = 0;
    SDL_Event event;
    
    std::cout << "Starting emulation loop..." << std::endl;
    std::cout << "Controls: Arrow keys, Z/X/A/S for buttons" << std::endl;
    
    while (running) {
        // No timeout for interactive use - user closes window to exit
        
        // Process SDL events once per frame
        {
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }
                input.handleEvent(event);
            }
        }

        // Run one full frame: 262 scanlines × 341 dots = 89342 master cycles
        static int ppuCounter = 0;
        static int cpuCounter = 0;
        static int apuCounter = 0;
        static int lastScanline = -1;

        for (int mc = 0; mc < 89342 && running; mc++) {
            ppuCounter++;
            cpuCounter++;
            apuCounter++;

            if (ppuCounter >= 4) {
                ppuCounter = 0;
                ppu.step();

                // Auto-Joypad at VBlank
                if (lastScanline != 225 && ppu.getScanline() == 225) {
                    memory.performAutoJoypadRead();
                }
                lastScanline = ppu.getScanline();
            }

            if (cpuCounter >= 6) {
                cpuCounter = 0;
                // Skip CPU during DMA (CPU is halted while DMA runs)
                if (memory.m_dmaCyclesPending > 0) {
                    memory.m_dmaCyclesPending -= 6;
                } else {
                    cpu.step();
                    if (cpu.m_quitEmulation) { running = false; break; }
                }
                cycleCount++;
            }

            if (apuCounter >= 2) {
                apuCounter = 0;
                apu.step();
            }
        } // end frame loop

        // Render frame if ready
        if (ppu.isFrameReady()) {
            ppu.renderFrame();
            ppu.clearFrameReady();
            frameCount++;

            // Auto-test: simulate input after initialization
            if (frameCount == 15) {
                std::cout << "[AUTO] Frame " << frameCount << ": pressing DOWN" << std::endl;
                input.setButton(SimpleInput::BIT_DOWN, true);
            } else if (frameCount == 16) {
                input.setButton(SimpleInput::BIT_DOWN, false);
            } else if (frameCount == 18) {
                std::cout << "[AUTO] Frame " << frameCount << ": pressing START" << std::endl;
                input.setButton(SimpleInput::BIT_START, true);
            } else if (frameCount == 19) {
                input.setButton(SimpleInput::BIT_START, false);
            }
        }

    }
    
    std::cout << "Emulation finished." << std::endl;
    std::cout << "Total CPU instructions executed: " << cycleCount << std::endl;
    //system("pause");
    // Cleanup
    apu.cleanup();
    ppu.cleanup();
    SDL_Quit();
    
    return 0;
}
