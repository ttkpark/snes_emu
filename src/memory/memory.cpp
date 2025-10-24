#include "memory.h"
#include "../ppu/ppu.h"
#include "../apu/apu.h"
#include <iostream>
#include <iomanip>
#include <ostream>

Memory::Memory() : m_cpu(nullptr), m_ppu(nullptr), m_apu(nullptr), m_input(nullptr) {
    m_wram.resize(128 * 1024, 0);
    m_sram.resize(32 * 1024, 0);
    m_romMapping = ROMMapping::Unknown;
}

bool Memory::loadROM(const std::vector<uint8_t>& romData) {
    m_rom = romData;
    // Don't auto-detect mapping here - it will be set externally
    std::cout << "ROM loaded into memory: " << m_rom.size() << " bytes" << std::endl;
    return true;
}

void Memory::setROMMapping(ROMMapping mapping) {
    m_romMapping = mapping;
    std::cout << "ROM Mapping set to: ";
    switch (m_romMapping) {
        case ROMMapping::LoROM: std::cout << "LoROM"; break;
        case ROMMapping::HiROM: std::cout << "HiROM"; break;
        case ROMMapping::ExLoROM: std::cout << "ExLoROM"; break;
        case ROMMapping::ExHiROM: std::cout << "ExHiROM"; break;
        default: std::cout << "Unknown"; break;
    }
    std::cout << std::endl;
}

void Memory::setHeaderPhysicalAddress(uint32_t address) {
    m_headerPhysicalAddress = address;
}

uint32_t Memory::getHeaderPhysicalAddress() const {
    return m_headerPhysicalAddress;
}

uint8_t Memory::read8(uint32_t address) {
    // LoROM memory map for SNES
    // Banks $00-$7F: ROM at $8000-$FFFF (32KB per bank)
    // Banks $80-$FF: ROM mirror
    // Work RAM: $7E0000-$7FFFFF (128KB)
    
    uint8_t bank = (address >> 16) & 0xFF;
    uint16_t offset = address & 0xFFFF;

    bool isROMShort = bank >= 0x00 && bank <= 0x3F || bank >= 0x80 && bank <= 0xBF;
    bool isROMLong = bank >= 0x40 && bank <= 0x7D || bank >= 0xC0 && bank <= 0xFF;
    
    // Work RAM ($7E and $7F banks)
    if (bank == 0x7E || bank == 0x7F) {
        uint32_t wramAddr = (address & 0x1FFFF);
        if (wramAddr < m_wram.size()) {
            return m_wram[wramAddr];
        }
        return 0;
    }
    if (isROMShort) {
        // Low RAM mirror ($0000-$1FFF in banks $00-$3F and $80-$BF)
        if (offset < 0x2000) {
            return m_wram[offset];
        }
        
        // APU I/O ports ($2140-$2143 in banks $00-$3F and $80-$BF) - CHECK BEFORE OTHER I/O!
        if (offset >= 0x2140 && offset < 0x2144) {
            // Forward to APU
            if (m_apu) {
                return m_apu->readPort(offset - 0x2140);
            }
            return 0x00;
        }
        
        // I/O registers ($2100-$21FF in banks $00-$3F and $80-$BF) - EXCEPT APU ports
        if (offset >= 0x2100 && offset < 0x2200) {
            // PPU registers - forward to PPU
            if (m_ppu) {
                return m_ppu->readRegister(offset);
            }
            return 0;
        }
        
        // CPU I/O registers ($4200-$421F in banks $00-$3F and $80-$BF)
        if (offset >= 0x4200 && offset < 0x4220) {
            // Forward to PPU (for NMI control)
            if (m_ppu) {
                return m_ppu->readRegister(offset);
            }
            return 0;
        }
    }

    // SRAM area ($6000-$7FFF in banks $70-$7F, and $F0-$FF for LoROM)
    if ((offset >= 0x6000 && offset < 0x8000) && ((bank >= 0x70 && bank < 0x80) || (bank >= 0xF0))) {
        uint32_t sramAddr = ((bank & 0x0F) * 0x2000) + (offset - 0x6000);
        if (sramAddr < m_sram.size()) {
            return m_sram[sramAddr];
        }
    }
    
    // ROM area ($8000-$FFFF in banks $00-$7F, and $80-$FF)
    if ((isROMShort && offset >= 0x8000) || isROMLong) {
        // Use detected ROM mapping for address translation
        uint32_t romOffset = getROMAddress(address, m_romMapping);
        
        if (romOffset < m_rom.size()) {
            return m_rom[romOffset];
        }
    }
    
    
    return 0;
}

uint16_t Memory::read16(uint32_t address) {
    uint8_t low = read8(address);
    uint8_t high = read8(address + 1);
    return low | (high << 8);
}

void Memory::write8(uint32_t address, uint8_t value) {
    uint8_t bank = (address >> 16) & 0xFF;
    uint16_t offset = address & 0xFFFF;
    
    // Work RAM ($7E and $7F banks)
    if (bank == 0x7E || bank == 0x7F) {
        uint32_t wramAddr = (address & 0x1FFFF);
        if (wramAddr < m_wram.size()) {
            m_wram[wramAddr] = value;
        }
        return;
    }
    
    // Low RAM mirror ($0000-$1FFF in banks $00-$3F and $80-$BF)
    if (offset < 0x2000 && ((bank < 0x40) || (bank >= 0x80 && bank < 0xC0))) {
        m_wram[offset] = value;
        return;
    }
    
    // APU I/O ports ($2140-$2143 in banks $00-$3F and $80-$BF) - CHECK BEFORE PPU!
    if (offset >= 0x2140 && offset < 0x2144 && ((bank < 0x40) || (bank >= 0x80 && bank < 0xC0))) {
        // Forward to APU
        if (m_apu) {
            m_apu->writePort(offset - 0x2140, value);
        }
        return;
    }
    
    // I/O registers ($2100-$21FF in banks $00-$3F and $80-$BF)
    if (offset >= 0x2100 && offset < 0x2200 && ((bank < 0x40) || (bank >= 0x80 && bank < 0xC0))) {
        // Log PPU register writes
        static int ppuWriteCount = 0;
        if (ppuWriteCount < 50) {
            std::cout << "Memory: PPU Write [$" << std::hex << offset << "] = $" << (int)value << std::dec << std::endl;
            ppuWriteCount++;
        }
        // PPU registers - forward to PPU
        if (m_ppu) {
            m_ppu->writeRegister(offset, value);
        }
        return;
    }
    
    // CPU I/O registers ($4200-$421F in banks $00-$3F and $80-$BF)
    if (offset >= 0x4200 && offset < 0x4220 && ((bank < 0x40) || (bank >= 0x80 && bank < 0xC0))) {
        // Log CPU I/O register writes
        static int cpuIoWriteCount = 0;
        if (cpuIoWriteCount < 50) {
            std::cout << "Memory: CPU I/O Write [$" << std::hex << offset << "] = $" << (int)value << std::dec << std::endl;
            cpuIoWriteCount++;
        }
        
        // Handle specific CPU I/O registers
        if (offset == 0x420B) { // MDMAEN - DMA Enable
            std::cout << "PPU: DMA Enable=$" << std::hex << (int)value << std::dec << std::endl;
            // Enable DMA channels based on bit flags
            for (int i = 0; i < 8; i++) {
                if (value & (1 << i)) {
                    m_dmaChannels[i].control |= 0x80; // Enable DMA
                    performDMA(i); // Execute DMA immediately
                }
            }
        } else if (offset == 0x420C) { // HDMAEN - HDMA Enable
            std::cout << "PPU: HDMA Enable=$" << std::hex << (int)value << std::dec << std::endl;
            // HDMA not implemented yet
        } else {
            // Forward to PPU (for NMI control)
            if (m_ppu) {
                m_ppu->writeRegister(offset, value);
            }
        }
        return;
    }
    
    // DMA registers ($43xx in banks $00-$3F and $80-$BF)
    if (offset >= 0x4300 && offset < 0x4380 && ((bank < 0x40) || (bank >= 0x80 && bank < 0xC0))) {
        // Log DMA register writes
        static int dmaRegWriteCount = 0;
        if (dmaRegWriteCount < 50) {
            std::cout << "Memory: DMA Register Write [$" << std::hex << offset << "] = $" << (int)value << std::dec << std::endl;
            dmaRegWriteCount++;
        }
        
        // Handle DMA register writes
        uint8_t channel = (offset - 0x4300) / 8;
        uint8_t reg = (offset - 0x4300) % 8;
        
        if (channel < 8) {
            switch (reg) {
                case 0: // Control register
                    m_dmaChannels[channel].control = value;
                    break;
                case 1: // Destination register
                    m_dmaChannels[channel].destAddr = value;
                    break;
                case 2: // Source address low
                    m_dmaChannels[channel].sourceAddr = (m_dmaChannels[channel].sourceAddr & 0xFF00) | value;
                    break;
                case 3: // Source address high
                    m_dmaChannels[channel].sourceAddr = (m_dmaChannels[channel].sourceAddr & 0x00FF) | (value << 8);
                    break;
                case 4: // Source bank
                    m_dmaChannels[channel].sourceBank = value;
                    break;
                case 5: // Size low
                    m_dmaChannels[channel].size = (m_dmaChannels[channel].size & 0xFF00) | value;
                    break;
                case 6: // Size high
                    m_dmaChannels[channel].size = (m_dmaChannels[channel].size & 0x00FF) | (value << 8);
                    break;
            }
        }
        return;
    }
    
    // SRAM area
    if (offset >= 0x6000 && offset < 0x8000) {
        if ((bank >= 0x70 && bank < 0x80) || (bank >= 0xF0)) {
            uint32_t sramAddr = ((bank & 0x0F) * 0x2000) + (offset - 0x6000);
            if (sramAddr < m_sram.size()) {
                m_sram[sramAddr] = value;
            }
        }
    }
    
    // ROM is read-only, ignore writes
}

void Memory::write16(uint32_t address, uint16_t value) {
    write8(address, value & 0xFF);
    write8(address + 1, (value >> 8) & 0xFF);
}

void Memory::performDMA(uint8_t channel) {
    if (channel >= 8) return;
    
    DMAChannel& dma = m_dmaChannels[channel];
    
    // Check if DMA is enabled
    if (!(dma.control & 0x80)) return;
    
    // Get transfer direction (0 = CPU to PPU, 1 = PPU to CPU)
    bool toPPU = !(dma.control & 0x01);
    
    // Get transfer mode
    uint8_t mode = (dma.control >> 1) & 0x03;
    
    // Calculate source address
    uint32_t sourceAddr = (dma.sourceBank << 16) | dma.sourceAddr;
    
    // Calculate destination address (PPU register)
    uint32_t destAddr = 0x2100 | dma.destAddr;
    
    std::cout << "DMA Channel " << (int)channel << ": " 
              << (toPPU ? "CPU->PPU" : "PPU->CPU") 
              << " Mode=" << (int)mode 
              << " Size=" << dma.size 
              << " Source=0x" << std::hex << sourceAddr 
              << " Dest=0x" << destAddr << std::dec << std::endl;
    
    // Perform transfer based on mode
    switch (mode) {
        case 0: // 1 register, write once
            if (toPPU && dma.size > 0) {
                uint8_t data = read8(sourceAddr);
                if (m_ppu) {
                    m_ppu->writeRegister(dma.destAddr, data);
                }
            }
            break;
            
        case 1: // 2 registers, write twice
            if (toPPU && dma.size > 0) {
                for (uint16_t i = 0; i < dma.size; i++) {
                    uint8_t data = read8(sourceAddr + i);
                    if (m_ppu) {
                        m_ppu->writeRegister(dma.destAddr, data);
                    }
                }
            }
            break;
            
        case 2: // 1 register, write twice
        case 3: // 4 registers, write once
            // Simplified implementation
            if (toPPU && dma.size > 0) {
                for (uint16_t i = 0; i < dma.size; i++) {
                    uint8_t data = read8(sourceAddr + i);
                    if (m_ppu) {
                        m_ppu->writeRegister(dma.destAddr, data);
                    }
                }
            }
            break;
    }
    
    // Disable DMA after transfer
    dma.control &= ~0x80;
}

uint32_t Memory::translateAddress(uint32_t address) {
    // This is now unused, keeping for compatibility
    return address;
}

Memory::ROMMapping Memory::detectROMMapping() {
    if (m_rom.size() < 0x8000) {
        return ROMMapping::Unknown;
    }
    
    // Check LoROM vectors (0x7FFA-0x7FFF)
    bool loROMValid = true;
    for (int i = 0; i < 6; i++) {
        uint16_t vector = (m_rom[0x7FFB + i*2] << 8) | m_rom[0x7FFA + i*2];
        if (vector != 0x0000 && (vector < 0x8000 || vector >= 0x10000)) {
            loROMValid = false;
            break;
        }
    }
    
    // Check HiROM vectors (0xFFFA-0xFFFF)
    bool hiROMValid = true;
    for (int i = 0; i < 6; i++) {
        uint16_t vector = (m_rom[0xFFFB + i*2] << 8) | m_rom[0xFFFA + i*2];
        if (vector != 0x0000 && (vector < 0x8000 || vector >= 0x10000)) {
            hiROMValid = false;
            break;
        }
    }
    
    // Check ExHiROM vectors (0x40FFFA-0x40FFFF)
    bool exHiROMValid = false;
    if (m_rom.size() >= 0x410000) {
        exHiROMValid = true;
        for (int i = 0; i < 6; i++) {
            uint16_t vector = (m_rom[0x40FFFB + i*2] << 8) | m_rom[0x40FFFA + i*2];
            if (vector != 0x0000 && (vector < 0x8000 || vector >= 0x10000)) {
                exHiROMValid = false;
                break;
            }
        }
    }
    
    // Check ExLoROM vectors (0x407FFA-0x407FFF)
    bool exLoROMValid = false;
    if (m_rom.size() >= 0x408000) {
        exLoROMValid = true;
        for (int i = 0; i < 6; i++) {
            uint16_t vector = (m_rom[0x407FFB + i*2] << 8) | m_rom[0x407FFA + i*2];
            if (vector != 0x0000 && (vector < 0x8000 || vector >= 0x10000)) {
                exLoROMValid = false;
                break;
            }
        }
    }
    
    // Check ROM header for additional info
    if (m_rom.size() >= 0x7FC0) {
        uint8_t romType = m_rom[0x7FD5];
        if (romType & 0x10) return ROMMapping::ExHiROM;
        if (romType & 0x20) return ROMMapping::ExLoROM;
        if (romType & 0x02) return ROMMapping::HiROM;
        if (romType & 0x01) return ROMMapping::LoROM;
    }
    
    // Determine based on vector validity
    if (exHiROMValid && !hiROMValid && !loROMValid && !exLoROMValid) {
        return ROMMapping::ExHiROM;
    } else if (exLoROMValid && !loROMValid && !hiROMValid && !exHiROMValid) {
        return ROMMapping::ExLoROM;
    } else if (hiROMValid && !loROMValid && !exHiROMValid && !exLoROMValid) {
        return ROMMapping::HiROM;
    } else if (loROMValid && !hiROMValid && !exHiROMValid && !exLoROMValid) {
        return ROMMapping::LoROM;
    } else if (loROMValid && hiROMValid) {
        // Ambiguous - prefer LoROM for smaller ROMs
        return ROMMapping::LoROM;
    }
    
    return ROMMapping::Unknown;
}

uint32_t Memory::getROMAddress(uint32_t address, ROMMapping mapping) {
    uint8_t bank = (address >> 16) & 0xFF;
    uint32_t offset = address & 0x0000FFFF;
    
    switch (mapping) {
        case ROMMapping::LoROM:
            // LoROM: Banks $00-$7F map to ROM at $8000-$FFFF
            if (bank >= 0x00 && bank <= 0x7F && offset >= 0x8000) {
                return ((bank - 0x00) * 0x8000) + (offset - 0x8000);
            }
            // Banks $80-$FF mirror banks $00-$7F
            else if (bank >= 0x80 && bank <= 0xFF && offset >= 0x8000) {
                return ((bank - 0x80) * 0x8000) + (offset - 0x8000);
            }
            break;
            
        case ROMMapping::HiROM:
            // HiROM: Banks $C0-$FF map to ROM at $0000-$FFFF
            if (bank >= 0xC0 && bank <= 0xFF) {
                return ((bank - 0xC0) * 0x10000) + offset;
            }
            // Banks $80-$BF mirror banks $C0-$FF
            else if (bank >= 0x80 && bank <= 0xBF) {
                return ((bank - 0x80) * 0x10000) + offset;
            }
            break;
            
        case ROMMapping::ExLoROM:
            // ExLoROM: Extended LoROM with 4MB+ support
            if (bank >= 0x00 && bank <= 0x7F && offset >= 0x8000) {
                return ((bank - 0x00) * 0x8000) + (offset - 0x8000);
            }
            else if (bank >= 0x80 && bank <= 0xFF && offset >= 0x8000) {
                return ((bank - 0x80) * 0x8000) + (offset - 0x8000);
            }
            break;
            
        case ROMMapping::ExHiROM:
            // ExHiROM: Extended HiROM with 4MB+ support
            if (bank >= 0xC0 && bank <= 0xFF) {
                return ((bank - 0xC0) * 0x10000) + offset;
            }
            else if (bank >= 0x80 && bank <= 0xBF) {
                return ((bank - 0x80) * 0x10000) + offset;
            }
            break;
            
        default:
            break;
    }
    
    return 0xFFFFFFFF; // Invalid address
}
