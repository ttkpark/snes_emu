#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <stdlib.h>
#include <windows.h>
#include <io.h>
#include <fcntl.h>
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

    // Headless mode: --headless → no SDL window, binary frames to stdout, input from stdin
    bool headlessMode = false;
    uint64_t maxFrames = 0; // 0 = unlimited
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--headless") {
            headlessMode = true;
        } else if (std::string(argv[i]).rfind("--max-frames=", 0) == 0) {
            maxFrames = (uint64_t)std::atoi(argv[i] + 13);
        }
    }
    if (headlessMode) {
        // Redirect std::cout to stderr so binary stdout stays clean
        std::cout.rdbuf(std::cerr.rdbuf());
        // Binary mode stdout/stdin for frame pipe
        _setmode(_fileno(stdout), _O_BINARY);
        _setmode(_fileno(stdin),  _O_BINARY);
        // Dummy SDL drivers (no window/audio device)
        _putenv("SDL_VIDEODRIVER=dummy");
        _putenv("SDL_AUDIODRIVER=dummy");
    }

    std::cout << "SNES Emulator - Complete SDL2 Version" << std::endl;

    // Initialize SDL
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) < 0) {
        std::cerr << "Failed to initialize SDL: " << SDL_GetError() << std::endl;
        return 1;
    }

    // Check for cycle limit argument (argv[2] if it looks like a number)
    int cycleLimit = -1; // -1 means no limit
    if (argc > 2) {
        const char* a = argv[2];
        if (a[0] != '-' || (a[1] >= '0' && a[1] <= '9')) {
            cycleLimit = std::atoi(a);
            if (cycleLimit > 0)
                std::cout << "Cycle limit set to: " << cycleLimit << std::endl;
        }
    }
    
    // Load ROM (skip --headless and other flag arguments)
    std::string romPath = "spctest.sfc";
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]).rfind("--", 0) != 0) {
            romPath = argv[i];
            std::cout << "Loading ROM from command line: " << romPath << std::endl;
            break;
        }
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
    
    // Initialize PPU video (skip in headless mode)
    if (!headlessMode && !ppu.initVideo()) {
        std::cerr << "Failed to initialize PPU video." << std::endl;
        SDL_Quit();
        return 1;
    }

    // Initialize APU audio (skip in headless mode)
    if (!headlessMode && !apu.initAudio()) {
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
    
    // Headless stdin: accumulated line buffer for button commands
    static char s_stdinBuf[4096];
    static int  s_stdinBufLen = 0;

    while (running) {
        // No timeout for interactive use - user closes window to exit

        if (headlessMode) {
            // Non-blocking stdin read for button commands: "BTN <NAME> <0|1>\n"
            HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
            DWORD bytesAvail = 0;
            if (PeekNamedPipe(hStdin, nullptr, 0, nullptr, &bytesAvail, nullptr) && bytesAvail > 0) {
                int _avail = (int)bytesAvail, _space = (int)(sizeof(s_stdinBuf) - s_stdinBufLen - 1);
                DWORD toRead = (DWORD)(_avail < _space ? _avail : _space);
                DWORD nRead = 0;
                if (ReadFile(hStdin, s_stdinBuf + s_stdinBufLen, toRead, &nRead, nullptr)) {
                    s_stdinBufLen += (int)nRead;
                    s_stdinBuf[s_stdinBufLen] = '\0';
                    char* lineStart = s_stdinBuf;
                    char* nl;
                    while ((nl = strchr(lineStart, '\n')) != nullptr) {
                        *nl = '\0';
                        char btnName[32] = {};
                        int pressed = 0;
                        if (sscanf(lineStart, "BTN %31s %d", btnName, &pressed) == 2) {
                            std::string nm(btnName);
                            SimpleInput::ButtonBit bit = SimpleInput::BIT_B;
                            bool found = true;
                            if      (nm == "UP")     bit = SimpleInput::BIT_UP;
                            else if (nm == "DOWN")   bit = SimpleInput::BIT_DOWN;
                            else if (nm == "LEFT")   bit = SimpleInput::BIT_LEFT;
                            else if (nm == "RIGHT")  bit = SimpleInput::BIT_RIGHT;
                            else if (nm == "A")      bit = SimpleInput::BIT_A;
                            else if (nm == "B")      bit = SimpleInput::BIT_B;
                            else if (nm == "X")      bit = SimpleInput::BIT_X;
                            else if (nm == "Y")      bit = SimpleInput::BIT_Y;
                            else if (nm == "L")      bit = SimpleInput::BIT_L;
                            else if (nm == "R")      bit = SimpleInput::BIT_R;
                            else if (nm == "START")  bit = SimpleInput::BIT_START;
                            else if (nm == "SELECT") bit = SimpleInput::BIT_SELECT;
                            else found = false;
                            if (found) {
                                input.setButton(bit, pressed != 0);
                                fprintf(stderr, "[BTN_RECV] Frame=%llu %s %d\n",
                                    (unsigned long long)frameCount, btnName, pressed);
                            }
                        }
                        lineStart = nl + 1;
                    }
                    s_stdinBufLen = (int)(s_stdinBuf + s_stdinBufLen - lineStart);
                    memmove(s_stdinBuf, lineStart, s_stdinBufLen);
                }
            }
        } else {
            // Process SDL events once per frame
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                if (event.type == SDL_QUIT) {
                    running = false;
                }
                input.handleEvent(event);
            }
        }

        // Run one full frame: 262 scanlines × 341 dots (= 89342 master cycles)
        // Real SNES clocks (relative to 21.477 MHz master):
        //   PPU: 1 step every  4 master cycles (5.37 MHz dot clock)
        //   CPU: 1 cycle every 6 master cycles slow-ROM (3.58 MHz avg)
        //   APU: 1 SPC700 cycle every ~21 master cycles (1.024 MHz)
        // NOTE: Our apu.step() executes ONE SPC700 instruction per call (not one cycle).
        //   Average SPC700 instruction = ~4 SPC cycles ≈ 84 master cycles.
        //   CPU:APU master-clock ratio = 6:24 = 4:1 (CPU triggers 4× more often than APU).
        //   Previous code ran apu.step() 3× per 6-MC tick (1 instr per 2 MC) which was
        //   ~40× too fast AND consumed so much CPU time that the host loop dropped to
        //   ~0.35 fps, making IPL transfer take hours. With ~4 ticks/APU instr the APU
        //   still runs slightly faster than real SPC (since 1 step = 1 instr not 1 cycle),
        //   but stays well ahead of CPU STA/CMP handshake loops without choking the host.
        static int lastScanline = -1;
        static int cpuCyclePending = 0;  // extra ticks to stall CPU (slow-ROM timing)
        static int apuTickAccum = 0;     // master-clock accumulator for 4:1 CPU:APU ratio

        // 6 master cycles per tick: 1.5 PPU, 1 CPU step-trigger, 1 APU step every ~4 ticks
        // Use 14890 ticks (= 89340 master cycles, close to 89342)
        for (int tick = 0; tick < 14890 && running; tick++) {
            // PPU: 1 step every 4 master cycles → 1.5 per 6mc tick
            // Alternate: 1 PPU on even ticks, 2 PPU on odd ticks (avg 1.5)
            if (tick & 1) {
                ppu.step();
                ppu.step();
            } else {
                ppu.step();
            }

            // Auto-Joypad at VBlank; HDMA at each H-blank (scanlines 0-223)
            int curScanline = ppu.getScanline();
            if (curScanline != lastScanline) {
                // H-blank DMA: fires once per scanline during active display
                if (lastScanline >= 0 && lastScanline < 224) {
                    memory.performHDMA(lastScanline);
                }
                // Auto-Joypad read at VBlank start
                if (lastScanline != 225 && curScanline == 225) {
                    memory.performAutoJoypadRead();
                    // HDMA re-initialised every VBlank (hardware re-loads table pointers)
                    memory.initHDMA();
                }
                // Tick auto-joypad busy countdown once per scanline
                memory.tickAutoJoypadBusy();
            }
            lastScanline = curScanline;

            // CPU: slow-ROM = 4 cycles/instr × 8MC/cycle = 32MC = ~5.3 ticks/instr
            // Add 4 extra stall ticks after each instruction (net ~5 ticks/instr)
            if (memory.m_dmaCyclesPending > 0) {
                memory.m_dmaCyclesPending -= 6;
            } else if (cpuCyclePending > 0) {
                cpuCyclePending--;
            } else {
                cpu.step();
                // Per-instruction cycle timing (slow-ROM: 1 CPU cycle = 8 MC = ~1.33 ticks of 6 MC)
                // cpuCyclePending = round(lastCycles * 8/6) - 1 = round(lastCycles * 4/3) - 1
                {
                    int lc = cpu.getLastCycles();
                    cpuCyclePending = (lc * 4 + 1) / 3 - 1;
                    if (cpuCyclePending < 0) cpuCyclePending = 0;
                }
                if (cpu.m_quitEmulation) { running = false; break; }
            }
            cycleCount++;

            // APU: target CPU:APU = 4:1 master-clock ratio.
            // One tick = 6 MC. Trigger apu.step() every 4 ticks (24 MC) so the APU
            // runs at 1 SPC instruction per 24 MC. Using a master-clock accumulator
            // keeps the cadence even if the tick budget ever drifts from 4-aligned.
            apuTickAccum += 6; // MC advanced this tick
            while (apuTickAccum >= 24) {
                apu.step();
                apuTickAccum -= 24;
            }
        } // end frame loop

        // Render frame if ready
        if (ppu.isFrameReady()) {
            ppu.renderFrame();
            ppu.clearFrameReady();
            frameCount++;

            if (headlessMode) {
                // Binary frame to stdout: 'FRME' + frame_num(4) + apu_ports(4) + RGBA(229376)
                static const uint8_t kMagic[4] = {'F','R','M','E'};
                uint32_t fn = (uint32_t)frameCount;
                uint8_t apu_ports[4] = {
                    apu.readPort(0), apu.readPort(1),
                    apu.readPort(2), apu.readPort(3)
                };
                fwrite(kMagic,     1, 4, stdout);
                fwrite(&fn,        4, 1, stdout);
                fwrite(apu_ports,  1, 4, stdout);
                fwrite(ppu.getFramebuffer(), 4,
                       PPU::SCREEN_WIDTH * PPU::SCREEN_HEIGHT, stdout);
                fflush(stdout);
            }

            // Save BMP snapshot at specific frames for visual diff against Snes9x reference
            if (frameCount == 30 || frameCount == 60 || frameCount == 120 ||
                frameCount == 180 || frameCount == 300 || frameCount == 450 ||
                frameCount == 540 || frameCount == 580 || frameCount == 600 ||
                frameCount == 900 || frameCount == 920 || frameCount == 940 ||
                frameCount == 950 || frameCount == 955 || frameCount == 960 ||
                frameCount == 965 || frameCount == 970 || frameCount == 980 ||
                frameCount == 1000 ||
                frameCount == 1050 || frameCount == 1100 || frameCount == 1150 ||
                frameCount == 1200 || frameCount == 1250 || frameCount == 1300 ||
                frameCount == 1350 || frameCount == 1400 || frameCount == 1410 ||
                frameCount == 1420 || frameCount == 1430 || frameCount == 1440 ||
                frameCount == 1450 ||
                frameCount == 1500 ||
                frameCount == 2000 || frameCount == 2500 || frameCount == 3000 ||
                frameCount == 3500 || frameCount == 4000 || frameCount == 4200 ||
                frameCount == 4500 || frameCount == 5000) {
                const uint32_t W = PPU::SCREEN_WIDTH, H = PPU::SCREEN_HEIGHT;
                uint32_t rowBytes = ((W * 3 + 3) & ~3u);
                uint32_t dataSz  = rowBytes * H;
                uint32_t fileSz  = 14 + 40 + dataSz;
                char fname[64];
                snprintf(fname, sizeof(fname), "frame_f%04llu.bmp", (unsigned long long)frameCount);
                FILE* bf = fopen(fname, "wb");
                if (bf) {
                    uint8_t hdr[54] = {0};
                    hdr[0]='B'; hdr[1]='M';
                    hdr[2]= fileSz      & 0xFF;
                    hdr[3]=(fileSz>>8)  & 0xFF;
                    hdr[4]=(fileSz>>16) & 0xFF;
                    hdr[5]=(fileSz>>24) & 0xFF;
                    hdr[10]=54;
                    hdr[14]=40;
                    hdr[18]= W     & 0xFF; hdr[19]=(W>>8)&0xFF; hdr[20]=(W>>16)&0xFF; hdr[21]=(W>>24)&0xFF;
                    int32_t negH = -(int32_t)H;
                    hdr[22]= negH      & 0xFF;
                    hdr[23]=(negH>>8)  & 0xFF;
                    hdr[24]=(negH>>16) & 0xFF;
                    hdr[25]=(negH>>24) & 0xFF;
                    hdr[26]=1; hdr[28]=24;
                    hdr[34]= dataSz     & 0xFF;
                    hdr[35]=(dataSz>>8) & 0xFF;
                    hdr[36]=(dataSz>>16)& 0xFF;
                    hdr[37]=(dataSz>>24)& 0xFF;
                    fwrite(hdr, 1, 54, bf);
                    const uint32_t* fb = ppu.getFramebuffer();
                    std::vector<uint8_t> row(rowBytes, 0);
                    for (uint32_t y = 0; y < H; y++) {
                        for (uint32_t x = 0; x < W; x++) {
                            uint32_t px = fb[y*W + x];
                            uint8_t r = px & 0xFF;
                            uint8_t g = (px >> 8) & 0xFF;
                            uint8_t b = (px >> 16) & 0xFF;
                            row[x*3+0] = b;
                            row[x*3+1] = g;
                            row[x*3+2] = r;
                        }
                        fwrite(row.data(), 1, rowBytes, bf);
                    }
                    fclose(bf);
                    fprintf(stderr, "[BMP] Saved %s\n", fname);
                }
            }

            // Max-frames limit (headless testing)
            if (maxFrames > 0 && frameCount >= maxFrames) {
                running = false;
            }

            // FPS counter every 60 frames
            if (frameCount % 60 == 0) {
                auto now = std::chrono::high_resolution_clock::now();
                double elapsed = std::chrono::duration<double>(now - startTime).count();
                double fps = frameCount / elapsed;
                fprintf(stderr, "[PERF] Frame %llu, %.1f FPS (%.1fx speed)\n",
                    (unsigned long long)frameCount, fps, fps / 60.09);
            }

            // Snapshot WRAM[$0000-$00FF] at each frame during test window to find fail flag
            bool isFinalFrame = (maxFrames > 0 && frameCount == maxFrames);
            if ((frameCount >= 168 && frameCount <= 178) || frameCount == 200 || frameCount == 220 || frameCount == 240 || frameCount == 280 || frameCount == 340 || frameCount == 400 || frameCount == 450 || frameCount == 500 || frameCount == 600 || frameCount == 680 || frameCount == 700 || frameCount == 800 || frameCount == 900 || frameCount == 1000 || frameCount == 1100 || frameCount == 1200 || frameCount == 1400 || frameCount == 1410 || frameCount == 1415 || frameCount == 1420 || frameCount == 1425 || isFinalFrame) {
                // Dump key Phase 2 state flags found via NMI handler disassembly
                const auto& w = memory.getWRAM();
                static uint64_t prevCycles = 0;
                static uint64_t prevFrame = 0;
                uint64_t cyc = cpu.getCycles();
                uint64_t dCyc = (prevCycles > 0) ? (cyc - prevCycles) : 0;
                uint64_t dFrame = (prevFrame > 0) ? (frameCount - prevFrame) : 1;
                double cycPerFrame = dFrame > 0 ? (double)dCyc / dFrame : 0;
                fprintf(stderr, "[PHASE-FLAGS] F:%llu $20=%02X $23=%02X $00A1=%02X $48=%02X cyc=%llu dCyc=%llu cpf=%.0f\n",
                    (unsigned long long)frameCount, w[0x0020], w[0x0023], w[0x00A1], w[0x0048],
                    (unsigned long long)cyc, (unsigned long long)dCyc, cycPerFrame);
                prevCycles = cyc;
                prevFrame = frameCount;
                const auto& wram = memory.getWRAM();
                fprintf(stderr, "[SNAP] F:%llu WRAM[00..7F]:", (unsigned long long)frameCount);
                for (int i = 0; i < 0x80; i++) {
                    if (i % 16 == 0) fprintf(stderr, "\n  %02X: ", i);
                    fprintf(stderr, "%02X ", wram[i]);
                }
                fprintf(stderr, "\n[SNAP] F:%llu WRAM[80..FF]:", (unsigned long long)frameCount);
                for (int i = 0x80; i < 0x100; i++) {
                    if (i % 16 == 0) fprintf(stderr, "\n  %02X: ", i);
                    fprintf(stderr, "%02X ", wram[i]);
                }
                fprintf(stderr, "\n");
            }

            // Auto-test: press buttons to advance through test screens
            {
            struct AutoInput { uint64_t frame; SimpleInput::ButtonBit btn; bool press; const char* name; };
            static const AutoInput autoInputs[] = {
                // === Electronics Test (item 1, default) ===
                {90,  SimpleInput::BIT_START,  true,  "START"},   // Enter Electronics Test
                {92,  SimpleInput::BIT_START,  false, nullptr},
                // "PASSED ELECTRONICS TEST" visible at ~F:558; press SELECT to return to menu
                {590, SimpleInput::BIT_SELECT, true,  "SELECT"},  // Exit PASSED screen
                {592, SimpleInput::BIT_SELECT, false, nullptr},
                // === Character Test (item 2) ===
                // Back at main menu ~F:595; SELECT moves cursor from item1 to item2
                {630, SimpleInput::BIT_SELECT, true,  "SELECT"},  // Navigate to item 2
                {632, SimpleInput::BIT_SELECT, false, nullptr},
                {670, SimpleInput::BIT_START,  true,  "START"},   // Enter Character Test
                {672, SimpleInput::BIT_START,  false, nullptr},
                // Character Test runs ~880 frames (F:670-F:1550), auto-exits to menu
                // After Character Test finishes (~F:1560), navigate to Color Test
                // SELECT at F:1600 to exit if needed, then navigate to item 5
                {1600, SimpleInput::BIT_SELECT, true,  "SELECT"},
                {1602, SimpleInput::BIT_SELECT, false, nullptr},
                // Navigate: SELECT cycles menu items: item2→item3→item4→item5
                {1640, SimpleInput::BIT_SELECT, true,  "SELECT"}, // item 3
                {1642, SimpleInput::BIT_SELECT, false, nullptr},
                {1660, SimpleInput::BIT_SELECT, true,  "SELECT"}, // item 4
                {1662, SimpleInput::BIT_SELECT, false, nullptr},
                {1680, SimpleInput::BIT_SELECT, true,  "SELECT"}, // item 5 (Color Test)
                {1682, SimpleInput::BIT_SELECT, false, nullptr},
                {1720, SimpleInput::BIT_START,  true,  "START"},  // Enter Color Test
                {1722, SimpleInput::BIT_START,  false, nullptr},
            };
            for (const auto& ai : autoInputs) {
                if (frameCount == ai.frame) {
                    input.setButton(ai.btn, ai.press);
                    fprintf(stderr, "[AUTO] Frame %llu: %s %s\n", (unsigned long long)frameCount, ai.name ? ai.name : "REL", ai.press ? "PRESS" : "RELEASE");
                }
            }
            // Non-headless: no extra navigation input needed (Electronics Test is default)
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
