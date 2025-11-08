#include "apu.h"
#include "../cpu/cpu.h"
#include "../debug/logger.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <vector>
#ifdef USE_SDL
#include <SDL.h>
#endif

APU::APU() 
    : m_cpu(nullptr)
    , m_ready(false)
    , m_initialized(false)
    , m_bootComplete(false)
    , m_bootStep(0)
    , m_spc700Cycles(0)
    , m_dspAddr(0)
    , m_dspEnabled(false)
    , m_bootState(BOOT_IDLE)
    , m_spcLoadState(SPC_LOAD_IDLE)
    , m_spcLoadAddr(0)
    , m_spcLoadSize(0)
    , m_spcLoadIndex(0)
    , m_spcExecAddr(0)
#ifdef USE_SDL
    , m_audioDevice(0)
    , m_audioSpec(nullptr)
#endif
{
    
    // Initialize APU ports
    for (int i = 0; i < 4; i++) {
        m_ports[i] = 0x00;
    }
    
    // Initialize 64KB ARAM
    m_aram.resize(64 * 1024, 0);
    
    // Initialize DSP registers
    memset(m_dspRegs, 0, sizeof(m_dspRegs));
    
    // Initialize audio channels
    for (int i = 0; i < 8; i++) {
        m_channels[i] = AudioChannel();
    }
    
    // Initialize audio buffer (32kHz sample rate, stereo, 1024 samples)
    m_audioBuffer.resize(2048, 0);
    m_audioBufferPos = 0;
    
    // Load boot ROM
    loadBootROM();
}

APU::~APU() {
#ifdef USE_SDL
    // Close SDL audio device
    if (m_audioDevice != 0) {
        SDL_CloseAudioDevice(m_audioDevice);
    }
    
    // Free audio spec
    if (m_audioSpec != nullptr) {
        delete m_audioSpec;
    }
#endif
}

void APU::cleanup() {
#ifdef USE_SDL
    if (m_audioDevice != 0) {
        SDL_CloseAudioDevice(m_audioDevice);
        m_audioDevice = 0;
    }
    
    if (m_audioSpec != nullptr) {
        delete m_audioSpec;
        m_audioSpec = nullptr;
    }
#endif
}

bool APU::initAudio() {
#ifdef USE_SDL
    // Initialize SDL audio subsystem
    if (SDL_Init(SDL_INIT_AUDIO) < 0) {
        std::cerr << "Failed to initialize SDL audio: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Set up SDL audio specification
    SDL_AudioSpec desired;
    SDL_zero(desired);
    desired.freq = 32000;      // 32kHz sample rate (SNES uses 32kHz)
    desired.format = AUDIO_S16; // 16-bit signed audio
    desired.channels = 2;       // Stereo
    desired.samples = 1024;     // Buffer size
    desired.callback = audioCallback;
    desired.userdata = this;
    
    // Allocate audio spec
    m_audioSpec = new SDL_AudioSpec();
    
    // Open audio device
    m_audioDevice = SDL_OpenAudioDevice(nullptr, 0, &desired, m_audioSpec, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    
    if (m_audioDevice == 0) {
        std::cerr << "Failed to open audio device: " << SDL_GetError() << std::endl;
        delete m_audioSpec;
        m_audioSpec = nullptr;
        return false;
    }
    
    std::cout << "APU: Audio initialized - " << m_audioSpec->freq << "Hz, " 
              << (int)m_audioSpec->channels << " channels, " 
              << m_audioSpec->samples << " samples buffer" << std::endl;
    
    // Start audio playback
    SDL_PauseAudioDevice(m_audioDevice, 0);
    std::cout << "APU: Audio playback started (device unpaused)" << std::endl;
    
    return true;
#else
    // SDL not available, just initialize audio buffer
    std::cout << "APU: Audio buffer initialized (SDL not available)" << std::endl;
    return true;
#endif
}

void APU::reset() {
    m_ready = false;
    m_initialized = false;
    m_bootComplete = false;
    m_bootStep = 0;
    m_bootState = BOOT_IDLE;
    m_spcLoadState = SPC_LOAD_IDLE;
    m_spcLoadAddr = 0;
    m_spcLoadSize = 0;
    m_spcLoadIndex = 0;
    m_spcExecAddr = 0;
    
    // Reset SPC700 registers
    m_regs.a = 0;
    m_regs.x = 0;
    m_regs.y = 0;
    m_regs.sp = 0;
    m_regs.pc = 0;
    m_regs.psw = 0;
    
    // Reset ports
    for (int i = 0; i < 4; i++) {
        m_ports[i] = 0x00;
    }
    
    // Reset timers
    for (int i = 0; i < 3; i++) {
        m_timers[i] = Timer();
    }
    
    // Reset DSP
    m_dspEnabled = false;
    memset(m_dspRegs, 0, sizeof(m_dspRegs));
    
    // Initialize DSP registers to default values
    // Master Volume Left/Right (signed, default to max)
    m_dspRegs[0x0C] = 0x7F; // MVOL L
    m_dspRegs[0x1C] = 0x7F; // MVOL R
    
    // Echo Volume Left/Right (signed, default to 0)
    m_dspRegs[0x2C] = 0x00; // EVOL L
    m_dspRegs[0x3C] = 0x00; // EVOL R
    
    // Sample Directory Address (default to page 0)
    m_dspRegs[0x5D] = 0x00; // DIR
    
    // Echo Start Address (default to page 0)
    m_dspRegs[0x5E] = 0x00; // ESA
    
    // Reset audio channels
    for (int i = 0; i < 8; i++) {
        m_channels[i] = AudioChannel();
    }
    
    // Reload boot ROM
    loadBootROM();
}

void APU::step() {
    // Debug: Check if APU step is being called
    static int stepCount = 0;
    static bool firstLog = true;
    if (firstLog) {
        std::cout << "APU: step() called for the first time" << std::endl;
        std::cout << "APU: PC=0x" << std::hex << m_regs.pc << ", bootComplete=" << m_bootComplete << std::dec << std::endl;
        firstLog = false;
    }
    stepCount++;
    
    // Check if boot is complete by monitoring port values
    if (!m_bootComplete && m_ports[0] == 0xAA && m_ports[1] == 0xBB) {
        m_bootComplete = true;
        m_ready = true;
        m_initialized = true;
        std::cout << "APU: Boot completed after " << stepCount << " steps - Ready signature detected (0xBBAA)" << std::endl;
        // After boot, set ports to 0xBBAA for IPL protocol
        m_ports[0] = 0xAA;
        m_ports[1] = 0xBB;
    }
    
    // Execute SPC700 instructions - continue executing even after boot
    // This allows SPC programs to run
    if (m_bootComplete && m_spcLoadState == SPC_LOAD_COMPLETE) {
        // Execute SPC700 instructions when program is loaded
        executeSPC700Instruction();
    } else if (!m_bootComplete) {
        // Execute IPL ROM during boot
        executeSPC700Instruction();
    }
    
    // Update timers
    updateTimers();
    
    
    if (m_bootComplete) {
        updateDSP();
    }
    
    // Update overall state
    updateState();
}

uint8_t APU::readPort(uint8_t port) {
    if (port >= 4) return 0x00;
    
    static int readCount = 0;
    readCount++;
    
    uint8_t value = m_ports[port];
    
    // Always log reads after SPC_LOAD_COMPLETE to debug port communication
    if (m_spcLoadState == SPC_LOAD_COMPLETE || readCount <= 50) {
        std::cout << "APU read $" << std::hex << (0x2140 + port) 
                  << " = 0x" << (int)value << " (bootComplete=" << m_bootComplete 
                  << ", loadState=" << (int)m_spcLoadState << ")" << std::dec << std::endl;
    }
    
    // Handle IPL protocol handshake
    if (m_bootComplete && port == 0) {
        // During IPL protocol, port 0 is used for handshake
        // CPU reads to confirm APU echoed back the value
        if (m_spcLoadState == SPC_LOAD_IDLE) {
            // Return 0xBBAA for initial handshake
            uint16_t bbaa = (m_ports[1] << 8) | m_ports[0];
            if (bbaa == 0xBBAA) {
                return m_ports[0]; // Return 0xAA
            }
        }
    }
    
    // After boot is complete, APU automatically echoes back what CPU wrote
    // This simulates the SNES IPL handshake protocol where:
    // 1. CPU writes a value to port 0
    // 2. APU processes it and writes the same value back
    // 3. CPU reads and confirms the value matches
    return value;
}

void APU::writePort(uint8_t port, uint8_t value) {
    if (port >= 4) return;
    
    static int writeCount = 0;
    writeCount++;
    
    if (writeCount <= 50) {
        std::cout << "APU write $" << std::hex << (0x2140 + port) 
                  << " = 0x" << (int)value << " (bootComplete=" << m_bootComplete 
                  << ", loadState=" << (int)m_spcLoadState << ")" << std::dec << std::endl;
    }
    
    // Store the written value (only during IPL protocol)
    // After IPL protocol completes, SPC can write to ports via $F4-$F7
    // CPU can still write to ports 1-3 after IPL protocol completes (for test communication)
    // Port 0 is controlled by SPC after IPL protocol completes
    if (m_spcLoadState != SPC_LOAD_COMPLETE) {
        m_ports[port] = value;
    } else if (port >= 1 && port <= 3) {
        // After IPL protocol completes, allow CPU to write to ports 1-3 for communication
        m_ports[port] = value;
        // Always log writes to ports 1-3 after IPL complete for debugging
        std::cout << "APU: CPU wrote to port " << (int)port << " = 0x" << std::hex << (int)value << " (after IPL complete)" << std::dec << std::endl;
    }
    
    // Handle IPL protocol for SPC program loading
    if (m_bootComplete) {
        if (port == 2) {
            // Port 2: Low byte of address
            // This can be written before or during protocol
            if (m_spcLoadState == SPC_LOAD_IDLE || m_spcLoadState == SPC_LOAD_WAIT_CC) {
                // Store low byte, will be combined with high byte later
                m_spcLoadAddr = value;
            } else if (m_spcLoadState == SPC_LOAD_WAIT_EXEC || m_spcLoadState == SPC_LOAD_RECEIVING) {
                // Execution address low byte (can be written during or after data transfer)
                m_spcExecAddr = value;
                std::cout << "APU: Execution address low byte: 0x" << std::hex << (int)value << std::dec << std::endl;
            }
        } else if (port == 3) {
            // Port 3: High byte of address
            if (m_spcLoadState == SPC_LOAD_IDLE || m_spcLoadState == SPC_LOAD_WAIT_CC) {
                // Combine with low byte to get full destination address
                m_spcLoadAddr |= (value << 8);
                std::cout << "APU: Destination address set to 0x" << std::hex << m_spcLoadAddr << std::dec << std::endl;
            } else if (m_spcLoadState == SPC_LOAD_WAIT_EXEC || m_spcLoadState == SPC_LOAD_RECEIVING) {
                // Execution address high byte (can be written during or after data transfer)
                m_spcExecAddr |= (value << 8);
                std::cout << "APU: Execution address set to 0x" << std::hex << m_spcExecAddr << std::dec << std::endl;
                // If we were in RECEIVING state and exec address is now complete, switch to WAIT_EXEC
                if (m_spcLoadState == SPC_LOAD_RECEIVING) {
                    m_spcLoadState = SPC_LOAD_WAIT_EXEC;
                }
            }
        } else if (port == 0) {
            // Port 0: Handshake and data transfer
            if (m_spcLoadState == SPC_LOAD_IDLE) {
                // Wait for CPU to write 0xCC to start transfer
                if (value == 0xCC) {
                    m_spcLoadState = SPC_LOAD_WAIT_CC;
                    // Echo back 0xCC
                    m_ports[0] = 0xCC;
                    std::cout << "APU: IPL protocol started, destination address=0x" << std::hex << m_spcLoadAddr << std::dec << std::endl;
                }
            } else if (m_spcLoadState == SPC_LOAD_WAIT_CC) {
                // Should have already echoed 0xCC, but echo again if needed
                m_ports[0] = 0xCC;
            } else if (m_spcLoadState == SPC_LOAD_RECEIVING) {
                // Data transfer: CPU writes byte index to port 0
                // We echo it back to acknowledge
                m_ports[0] = value;
                
                // Check if this might be the execution command
                // After data transfer, CPU writes (size+2) to port 0
                // If execution address has been set (via port 2/3), this is the execution command
                if (m_spcExecAddr != 0 && value >= m_spcLoadSize) {
                    // This is likely the execution command (size+2)
                    m_spcLoadState = SPC_LOAD_WAIT_EXEC;
                }
            } else if (m_spcLoadState == SPC_LOAD_WAIT_EXEC) {
                // Check if execution address has been set
                if (m_spcExecAddr != 0) {
                    // Echo back execution command
                    m_ports[0] = value;
                    // Start executing SPC program
                    m_regs.pc = m_spcExecAddr;
                    m_spcLoadState = SPC_LOAD_COMPLETE;
                    std::cout << "APU: SPC program loaded, starting execution at PC=0x" << std::hex << m_regs.pc << std::dec << std::endl;
                } else {
                    // Still waiting for execution address, just echo
                    m_ports[0] = value;
                }
            }
        } else if (port == 1) {
            // Port 1: Data byte during transfer
            if (m_spcLoadState == SPC_LOAD_WAIT_CC) {
                // First data byte after 0xCC handshake
                // Start receiving data
                m_spcLoadState = SPC_LOAD_RECEIVING;
                m_spcLoadIndex = 0;
                m_spcLoadSize = 0; // We don't know size yet, will track by data received
            }
            
            if (m_spcLoadState == SPC_LOAD_RECEIVING) {
                // Write data byte to ARAM
                uint16_t addr = m_spcLoadAddr + m_spcLoadIndex;
                writeARAM(addr, value);
                m_spcLoadIndex++;
                m_spcLoadSize = m_spcLoadIndex; // Track size as we receive
                
                // Log first 50 bytes loaded
                if (m_spcLoadIndex <= 50) {
                    std::cout << "APU: Loaded byte[" << std::dec << (m_spcLoadIndex - 1) 
                              << "] = 0x" << std::hex << (int)value 
                              << " to ARAM[0x" << addr << "]" << std::dec << std::endl;
                }
                
                // Note: We don't detect end of data transfer here
                // The CPU will write execution address to port 2/3 after data transfer
                // We detect the end when port 0 receives (size+2) value
            }
        }
    }
    
    // Simplified boot sequence handling
    if (!m_bootComplete) {
        // Super Mario World writes 0xAA, 0xBB, 0xCC, 0xDD to ports 0-3
        // We just acknowledge these writes
        if (value == 0xAA || value == 0xBB || value == 0xCC || value == 0xDD) {
            std::cout << "APU: Received boot value 0x" << std::hex << (int)value << " on port " << (int)port << std::dec << std::endl;
        }
    }
}

void APU::updateState() {
    // Update APU state based on current conditions
    if (m_bootComplete && m_initialized) {
        m_ready = true;
    }
}

// Helper function to get instruction operand length
static int getSPC700OperandLength(uint8_t opcode) {
    // Return operand length in bytes (excluding opcode)
    switch (opcode) {
        // 0 operands
        case 0x00: case 0xEF: case 0xFF: // NOP, SLEEP, BRK
        case 0x6F: case 0x7F: // RET, RETI
        case 0xBC: case 0x3D: case 0xFC: case 0xC8: // INC A/X/Y
        case 0x9C: case 0x1D: case 0xDC: // DEC A/X/Y
        case 0x7D: case 0xDD: case 0xFD: // MOV A,X/Y and MOV Y,A
        case 0x3C: case 0x7C: case 0x1C: case 0x5C: // ROL/ROR/ASL/LSR A
        case 0x2D: case 0x4D: case 0x6D: case 0x0D: // PUSH
        case 0xAE: case 0xCE: case 0xEE: case 0x8E: // POP
        case 0x20: case 0x40: case 0x60: case 0x80: case 0xA0: case 0xC0: case 0xE0: // CLRP, SETP, CLRC, SETC, EI, DI, CLRV
        case 0xED: case 0x9D: case 0xBD: case 0x9E: case 0x9F: case 0xBE: case 0xCF: case 0xDF: // NOTC, MOV X,SP, MOV SP,X, DIV, XCN, DAS, MUL, DAA
        case 0xBF: // MOV A,(X)+
            return 0;
        
        // 1 byte operand (immediate, direct page, relative)
        case 0xE8: case 0xCD: // MOV A/X,#imm (0x8D is duplicate, removed)
        case 0xC4: case 0xD8: case 0xCB: // MOV dp,A/X/Y
        case 0xE4: case 0xF8: case 0xEB: // MOV A/X/Y,dp
        case 0x88: case 0xA8: case 0x28: case 0x08: case 0x48: // ADC/SBC/AND/OR/EOR A,#imm
        case 0x84: case 0xA4: case 0x24: case 0x04: case 0x44: // ADC/SBC/AND/OR/EOR A,dp
        case 0x68: case 0x64: case 0x3E: // CMP (0x7E is CMP Y,dp - 1 byte, 0xAD is duplicate, removed)
        case 0x2F: case 0xF0: case 0xD0: case 0x90: case 0xB0: case 0x30: case 0x10: case 0x50: case 0x70: // Branch instructions
        case 0xFA: // MOV dp1,dp2
        case 0x3A: case 0x5A: case 0x7A: case 0x9A: case 0xBA: case 0xDA: // Word operations
        case 0x78: case 0x12: // SET1/CLR1 dp.bit
        case 0x03: case 0x13: case 0x23: case 0x33: case 0x43: case 0x63: case 0x73: case 0x83: case 0xA3: case 0xC3: case 0xD3: case 0xE3: case 0xF3: // BBS/BBC dp.bit,rel
        case 0x2E: case 0x6E: case 0xDE: // CBNE/DBNZ dp,rel and CBNE dp+X,rel
        case 0x46: // EOR A,(X) (0x56, 0x66, 0x76 are !abs+Y or !abs+X, moved to 2-byte)
        case 0x05: case 0x06: case 0x07: case 0x09: case 0x0B: case 0x14: case 0x17: case 0x18: case 0x19: case 0x1A: case 0x1B: // Various operations (0x15, 0x16 are !abs+X/Y, moved to 2-byte)
        case 0x25: case 0x26: case 0x27: case 0x29: case 0x2B: case 0x35: case 0x37: case 0x38: case 0x39: case 0x3B: // Various operations (0x2C is ROL abs - 2 bytes, 0x36 is !abs+Y - 2 bytes, moved to 2-byte)
        case 0x45: case 0x47: case 0x49: case 0x4B: case 0x54: case 0x57: case 0x58: case 0x59: case 0x5B: // Various operations (0x4C is LSR abs - 2 bytes, 0x55, 0x56 are !abs+X/Y - 2 bytes, moved to 2-byte)
        case 0x65: case 0x67: case 0x69: case 0x6B: case 0x74: case 0x77: case 0x79: case 0x7B: // Various operations (0x6C, 0x66, 0x75, 0x76 are abs or !abs+X/Y, moved to 2-byte)
        case 0x7E: // CMP Y,dp - 1 byte operand
        case 0x85: case 0x86: case 0x87: case 0x89: case 0x8B: case 0x8C: case 0x94: case 0x97: case 0x99: case 0x9B: // Various operations (0x95, 0x96 are !abs+X/Y, moved to 2-byte)
        case 0xA5: case 0xA6: case 0xA7: case 0xA9: case 0xAB: case 0xAC: case 0xB4: case 0xB7: case 0xB9: case 0xBB: // Various operations (0xB5, 0xB6 are !abs+X/Y, moved to 2-byte)
        case 0xC5: case 0xC6: case 0xC7: case 0xC9: case 0xCC: case 0xD4: case 0xD7: case 0xD9: case 0xDB: // Various operations (0xD5, 0xD6 are !abs+X/Y, moved to 2-byte)
        case 0xE5: case 0xE6: case 0xE7: case 0xE9: case 0xEC: case 0xF4: case 0xF7: case 0xF9: case 0xFB: // Various operations (0xF5, 0xF6 are !abs+X/Y, moved to 2-byte)
        case 0x0A: case 0x2A: case 0x4A: case 0x6A: case 0x8A: case 0xAA: case 0xCA: case 0xEA: // Bit operations
            return 1;
        
        // 2 byte operand (immediate+dp, abs, !abs+X/Y)
        case 0x8F: // MOV dp,#imm
        case 0x3F: case 0x5F: // CALL/JMP abs
        case 0x0C: case 0x0E: case 0x1E: case 0x1F: case 0x2C: case 0x4C: case 0x4E: case 0x5E: case 0x6C: // Various abs operations (0x7E is CMP Y,dp - 1 byte, removed)
        // !abs+X/Y addressing mode - 2 bytes for absolute address
        case 0x15: case 0x16: // OR A,!abs+X/Y
        case 0x36: case 0x56: // AND/EOR A,!abs+Y
        case 0x55: // EOR A,!abs+X
        case 0x66: case 0x75: case 0x76: // OR/CMP A,!abs+X/Y
        case 0x95: case 0x96: // ADC A,!abs+X/Y
        case 0xB5: case 0xB6: // SBC A,!abs+X/Y
        case 0xD5: case 0xD6: // MOV !abs+X/Y,A
        case 0xF5: case 0xF6: // MOV A,!abs+X/Y
            return 2;
        
        default:
            return 0; // Unknown opcode, assume 0 operands
    }
}

void APU::executeSPC700Instruction() {
    // Execute SPC700 instruction
    uint16_t savedPC = m_regs.pc;
    uint8_t opcode = readARAM(m_regs.pc);
    uint8_t savedA = m_regs.a;
    uint8_t savedX = m_regs.x;
    uint8_t savedY = m_regs.y;
    uint8_t savedSP = m_regs.sp;
    uint8_t savedPSW = m_regs.psw;
    
    // Get operand length and read operands for logging
    int operandLength = getSPC700OperandLength(opcode);
    std::vector<uint8_t> operands;
    for (int i = 0; i < operandLength; i++) {
        operands.push_back(readARAM(m_regs.pc + 1 + i));
    }
    
    m_regs.pc++;
    
    // Get opcode name (simplified - just show opcode hex for now)
    std::string opcodeName = "UNKNOWN";
    switch (opcode) {
        case 0x00: opcodeName = "NOP"; break;
        case 0x8F: opcodeName = "MOV dp,#imm"; break;
        case 0xE8: opcodeName = "MOV A,#imm"; break;
        case 0xCD: opcodeName = "MOV X,#imm"; break;
        case 0x8D: opcodeName = "MOV Y,#imm"; break;
        case 0xC4: opcodeName = "MOV dp,A"; break;
        case 0xD8: opcodeName = "MOV dp,X"; break;
        case 0xCB: opcodeName = "MOV dp,Y"; break;
        case 0xE4: opcodeName = "MOV A,dp"; break;
        case 0xF8: opcodeName = "MOV X,dp"; break;
        case 0xEB: opcodeName = "MOV Y,dp"; break;
        case 0x7D: opcodeName = "MOV A,X"; break;
        case 0xDD: opcodeName = "MOV A,Y"; break;
        case 0xFD: opcodeName = "MOV Y,A"; break;
        case 0xFA: opcodeName = "MOV dp1,dp2"; break;
        case 0xBC: opcodeName = "INC A"; break;
        case 0x3D: opcodeName = "INC X"; break;
        case 0xFC: opcodeName = "INC Y"; break;
        case 0xC8: opcodeName = "INC Y"; break;
        case 0x9C: opcodeName = "DEC A"; break;
        case 0x1D: opcodeName = "DEC X"; break;
        case 0xDC: opcodeName = "DEC Y"; break;
        case 0x88: opcodeName = "ADC A,#imm"; break;
        case 0x84: opcodeName = "ADC A,dp"; break;
        case 0xA8: opcodeName = "SBC A,#imm"; break;
        case 0xA4: opcodeName = "SBC A,dp"; break;
        case 0x28: opcodeName = "AND A,#imm"; break;
        case 0x24: opcodeName = "AND A,dp"; break;
        case 0x08: opcodeName = "OR A,#imm"; break;
        case 0x04: opcodeName = "OR A,dp"; break;
        case 0x48: opcodeName = "EOR A,#imm"; break;
        case 0x44: opcodeName = "EOR A,dp"; break;
        case 0x46: opcodeName = "EOR A,(X)"; break;
        case 0x56: opcodeName = "EOR A,!abs+Y"; break;
        case 0x34: opcodeName = "AND A,!abs+X"; break;
        case 0x59: opcodeName = "EOR X,dp"; break;
        case 0x68: opcodeName = "CMP A,#imm"; break;
        case 0x64: opcodeName = "CMP A,dp"; break;
        case 0x3E: opcodeName = "CMP X,dp"; break;
        case 0x7E: opcodeName = "CMP Y,dp"; break;
        case 0xAD: opcodeName = "CMP Y,#imm"; break;
        case 0x2F: opcodeName = "BRA rel"; break;
        case 0xF0: opcodeName = "BEQ rel"; break;
        case 0xD0: opcodeName = "BNE rel"; break;
        case 0x90: opcodeName = "BCC rel"; break;
        case 0xB0: opcodeName = "BCS rel"; break;
        case 0x30: opcodeName = "BMI rel"; break;
        case 0x10: opcodeName = "BPL rel"; break;
        case 0x50: opcodeName = "BVC rel"; break;
        case 0x70: opcodeName = "BVS rel"; break;
        case 0x2D: opcodeName = "PUSH A"; break;
        case 0x4D: opcodeName = "PUSH X"; break;
        case 0x6D: opcodeName = "PUSH Y"; break;
        case 0x0D: opcodeName = "PUSH PSW"; break;
        case 0xAE: opcodeName = "POP A"; break;
        case 0xCE: opcodeName = "POP X"; break;
        case 0xEE: opcodeName = "POP Y"; break;
        case 0x8E: opcodeName = "POP PSW"; break;
        case 0x3F: opcodeName = "CALL abs"; break;
        case 0x6F: opcodeName = "RET"; break;
        case 0x7F: opcodeName = "RETI"; break;
        case 0x5F: opcodeName = "JMP abs"; break;
        case 0x3C: opcodeName = "ROL A"; break;
        case 0x7C: opcodeName = "ROR A"; break;
        case 0x1C: opcodeName = "ASL A"; break;
        case 0x5C: opcodeName = "LSR A"; break;
        case 0x3A: opcodeName = "INCW dp"; break;
        case 0x5A: opcodeName = "CMPW YA,dp"; break;
        case 0x78: opcodeName = "SET1 dp.bit"; break;
        case 0x12: opcodeName = "CLR1 dp.bit"; break;
        case 0x01: opcodeName = "TCALL 0"; break;
        case 0x03: opcodeName = "BBS dp.bit,rel"; break;
        case 0x11: opcodeName = "TCALL 1"; break;
        case 0x21: opcodeName = "TCALL 2"; break;
        case 0x31: opcodeName = "TCALL 3"; break;
        case 0x41: opcodeName = "TCALL 4"; break;
        case 0x51: opcodeName = "TCALL 5"; break;
        case 0x61: opcodeName = "TCALL 6"; break;
        case 0x71: opcodeName = "TCALL 7"; break;
        case 0x81: opcodeName = "TCALL 8"; break;
        case 0x91: opcodeName = "TCALL 9"; break;
        case 0xA1: opcodeName = "TCALL 10"; break;
        case 0xB1: opcodeName = "TCALL 11"; break;
        case 0xC1: opcodeName = "TCALL 12"; break;
        case 0xD1: opcodeName = "TCALL 13"; break;
        case 0xE1: opcodeName = "TCALL 14"; break;
        case 0xF1: opcodeName = "TCALL 15"; break;
        case 0x02: opcodeName = "SET1 dp.bit"; break;
        case 0x22: opcodeName = "SET1 dp.bit"; break;
        case 0x42: opcodeName = "SET1 dp.bit"; break;
        case 0x62: opcodeName = "SET1 dp.bit"; break;
        case 0x82: opcodeName = "SET1 dp.bit"; break;
        case 0xA2: opcodeName = "SET1 dp.bit"; break;
        case 0xC2: opcodeName = "SET1 dp.bit"; break;
        case 0xE2: opcodeName = "SET1 dp.bit"; break;
        case 0x13: opcodeName = "BBC dp.bit,rel"; break;
        case 0x23: opcodeName = "BBC dp.bit,rel"; break;
        case 0x33: opcodeName = "BBC dp.bit,rel"; break;
        case 0x43: opcodeName = "BBC dp.bit,rel"; break;
        case 0x63: opcodeName = "BBC dp.bit,rel"; break;
        case 0x83: opcodeName = "BBC dp.bit,rel"; break;
        case 0xA3: opcodeName = "BBC dp.bit,rel"; break;
        case 0xC3: opcodeName = "BBC dp.bit,rel"; break;
        case 0xE3: opcodeName = "BBC dp.bit,rel"; break;
        case 0x05: opcodeName = "OR A,dp"; break;
        case 0x06: opcodeName = "OR A,(X)"; break;
        case 0x07: opcodeName = "OR A,(dp+X)"; break;
        case 0x09: opcodeName = "OR dp,dp"; break;
        case 0x0A: opcodeName = "OR1 C,mem.bit"; break;
        case 0x0B: opcodeName = "ASL dp"; break;
        case 0x0C: opcodeName = "ASL abs"; break;
        case 0x0E: opcodeName = "TSET1 abs"; break;
        case 0x0F: opcodeName = "BRK"; break;
        case 0x14: opcodeName = "OR A,dp+X"; break;
        case 0x15: opcodeName = "OR A,!abs+X"; break;
        case 0x16: opcodeName = "OR A,!abs+Y"; break;
        case 0x17: opcodeName = "OR A,(dp)+Y"; break;
        case 0x18: opcodeName = "OR dp,#imm"; break;
        case 0x19: opcodeName = "OR (X),(Y)"; break;
        case 0x1A: opcodeName = "DECW dp"; break;
        case 0x1B: opcodeName = "ASL dp+X"; break;
        case 0x1E: opcodeName = "CMP X,abs"; break;
        case 0x1F: opcodeName = "JMP (!abs+X)"; break;
        case 0x20: opcodeName = "CLRP"; break;
        case 0x25: opcodeName = "AND A,abs"; break;
        case 0x26: opcodeName = "AND A,(X)"; break;
        case 0x27: opcodeName = "AND A,(dp+X)"; break;
        case 0x29: opcodeName = "AND dp,dp"; break;
        case 0x2A: opcodeName = "OR1 C,mem.bit"; break;
        case 0x2B: opcodeName = "ROL dp"; break;
        case 0x2C: opcodeName = "ROL abs"; break;
        case 0x2E: opcodeName = "CBNE dp,rel"; break;
        case 0x35: opcodeName = "AND A,dp+X"; break;
        case 0x36: opcodeName = "AND A,!abs+Y"; break;
        case 0x37: opcodeName = "AND A,(dp)+Y"; break;
        case 0x38: opcodeName = "AND dp,#imm"; break;
        case 0x39: opcodeName = "AND (X),(Y)"; break;
        case 0x3B: opcodeName = "ROL dp+X"; break;
        case 0x40: opcodeName = "SETP"; break;
        case 0x45: opcodeName = "EOR A,abs"; break;
        case 0x47: opcodeName = "EOR A,(dp+X)"; break;
        case 0x49: opcodeName = "EOR dp,dp"; break;
        case 0x4A: opcodeName = "AND1 C,mem.bit"; break;
        case 0x4B: opcodeName = "LSR dp"; break;
        case 0x4C: opcodeName = "LSR abs"; break;
        case 0x4E: opcodeName = "TCLR1 abs"; break;
        case 0x4F: opcodeName = "PUSH X"; break;
        case 0x54: opcodeName = "EOR A,dp+X"; break;
        case 0x55: opcodeName = "EOR A,!abs+X"; break;
        case 0x57: opcodeName = "EOR A,(dp)+Y"; break;
        case 0x58: opcodeName = "EOR dp,#imm"; break;
        case 0x5B: opcodeName = "LSR dp+X"; break;
        case 0x5D: opcodeName = "MOV X,A"; break;
        case 0x5E: opcodeName = "CMP Y,abs"; break;
        case 0x60: opcodeName = "CLRC"; break;
        case 0x65: opcodeName = "CMP A,abs"; break;
        case 0x66: opcodeName = "CMP A,(X)"; break;
        case 0x67: opcodeName = "CMP A,(dp+X)"; break;
        case 0x69: opcodeName = "CMP dp,dp"; break;
        case 0x6A: opcodeName = "AND1 C,mem.bit"; break;
        case 0x6B: opcodeName = "ROR dp"; break;
        case 0x6C: opcodeName = "ROR abs"; break;
        case 0x6E: opcodeName = "DBNZ dp,rel"; break;
        case 0x73: opcodeName = "BBC dp.bit,rel"; break;
        case 0x74: opcodeName = "CMP A,dp+X"; break;
        case 0x75: opcodeName = "CMP A,!abs+X"; break;
        case 0x76: opcodeName = "CMP A,!abs+Y"; break;
        case 0x77: opcodeName = "CMP A,(dp)+Y"; break;
        case 0x79: opcodeName = "CMP (X),(Y)"; break;
        case 0x7A: opcodeName = "ADDW YA,dp"; break;
        case 0x7B: opcodeName = "ROR dp+X"; break;
        case 0x80: opcodeName = "SETC"; break;
        case 0x85: opcodeName = "ADC A,abs"; break;
        case 0x86: opcodeName = "ADC A,(X)"; break;
        case 0x87: opcodeName = "ADC A,(dp+X)"; break;
        case 0x89: opcodeName = "ADC dp,dp"; break;
        case 0x8A: opcodeName = "EOR1 C,mem.bit"; break;
        case 0x8B: opcodeName = "DEC dp"; break;
        case 0x8C: opcodeName = "DEC abs"; break;
        case 0x94: opcodeName = "ADC A,dp+X"; break;
        case 0x95: opcodeName = "ADC A,!abs+X"; break;
        case 0x96: opcodeName = "ADC A,!abs+Y"; break;
        case 0x97: opcodeName = "ADC A,(dp)+Y"; break;
        case 0x99: opcodeName = "ADC (X),(Y)"; break;
        case 0x9A: opcodeName = "SUBW YA,dp"; break;
        case 0x9B: opcodeName = "DEC dp+X"; break;
        case 0x9D: opcodeName = "MOV X,SP"; break;
        case 0x9E: opcodeName = "DIV YA,X"; break;
        case 0x9F: opcodeName = "XCN A"; break;
        case 0xA0: opcodeName = "EI"; break;
        case 0xA5: opcodeName = "SBC A,abs"; break;
        case 0xA6: opcodeName = "SBC A,(X)"; break;
        case 0xA7: opcodeName = "SBC A,(dp+X)"; break;
        case 0xA9: opcodeName = "SBC dp,dp"; break;
        case 0xAA: opcodeName = "MOV1 C,mem.bit"; break;
        case 0xAB: opcodeName = "INC dp"; break;
        case 0xAC: opcodeName = "INC abs"; break;
        case 0xB3: opcodeName = "BBC dp.bit,rel"; break;
        case 0xB4: opcodeName = "SBC A,dp+X"; break;
        case 0xB5: opcodeName = "SBC A,!abs+X"; break;
        case 0xB6: opcodeName = "SBC A,!abs+Y"; break;
        case 0xB7: opcodeName = "SBC A,(dp)+Y"; break;
        case 0xB9: opcodeName = "SBC (X),(Y)"; break;
        case 0xBA: opcodeName = "MOVW YA,dp"; break;
        case 0xBB: opcodeName = "INC dp+X"; break;
        case 0xBD: opcodeName = "MOV SP,X"; break;
        case 0xBE: opcodeName = "DAS A"; break;
        case 0xBF: opcodeName = "MOV A,(X)+"; break;
        case 0xC0: opcodeName = "DI"; break;
        case 0xC5: opcodeName = "MOV abs,A"; break;
        case 0xC6: opcodeName = "MOV (X),A"; break;
        case 0xC7: opcodeName = "MOV (dp+X),A"; break;
        case 0xC9: opcodeName = "MOV abs,X"; break;
        case 0xCA: opcodeName = "MOV1 mem.bit,C"; break;
        case 0xCC: opcodeName = "MOV abs,Y"; break;
        case 0xCF: opcodeName = "MUL YA"; break;
        case 0xD3: opcodeName = "BBC dp.bit,rel"; break;
        case 0xD4: opcodeName = "MOV dp+X,A"; break;
        case 0xD5: opcodeName = "MOV !abs+X,A"; break;
        case 0xD6: opcodeName = "MOV !abs+Y,A"; break;
        case 0xD7: opcodeName = "MOV (dp)+Y,A"; break;
        case 0xD9: opcodeName = "MOV dp+Y,X"; break;
        case 0xDA: opcodeName = "MOVW dp,YA"; break;
        case 0xDB: opcodeName = "MOV dp+X,Y"; break;
        case 0xDE: opcodeName = "CBNE dp+X,rel"; break;
        case 0xDF: opcodeName = "DAA A"; break;
        case 0xE0: opcodeName = "CLRV"; break;
        case 0xE5: opcodeName = "MOV A,abs"; break;
        case 0xE6: opcodeName = "MOV A,(X)"; break;
        case 0xE7: opcodeName = "MOV A,(dp+X)"; break;
        case 0xE9: opcodeName = "MOV X,abs"; break;
        case 0xEA: opcodeName = "NOT1 mem.bit"; break;
        case 0xEC: opcodeName = "MOV Y,abs"; break;
        case 0xED: opcodeName = "NOTC"; break;
        case 0xF3: opcodeName = "BBC dp.bit,rel"; break;
        case 0xF4: opcodeName = "MOV A,dp+X"; break;
        case 0xF5: opcodeName = "MOV A,!abs+X"; break;
        case 0xF6: opcodeName = "MOV A,!abs+Y"; break;
        case 0xF7: opcodeName = "MOV A,(dp)+Y"; break;
        case 0xF9: opcodeName = "MOV X,dp+Y"; break;
        case 0xFB: opcodeName = "MOV Y,dp+X"; break;
        case 0xEF: opcodeName = "SLEEP"; break;
        case 0xFF: opcodeName = "BRK"; break;
        default: {
            std::ostringstream oss;
            oss << "UNK_0x" << std::hex << std::setw(2) << std::setfill('0') << (int)opcode;
            opcodeName = oss.str();
            break;
        }
    }
    
    // Log SPC700 execution - always log after SPC_LOAD_COMPLETE to debug test failures
    static int spcLogCount = 0;
    bool shouldLog = false;
    if (m_spcLoadState == SPC_LOAD_COMPLETE) {
        // Log first 200 instructions after load, then log every 1000th instruction
        if (spcLogCount < 200 || (spcLogCount % 1000 == 0)) {
            shouldLog = true;
        }
        spcLogCount++;
    } else if (Logger::getInstance().isLoggingEnabled()) {
        shouldLog = true;
    }
    
    if (shouldLog) {
        std::ostringstream oss;
        oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') << (m_cpu ? m_cpu->getCycles() : 0) << "] "
            << "SPC700 PC:0x" << std::hex << std::setw(4) << std::setfill('0') << savedPC << " | ";
        
        // Output machine code bytes
        oss << std::setw(2) << std::setfill('0') << (int)opcode;
        for (size_t i = 0; i < operands.size(); i++) {
            oss << " " << std::setw(2) << std::setfill('0') << (int)operands[i];
        }
        // Pad to fixed width for alignment
        int totalBytes = 1 + operandLength;
        for (int i = totalBytes; i < 4; i++) {
            oss << "   ";
        }
        
        oss << " | " << std::left << std::setw(20) << std::setfill(' ') << opcodeName << std::right;
        
        // Output operand values if present
        if (operandLength > 0) {
            oss << " | ";
            if (operandLength == 1) {
                oss << "operand=0x" << std::setw(2) << std::setfill('0') << (int)operands[0];
            } else if (operandLength == 2) {
                uint16_t val = operands[0] | (operands[1] << 8);
                oss << "operand=0x" << std::setw(4) << std::setfill('0') << val;
            } else {
                for (size_t j = 0; j < operands.size(); j++) {
                    if (j > 0) oss << ",";
                    oss << "0x" << std::setw(2) << std::setfill('0') << (int)operands[j];
                }
            }
        }
        
        oss << " | A:0x" << std::setw(2) << std::setfill('0') << (int)savedA
            << " | X:0x" << std::setw(2) << std::setfill('0') << (int)savedX
            << " | Y:0x" << std::setw(2) << std::setfill('0') << (int)savedY
            << " | SP:0x" << std::setw(2) << std::setfill('0') << (int)savedSP
            << " | PSW:0x" << std::setw(2) << std::setfill('0') << (int)savedPSW;
        std::cout << oss.str() << std::endl;
        if (Logger::getInstance().isLoggingEnabled()) {
            Logger::getInstance().logAPU(oss.str());
        }
    }
    
    // SPC700 instruction execution
    switch (opcode) {
        case 0x00: // NOP
            break;
            
        // MOV instructions - Immediate to Direct Page
        case 0x8F: { // MOV dp, #imm
            uint8_t imm = readARAM(m_regs.pc++);
            uint8_t dp = readARAM(m_regs.pc++);
            writeARAM(dp, imm);
        } break;
        
        // MOV instructions - Immediate to Register
        case 0xE8: { // MOV A, #imm
            m_regs.a = readARAM(m_regs.pc++);
            updateNZ(m_regs.a);
        } break;
        case 0xCD: { // MOV X, #imm
            m_regs.x = readARAM(m_regs.pc++);
            updateNZ(m_regs.x);
        } break;
        case 0x8D: { // MOV Y, #imm
            m_regs.y = readARAM(m_regs.pc++);
            updateNZ(m_regs.y);
        } break;
        
        // MOV instructions - Register to Direct Page
        case 0xC4: { // MOV dp, A
            uint8_t dp = readARAM(m_regs.pc++);
            writeARAM(dp, m_regs.a);
        } break;
        case 0xD8: { // MOV dp, X
            uint8_t dp = readARAM(m_regs.pc++);
            writeARAM(dp, m_regs.x);
        } break;
        case 0xCB: { // MOV dp, Y
            uint8_t dp = readARAM(m_regs.pc++);
            writeARAM(dp, m_regs.y);
        } break;
        
        // MOV instructions - Direct Page to Register
        case 0xE4: { // MOV A, dp
            uint8_t dp = readARAM(m_regs.pc++);
            m_regs.a = readARAM(dp);
            updateNZ(m_regs.a);
        } break;
        case 0xF8: { // MOV X, dp
            uint8_t dp = readARAM(m_regs.pc++);
            m_regs.x = readARAM(dp);
            updateNZ(m_regs.x);
        } break;
        case 0xEB: { // MOV Y, dp
            uint8_t dp = readARAM(m_regs.pc++);
            m_regs.y = readARAM(dp);
            updateNZ(m_regs.y);
        } break;
        
        // MOV instructions - Register to Register
        case 0x7D: { // MOV A, X
            m_regs.a = m_regs.x;
            updateNZ(m_regs.a);
        } break;
        case 0xDD: { // MOV A, Y
            m_regs.a = m_regs.y;
            updateNZ(m_regs.a);
        } break;
        case 0x5D: { // MOV X, A
            m_regs.x = m_regs.a;
            updateNZ(m_regs.x);
        } break;
        case 0xFD: { // MOV Y, A
            m_regs.y = m_regs.a;
            updateNZ(m_regs.y);
        } break;
        
        // MOV instructions - Direct Page to Direct Page
        case 0xFA: { // MOV dp1, dp2
            uint8_t dp1 = readARAM(m_regs.pc++);
            uint8_t dp2 = readARAM(m_regs.pc++);
            writeARAM(dp1, readARAM(dp2));
        } break;
        
        // Arithmetic operations
        case 0xBC: { // INC A
            m_regs.a++;
            updateNZ(m_regs.a);
        } break;
        case 0x3D: { // INC X
            m_regs.x++;
            updateNZ(m_regs.x);
        } break;
        case 0xFC: { // INC Y
            m_regs.y++;
            updateNZ(m_regs.y);
        } break;
        case 0xC8: { // INC Y (alternate encoding)
            m_regs.y++;
            updateNZ(m_regs.y);
        } break;
        case 0x9C: { // DEC A
            m_regs.a--;
            updateNZ(m_regs.a);
        } break;
        case 0x1D: { // DEC X
            m_regs.x--;
            updateNZ(m_regs.x);
        } break;
        case 0xDC: { // DEC Y
            m_regs.y--;
            updateNZ(m_regs.y);
        } break;
        
        case 0x88: { // ADC A, #imm
            uint8_t imm = readARAM(m_regs.pc++);
            uint16_t sum = m_regs.a + imm + (getFlag(FLAG_C) ? 1 : 0);
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ sum) & (imm ^ sum) & 0x80) != 0);
            m_regs.a = sum & 0xFF;
            updateNZ(m_regs.a);
        } break;
        case 0x84: { // ADC A, dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            uint16_t sum = m_regs.a + val + (getFlag(FLAG_C) ? 1 : 0);
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ sum) & (val ^ sum) & 0x80) != 0);
            m_regs.a = sum & 0xFF;
            updateNZ(m_regs.a);
        } break;
        case 0xA8: { // SBC A, #imm
            uint8_t imm = readARAM(m_regs.pc++);
            uint16_t diff = m_regs.a - imm - (getFlag(FLAG_C) ? 0 : 1);
            setFlag(FLAG_C, diff <= 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ diff) & (imm ^ diff) & 0x80) != 0);
            m_regs.a = diff & 0xFF;
            updateNZ(m_regs.a);
        } break;
        case 0xA4: { // SBC A, dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            uint16_t diff = m_regs.a - val - (getFlag(FLAG_C) ? 0 : 1);
            setFlag(FLAG_C, diff <= 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ diff) & (val ^ diff) & 0x80) != 0);
            m_regs.a = diff & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        case 0x28: { // AND A, #imm
            m_regs.a &= readARAM(m_regs.pc++);
            updateNZ(m_regs.a);
        } break;
        case 0x24: { // AND A, dp
            uint8_t dp = readARAM(m_regs.pc++);
            m_regs.a &= readARAM(dp);
            updateNZ(m_regs.a);
        } break;
        case 0x08: { // OR A, #imm
            m_regs.a |= readARAM(m_regs.pc++);
            updateNZ(m_regs.a);
        } break;
        case 0x04: { // OR A, dp
            uint8_t dp = readARAM(m_regs.pc++);
            m_regs.a |= readARAM(dp);
            updateNZ(m_regs.a);
        } break;
        case 0x48: { // EOR A, #imm
            m_regs.a ^= readARAM(m_regs.pc++);
            updateNZ(m_regs.a);
        } break;
        case 0x44: { // EOR A, dp
            uint8_t dp = readARAM(m_regs.pc++);
            m_regs.a ^= readARAM(dp);
            updateNZ(m_regs.a);
        } break;
        case 0x46: { // EOR A, (X)
            uint8_t addr = m_regs.x;
            m_regs.a ^= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        case 0x56: { // EOR A, !abs+Y
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.y;
            m_regs.a ^= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        case 0x34: { // AND A, !abs+X
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.x;
            m_regs.a &= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        case 0x59: { // EOR X, dp
            uint8_t dp = readARAM(m_regs.pc++);
            m_regs.x ^= readARAM(dp);
            updateNZ(m_regs.x);
        } break;
        
        // Comparison operations
        case 0x68: { // CMP A, #imm
            uint8_t imm = readARAM(m_regs.pc++);
            uint8_t result = m_regs.a - imm;
            setFlag(FLAG_C, m_regs.a >= imm);
            updateNZ(result);
        } break;
        case 0x64: { // CMP A, dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            uint8_t result = m_regs.a - val;
            setFlag(FLAG_C, m_regs.a >= val);
            updateNZ(result);
        } break;
        case 0x3E: { // CMP X, dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            uint8_t result = m_regs.x - val;
            setFlag(FLAG_C, m_regs.x >= val);
            updateNZ(result);
        } break;
        case 0x7E: { // CMP Y, dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            uint8_t result = m_regs.y - val;
            setFlag(FLAG_C, m_regs.y >= val);
            updateNZ(result);
        } break;
        case 0xAD: { // CMP Y, #imm
            uint8_t imm = readARAM(m_regs.pc++);
            uint8_t result = m_regs.y - imm;
            setFlag(FLAG_C, m_regs.y >= imm);
            updateNZ(result);
        } break;
        
        // Branch instructions
        case 0x2F: { // BRA rel (Branch Always)
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            m_regs.pc += offset;
        } break;
        case 0xF0: { // BEQ rel (Branch if Equal/Zero)
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if (getFlag(FLAG_Z)) {
                m_regs.pc += offset;
            }
        } break;
        case 0xD0: { // BNE rel (Branch if Not Equal/Not Zero)
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if (!getFlag(FLAG_Z)) {
                m_regs.pc += offset;
            }
        } break;
        case 0x90: { // BCC rel (Branch if Carry Clear)
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if (!getFlag(FLAG_C)) {
                m_regs.pc += offset;
            }
        } break;
        case 0xB0: { // BCS rel (Branch if Carry Set)
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if (getFlag(FLAG_C)) {
                m_regs.pc += offset;
            }
        } break;
        case 0x30: { // BMI rel (Branch if Minus/Negative)
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if (getFlag(FLAG_N)) {
                m_regs.pc += offset;
            }
        } break;
        case 0x10: { // BPL rel (Branch if Plus/Positive)
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if (!getFlag(FLAG_N)) {
                m_regs.pc += offset;
            }
        } break;
        case 0x50: { // BVC rel (Branch if Overflow Clear)
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if (!getFlag(FLAG_V)) {
                m_regs.pc += offset;
            }
        } break;
        case 0x70: { // BVS rel (Branch if Overflow Set)
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if (getFlag(FLAG_V)) {
                m_regs.pc += offset;
            }
        } break;
        
        // Stack operations
        case 0x2D: { // PUSH A
            push(m_regs.a);
        } break;
        case 0x4D: { // PUSH X
            push(m_regs.x);
        } break;
        case 0x6D: { // PUSH Y
            push(m_regs.y);
        } break;
        case 0x0D: { // PUSH PSW
            push(m_regs.psw);
        } break;
        case 0xAE: { // POP A
            m_regs.a = pop();
            updateNZ(m_regs.a);
        } break;
        case 0xCE: { // POP X
            m_regs.x = pop();
            updateNZ(m_regs.x);
        } break;
        case 0xEE: { // POP Y
            m_regs.y = pop();
            updateNZ(m_regs.y);
        } break;
        case 0x8E: { // POP PSW
            m_regs.psw = pop();
        } break;
        
        // Call and Return
        case 0x3F: { // CALL abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            push((m_regs.pc >> 8) & 0xFF);
            push(m_regs.pc & 0xFF);
            m_regs.pc = addr;
        } break;
        case 0x6F: { // RET
            uint8_t low = pop();
            uint8_t high = pop();
            m_regs.pc = low | (high << 8);
        } break;
        case 0x7F: { // RETI
            uint8_t low = pop();
            uint8_t high = pop();
            m_regs.pc = low | (high << 8);
            m_regs.psw |= FLAG_I; // Enable interrupts
        } break;
        case 0x5F: { // JMP abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            m_regs.pc = addr;
        } break;
        
        // Shift/Rotate operations
        case 0x3C: { // ROL A (Rotate Left through Carry)
            bool carry = getFlag(FLAG_C);
            setFlag(FLAG_C, (m_regs.a & 0x80) != 0);
            m_regs.a = (m_regs.a << 1) | (carry ? 1 : 0);
            updateNZ(m_regs.a);
        } break;
        case 0x7C: { // ROR A (Rotate Right through Carry)
            bool carry = getFlag(FLAG_C);
            setFlag(FLAG_C, (m_regs.a & 0x01) != 0);
            m_regs.a = (m_regs.a >> 1) | (carry ? 0x80 : 0);
            updateNZ(m_regs.a);
        } break;
        case 0x1C: { // ASL A (Arithmetic Shift Left)
            setFlag(FLAG_C, (m_regs.a & 0x80) != 0);
            m_regs.a <<= 1;
            updateNZ(m_regs.a);
        } break;
        case 0x5C: { // LSR A (Logical Shift Right)
            setFlag(FLAG_C, (m_regs.a & 0x01) != 0);
            m_regs.a >>= 1;
            updateNZ(m_regs.a);
        } break;
        
        // 16-bit operations
        case 0x3A: { // INCW dp (16-bit increment)
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t val = readARAM(dp);
            val |= readARAM(dp + 1) << 8;
            val++;
            writeARAM(dp, val & 0xFF);
            writeARAM(dp + 1, (val >> 8) & 0xFF);
            updateNZ((uint8_t)val);
        } break;
        case 0x5A: { // CMPW YA, dp (16-bit compare)
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t memVal = readARAM(dp);
            memVal |= readARAM(dp + 1) << 8;
            uint16_t ya = m_regs.a | (m_regs.y << 8);
            uint16_t result = ya - memVal;
            setFlag(FLAG_C, ya >= memVal);
            updateNZ((uint8_t)result);
        } break;
        
        // Bit operations
        case 0x78: { // SET1 dp.bit
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t bit = (dp >> 5) & 0x07;
            dp &= 0x1F;
            uint8_t val = readARAM(dp);
            val |= (1 << bit);
            writeARAM(dp, val);
        } break;
        case 0x03: { // BBS dp.bit, rel (Branch if Bit Set)
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t bit = (dp >> 5) & 0x07;
            dp &= 0x1F;
            uint8_t val = readARAM(dp);
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if ((val >> bit) & 1) {
                m_regs.pc += offset;
            }
        } break;
        
        // MOV with indexed addressing
        case 0xF5: { // MOV A, !abs+X
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.x;
            m_regs.a = readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // Special operations
        case 0xEF: { // SLEEP - wait for interrupt
            // In a real SPC700, this halts execution until an interrupt occurs
            // For now, just loop back (effectively a NOP in our simplified implementation)
            m_regs.pc--;  // Stay on SLEEP instruction
        } break;
        case 0xFF: { // BRK - Software interrupt
            push((m_regs.pc >> 8) & 0xFF);
            push(m_regs.pc & 0xFF);
            push(m_regs.psw);
            setFlag(FLAG_B, true);
            setFlag(FLAG_I, false);
            // BRK vector at 0xFFDE
            uint16_t brkVector = (readARAM(0xFFDF) << 8) | readARAM(0xFFDE);
            m_regs.pc = brkVector;
        } break;
        
        // TCALL instructions (0x01, 0x11, 0x21, ... 0xF1)
        case 0x01: case 0x11: case 0x21: case 0x31: case 0x41: case 0x51: case 0x61: case 0x71:
        case 0x81: case 0x91: case 0xA1: case 0xB1: case 0xC1: case 0xD1: case 0xE1: case 0xF1: {
            // TCALL n: Call vector at 0xFFDE - 2*n
            uint8_t tcallNum = (opcode >> 4) & 0x0F;
            uint16_t vectorAddr = 0xFFDE - (tcallNum * 2);
            push((m_regs.pc >> 8) & 0xFF);
            push(m_regs.pc & 0xFF);
            uint8_t low = readARAM(vectorAddr);
            uint8_t high = readARAM(vectorAddr + 1);
            m_regs.pc = low | (high << 8);
        } break;
        
        // SET1 dp.bit (0x02, 0x22, 0x42, 0x62, 0x82, 0xA2, 0xC2, 0xE2)
        case 0x02: case 0x22: case 0x42: case 0x62: case 0x82: case 0xA2: case 0xC2: case 0xE2: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t bit = (opcode >> 5) & 0x07;
            uint8_t val = readARAM(dp);
            val |= (1 << bit);
            writeARAM(dp, val);
        } break;
        
        // CLR1 dp.bit (0x12, 0x32, 0x52, 0x72, 0x92, 0xB2, 0xD2, 0xF2)
        case 0x12: case 0x32: case 0x52: case 0x72: case 0x92: case 0xB2: case 0xD2: case 0xF2: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t bit = (opcode >> 5) & 0x07;
            uint8_t val = readARAM(dp);
            val &= ~(1 << bit);
            writeARAM(dp, val);
        } break;
        
        // BBS dp.bit,rel (0x03, 0x23, 0x43, 0x63, 0x83, 0xA3, 0xC3, 0xE3)
        case 0x23: case 0x43: case 0x63: case 0x83: case 0xA3: case 0xC3: case 0xE3: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t bit = (opcode >> 5) & 0x07;
            uint8_t val = readARAM(dp);
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if ((val >> bit) & 1) {
                m_regs.pc += offset;
            }
        } break;
        
        // BBC dp.bit,rel (0x13, 0x33, 0x53, 0x73, 0x93, 0xB3, 0xD3, 0xF3)
        case 0x13: case 0x33: case 0x53: case 0x73: case 0x93: case 0xB3: case 0xD3: case 0xF3: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t bit = (opcode >> 5) & 0x07;
            uint8_t val = readARAM(dp);
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if (!((val >> bit) & 1)) {
                m_regs.pc += offset;
            }
        } break;
        
        // OR A,(X) - 0x06
        case 0x06: {
            m_regs.a |= readARAM(m_regs.x);
            updateNZ(m_regs.a);
        } break;
        
        // OR A,(dp+X) - 0x07
        case 0x07: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            m_regs.a |= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // OR A,dp+X - 0x14
        case 0x14: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            m_regs.a |= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // OR A,!abs+X - 0x15
        case 0x15: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.x;
            m_regs.a |= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // OR A,!abs+Y - 0x16
        case 0x16: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.y;
            m_regs.a |= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // OR A,(dp)+Y - 0x17
        case 0x17: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = readARAM(dp);
            addr |= readARAM(dp + 1) << 8;
            addr += m_regs.y;
            m_regs.a |= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // OR dp,#imm - 0x18
        case 0x18: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t imm = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            val |= imm;
            writeARAM(dp, val);
            updateNZ(val);
        } break;
        
        // OR (X),(Y) - 0x19
        case 0x19: {
            uint8_t val = readARAM(m_regs.x) | readARAM(m_regs.y);
            writeARAM(m_regs.x, val);
            updateNZ(val);
        } break;
        
        // AND A,abs - 0x25
        case 0x25: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            m_regs.a &= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // AND A,(X) - 0x26
        case 0x26: {
            m_regs.a &= readARAM(m_regs.x);
            updateNZ(m_regs.a);
        } break;
        
        // AND A,(dp+X) - 0x27
        case 0x27: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            m_regs.a &= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // AND A,dp+X - 0x35
        case 0x35: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            m_regs.a &= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // AND A,!abs+Y - 0x36
        case 0x36: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.y;
            m_regs.a &= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // AND A,(dp)+Y - 0x37
        case 0x37: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = readARAM(dp);
            addr |= readARAM(dp + 1) << 8;
            addr += m_regs.y;
            m_regs.a &= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // AND dp,#imm - 0x38
        case 0x38: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t imm = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            val &= imm;
            writeARAM(dp, val);
            updateNZ(val);
        } break;
        
        // AND (X),(Y) - 0x39
        case 0x39: {
            uint8_t val = readARAM(m_regs.x) & readARAM(m_regs.y);
            writeARAM(m_regs.x, val);
            updateNZ(val);
        } break;
        
        // EOR A,abs - 0x45
        case 0x45: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            m_regs.a ^= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // EOR A,(dp+X) - 0x47
        case 0x47: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            m_regs.a ^= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // EOR A,dp+X - 0x54
        case 0x54: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            m_regs.a ^= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // EOR A,!abs+X - 0x55
        case 0x55: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.x;
            m_regs.a ^= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // EOR A,(dp)+Y - 0x57
        case 0x57: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = readARAM(dp);
            addr |= readARAM(dp + 1) << 8;
            addr += m_regs.y;
            m_regs.a ^= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // EOR dp,#imm - 0x58
        case 0x58: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t imm = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            val ^= imm;
            writeARAM(dp, val);
            updateNZ(val);
        } break;
        
        // CMP A,abs - 0x65
        case 0x65: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t val = readARAM(addr);
            uint8_t result = m_regs.a - val;
            setFlag(FLAG_C, m_regs.a >= val);
            updateNZ(result);
        } break;
        
        // CMP A,(X) - 0x66
        case 0x66: {
            uint8_t val = readARAM(m_regs.x);
            uint8_t result = m_regs.a - val;
            setFlag(FLAG_C, m_regs.a >= val);
            updateNZ(result);
        } break;
        
        // CMP A,(dp+X) - 0x67
        case 0x67: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            uint8_t val = readARAM(addr);
            uint8_t result = m_regs.a - val;
            setFlag(FLAG_C, m_regs.a >= val);
            updateNZ(result);
        } break;
        
        // CMP A,dp+X - 0x74
        case 0x74: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            uint8_t val = readARAM(addr);
            uint8_t result = m_regs.a - val;
            setFlag(FLAG_C, m_regs.a >= val);
            updateNZ(result);
        } break;
        
        // CMP A,!abs+X - 0x75
        case 0x75: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.x;
            uint8_t val = readARAM(addr);
            uint8_t result = m_regs.a - val;
            setFlag(FLAG_C, m_regs.a >= val);
            updateNZ(result);
        } break;
        
        // CMP A,!abs+Y - 0x76
        case 0x76: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.y;
            uint8_t val = readARAM(addr);
            uint8_t result = m_regs.a - val;
            setFlag(FLAG_C, m_regs.a >= val);
            updateNZ(result);
        } break;
        
        // CMP A,(dp)+Y - 0x77
        case 0x77: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = readARAM(dp);
            addr |= readARAM(dp + 1) << 8;
            addr += m_regs.y;
            uint8_t val = readARAM(addr);
            uint8_t result = m_regs.a - val;
            setFlag(FLAG_C, m_regs.a >= val);
            updateNZ(result);
        } break;
        
        // CMP (X),(Y) - 0x79
        case 0x79: {
            uint8_t valX = readARAM(m_regs.x);
            uint8_t valY = readARAM(m_regs.y);
            uint8_t result = valX - valY;
            setFlag(FLAG_C, valX >= valY);
            updateNZ(result);
        } break;
        
        // ADC A,abs - 0x85
        case 0x85: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t val = readARAM(addr);
            uint16_t sum = m_regs.a + val + (getFlag(FLAG_C) ? 1 : 0);
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ sum) & (val ^ sum) & 0x80) != 0);
            m_regs.a = sum & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // ADC A,(X) - 0x86
        case 0x86: {
            uint8_t val = readARAM(m_regs.x);
            uint16_t sum = m_regs.a + val + (getFlag(FLAG_C) ? 1 : 0);
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ sum) & (val ^ sum) & 0x80) != 0);
            m_regs.a = sum & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // ADC A,(dp+X) - 0x87
        case 0x87: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            uint8_t val = readARAM(addr);
            uint16_t sum = m_regs.a + val + (getFlag(FLAG_C) ? 1 : 0);
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ sum) & (val ^ sum) & 0x80) != 0);
            m_regs.a = sum & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // ADC A,dp+X - 0x94
        case 0x94: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            uint8_t val = readARAM(addr);
            uint16_t sum = m_regs.a + val + (getFlag(FLAG_C) ? 1 : 0);
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ sum) & (val ^ sum) & 0x80) != 0);
            m_regs.a = sum & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // ADC A,!abs+X - 0x95
        case 0x95: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.x;
            uint8_t val = readARAM(addr);
            uint16_t sum = m_regs.a + val + (getFlag(FLAG_C) ? 1 : 0);
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ sum) & (val ^ sum) & 0x80) != 0);
            m_regs.a = sum & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // ADC A,!abs+Y - 0x96
        case 0x96: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.y;
            uint8_t val = readARAM(addr);
            uint16_t sum = m_regs.a + val + (getFlag(FLAG_C) ? 1 : 0);
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ sum) & (val ^ sum) & 0x80) != 0);
            m_regs.a = sum & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // ADC A,(dp)+Y - 0x97
        case 0x97: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = readARAM(dp);
            addr |= readARAM(dp + 1) << 8;
            addr += m_regs.y;
            uint8_t val = readARAM(addr);
            uint16_t sum = m_regs.a + val + (getFlag(FLAG_C) ? 1 : 0);
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ sum) & (val ^ sum) & 0x80) != 0);
            m_regs.a = sum & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // ADC (X),(Y) - 0x99
        case 0x99: {
            uint8_t valX = readARAM(m_regs.x);
            uint8_t valY = readARAM(m_regs.y);
            uint16_t sum = valX + valY + (getFlag(FLAG_C) ? 1 : 0);
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_V, ((valX ^ sum) & (valY ^ sum) & 0x80) != 0);
            uint8_t result = sum & 0xFF;
            writeARAM(m_regs.x, result);
            updateNZ(result);
        } break;
        
        // SBC A,abs - 0xA5
        case 0xA5: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t val = readARAM(addr);
            uint16_t diff = m_regs.a - val - (getFlag(FLAG_C) ? 0 : 1);
            setFlag(FLAG_C, diff <= 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ diff) & (val ^ diff) & 0x80) != 0);
            m_regs.a = diff & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // SBC A,(X) - 0xA6
        case 0xA6: {
            uint8_t val = readARAM(m_regs.x);
            uint16_t diff = m_regs.a - val - (getFlag(FLAG_C) ? 0 : 1);
            setFlag(FLAG_C, diff <= 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ diff) & (val ^ diff) & 0x80) != 0);
            m_regs.a = diff & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // SBC A,(dp+X) - 0xA7
        case 0xA7: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            uint8_t val = readARAM(addr);
            uint16_t diff = m_regs.a - val - (getFlag(FLAG_C) ? 0 : 1);
            setFlag(FLAG_C, diff <= 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ diff) & (val ^ diff) & 0x80) != 0);
            m_regs.a = diff & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // SBC A,dp+X - 0xB4
        case 0xB4: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            uint8_t val = readARAM(addr);
            uint16_t diff = m_regs.a - val - (getFlag(FLAG_C) ? 0 : 1);
            setFlag(FLAG_C, diff <= 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ diff) & (val ^ diff) & 0x80) != 0);
            m_regs.a = diff & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // SBC A,!abs+X - 0xB5
        case 0xB5: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.x;
            uint8_t val = readARAM(addr);
            uint16_t diff = m_regs.a - val - (getFlag(FLAG_C) ? 0 : 1);
            setFlag(FLAG_C, diff <= 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ diff) & (val ^ diff) & 0x80) != 0);
            m_regs.a = diff & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // SBC A,!abs+Y - 0xB6
        case 0xB6: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.y;
            uint8_t val = readARAM(addr);
            uint16_t diff = m_regs.a - val - (getFlag(FLAG_C) ? 0 : 1);
            setFlag(FLAG_C, diff <= 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ diff) & (val ^ diff) & 0x80) != 0);
            m_regs.a = diff & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // SBC A,(dp)+Y - 0xB7
        case 0xB7: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = readARAM(dp);
            addr |= readARAM(dp + 1) << 8;
            addr += m_regs.y;
            uint8_t val = readARAM(addr);
            uint16_t diff = m_regs.a - val - (getFlag(FLAG_C) ? 0 : 1);
            setFlag(FLAG_C, diff <= 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ diff) & (val ^ diff) & 0x80) != 0);
            m_regs.a = diff & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // SBC (X),(Y) - 0xB9
        case 0xB9: {
            uint8_t valX = readARAM(m_regs.x);
            uint8_t valY = readARAM(m_regs.y);
            uint16_t diff = valX - valY - (getFlag(FLAG_C) ? 0 : 1);
            setFlag(FLAG_C, diff <= 0xFF);
            setFlag(FLAG_V, ((valX ^ diff) & (valY ^ diff) & 0x80) != 0);
            uint8_t result = diff & 0xFF;
            writeARAM(m_regs.x, result);
            updateNZ(result);
        } break;
        
        // Special instructions
        case 0x20: { // CLRP - Clear Direct Page flag
            setFlag(FLAG_P, false);
        } break;
        case 0x40: { // SETP - Set Direct Page flag
            setFlag(FLAG_P, true);
        } break;
        case 0x60: { // CLRC - Clear Carry flag
            setFlag(FLAG_C, false);
        } break;
        case 0x80: { // SETC - Set Carry flag
            setFlag(FLAG_C, true);
        } break;
        case 0xE0: { // CLRV - Clear Overflow flag
            setFlag(FLAG_V, false);
            setFlag(FLAG_H, false);
        } break;
        case 0xA0: { // EI - Enable Interrupts
            setFlag(FLAG_I, true);
        } break;
        case 0xC0: { // DI - Disable Interrupts
            setFlag(FLAG_I, false);
        } break;
        case 0xED: { // NOTC - Complement Carry flag
            setFlag(FLAG_C, !getFlag(FLAG_C));
        } break;
        case 0xDF: { // DAA A - Decimal Adjust Accumulator
            uint8_t a = m_regs.a;
            bool c = getFlag(FLAG_C);
            if (getFlag(FLAG_H) || (a & 0x0F) > 9) {
                a += 0x06;
            }
            if (c || (a & 0xF0) > 0x90) {
                a += 0x60;
                setFlag(FLAG_C, true);
            }
            m_regs.a = a;
            updateNZ(m_regs.a);
        } break;
        case 0xBE: { // DAS A - Decimal Adjust for Subtraction
            uint8_t a = m_regs.a;
            bool c = getFlag(FLAG_C);
            if (!getFlag(FLAG_H) && (a & 0x0F) > 9) {
                a -= 0x06;
            }
            if (!c && (a & 0xF0) > 0x90) {
                a -= 0x60;
                setFlag(FLAG_C, false);
            }
            m_regs.a = a;
            updateNZ(m_regs.a);
        } break;
        case 0x9F: { // XCN A - Exchange nibbles
            m_regs.a = ((m_regs.a << 4) | (m_regs.a >> 4));
            updateNZ(m_regs.a);
        } break;
        case 0x9E: { // DIV YA,X - Divide YA by X
            uint16_t ya = m_regs.a | (m_regs.y << 8);
            uint8_t x = m_regs.x;
            if (x == 0) {
                // Division by zero - undefined behavior
                m_regs.a = 0xFF;
                m_regs.y = 0xFF;
            } else {
                m_regs.a = ya / x;
                m_regs.y = ya % x;
            }
            updateNZ(m_regs.a);
            setFlag(FLAG_H, (m_regs.y & 0x0F) >= (x & 0x0F));
        } break;
        case 0xCF: { // MUL YA - Multiply Y by A
            uint16_t result = m_regs.y * m_regs.a;
            m_regs.a = result & 0xFF;
            m_regs.y = (result >> 8) & 0xFF;
            updateNZ(m_regs.y);
        } break;
        
        // 16-bit operations
        case 0x1A: { // DECW dp - 16-bit decrement
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t val = readARAM(dp);
            val |= readARAM(dp + 1) << 8;
            val--;
            writeARAM(dp, val & 0xFF);
            writeARAM(dp + 1, (val >> 8) & 0xFF);
            updateNZ((uint8_t)val);
        } break;
        case 0x7A: { // ADDW YA,dp - 16-bit add
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t memVal = readARAM(dp);
            memVal |= readARAM(dp + 1) << 8;
            uint16_t ya = m_regs.a | (m_regs.y << 8);
            uint32_t sum = ya + memVal;
            setFlag(FLAG_C, sum > 0xFFFF);
            setFlag(FLAG_V, ((ya ^ sum) & (memVal ^ sum) & 0x8000) != 0);
            m_regs.a = sum & 0xFF;
            m_regs.y = (sum >> 8) & 0xFF;
            updateNZ(m_regs.y);
        } break;
        case 0x9A: { // SUBW YA,dp - 16-bit subtract
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t memVal = readARAM(dp);
            memVal |= readARAM(dp + 1) << 8;
            uint16_t ya = m_regs.a | (m_regs.y << 8);
            uint32_t diff = ya - memVal;
            setFlag(FLAG_C, diff <= 0xFFFF);
            setFlag(FLAG_V, ((ya ^ diff) & (memVal ^ diff) & 0x8000) != 0);
            m_regs.a = diff & 0xFF;
            m_regs.y = (diff >> 8) & 0xFF;
            updateNZ(m_regs.y);
        } break;
        case 0xBA: { // MOVW YA,dp - 16-bit move
            uint8_t dp = readARAM(m_regs.pc++);
            m_regs.a = readARAM(dp);
            m_regs.y = readARAM(dp + 1);
            updateNZ(m_regs.y);
        } break;
        case 0xDA: { // MOVW dp,YA - 16-bit move
            uint8_t dp = readARAM(m_regs.pc++);
            writeARAM(dp, m_regs.a);
            writeARAM(dp + 1, m_regs.y);
        } break;
        
        // MOV instructions - remaining addressing modes
        case 0xE5: { // MOV A,abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            m_regs.a = readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        case 0xE6: { // MOV A,(X)
            m_regs.a = readARAM(m_regs.x);
            updateNZ(m_regs.a);
        } break;
        case 0xE7: { // MOV A,(dp+X)
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            m_regs.a = readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        case 0xF4: { // MOV A,dp+X
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            m_regs.a = readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        case 0xF6: { // MOV A,!abs+Y
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.y;
            m_regs.a = readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        case 0xF7: { // MOV A,(dp)+Y
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = readARAM(dp);
            addr |= readARAM(dp + 1) << 8;
            addr += m_regs.y;
            m_regs.a = readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        case 0xBF: { // MOV A,(X)+
            m_regs.a = readARAM(m_regs.x);
            m_regs.x++;
            updateNZ(m_regs.a);
        } break;
        case 0xC5: { // MOV abs,A
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            writeARAM(addr, m_regs.a);
        } break;
        case 0xC6: { // MOV (X),A
            writeARAM(m_regs.x, m_regs.a);
        } break;
        case 0xC7: { // MOV (dp+X),A
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            writeARAM(addr, m_regs.a);
        } break;
        case 0xD4: { // MOV dp+X,A
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            writeARAM(addr, m_regs.a);
        } break;
        case 0xD5: { // MOV !abs+X,A
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.x;
            writeARAM(addr, m_regs.a);
        } break;
        case 0xD6: { // MOV !abs+Y,A
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.y;
            writeARAM(addr, m_regs.a);
        } break;
        case 0xD7: { // MOV (dp)+Y,A
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = readARAM(dp);
            addr |= readARAM(dp + 1) << 8;
            addr += m_regs.y;
            writeARAM(addr, m_regs.a);
        } break;
        case 0xE9: { // MOV X,abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            m_regs.x = readARAM(addr);
            updateNZ(m_regs.x);
        } break;
        case 0xF9: { // MOV X,dp+Y
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.y) & 0xFF;
            m_regs.x = readARAM(addr);
            updateNZ(m_regs.x);
        } break;
        case 0xFB: { // MOV Y,dp+X
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            m_regs.y = readARAM(addr);
            updateNZ(m_regs.y);
        } break;
        case 0xEC: { // MOV Y,abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            m_regs.y = readARAM(addr);
            updateNZ(m_regs.y);
        } break;
        case 0xC9: { // MOV abs,X
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            writeARAM(addr, m_regs.x);
        } break;
        case 0xCC: { // MOV abs,Y
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            writeARAM(addr, m_regs.y);
        } break;
        case 0xD9: { // MOV dp+Y,X
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.y) & 0xFF;
            writeARAM(addr, m_regs.x);
        } break;
        case 0xDB: { // MOV dp+X,Y
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            writeARAM(addr, m_regs.y);
        } break;
        case 0x9D: { // MOV X,SP
            m_regs.x = m_regs.sp;
            updateNZ(m_regs.x);
        } break;
        case 0xBD: { // MOV SP,X
            m_regs.sp = m_regs.x;
        } break;
        
        // INC/DEC with addressing modes
        case 0xAB: { // INC dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            val++;
            writeARAM(dp, val);
            updateNZ(val);
        } break;
        case 0xAC: { // INC abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t val = readARAM(addr);
            val++;
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0xBB: { // INC dp+X
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            uint8_t val = readARAM(addr);
            val++;
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0x8B: { // DEC dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            val--;
            writeARAM(dp, val);
            updateNZ(val);
        } break;
        case 0x8C: { // DEC abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t val = readARAM(addr);
            val--;
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0x9B: { // DEC dp+X
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            uint8_t val = readARAM(addr);
            val--;
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0x6E: { // DBNZ dp,rel
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            val--;
            writeARAM(dp, val);
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if (val != 0) {
                m_regs.pc += offset;
            }
        } break;
        case 0xFE: { // DBNZ Y,rel
            m_regs.y--;
            updateNZ(m_regs.y);
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if (m_regs.y != 0) {
                m_regs.pc += offset;
            }
        } break;
        case 0x2E: { // CBNE dp,rel
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if (m_regs.a != val) {
                m_regs.pc += offset;
            }
        } break;
        case 0xDE: { // CBNE dp+X,rel
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            uint8_t val = readARAM(addr);
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if (m_regs.a != val) {
                m_regs.pc += offset;
            }
        } break;
        
        // Shift/Rotate with addressing modes
        case 0x0B: { // ASL dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            setFlag(FLAG_C, (val & 0x80) != 0);
            val <<= 1;
            writeARAM(dp, val);
            updateNZ(val);
        } break;
        case 0x0C: { // ASL abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, (val & 0x80) != 0);
            val <<= 1;
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0x1B: { // ASL dp+X
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, (val & 0x80) != 0);
            val <<= 1;
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0x2B: { // ROL dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            bool carry = getFlag(FLAG_C);
            setFlag(FLAG_C, (val & 0x80) != 0);
            val = (val << 1) | (carry ? 1 : 0);
            writeARAM(dp, val);
            updateNZ(val);
        } break;
        case 0x2C: { // ROL abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t val = readARAM(addr);
            bool carry = getFlag(FLAG_C);
            setFlag(FLAG_C, (val & 0x80) != 0);
            val = (val << 1) | (carry ? 1 : 0);
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0x3B: { // ROL dp+X
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            uint8_t val = readARAM(addr);
            bool carry = getFlag(FLAG_C);
            setFlag(FLAG_C, (val & 0x80) != 0);
            val = (val << 1) | (carry ? 1 : 0);
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0x4B: { // LSR dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            setFlag(FLAG_C, (val & 0x01) != 0);
            val >>= 1;
            writeARAM(dp, val);
            updateNZ(val);
        } break;
        case 0x4C: { // LSR abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, (val & 0x01) != 0);
            val >>= 1;
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0x5B: { // LSR dp+X
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, (val & 0x01) != 0);
            val >>= 1;
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0x6B: { // ROR dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp);
            bool carry = getFlag(FLAG_C);
            setFlag(FLAG_C, (val & 0x01) != 0);
            val = (val >> 1) | (carry ? 0x80 : 0);
            writeARAM(dp, val);
            updateNZ(val);
        } break;
        case 0x6C: { // ROR abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t val = readARAM(addr);
            bool carry = getFlag(FLAG_C);
            setFlag(FLAG_C, (val & 0x01) != 0);
            val = (val >> 1) | (carry ? 0x80 : 0);
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0x7B: { // ROR dp+X
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = (dp + m_regs.x) & 0xFF;
            uint8_t val = readARAM(addr);
            bool carry = getFlag(FLAG_C);
            setFlag(FLAG_C, (val & 0x01) != 0);
            val = (val >> 1) | (carry ? 0x80 : 0);
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        
        // CMP with remaining addressing modes
        case 0x1E: { // CMP X,abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t val = readARAM(addr);
            uint8_t result = m_regs.x - val;
            setFlag(FLAG_C, m_regs.x >= val);
            updateNZ(result);
        } break;
        case 0x5E: { // CMP Y,abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t val = readARAM(addr);
            uint8_t result = m_regs.y - val;
            setFlag(FLAG_C, m_regs.y >= val);
            updateNZ(result);
        } break;
        case 0x69: { // CMP dp,dp
            uint8_t dp1 = readARAM(m_regs.pc++);
            uint8_t dp2 = readARAM(m_regs.pc++);
            uint8_t val1 = readARAM(dp1);
            uint8_t val2 = readARAM(dp2);
            uint8_t result = val1 - val2;
            setFlag(FLAG_C, val1 >= val2);
            updateNZ(result);
        } break;
        
        // JMP with indirect addressing
        case 0x1F: { // JMP (!abs+X)
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.x;
            uint8_t low = readARAM(addr);
            uint8_t high = readARAM(addr + 1);
            m_regs.pc = low | (high << 8);
        } break;
        
        // Bit manipulation instructions
        case 0x0A: { // OR1 C,mem.bit
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t bit = (addr >> 13) & 0x07;
            addr &= 0x1FFF;
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, getFlag(FLAG_C) || ((val >> bit) & 1) != 0);
        } break;
        case 0x2A: { // OR1 C,mem.bit (alternate)
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t bit = (addr >> 13) & 0x07;
            addr &= 0x1FFF;
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, getFlag(FLAG_C) || ((val >> bit) & 1) != 0);
        } break;
        case 0x4A: { // AND1 C,mem.bit
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t bit = (addr >> 13) & 0x07;
            addr &= 0x1FFF;
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, getFlag(FLAG_C) && ((val >> bit) & 1) != 0);
        } break;
        case 0x6A: { // AND1 C,mem.bit (alternate)
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t bit = (addr >> 13) & 0x07;
            addr &= 0x1FFF;
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, getFlag(FLAG_C) && ((val >> bit) & 1) != 0);
        } break;
        case 0x8A: { // EOR1 C,mem.bit
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t bit = (addr >> 13) & 0x07;
            addr &= 0x1FFF;
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, getFlag(FLAG_C) ^ (((val >> bit) & 1) != 0));
        } break;
        case 0xAA: { // MOV1 C,mem.bit
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t bit = (addr >> 13) & 0x07;
            addr &= 0x1FFF;
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, ((val >> bit) & 1) != 0);
        } break;
        case 0xCA: { // MOV1 mem.bit,C
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t bit = (addr >> 13) & 0x07;
            addr &= 0x1FFF;
            uint8_t val = readARAM(addr);
            if (getFlag(FLAG_C)) {
                val |= (1 << bit);
            } else {
                val &= ~(1 << bit);
            }
            writeARAM(addr, val);
        } break;
        case 0xEA: { // NOT1 mem.bit
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t bit = (addr >> 13) & 0x07;
            addr &= 0x1FFF;
            uint8_t val = readARAM(addr);
            val ^= (1 << bit);
            writeARAM(addr, val);
        } break;
        case 0x0E: { // TSET1 abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t val = readARAM(addr);
            uint8_t result = m_regs.a - val;
            setFlag(FLAG_Z, result == 0);
            setFlag(FLAG_N, (result & 0x80) != 0);
            val |= m_regs.a;
            writeARAM(addr, val);
        } break;
        case 0x4E: { // TCLR1 abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t val = readARAM(addr);
            uint8_t result = m_regs.a - val;
            setFlag(FLAG_Z, result == 0);
            setFlag(FLAG_N, (result & 0x80) != 0);
            val &= ~m_regs.a;
            writeARAM(addr, val);
        } break;
        
        // OR/AND/EOR with remaining addressing modes
        case 0x09: { // OR dp,dp
            uint8_t dp1 = readARAM(m_regs.pc++);
            uint8_t dp2 = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp1) | readARAM(dp2);
            writeARAM(dp1, val);
            updateNZ(val);
        } break;
        case 0x29: { // AND dp,dp
            uint8_t dp1 = readARAM(m_regs.pc++);
            uint8_t dp2 = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp1) & readARAM(dp2);
            writeARAM(dp1, val);
            updateNZ(val);
        } break;
        case 0x49: { // EOR dp,dp
            uint8_t dp1 = readARAM(m_regs.pc++);
            uint8_t dp2 = readARAM(m_regs.pc++);
            uint8_t val = readARAM(dp1) ^ readARAM(dp2);
            writeARAM(dp1, val);
            updateNZ(val);
        } break;
        case 0x89: { // ADC dp,dp
            uint8_t dp1 = readARAM(m_regs.pc++);
            uint8_t dp2 = readARAM(m_regs.pc++);
            uint8_t val1 = readARAM(dp1);
            uint8_t val2 = readARAM(dp2);
            uint16_t sum = val1 + val2 + (getFlag(FLAG_C) ? 1 : 0);
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_V, ((val1 ^ sum) & (val2 ^ sum) & 0x80) != 0);
            uint8_t result = sum & 0xFF;
            writeARAM(dp1, result);
            updateNZ(result);
        } break;
        case 0xA9: { // SBC dp,dp
            uint8_t dp1 = readARAM(m_regs.pc++);
            uint8_t dp2 = readARAM(m_regs.pc++);
            uint8_t val1 = readARAM(dp1);
            uint8_t val2 = readARAM(dp2);
            uint16_t diff = val1 - val2 - (getFlag(FLAG_C) ? 0 : 1);
            setFlag(FLAG_C, diff <= 0xFF);
            setFlag(FLAG_V, ((val1 ^ diff) & (val2 ^ diff) & 0x80) != 0);
            uint8_t result = diff & 0xFF;
            writeARAM(dp1, result);
            updateNZ(result);
        } break;
        
        default:
            // Unknown opcode - log once
            static int unknownCount = 0;
            if (unknownCount < 10) {
                std::cout << "[Cyc:" << std::dec << (m_cpu ? m_cpu->getCycles() : 0) << "] "
                          << "SPC700: Unknown opcode 0x" << std::hex << std::setw(2) << std::setfill('0') << (int)opcode 
                          << " at PC=0x" << std::setw(4) << (m_regs.pc - 1) << std::dec << std::endl;
                unknownCount++;
            }
            break;
    }
    
    // Wrap around at 64KB boundary
    m_regs.pc &= 0xFFFF;
}

void APU::updateTimers() {
    // Update APU timers
    for (int i = 0; i < 3; i++) {
        if (m_timers[i].enabled) {
            m_timers[i].counter++;
            if (m_timers[i].counter >= m_timers[i].target) {
                m_timers[i].counter = 0;
                // Timer overflow - trigger interrupt if enabled
            }
        }
    }
}

void APU::updateDSP() {
    if (!m_dspEnabled) return;
    
    // Update DSP processing
    // Handle DSP register writes
    for (int i = 0; i < 8; i++) {
        // Check if channel is enabled
        if (m_dspRegs[0x4C + i] & 0x80) { // Channel enable bit
            m_channels[i].enabled = true;
            m_channels[i].volume = m_dspRegs[0x0C + i]; // Volume register
            m_channels[i].frequency = (m_dspRegs[0x2C + i] << 8) | m_dspRegs[0x3C + i]; // Frequency
        } else {
            m_channels[i].enabled = false;
        }
    }
}

void APU::handleBootSequence() {
    // Handle APU boot sequence
    switch (m_bootState) {
        case BOOT_IDLE:
            // Wait for initial handshake
            break;
        case BOOT_WAIT_AA:
        case BOOT_WAIT_BB:
        case BOOT_WAIT_CC:
        case BOOT_WAIT_DD:
            // Wait for specific values
            break;
        case BOOT_COMPLETE:
            m_bootComplete = true;
            m_ready = true;
            m_initialized = true;
            break;
    }
    
    // Simplified boot sequence for Super Mario World
    static int bootCycles = 0;
    bootCycles++;
    
    if (bootCycles > 5 && !m_bootComplete) {
        m_bootComplete = true;
        m_ready = true;
        m_initialized = true;
        m_ports[0] = 0xA9; // Set ready state
        std::cout << "APU: Boot completed after " << bootCycles << " cycles" << std::endl;
    }
}

void APU::loadBootROM() {
    // Load SPC700 IPL (Initial Program Loader) boot ROM into ARAM
    // The IPL ROM is 64 bytes located at 0xFFC0-0xFFFF
    
    // Clear ARAM
    memset(m_aram.data(), 0, m_aram.size());
    
    // Simplified IPL ROM: Just write 0xAA, 0xBB and sleep
    // The actual handshake protocol is handled in writePort/readPort
    
    uint16_t iplBase = 0xFFC0;
    int i = 0;
    
    // Write 0xAA to port 0
    m_aram[iplBase + i++] = 0x8F;  // MOV dp, #imm
    m_aram[iplBase + i++] = 0xAA;  // immediate value
    m_aram[iplBase + i++] = 0xF4;  // port 0 address
    
    // Write 0xBB to port 1
    m_aram[iplBase + i++] = 0x8F;  // MOV dp, #imm
    m_aram[iplBase + i++] = 0xBB;  // immediate value
    m_aram[iplBase + i++] = 0xF5;  // port 1 address
    
    // Enter sleep mode - all handshake is handled by port I/O
    m_aram[iplBase + i++] = 0xEF;  // SLEEP
    
    // Infinite SLEEP loop
    m_aram[iplBase + i++] = 0x2F;  // BRA rel
    m_aram[iplBase + i++] = 0xFD;  // -3 offset (back to SLEEP)
    
    // Set PC to start of IPL ROM
    m_regs.pc = iplBase;
    m_regs.sp = 0xFF;
    m_regs.psw = 0x02;
    m_regs.a = 0x00;
    
    std::cout << "APU: IPL Boot ROM loaded at 0x" << std::hex << iplBase 
              << " (stub mode - handshake in I/O)" << std::dec << std::endl;
}

void APU::generateAudio() {
    // Generate audio samples for all channels
    // DSP sampling rate: 32000 Hz
    // Buffer size: 2048 samples (1024 stereo samples)
    
    // Debug: Log first call
    static bool firstCall = true;
    if (firstCall) {
        std::cout << "APU: generateAudio() called for first time" << std::endl;
        firstCall = false;
    }
    
    // Always regenerate buffer - simple approach without complex synchronization
    // The callback will read whatever is in the buffer at the time
#ifdef USE_SDL
    SDL_LockAudioDevice(m_audioDevice);  // Lock during buffer generation
#endif
    
    m_audioBufferPos = 0;
    
    // Generate actual game audio (no test tone)
    const double sampleRate = 32000.0;
    
    // Generate stereo samples (buffer size / 2 = number of stereo pairs)
    int stereoSamples = m_audioBuffer.size() / 2;
    
    for (int sample = 0; sample < stereoSamples; sample++) {
        int16_t mixedSampleL = 0;
        int16_t mixedSampleR = 0;
        
        // Mix all enabled channels
        bool anyChannelEnabled = false;
        for (int i = 0; i < 8; i++) {
            AudioChannel& ch = m_channels[i];
            uint8_t* dsp = m_dspRegs;
            
            // Update envelope and pitch for this channel
            updateEnvelopeAndPitch(i);
            
            if (ch.enabled || ch.envState != ENV_DIRECT) {
                anyChannelEnabled = true;
                
                // Get sample with pitch-based interpolation
                int16_t brrSample = getSampleWithPitch(i);
                
                // Apply envelope
                // Envelope level is 0x0000-0x7FFF (15-bit)
                int32_t finalSample = (brrSample * ch.envLevel) / 0x8000;
                
                // Apply volume and mix
                int8_t volumeL = (int8_t)dsp[i * 0x10 + 0x00]; // VOL L (signed)
                int8_t volumeR = (int8_t)dsp[i * 0x10 + 0x01]; // VOL R (signed)
                
                mixedSampleL += (finalSample * volumeL) / 128;
                mixedSampleR += (finalSample * volumeR) / 128;
            }
        }
        
        // If no channels are enabled, generate silence
        if (!anyChannelEnabled) {
            mixedSampleL = 0;
            mixedSampleR = 0;
        }
        
        // Apply master volume (only if channels are enabled, not for test tone)
        if (anyChannelEnabled) {
            int8_t masterVolL = (int8_t)m_dspRegs[0x0C]; // MVOL L
            int8_t masterVolR = (int8_t)m_dspRegs[0x1C]; // MVOL R
            mixedSampleL = (mixedSampleL * masterVolL) / 128;
            mixedSampleR = (mixedSampleR * masterVolR) / 128;
        }
        
        // Clamp to prevent overflow
        mixedSampleL = (int16_t)std::min(32767, std::max(-32768, (int32_t)mixedSampleL));
        mixedSampleR = (int16_t)std::min(32767, std::max(-32768, (int32_t)mixedSampleR));
        
        // Store stereo samples
        m_audioBuffer[m_audioBufferPos++] = mixedSampleL;
        m_audioBuffer[m_audioBufferPos++] = mixedSampleR;
    }
    
    // Mark buffer as complete
    m_audioBufferPos = m_audioBuffer.size();
    
    // Unlock audio device
#ifdef USE_SDL
    SDL_UnlockAudioDevice(m_audioDevice);
#endif
}

void APU::processAudioChannel(int channel) {
    if (channel >= 8) return;
    
    AudioChannel& ch = m_channels[channel];
    
    // Simple waveform generation
    if (ch.enabled && ch.volume > 0) {
        // Generate a simple sine wave
        int16_t sample = (int16_t)(ch.volume * 32 * sin(ch.phase * 3.14159 / 128.0));
        
        // Add to audio buffer
        if (m_audioBufferPos < m_audioBuffer.size()) {
            m_audioBuffer[m_audioBufferPos] += sample;
        }
        
        // Update phase
        ch.phase = (ch.phase + 1) % 256;
    }
}

#ifdef USE_SDL
// SDL Audio callback - called by SDL when it needs more audio data
void APU::audioCallback(void* userdata, uint8_t* stream, int len) {
    APU* apu = static_cast<APU*>(userdata);
    int16_t* output = reinterpret_cast<int16_t*>(stream);
    int samples = len / sizeof(int16_t);
    
    // Debug: Log first callback
    static bool firstCallback = true;
    if (firstCallback) {
        std::cout << "APU: audioCallback() called! Requested " << samples 
                  << " samples, buffer size " << apu->m_audioBuffer.size() << std::endl;
        firstCallback = false;
    }
    
    // Lock audio buffer access (generateAudio also locks)
#ifdef USE_SDL
    SDL_LockAudioDevice(apu->m_audioDevice);
#endif
    
    // Copy available audio data to output
    int availableSamples = std::min(samples, (int)apu->m_audioBuffer.size());
    for (int i = 0; i < samples; i++) {
        if (i < availableSamples) {
            output[i] = apu->m_audioBuffer[i];
        } else {
            output[i] = 0; // Silence if buffer underrun
        }
    }
    
    // Unlock audio buffer
#ifdef USE_SDL
    SDL_UnlockAudioDevice(apu->m_audioDevice);
#endif
}
#endif // USE_SDL

// SPC700 Helper Functions

void APU::setFlag(uint8_t flag, bool value) {
    if (value) {
        m_regs.psw |= flag;
    } else {
        m_regs.psw &= ~flag;
    }
}

bool APU::getFlag(uint8_t flag) const {
    return (m_regs.psw & flag) != 0;
}

void APU::updateNZ(uint8_t value) {
    setFlag(FLAG_Z, value == 0);
    setFlag(FLAG_N, (value & 0x80) != 0);
}

uint8_t APU::readARAM(uint16_t addr) {
    // Handle I/O port reads ($F0-$FF)
    if (addr >= 0xF0) {
        switch (addr) {
            case 0xF2: return m_dspAddr;  // DSP address register
            case 0xF3: return m_dspRegs[m_dspAddr & 0x7F];  // DSP data register
            case 0xF4: {
                uint8_t value = m_ports[0];
                if (m_spcLoadState == SPC_LOAD_COMPLETE) {
                    static int readCount = 0;
                    if (readCount < 200) {
                        std::cout << "APU: SPC read $F4 = 0x" << std::hex << (int)value << " (PC=0x" << m_regs.pc << ")" << std::dec << std::endl;
                    }
                    readCount++;
                }
                return value;  // CPU I/O port 0
            }
            case 0xF5: {
                uint8_t value = m_ports[1];
                if (m_spcLoadState == SPC_LOAD_COMPLETE) {
                    static int readCount = 0;
                    if (readCount < 200) {
                        std::cout << "APU: SPC read $F5 = 0x" << std::hex << (int)value << " (PC=0x" << m_regs.pc << ")" << std::dec << std::endl;
                    }
                    readCount++;
                }
                return value;  // CPU I/O port 1
            }
            case 0xF6: {
                uint8_t value = m_ports[2];
                if (m_spcLoadState == SPC_LOAD_COMPLETE) {
                    static int readCount = 0;
                    if (readCount < 200) {
                        std::cout << "APU: SPC read $F6 = 0x" << std::hex << (int)value << " (PC=0x" << m_regs.pc << ")" << std::dec << std::endl;
                    }
                    readCount++;
                }
                return value;  // CPU I/O port 2
            }
            case 0xF7: {
                uint8_t value = m_ports[3];
                if (m_spcLoadState == SPC_LOAD_COMPLETE) {
                    static int readCount = 0;
                    if (readCount < 200) {
                        std::cout << "APU: SPC read $F7 = 0x" << std::hex << (int)value << " (PC=0x" << m_regs.pc << ")" << std::dec << std::endl;
                    }
                    readCount++;
                }
                return value;  // CPU I/O port 3
            }
            case 0xF8: return m_timers[0].counter;  // Timer 0 counter
            case 0xF9: return m_timers[1].counter;  // Timer 1 counter
            case 0xFA: return m_timers[2].counter;  // Timer 2 counter
            case 0xFD: // Timer 0 target
            case 0xFE: // Timer 1 target
            case 0xFF: // Timer 2 target
                return m_timers[addr - 0xFD].target;
            default:
                return m_aram[addr];
        }
    }
    return m_aram[addr];
}

void APU::writeARAM(uint16_t addr, uint8_t value) {
    // Handle I/O port writes ($F0-$FF)
    if (addr >= 0xF0) {
        switch (addr) {
            case 0xF1: // Control register
                // Bit 0-2: Timer enable
                m_timers[0].enabled = (value & 0x01) != 0;
                m_timers[1].enabled = (value & 0x02) != 0;
                m_timers[2].enabled = (value & 0x04) != 0;
                // Bit 4-5: Clear ports 0-1
                if (value & 0x10) m_ports[0] = 0;
                if (value & 0x20) m_ports[1] = 0;
                // Bit 7: IPL ROM enable (ignore for now)
                m_aram[addr] = value;
                break;
            case 0xF2: // DSP address register
                m_dspAddr = value & 0x7F;
                break;
            case 0xF3: { // DSP data register
                uint8_t dspAddr = m_dspAddr & 0x7F;
                uint8_t oldValue = m_dspRegs[dspAddr];
                m_dspRegs[dspAddr] = value;
                m_dspEnabled = true;
                
                // Handle DSP register writes based on address
                handleDSPRegisterWrite(dspAddr, value, oldValue);
                break;
            }
            case 0xF4: {
                uint8_t oldPort = m_ports[0];
                m_ports[0] = value;
                if (oldPort != value) {
                    std::cout << "APU: SPC wrote $F4 = 0x" << std::hex << (int)value 
                              << " (old: 0x" << (int)oldPort << ")" << std::dec << std::endl;
                }
                break;
            }  // CPU I/O port 0
            case 0xF5: m_ports[1] = value; break;  // CPU I/O port 1
            case 0xF6: m_ports[2] = value; break;  // CPU I/O port 2
            case 0xF7: m_ports[3] = value; break;  // CPU I/O port 3
            case 0xFA: // Timer 0 target
            case 0xFB: // Timer 1 target
            case 0xFC: // Timer 2 target
                m_timers[addr - 0xFA].target = value;
                break;
            default:
                m_aram[addr] = value;
                break;
        }
    } else {
        m_aram[addr] = value;
    }
}

void APU::push(uint8_t value) {
    m_aram[0x0100 + m_regs.sp] = value;
    m_regs.sp--;
}

uint8_t APU::pop() {
    m_regs.sp++;
    return m_aram[0x0100 + m_regs.sp];
}

// BRR filter coefficients (fixed values from SNES DSP chip)
static const int16_t FILTER_COEFFICIENTS[4][2] = {
    {0, 0},
    {60, 0},
    {115, -52},
    {98, -55}
};

int16_t APU::decodeBRR(int channel) {
    AudioChannel& ch = m_channels[channel];
    
    // 1. Check if we need to start a new BRR block (16 samples per block)
    if (ch.brrNibblePos == 0) {
        if (ch.brrBytePos == 0) {
            // Read BRR block header from ARAM
            ch.brrHeader = readARAM(ch.currentAddr);
            ch.brrBytePos++; // Move to data bytes
        }
    }
    
    // 2. Extract filter and pitch shift from header
    uint8_t filterIndex = (ch.brrHeader >> 4) & 0x03;
    uint8_t pitchShift = ch.brrHeader & 0x0F;
    
    // 3. Read data byte and extract nibble
    uint8_t dataByte = readARAM(ch.currentAddr + ch.brrBytePos);
    
    int8_t nibbleVal;
    if (ch.brrNibblePos % 2 == 0) { // Upper nibble (samples 0, 2, 4, ..., 14)
        nibbleVal = (dataByte >> 4) & 0x0F;
    } else { // Lower nibble (samples 1, 3, 5, ..., 15)
        nibbleVal = dataByte & 0x0F;
        ch.brrBytePos++; // Move to next byte
    }
    
    // 4. Sign-extend 4-bit value to 8-bit
    if (nibbleVal & 0x08) {
        nibbleVal |= 0xF0; // Negative
    }
    
    // 5. Decode: (nibble << pitch) + filter
    int32_t decodedSample = (int32_t)nibbleVal << pitchShift;
    
    // 6. Apply filter (BRR formula)
    decodedSample += (ch.samplePrev[0] * FILTER_COEFFICIENTS[filterIndex][0]) >> 6;
    decodedSample += (ch.samplePrev[1] * FILTER_COEFFICIENTS[filterIndex][1]) >> 6;
    
    // 7. Clamp to 16-bit range
    int16_t output = (int16_t)std::min(32767, std::max(-32768, decodedSample));
    
    // 8. Update previous samples
    ch.samplePrev[1] = ch.samplePrev[0];
    ch.samplePrev[0] = output;
    
    ch.brrNibblePos++;
    
    // 9. Handle BRR block end (16 nibbles = 8 bytes of data + 1 header byte)
    if (ch.brrNibblePos >= 16) {
        ch.brrNibblePos = 0;
        ch.brrBytePos = 0;
        
        // Move to next BRR block (9 bytes: 1 header + 8 data)
        ch.currentAddr += 9;
        
        // Check for loop/end flags in header
        uint8_t flags = ch.brrHeader & 0x03;
        if (flags & 0x01) { // End flag
            if (flags & 0x02) { // Loop flag
                // Loop back to loop address (stored in DSP registers)
                // For now, just restart from source address
                ch.currentAddr = ch.sourceAddr;
            } else {
                // Stop playback
                ch.enabled = false;
            }
        }
    }
    
    return output;
}

void APU::updateEnvelopeAndPitch(int channel) {
    AudioChannel& ch = m_channels[channel];
    uint8_t* dsp = m_dspRegs;
    
    // 1. Handle Key On
    if (ch.keyOn) {
        ch.envState = ENV_ATTACK;
        ch.envLevel = 0;
        ch.keyOn = false;
        
        // Reset BRR decoder state
        ch.brrBytePos = 0;
        ch.brrNibblePos = 0;
        ch.samplePrev[0] = 0;
        ch.samplePrev[1] = 0;
        ch.samplePos = 0;
        ch.brrBufferIndex = 0;
        
        // Set source address from DSP registers
        uint8_t srcDir = dsp[0x5D]; // DIR register
        uint8_t srcn = dsp[channel * 0x10 + 0x04]; // SRCN register for this channel
        ch.sourceAddr = (srcDir << 8) + (srcn << 2);
        ch.currentAddr = ch.sourceAddr;
        
        // Decode first BRR block to fill buffer
        for (int i = 0; i < 16; i++) {
            ch.brrBuffer[i] = decodeBRR(channel);
        }
        ch.brrBufferIndex = 0;
    }
    
    // 2. Update pitch from DSP registers
    uint8_t pitchL = dsp[channel * 0x10 + 0x02]; // PITCH L (low byte)
    uint8_t pitchH = dsp[channel * 0x10 + 0x03]; // PITCH H (high 6 bits)
    ch.pitch = ((pitchH & 0x3F) << 8) | pitchL; // 14-bit pitch value
    
    // 3. Calculate sample step (16.16 fixed point)
    // SNES pitch calculation: step = (pitch * 32000) / (2^14 * 32000)
    // Simplified: step = pitch / 16384.0 in fixed point (16.16)
    // Base rate is 32000 Hz, pitch multiplies it
    ch.sampleStep = ((uint32_t)ch.pitch << 16) / 16384;
    
    // Minimum step to prevent division by zero
    if (ch.sampleStep == 0) ch.sampleStep = 1;
    
    // 2. Envelope state machine
    switch (ch.envState) {
        case ENV_ATTACK: {
            // Attack: increase level quickly to 0x7FFF
            uint8_t adsr1 = dsp[channel * 0x10 + 0x05]; // ADSR1 register
            uint8_t attackRate = (adsr1 >> 4) & 0x0F;
            
            if (ch.envLevel < 0x7FFF) {
                // Attack rate determines increment (simplified)
                ch.envLevel += (0x20 << attackRate);
                if (ch.envLevel > 0x7FFF) ch.envLevel = 0x7FFF;
            } else {
                ch.envState = ENV_DECAY;
            }
            break;
        }
        
        case ENV_DECAY: {
            // Decay: decrease to sustain level
            uint8_t adsr1 = dsp[channel * 0x10 + 0x05]; // ADSR1 register
            uint8_t decayRate = adsr1 & 0x07;
            uint16_t sustainTarget = ch.sustainLevel * 0x100;
            
            if (ch.envLevel > sustainTarget) {
                // Decay rate determines decrement (simplified)
                ch.envLevel -= (0x08 << decayRate);
                if (ch.envLevel < sustainTarget) ch.envLevel = sustainTarget;
            } else {
                ch.envState = ENV_SUSTAIN;
            }
            break;
        }
        
        case ENV_SUSTAIN: {
            // Sustain: maintain or slowly decrease
            uint8_t adsr2 = dsp[channel * 0x10 + 0x06]; // ADSR2 register
            uint8_t sustainRate = adsr2 & 0x1F;
            
            if (sustainRate > 0 && ch.envLevel > 0) {
                // Slow decrease (simplified)
                if ((m_spc700Cycles & 0xFF) == 0) {
                    ch.envLevel -= (sustainRate >> 1);
                    if (ch.envLevel < 0) ch.envLevel = 0;
                }
            }
            break;
        }
        
        case ENV_RELEASE: {
            // Release: decrease to 0
            if (ch.envLevel > 0) {
                ch.envLevel -= 0x40; // Fast release (simplified)
                if (ch.envLevel < 0) {
                    ch.envLevel = 0;
                    ch.enabled = false;
                }
            }
            break;
        }
        
        case ENV_DIRECT: {
            // Direct mode: use volume directly
            uint8_t vol = dsp[channel * 0x10 + 0x00]; // VOL L
            ch.envLevel = vol * 0x80;
            break;
        }
    }
}

int16_t APU::getSampleWithPitch(int channel) {
    AudioChannel& ch = m_channels[channel];
    
    // Advance sample position by step
    ch.samplePos += ch.sampleStep;
    
    // Extract integer and fractional parts (16.16 fixed point)
    uint16_t sampleIndex = (ch.samplePos >> 16) & 0xFFFF;
    uint16_t fraction = ch.samplePos & 0xFFFF;
    
    // While we need more samples in the buffer
    while (sampleIndex >= 16) {
        // Decode next BRR block (16 samples)
        for (int i = 0; i < 16; i++) {
            ch.brrBuffer[i] = decodeBRR(channel);
        }
        sampleIndex -= 16;
        ch.samplePos -= (16 << 16); // Adjust position
    }
    
    // Get current and next sample for interpolation
    int16_t sample0 = ch.brrBuffer[sampleIndex];
    int16_t sample1 = (sampleIndex < 15) ? ch.brrBuffer[sampleIndex + 1] : decodeBRR(channel);
    
    // Linear interpolation
    // result = sample0 + (sample1 - sample0) * fraction / 65536
    int32_t diff = sample1 - sample0;
    int32_t interpolated = sample0 + ((diff * fraction) >> 16);
    
    // Clamp to 16-bit range
    int16_t result = (int16_t)std::min(32767, std::max(-32768, interpolated));
    
    return result;
}

// DSP Register Mapping:
// Channels: $00-$0F (ch0), $10-$1F (ch1), ..., $70-$7F (ch7)
//   Each channel: VOL L($00), VOL R($01), PITCH L($02), PITCH H($03), 
//                  SRCN($04), ADSR1($05), ADSR2($06), GAIN($07)
// Global registers:
//   $0C: MVOL L (Master Volume Left)
//   $1C: MVOL R (Master Volume Right)
//   $2C: EVOL L (Echo Volume Left)
//   $3C: EVOL R (Echo Volume Right)
//   $4C: KON (Key On - bits 0-7 = channels 0-7)
//   $5C: KOF (Key Off - bits 0-7 = channels 0-7)
//   $5D: DIR (Sample Directory Address - page $XX00)
//   $5E: ESA (Echo Start Address - page $XX00)
//   $6C: EDL (Echo Delay - lower 4 bits)
//   $7C: EFB (Echo Feedback - signed)
//   $0D: EON (Echo Enable - bits 0-7 = channels 0-7)
//   $2D: FLG (Reset/Mute/Noise clock - bit 7=mute, bit 6=reset)
//   $3D: NON (Noise Enable - bits 0-7 = channels 0-7)
//   $4D: PMON (Pitch Modulation Enable - bits 0-7 = channels 0-7)
//   $5D: DIR (already mentioned)
//   $6D: KOF (already mentioned)
//   $7D: KOF (duplicate?)
//   $0E-$0F: FIR filter coefficients
//   $1E-$1F: FIR filter coefficients
//   $2E-$2F: FIR filter coefficients
//   $3E-$3F: FIR filter coefficients
//   $4E-$4F: FIR filter coefficients
//   $5E-$5F: FIR filter coefficients (ESA overlaps)
//   $6E-$6F: FIR filter coefficients
//   $7E-$7F: FIR filter coefficients

void APU::handleDSPRegisterWrite(uint8_t addr, uint8_t value, uint8_t oldValue) {
    // Channel-specific registers (each channel uses $00-$0F, $10-$1F, etc.)
    if ((addr & 0x0F) < 0x08) {
        int channel = addr >> 4;
        if (channel < 8) {
            uint8_t reg = addr & 0x0F;
            
            switch (reg) {
                case 0x00: // VOL L (signed, -128 to +127)
                case 0x01: // VOL R (signed, -128 to +127)
                    // Volume registers are read during audio generation
                    break;
                    
                case 0x02: // PITCH L (low byte of 14-bit pitch)
                case 0x03: // PITCH H (high byte of 14-bit pitch, bits 6-7)
                    // Pitch registers are used in pitch calculation
                    break;
                    
                case 0x04: // SRCN (Sample Number, 0-255)
                    // Source number is used when key is pressed
                    break;
                    
                case 0x05: { // ADSR1 (Attack Rate[4], Decay Rate[3])
                    // ADSR1 is used in envelope processing
                    break;
                }
                    
                case 0x06: { // ADSR2 (Sustain Rate[5], Sustain Level[3])
                    // Extract sustain level (bits 5-7)
                    m_channels[channel].sustainLevel = (value >> 5) & 0x07;
                    break;
                }
                    
                case 0x07: // GAIN (Gain mode and value)
                    // GAIN is used for direct volume control mode
                    break;
            }
        }
    }
    
    // Global registers
    switch (addr) {
        case 0x0C: // MVOL L (Master Volume Left, signed)
        case 0x1C: // MVOL R (Master Volume Right, signed)
            // Master volume is applied during mixing
            break;
            
        case 0x2C: // EVOL L (Echo Volume Left, signed)
        case 0x3C: // EVOL R (Echo Volume Right, signed)
            // Echo volume is used when echo is enabled
            break;
            
        case 0x4C: { // KON (Key On)
            // Key On - start playback for enabled channels
            for (int i = 0; i < 8; i++) {
                if (value & (1 << i)) {
                    m_channels[i].keyOn = true;
                    m_channels[i].enabled = true;
                }
            }
            break;
        }
        
        case 0x5C: { // KOF (Key Off)
            // Key Off - enter release phase for enabled channels
            for (int i = 0; i < 8; i++) {
                if (value & (1 << i)) {
                    m_channels[i].envState = ENV_RELEASE;
                    m_channels[i].keyOn = false;
                }
            }
            break;
        }
        
        case 0x0D: // EON (Echo Enable)
            // Echo enable flags - stored in register, used during echo processing
            break;
            
        case 0x2D: { // FLG (Flags)
            // Bit 7: MUTE (mute all channels)
            // Bit 6: ECEN (Echo Enable - master enable)
            // Bit 5-0: Noise clock divider
            if (value & 0x80) {
                // Mute all channels
                for (int i = 0; i < 8; i++) {
                    m_channels[i].enabled = false;
                }
            }
            break;
        }
        
        case 0x3D: // NON (Noise Enable)
            // Noise enable flags - stored in register, used when noise is implemented
            break;
            
        case 0x4D: // PMON (Pitch Modulation Enable)
            // Pitch modulation enable flags - stored in register
            break;
        
        case 0x5D: // DIR (Sample Directory Address)
            // Sample directory page - used when calculating source address
            break;
            
        case 0x5E: // ESA (Echo Start Address)
            // Echo buffer start address - used when echo is enabled
            break;
            
        case 0x6C: // EDL (Echo Delay)
            // Echo delay in samples - lower 4 bits
            break;
            
        case 0x7C: // EFB (Echo Feedback)
            // Echo feedback amount (signed) - used in echo processing
            break;
            
        // FIR filter coefficients ($0E-$0F, $1E-$1F, ..., $7E-$7F)
        // Each coefficient is signed 8-bit
        case 0x0E: case 0x0F:
        case 0x1E: case 0x1F:
        case 0x2E: case 0x2F:
        case 0x3E: case 0x3F:
        case 0x4E: case 0x4F:
        case 0x6E: case 0x6F:
        case 0x7E: case 0x7F:
            // FIR filter coefficients are stored and used during echo processing
            break;
    }
}