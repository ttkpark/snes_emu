#include "memory.h"
#include "../ppu/ppu.h"
#include "../apu/apu.h"
#include "../input/simple_input.h"
#include "../debug/logger.h"
#include <iostream>
#include <iomanip>
#include <ostream>
#include <sstream>

Memory::Memory() : m_cpu(nullptr), m_ppu(nullptr), m_apu(nullptr), m_input(nullptr) {
    m_wram.resize(128 * 1024, 0);
    m_sram.resize(32 * 1024, 0);
    m_romMapping = ROMMapping::Unknown;
    m_autoJoypadEnabled = false;
    for (int i = 0; i < 4; i++) {
        m_joypadData[i] = 0;
    }
    // Initialize I/O registers
    for (int i = 0; i < 0x200; i++) {
        m_ioRegisters[i] = 0;
    }
}

bool Memory::loadROM(const std::vector<uint8_t>& romData) {
    m_rom = romData;
    // Don't auto-detect mapping here - it will be set externally
    std::cout << "ROM loaded into memory: " << m_rom.size() << " bytes" << std::endl;
    
    // Debug: Check font data at offset 0x4D2C
    if (m_rom.size() > 0x502C) {
        std::cout << "Font data check at ROM offset 0x4D2C (first 16 bytes): ";
        for (int i = 0; i < 16; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)m_rom[0x4D2C + i] << " ";
        }
        std::cout << std::dec << std::endl;
        
        std::cout << "Font data check at ROM offset 0x502C (char '0', first 16 bytes): ";
        for (int i = 0; i < 16; i++) {
            std::cout << std::hex << std::setw(2) << std::setfill('0') << (int)m_rom[0x502C + i] << " ";
        }
        std::cout << std::dec << std::endl;
    }
    
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
            // Log $1FF0 reads to track NMI handler state
            if (wramAddr == 0x1FF0) {
                int fc1ff0r = m_ppu ? m_ppu->getFrameCount() : -1;
                static int ff0ReadLog = 0;
                if (fc1ff0r >= 130 && fc1ff0r <= 500 && ff0ReadLog < 2000) {
                    fprintf(stderr, "[1FF0_R] F:%d val=%02X scan=%d\n",
                            fc1ff0r, m_wram[wramAddr], m_ppu ? m_ppu->getScanline() : -1);
                    ff0ReadLog++;
                }
            }
            return m_wram[wramAddr];
        }
        return 0;
    }
    if (isROMShort) {
        // Low RAM mirror ($0000-$1FFF in banks $00-$3F and $80-$BF)
        if (offset < 0x2000) {
            // Log $1FF0 reads to track NMI handler state
            if (offset == 0x1FF0) {
                int fc1ff0r2 = m_ppu ? m_ppu->getFrameCount() : -1;
                static int ff0ReadLog2 = 0;
                if (fc1ff0r2 >= 130 && fc1ff0r2 <= 500 && ff0ReadLog2 < 2000) {
                    fprintf(stderr, "[1FF0_R] F:%d val=%02X scan=%d\n",
                            fc1ff0r2, m_wram[offset], m_ppu ? m_ppu->getScanline() : -1);
                    ff0ReadLog2++;
                }
            }
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
        
        // WRAM data port read ($2180) — handle BEFORE general PPU routing
        if (offset == 0x2180) {
            return readWRAMPort();
        }

        // I/O registers ($2100-$21FF in banks $00-$3F and $80-$BF) - EXCEPT APU ports and WRAM port
        if (offset >= 0x2100 && offset < 0x2200) {
            // PPU registers - forward to PPU
            if (m_ppu) {
                return m_ppu->readRegister(offset);
            }
            return 0;
        }
        
        // Old-style joypad I/O ($4016-$4017)
        if (offset == 0x4016) {
            uint8_t v = m_input ? m_input->readController1() : 0;
            int fc16 = m_ppu ? m_ppu->getFrameCount() : -1;
            static int joy4016LogCount = 0;
            if (fc16 >= 90 && fc16 <= 200 && joy4016LogCount < 5000) {
                fprintf(stderr, "[MEM] F:%d $4016=%02X scan=%d\n", fc16, v,
                        m_ppu ? m_ppu->getScanline() : -1);
                joy4016LogCount++;
            }
            return v;
        }
        if (offset == 0x4017) {
            uint8_t v2 = m_input ? m_input->readController2() : 0;
            int fc17 = m_ppu ? m_ppu->getFrameCount() : -1;
            static int joy4017LogCount = 0;
            if (fc17 >= 90 && fc17 <= 200 && joy4017LogCount < 500) {
                fprintf(stderr, "[MEM] F:%d $4017=%02X scan=%d\n", fc17, v2,
                        m_ppu ? m_ppu->getScanline() : -1);
                joy4017LogCount++;
            }
            return v2;
        }

        // CPU math registers: read-back ($4202 = WRMPYA, $4214-$4217 = results)
        // $4202: WRMPYA readable (returns last written multiply-A)
        if (offset == 0x4202) {
            return m_mathA;
        }
        // $4214-$4215: RDDIVL/H (quotient or multiply result hi word)
        if (offset == 0x4214) return  m_mathRDDIV        & 0xFF;
        if (offset == 0x4215) return (m_mathRDDIV >> 8)  & 0xFF;
        // $4216-$4217: RDMPYL/H (multiply result lo / divide remainder)
        if (offset == 0x4216) return  m_mathRDMPY        & 0xFF;
        if (offset == 0x4217) return (m_mathRDMPY >> 8)  & 0xFF;

        // CPU I/O registers ($4200-$421F in banks $00-$3F and $80-$BF)
        if (offset >= 0x4200 && offset < 0x4220) {
            // $4212 - HVBJOY: H/V Blank and Joypad Status
            if (offset == 0x4212) {
                uint8_t result = 0;
                int scanline4212 = -1, dot4212 = -1;
                if (m_ppu) {
                    scanline4212 = m_ppu->getScanline();
                    dot4212 = m_ppu->getDot();
                    // Bit 7: VBlank — start scanline depends on $2133 OVERSCAN bit
                    int vblankStart4212 = m_ppu->getVBlankStart();
                    if (scanline4212 >= vblankStart4212) result |= 0x80;
                    // Bit 6: HBlank (dots 274-340 of each scanline)
                    if (dot4212 >= 274) result |= 0x40;
                    // Bit 0: Auto-Joypad busy (set for ~4224 cycles at VBlank start)
                    if (m_autoJoypadBusy) result |= 0x01;
                }
                int fc4212 = m_ppu ? m_ppu->getFrameCount() : -1;
                // Track VBlank→Active transition: log first scan=0 read after VBlank
                {
                    static uint8_t prev4212 = 0xFF;
                    static int transLogCount = 0;
                    if (transLogCount < 20) {
                        // Log when bit7 transitions from 1→0 (VBlank ends, first active read)
                        if ((prev4212 & 0x80) && !(result & 0x80)) {
                            fprintf(stderr, "[4212_TRANS] F:%d VBlank→Active scan=%d dot=%d absPos=%d\n",
                                fc4212, scanline4212, dot4212, scanline4212*341+dot4212);
                            transLogCount++;
                        }
                    }
                    prev4212 = result;
                }
                static int hvb4212LogCount = 0;
                // Log $4212 reads in frames 200-215 for test $6D diagnosis
                if (fc4212 >= 203 && fc4212 <= 215 && hvb4212LogCount < 3000) {
                    static uint8_t last4212 = 0xFF;
                    if (result != last4212 || hvb4212LogCount < 5) {
                        fprintf(stderr, "[MEM] F:%d $4212=%02X scan=%d dot=%d\n",
                                fc4212, result, scanline4212, dot4212);
                        last4212 = result;
                        hvb4212LogCount++;
                    }
                }
                return result;
            }
            // Auto-Joypad data registers ($4218-$421F)
            if (offset >= 0x4218 && offset < 0x4220) {
                int joypadIndex = (offset - 0x4218) / 2;
                bool highByte = (offset & 0x01) != 0;
                uint8_t result = 0;
                if (joypadIndex < 4) {
                    result = highByte ? ((m_joypadData[joypadIndex] >> 8) & 0xFF) : (m_joypadData[joypadIndex] & 0xFF);
                }
                int fc42 = m_ppu ? m_ppu->getFrameCount() : -1;
                static int joyReadLogCount = 0;
                if (((fc42 >= 90 && fc42 <= 200) || (fc42 >= 945 && fc42 <= 1010)) && joyReadLogCount < 5000) {
                    fprintf(stderr, "[JOY_RD] F:%d $%04X=%02X joy1=%04X scan=%d\n",
                            fc42, offset, result, m_joypadData[0], m_ppu ? m_ppu->getScanline() : -1);
                    joyReadLogCount++;
                }
                return result;
            }
            // Forward to PPU (for NMI control and other registers)
            if (m_ppu) {
                return m_ppu->readRegister(offset);
            }
            return 0;
        }

        // DMA registers ($4300-$43FF in banks $00-$3F and $80-$BF) - readable on real hardware
        if (offset >= 0x4300 && offset < 0x4380) {
            uint8_t channel = (offset - 0x4300) / 16;
            uint8_t reg     = (offset - 0x4300) % 16;
            if (channel < 8) {
                switch (reg) {
                    case 0: return m_dmaChannels[channel].control;
                    case 1: return m_dmaChannels[channel].destAddr;
                    case 2: return  m_dmaChannels[channel].sourceAddr        & 0xFF;
                    case 3: return (m_dmaChannels[channel].sourceAddr >> 8)  & 0xFF;
                    case 4: return  m_dmaChannels[channel].sourceBank;
                    case 5: return  m_dmaChannels[channel].size               & 0xFF;
                    case 6: return (m_dmaChannels[channel].size        >> 8)  & 0xFF;
                    case 7: return  m_dmaChannels[channel].hdmaIndirect        & 0xFF;
                    case 8: return (m_dmaChannels[channel].hdmaIndirect >> 8)  & 0xFF;
                    case 9: return  m_dmaChannels[channel].hdmaIndBank;
                    case 10: return m_dmaChannels[channel].hdmaLineCount;
                    // Bytes 11-15: open bus — return 0xFF
                    default: return 0xFF;
                }
            }
            return 0xFF;
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

uint32_t Memory::read24(uint32_t address) {
    uint8_t low = read8(address);
    uint8_t mid = read8(address + 1);
    uint8_t high = read8(address + 2);
    return low | (mid << 8) | (high << 16);
}

void Memory::write8(uint32_t address, uint8_t value) {
    uint8_t bank = (address >> 16) & 0xFF;
    uint16_t offset = address & 0xFFFF;

    // Work RAM ($7E and $7F banks)
    if (bank == 0x7E || bank == 0x7F) {
        uint32_t wramAddr = (address & 0x1FFFF);
        if (wramAddr < m_wram.size()) {
            // Log WRAM writes to page 0 ($0000-$00FF) to catch pass/fail flags
            int fcw = m_ppu ? m_ppu->getFrameCount() : -1;
            if (fcw >= 130 && ((wramAddr >= 0x48 && wramAddr <= 0x49) || (wramAddr >= 0x60 && wramAddr <= 0xAF))) {
                // Only log when value CHANGES to avoid flooding from polling loops
                if (value != m_wram[wramAddr]) {
                    static int wramWriteLog = 0;
                    if (wramWriteLog < 20000) {
                        fprintf(stderr, "[WRAM_W] F:%d $%04X=%02X (was %02X) scan=%d\n",
                                fcw, (uint32_t)wramAddr, value, m_wram[wramAddr],
                                m_ppu ? m_ppu->getScanline() : -1);
                        wramWriteLog++;
                    }
                }
            }
            // Log $1FF0 writes to track when NMI STAT77 gate is set/cleared
            if (wramAddr == 0x1FF0) {
                fprintf(stderr, "[1FF0_W] F:%d val=%02X (was %02X) scan=%d\n",
                        fcw, value, m_wram[wramAddr], m_ppu ? m_ppu->getScanline() : -1);
            }
            // Log writes to $1000-$121F (OAM buffer) and dp$13/$14 for frames 680-1100
            if (fcw >= 680 && fcw <= 1100 && (
                (wramAddr >= 0x1000 && wramAddr < 0x1220) ||
                (wramAddr == 0x0013 || wramAddr == 0x0014)
            )) {
                static int hiOamWriteLog = 0;
                if (hiOamWriteLog < 5000) {
                    hiOamWriteLog++;
                    fprintf(stderr, "[OAMBUF_W] F:%d $%04X=%02X (was %02X) scan=%d\n",
                            fcw, (uint32_t)wramAddr, value, m_wram[wramAddr],
                            m_ppu ? m_ppu->getScanline() : -1);
                }
            }
            m_wram[wramAddr] = value;
        }
        return;
    }

    // Low RAM mirror ($0000-$1FFF in banks $00-$3F and $80-$BF)
    if (offset < 0x2000 && ((bank < 0x40) || (bank >= 0x80 && bank < 0xC0))) {
        // Log WRAM writes to page 0 during test frames
        int fcw2 = m_ppu ? m_ppu->getFrameCount() : -1;
        if (fcw2 >= 130 && ((offset >= 0x48 && offset <= 0x49) || (offset >= 0x60 && offset <= 0xAF))) {
            if (value != m_wram[offset]) {
                static int wramMirrorLog = 0;
                if (wramMirrorLog < 20000) {
                    fprintf(stderr, "[WRAM_W] F:%d $%04X=%02X (was %02X) scan=%d\n",
                            fcw2, (uint32_t)offset, value, m_wram[offset],
                            m_ppu ? m_ppu->getScanline() : -1);
                    wramMirrorLog++;
                }
            }
        }
        // Log $1FF0 writes (mirror path)
        if (offset == 0x1FF0) {
            fprintf(stderr, "[1FF0_W] F:%d val=%02X (was %02X) scan=%d [mirror]\n",
                    fcw2, value, m_wram[offset], m_ppu ? m_ppu->getScanline() : -1);
        }
        // Log writes to $1000-$121F (OAM buffer) and dp$13/$14 for frames 680-1100 (mirror path)
        if (fcw2 >= 680 && fcw2 <= 1100 && (
            (offset >= 0x1000 && offset < 0x1220) ||
            (offset == 0x0013 || offset == 0x0014)
        )) {
            static int hiOamMirrorLog = 0;
            if (hiOamMirrorLog < 5000) {
                hiOamMirrorLog++;
                fprintf(stderr, "[OAMBUF_W] F:%d $%04X=%02X (was %02X) scan=%d [mirror]\n",
                        fcw2, (uint32_t)offset, value, m_wram[offset],
                        m_ppu ? m_ppu->getScanline() : -1);
            }
        }
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
    
    // WRAM data port ($2180-$2183) — handle BEFORE general PPU routing
    if ((offset >= 0x2180 && offset <= 0x2183) && ((bank < 0x40) || (bank >= 0x80 && bank < 0xC0))) {
        switch (offset) {
            case 0x2180: // WMDATA - WRAM data port (read/write, auto-increment)
                writeWRAMPort(value);
                break;
            case 0x2181: // WMADDL - WRAM address low byte (bits 7-0)
                m_wramPortAddr = (m_wramPortAddr & 0x1FF00) | value;
                break;
            case 0x2182: // WMADDM - WRAM address mid byte (bits 15-8)
                m_wramPortAddr = (m_wramPortAddr & 0x100FF) | ((uint32_t)value << 8);
                break;
            case 0x2183: // WMADDH - WRAM address high bit (bit 16 only)
                m_wramPortAddr = (m_wramPortAddr & 0x0FFFF) | (((uint32_t)value & 0x01) << 16);
                break;
        }
        return;
    }

    // I/O registers ($2100-$21FF in banks $00-$3F and $80-$BF)
    if (offset >= 0x2100 && offset < 0x2200 && ((bank < 0x40) || (bank >= 0x80 && bank < 0xC0))) {
        // Log PPU register writes (especially VRAM writes)
        static int ppuWriteCount = 0;
        if (ppuWriteCount < 50 || offset == 0x2118 || offset == 0x2119) {
            // std::cout << "Memory: PPU Write [$" << std::hex << offset << "] = $" << (int)value << std::dec << std::endl;
            if (ppuWriteCount < 50) ppuWriteCount++;
        }
        // PPU registers - forward to PPU
        if (m_ppu) {
            m_ppu->writeRegister(offset, value);
        }
        return;
    }
    
    // Old-style joypad strobe ($4016 write)
    if (offset == 0x4016 && ((bank < 0x40) || (bank >= 0x80 && bank < 0xC0))) {
        if (m_input) {
            m_input->writeStrobe(value);
        }
        return;
    }

    // CPU math registers ($4202-$4206) — write handlers
    // These live in the $4200-$421F range but need separate handling before the
    // generic PPU-forward block. Range is strictly $4202-$4206 only.
    if ((offset >= 0x4202 && offset <= 0x4206) && ((bank < 0x40) || (bank >= 0x80 && bank < 0xC0))) {
        switch (offset) {
            case 0x4202: // WRMPYA — unsigned multiply A operand
                m_mathA = value;
                break;
            case 0x4203: { // WRMPYB — unsigned multiply B; triggers multiply
                // Result = (uint8_t)m_mathA * (uint8_t)value, stored in RDMPY (16-bit)
                // RDDIV holds the product's high word (always 0 for 8x8 = max $FE01)
                uint16_t product = (uint16_t)m_mathA * (uint16_t)value;
                m_mathRDMPY = product;
                m_mathRDDIV = 0; // high word of 8x8 multiply is always 0
                break;
            }
            case 0x4204: // WRDIVL — dividend low byte
                m_mathDividend = (m_mathDividend & 0xFF00) | value;
                break;
            case 0x4205: // WRDIVH — dividend high byte
                m_mathDividend = (m_mathDividend & 0x00FF) | ((uint16_t)value << 8);
                break;
            case 0x4206: { // WRDIVB — divisor; triggers unsigned 16/8 divide
                if (value == 0) {
                    // Divide by zero: quotient=$FFFF, remainder=dividend
                    m_mathRDDIV = 0xFFFF;
                    m_mathRDMPY = m_mathDividend;
                } else {
                    m_mathRDDIV = m_mathDividend / (uint16_t)value;
                    m_mathRDMPY = m_mathDividend % (uint16_t)value;
                }
                break;
            }
            default: break;
        }
        return;
    }

    // CPU I/O registers ($4200-$421F in banks $00-$3F and $80-$BF)
    if (offset >= 0x4200 && offset < 0x4220 && ((bank < 0x40) || (bank >= 0x80 && bank < 0xC0))) {
        // Log CPU I/O register writes
        static int cpuIoWriteCount = 0;
        if (cpuIoWriteCount < 50) {
            // std::cout << "Memory: CPU I/O Write [$" << std::hex << offset << "] = $" << (int)value << std::dec << std::endl;
            cpuIoWriteCount++;
        }

        // Handle specific CPU I/O registers
        if (offset == 0x4200) { // NMITIMEN - NMI Enable and Auto-Joypad
            // Bit 7: NMI Enable (forwarded to PPU)
            // Bit 0: Auto-Joypad Enable
            m_autoJoypadEnabled = (value & 0x01) != 0;
            if (m_ppu) {
                m_ppu->writeRegister(offset, value);
            }
            static bool warnedAutoJoypad = false;
            if (!warnedAutoJoypad && m_autoJoypadEnabled) {
                std::cout << "[INFO] Auto-Joypad enabled ($4200 bit 0) - Controller data is automatically stored in $4218-$421F during VBlank." << std::endl;
                warnedAutoJoypad = true;
            }
        } else if (offset == 0x420B) { // MDMAEN - DMA Enable
            int fc2 = m_ppu ? m_ppu->getFrameCount() : -1;
            // Log DMA trigger with channel parameters to stderr
            if (fc2 >= 130 && fc2 <= 210) {
                for (int i = 0; i < 8; i++) {
                    if (value & (1 << i)) {
                        DMAChannel& d = m_dmaChannels[i];
                        fprintf(stderr, "[DMA] F:%d Ch%d ctrl=%02X dst=$21%02X src=%02X:%04X size=%04X mode=%d\n",
                                fc2, i, d.control, d.destAddr,
                                d.sourceBank, d.sourceAddr, d.size,
                                d.control & 0x07);
                    }
                }
            }
            // Trigger DMA channels based on bit flags
            for (int i = 0; i < 8; i++) {
                if (value & (1 << i)) {
                    performDMA(i); // Execute DMA immediately
                }
            }
        } else if (offset == 0x4201) { // WRIO - I/O Port Write (also triggers H/V latch on bit7 HIGH→LOW)
            // On real SNES: bit7 falling edge (1→0) triggers H/V counter latch, same as reading $2137
            {
                int fc4201 = m_ppu ? m_ppu->getFrameCount() : -1;
                static int wrio_log = 0;
                if (wrio_log < 200) {
                    fprintf(stderr, "[WRIO_W] F:%d $4201=$%02X prev=$%02X scan=%d\n",
                        fc4201, value, m_wrioPrev, m_ppu ? m_ppu->getScanline() : -1);
                    wrio_log++;
                }
            }
            if ((m_wrioPrev & 0x80) && !(value & 0x80)) {
                if (m_ppu) {
                    m_ppu->triggerHVLatch();
                    int fc4201 = m_ppu->getFrameCount();
                    int dot4201 = m_ppu->getDot(), scan4201 = m_ppu->getScanline();
                    fprintf(stderr, "[WRIO] F:%d $4201=$%02X latch! H=%d V=%d absPos=%d\n",
                        fc4201, value, dot4201, scan4201, scan4201*341+dot4201);
                }
            }
            m_wrioPrev = value;
            m_ioRegisters[offset - 0x4200] = value;
        } else if (offset == 0x420C) { // HDMAEN - HDMA Enable
            m_hdmaEnable = value;
            m_ioRegisters[offset - 0x4200] = value;
            int fc420C = m_ppu ? m_ppu->getFrameCount() : -1;
            if (fc420C >= 130 && fc420C <= 200) {
                fprintf(stderr, "[HDMA] F:%d HDMAEN=$%02X scan=%d\n",
                        fc420C, value, m_ppu ? m_ppu->getScanline() : -1);
                if (value != 0) {
                    for (int i = 0; i < 8; i++) {
                        if (value & (1 << i)) {
                            DMAChannel& d = m_dmaChannels[i];
                            fprintf(stderr, "  Ch%d ctrl=%02X dst=$21%02X src=%02X:%04X\n",
                                    i, d.control, d.destAddr, d.sourceBank, d.sourceAddr);
                        }
                    }
                }
            }
            if (value != 0) {
                initHDMA();
            }
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
            // std::cout << "Memory: DMA Register Write [$" << std::hex << offset << "] = $" << (int)value << std::dec << std::endl;
            dmaRegWriteCount++;
        }
        
        // Handle DMA register writes (each channel uses 16 bytes: $43x0-$43xF)
        uint8_t channel = (offset - 0x4300) / 16;
        uint8_t reg = (offset - 0x4300) % 16;
        
        if (channel < 8) {
            switch (reg) {
                case 0: m_dmaChannels[channel].control    = value; break;
                case 1: m_dmaChannels[channel].destAddr   = value; break;
                case 2: m_dmaChannels[channel].sourceAddr = (m_dmaChannels[channel].sourceAddr & 0xFF00) | value; break;
                case 3: m_dmaChannels[channel].sourceAddr = (m_dmaChannels[channel].sourceAddr & 0x00FF) | (value << 8); break;
                case 4: m_dmaChannels[channel].sourceBank = value; break;
                case 5: m_dmaChannels[channel].size       = (m_dmaChannels[channel].size & 0xFF00) | value; break;
                case 6: m_dmaChannels[channel].size       = (m_dmaChannels[channel].size & 0x00FF) | (value << 8); break;
                case 7: m_dmaChannels[channel].hdmaIndirect = (m_dmaChannels[channel].hdmaIndirect & 0xFF00) | value; break;
                case 8: m_dmaChannels[channel].hdmaIndirect = (m_dmaChannels[channel].hdmaIndirect & 0x00FF) | (value << 8); break;
                case 9: m_dmaChannels[channel].hdmaIndBank  = value; break;
                case 10: m_dmaChannels[channel].hdmaLineCount = value; break;
                // Bytes 11-15: unused — writes ignored, reads return 0xFF
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

// ---------------------------------------------------------------------------
// WRAM data port ($2180-$2183) helpers
// ---------------------------------------------------------------------------
uint8_t Memory::readWRAMPort() {
    uint32_t addr = m_wramPortAddr & 0x1FFFF;
    uint8_t val = (addr < m_wram.size()) ? m_wram[addr] : 0;
    m_wramPortAddr = (m_wramPortAddr + 1) & 0x1FFFF;
    return val;
}

void Memory::writeWRAMPort(uint8_t value) {
    uint32_t addr = m_wramPortAddr & 0x1FFFF;
    if (addr < m_wram.size()) {
        int fcwp = m_ppu ? m_ppu->getFrameCount() : -1;
        if (fcwp >= 130 && fcwp <= 240 && addr >= 0x60 && addr <= 0xAF) {
            static int wramPortLog = 0;
            if (wramPortLog < 3000) {
                fprintf(stderr, "[WRAM_W] F:%d $%04X=%02X (was %02X) scan=%d [port]\n",
                        fcwp, addr, value, m_wram[addr],
                        m_ppu ? m_ppu->getScanline() : -1);
                wramPortLog++;
            }
        }
        m_wram[addr] = value;
    }
    m_wramPortAddr = (m_wramPortAddr + 1) & 0x1FFFF;
}

void Memory::setWRAMAddress(uint32_t addr) {
    m_wramPortAddr = addr & 0x1FFFF;
}

void Memory::performDMA(uint8_t channel) {
    if (channel >= 8) return;
    
    DMAChannel& dma = m_dmaChannels[channel];
    
    // Get transfer direction: Bit 7 (0 = CPU to PPU, 1 = PPU to CPU)
    bool toPPU = !(dma.control & 0x80);
    
    // Get transfer mode (bits 2-0)
    uint8_t mode = dma.control & 0x07;
    
    // Warn about simplified DMA modes
    static bool warnedDMAModes = false;
    if (!warnedDMAModes && mode >= 2) {
        std::cout << "[WARN] DMA Transfer Modes 2-7 are simplified; precise transfer patterns may not match hardware." << std::endl;
        warnedDMAModes = true;
    }
    
    // Calculate source address
    uint32_t sourceAddr = (dma.sourceBank << 16) | dma.sourceAddr;
    
    // Calculate destination address
    // Can be PPU register (0x2100-0x21FF) or APU port (0x2140-0x2143)
    uint16_t destAddr = 0x2100 + dma.destAddr;
    bool isAPUPort = (destAddr >= 0x2140 && destAddr < 0x2144);
    
    static int dmaExecLog = 0;
    if (dmaExecLog < 20) {
        std::cout << "  DMA Ch" << (int)channel << ": "
            << (toPPU ? "CPU->" : "") << (isAPUPort ? "APU" : "PPU")
            << " Mode=" << (int)mode << " Size=" << dma.size
            << " Src=0x" << std::hex << sourceAddr
            << " Dst=0x" << destAddr << std::dec << std::endl;
        dmaExecLog++;
    }

    // Diagnostic: log DMA to VRAM ($2118/$2119) with frame and VRAM address
    bool isVRAMDest = (destAddr == 0x2118 || destAddr == 0x2119);
    if (isVRAMDest) {
        static int dmaVRAMTotal = 0;
        dmaVRAMTotal++;
        int fc = m_ppu ? m_ppu->getFrameCount() : -1;
        uint16_t vramWordAddr = m_ppu ? m_ppu->getVRAMAddress() : 0xFFFF;
        uint32_t logSize = (dma.size == 0) ? 65536 : dma.size;
        fprintf(stderr, "[DIAG] DMA->VRAM #%d F:%d Ch%d ctrl=$%02X Src=$%06X Dst=$%04X VRAMword=%04X Size=%u Mode=%d\n",
            dmaVRAMTotal, fc, (int)channel, dma.control, sourceAddr, destAddr, vramWordAddr, logSize, (int)mode);
        // Log first 16 source bytes
        fprintf(stderr, "[DIAG]   SrcBytes:");
        uint32_t logSrc = sourceAddr;
        for (int li = 0; li < (int)std::min(logSize, 16u); li++) {
            fprintf(stderr, " %02X", read8(logSrc));
            logSrc++;
        }
        fprintf(stderr, "\n");
        // Also log WRAM[$0040-$004F] for sub-test context
        fprintf(stderr, "[DIAG]   WRAM[$0040-$004F]:");
        for (int li = 0; li < 16; li++) fprintf(stderr, " %02X", m_wram[0x0040+li]);
        fprintf(stderr, "\n");
    }

    // SNES DMA: size=0 means 65536 bytes
    uint32_t transferSize = (dma.size == 0) ? 65536 : dma.size;

    // Source address adjustment mode
    // Per SNES hardware (bsnes-confirmed):
    // bit3=1: FIXED address (source does not change)
    // bit4=1: REVERSE (decrement source) — only when bit3=0
    // else: INCREMENT source
    bool addrFixed  = (dma.control >> 3) & 1;
    bool addrDecr   = (dma.control >> 4) & 1;
    // srcAdj: 0=increment, 2=decrement, 1=fixed (to match existing if-chain)
    uint8_t srcAdj = addrFixed ? 1 : (addrDecr ? 2 : 0);

    // Log actual transfer size for diagnostics
    {
        int fc3 = m_ppu ? m_ppu->getFrameCount() : -1;
        if (fc3 >= 130 && fc3 <= 200) {
            fprintf(stderr, "[DMA_EXEC] F:%d Ch%d toPPU=%d size=%u srcAdj=%d destAddr=$%04X\n",
                    fc3, channel, (int)toPPU, transferSize, (int)srcAdj, destAddr);
        }
    }

    // Perform transfer based on mode
    // Helper: write one byte to B-bus destination register
    // Handles WRAM port ($2180), APU ports ($2140-$2143), and PPU registers
    bool isWRAMPort = (destAddr == 0x2180);

    // Diagnostic: log OAM DMA for selected frames
    bool isOAMDest = (destAddr == 0x2104);
    if (isOAMDest) {
        int fcOam = m_ppu ? m_ppu->getFrameCount() : -1;
        bool logThis = (fcOam <= 50) || (fcOam >= 680 && fcOam <= 695);
        static int oamDmaCount = 0;
        if (logThis && oamDmaCount < 300) {
            fprintf(stderr, "[OAM_DMA] F:%d size=%u src=%06X oamAddr=%u spr0: %02X,%02X,%02X,%02X spr1: %02X,%02X,%02X,%02X\n",
                fcOam, transferSize, sourceAddr, m_ppu ? m_ppu->getOAMAddress() : 0xFFFF,
                read8(sourceAddr+0), read8(sourceAddr+1), read8(sourceAddr+2), read8(sourceAddr+3),
                read8(sourceAddr+4), read8(sourceAddr+5), read8(sourceAddr+6), read8(sourceAddr+7));
            oamDmaCount++;
        }
    }

    if (toPPU) {
        for (uint32_t i = 0; i < transferSize; i++) {
            uint8_t data = read8(sourceAddr);
            uint16_t targetReg;
            switch (mode) {
                case 0: targetReg = destAddr; break;
                case 1: targetReg = destAddr + (i & 1); break;
                case 2: targetReg = destAddr; break;
                case 3: targetReg = destAddr + ((i >> 1) & 1); break;
                case 4: targetReg = destAddr + (i & 3); break;
                default: targetReg = destAddr; break;
            }
            if (isWRAMPort) {
                // DMA to WRAM port: write via port (auto-increments m_wramPortAddr)
                // Note: targetReg may be 0x2180 only (mode 0); other modes unlikely for WRAM
                writeWRAMPort(data);
            } else if (isAPUPort && m_apu) {
                uint8_t port = targetReg - 0x2140;
                if (port < 4) m_apu->writePort(port, data);
            } else if (m_ppu) {
                m_ppu->writeRegister(targetReg, data);
            }
            // Adjust source address
            if (srcAdj == 0) sourceAddr++;
            else if (srcAdj == 2) sourceAddr--;
        }
    } else {
        // PPU-to-CPU DMA: read from B-Bus (PPU register) and write to A-Bus (RAM)
        for (uint32_t i = 0; i < transferSize; i++) {
            uint16_t srcReg;
            switch (mode) {
                case 0: srcReg = destAddr; break;
                case 1: srcReg = destAddr + (i & 1); break;
                case 2: srcReg = destAddr; break;
                case 3: srcReg = destAddr + ((i >> 1) & 1); break;
                case 4: srcReg = destAddr + (i & 3); break;
                default: srcReg = destAddr; break;
            }
            uint8_t data = 0;
            if (isWRAMPort) {
                data = readWRAMPort();
            } else if (m_ppu) {
                data = m_ppu->readRegister(srcReg);
            }
            write8(sourceAddr, data);
            // Adjust destination (A-Bus) address
            if (srcAdj == 0) sourceAddr++;
            else if (srcAdj == 2) sourceAddr--;
        }
    }

    // After transfer: update source address and clear size
    dma.sourceAddr = sourceAddr & 0xFFFF;
    dma.sourceBank = (sourceAddr >> 16) & 0xFF;
    dma.size = 0;
    // DMA consumes ~8 master cycles per byte transferred
    m_dmaCyclesPending += transferSize * 8;
}

// ---------------------------------------------------------------------------
// HDMA – H-Blank DMA
// ---------------------------------------------------------------------------

void Memory::initHDMA() {
    static bool warnedInit = false;
    if (!warnedInit) {
        std::cout << "[HDMA] initHDMA() called – initialising enabled channels." << std::endl;
        warnedInit = true;
    }

    for (int ch = 0; ch < 8; ch++) {
        HDMAChannel& hdma = m_hdmaChannels[ch];
        hdma.terminated  = true;   // default: inactive
        hdma.lineCounter = 0;
        hdma.repeatMode  = false;
        hdma.doTransfer  = false;

        if (!(m_hdmaEnable & (1 << ch))) continue;

        DMAChannel& dma = m_dmaChannels[ch];
        // Table start address: bank from sourceBank ($43x4), word addr from sourceAddr ($43x2-3)
        hdma.tableAddr   = ((uint32_t)dma.sourceBank << 16) | dma.sourceAddr;
        hdma.terminated  = false;

        // Read the first header byte so the first performHDMA call is ready
        uint8_t header = read8(hdma.tableAddr);
        if (header == 0x00) {
            hdma.terminated = true;
            continue;
        }
        // Do NOT advance tableAddr here; performHDMA will consume the header on
        // the first scanline (lineCounter == 0).
    }
}

// Returns the number of data bytes per transfer for a given DMAP mode.
static int hdmaDataBytes(uint8_t mode) {
    switch (mode & 0x07) {
        case 0: return 1;  // 1 byte  → destReg
        case 1: return 2;  // 2 bytes → destReg, destReg+1
        case 2: return 2;  // 2 bytes → destReg, destReg   (same reg twice)
        case 3: return 4;  // 4 bytes → destReg, destReg+1, destReg, destReg+1
        case 4: return 4;  // 4 bytes → destReg, +1, +2, +3
        case 5: return 4;  // same as mode 1 but repeated (alias)
        default: return 1;
    }
}

void Memory::performHDMA(int scanline) {
    if (m_hdmaEnable == 0) return;
    // HDMA only fires during active display (scanlines 0-223)
    if (scanline < 0 || scanline >= 224) return;

    for (int ch = 0; ch < 8; ch++) {
        if (!(m_hdmaEnable & (1 << ch))) continue;

        HDMAChannel& hdma = m_hdmaChannels[ch];
        if (hdma.terminated) continue;

        DMAChannel& dma = m_dmaChannels[ch];
        uint8_t  mode    = dma.control & 0x07;
        uint16_t destReg = 0x2100 + dma.destAddr;
        int      nBytes  = hdmaDataBytes(mode);

        // ---- Step 1: if line counter exhausted, read a new table header ----
        if (hdma.lineCounter == 0) {
            uint8_t header = read8(hdma.tableAddr++);
            if (header == 0x00) {
                hdma.terminated = true;
                continue;
            }
            hdma.lineCounter = header & 0x7F;
            hdma.repeatMode  = (header & 0x80) != 0;
            hdma.doTransfer  = true;

            // For non-repeat (write-once) mode the data pointer sits right
            // after the header.  We keep tableAddr pointing at that data and
            // advance it ONLY when we actually consume an entry (see Step 3).
        }

        // ---- Step 2: perform the data transfer if flagged ----
        if (hdma.doTransfer) {
            // Read from the current data position (tableAddr already past header)
            uint32_t dataAddr = hdma.tableAddr;

            // Helper: write one byte to B-bus register (handles WRAM port and APU)
            auto writeB = [&](uint16_t reg, uint8_t val) {
                if (reg == 0x2180) {
                    // WRAM data port — use port helper (auto-increments address)
                    writeWRAMPort(val);
                } else if (reg >= 0x2140 && reg <= 0x2143 && m_apu) {
                    m_apu->writePort(reg - 0x2140, val);
                } else if (m_ppu) {
                    m_ppu->writeRegister(reg, val);
                }
            };

            switch (mode & 0x07) {
                case 0: // 1 byte → destReg
                    writeB(destReg,   read8(dataAddr));
                    break;
                case 1: // 2 bytes → destReg, destReg+1
                    writeB(destReg,   read8(dataAddr));
                    writeB(destReg+1, read8(dataAddr+1));
                    break;
                case 2: // 2 bytes → destReg, destReg (same reg twice)
                    writeB(destReg, read8(dataAddr));
                    writeB(destReg, read8(dataAddr+1));
                    break;
                case 3: // 4 bytes → destReg, destReg+1, destReg, destReg+1
                    writeB(destReg,   read8(dataAddr));
                    writeB(destReg+1, read8(dataAddr+1));
                    writeB(destReg,   read8(dataAddr+2));
                    writeB(destReg+1, read8(dataAddr+3));
                    break;
                case 4: // 4 bytes → destReg, +1, +2, +3
                    for (int b = 0; b < 4; b++)
                        writeB(destReg + b, read8(dataAddr + b));
                    break;
                case 5: // 4 bytes → destReg, destReg+1 (repeated, like mode 1 x2)
                    writeB(destReg,   read8(dataAddr));
                    writeB(destReg+1, read8(dataAddr+1));
                    writeB(destReg,   read8(dataAddr+2));
                    writeB(destReg+1, read8(dataAddr+3));
                    break;
                default:
                    writeB(destReg, read8(dataAddr));
                    break;
            }

            // Non-repeat: advance the data pointer after each scanline's data
            if (!hdma.repeatMode) {
                hdma.tableAddr += (uint32_t)nBytes;
            }
        }

        // ---- Step 3: decrement line counter ----
        hdma.lineCounter--;

        // When a repeat-mode group is exhausted, advance past the shared data block
        if (hdma.lineCounter == 0 && hdma.repeatMode) {
            hdma.tableAddr += (uint32_t)nBytes;
        }

        // doTransfer stays true every scanline for repeat mode;
        // for non-repeat it is also true every scanline (each line has its own data).
        // (The distinction is handled by the tableAddr advance logic above.)
    }
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
                uint32_t romAddr = ((bank - 0x00) * 0x8000) + (offset - 0x8000);
                if (address >= 0xCD2C && address <= 0xCD3C) {
                    std::ostringstream oss2;
                    oss2 << "=== DEBUG LoROM: address=0x" << std::hex << address 
                         << " -> romAddr=0x" << romAddr 
                         << " m_rom.size()=" << std::dec << m_rom.size()
                         << " m_rom[romAddr]=" << std::hex << (int)m_rom[romAddr] << " ===";
                    Logger::getInstance().logCPU(oss2.str());
                    Logger::getInstance().flush();
                }
                return romAddr;
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
            // Banks $00-$3F, offset $8000-$FFFF mirror ROM
            else if (bank <= 0x3F && offset >= 0x8000) {
                return (bank * 0x10000) + offset;
            }
            // Banks $40-$7D map full 64KB to ROM
            else if (bank >= 0x40 && bank <= 0x7D) {
                return (bank * 0x10000) + offset;
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

void Memory::performAutoJoypadRead() {
    if (!m_autoJoypadEnabled || !m_input) {
        return;
    }
    // Set busy flag; hardware takes ~4224 master cycles (~2 scanlines at 1364/scanline)
    // We approximate as busy for ~2 scanline transitions
    m_autoJoypadBusy = true;
    m_autoJoypadBusyCountdown = 2;

    // Hardware behavior: the auto-joypad process pulses the /LATCH pin at VBlank start,
    // which sets the H/V counter latch flag (bit7 of $213F / STAT78), exactly as if
    // the CPU had read $2137 (SLHV). Test 18 of the test ROM checks for this.
    if (m_ppu) {
        m_ppu->triggerHVLatch();
    }

    // Auto-Joypad: Read controller data and store in $4218-$421F
    // SNES hardware bit layout for $4218/$4219 (JOY1):
    //   $4219 (high byte): B Y Select Start Up Down Left Right
    //   $4218 (low byte):  A X L R 0 0 0 0
    // Serial read order: B,Y,Select,Start,Up,Down,Left,Right,A,X,L,R,0,0,0,0
    // So bit 15=B, bit 14=Y, ..., bit 8=Right, bit 7=A, ..., bit 4=R
    if (m_input) {
        // Controller 1: read serial bits in SNES order (B first = bit 15)
        uint16_t joy1 = 0;
        m_input->writeStrobe(1);
        m_input->writeStrobe(0);
        for (int i = 0; i < 16; i++) {
            uint8_t bit = m_input->readController1();
            joy1 |= (bit << (15 - i));  // First serial bit = bit 15 (B)
        }
        m_joypadData[0] = joy1;
        if (joy1 != 0) {
            static int joyLogCount = 0;
            if (joyLogCount < 100) {
                std::cout << "[JOYPAD] joy1=0x" << std::hex << joy1 << std::dec << std::endl;
                joyLogCount++;
            }
        }

        // Controller 2: same layout
        uint16_t joy2 = 0;
        m_input->writeStrobe(1);
        m_input->writeStrobe(0);
        for (int i = 0; i < 16; i++) {
            uint8_t bit = m_input->readController2();
            joy2 |= (bit << (15 - i));
        }
        m_joypadData[1] = joy2;

        // Controller 3-4: Multi-tap not implemented (set to 0)
        m_joypadData[2] = 0;
        m_joypadData[3] = 0;
    }
}

void Memory::tickAutoJoypadBusy() {
    if (m_autoJoypadBusy && m_autoJoypadBusyCountdown > 0) {
        m_autoJoypadBusyCountdown--;
        if (m_autoJoypadBusyCountdown == 0) {
            m_autoJoypadBusy = false;
        }
    }
}
