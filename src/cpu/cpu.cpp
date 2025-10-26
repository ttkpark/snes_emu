#include "cpu.h"
#include "../memory/memory.h"
#include "../ppu/ppu.h"
#include "../debug/logger.h"
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <SDL.h>

#define m_sp_address ((m_pbr<<16)|m_sp)
#define m_address ((m_pbr<<16)|m_pc)
#define m_address_plus_1 ((m_pbr<<16)|(m_pc++))

CPU::CPU(Memory* memory) 
    : m_memory(memory)
    , m_ppu(nullptr)
    , m_pc(0)
    , m_a(0)
    , m_x(0)
    , m_y(0)
    , m_sp(0x01FF)  // Stack pointer in page 1 (0x01FF = 0x0100 + 0xFF)
    , m_p(0x34)  // P = 0x34: M=1 (8-bit A), X=1 (8-bit X/Y), D=0, I=1, Z=0, C=0
    , m_cycles(0)
    , m_nmiPending(false)
    , m_nmiEnabled(true)
    , m_modeM(true)   // Start in 8-bit mode
    , m_modeX(true)   // Start in 8-bit mode
    , m_emulationMode(true)  // Start in emulation mode
    , m_d(0x0000)     // Direct Page register
    , m_dbr(0x00)     // Data Bank register
    , m_pbr(0x00)     // Program Bank register
    , m_suppressLogging(false) {
}

void CPU::reset() {
    // Read reset vector based on CPU mode:
    // - Emulation mode: 0xFFFC-0xFFFD
    // - Native mode: 0xFFEC-0xFFED (but SNES always resets in emulation mode)
    uint16_t resetVector = m_memory->read16(0xFFFC);
    m_pc = resetVector;  // Use vector value directly
    m_sp = 0x01FF;  // Stack pointer in page 1 (0x0100-0x01FF)
    m_p = 0xB4;   // P = 0xB4: M=1, X=1, D=0, I=1, E=1 (Emulation mode)
    m_emulationMode = true;
    m_modeM = true;   // 8-bit A
    m_modeX = true;   // 8-bit X/Y
    m_d = 0x0000;     // Direct Page register
    m_dbr = 0x00;     // Data Bank register
    m_pbr = 0x00;     // Program Bank register
    
    std::cout << "CPU Reset: Vector=0x" << std::hex << resetVector 
              << ", PC=0x" << m_pc 
              << ", PBR=0x" << (int)m_pbr << std::dec << std::endl;
}

void CPU::step() {
    // Check for pending NMI (Non-Maskable Interrupt)
    static int nmiLogCount = 0;
    if (m_nmiPending && m_nmiEnabled) {
        if (true) {
            std::ostringstream oss;
            oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') << m_cycles << "] "
                << "*** NMI TRIGGERED *** PC:0x" << std::hex << std::setw(6) << std::setfill('0') << m_pc
                << " -> Vector:0x" << (m_emulationMode ? 0xFFFA : 0xFFEA)
                << "($" << m_memory->read16(m_emulationMode ? 0xFFFA : 0xFFEA) << ")";
            Logger::getInstance().logCPU(oss.str());
            nmiLogCount++;
        }
        handleNMI();
        m_nmiPending = false;
    }
    
    // Check for fail condition (entering fail routine at 0x008242)
    if (m_address == 0x008242) {
        Logger::getInstance().logCPU("=== TEST FAILED - ENTERING fail routine ===");
        Logger::getInstance().logCPU("Memory Dump (0x000000 - 0x000040):");
        
        std::ostringstream memDump;
        for (uint32_t addr = 0x000000; addr <= 0x000040; addr += 16) {
            memDump.str("");
            memDump << std::hex << std::uppercase << std::setfill('0');
            memDump << std::setw(6) << addr << ": ";
            
            for (int i = 0; i < 16; i++) {
                if (addr + i <= 0x000040) {
                    uint8_t byte = m_memory->read8(addr + i);
                    memDump << std::setw(2) << (int)byte << " ";
                }
            }
            Logger::getInstance().logCPU(memDump.str());
        }
        
        Logger::getInstance().logCPU("\n=== Direct Page $FF90 - $FFAF (pointer area) ===");
        memDump.str("");
        for (int j = 0; j < 2; j++) {  
            memDump << std::hex << std::uppercase << std::setfill('0');
            memDump << "00" << std::setw(4) << 0xFF90 + j*16 << ": ";
            for (int i = 0; i < 16; i++) {  
                uint8_t byte = m_memory->read8(0xFF90 + i+j*16);
                memDump << std::setw(2) << (int)byte << " ";
            }
            memDump << std::endl;
        }
        Logger::getInstance().logCPU(memDump.str());
        
        Logger::getInstance().logCPU("\n=== Memory $CCE0 - $CD1F (near 0xCCF7, 0xCD10) ===");
        memDump.str("");
        for (int j = 0; j < 4; j++) {  
            memDump << std::hex << std::uppercase << std::setfill('0');
            memDump << "00" << std::setw(4) << 0xCCE0 + j*16 << ": ";
            for (int i = 0; i < 16; i++) {  
                uint8_t byte = m_memory->read8(0xCCE0 + i+j*16);
                memDump << std::setw(2) << (int)byte << " ";
            }
            memDump << std::endl;
        }
        Logger::getInstance().logCPU(memDump.str());
        
        Logger::getInstance().logCPU("\n=== Memory $7F:1210 - $7F:12FF (test data) ===");
        memDump.str("");
        for (int j = 0; j < 15; j++) {  
            memDump << std::hex << std::uppercase << std::setfill('0');
            memDump << "7F" << std::setw(4) << (0x1210 + j*16) << ": ";
            for (int i = 0; i < 16; i++) {  
                uint8_t byte = m_memory->read8(0x7F1210 + i+j*16);
                memDump << std::setw(2) << (int)byte << " ";
            }
            memDump << std::endl;
        }
        Logger::getInstance().logCPU(memDump.str());
        
        Logger::getInstance().logCPU("\n=== VRAM Dump (tile map area 0x0000 - 0x01FF) ===");
        if (m_ppu) {
            for (uint32_t addr = 0x0000; addr <= 0x01FF; addr += 16) {
                memDump.str("");
                memDump << std::hex << std::uppercase << std::setfill('0');
                memDump << "VRAM " << std::setw(4) << addr << ": ";
                
                for (int i = 0; i < 16; i++) {
                    if (addr + i <= 0x01FF) {
                        uint8_t vramData = m_ppu->readVRAM(addr + i);
                        memDump << std::setw(2) << (int)vramData << " ";
                    }
                }
                Logger::getInstance().logCPU(memDump.str());
            }
            
            Logger::getInstance().logCPU("\n=== VRAM Font Data (byte address 0x8000 - 0x80FF) ===");
            for (uint32_t addr = 0x8000; addr <= 0x80FF; addr += 16) {
                memDump.str("");
                memDump << std::hex << std::uppercase << std::setfill('0');
                memDump << "VRAM " << std::setw(4) << addr << ": ";
                
                for (int i = 0; i < 16; i++) {
                    if (addr + i <= 0x80FF) {
                        uint8_t vramData = m_ppu->readVRAM(addr + i);
                        memDump << std::setw(2) << (int)vramData << " ";
                    }
                }
                Logger::getInstance().logCPU(memDump.str());
            }
            
            Logger::getInstance().logCPU("\n=== VRAM Font Data (char '0' at byte address 0x8600 - 0x860F) ===");
            memDump.str("");
            memDump << std::hex << std::uppercase << std::setfill('0');
            memDump << "VRAM 8600: ";
            for (int i = 0; i < 16; i++) {
                uint8_t vramData = m_ppu->readVRAM(0x8600 + i);
                memDump << std::setw(2) << (int)vramData << " ";
            }
            Logger::getInstance().logCPU(memDump.str());
        } else {
            Logger::getInstance().logCPU("PPU not available for VRAM dump");
        }
        
        Logger::getInstance().logCPU("=== TEST FAILED - Continuing execution ===");
        Logger::getInstance().flush();
        // Continue execution without pausing

        SDL_Quit();
        exit(0);
    }
    
    // Log CPU execution
    static int instructionCount = 0;
    static int frameCount = 0;
    
    if (instructionCount < 500000) {  // Trace first 50000 instructions to see forced blank setup
        // Suppress logging for wait_for_vblank routine (PC 0x8260-0x8265)
        bool shouldLog = !(m_address >= 0x8260 && m_address <= 0x8265);
        
        if (instructionCount % 10000 == 0) {
            shouldLog = true;
            stackTrace();
        }
            
        uint8_t opcode = m_memory->read8(m_address);
        
        // Get opcode name and byte length
        const char* opcodeName = "???";
        int byteLength = 1; // Default: opcode only
        
        // Complete 65816 instruction decode table
        switch (opcode) {
            // Interrupts and Exceptions
            case 0x00: opcodeName = "BRK"; byteLength = 2; break;
            case 0x02: opcodeName = "COP"; byteLength = 2; break;
            case 0x40: opcodeName = "RTI"; break;
            
            // Stack Operations
            case 0x08: opcodeName = "PHP"; break;
            case 0x28: opcodeName = "PLP"; break;
            case 0x48: opcodeName = "PHA"; break;
            case 0x68: opcodeName = "PLA"; break;
            case 0x5A: opcodeName = "PHY"; break;
            case 0x7A: opcodeName = "PLY"; break;
            case 0x8B: opcodeName = "PHB"; break;
            case 0xAB: opcodeName = "PLB"; break;
            case 0xDA: opcodeName = "PHX"; break;
            case 0xFA: opcodeName = "PLX"; break;
            case 0xF4: opcodeName = "PEA"; byteLength = 3; break;
            case 0x0B: opcodeName = "PHD"; break;
            case 0x2B: opcodeName = "PLD"; break;
            
            // Jumps and Subroutines
            case 0x20: opcodeName = "JSR abs"; byteLength = 3; break;
            case 0x22: opcodeName = "JSL"; byteLength = 4; break;
            case 0x4C: opcodeName = "JMP"; byteLength = 3; break;
            case 0x5C: opcodeName = "JMP Long"; byteLength = 4; break;
            case 0x6C: opcodeName = "JMP (abs)"; byteLength = 3; break;
            case 0x7C: opcodeName = "JMP (abs,X)"; byteLength = 3; break;
            case 0xDC: opcodeName = "JMP [abs]"; byteLength = 3; break;
            case 0x60: opcodeName = "RTS"; break;
            case 0x6B: opcodeName = "RTL"; break;
            case 0x4B: opcodeName = "PHK"; break;
            
            // Branches
            case 0x10: opcodeName = "BPL"; byteLength = 2; break;
            case 0x30: opcodeName = "BMI"; byteLength = 2; break;
            case 0x50: opcodeName = "BVC"; byteLength = 2; break;
            case 0x70: opcodeName = "BVS"; byteLength = 2; break;
            case 0x80: opcodeName = "BRA"; byteLength = 2; break;
            case 0x82: opcodeName = "BRL"; byteLength = 3; break;
            case 0x90: opcodeName = "BCC"; byteLength = 2; break;
            case 0xB0: opcodeName = "BCS"; byteLength = 2; break;
            case 0xD0: opcodeName = "BNE"; byteLength = 2; break;
            case 0xF0: opcodeName = "BEQ"; byteLength = 2; break;
            
            // Flag Operations
            case 0x18: opcodeName = "CLC"; break;
            case 0x38: opcodeName = "SEC"; break;
            case 0x58: opcodeName = "CLI"; break;
            case 0x78: opcodeName = "SEI"; break;
            case 0xB8: opcodeName = "CLV"; break;
            case 0xD8: opcodeName = "CLD"; break;
            case 0xF8: opcodeName = "SED"; break;
            case 0xFB: opcodeName = "XCE"; break;
            
            // Processor Status
            case 0xC2: opcodeName = "REP"; byteLength = 2; break;
            case 0xE2: opcodeName = "SEP"; byteLength = 2; break;
            case 0xDB: opcodeName = "STP"; break;
            case 0xCB: opcodeName = "WAI"; break;
            
            // Accumulator Operations
            case 0x0A: opcodeName = "ASL A"; break;
            case 0x2A: opcodeName = "ROL A"; break;
            case 0x4A: opcodeName = "LSR A"; break;
            case 0x6A: opcodeName = "ROR A"; break;
            case 0x1A: opcodeName = "INC A"; break;
            case 0x3A: opcodeName = "DEC A"; break;
            case 0xEB: opcodeName = "XBA"; break;
            
            // Register Transfers
            case 0x8A: opcodeName = "TXA"; break;
            case 0x9A: opcodeName = "TXS"; break;
            case 0x98: opcodeName = "TYA"; break;
            case 0xA8: opcodeName = "TAY"; break;
            case 0xAA: opcodeName = "TAX"; break;
            case 0xBA: opcodeName = "TSX"; break;
            case 0x1B: opcodeName = "TCS"; break; 
            case 0x3B: opcodeName = "TSC"; break;
            case 0x5B: opcodeName = "TCD"; break;
            case 0x7B: opcodeName = "TDC"; break;
            
            // Load Operations
            case 0xA9: opcodeName = "LDA #"; byteLength = m_modeM ? 2 : 3; break;
            case 0xA5: opcodeName = "LDA dp"; byteLength = 2; break;
            case 0xB5: opcodeName = "LDA dp,X"; byteLength = 2; break;
            case 0xAD: opcodeName = "LDA abs"; byteLength = 3; break;
            case 0xBD: opcodeName = "LDA abs,X"; byteLength = 3; break;
            case 0xB9: opcodeName = "LDA abs,Y"; byteLength = 3; break;
            case 0xA1: opcodeName = "LDA (dp,X)"; byteLength = 2; break;
            case 0xB1: opcodeName = "LDA (dp),Y"; byteLength = 2; break;
            case 0xB2: opcodeName = "LDA (dp)"; byteLength = 2; break;
            case 0xB7: opcodeName = "LDA [dp],Y"; byteLength = 2; break;
            case 0xAF: opcodeName = "LDA long"; byteLength = 4; break;
            case 0xBF: opcodeName = "LDA long,X"; byteLength = 4; break;
            
            case 0xA2: opcodeName = "LDX #"; byteLength = m_modeX ? 2 : 3; break;
            case 0xA6: opcodeName = "LDX dp"; byteLength = 2; break;
            case 0xB6: opcodeName = "LDX dp,Y"; byteLength = 2; break;
            case 0xAE: opcodeName = "LDX abs"; byteLength = 3; break;
            case 0xBE: opcodeName = "LDX abs,Y"; byteLength = 3; break;
            
            case 0xA0: opcodeName = "LDY #"; byteLength = m_modeX ? 2 : 3; break;
            case 0xA4: opcodeName = "LDY dp"; byteLength = 2; break;
            case 0xB4: opcodeName = "LDY dp,X"; byteLength = 2; break;
            case 0xAC: opcodeName = "LDY abs"; byteLength = 3; break;
            case 0xBC: opcodeName = "LDY abs,X"; byteLength = 3; break;
            
            // Store Operations
            case 0x85: opcodeName = "STA dp"; byteLength = 2; break;
            case 0x95: opcodeName = "STA dp,X"; byteLength = 2; break;
            case 0x8D: opcodeName = "STA abs"; byteLength = 3; break;
            case 0x9D: opcodeName = "STA abs,X"; byteLength = 3; break;
            case 0x99: opcodeName = "STA abs,Y"; byteLength = 3; break;
            case 0x81: opcodeName = "STA (dp,X)"; byteLength = 2; break;
            case 0x91: opcodeName = "STA (dp),Y"; byteLength = 2; break;
            case 0x92: opcodeName = "STA (dp)"; byteLength = 2; break;
            case 0x97: opcodeName = "STA [dp],Y"; byteLength = 2; break;
            case 0x8F: opcodeName = "STA long"; byteLength = 4; break;
            case 0x9F: opcodeName = "STA long,X"; byteLength = 4; break;
            
            case 0x86: opcodeName = "STX dp"; byteLength = 2; break;
            case 0x96: opcodeName = "STX dp,Y"; byteLength = 2; break;
            case 0x8E: opcodeName = "STX abs"; byteLength = 3; break;
            
            case 0x84: opcodeName = "STY dp"; byteLength = 2; break;
            case 0x94: opcodeName = "STY dp,X"; byteLength = 2; break;
            case 0x8C: opcodeName = "STY abs"; byteLength = 3; break;
            
            // Zero Store Operations
            case 0x64: opcodeName = "STZ dp"; byteLength = 2; break;
            case 0x74: opcodeName = "STZ dp,X"; byteLength = 2; break;
            case 0x9C: opcodeName = "STZ abs"; byteLength = 3; break;
            case 0x9E: opcodeName = "STZ abs,X"; byteLength = 3; break;
            
            // Arithmetic Operations
            case 0x69: opcodeName = "ADC #"; byteLength = m_modeM ? 2 : 3; break;
            case 0x65: opcodeName = "ADC dp"; byteLength = 2; break;
            case 0x67: opcodeName = "ADC [dp]"; byteLength = 2; break;
            case 0x75: opcodeName = "ADC dp,X"; byteLength = 2; break;
            case 0x6D: opcodeName = "ADC abs"; byteLength = 3; break;
            case 0x7D: opcodeName = "ADC abs,X"; byteLength = 3; break;
            case 0x79: opcodeName = "ADC abs,Y"; byteLength = 3; break;
            case 0x61: opcodeName = "ADC (dp,X)"; byteLength = 2; break;
            case 0x63: opcodeName = "ADC sr,S"; byteLength = 2; break;
            case 0x71: opcodeName = "ADC (dp),Y"; byteLength = 2; break;
            case 0x72: opcodeName = "ADC (dp)"; byteLength = 2; break;
            case 0x73: opcodeName = "ADC (sr,S),Y"; byteLength = 2; break;
            case 0x77: opcodeName = "ADC [dp],Y"; byteLength = 2; break;
            case 0x6F: opcodeName = "ADC long"; byteLength = 4; break;
            case 0x7F: opcodeName = "ADC long,X"; byteLength = 4; break;
            
            case 0xE9: opcodeName = "SBC #"; byteLength = m_modeM ? 2 : 3; break;
            case 0xE5: opcodeName = "SBC dp"; byteLength = 2; break;
            case 0xF5: opcodeName = "SBC dp,X"; byteLength = 2; break;
            case 0xED: opcodeName = "SBC abs"; byteLength = 3; break;
            case 0xFD: opcodeName = "SBC abs,X"; byteLength = 3; break;
            case 0xF9: opcodeName = "SBC abs,Y"; byteLength = 3; break;
            case 0xE1: opcodeName = "SBC (dp,X)"; byteLength = 2; break;
            case 0xF1: opcodeName = "SBC (dp),Y"; byteLength = 2; break;
            case 0xF2: opcodeName = "SBC (dp)"; byteLength = 2; break;
            case 0xF7: opcodeName = "SBC [dp],Y"; byteLength = 2; break;
            case 0xEF: opcodeName = "SBC long"; byteLength = 4; break;
            case 0xFF: opcodeName = "SBC long,X"; byteLength = 4; break;
            
            // Logical Operations
            case 0x29: opcodeName = "AND #"; byteLength = m_modeM ? 2 : 3; break;
            case 0x25: opcodeName = "AND dp"; byteLength = 2; break;
            case 0x35: opcodeName = "AND dp,X"; byteLength = 2; break;
            case 0x2D: opcodeName = "AND abs"; byteLength = 3; break;
            case 0x3D: opcodeName = "AND abs,X"; byteLength = 3; break;
            case 0x39: opcodeName = "AND abs,Y"; byteLength = 3; break;
            case 0x21: opcodeName = "AND (dp,X)"; byteLength = 2; break;
            case 0x31: opcodeName = "AND (dp),Y"; byteLength = 2; break;
            case 0x32: opcodeName = "AND (dp)"; byteLength = 2; break;
            case 0x37: opcodeName = "AND [dp],Y"; byteLength = 2; break;
            case 0x2F: opcodeName = "AND long"; byteLength = 4; break;
            case 0x3F: opcodeName = "AND long,X"; byteLength = 4; break;
            
            case 0x49: opcodeName = "EOR #"; byteLength = m_modeM ? 2 : 3; break;
            case 0x45: opcodeName = "EOR dp"; byteLength = 2; break;
            case 0x55: opcodeName = "EOR dp,X"; byteLength = 2; break;
            case 0x4D: opcodeName = "EOR abs"; byteLength = 3; break;
            case 0x5D: opcodeName = "EOR abs,X"; byteLength = 3; break;
            case 0x59: opcodeName = "EOR abs,Y"; byteLength = 3; break;
            case 0x41: opcodeName = "EOR (dp,X)"; byteLength = 2; break;
            case 0x51: opcodeName = "EOR (dp),Y"; byteLength = 2; break;
            case 0x52: opcodeName = "EOR (dp)"; byteLength = 2; break;
            case 0x57: opcodeName = "EOR [dp],Y"; byteLength = 2; break;
            case 0x4F: opcodeName = "EOR long"; byteLength = 4; break;
            case 0x5F: opcodeName = "EOR long,X"; byteLength = 4; break;
            
            case 0x09: opcodeName = "ORA #"; byteLength = m_modeM ? 2 : 3; break;
            case 0x05: opcodeName = "ORA dp"; byteLength = 2; break;
            case 0x15: opcodeName = "ORA dp,X"; byteLength = 2; break;
            case 0x0D: opcodeName = "ORA abs"; byteLength = 3; break;
            case 0x1D: opcodeName = "ORA abs,X"; byteLength = 3; break;
            case 0x19: opcodeName = "ORA abs,Y"; byteLength = 3; break;
            case 0x01: opcodeName = "ORA (dp,X)"; byteLength = 2; break;
            case 0x11: opcodeName = "ORA (dp),Y"; byteLength = 2; break;
            case 0x12: opcodeName = "ORA (dp)"; byteLength = 2; break;
            case 0x17: opcodeName = "ORA [dp],Y"; byteLength = 2; break;
            case 0x0F: opcodeName = "ORA long"; byteLength = 4; break;
            case 0x1F: opcodeName = "ORA long,X"; byteLength = 4; break;
            
            // Compare Operations
            case 0xC9: opcodeName = "CMP #"; byteLength = m_modeM ? 2 : 3; break;
            case 0xC5: opcodeName = "CMP dp"; byteLength = 2; break;
            case 0xD5: opcodeName = "CMP dp,X"; byteLength = 2; break;
            case 0xCD: opcodeName = "CMP abs"; byteLength = 3; break;
            case 0xDD: opcodeName = "CMP abs,X"; byteLength = 3; break;
            case 0xD9: opcodeName = "CMP abs,Y"; byteLength = 3; break;
            case 0xC1: opcodeName = "CMP (dp,X)"; byteLength = 2; break;
            case 0xD1: opcodeName = "CMP (dp),Y"; byteLength = 2; break;
            case 0xD2: opcodeName = "CMP (dp)"; byteLength = 2; break;
            case 0xD7: opcodeName = "CMP [dp],Y"; byteLength = 2; break;
            case 0xCF: opcodeName = "CMP long"; byteLength = 4; break;
            case 0xDF: opcodeName = "CMP long,X"; byteLength = 4; break;
            
            case 0xE0: opcodeName = "CPX #"; byteLength = m_modeX ? 2 : 3; break;
            case 0xE4: opcodeName = "CPX dp"; byteLength = 2; break;
            case 0xEC: opcodeName = "CPX abs"; byteLength = 3; break;
            
            case 0xC0: opcodeName = "CPY #"; byteLength = m_modeX ? 2 : 3; break;
            case 0xC4: opcodeName = "CPY dp"; byteLength = 2; break;
            case 0xCC: opcodeName = "CPY abs"; byteLength = 3; break;
            
            // Increment/Decrement
            case 0xE6: opcodeName = "INC dp"; byteLength = 2; break;
            case 0xF6: opcodeName = "INC dp,X"; byteLength = 2; break;
            case 0xEE: opcodeName = "INC abs"; byteLength = 3; break;
            case 0xFE: opcodeName = "INC abs,X"; byteLength = 3; break;
            case 0xE8: opcodeName = "INX"; break;
            case 0xC8: opcodeName = "INY"; break;
            
            case 0xC6: opcodeName = "DEC dp"; byteLength = 2; break;
            case 0xD6: opcodeName = "DEC dp,X"; byteLength = 2; break;
            case 0xCE: opcodeName = "DEC abs"; byteLength = 3; break;
            case 0xDE: opcodeName = "DEC abs,X"; byteLength = 3; break;
            case 0xCA: opcodeName = "DEX"; break;
            case 0x88: opcodeName = "DEY"; break;
            
            // Bit Operations
            case 0x24: opcodeName = "BIT dp"; byteLength = 2; break;
            case 0x2C: opcodeName = "BIT abs"; byteLength = 3; break;
            case 0x34: opcodeName = "BIT dp,X"; byteLength = 2; break;
            case 0x3C: opcodeName = "BIT abs,X"; byteLength = 3; break;
            case 0x89: opcodeName = "BIT #"; byteLength = m_modeM ? 2 : 3; break;
            
            // Shift Operations
            case 0x06: opcodeName = "ASL dp"; byteLength = 2; break;
            case 0x16: opcodeName = "ASL dp,X"; byteLength = 2; break;
            case 0x0E: opcodeName = "ASL abs"; byteLength = 3; break;
            case 0x1E: opcodeName = "ASL abs,X"; byteLength = 3; break;
            
            case 0x26: opcodeName = "ROL dp"; byteLength = 2; break;
            case 0x36: opcodeName = "ROL dp,X"; byteLength = 2; break;
            case 0x2E: opcodeName = "ROL abs"; byteLength = 3; break;
            case 0x3E: opcodeName = "ROL abs,X"; byteLength = 3; break;
            
            case 0x46: opcodeName = "LSR dp"; byteLength = 2; break;
            case 0x56: opcodeName = "LSR dp,X"; byteLength = 2; break;
            case 0x4E: opcodeName = "LSR abs"; byteLength = 3; break;
            case 0x5E: opcodeName = "LSR abs,X"; byteLength = 3; break;
            
            case 0x66: opcodeName = "ROR dp"; byteLength = 2; break;
            case 0x76: opcodeName = "ROR dp,X"; byteLength = 2; break;
            case 0x6E: opcodeName = "ROR abs"; byteLength = 3; break;
            case 0x7E: opcodeName = "ROR abs,X"; byteLength = 3; break;
            
            // Block Move Operations
            case 0x44: opcodeName = "MVP"; byteLength = 3; break;
            case 0x54: opcodeName = "MVN"; byteLength = 3; break;
            
            // Reserved/Undocumented
            case 0x42: opcodeName = "WDM"; byteLength = 2; break;

            case 0xEA: opcodeName = "NOP";  break;
            
            default: opcodeName = "???"; break;
        }
        
        // Read instruction bytes
        std::ostringstream bytesStr;
        bytesStr << std::hex << std::uppercase << std::setfill('0');
        bytesStr << std::setw(2) << (int)opcode;
        for (int i = 1; i < byteLength; i++) {
            bytesStr << " " << std::setw(2) << (int)m_memory->read8(m_address + i);
        }
        
        std::ostringstream oss;
        oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') << m_cycles 
            << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
            << "PC:0x" << std::hex << std::setw(6) << std::setfill('0') << (uint32_t)(m_address) << " | "
            << std::left << std::setw(11) << std::setfill(' ') << bytesStr.str() << std::right << " | "
            << std::left << std::setw(12) << std::setfill(' ') << opcodeName << std::right << " | "
            << "A:0x" << std::setw(4) << std::setfill('0') << m_a << " | "
            << "X:0x" << std::setw(4) << std::setfill('0') << m_x << " | "
            << "Y:0x" << std::setw(4) << std::setfill('0') << m_y << " | "
            << "SP:0x" << std::setw(2) << std::setfill('0') << (int)m_sp << " | "
            << "P:0x" << std::setw(2) << std::setfill('0') << (int)m_p << " | "
            << "DBR:0x" << std::setw(2) << std::setfill('0') << (int)m_dbr << " | "
            << "PBR:0x" << std::setw(2) << std::setfill('0') << (int)m_pbr << " | "
            << "D:0x" << std::setw(4) << std::setfill('0') << m_d << " | "
            << "M:" << (m_modeM ? "8" : "16") << " X:" << (m_modeX ? "8" : "16") << " "
            << "E:" << (m_emulationMode ? "1" : "0");
        
        if (shouldLog) {
        Logger::getInstance().logCPU(oss.str());
        }
        instructionCount++;
        
        // Flush every 100 instructions
        if (instructionCount % 100 == 0) {
            Logger::getInstance().flush();
        }
    }
    uint8_t opcode = m_memory->read8(m_address);
    m_pc++;
    
    executeInstruction(opcode);
    m_cycles++;
}

void CPU::triggerNMI() {
    m_nmiPending = true;
}

void CPU::handleNMI() {
    // NMI (Non-Maskable Interrupt) handling
    // Push PBR, PC, and status to stack
    m_memory->write8(0x0100 + m_sp--, m_pbr);               // Program Bank
    m_memory->write8(0x0100 + m_sp--, (m_pc >> 8) & 0xFF);  // PC high byte
    m_memory->write8(0x0100 + m_sp--, m_pc & 0xFF);         // PC low byte
    m_memory->write8(0x0100 + m_sp--, m_p);                 // Status register
    
    // Set interrupt disable flag
    m_p |= 0x04;
    
    // Clear PBR (interrupts always jump to bank 0)
    m_pbr = 0x00;
    
    // Read NMI vector based on CPU mode:
    // - Native mode (E=0): 0xFFEA-0xFFEB
    // - Emulation mode (E=1): 0xFFFA-0xFFFB
    uint16_t vectorAddr = m_emulationMode ? 0xFFFA : 0xFFEA;
    uint16_t nmiVector = m_memory->read16(vectorAddr);
    
    // Debug: Print NMI vector information
    static int nmiCount = 0;
    if (nmiCount < 5) {
        std::cout << "NMI triggered! Mode=" << (m_emulationMode ? "Emulation" : "Native")
                  << ", VectorAddr=0x" << std::hex << vectorAddr
                  << ", Vector=0x" << nmiVector 
                  << ", PC=0x" << nmiVector 
                  << ", PBR=0x" << (int)m_pbr << std::dec << std::endl;
        nmiCount++;
    }
    
    m_pc = nmiVector;
}

void CPU::updateModeFlags() {
    // Update M and X flags from P register
    // M flag is bit 5, X flag is bit 4
    m_modeM = (m_p & 0x20) != 0;  // Bit 5
    m_modeX = (m_p & 0x10) != 0;  // Bit 4
    
}
void CPU::stackTrace(){
    std::ostringstream stackOss;
    stackOss << "Stack Monitor [Cyc:" << m_cycles << "] SP:0x" << std::hex << m_sp << " Stack: ";
    for (int i = 1; i < 32; i++) {
        uint8_t val;
        if (m_emulationMode) {
            val = m_memory->read8(0x0100 + ((m_sp + i) & 0xFF) | (m_pbr<<16));
        } else {
            val = m_memory->read8(m_sp_address + i);
        }
        stackOss << std::setw(2) << std::setfill('0') << (int)val << " ";
    }
    stackOss << std::dec;
    Logger::getInstance().logCPU(stackOss.str());
    Logger::getInstance().flush();
}
    
void CPU::pushStack(uint8_t value) {
    if(!m_emulationMode) {
        m_memory->write8(m_sp_address, value);
        m_sp--;
    } else {
        uint8_t SL = m_sp;
        m_memory->write8(0x100 | SL, value);
        SL--;
        m_sp = (m_sp&0xFF00)|SL;
    }
}
void CPU::pushStack16(uint16_t value) {
        pushStack(value >> 8);
        pushStack(value & 0xFF);
}
uint8_t CPU::pullStack() {
    if(!m_emulationMode) {
        m_sp++;
        return m_memory->read8(m_sp_address);
    } else {
        uint8_t SL = m_sp;
        SL++;
        m_sp = (m_sp&0xFF00)|SL;
        return m_memory->read8(0x100 | SL);
    }
}
uint16_t CPU::pullStack16() {
    uint8_t low = pullStack();
    uint8_t high = pullStack();
    return (low|(uint16_t)(high<<8));
}

uint16_t CPU::read16bit(uint16_t address) {
    // Read 16-bit value (little endian)
    uint8_t low = m_memory->read8(address);
    uint8_t high = m_memory->read8(address + 1);
    return (high << 8) | low;
}

void CPU::write16bit(uint16_t address, uint16_t value) {
    // Write 16-bit value (little endian)
    m_memory->write8(address, value & 0xFF);
    m_memory->write8(address + 1, (value >> 8) & 0xFF);
}

void CPU::executeInstruction(uint8_t opcode) {
    switch(opcode) {
        // Load/Store Instructions
        case 0xA9: { // LDA Immediate
            if (m_modeM) {
                // 8-bit mode
                m_a = (m_a & 0xFF00) | m_memory->read8(m_address_plus_1);
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                m_a = read16bit(m_address);
                m_pc += 2;
                setZeroNegative(m_a);
            }
        } break;
        
        case 0xA5: { // LDA Direct Page (Zero Page)
            uint8_t operand = m_memory->read8(m_address_plus_1);
            uint16_t addr = (m_d + operand) & 0xFFFF; // Use Direct Page register
            if (m_modeM) {
                m_a = (m_a & 0xFF00) | m_memory->read8(addr);
                setZeroNegative8(m_a & 0xFF);
            } else {
                m_a = read16bit(addr);
                setZeroNegative(m_a);
            }
        } break;
        
        case 0xB5: { // LDA Direct Page,X
            uint8_t operand = m_memory->read8(m_address_plus_1);
            // Use only low byte of X if in 8-bit index mode
            uint16_t xValue = m_modeX ? (m_x & 0xFF) : m_x;
            uint16_t addr = (m_d + operand + xValue) & 0xFFFF; // Use Direct Page register
            if (m_modeM) {
                m_a = (m_a & 0xFF00) | m_memory->read8(addr);
                setZeroNegative8(m_a & 0xFF);
            } else {
                m_a = read16bit(addr);
                setZeroNegative(m_a);
            }
        } break;
        
        case 0xAD: { // LDA Absolute
            uint16_t addr = m_memory->read16(m_address);
            m_pc += 2;
            if (m_modeM) {
                m_a = (m_a & 0xFF00) | m_memory->read8(addr);
                setZeroNegative8(m_a & 0xFF);
            } else {
                m_a = read16bit(addr);
                setZeroNegative(m_a);
            }
        } break;
        
        case 0xBD: { // LDA Absolute,X
            uint16_t addr = m_memory->read16(m_address) + (m_x & (m_modeX ? 0xFF : 0xFFFF));
            m_pc += 2;
            if (m_modeM) {
                m_a = (m_a & 0xFF00) | m_memory->read8(addr);
                setZeroNegative8(m_a & 0xFF);
            } else {
                m_a = read16bit(addr);
                setZeroNegative(m_a);
            }
        } break;
        
        case 0xB9: { // LDA Absolute,Y
            uint16_t addr = m_memory->read16(m_address) + (m_y & (m_modeX ? 0xFF : 0xFFFF));
            m_pc += 2;
            if (m_modeM) {
                m_a = (m_a & 0xFF00) | m_memory->read8(addr);
                setZeroNegative8(m_a & 0xFF);
            } else {
                m_a = read16bit(addr);
                setZeroNegative(m_a);
            }
        } break;
        
        case 0x85: { // STA Direct Page
            uint8_t operand = m_memory->read8(m_address_plus_1);
            uint16_t addr = (m_d + operand) & 0xFFFF;
            if (m_modeM) {
                m_memory->write8(addr, m_a & 0xFF);
            } else {
                write16bit(addr, m_a);
            }
        } break;
        
        case 0x95: { // STA Direct Page,X
            uint8_t operand = m_memory->read8(m_address_plus_1);
            uint16_t addr = (m_d + operand + (m_x & 0xFF)) & 0xFFFF;
            if (m_modeM) {
                m_memory->write8(addr, m_a & 0xFF);
            } else {
                write16bit(addr, m_a);
            }
        } break;
        
        case 0x8D: { // STA Absolute
            uint16_t addr = m_memory->read16(m_address);
            m_pc += 2;
            uint32_t fullAddr = (m_dbr << 16) | addr;  // Use DBR for bank
            static int staCount = 0;
            if (staCount < 100 && addr >= 0x4000 && addr < 0x4300) {
                std::ostringstream debug;
                debug << "=== DEBUG: STA addr=0x" << std::hex << addr 
                      << " fullAddr=0x" << fullAddr 
                      << " value=0x" << (int)(m_a & 0xFF) << std::dec;
                Logger::getInstance().logCPU(debug.str());
                Logger::getInstance().flush();
                staCount++;
            }
            if (m_modeM) {
                m_memory->write8(fullAddr, m_a & 0xFF);
            } else {
                m_memory->write8(fullAddr, m_a & 0xFF);
                m_memory->write8(fullAddr + 1, (m_a >> 8) & 0xFF);
            }
        } break;
        
        case 0x9D: { // STA Absolute,X
            uint16_t addr = m_memory->read16(m_address) + (m_x & (m_modeX ? 0xFF : 0xFFFF));
            m_pc += 2;
            if (m_modeM) {
                m_memory->write8(addr, m_a & 0xFF);
            } else {
                write16bit(addr, m_a);
            }
        } break;
        
        case 0x99: { // STA Absolute,Y
            uint16_t addr = m_memory->read16(m_address) + (m_y & (m_modeX ? 0xFF : 0xFFFF));
            m_pc += 2;
            if (m_modeM) {
                m_memory->write8(addr, m_a & 0xFF);
            } else {
                write16bit(addr, m_a);
            }
        } break;
        
        case 0xA2: { // LDX Immediate
            if (m_modeX) {
                m_x = (m_x & 0xFF00) | m_memory->read8(m_address_plus_1);
                setZeroNegative8(m_x & 0xFF);
            } else {
                m_x = read16bit(m_address);
                m_pc += 2;
                setZeroNegative(m_x);
            }
        } break;
        
        case 0xA6: { // LDX Zero Page
            uint8_t addr = m_memory->read8(m_address_plus_1);
            if (m_modeX) {
                m_x = (m_x & 0xFF00) | m_memory->read8(addr);
                setZeroNegative8(m_x & 0xFF);
            } else {
                m_x = read16bit(addr);
                setZeroNegative(m_x);
            }
        } break;
        
        case 0xAE: { // LDX Absolute
            uint16_t addr = m_memory->read16(m_address);
            m_pc += 2;
            if (m_modeX) {
                m_x = (m_x & 0xFF00) | m_memory->read8(addr);
                setZeroNegative8(m_x & 0xFF);
            } else {
                m_x = read16bit(addr);
                setZeroNegative(m_x);
            }
        } break;
        
        case 0xBE: { // LDX Absolute,Y
            uint16_t addr = m_memory->read16(m_address) + (m_y & (m_modeX ? 0xFF : 0xFFFF));
            m_pc += 2;
            if (m_modeX) {
                m_x = (m_x & 0xFF00) | m_memory->read8(addr);
                setZeroNegative8(m_x & 0xFF);
            } else {
                m_x = read16bit(addr);
                setZeroNegative(m_x);
            }
        } break;
        
        case 0x86: { // STX Zero Page
            uint8_t addr = m_memory->read8(m_address_plus_1);
            if (m_modeX) {
                m_memory->write8(addr, m_x & 0xFF);
            } else {
                write16bit(addr, m_x);
            }
        } break;
        
        case 0x96: { // STX Zero Page,Y
            uint8_t addr = (m_memory->read8(m_address_plus_1) + (m_y & 0xFF)) & 0xFF;
            if (m_modeX) {
                m_memory->write8(addr, m_x & 0xFF);
            } else {
                write16bit(addr, m_x);
            }
        } break;
        
        case 0x8E: { // STX Absolute
            uint16_t addr = m_memory->read16(m_address);
            m_pc += 2;
            if (m_modeX) {
                m_memory->write8(addr, m_x & 0xFF);
            } else {
                write16bit(addr, m_x);
            }
        } break;
        
        case 0xA0: { // LDY Immediate
            if (m_modeX) {
                m_y = (m_y & 0xFF00) | m_memory->read8(m_address_plus_1);
                setZeroNegative8(m_y & 0xFF);
            } else {
                m_y = read16bit(m_address);
                m_pc += 2;
                setZeroNegative(m_y);
            }
        } break;
        
        case 0xA4: { // LDY Zero Page
            uint8_t addr = m_memory->read8(m_address_plus_1);
            if (m_modeX) {
                m_y = (m_y & 0xFF00) | m_memory->read8(addr);
                setZeroNegative8(m_y & 0xFF);
            } else {
                m_y = read16bit(addr);
                setZeroNegative(m_y);
            }
        } break;
        
        case 0xB4: { // LDY Zero Page,X
            uint8_t addr = (m_memory->read8(m_address_plus_1) + (m_x & 0xFF)) & 0xFF;
            if (m_modeX) {
                m_y = (m_y & 0xFF00) | m_memory->read8(addr);
                setZeroNegative8(m_y & 0xFF);
            } else {
                m_y = read16bit(addr);
                setZeroNegative(m_y);
            }
        } break;
        
        case 0xAC: { // LDY Absolute
            uint16_t addr = m_memory->read16(m_address);
            m_pc += 2;
            if (m_modeX) {
                m_y = (m_y & 0xFF00) | m_memory->read8(addr);
                setZeroNegative8(m_y & 0xFF);
            } else {
                m_y = read16bit(addr);
                setZeroNegative(m_y);
            }
        } break;
        
        case 0xBC: { // LDY Absolute,X
            uint16_t addr = m_memory->read16(m_address) + (m_x & (m_modeX ? 0xFF : 0xFFFF));
            m_pc += 2;
            if (m_modeX) {
                m_y = (m_y & 0xFF00) | m_memory->read8(addr);
                setZeroNegative8(m_y & 0xFF);
            } else {
                m_y = read16bit(addr);
                setZeroNegative(m_y);
            }
        } break;
        
        case 0x84: { // STY Zero Page
            uint8_t addr = m_memory->read8(m_address_plus_1);
            if (m_modeX) {
                m_memory->write8(addr, m_y & 0xFF);
            } else {
                write16bit(addr, m_y);
            }
        } break;
        
        case 0x94: { // STY Zero Page,X
            uint8_t addr = (m_memory->read8(m_address_plus_1) + (m_x & 0xFF)) & 0xFF;
            if (m_modeX) {
                m_memory->write8(addr, m_y & 0xFF);
            } else {
                write16bit(addr, m_y);
            }
        } break;
        
        case 0x8C: { // STY Absolute
            uint16_t addr = m_memory->read16(m_address);
            m_pc += 2;
            if (m_modeX) {
                m_memory->write8(addr, m_y & 0xFF);
            } else {
                write16bit(addr, m_y);
            }
        } break;
        
        // Arithmetic Instructions
        case 0x69: { // ADC Immediate
            if (m_modeM) {
                // 8-bit mode
            uint8_t value = m_memory->read8(m_address_plus_1);
                uint8_t a8 = m_a & 0xFF;
                uint16_t result = a8 + value + (isCarry() ? 1 : 0);
                setCarry(result > 0xFF);
                // Overflow: (A^result) & (value^result) & 0x80
                setOverflow((~(a8 ^ value) & (a8 ^ result) & 0x80) != 0);
                m_a = (m_a & 0xFF00) | (result & 0xFF);
                setZeroNegative8(result & 0xFF);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(m_address);
                m_pc += 2;
                uint32_t result = m_a + value + (isCarry() ? 1 : 0);
                setCarry(result > 0xFFFF);
                // Overflow: (A^result) & (value^result) & 0x8000
                setOverflow((~(m_a ^ value) & (m_a ^ result) & 0x8000) != 0);
                m_a = result & 0xFFFF;
                setZeroNegative(m_a);
            }
        } break;
        
        case 0xE9: { // SBC Immediate
            if (m_modeM) {
                // 8-bit mode
            uint8_t value = m_memory->read8(m_address_plus_1);
                uint8_t a8 = m_a & 0xFF;
                uint16_t result = a8 - value - (isCarry() ? 0 : 1);
                setCarry(result < 0x100);
                // Overflow: (A^value) & (A^result) & 0x80
                setOverflow(((a8 ^ value) & (a8 ^ result) & 0x80) != 0);
                m_a = (m_a & 0xFF00) | (result & 0xFF);
                setZeroNegative8(result & 0xFF);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(m_address);
                m_pc += 2;
                uint32_t result = m_a - value - (isCarry() ? 0 : 1);
                setCarry(result < 0x10000);
                // Overflow: (A^value) & (A^result) & 0x8000
                setOverflow(((m_a ^ value) & (m_a ^ result) & 0x8000) != 0);
                m_a = result & 0xFFFF;
                setZeroNegative(m_a);
            }
        } break;

        case 0xE5: { // SBC Zero Page
            uint8_t addr = m_memory->read8(m_address_plus_1);
            if (m_modeM) {
                // 8-bit mode
                uint8_t value = m_memory->read8(addr);
                uint16_t result = (m_a & 0xFF) - value - (isCarry() ? 0 : 1);
                setCarry(result < 0x100);
                setOverflow(((m_a ^ value) & (m_a ^ result) & 0x80) != 0);
                m_a = (m_a & 0xFF00) | (result & 0xFF);
                setZeroNegative8(result & 0xFF);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(addr);
                uint32_t result = m_a - value - (isCarry() ? 0 : 1);
                setCarry(result < 0x10000);
                setOverflow(((m_a ^ value) & (m_a ^ result) & 0x8000) != 0);
                m_a = result & 0xFFFF;
                setZeroNegative(m_a);
            }
        } break;


        
        case 0xC9: { // CMP Immediate
            if (m_modeM) {
                // 8-bit mode
            uint8_t value = m_memory->read8(m_address_plus_1);
                uint16_t result = (m_a & 0xFF) - value;
                setCarry((m_a & 0xFF) >= value);
                setZero(result == 0);
                setNegative(result & 0x80);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(m_address);
                m_pc += 2;
                uint32_t result = m_a - value;
                setCarry(m_a >= value);
                setZero(result == 0);
                setNegative(result & 0x8000);
            }
        } break;
        
        case 0xC5: { // CMP Zero Page
            uint8_t addr = m_memory->read8(m_address_plus_1);
            if (m_modeM) {
                // 8-bit mode
                uint8_t value = m_memory->read8(addr);
                uint16_t result = (m_a & 0xFF) - value;
                setCarry((m_a & 0xFF) >= value);
                setZero(result == 0);
                setNegative(result & 0x80);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(addr);
                uint32_t result = m_a - value;
                setCarry(m_a >= value);
                setZero(result == 0);
                setNegative(result & 0x8000);
            }
        } break;
        
        case 0xCD: { // CMP Absolute
            uint16_t addr = m_memory->read16(m_address);
    m_pc += 2;
            if (m_modeM) {
                // 8-bit mode
            uint8_t value = m_memory->read8(addr);
                uint16_t result = (m_a & 0xFF) - value;
                setCarry((m_a & 0xFF) >= value);
                setZero(result == 0);
                setNegative(result & 0x80);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(addr);
                uint32_t result = m_a - value;
                setCarry(m_a >= value);
                setZero(result == 0);
                setNegative(result & 0x8000);
            }
        } break;
        
        case 0xE0: { // CPX Immediate
            if (m_modeX) {
                // 8-bit mode
            uint8_t value = m_memory->read8(m_address_plus_1);
                uint16_t result = (m_x & 0xFF) - value;
                setCarry((m_x & 0xFF) >= value);
                setZero(result == 0);
                setNegative(result & 0x80);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(m_address);
                m_pc += 2;
                uint32_t result = m_x - value;
                setCarry(m_x >= value);
                setZero(result == 0);
                setNegative(result & 0x8000);
            }
        } break;
        
        case 0xC0: { // CPY Immediate
            if (m_modeX) {
                // 8-bit mode
            uint8_t value = m_memory->read8(m_address_plus_1);
                uint16_t result = (m_y & 0xFF) - value;
                setCarry((m_y & 0xFF) >= value);
                setZero(result == 0);
                setNegative(result & 0x80);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(m_address);
                m_pc += 2;
                uint32_t result = m_y - value;
                setCarry(m_y >= value);
                setZero(result == 0);
                setNegative(result & 0x8000);
            }
        } break;
        case 0xE4: { // CPX Zero Page
            uint8_t addr = m_memory->read8(m_address_plus_1);
            if (m_modeX) {
                uint8_t value = m_memory->read8(addr);
                uint16_t result = (m_x & 0xFF) - value;
                setCarry((m_x & 0xFF) >= value);
                setZero(result == 0);
                setNegative(result & 0x80);
            } else {
                uint16_t value = read16bit(addr);
                uint32_t result = m_x - value;
                setCarry(m_x >= value);
                setZero(result == 0);
                setNegative(result & 0x8000);
            }
        } break;
        
        case 0xEC: { // CPX Absolute
            uint16_t addr = m_memory->read16(m_address);
            m_pc += 2;
            if (m_modeX) {
                // 8-bit mode
                uint8_t value = m_memory->read8(addr);
                uint16_t result = (m_x & 0xFF) - value;
                setCarry((m_x & 0xFF) >= value);
                setZero(result == 0);
                setNegative(result & 0x80);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(addr);
                uint32_t result = m_x - value;
                setCarry(m_x >= value);
                setZero(result == 0);
                setNegative(result & 0x8000);
            }
        } break;
        
        case 0xCC: { // CPY Absolute
            uint16_t addr = m_memory->read16(m_address);
            m_pc += 2;
            if (m_modeX) {
                // 8-bit mode
                uint8_t value = m_memory->read8(addr);
                uint16_t result = (m_y & 0xFF) - value;
                setCarry((m_y & 0xFF) >= value);
                setZero(result == 0);
                setNegative(result & 0x80);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(addr);
                uint32_t result = m_y - value;
                setCarry(m_y >= value);
                setZero(result == 0);
                setNegative(result & 0x8000);
            }
        } break;
        
        // Logical Instructions
        case 0x29: { // AND Immediate
            if (m_modeM) {
                // 8-bit mode
                m_a = (m_a & 0xFF00) | ((m_a & 0xFF) & m_memory->read8(m_address_plus_1));
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                m_a &= read16bit(m_address);
                m_pc += 2;
                setZeroNegative(m_a);
            }
        } break;
        
        case 0x25: { // AND Zero Page
            uint8_t addr = m_memory->read8(m_address_plus_1);
            if (m_modeM) {
                // 8-bit mode
                m_a = (m_a & 0xFF00) | ((m_a & 0xFF) & m_memory->read8(addr));
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                m_a &= read16bit(addr);
                setZeroNegative(m_a);
            }
        } break;
        
        case 0x2D: { // AND Absolute
            uint16_t addr = m_memory->read16(m_address);
            m_pc += 2;
            if (m_modeM) {
                // 8-bit mode
                m_a = (m_a & 0xFF00) | ((m_a & 0xFF) & m_memory->read8(addr));
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                m_a &= read16bit(addr);
                setZeroNegative(m_a);
            }
        } break;
        case 0x09: { // ORA Immediate
            if (m_modeM) {
                // 8-bit mode
                m_a = (m_a & 0xFF00) | ((m_a & 0xFF) | m_memory->read8(m_address_plus_1));
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                m_a |= read16bit(m_address);
                m_pc += 2;
                setZeroNegative(m_a);
            }
        } break;
        
        case 0x05: { // ORA Zero Page
            uint8_t addr = m_memory->read8(m_address_plus_1);
            if (m_modeM) {
                // 8-bit mode
                m_a = (m_a & 0xFF00) | ((m_a & 0xFF) | m_memory->read8(addr));
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                m_a |= read16bit(addr);
                setZeroNegative(m_a);
            }
        } break;
        
        case 0x0D: { // ORA Absolute
            uint16_t addr = m_memory->read16(m_address);
            m_pc += 2;
            if (m_modeM) {
                // 8-bit mode
                m_a = (m_a & 0xFF00) | ((m_a & 0xFF) | m_memory->read8(addr));
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                m_a |= read16bit(addr);
                setZeroNegative(m_a);
            }
        } break;
        
        case 0x49: { // EOR Immediate
            if (m_modeM) {
                // 8-bit mode
                m_a = (m_a & 0xFF00) | ((m_a & 0xFF) ^ m_memory->read8(m_address_plus_1));
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                m_a ^= read16bit(m_address);
                m_pc += 2;
                setZeroNegative(m_a);
            }
        } break;
        
        case 0x45: { // EOR Zero Page
            uint8_t addr = m_memory->read8(m_address_plus_1);
            if (m_modeM) {
                // 8-bit mode
                m_a = (m_a & 0xFF00) | ((m_a & 0xFF) ^ m_memory->read8(addr));
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                m_a ^= read16bit(addr);
                setZeroNegative(m_a);
            }
        } break;
        case 0x4D: { // EOR Absolute
            uint16_t addr = m_memory->read16(m_address);
            m_pc += 2;
            if (m_modeM) {
                // 8-bit mode
                m_a = (m_a & 0xFF00) | ((m_a & 0xFF) ^ m_memory->read8(addr));
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                m_a ^= read16bit(addr);
                setZeroNegative(m_a);
            }
        } break;
        case 0x24: { // BIT Zero Page
            uint8_t addr = m_memory->read8(m_address_plus_1);
            if (m_modeM) {
                // 8-bit mode
                uint8_t value = m_memory->read8(addr);
                setNegative(value & 0x80);
                setZero(((m_a & 0xFF) & value) == 0);
                setOverflow(value & 0x40);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(addr);
                setNegative(value & 0x8000);
                setZero((m_a & value) == 0);
                setOverflow(value & 0x4000);
            }
        } break;
        
        case 0x2C: { // BIT Absolute
            uint16_t addr = m_memory->read16(m_address);
    m_pc += 2;
            if (m_modeM) {
                // 8-bit mode
            uint8_t value = m_memory->read8(addr);
                setNegative(value & 0x80);
                setZero(((m_a & 0xFF) & value) == 0);
                setOverflow(value & 0x40);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(addr);
                setNegative(value & 0x8000);
                setZero((m_a & value) == 0);
                setOverflow(value & 0x4000);
            }
        } break;
        
        // Increment/Decrement
        case 0xE8: { // INX
            if (m_modeX) {
                // 8-bit mode
                m_x = (m_x & 0xFF00) | ((m_x & 0xFF) + 1);
                setZeroNegative8(m_x & 0xFF);
            } else {
                // 16-bit mode
                m_x++;
                setZeroNegative(m_x);
            }
        } break;
        
        case 0xC8: { // INY
            if (m_modeX) {
                // 8-bit mode
                m_y = (m_y & 0xFF00) | ((m_y & 0xFF) + 1);
                setZeroNegative8(m_y & 0xFF);
            } else {
                // 16-bit mode
                m_y++;
                setZeroNegative(m_y);
            }
        } break;
        
        case 0xAA: { // TAX
            if (m_modeX) {
                // 8-bit mode
                m_x = (m_x & 0xFF00) | (m_a & 0xFF);
                setZeroNegative8(m_x & 0xFF);
            } else {
                // 16-bit mode
                m_x = m_a;
                setZeroNegative(m_x);
            }
        } break;
        case 0xCA: { // DEX
            if (m_modeX) {
                // 8-bit mode
                m_x = (m_x & 0xFF00) | ((m_x & 0xFF) - 1);
                setZeroNegative8(m_x & 0xFF);
            } else {
                // 16-bit mode
            m_x--;
                setZeroNegative(m_x);
            }
            static int dexCount = 0;
            if (dexCount < 3) {
                std::cout << "[Cyc:" << std::dec << m_cycles << "] "
                          << "DEX: X=" << std::hex << (int)m_x << ", Zero=" << ((m_p & 0x02) ? "true" : "false") 
                          << ", Negative=" << ((m_p & 0x80) ? "true" : "false") << std::dec << std::endl;
                dexCount++;
            }
        } break;
        case 0x88: { // DEY
            if (m_modeX) {
                // 8-bit mode
                m_y = (m_y & 0xFF00) | ((m_y & 0xFF) - 1);
                setZeroNegative8(m_y & 0xFF);
            } else {
                // 16-bit mode
                m_y--;
                setZeroNegative(m_y);
            }
        } break;
        case 0x1A: { // INC A
            if (m_modeM) {
                // 8-bit mode
                m_a = (m_a & 0xFF00) | ((m_a & 0xFF) + 1);
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                m_a++;
                setZeroNegative(m_a);
            }
        } break;
        
        case 0x3A: { // DEC A
            if (m_modeM) {
                // 8-bit mode
                m_a = (m_a & 0xFF00) | ((m_a & 0xFF) - 1);
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                m_a--;
                setZeroNegative(m_a);
            }
        } break;
        case 0xE6: { // INC Zero Page
            uint8_t addr = m_memory->read8(m_address_plus_1);
            if (m_modeM) {
                // 8-bit mode
            uint8_t value = m_memory->read8(addr) + 1;
            m_memory->write8(addr, value);
                setZeroNegative8(value);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(addr) + 1;
                write16bit(addr, value);
                setZeroNegative(value);
            }
        } break;
        
        case 0xC6: { // DEC Zero Page
            uint8_t addr = m_memory->read8(m_address_plus_1);
            if (m_modeM) {
                // 8-bit mode
            uint8_t value = m_memory->read8(addr) - 1;
            m_memory->write8(addr, value);
                setZeroNegative8(value);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(addr) - 1;
                write16bit(addr, value);
                setZeroNegative(value);
            }
        } break;
        
        case 0xEE: { // INC Absolute
            uint16_t addr = m_memory->read16(m_address);
    m_pc += 2;
            if (m_modeM) {
                // 8-bit mode
            uint8_t value = m_memory->read8(addr) + 1;
            m_memory->write8(addr, value);
                setZeroNegative8(value);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(addr) + 1;
                write16bit(addr, value);
                setZeroNegative(value);
            }
        } break;
        
        case 0xCE: { // DEC Absolute
            uint16_t addr = m_memory->read16(m_address);
    m_pc += 2;
            if (m_modeM) {
                // 8-bit mode
            uint8_t value = m_memory->read8(addr) - 1;
            m_memory->write8(addr, value);
                setZeroNegative8(value);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(addr) - 1;
                write16bit(addr, value);
                setZeroNegative(value);
            }
        } break;
        
        // Register Transfers
        case 0x8A: { // TXA
            if (m_modeM) {
                // 8-bit mode
                m_a = (m_a & 0xFF00) | (m_x & 0xFF);
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                m_a = m_x;
                setZeroNegative(m_a);
            }
        } break;
        
        case 0xA8: { // TAY
            if (m_modeX) {
                // 8-bit mode
                m_y = (m_y & 0xFF00) | (m_a & 0xFF);
                setZeroNegative8(m_y & 0xFF);
            } else {
                // 16-bit mode
                m_y = m_a;
                setZeroNegative(m_y);
            }
        } break;
        
        case 0x98: { // TYA
            if (m_modeM) {
                // 8-bit mode
                m_a = (m_a & 0xFF00) | (m_y & 0xFF);
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                m_a = m_y;
                setZeroNegative(m_a);
            }
        } break;
        
        case 0xBA: { // TSX
            if (m_emulationMode) {
                // Emulation mode: SP is 8-bit, always in page 1
            if (m_modeX) {
                    // 8-bit X
                m_x = (m_x & 0xFF00) | m_sp;
                setZeroNegative8(m_x & 0xFF);
            } else {
                    // 16-bit X
                m_x = 0x0100 | m_sp;
                setZeroNegative(m_x);
                }
            } else {
                // Native mode: SP is 16-bit
                if (m_modeX) {
                    // 8-bit X: only transfer low byte of SP
                    m_x = (m_x & 0xFF00) | (m_sp & 0xFF);
                    setZeroNegative8(m_x & 0xFF);
                } else {
                    // 16-bit X: transfer full 16-bit SP
                    m_x = m_sp;
                    setZeroNegative(m_x);
                }
            }
        } break;
        
        case 0x9A: { // TXS
            if (m_emulationMode) {
                // Emulation mode: SP is 8-bit
                m_sp = m_x & 0xFF;
            } else {
                // Native mode: SP is 16-bit
                if (m_modeX) {
                    // 8-bit X: only set low byte of SP, high byte unchanged
                    m_sp = (m_sp & 0xFF00) | (m_x & 0xFF);
                } else {
                    // 16-bit X: transfer full 16-bit value
                    m_sp = m_x;
                }
            }
        } break;
        
        // Stack Instructions
        case 0x48: { // PHA
            if (m_modeM) {
                // 8-bit mode
                pushStack(m_a);
            } else {
                // 16-bit mode
                pushStack16(m_a);
            }
            stackTrace();
        } break;
        
        case 0x68: { // PLA
            if (m_modeM) {
                // 8-bit mode
                m_a = (m_a&0xFF00) | pullStack();
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                m_a = pullStack16();
                setZeroNegative(m_a);   
            }
            stackTrace();
        } break;
        
        case 0x08: pushStack(m_p); stackTrace();break; // PHP
        case 0x28: m_p = pullStack(); stackTrace();break; // PLP
        
        // Jump/Branch Instructions
        
        // Branch Instructions
        case 0x90: if (!(m_p & 0x01)) m_pc += (int8_t)m_memory->read8(m_address) + 1; else m_pc++; break; // BCC
        case 0xB0: if (m_p & 0x01) m_pc += (int8_t)m_memory->read8(m_address) + 1; else m_pc++; break; // BCS
        case 0xF0: if (m_p & 0x02) m_pc += (int8_t)m_memory->read8(m_address) + 1; else m_pc++; break; // BEQ
        case 0xD0: { // BNE - Branch if Not Equal
            uint8_t offset = m_memory->read8(m_address_plus_1);
            bool zeroFlag = (m_p & 0x02) != 0;
            static int bneCount = 0;
            if (bneCount < 5) {
                std::cout << "[Cyc:" << std::dec << m_cycles << "] "
                          << "BNE: Zero=" << (zeroFlag ? "true" : "false") 
                          << ", offset=0x" << std::hex << (int)offset << std::dec
                          << ", PC before=" << std::hex << (m_address - 1) << std::dec;
            }
            if (!zeroFlag) {
                m_pc += (int8_t)offset;
                if (bneCount < 5) {
                    std::cout << ", BRANCH TAKEN, PC after=" << std::hex << m_pc << std::dec;
                }
            } else {
                if (bneCount < 5) {
                    std::cout << ", NO BRANCH";
                }
            }
            if (bneCount < 5) {
                std::cout << std::endl;
                bneCount++;
            }
        } break;
        case 0x30: { // BMI - Branch if Minus
            uint8_t offset = m_memory->read8(m_address_plus_1);
            if (m_p & 0x80) {
                m_pc += (int8_t)offset;
            }
        } break;
        case 0x10: if (!(m_p & 0x80)) m_pc += (int8_t)m_memory->read8(m_address) + 1; else m_pc++; break; // BPL
        case 0x50: if (!(m_p & 0x40)) m_pc += (int8_t)m_memory->read8(m_address) + 1; else m_pc++; break; // BVC
        case 0x70: if (m_p & 0x40) m_pc += (int8_t)m_memory->read8(m_address) + 1; else m_pc++; break; // BVS
        case 0x80: m_pc += (int8_t)m_memory->read8(m_address) + 1; break; // BRA (Always branch)
        
        // Jump Instructions
        case 0x4C: { // JMP Absolute
            uint16_t addr = m_memory->read16(m_address);
            m_pc = addr;
        } break;
        
        case 0x6C: { // JMP Indirect
            uint16_t addr = m_memory->read16(m_address);
            m_pc = m_memory->read16(addr);
        } break;
        
        case 0x20: { // JSR - Jump to Subroutine
            uint16_t addr = m_memory->read16(m_address);
    m_pc += 2;
            pushStack16(m_address - 1);
            stackTrace();
            m_pc = addr;
        } break;
        
        case 0x40: { // RTI - Return from Interrupt
            // Pull P, PC, and PBR from stack
            m_p = pullStack();
            m_pc = pullStack16();
            m_pbr = pullStack();
            updateModeFlags(); // Update M and X flags after status change
            
            static int rtiCount = 0;
            if (rtiCount < 5) {
                std::cout << "RTI: Restored PC=0x" << std::hex << m_pc 
                          << ", PBR=0x" << (int)m_pbr
                          << ", P=0x" << (int)m_p << std::dec << std::endl;
                rtiCount++;
            }
            stackTrace();
        } break;
        
        case 0x60: { // RTS - Return from Subroutine
            // Pull return address from stack (in page 1: 0x0100-0x01FF)
            // Stack grows downward, so we need to read in reverse order
            uint16_t addr = pullStack16();
            m_pc = addr + 1;  // RTS returns to address + 1
            stackTrace();
        } break;
        
        // Flag Instructions
        case 0x18: m_p &= ~0x01; break; // CLC
        
        case 0x78: m_p |= 0x04; break; // SEI - Set Interrupt Disable
        
        case 0x9C: { // STZ Absolute
            uint16_t addr = m_memory->read16(m_address);
            m_pc += 2;
            m_memory->write8(addr, 0x00);
        } break;
        
        case 0xFB: { // XCE - Exchange Carry and Emulation bit
            bool carry = isCarry();
            bool emulation = m_emulationMode;
            
            // Exchange Carry and Emulation flags
            setCarry(emulation);
            m_emulationMode = carry;
            
            // If switching to Emulation mode, force M and X to 1 (8-bit mode)
            if (m_emulationMode) {
                setAccumulator8bit(true);
                setIndex8bit(true);
                // In emulation mode, high byte of X and Y are forced to 0
                m_x &= 0x00FF;
                m_y &= 0x00FF;
            }
            
            updateModeFlags();
        } break;
        case 0x38: m_p |= 0x01; break; // SEC
        case 0xD8: m_p &= ~0x08; break; // CLD
        case 0xF8: m_p |= 0x08; break; // SED
        case 0x58: m_p &= ~0x04; break; // CLI
        case 0xB8: m_p &= ~0x40; break; // CLV
        
        // Additional instructions
        case 0xC4: { // CPY Zero Page
            uint8_t addr = m_memory->read8(m_address_plus_1);
            if (m_modeX) {
                // 8-bit mode
            uint8_t value = m_memory->read8(addr);
                uint16_t result = (m_y & 0xFF) - value;
                setCarry((m_y & 0xFF) >= value);
                setZero((result & 0xFF) == 0);
                setNegative(result & 0x80);
            } else {
                // 16-bit mode
                uint16_t value = read16bit(addr);
                uint32_t result = m_y - value;
                setCarry(m_y >= value);
                setZero((result & 0xFFFF) == 0);
                setNegative(result & 0x8000);
            }
        } break;
        
        // 65816 specific instructions
        case 0xC2: { // REP - Reset Status Bits
            static int repCount = 0;
            if (repCount < 5) {
                std::cout << "[Cyc:" << std::dec << m_cycles << "] "
                          << "P = 0x" << std::hex << (int)m_p 
                          << " - M=" << (m_modeM ? "8" : "16") 
                          << "bit, X=" << (m_modeX ? "8" : "16") << "bit" 
                          << std::dec << std::endl;
            }
            uint8_t mask = m_memory->read8(m_address_plus_1);
            // Apply mask to reset flags
            m_p = m_p & ~mask;
            updateModeFlags();  // Update M and X flags
            
            if (repCount < 5) {
                std::cout << "[Cyc:" << std::dec << m_cycles << "] "
                          << "REP $" << std::hex << (int)mask 
                          << " - M=" << (m_modeM ? "8" : "16") 
                          << "bit, X=" << (m_modeX ? "8" : "16") << "bit" 
                          << std::dec << std::endl;
                repCount++;
            }
        } break;
        case 0xE2: { // SEP - Set Status Bits
            uint8_t mask = m_memory->read8(m_address_plus_1);
            // Apply mask to set flags
            m_p = (m_p | mask);
            
            // If X flag (bit 4) is being set to 1 (8-bit mode)
            // Clear high bytes of X and Y registers
            if ((mask & 0x10) && !m_emulationMode) {
                m_x = m_x & 0xFF;
                m_y = m_y & 0xFF;
            }
            
            updateModeFlags();  // Update M and X flags
            
            static int sepCount = 0;
            if (sepCount < 5) {
                std::cout << "[Cyc:" << std::dec << m_cycles << "] "
                          << "SEP $" << std::hex << (int)mask 
                          << " - M=" << (m_modeM ? "8" : "16") 
                          << "bit, X=" << (m_modeX ? "8" : "16") << "bit" 
                          << std::dec << std::endl;
                sepCount++;
            }
        } break;
        case 0xEB: { // XBA - Exchange B and A
            // Exchange high and low bytes of A register
            uint8_t temp = (m_a >> 8) & 0xFF;
            m_a = (m_a & 0xFF) | ((m_a & 0xFF) << 8);
            m_a = (m_a & 0xFF00) | temp;
            
            // Set Zero and Negative flags based on low byte
            setZeroNegative8(m_a & 0xFF);
        } break;
        
        // Shift/Rotate Instructions
        case 0x0A: { // ASL A
            if (m_modeM) {
                // 8-bit mode
                setCarry(m_a & 0x80);
                m_a = (m_a & 0xFF00) | ((m_a & 0xFF) << 1);
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                setCarry(m_a & 0x8000);
    m_a <<= 1;
                setZeroNegative(m_a);
            }
        } break;
        
        case 0x4A: { // LSR A
            if (m_modeM) {
                // 8-bit mode
                setCarry(m_a & 0x01);
                m_a = (m_a & 0xFF00) | ((m_a & 0xFF) >> 1);
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                setCarry(m_a & 0x01);
            m_a >>= 1;
                setZeroNegative(m_a);
            }
        } break;
        
        case 0x2A: { // ROL A
            if (m_modeM) {
                // 8-bit mode
                uint8_t carry = isCarry() ? 1 : 0;
                setCarry(m_a & 0x80);
                m_a = (m_a & 0xFF00) | (((m_a & 0xFF) << 1) | carry);
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                uint16_t carry = isCarry() ? 1 : 0;
                setCarry(m_a & 0x8000);
            m_a = (m_a << 1) | carry;
                setZeroNegative(m_a);
            }
        } break;
        
        case 0x6A: { // ROR A
            if (m_modeM) {
                // 8-bit mode
                uint8_t carry = isCarry() ? 0x80 : 0;
                setCarry(m_a & 0x01);
                m_a = (m_a & 0xFF00) | (((m_a & 0xFF) >> 1) | carry);
                setZeroNegative8(m_a & 0xFF);
            } else {
                // 16-bit mode
                uint16_t carry = isCarry() ? 0x8000 : 0;
                setCarry(m_a & 0x01);
            m_a = (m_a >> 1) | carry;
                setZeroNegative(m_a);
            }
        } break;
        
        // Special Instructions
        case 0xEA: break; // NOP
        case 0x9E: m_memory->write8(m_memory->read16(m_address) + m_x, 0); m_pc += 2; break; // STZ Absolute,X
        case 0xFE: { // INC Absolute,X
            uint16_t addr = m_memory->read16(m_address) + m_x;
            m_pc += 2;
            uint8_t value = m_memory->read8(addr) + 1;
            m_memory->write8(addr, value);
        } break;
        case 0x5A: 
            if(m_modeX)
                pushStack(m_y&0xFF);
            else 
                pushStack16(m_y);
            stackTrace();
            break; // PHY
        case 0x7A: 
            if(m_modeX)
                m_y = pullStack();
            else 
                m_y = pullStack16();
            stackTrace();
            break; // PLY
        case 0xDA: 
            if(m_modeX)
                pushStack(m_x&0xFF);
            else 
                pushStack16(m_x);
            stackTrace();
            break; // PHX
        case 0xFA: 
            if(m_modeX)
                m_x = pullStack();
            else 
                m_x = pullStack16();
            stackTrace();
            break; // PLX
        case 0x5B: { // TCD - Transfer 16-bit A to D
            // Transfer 16-bit A to Direct Page register
            m_d = m_a;
            
            setNegative(m_d & 0x8000);
            // Set Zero and Negative flags based on D register
            setZeroNegative(m_d);
        } break;
        case 0x1B: { // TCS - Transfer A to SP
            if (m_emulationMode) {
                // Emulation mode: Transfer 8-bit A to 8-bit SP
                m_sp = m_a & 0xFF;
            } else {
                // Native mode: Transfer 16-bit A to 16-bit SP
                m_sp = m_a;
            }
        } break;
        case 0x7B: { // TDC - Transfer D to 16-bit A
            // Transfer Direct Page register to 16-bit A
            m_a = m_d;
            
            // Set Zero and Negative flags based on A register
            setZeroNegative(m_a);
        } break;
        case 0x3B: { // TSC - Transfer SP to 16-bit A
            // Transfer Stack Pointer to 16-bit A
            m_a = m_sp;
            
            // Set Zero and Negative flags based on A register
            setZeroNegative(m_a);
        } break;
        case 0x8B: { // PHB - Push Data Bank
            // Push Data Bank register to stack
            pushStack(m_dbr);
            stackTrace();
        } break;
        case 0xAB: { // PLB - Pull Data Bank
            // Pull Data Bank register from stack
            m_dbr = pullStack();
            stackTrace();
            // Set Zero and Negative flags based on Data Bank register
            setZeroNegative8(m_dbr);
        } break;
        case 0x8F: { // STA Long (24-bit address)
            // Read 24-bit address (3 bytes: low, mid, high)
            uint8_t addrLow = m_memory->read8(m_address_plus_1);
            uint8_t addrMid = m_memory->read8(m_address_plus_1);
            uint8_t addrBank = m_memory->read8(m_address_plus_1);
            
            // Calculate full 24-bit address
            uint32_t addr = (addrBank << 16) | (addrMid << 8) | addrLow;
            
            // Write A to memory (handle 8-bit/16-bit modes)
            if (m_modeM)
                m_memory->write8(addr, m_a & 0xFF);
            else
                m_memory->write16(addr, m_a);
        } break;
        
        // Additional missing instructions
        case 0x64: { // STZ Zero Page
            uint8_t addr = m_memory->read8(m_address_plus_1);
            m_memory->write8(addr, 0);
        } break;
        
        case 0x74: { // STZ Zero Page,X
            uint8_t addr = (m_memory->read8(m_address_plus_1) + (m_x & 0xFF)) & 0xFF;
            m_memory->write8(addr, 0);
        } break;
        
        case 0xB3: { // LDA (Direct Page),Y
            uint8_t dp = m_memory->read8(m_address_plus_1);
            uint16_t addr = dp + m_y;
            if (m_modeM) {
                m_a = (m_a & 0xFF00) | m_memory->read8(addr);
                setZeroNegative8(m_a & 0xFF);
            } else {
                m_a = read16bit(addr);
                setZeroNegative(m_a);
            }
        } break;
        
        // Additional Super Mario World instructions
        case 0x9F: { // STA Long,X (24-bit address + X)
            // Read 24-bit address (3 bytes: low, mid, high)
            uint8_t addrLow = m_memory->read8(m_address_plus_1);
            uint8_t addrMid = m_memory->read8(m_address_plus_1);
            uint8_t addrBank = m_memory->read8(m_address_plus_1);
            
            // Calculate full 24-bit address + X
            uint32_t addr = (addrBank << 16) | (addrMid << 8) | addrLow;
            addr += (m_x & (m_modeX ? 0xFF : 0xFFFF));
            
            // Write A to memory (handle 8-bit/16-bit modes)
            if (m_modeM) {
                m_memory->write8(addr & 0xFFFF, m_a & 0xFF);
            } else {
                m_memory->write8(addr & 0xFFFF, m_a & 0xFF);
                m_memory->write8((addr + 1) & 0xFFFF, (m_a >> 8) & 0xFF);
            }
        } break;
        
        
        // Additional 65C816 instructions for complete coverage
        
        // More Load/Store variants
        case 0xA1: { // LDA (Direct Page,X)
            uint8_t dp = m_memory->read8(m_address_plus_1);
            uint16_t addr = read16bit((dp + (m_x & 0xFF)) & 0xFF);
            if (m_modeM) {
                m_a = (m_a & 0xFF00) | m_memory->read8(addr);
                m_p = (m_p & ~0x82) | ((m_a & 0xFF) == 0 ? 0x02 : 0) | ((m_a & 0x80) ? 0x80 : 0);
            } else {
                m_a = read16bit(addr);
                m_p = (m_p & ~0x82) | (m_a == 0 ? 0x02 : 0) | ((m_a & 0x8000) ? 0x80 : 0);
            }
        } break;
        
        case 0xA7: { // LDA [Direct Page]
            uint8_t dp = m_memory->read8(m_address_plus_1);
            uint16_t addr = read16bit(dp);
            if (m_modeM) {
                m_a = (m_a & 0xFF00) | m_memory->read8(addr);
                m_p = (m_p & ~0x82) | ((m_a & 0xFF) == 0 ? 0x02 : 0) | ((m_a & 0x80) ? 0x80 : 0);
            } else {
                m_a = read16bit(addr);
                m_p = (m_p & ~0x82) | (m_a == 0 ? 0x02 : 0) | ((m_a & 0x8000) ? 0x80 : 0);
            }
        } break;
        
        case 0xB7: { // LDA [Direct Page],Y
            uint8_t dp = m_memory->read8(m_address_plus_1);
            uint16_t ptrAddr = (m_d + dp) & 0xFFFF;
            uint16_t baseAddr = read16bit(ptrAddr);
            uint16_t addr = (baseAddr + (m_y & 0xFFFF)) & 0xFFFF;
            if (m_modeM) {
                m_a = (m_a & 0xFF00) | m_memory->read8(addr);
                m_p = (m_p & ~0x82) | ((m_a & 0xFF) == 0 ? 0x02 : 0) | ((m_a & 0x80) ? 0x80 : 0);
            } else {
                m_a = read16bit(addr);
                m_p = (m_p & ~0x82) | (m_a == 0 ? 0x02 : 0) | ((m_a & 0x8000) ? 0x80 : 0);
            }
        } break;
        
        case 0xAF: { // LDA Long (24-bit address)
            uint8_t addrLow = m_memory->read8(m_address_plus_1);
            uint8_t addrMid = m_memory->read8(m_address_plus_1);
            uint8_t addrBank = m_memory->read8(m_address_plus_1);
            uint32_t addr = (addrBank << 16) | (addrMid << 8) | addrLow;
            if (m_modeM) {
                m_a = (m_a & 0xFF00) | m_memory->read8(addr);
                setZeroNegative8(m_a & 0xFF);
            } else {
                m_a = m_memory->read8(addr) | (m_memory->read8(addr + 1) << 8);
                setZeroNegative(m_a);
            }
        } break;
        
        case 0xBF: { // LDA Long,X
            uint8_t addrLow = m_memory->read8(m_address_plus_1);
            uint8_t addrMid = m_memory->read8(m_address_plus_1);
            uint8_t addrBank = m_memory->read8(m_address_plus_1);
            uint32_t addr = (addrBank << 16) | (addrMid << 8) | addrLow;
            addr += (m_x & (m_modeX ? 0xFF : 0xFFFF));
            if (m_modeM) {
                m_a = (m_a & 0xFF00) | m_memory->read8(addr & 0xFFFF);
                setZeroNegative8(m_a & 0xFF);
            } else {
                m_a = read16bit(addr & 0xFFFF);
                setZeroNegative(m_a);
            }
        } break;
        
        // Store variants
        case 0x81: { // STA (Direct Page,X)
            uint8_t dp = m_memory->read8(m_address_plus_1);
            uint16_t addr = read16bit((dp + (m_x & 0xFF)) & 0xFF);
            if (m_modeM) {
                m_memory->write8(addr, m_a & 0xFF);
            } else {
                write16bit(addr, m_a);
            }
        } break;
        
        case 0x87: { // STA [Direct Page]
            uint8_t dp = m_memory->read8(m_address_plus_1);
            uint16_t addr = read16bit(dp);
            if (m_modeM) {
                m_memory->write8(addr, m_a & 0xFF);
            } else {
                write16bit(addr, m_a);
            }
        } break;
        
        case 0x91: { // STA (Direct Page),Y
            uint8_t dp = m_memory->read8(m_address_plus_1);
            uint16_t addr = read16bit(dp) + (m_y & (m_modeX ? 0xFF : 0xFFFF));
            if (m_modeM) {
                m_memory->write8(addr, m_a & 0xFF);
            } else {
                write16bit(addr, m_a);
            }
        } break;
        
        case 0x92: { // STA (Direct Page)
            uint8_t dp = m_memory->read8(m_address_plus_1);
            uint16_t addr = read16bit(dp);
            if (m_modeM) {
                m_memory->write8(addr, m_a & 0xFF);
            } else {
                write16bit(addr, m_a);
            }
        } break;
        
        case 0x97: { // STA [Direct Page],Y
            uint8_t dp = m_memory->read8(m_address_plus_1);
            uint16_t addr = read16bit(dp) + (m_y & (m_modeX ? 0xFF : 0xFFFF));
            if (m_modeM) {
                m_memory->write8(addr, m_a & 0xFF);
            } else {
                write16bit(addr, m_a);
            }
        } break;
        
        // STX/STY variants
        case 0x82: { // BRL - Branch Always Long
            int16_t offset = m_memory->read16(m_address);
            m_pc += 2;
            m_pc += offset;
        } break;
        
        // More arithmetic
        case 0x61: { // ADC (Direct Page,X) - Indexed Indirect
            uint8_t dp = m_memory->read8(m_address_plus_1);
            
            // Calculate pointer address within Direct Page
            // In Direct Page, only low byte wraps around
            uint16_t xValue = m_modeX ? (m_x & 0xFF) : m_x;
            uint16_t pointerLow = (m_d & 0xFF) + dp + (xValue & 0xFF);
            uint16_t pointerAddr = (m_d & 0xFF00) | (pointerLow & 0xFF);
            
            // Read 16-bit pointer from Direct Page (bank 0)
            uint32_t ptrAddr = pointerAddr;
            uint16_t pointer = m_memory->read16(ptrAddr);
            
            // Read data from DBR:pointer
            uint32_t dataAddr = (m_dbr << 16) | pointer;
            
            if (m_modeM) {
                // 8-bit mode (m=1, m_modeM=true)
                uint8_t value = m_memory->read8(dataAddr);
                uint8_t a8 = m_a & 0xFF;
                uint16_t result = a8 + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFF);
                setOverflow((~(a8 ^ value) & (a8 ^ result) & 0x80) != 0);
                m_a = (m_a & 0xFF00) | (result & 0xFF); // A_H 보존
                setZeroNegative8(result);
            } else {
                // 16-bit mode (m=0, m_modeM=false)
                uint16_t value = m_memory->read16(dataAddr);
                uint32_t result = m_a + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFFFF);
                setOverflow((~(m_a ^ value) & (m_a ^ (uint16_t)result) & 0x8000) != 0); 
                m_a = result & 0xFFFF;
                setZeroNegative(result);
            }
        } break;
        
        case 0x63: { // ADC Stack Relative (sr,S)
            uint8_t offset = m_memory->read8(m_address_plus_1);
            
            // Stack Relative: SP + offset
            uint32_t stackAddr = m_sp + offset;
            
            if (m_modeM) {
                // 8-bit mode (m=1, m_modeM=true)
                uint8_t value = m_memory->read8(stackAddr);
                uint8_t a8 = m_a & 0xFF;
                uint16_t result = a8 + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFF);
                setOverflow((~(a8 ^ value) & (a8 ^ result) & 0x80) != 0);
                m_a = (m_a & 0xFF00) | (result & 0xFF);
                setZeroNegative8(result);
            } else {
                // 16-bit mode (m=0, m_modeM=false)
                uint16_t value = m_memory->read16(stackAddr);
                uint32_t result = m_a + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFFFF);
                setOverflow((~(m_a ^ value) & (m_a ^ (uint16_t)result) & 0x8000) != 0);
                m_a = result & 0xFFFF;
                setZeroNegative(result);
            }
        } break;
        
        case 0x65: { // ADC Direct Page
            uint8_t dp = m_memory->read8(m_address_plus_1);
            
            // Direct Page addressing: D + dp
            uint32_t dpAddr = m_d + dp;
            
            if (m_modeM) {
                // 8-bit mode (m=1, m_modeM=true)
                uint8_t value = m_memory->read8(dpAddr);
                uint8_t a8 = m_a & 0xFF;
                uint16_t result = a8 + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFF);
                setOverflow((~(a8 ^ value) & (a8 ^ result) & 0x80) != 0);
                m_a = (m_a & 0xFF00) | (result & 0xFF);
                setZeroNegative8(result);
            } else {
                // 16-bit mode (m=0, m_modeM=false)
                uint16_t value = m_memory->read16(dpAddr);
                uint32_t result = m_a + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFFFF);
                setOverflow((~(m_a ^ value) & (m_a ^ (uint16_t)result) & 0x8000) != 0);
                m_a = result & 0xFFFF;
                setZeroNegative(result);
            }
        } break;
        
        case 0x67: { // ADC [Direct Page] - Direct Page Indirect Long
            uint8_t dp = m_memory->read8(m_address_plus_1);
            
            // Read 24-bit pointer from Direct Page
            uint32_t ptrAddr = m_d + dp;
            uint8_t addrLow = m_memory->read8(ptrAddr);
            uint8_t addrHigh = m_memory->read8(ptrAddr + 1);
            uint8_t addrBank = m_memory->read8(ptrAddr + 2);
            uint32_t dataAddr = (addrBank << 16) | (addrHigh << 8) | addrLow;
            
            if (m_modeM) {
                // 8-bit mode (m=1, m_modeM=true)
                uint8_t value = m_memory->read8(dataAddr);
                uint8_t a8 = m_a & 0xFF;
                uint16_t result = a8 + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFF);
                setOverflow((~(a8 ^ value) & (a8 ^ result) & 0x80) != 0);
                m_a = (m_a & 0xFF00) | (result & 0xFF);
                setZeroNegative8(result);
            } else {
                // 16-bit mode (m=0, m_modeM=false)
                uint16_t value = m_memory->read16(dataAddr);
                uint32_t result = m_a + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFFFF);
                setOverflow((~(m_a ^ value) & (m_a ^ (uint16_t)result) & 0x8000) != 0);
                m_a = result & 0xFFFF;
                setZeroNegative(result);
            }
        } break;
                
        case 0x6D: { // ADC Absolute
            uint16_t addr = m_memory->read16(m_address);
            m_pc += 2;
            
            // Absolute addressing uses DBR:addr
            uint32_t fullAddr = (m_dbr << 16) | addr;
            
            if (m_modeM) {
                // 8-bit mode
                uint8_t value = m_memory->read8(fullAddr);
                uint8_t a8 = m_a & 0xFF;
                uint16_t result = a8 + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFF);
                setOverflow((~(a8 ^ value) & (a8 ^ result) & 0x80) != 0);
                m_a = (m_a & 0xFF00) | (result & 0xFF);
                setZeroNegative8(result);
            } else {
                // 16-bit mode
                uint16_t value = m_memory->read16(fullAddr);
                uint32_t result = m_a + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFFFF);
                setOverflow((~(m_a ^ value) & (m_a ^ (uint16_t)result) & 0x8000) != 0);
                m_a = result & 0xFFFF;
                setZeroNegative(result);
            }
        } break;
        
        case 0x6F: { // ADC Absolute Long
            // Read 24-bit address (3 bytes)
            uint8_t addrLow = m_memory->read8(m_address_plus_1);
            uint8_t addrMid = m_memory->read8(m_address_plus_1);
            uint8_t addrHigh = m_memory->read8(m_address_plus_1);
            
            uint32_t fullAddr = (addrHigh << 16) | (addrMid << 8) | addrLow;
            
            if (m_modeM) {
                // 8-bit mode
                uint8_t value = m_memory->read8(fullAddr);
                uint8_t a8 = m_a & 0xFF;
                uint16_t result = a8 + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFF);
                setOverflow((~(a8 ^ value) & (a8 ^ result) & 0x80) != 0);
                m_a = (m_a & 0xFF00) | (result & 0xFF);
                setZeroNegative8(result);
            } else {
                // 16-bit mode
                uint16_t value = m_memory->read16(fullAddr);
                uint32_t result = m_a + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFFFF);
                setOverflow((~(m_a ^ value) & (m_a ^ (uint16_t)result) & 0x8000) != 0);
                m_a = result & 0xFFFF;
                setZeroNegative(result);
            }
        } break;
        
        case 0x71: { // ADC (Direct Page),Y - Direct Page Indirect Indexed
            uint8_t dp = m_memory->read8(m_address_plus_1);
            
            // Read pointer from Direct Page
            uint32_t ptrAddr = m_d + dp;
            uint16_t pointer = m_memory->read16(ptrAddr);
            
            // Add Y index to pointer (can cross bank boundary)
            uint16_t yValue = m_modeX ? (m_y & 0xFF) : m_y;
            uint32_t dataAddr = (m_dbr << 16) + pointer + yValue;
            
            if (m_modeM) {
                // 8-bit mode
                uint8_t value = m_memory->read8(dataAddr);
                uint8_t a8 = m_a & 0xFF;
                uint16_t result = a8 + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFF);
                setOverflow((~(a8 ^ value) & (a8 ^ result) & 0x80) != 0);
                m_a = (m_a & 0xFF00) | (result & 0xFF);
                setZeroNegative8(result);
            } else {
                // 16-bit mode
                uint16_t value = m_memory->read16(dataAddr);
                uint32_t result = m_a + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFFFF);
                setOverflow((~(m_a ^ value) & (m_a ^ (uint16_t)result) & 0x8000) != 0);
                m_a = result & 0xFFFF;
                setZeroNegative(result);
            }
        } break;
        
        case 0x72: { // ADC (Direct Page) - Direct Page Indirect
            uint8_t dp = m_memory->read8(m_address_plus_1);
            
            // Read pointer from Direct Page
            uint32_t ptrAddr = m_d + dp;
            uint16_t pointer = m_memory->read16(ptrAddr);
            
            // Read data from DBR:pointer
            uint32_t dataAddr = (m_dbr << 16) | pointer;
            
            if (m_modeM) {
                // 8-bit mode
                uint8_t value = m_memory->read8(dataAddr);
                uint8_t a8 = m_a & 0xFF;
                uint16_t result = a8 + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFF);
                setOverflow((~(a8 ^ value) & (a8 ^ result) & 0x80) != 0);
                m_a = (m_a & 0xFF00) | (result & 0xFF);
                setZeroNegative8(result);
            } else {
                // 16-bit mode
                uint16_t value = m_memory->read16(dataAddr);
                uint32_t result = m_a + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFFFF);
                setOverflow((~(m_a ^ value) & (m_a ^ (uint16_t)result) & 0x8000) != 0);
                m_a = result & 0xFFFF;
                setZeroNegative(result);
            }
        } break;
        
        case 0x73: { // ADC (Stack Relative,S),Y - Stack Relative Indirect Indexed Y
            uint8_t offset = m_memory->read8(m_address_plus_1);
            
            // Read pointer from Stack (SP + offset)
            uint32_t stackAddr = m_sp + offset;
            uint16_t pointer = m_memory->read16(stackAddr);
            
            // Add Y index to pointer (can cross bank boundary)
            uint16_t yValue = m_modeX ? (m_y & 0xFF) : m_y;
            uint32_t dataAddr = (m_dbr << 16) + pointer + yValue;
            
            if (m_modeM) {
                // 8-bit mode
                uint8_t value = m_memory->read8(dataAddr);
                uint8_t a8 = m_a & 0xFF;
                uint16_t result = a8 + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFF);
                setOverflow((~(a8 ^ value) & (a8 ^ result) & 0x80) != 0);
                m_a = (m_a & 0xFF00) | (result & 0xFF);
                setZeroNegative8(result);
            } else {
                // 16-bit mode
                uint16_t value = m_memory->read16(dataAddr);
                uint32_t result = m_a + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFFFF);
                setOverflow((~(m_a ^ value) & (m_a ^ (uint16_t)result) & 0x8000) != 0);
                m_a = result & 0xFFFF;
                setZeroNegative(result);
            }
        } break;
        
        case 0x75: { // ADC Direct Page,X
            uint8_t dp = m_memory->read8(m_address_plus_1);
            
            // Direct Page,X addressing: D + dp + X
            uint16_t xValue = m_modeX ? (m_x & 0xFF) : m_x;
            uint32_t dpAddr = m_d + dp + xValue;
            
            if (m_modeM) {
                // 8-bit mode
                uint8_t value = m_memory->read8(dpAddr);
                uint8_t a8 = m_a & 0xFF;
                uint16_t result = a8 + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFF);
                setOverflow((~(a8 ^ value) & (a8 ^ result) & 0x80) != 0);
                m_a = (m_a & 0xFF00) | (result & 0xFF);
                setZeroNegative8(result);
            } else {
                // 16-bit mode
                uint16_t value = m_memory->read16(dpAddr);
                uint32_t result = m_a + value + (isCarry() ? 1 : 0);
                
                setCarry(result > 0xFFFF);
                setOverflow((~(m_a ^ value) & (m_a ^ (uint16_t)result) & 0x8000) != 0);
                m_a = result & 0xFFFF;
                setZeroNegative(result);
            }
        } break;
        
        case 0xE1: { // SBC (Direct Page,X)
            uint8_t dp = m_memory->read8(m_address_plus_1);
            uint16_t addr = read16bit((dp + (m_x & 0xFF)) & 0xFF);
            if (m_modeM) {
                uint8_t value = m_memory->read8(addr);
                uint16_t result = (m_a & 0xFF) - value - (isCarry() ? 0 : 1);
                setCarry(result < 0x100);
                m_a = (m_a & 0xFF00) | (result & 0xFF);
                setZeroNegative8(result & 0xFF);
            } else {
                uint16_t value = read16bit(addr);
                uint32_t result = m_a - value - (isCarry() ? 0 : 1);
                setCarry(result < 0x10000);
                m_a = result & 0xFFFF;
                setZeroNegative(m_a);
            }
        } break;
        
        case 0xED: { // SBC Absolute
            uint16_t addr = m_memory->read16(m_address);
            m_pc += 2;
            if (m_modeM) {
                uint8_t value = m_memory->read8(addr);
                uint16_t result = (m_a & 0xFF) - value - (isCarry() ? 0 : 1);
                setCarry(result < 0x100);
                m_a = (m_a & 0xFF00) | (result & 0xFF);
                setZeroNegative8(result & 0xFF);
            } else {
                uint16_t value = read16bit(addr);
                uint32_t result = m_a - value - (isCarry() ? 0 : 1);
                setCarry(result < 0x10000);
                m_a = result & 0xFFFF;
                setZeroNegative(m_a);
            }
        } break;
        
        // Jump/Branch variants
        case 0x5C: { // JMP Long (24-bit)
            uint8_t addrLow = m_memory->read8(m_address_plus_1);
            uint8_t addrMid = m_memory->read8(m_address_plus_1);
            uint8_t addrBank = m_memory->read8(m_address_plus_1);
            m_pc = (addrMid << 8) | addrLow;
            m_pbr = addrBank;
        } break;
        
        case 0x6B: { // RTL - Return from Subroutine Long
            stackTrace();
            m_pc = pullStack16() + 1;
            m_pbr = pullStack();
            stackTrace();
        } break;
        
        case 0x22: { // JSL - Jump to Subroutine Long
            stackTrace();
            uint8_t addrLow = m_memory->read8(m_address_plus_1);
            uint8_t addrMid = m_memory->read8(m_address_plus_1);
            uint8_t addrBank = m_memory->read8(m_address_plus_1);
            
            // Push PBR and return address
            pushStack(m_pbr);
            pushStack16(m_pc - 1);
            m_pc = (addrMid << 8) | addrLow;
            m_pbr = addrBank;
            stackTrace();
        } break;
        
        case 0xDC: { // JMP [Absolute]
            uint16_t addr = m_memory->read16(m_address);
            m_pc = read16bit(addr);
        } break;
        
        case 0x7C: { // JMP (Absolute,X)
            uint16_t addr = m_memory->read16(m_address) + (m_x & (m_modeX ? 0xFF : 0xFFFF));
            m_pc = read16bit(addr);
        } break;
        
        // Block move instructions
        case 0x44: { // MVP - Block Move Previous
            uint8_t destBank = m_memory->read8(m_address_plus_1);
            uint8_t srcBank = m_memory->read8(m_address_plus_1);
            // Simplified: just skip for now
            if (m_a != 0xFFFF) {
                m_a--;
                m_x--;
                m_y--;
                m_pc -= 3; // Repeat instruction
            }
        } break;
        
        case 0x54: { // MVN - Block Move Next
            uint8_t destBank = m_memory->read8(m_address_plus_1);
            uint8_t srcBank = m_memory->read8(m_address_plus_1);
            // Simplified: just skip for now
            if (m_a != 0xFFFF) {
                m_a--;
                m_x++;
                m_y++;
                m_pc -= 3; // Repeat instruction
            }
        } break;
        
        // Processor status
        case 0x02: { // COP - Co-processor
            m_pc++; // Skip signature byte
            // Push PC and P
            pushStack16(m_pc);
            pushStack(m_p);
            m_p |= 0x04; // Set I flag
            
            // COP vector: 0xFFF4-0xFFF5 (emulation) or 0xFFE4-0xFFE5 (native)
            uint16_t vectorAddr = m_emulationMode ? 0xFFF4 : 0xFFE4;
            uint16_t copVector = m_memory->read16(vectorAddr);
            
            // Debug: Print COP vector information
            static int copCount = 0;
            if (copCount < 5) {
                std::cout << "COP triggered! Mode=" << (m_emulationMode ? "Emulation" : "Native") 
                          << ", VectorAddr=0x" << std::hex << vectorAddr 
                          << ", Vector=0x" << copVector 
                          << ", PC=0x" << copVector 
                          << ", PBR=0x" << (int)m_pbr << std::dec << std::endl;
                copCount++;
            }
            
            m_pc = copVector;
            stackTrace();
        } break;
        
        case 0x00: { // BRK
            m_pc++;
            // Push PBR, PC, and P with B flag set
            pushStack(m_pbr);
            pushStack16(m_pc);
            pushStack(m_p | 0x10);
            m_p |= 0x04; // Set I flag
            m_pbr = 0x00; // Clear program bank
            // BRK vector: 0xFFFE-0xFFFF (emulation) or 0xFFE6-0xFFE7 (native)
            uint16_t vectorAddr = m_emulationMode ? 0xFFFE : 0xFFE6;
            uint16_t brkVector = m_memory->read16(vectorAddr);
            
            // Debug: Print BRK vector information
            static int brkCount = 0;
            if (brkCount < 5) {
                std::cout << "BRK triggered! Mode=" << (m_emulationMode ? "Emulation" : "Native") 
                          << ", VectorAddr=0x" << std::hex << vectorAddr 
                          << ", Vector=0x" << brkVector 
                          << ", PC=0x" << brkVector 
                          << ", PBR=0x" << (int)m_pbr << std::dec << std::endl;
                brkCount++;
            }
            
            m_pc = brkVector;
            stackTrace();
        } break;
        
        case 0xDB: { // STP - Stop Processor
            // Halt execution - keep PC at current position
            // This will cause the CPU to repeatedly execute STP
            // In a real implementation, this would halt the CPU until reset
        } break;
        
        case 0xCB: { // WAI - Wait for Interrupt
            // Wait for interrupt - for now, just continue
        } break;
        
        // Additional useful instructions
        case 0x42: { // WDM - Reserved
            m_pc++; // Skip signature byte
        } break;
        
        case 0x4B: { // PHK - Push Program Bank
            pushStack(m_pbr);
            stackTrace();
        } break;
        
        case 0x0B: { // PHD - Push Direct Page
            pushStack16(m_d);
            stackTrace();
        } break;
        
        case 0x2B: { // PLD - Pull Direct Page
            m_d = pullStack16();
            setZeroNegative(m_d);
            stackTrace();
        } break;
        
        case 0xF4: { // PEA - Push Effective Absolute
            uint16_t value = m_memory->read16(m_address);
            m_pc += 2;
            pushStack16(value);
            stackTrace();
        } break;
        
        case 0xD4: { // PEI - Push Effective Indirect
            uint8_t dp = m_memory->read8(m_address_plus_1);
            uint16_t value = read16bit(dp);
            pushStack16(value);
            stackTrace();
        } break;
        
        case 0x62: { // PER - Push Effective PC Relative
            int16_t offset = m_memory->read16(m_address);
            m_pc += 2;
            uint16_t value = m_pc + offset;
            pushStack16(value);
            stackTrace();
        } break;
        
        
        default:
            // Silently ignore unknown opcodes to reduce console spam
            break;
    }
}

void CPU::NOP() {
    // Do nothing
}

void CPU::LDA_Immediate() {
    m_a = m_memory->read8(m_address_plus_1);
}

void CPU::STA_Absolute() {
    uint16_t addr = m_memory->read16(m_address);
    m_pc += 2;
    m_memory->write8(addr, m_a & 0xFF);
}

void CPU::JMP_Absolute() {
    m_pc = m_memory->read16(m_address);
}
