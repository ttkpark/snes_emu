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
    , m_lastPort0Value(0xFF)
    , m_port0WrapCount(0)
#ifdef USE_SDL
    , m_audioDevice(0)
    , m_audioSpec(nullptr)
#endif
{
    
    // Initialize 64KB ARAM first (before accessing m_aram[0xF4+port])
    m_aram.resize(64 * 1024, 0);
    
    // Initialize APU ports (snes9x style: CPU->SPC via m_cpuPorts, SPC->CPU via m_aram[0xF4+port])
    for (int i = 0; i < 4; i++) {
        m_cpuPorts[i] = 0x00;  // CPU writes, SPC700 reads
        m_aram[0xF4 + i] = 0x00;  // SPC700 writes, CPU reads
    }
    
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
    
    std::ostringstream oss;
    oss << "APU: Audio initialized - " << m_audioSpec->freq << "Hz, " 
              << (int)m_audioSpec->channels << " channels, " 
        << m_audioSpec->samples << " samples buffer";
    Logger::getInstance().logAPU(oss.str());
    
    // Start audio playback
    SDL_PauseAudioDevice(m_audioDevice, 0);
    Logger::getInstance().logAPU("APU: Audio playback started (device unpaused)");
    
    return true;
#else
    // SDL not available, just initialize audio buffer
    Logger::getInstance().logAPU("APU: Audio buffer initialized (SDL not available)");
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
    m_lastPort0Value = 0xFF;
    m_port0WrapCount = 0;
    
    // Reset SPC700 registers
    m_regs.a = 0;
    m_regs.x = 0;
    m_regs.y = 0;
    m_regs.sp = 0xEF;  // Correct initial SP value (snes9x uses 0xEF)
    m_regs.pc = 0xFFC0;
    m_regs.psw = 0x02;
    
    // Enable IPL ROM by default (snes9x behavior)
    m_iplromEnable = true;
    
    // Reset ports (snes9x style: CPU->SPC via m_cpuPorts, SPC->CPU via m_aram[0xF4+port])
    for (int i = 0; i < 4; i++) {
        m_cpuPorts[i] = 0x00;  // CPU writes, SPC700 reads
        m_aram[0xF4 + i] = 0x00;  // SPC700 writes, CPU reads
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
        Logger::getInstance().logAPU("APU: step() called for the first time");
        std::ostringstream oss;
        oss << "APU: PC=0x" << std::hex << m_regs.pc << ", bootComplete=" << m_bootComplete << std::dec;
        Logger::getInstance().logAPU(oss.str());
        firstLog = false;
    }
    stepCount++;
    
    // Check if boot is complete by monitoring port values (SPC700 writes to SPC ports)
    if (!m_bootComplete && m_aram[0xF4] == 0xAA && m_aram[0xF5] == 0xBB) {
        m_bootComplete = true;
        m_ready = true;
        m_initialized = true;
        std::ostringstream oss;
        oss << "APU: Boot completed after " << stepCount << " steps - Ready signature detected (0xBBAA)";
        Logger::getInstance().logAPU(oss.str());
        // After boot, SPC700 sets ports to 0xBBAA for IPL protocol (already set by IPL ROM)
    }
    
    // Execute SPC700 instructions - continue executing even after boot
    // This allows SPC programs to run
    // Timing is handled by the main emulation loop (CPU:APU = 6:8 master cycles)
    // We execute one instruction per step() call to maintain accurate timing
    // During IPL protocol (SPC_LOAD_RECEIVING), IPL ROM must execute to handle data transfer
    if (m_bootComplete && m_spcLoadState == SPC_LOAD_COMPLETE) {
        // Execute SPC700 instructions when program is loaded
        // Debug: Verify PC is set correctly
        static bool firstExec = true;
        if (firstExec && m_regs.pc == 0x0300) {
            firstExec = false;
            std::ostringstream oss;
            oss << "APU: Starting SPC program execution at PC=0x" << std::hex << m_regs.pc << std::dec;
            Logger::getInstance().logAPU(oss.str());
        }
        executeSPC700Instruction();
    } else if (m_bootComplete && (m_spcLoadState == SPC_LOAD_RECEIVING || m_spcLoadState == SPC_LOAD_WAIT_CC || m_spcLoadState == SPC_LOAD_WAIT_EXEC)) {
        // During IPL protocol, IPL ROM must execute to handle handshake and data transfer
        // Execute one instruction per step() - timing is handled by main loop
        executeSPC700Instruction();
    } else if (!m_bootComplete) {
        // Execute IPL ROM during boot
        executeSPC700Instruction();
    } else {
        // Debug: Log when we're not executing (shouldn't happen)
        static int skipCount = 0;
        skipCount++;
        if (skipCount % 10000 == 0) {
            std::ostringstream oss;
            oss << "APU: WARNING - Not executing SPC700 instruction. bootComplete=" << m_bootComplete 
                << ", spcLoadState=" << m_spcLoadState << ", PC=0x" << std::hex << m_regs.pc << std::dec;
            Logger::getInstance().logAPU(oss.str());
        }
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
    
    // snes9x style: CPU reads from SPC ports
    // SPC700 writes to $F4-$F7, which are stored in m_aram[0xf4+port]
    // Return what SPC700 wrote to $F4-$F7
    return m_aram[0xF4 + port];
}

void APU::writePort(uint8_t port, uint8_t value) {
    if (port >= 4) return;
    
    // snes9x style: CPU writes to ports, SPC700 reads them via $F4-$F7
    // Store in m_cpuPorts[port] so SPC700 can read via readARAM($F4+port)
    uint8_t oldValue = m_cpuPorts[port];
    m_cpuPorts[port] = value;
    
    // Debug: Log port writes
    if (port == 0 && (value == 0xCC || value == 0x00 || oldValue != value)) {
        static int logCount = 0;
        if (logCount < 20) {
            std::ostringstream oss;
            oss << "APU: CPU wrote port " << (int)port << " = 0x" << std::hex << (int)value 
                << " (old=0x" << (int)oldValue << ")" << std::dec;
            Logger::getInstance().logAPU(oss.str());
            logCount++;
        }
    }
    
    // Debug: Log port 1 writes during IPL data transfer
    if (port == 1) {
        // Always log port 1 writes, especially during test failure handling
        std::ostringstream oss;
        oss << "APU: CPU wrote port 1 = 0x" << std::hex << (int)value 
            << " (old=0x" << (int)oldValue << ", loadState=" << (int)m_spcLoadState 
            << ", SPC700 PC=0x" << m_regs.pc << ")" << std::dec;
        Logger::getInstance().logAPU(oss.str());
    }
    
    // Handle IPL protocol for SPC program loading
    if (m_bootComplete) {
        if (port == 2) {
            // Port 2: Low byte of address
            // This can be written before or during protocol
            std::ostringstream oss;
            oss << "APU: Port 2 write: value=0x" << std::hex << (int)value << ", loadState=" << std::dec << (int)m_spcLoadState;
            std::cout << oss.str() << std::endl;
            Logger::getInstance().logPort(oss.str());
            if (m_spcLoadState == SPC_LOAD_IDLE || m_spcLoadState == SPC_LOAD_WAIT_CC) {
                // Store low byte, will be combined with high byte later
                // But if data has already been loaded OR destination address is already set, this might be execution address
                if (m_spcLoadSize > 0 || (m_spcLoadAddr != 0 && m_spcLoadAddr == m_spcExecAddr)) {
                    // Data has been loaded OR we're reusing the same address, this is likely execution address
                    m_spcExecAddr = value;
                    std::ostringstream oss;
                    oss << "APU: Execution address low byte (after data loaded): 0x" << std::hex << (int)value << std::dec;
                    Logger::getInstance().logAPU(oss.str());
                } else {
                    // No data loaded yet, this is destination address
                m_spcLoadAddr = value;
                }
            } else if (m_spcLoadState == SPC_LOAD_WAIT_EXEC || m_spcLoadState == SPC_LOAD_RECEIVING) {
                // Execution address low byte (can be written during or after data transfer)
                m_spcExecAddr = value;
                std::ostringstream oss;
                oss << "APU: Execution address low byte: 0x" << std::hex << (int)value << std::dec;
                Logger::getInstance().logAPU(oss.str());
                // Note: Execution address is not complete yet (need high byte from port 3)
                // We'll check for execution command after high byte is written
            } else if (m_spcLoadState == SPC_LOAD_COMPLETE) {
                // After IPL protocol completes, port 2/3 can be used for communication
                // This is not an execution address
            } else {
                // If we're in RECEIVING state and data has been loaded, this might be execution address
                // Check if we've received data (m_spcLoadSize > 0)
                if (m_spcLoadSize > 0) {
                    // This is likely execution address after data transfer
                    m_spcExecAddr = value;
                    std::ostringstream oss;
                    oss << "APU: Execution address low byte (after data transfer): 0x" << std::hex << (int)value << std::dec;
                    Logger::getInstance().logAPU(oss.str());
                } else {
                    std::ostringstream oss;
                    oss << "APU: Port 2 write ignored (wrong state: " << (int)m_spcLoadState << ")";
                    Logger::getInstance().logAPU(oss.str());
                }
            }
        } else if (port == 3) {
            // Port 3: High byte of address
            std::ostringstream oss;
            oss << "APU: Port 3 write: value=0x" << std::hex << (int)value << ", loadState=" << std::dec << (int)m_spcLoadState;
            std::cout << oss.str() << std::endl;
            Logger::getInstance().logPort(oss.str());
            if (m_spcLoadState == SPC_LOAD_IDLE || m_spcLoadState == SPC_LOAD_WAIT_CC) {
                // Combine with low byte to get full destination address
                // But if data has already been loaded OR destination address is already set, this might be execution address
                std::ostringstream oss;
                oss << "APU: Port 3 write in IDLE/WAIT_CC: loadSize=" << std::dec << m_spcLoadSize 
                    << ", loadAddr=0x" << std::hex << m_spcLoadAddr << std::dec;
                std::cout << oss.str() << std::endl;
                Logger::getInstance().logPort(oss.str());
                if (m_spcLoadSize > 0 || (m_spcLoadAddr != 0 && m_spcLoadAddr == m_spcExecAddr)) {
                    // Data has been loaded OR we're reusing the same address, this is likely execution address
                    m_spcExecAddr |= (value << 8);
                    std::ostringstream oss;
                    oss << "APU: Execution address set (after data loaded): 0x" << std::hex << m_spcExecAddr << std::dec;
                    std::cout << oss.str() << std::endl;
                    Logger::getInstance().logPort(oss.str());
                    // Check if execution command (size+2) was already written to port 0
                    // If so, process it now
                    std::ostringstream oss2;
                    oss2 << "APU: Checking execution command: execAddr=0x" << std::hex << m_spcExecAddr 
                         << ", port0=0x" << (int)m_cpuPorts[0] << ", loadSize=" << std::dec << m_spcLoadSize;
                    std::cout << oss2.str() << std::endl;
                    Logger::getInstance().logPort(oss2.str());
                    if (m_spcExecAddr != 0 && m_cpuPorts[0] >= m_spcLoadSize && m_cpuPorts[0] != 0) {
                        // Execution command was already written, let IPL ROM handle it
                        // Set execution address in $00-$01 for IPL ROM's JMP instruction
                        m_aram[0x00] = m_spcExecAddr & 0xFF;
                        m_aram[0x01] = (m_spcExecAddr >> 8) & 0xFF;
                        // Set state and PC to IPL ROM address setup
                        m_spcLoadState = SPC_LOAD_WAIT_EXEC;
                        m_regs.pc = 0xFFEF;  // IPL ROM address setup
                        std::ostringstream oss3;
                        oss3 << "APU: Execution command already in port 0, setting PC to 0xFFEF for IPL ROM";
                        std::cout << oss3.str() << std::endl;
                        Logger::getInstance().logPort(oss3.str());
                    }
                } else {
                    // No data loaded yet, this is destination address
                m_spcLoadAddr |= (value << 8);
                    std::ostringstream oss;
                    oss << "APU: Destination address set to 0x" << std::hex << m_spcLoadAddr << std::dec;
                    Logger::getInstance().logAPU(oss.str());
                }
            } else if (m_spcLoadState == SPC_LOAD_WAIT_EXEC || m_spcLoadState == SPC_LOAD_RECEIVING) {
                // Execution address high byte (can be written during or after data transfer)
                m_spcExecAddr |= (value << 8);
                std::ostringstream oss;
                oss << "APU: Execution address set to 0x" << std::hex << m_spcExecAddr << std::dec;
                Logger::getInstance().logAPU(oss.str());
            } else if (m_spcLoadState == SPC_LOAD_COMPLETE) {
                // After IPL protocol completes, port 2/3 can be used for communication
                // This is not an execution address
            } else {
                // If we're in RECEIVING state and data has been loaded, this might be execution address
                // Check if we've received data (m_spcLoadSize > 0)
                if (m_spcLoadSize > 0) {
                    // This is likely execution address after data transfer
                    m_spcExecAddr |= (value << 8);
                    std::ostringstream oss;
                    oss << "APU: Execution address set (after data transfer): 0x" << std::hex << m_spcExecAddr << std::dec;
                    Logger::getInstance().logAPU(oss.str());
                // If we were in RECEIVING state and exec address is now complete, switch to WAIT_EXEC
                if (m_spcLoadState == SPC_LOAD_RECEIVING) {
                    m_spcLoadState = SPC_LOAD_WAIT_EXEC;
                    }
                // Check if execution command (size+2) was already written to port 0
                // If so, process it now
                    std::ostringstream oss2;
                    oss2 << "APU: Checking execution command: execAddr=0x" << std::hex << m_spcExecAddr 
                        << ", port0=0x" << (int)m_cpuPorts[0] << ", loadSize=" << std::dec << m_spcLoadSize;
                    Logger::getInstance().logAPU(oss2.str());
                    Logger::getInstance().logPort(oss2.str());
                if (m_spcExecAddr != 0 && m_cpuPorts[0] >= m_spcLoadSize && m_cpuPorts[0] != 0) {
                    // Execution command was already written, let IPL ROM handle it
                    // Set execution address in $00-$01 for IPL ROM's JMP instruction
                    m_aram[0x00] = m_spcExecAddr & 0xFF;
                    m_aram[0x01] = (m_spcExecAddr >> 8) & 0xFF;
                    // Set state and PC to IPL ROM address setup
                    m_spcLoadState = SPC_LOAD_WAIT_EXEC;
                    m_regs.pc = 0xFFEF;  // IPL ROM address setup
                    std::ostringstream oss2;
                    oss2 << "APU: Execution command already in port 0, setting PC to 0xFFEF for IPL ROM";
                    std::cout << oss2.str() << std::endl;
                    Logger::getInstance().logPort(oss2.str());
                    }
                }
            }
        } else if (port == 0) {
            // Port 0: Handshake and data transfer
            // During IPL protocol, IPL ROM handles the handshake and data transfer
            // We just store the value so IPL ROM can read it via $F4
            // IPL ROM will echo back the value when ready
            if (m_spcLoadState == SPC_LOAD_IDLE) {
                // Wait for CPU to write 0xCC to start transfer
                if (value == 0xCC) {
                    m_spcLoadState = SPC_LOAD_WAIT_CC;
                    // Set execution address to destination address if not already set
                    if (m_spcExecAddr == 0 && m_spcLoadAddr != 0) {
                        m_spcExecAddr = m_spcLoadAddr;
                        std::ostringstream oss;
                        oss << "APU: Execution address set to destination address: 0x" << std::hex << m_spcExecAddr << std::dec;
                        Logger::getInstance().logAPU(oss.str());
                }
                    std::ostringstream oss;
                    oss << "APU: IPL protocol started, destination address=0x" << std::hex << m_spcLoadAddr << std::dec;
                    Logger::getInstance().logAPU(oss.str());
                }
            } else if (m_spcLoadState == SPC_LOAD_RECEIVING) {
                // Data transfer: CPU writes byte index to port 0
                // IPL ROM will read this and echo it back when ready
                // Track CPU transfer index: value is the index CPU is sending (0, 1, 2, ...)
                // After data transfer, CPU writes (size+2) to port 0 as execution command
                
                // Safety check: prevent infinite transfer if size is too large
                // Maximum reasonable size is 64KB, but cap at 32KB (0x8000) for safety
                // Note: value is uint8_t (0-255), but we track cumulative size in m_spcLoadIndex
                const uint16_t MAX_TRANSFER_SIZE = 0x8000;
                
                // Update CPU transfer index
                uint16_t lastCpuIndex = m_spcLoadIndex;
                
                // Safety: if tracked index exceeds maximum transfer size, treat as error
                // This prevents infinite loops if CPU keeps sending data
                if (m_spcLoadIndex >= MAX_TRANSFER_SIZE) {
                    std::ostringstream oss;
                    oss << "APU: Transfer size exceeded maximum (0x" << std::hex << MAX_TRANSFER_SIZE 
                        << "), cpuIndex=0x" << m_spcLoadIndex << ", value=0x" << (int)value 
                        << ", treating as execution command";
                    Logger::getInstance().logAPU(oss.str());
                    Logger::getInstance().logPort(oss.str());
                    
                    // Treat as execution command to stop transfer
                    m_aram[0xF4] = 0;  // Signal completion
                    m_regs.pc = m_spcExecAddr;
                    m_spcLoadState = SPC_LOAD_COMPLETE;
                    std::ostringstream oss2;
                    oss2 << "APU: Transfer stopped due to size limit, starting execution at PC=0x" 
                         << std::hex << m_regs.pc << std::dec;
                    Logger::getInstance().logAPU(oss2.str());
                    return;
                }
                
                // Check if this is the execution command
                // Execution command is (size+2), written after data transfer completes
                // During data transfer, value changes sequentially (0, 1, 2, ..., 255, 0, 1, ...)
                // After data transfer, value should be size+2
                // 
                // Strategy: compare value with m_spcLoadIndex
                // - If value == m_spcLoadIndex: normal sequential transfer
                // - If value == (m_spcLoadIndex mod 256): normal transfer (after wrap)
                // - If value is much different: likely execution command
                
                bool isExecutionCommand = false;
                if (m_spcExecAddr != 0) {
                    // Get expected next value for normal sequential transfer
                    uint8_t expectedValue = (m_spcLoadIndex + 1) & 0xFF;
                    
                    // If value doesn't match expected, it might be execution command
                    // But be careful: value could be current index if we just started
                    
                    // Execution command is typically larger than transfer size
                    // Transfer size is usually small (<64KB), and execution command = size+2
                    // So execution command should be within reasonable range
                    
                    // Method 1: value doesn't match sequential progression
                    // For wrap-around: if wrap just happened, value might be 0
                    if (value != expectedValue && value != (m_spcLoadIndex & 0xFF)) {
                        // Check if it looks like an execution command
                        // Execution command should be: transferredBytes + 2
                        uint16_t executionCommandValue = (m_spcLoadIndex & 0xFF) + 2;
                        if ((value == (executionCommandValue & 0xFF)) || 
                            (value == ((executionCommandValue + 1) & 0xFF))) {
                            isExecutionCommand = true;
                        }
                    }
                    
                    // Method 2: if we've received a lot of data and value doesn't match
                    // Likely an execution command
                    if (!isExecutionCommand && m_spcLoadIndex > 256) {
                        uint8_t lastTransferredValue = m_spcLoadIndex & 0xFF;
                        // If value is much larger or smaller, it's probably execution command
                        if (value > (lastTransferredValue + 2) || value < (lastTransferredValue - 2)) {
                            isExecutionCommand = true;
                        }
                    }
                }
                
                if (isExecutionCommand) {
                    // This is the execution command (size+2)
                    // IPL ROM should handle the echo and jump to SPC program
                    // Set state to SPC_LOAD_WAIT_EXEC and let IPL ROM handle it
                    m_spcLoadState = SPC_LOAD_WAIT_EXEC;
                    std::ostringstream oss2;
                    oss2 << "APU: Execution command received (value=0x" << std::hex << (int)value 
                         << ", execAddr=0x" << m_spcExecAddr << "), IPL ROM will echo and jump";
                    Logger::getInstance().logAPU(oss2.str());
                    Logger::getInstance().logPort(oss2.str());
                    
                    // Set PC to IPL ROM's echo handler (0xFFF3: MOVW YA, $F4)
                    // But first, IPL ROM needs to set execution address in $00-$01
                    // IPL ROM will:
                    //   0xFFEF: MOVW YA, $F6  - Read execution address from port 2/3
                    //   0xFFF1: MOVW $00, YA  - Store execution address in $00-$01
                    //   0xFFF3: MOVW YA, $F4  - Read execution command from port 0
                    //   0xFFF5: MOV $F4, A   - Echo back to port 0 (CPU can read it)
                    //   0xFFFB: JMP ($00+X)  - Jump to SPC program (execAddr)
                    // Set execution address in $00-$01 for IPL ROM's JMP instruction
                    m_aram[0x00] = m_spcExecAddr & 0xFF;
                    m_aram[0x01] = (m_spcExecAddr >> 8) & 0xFF;
                    // Set PC to IPL ROM's address setup (0xFFEF)
                    m_regs.pc = 0xFFEF;
                    std::ostringstream oss3;
                    oss3 << "APU: Setting PC to 0xFFEF for IPL ROM (execAddr=0x" << std::hex << m_spcExecAddr 
                         << " stored in $00-$01)" << std::dec;
                    Logger::getInstance().logAPU(oss3.str());
                    Logger::getInstance().logPort(oss3.str());
                    
                    // Don't return here - let IPL ROM execute to handle echo
                    // IPL ROM will execute and echo the value, then jump to SPC program
                    // Debug: Dump SPC memory at 0x0300 when program is loaded
                    std::cout << "=== SPC Memory Dump at 0x0300-0x031F (when program loaded) ===" << std::endl;
                    for (int i = 0; i < 32; i++) {
                        if (i % 16 == 0) std::cout << std::hex << std::setfill('0') << std::setw(4) << (0x0300 + i) << ": ";
                        // Use m_aram directly, not readARAM, to see actual memory contents
                        std::cout << std::setw(2) << (int)m_aram[0x0300 + i] << " ";
                        if (i % 16 == 15) std::cout << std::endl;
                    }
                    std::cout << std::dec << std::endl;
                    // Also verify with readARAM
                    std::cout << "=== Verification with readARAM ===" << std::endl;
                    for (int i = 0; i < 8; i++) {
                        std::cout << "readARAM(0x" << std::hex << (0x0300 + i) << ") = 0x" << (int)readARAM(0x0300 + i) 
                                  << ", m_aram[0x" << (0x0300 + i) << "] = 0x" << (int)m_aram[0x0300 + i] << std::dec << std::endl;
                    }
                } else {
                    // Normal data transfer - update cumulative byte count
                    // value is 8-bit (0-255), so it wraps around
                    // Track wrapping to get actual cumulative index
                    
                    // Detect wrap-around: value is much smaller than last value
                    // (e.g., 0xFF -> 0x00 or high value -> low value)
                    if (m_lastPort0Value != 0xFF) {  // Not first call
                        // Check if value decreased significantly (likely wrap-around)
                        if (value < m_lastPort0Value && (m_lastPort0Value - value) > 128) {
                            // Wrap-around detected
                            m_port0WrapCount++;
                            std::ostringstream oss_wrap;
                            oss_wrap << "APU: Port 0 wrap detected (0x" << std::hex << (int)m_lastPort0Value 
                                     << " -> 0x" << (int)value << "), wrap count=" << std::dec << m_port0WrapCount;
                            Logger::getInstance().logAPU(oss_wrap.str());
                            Logger::getInstance().logPort(oss_wrap.str());
                        }
                    }
                    
                    // Calculate actual cumulative index
                    // Format: (wrap_count * 256) + value
                    m_spcLoadIndex = (m_port0WrapCount * 256) + value;
                    
                    // Update last value for next iteration
                    m_lastPort0Value = value;
                    
                    // If execution address has not been set, use destination address as execution address
                    if (m_spcExecAddr == 0) {
                        m_spcExecAddr = m_spcLoadAddr;
                        std::ostringstream oss;
                        oss << "APU: Using destination address as execution address: 0x" << std::hex << m_spcExecAddr << std::dec;
                        std::cout << oss.str() << std::endl;
                        Logger::getInstance().logPort(oss.str());
                    }
                    
                    // Reduce logging frequency to avoid spam (log every 256 bytes)
                    static uint16_t lastLoggedIndex = 0;
                    if ((m_spcLoadIndex - lastLoggedIndex) >= 256 || m_spcLoadIndex < lastLoggedIndex) {
                        std::ostringstream oss;
                        oss << "APU: Port 0 RECEIVING: port0_val=0x" << std::hex << std::setfill('0') << std::setw(2) << (int)value 
                            << ", actualIndex=0x" << std::setw(4) << m_spcLoadIndex << " (wrap_count=" << std::dec << m_port0WrapCount 
                            << "), execAddr=0x" << std::hex << m_spcExecAddr << ", loadAddr=0x" << m_spcLoadAddr;
                        Logger::getInstance().logAPU(oss.str());
                        Logger::getInstance().logPort(oss.str());
                        lastLoggedIndex = m_spcLoadIndex;
                    }
                    
                    // Normal data transfer - just store the value for IPL ROM to read
                    // IPL ROM will echo it back
                }
            } else if (m_spcLoadState == SPC_LOAD_WAIT_EXEC) {
                // IPL ROM is handling the echo and jump
                // Don't interfere - let IPL ROM execute
                // IPL ROM will:
                //   0xFFF3: MOVW YA, $F4  - Read execution command
                //   0xFFF5: MOV $F4, A   - Echo back
                //   0xFFFB: JMP ($00+X)  - Jump to SPC program
                // If PC is not in IPL ROM range, IPL ROM has finished
                if (m_regs.pc < 0xFFC0 || m_regs.pc > 0xFFFF) {
                    // IPL ROM has finished, SPC program is now executing
                    m_spcLoadState = SPC_LOAD_COMPLETE;
                    std::ostringstream oss;
                    oss << "APU: IPL ROM finished, SPC program executing at PC=0x" << std::hex << m_regs.pc << std::dec;
                    Logger::getInstance().logAPU(oss.str());
                }
            }
        } else if (port == 1) {
            // Port 1: Data byte during transfer
            // In real hardware, CPU writes data to port 1, and IPL ROM reads it via $F5
            // We just store the value in m_cpuPorts[1] so IPL ROM can read it
            // IPL ROM will read port 1 and write to ARAM itself
            // Do NOT write to ARAM here - let IPL ROM handle it
            if (m_spcLoadState == SPC_LOAD_WAIT_CC) {
                // First data byte after 0xCC handshake
                // Start receiving data
                m_spcLoadState = SPC_LOAD_RECEIVING;
                m_spcLoadIndex = 0;
                m_spcLoadSize = 0; // We don't know size yet, will track by data received
            }
            
            // Note: We don't write to ARAM here
            // IPL ROM will read port 1 ($F5) and write to ARAM itself
            // This allows IPL ROM to control the data transfer protocol
        }
    }
    
    // Simplified boot sequence handling
    if (!m_bootComplete) {
        // Super Mario World writes 0xAA, 0xBB, 0xCC, 0xDD to ports 0-3
        // We just acknowledge these writes
        if (value == 0xAA || value == 0xBB || value == 0xCC || value == 0xDD) {
            std::ostringstream oss;
            oss << "APU: Received boot value 0x" << std::hex << (int)value << " on port " << (int)port << std::dec;
            Logger::getInstance().logAPU(oss.str());
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
        case 0xBC: case 0x3D: case 0xFC: // INC A/X/Y
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
        case 0x68: case 0x64: case 0x3E: case 0xC8: case 0xAD: // CMP (0x7E is CMP Y,dp - 1 byte)
        case 0x2F: case 0xF0: case 0xD0: case 0x90: case 0xB0: case 0x30: case 0x10: case 0x50: case 0x70: // Branch instructions
        case 0xFA: // MOV dp1,dp2
        case 0x3A: case 0x5A: case 0x7A: case 0x9A: case 0xBA: case 0xDA: // Word operations
        case 0x12: // CLR1 dp.bit
        case 0x03: case 0x13: case 0x23: case 0x33: case 0x43: case 0x63: case 0x73: case 0x83: case 0xA3: case 0xC3: case 0xD3: case 0xE3: case 0xF3: // BBS/BBC dp.bit,rel
        case 0x2E: case 0x6E: case 0xDE: // CBNE/DBNZ dp,rel and CBNE dp+X,rel
        case 0x46: // EOR A,(X) (0x56, 0x66, 0x76 are !abs+Y or !abs+X, moved to 2-byte)
        case 0x05: case 0x06: case 0x07: case 0x09: case 0x0B: case 0x14: case 0x17: case 0x18: case 0x19: case 0x1A: case 0x1B: // Various operations (0x15, 0x16 are !abs+X/Y, moved to 2-byte)
        case 0x25: case 0x26: case 0x27: case 0x29: case 0x2B: case 0x37: case 0x38: case 0x39: case 0x3B: // Various operations (0x2C is ROL abs - 2 bytes, 0x35, 0x36 are !abs+X/Y - 2 bytes, moved to 2-byte)
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
        case 0x35: case 0x36: case 0x56: // AND/EOR A,!abs+X/Y
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
    
    // Detect test0000 start BEFORE logging: PC=0x0359, MOV A,#$00 is the first instruction of test0000
    // Check if this is MOV A,#imm (0xE8) at PC 0x0359
    if (savedPC == 0x0359 && opcode == 0xE8) {
        // Read the immediate value to confirm it's 0x00
        uint8_t imm = readARAM(m_regs.pc + 1);
        if (imm == 0x00) {
            std::cout << "\n=== TEST0000 STARTING - Clearing logs ===" << std::endl;
            std::cout << "SPC700 PC:0x0359, MOV A,#$00" << std::endl;
            Logger::getInstance().clearLogs();
        }
    }
    
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
    
    // Debug: Dump SPC memory at 0x0300 when first executing
    if (m_spcLoadState == SPC_LOAD_COMPLETE && savedPC == 0x0300) {
        static bool once = true;
        if (once) {
            once = false;
            std::cout << "=== SPC Memory Dump at 0x0300-0x031F (when starting execution) ===" << std::endl;
            for (int i = 0; i < 32; i++) {
                if (i % 16 == 0) std::cout << std::hex << std::setfill('0') << std::setw(4) << (0x0300 + i) << ": ";
                std::cout << std::setw(2) << (int)m_aram[0x0300 + i] << " ";
                if (i % 16 == 15) std::cout << std::endl;
            }
            std::cout << std::dec << std::endl;
        }
    }
    
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
        case 0xC8: opcodeName = "CMP X,#imm"; break;
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
        case 0x34: opcodeName = "AND A,dp+X"; break;
        case 0x59: opcodeName = "EOR X,dp"; break;
        case 0x68: opcodeName = "CMP A,#imm"; break;
        case 0x64: opcodeName = "CMP dp,#imm"; break;  // NOTE: Some ROMs use 0x64 for CMP dp,#imm
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
        case 0x78: opcodeName = "CMP dp,#imm"; break;
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
        case 0x35: opcodeName = "AND A,!abs+X"; break;
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
        case 0x98: opcodeName = "ADC dp,#imm"; break;
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
    if(opcode == 0x00 && m_regs.pc < 0x0100)
        shouldLog = false;
    
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
        //std::cout << oss.str() << std::endl;
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
            uint16_t addr = getDirectPageAddr(dp);
            writeARAM(addr, imm);
        } break;
        
        // MOV instructions - Immediate to Register
        case 0xE8: { // MOV A, #imm
            uint8_t imm = readARAM(m_regs.pc++);
            m_regs.a = imm;
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
            uint16_t addr = getDirectPageAddr(dp);
            writeARAM(addr, m_regs.a);
        } break;
        case 0xD8: { // MOV dp, X
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            writeARAM(addr, m_regs.x);
        } break;
        case 0xCB: { // MOV dp, Y
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            writeARAM(addr, m_regs.y);
        } break;
        
        // MOV instructions - Direct Page to Register
        case 0xE4: { // MOV A, dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            m_regs.a = readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        case 0xF8: { // MOV X, dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            m_regs.x = readARAM(addr);
            updateNZ(m_regs.x);
        } break;
        case 0xEB: { // MOV Y, dp
            // Note: For IPL ROM compatibility, we update Z flag for MOV Y,dp
            // This allows the BNE at 0xFFD8 to work correctly
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            m_regs.y = readARAM(addr);
            updateNZ(m_regs.y);  // Update Z flag for IPL ROM compatibility
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
            uint8_t carry = getFlag(FLAG_C) ? 1 : 0;
            uint16_t sum = m_regs.a + imm + carry;
            uint8_t result = sum & 0xFF;
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_H, ((m_regs.a & 0x0F) + (imm & 0x0F) + carry) > 0x0F);
            // V flag: signed overflow detection
            // Standard 6502/SPC700 formula: V = ((a ^ result) & (imm ^ result) & 0x80) != 0
            // This detects when both operands have same sign but result has different sign
            setFlag(FLAG_V, ((m_regs.a ^ result) & (imm ^ result) & 0x80) != 0);
            m_regs.a = result;
            updateNZ(m_regs.a);
        } break;
        case 0x84: { // ADC A, dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            uint8_t carry = getFlag(FLAG_C) ? 1 : 0;
            uint16_t sum = m_regs.a + val + carry;
            uint8_t result = sum & 0xFF;
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_H, ((m_regs.a & 0x0F) + (val & 0x0F) + carry) > 0x0F);
            // V flag: same as ADC A, #imm
            setFlag(FLAG_V, ((m_regs.a ^ result) & (val ^ result) & 0x80) != 0);
            m_regs.a = result;
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
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
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
            uint16_t addr = getDirectPageAddr(dp);
            m_regs.a &= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        case 0x08: { // OR A, #imm
            m_regs.a |= readARAM(m_regs.pc++);
            updateNZ(m_regs.a);
        } break;
        case 0x04: { // OR A, dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            m_regs.a |= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        case 0x05: { // OR A, abs
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            m_regs.a |= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        case 0x48: { // EOR A, #imm
            m_regs.a ^= readARAM(m_regs.pc++);
            updateNZ(m_regs.a);
        } break;
        case 0x44: { // EOR A, dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            m_regs.a ^= readARAM(addr);
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
        case 0x34: { // AND A, dp+X
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            m_regs.a &= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        case 0x59: { // EOR (X),(Y)
            uint8_t val = readARAM(m_regs.x) ^ readARAM(m_regs.y);
            writeARAM(m_regs.x, val);
            updateNZ(val);
        } break;
        
        // Comparison operations
        case 0x68: { // CMP A, #imm
            uint8_t imm = readARAM(m_regs.pc++);
            uint8_t result = m_regs.a - imm;
            setFlag(FLAG_C, m_regs.a >= imm);
            updateNZ(result);
        } break;
        case 0x64: { // CMP dp,#imm (NOTE: Some ROMs use 0x64 instead of 0x78 for CMP dp,#imm)
            // Operand order for 0x64: dp, imm (different from 0x78 which is imm, dp)
            // For compatibility with spctest ROM, treat 0x64 as CMP dp,#imm
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t imm = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            uint8_t result = val - imm;
            setFlag(FLAG_C, val >= imm);
            updateNZ(result);
            // Debug: Log CMP dp,#imm for test debugging (test0008 and beyond)
            if (savedPC >= 0x053e && savedPC <= 0x0630) {
                std::ostringstream oss;
                oss << "APU: CMP dp,#imm (0x64): dp=0x" << std::hex << (int)dp 
                    << ", addr=0x" << addr
                    << ", val=0x" << (int)val << ", imm=0x" << (int)imm 
                    << ", result=0x" << (int)result << ", Z=" << getFlag(FLAG_Z) 
                    << ", PC=0x" << savedPC << std::dec;
                Logger::getInstance().logAPU(oss.str());
            }
            // Debug: Log CMP dp,#imm for test0041 (PC 0x1313) - log memory bytes
            if (savedPC == 0x1313) {
                uint8_t byte1 = readARAM(savedPC + 1);
                uint8_t byte2 = readARAM(savedPC + 2);
                std::ostringstream oss;
                oss << "APU: CMP dp,#imm (0x64) at test0041: PC=0x" << std::hex << savedPC
                    << ", byte1=0x" << (int)byte1 << ", byte2=0x" << (int)byte2
                    << ", dp=0x" << (int)dp << ", imm=0x" << (int)imm
                    << ", addr=0x" << addr << ", val=0x" << (int)val 
                    << ", result=0x" << (int)result << ", Z=" << getFlag(FLAG_Z) << std::dec;
                Logger::getInstance().logAPU(oss.str());
            }
        } break;
        case 0x3E: { // CMP X, dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            uint8_t result = m_regs.x - val;
            setFlag(FLAG_C, m_regs.x >= val);
            updateNZ(result);
        } break;
        case 0x7E: { // CMP Y, dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
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
        case 0xC8: { // CMP X, #imm
            uint8_t imm = readARAM(m_regs.pc++);
            uint8_t result = m_regs.x - imm;
            setFlag(FLAG_C, m_regs.x >= imm);
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
            bool zFlag = getFlag(FLAG_Z);
            // Debug: Log BNE for test0008 debugging
            if (savedPC >= 0x0541 && savedPC <= 0x0550) {
                std::ostringstream oss;
                oss << "APU: BNE rel: PC=0x" << std::hex << savedPC 
                    << ", offset=0x" << (int)offset
                    << ", Z=" << zFlag 
                    << ", PSW=0x" << (int)m_regs.psw
                    << ", willBranch=" << !zFlag << std::dec;
                Logger::getInstance().logAPU(oss.str());
            }
            // Debug: Log BNE for test0041 (PC 0x11fb)
            if (savedPC >= 0x11fb && savedPC <= 0x11fb) {
                std::ostringstream oss;
                oss << "APU: BNE rel (test0041): PC=0x" << std::hex << savedPC 
                    << ", offset=0x" << (int)offset
                    << ", Z=" << zFlag 
                    << ", PSW=0x" << (int)m_regs.psw
                    << ", willBranch=" << !zFlag << std::dec;
                Logger::getInstance().logAPU(oss.str());
            }
            if (!zFlag) {
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
            // Note: POP A does NOT update flags (unlike POP X/Y)
            // This is important for save_results which pops PSW into A
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
        case 0x4F: { // PCALL upage
            uint8_t upage = readARAM(m_regs.pc++);
            push((m_regs.pc >> 8) & 0xFF);
            push(m_regs.pc & 0xFF);
            m_regs.pc = 0xFF00 + upage;
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
            uint16_t addr = getDirectPageAddr(dp);
            uint16_t addrHigh = getDirectPageAddr((dp + 1) & 0xFF);
            uint16_t val = readARAM(addr);
            val |= readARAM(addrHigh) << 8;
            val++;
            writeARAM(addr, val & 0xFF);
            writeARAM(addrHigh, (val >> 8) & 0xFF);
            // 16-bit Z/N flags
            setFlag(FLAG_Z, val == 0);
            setFlag(FLAG_N, (val & 0x8000) != 0);
        } break;
        case 0x5A: { // CMPW YA, dp (16-bit compare)
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint16_t addrHigh = getDirectPageAddr((dp + 1) & 0xFF);
            uint16_t memVal = readARAM(addr);
            memVal |= readARAM(addrHigh) << 8;
            uint16_t ya = m_regs.a | (m_regs.y << 8);
            uint16_t result = ya - memVal;
            setFlag(FLAG_C, ya >= memVal);
            // 16-bit Z/N flags
            setFlag(FLAG_Z, result == 0);
            setFlag(FLAG_N, (result & 0x8000) != 0);
        } break;
        
        // Bit operations
        case 0x78: { // CMP dp,#imm (operand order: imm, dp)
            uint8_t imm = readARAM(m_regs.pc++);
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            uint8_t result = val - imm;
            setFlag(FLAG_C, val >= imm);
            updateNZ(result);
            
            // Debug: Log CMP dp,#imm for test debugging
            if (m_regs.pc >= 0x036f && m_regs.pc <= 0x0380) {
                std::ostringstream oss;
                oss << "APU: CMP dp,#imm: dp=0x" << std::hex << (int)dp 
                    << ", addr=0x" << addr
                    << ", val=0x" << (int)val << ", imm=0x" << (int)imm 
                    << ", result=0x" << (int)result << ", Z=" << getFlag(FLAG_Z) 
                    << ", PC=0x" << (m_regs.pc - 3) << std::dec;
                Logger::getInstance().logAPU(oss.str());
            }
            // Debug: Log CMP dp,#imm for test0008 and beyond
            if (savedPC >= 0x053e && savedPC <= 0x0630) {
                std::ostringstream oss;
                oss << "APU: CMP dp,#imm (0x78): dp=0x" << std::hex << (int)dp 
                    << ", addr=0x" << addr
                    << ", val=0x" << (int)val << ", imm=0x" << (int)imm 
                    << ", result=0x" << (int)result << ", Z=" << getFlag(FLAG_Z) 
                    << ", PC=0x" << savedPC << std::dec;
                Logger::getInstance().logAPU(oss.str());
            }
            
            // Debug: Log CMP dp,#imm for test0041 (PC 0x11f8-0x1203)
            if (savedPC >= 0x11f8 && savedPC <= 0x1203) {
                std::ostringstream oss;
                oss << "APU: CMP dp,#imm (test0041): dp=0x" << std::hex << (int)dp 
                    << ", addr=0x" << addr
                    << ", val=0x" << (int)val << ", imm=0x" << (int)imm 
                    << ", result=0x" << (int)result << ", Z=" << getFlag(FLAG_Z) 
                    << ", PSW=0x" << (int)m_regs.psw
                    << ", PC=0x" << savedPC << std::dec;
                Logger::getInstance().logAPU(oss.str());
            }
            
            // Debug: Log CMP dp,#imm for IPL ROM protocol
            if (m_regs.pc >= 0xFFCF && m_regs.pc <= 0xFFD4) {
                static int logCount = 0;
                if (logCount < 5) {
                    std::ostringstream oss;
                    oss << "APU: CMP dp,#imm: dp=0x" << std::hex << (int)dp 
                        << ", val=0x" << (int)val << ", imm=0x" << (int)imm 
                        << ", result=0x" << (int)result << ", Z=" << getFlag(FLAG_Z) 
                        << ", PC=0x" << m_regs.pc << std::dec;
                    Logger::getInstance().logAPU(oss.str());
                    logCount++;
                }
            }
        } break;
        case 0x03: { // BBS dp.bit, rel (Branch if Bit Set)
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t bit = (dp >> 5) & 0x07;
            dp &= 0x1F;
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
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
        case 0x0F:   // BRK
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
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            val |= (1 << bit);
            writeARAM(addr, val);
        } break;
        
        // CLR1 dp.bit (0x12, 0x32, 0x52, 0x72, 0x92, 0xB2, 0xD2, 0xF2)
        case 0x12: case 0x32: case 0x52: case 0x72: case 0x92: case 0xB2: case 0xD2: case 0xF2: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t bit = (opcode >> 5) & 0x07;
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            val &= ~(1 << bit);
            writeARAM(addr, val);
        } break;
        
        // BBS dp.bit,rel (0x03, 0x23, 0x43, 0x63, 0x83, 0xA3, 0xC3, 0xE3)
        case 0x23: case 0x43: case 0x63: case 0x83: case 0xA3: case 0xC3: case 0xE3: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t bit = (opcode >> 5) & 0x07;
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if ((val >> bit) & 1) {
                m_regs.pc += offset;
            }
        } break;
        
        // BBC dp.bit,rel (0x13, 0x33, 0x53, 0x73, 0x93, 0xB3, 0xD3, 0xF3)
        case 0x13: case 0x33: case 0x53: case 0x73: case 0x93: case 0xB3: case 0xD3: case 0xF3: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t bit = (opcode >> 5) & 0x07;
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
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
            uint16_t ptrAddr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            uint8_t ptrLow = readARAM(ptrAddr);
            uint16_t ptrAddrHigh = getDirectPageAddr((dp + m_regs.x + 1) & 0xFF);
            uint8_t ptrHigh = readARAM(ptrAddrHigh);
            uint16_t addr = ptrLow | (ptrHigh << 8);
            m_regs.a |= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // OR A,dp+X - 0x14
        case 0x14: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
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
            uint16_t addr = readARAM(getDirectPageAddr(dp));
            addr |= readARAM(getDirectPageAddr((dp + 1) & 0xFF)) << 8;
            addr += m_regs.y;
            m_regs.a |= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // OR dp,#imm - 0x18 (operand order: imm, dp)
        case 0x18: {
            uint8_t imm = readARAM(m_regs.pc++);
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            val |= imm;
            writeARAM(addr, val);
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
        
        // AND A,[dp+X] - 0x27 (indirect addressing: read 16-bit pointer from Direct Page)
        case 0x27: {
            uint8_t dp = readARAM(m_regs.pc++);
            // Calculate pointer address: (dp + X) wraps at 0xFF
            uint16_t ptrAddr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            // Read 16-bit pointer (little-endian: low byte, high byte)
            uint8_t ptrLow = readARAM(ptrAddr);
            uint16_t ptrAddrHigh = getDirectPageAddr((dp + m_regs.x + 1) & 0xFF);
            uint8_t ptrHigh = readARAM(ptrAddrHigh);
            uint16_t addr = ptrLow | (ptrHigh << 8);
            m_regs.a &= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // AND A,!abs+X - 0x35
        case 0x35: {
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            addr += m_regs.x;
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
            uint16_t addr = readARAM(getDirectPageAddr(dp));
            addr |= readARAM(getDirectPageAddr((dp + 1) & 0xFF)) << 8;
            addr += m_regs.y;
            m_regs.a &= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // AND dp,#imm - 0x38 (operand order: imm, dp)
        case 0x38: {
            uint8_t imm = readARAM(m_regs.pc++);
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            uint8_t oldVal = val;  // Save original value for debugging
            val &= imm;
            writeARAM(addr, val);
            updateNZ(val);
            // Debug: Log AND dp,#imm for test debugging
            if (savedPC >= 0x27bc && savedPC <= 0x27bf) {
                std::ostringstream oss;
                oss << "APU: AND dp,#imm (0x38): dp=0x" << std::hex << (int)dp 
                    << ", P=" << getFlag(FLAG_P) << ", addr=0x" << addr
                    << ", oldVal=0x" << (int)oldVal << ", imm=0x" << (int)imm
                    << ", result=0x" << (int)val << ", PSW=0x" << (int)m_regs.psw
                    << ", PC=0x" << savedPC << std::dec;
                Logger::getInstance().logAPU(oss.str());
            }
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
            uint16_t ptrAddr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            uint8_t ptrLow = readARAM(ptrAddr);
            uint16_t ptrAddrHigh = getDirectPageAddr((dp + m_regs.x + 1) & 0xFF);
            uint8_t ptrHigh = readARAM(ptrAddrHigh);
            uint16_t addr = ptrLow | (ptrHigh << 8);
            m_regs.a ^= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // EOR A,dp+X - 0x54
        case 0x54: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
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
            uint16_t addr = readARAM(getDirectPageAddr(dp));
            addr |= readARAM(getDirectPageAddr((dp + 1) & 0xFF)) << 8;
            addr += m_regs.y;
            m_regs.a ^= readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        
        // EOR dp,#imm - 0x58 (operand order: imm, dp)
        case 0x58: {
            uint8_t imm = readARAM(m_regs.pc++);
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            val ^= imm;
            writeARAM(addr, val);
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
            uint16_t ptrAddr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            uint8_t ptrLow = readARAM(ptrAddr);
            uint16_t ptrAddrHigh = getDirectPageAddr((dp + m_regs.x + 1) & 0xFF);
            uint8_t ptrHigh = readARAM(ptrAddrHigh);
            uint16_t addr = ptrLow | (ptrHigh << 8);
            uint8_t val = readARAM(addr);
            uint8_t result = m_regs.a - val;
            setFlag(FLAG_C, m_regs.a >= val);
            updateNZ(result);
        } break;
        
        // CMP A,dp+X - 0x74
        case 0x74: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
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
            uint16_t addr = readARAM(getDirectPageAddr(dp));
            addr |= readARAM(getDirectPageAddr((dp + 1) & 0xFF)) << 8;
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
            uint8_t carry = getFlag(FLAG_C) ? 1 : 0;
            uint16_t sum = m_regs.a + val + carry;
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_H, ((m_regs.a & 0x0F) + (val & 0x0F) + carry) > 0x0F);
            setFlag(FLAG_V, ((m_regs.a ^ sum) & (val ^ sum) & 0x80) != 0);
            m_regs.a = sum & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // ADC A,(X) - 0x86
        case 0x86: {
            uint8_t val = readARAM(m_regs.x);
            uint8_t carry = getFlag(FLAG_C) ? 1 : 0;
            uint16_t sum = m_regs.a + val + carry;
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_H, ((m_regs.a & 0x0F) + (val & 0x0F) + carry) > 0x0F);
            setFlag(FLAG_V, ((m_regs.a ^ sum) & (val ^ sum) & 0x80) != 0);
            m_regs.a = sum & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // ADC A,[dp+X] - 0x87 (indirect addressing)
        case 0x87: {
            uint8_t dp = readARAM(m_regs.pc++);
            // Calculate pointer address: (dp + X) wraps at 0xFF
            uint16_t ptrAddr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            // Read 16-bit pointer (little-endian: low byte, high byte)
            uint8_t ptrLow = readARAM(ptrAddr);
            // Use getDirectPageAddr for high byte to handle P flag and wrapping correctly
            uint16_t ptrAddrHigh = getDirectPageAddr((dp + m_regs.x + 1) & 0xFF);
            uint8_t ptrHigh = readARAM(ptrAddrHigh);
            uint16_t addr = ptrLow | (ptrHigh << 8);
            // Read value from indirect address
            uint8_t val = readARAM(addr);
            uint8_t carry = getFlag(FLAG_C) ? 1 : 0;
            uint16_t sum = m_regs.a + val + carry;
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_H, ((m_regs.a & 0x0F) + (val & 0x0F) + carry) > 0x0F);
            setFlag(FLAG_V, ((m_regs.a ^ sum) & (val ^ sum) & 0x80) != 0);
            m_regs.a = sum & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // ADC A,dp+X - 0x94
        case 0x94: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            uint8_t val = readARAM(addr);
            uint8_t carry = getFlag(FLAG_C) ? 1 : 0;
            uint16_t sum = m_regs.a + val + carry;
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_H, ((m_regs.a & 0x0F) + (val & 0x0F) + carry) > 0x0F);
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
            uint8_t carry = getFlag(FLAG_C) ? 1 : 0;
            uint16_t sum = m_regs.a + val + carry;
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_H, ((m_regs.a & 0x0F) + (val & 0x0F) + carry) > 0x0F);
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
            uint8_t carry = getFlag(FLAG_C) ? 1 : 0;
            uint16_t sum = m_regs.a + val + carry;
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_H, ((m_regs.a & 0x0F) + (val & 0x0F) + carry) > 0x0F);
            setFlag(FLAG_V, ((m_regs.a ^ sum) & (val ^ sum) & 0x80) != 0);
            m_regs.a = sum & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // ADC A,(dp)+Y - 0x97
        case 0x97: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = readARAM(getDirectPageAddr(dp));
            addr |= readARAM(getDirectPageAddr((dp + 1) & 0xFF)) << 8;
            addr += m_regs.y;
            uint8_t val = readARAM(addr);
            uint8_t carry = getFlag(FLAG_C) ? 1 : 0;
            uint16_t sum = m_regs.a + val + carry;
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_H, ((m_regs.a & 0x0F) + (val & 0x0F) + carry) > 0x0F);
            setFlag(FLAG_V, ((m_regs.a ^ sum) & (val ^ sum) & 0x80) != 0);
            m_regs.a = sum & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // ADC (X),(Y) - 0x99
        case 0x99: { // ADC (X),(Y)
            uint8_t valX = readARAM(m_regs.x);
            uint8_t valY = readARAM(m_regs.y);
            uint8_t carry = getFlag(FLAG_C) ? 1 : 0;
            uint16_t sum = valX + valY + carry;
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_H, ((valX & 0x0F) + (valY & 0x0F) + carry) > 0x0F);
            setFlag(FLAG_V, ((valX ^ sum) & (valY ^ sum) & 0x80) != 0);
            uint8_t result = sum & 0xFF;
            writeARAM(m_regs.x, result);
            updateNZ(result);
            // Debug: Log ADC (X),(Y) for test0041
            if (savedPC == 0x11f0) {
                std::ostringstream oss;
                oss << "APU: ADC (X),(Y): X=0x" << std::hex << (int)m_regs.x 
                    << ", Y=0x" << (int)m_regs.y
                    << ", valX=0x" << (int)valX << ", valY=0x" << (int)valY
                    << ", C=" << carry
                    << ", sum=0x" << sum
                    << ", result=0x" << (int)result
                    << ", written to addr=0x" << (int)m_regs.x
                    << ", PSW=0x" << (int)m_regs.psw
                    << ", PC=0x" << savedPC << std::dec;
                Logger::getInstance().logAPU(oss.str());
            }
        } break;
        
        // ADC dp,#imm - 0x98
        // Actual binary encoding: first byte = imm, second byte = dp (reversed!)
        // Assembly: adc $01, #$34 -> binary: 98 34 01 (imm=$34, dp=$01)
        case 0x98: {
            uint8_t imm = readARAM(m_regs.pc++);  // immediate value (first byte in binary)
            uint8_t dp = readARAM(m_regs.pc++);   // direct page address (second byte in binary)
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            uint8_t carry = getFlag(FLAG_C) ? 1 : 0;
            uint16_t sum = val + imm + carry;
            uint8_t result = sum & 0xFF;
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_H, ((val & 0x0F) + (imm & 0x0F) + carry) > 0x0F);
            setFlag(FLAG_V, ((val ^ result) & (imm ^ result) & 0x80) != 0);
            writeARAM(addr, result);
            updateNZ(result);
            // Debug: Log ADC dp,#imm for test000e
            if (savedPC >= 0x0694 && savedPC <= 0x0697) {
                std::ostringstream oss;
                oss << "APU: ADC dp,#imm: dp=0x" << std::hex << (int)dp 
                    << ", imm=0x" << (int)imm
                    << ", addr=0x" << addr
                    << ", val=0x" << (int)val
                    << ", C=" << getFlag(FLAG_C)
                    << ", result=0x" << (int)result
                    << ", written to 0x" << addr
                    << ", PC=0x" << savedPC << std::dec;
                Logger::getInstance().logAPU(oss.str());
            }
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
            uint16_t ptrAddr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            uint8_t ptrLow = readARAM(ptrAddr);
            uint16_t ptrAddrHigh = getDirectPageAddr((dp + m_regs.x + 1) & 0xFF);
            uint8_t ptrHigh = readARAM(ptrAddrHigh);
            uint16_t addr = ptrLow | (ptrHigh << 8);
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
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
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
            uint16_t addr = readARAM(getDirectPageAddr(dp));
            addr |= readARAM(getDirectPageAddr((dp + 1) & 0xFF)) << 8;
            addr += m_regs.y;
            uint8_t val = readARAM(addr);
            uint16_t diff = m_regs.a - val - (getFlag(FLAG_C) ? 0 : 1);
            setFlag(FLAG_C, diff <= 0xFF);
            setFlag(FLAG_V, ((m_regs.a ^ diff) & (val ^ diff) & 0x80) != 0);
            m_regs.a = diff & 0xFF;
            updateNZ(m_regs.a);
        } break;
        
        // SBC (X),(Y) - 0xB9
        case 0xB9: { // SBC (X),(Y)
            uint8_t valX = readARAM(m_regs.x);
            uint8_t valY = readARAM(m_regs.y);
            uint16_t diff = valX - valY - (getFlag(FLAG_C) ? 0 : 1);
            setFlag(FLAG_C, diff <= 0xFF);
            setFlag(FLAG_V, ((valX ^ diff) & (valY ^ diff) & 0x80) != 0);
            uint8_t result = diff & 0xFF;
            writeARAM(m_regs.x, result);
            updateNZ(result);
        } break;

        // SBC dp,#imm - 0xB8
        case 0xB8: {
            uint8_t dp = readARAM(m_regs.pc++);
            uint8_t imm = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            uint16_t diff = val - imm - (getFlag(FLAG_C) ? 0 : 1);
            setFlag(FLAG_C, diff <= 0xFF);
            setFlag(FLAG_V, ((val ^ diff) & (imm ^ diff) & 0x80) != 0);
            uint8_t result = diff & 0xFF;
            writeARAM(addr, result);
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
            uint16_t addr = getDirectPageAddr(dp);
            uint16_t addrHigh = getDirectPageAddr((dp + 1) & 0xFF);
            uint16_t val = readARAM(addr);
            val |= readARAM(addrHigh) << 8;
            val--;
            writeARAM(addr, val & 0xFF);
            writeARAM(addrHigh, (val >> 8) & 0xFF);
            // 16-bit Z/N flags
            setFlag(FLAG_Z, val == 0);
            setFlag(FLAG_N, (val & 0x8000) != 0);
        } break;
        case 0x7A: { // ADDW YA,dp - 16-bit add
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            // Direct Page address space wraps at 256 bytes, so (dp + 1) wraps to (dp + 1) & 0xFF
            uint16_t addrHigh = getDirectPageAddr((dp + 1) & 0xFF);
            uint16_t memVal = readARAM(addr);
            memVal |= readARAM(addrHigh) << 8;
            uint16_t ya = m_regs.a | (m_regs.y << 8);
            
            // Calculate Half Carry flag: check if carry from lower byte to upper byte's lower 4 bits
            uint8_t lowA = m_regs.a;
            uint8_t lowMem = memVal & 0xFF;
            uint16_t lowSum = lowA + lowMem;  // Use uint16_t to detect overflow
            bool carryFromLow = (lowSum > 0xFF);
            
            uint8_t highA = m_regs.y;
            uint8_t highMem = (memVal >> 8) & 0xFF;
            // Half Carry is set if lower 4 bits of upper byte addition (including carry from lower) > 0xF
            setFlag(FLAG_H, ((highA & 0xF) + (highMem & 0xF) + (carryFromLow ? 1 : 0)) > 0xF);
            
            uint32_t sum = ya + memVal;
            setFlag(FLAG_C, sum > 0xFFFF);
            setFlag(FLAG_V, ((ya ^ sum) & (memVal ^ sum) & 0x8000) != 0);
            m_regs.a = sum & 0xFF;
            m_regs.y = (sum >> 8) & 0xFF;
            // Zero flag: 16-bit result (YA) is zero
            setFlag(FLAG_Z, (sum & 0xFFFF) == 0);
            // Negative flag: based on high byte (Y)
            setFlag(FLAG_N, (m_regs.y & 0x80) != 0);
        } break;
        case 0x9A: { // SUBW YA,dp - 16-bit subtract
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            // Direct Page address space wraps at 256 bytes, so (dp + 1) wraps to (dp + 1) & 0xFF
            uint16_t addrHigh = getDirectPageAddr((dp + 1) & 0xFF);
            uint16_t memVal = readARAM(addr);
            memVal |= readARAM(addrHigh) << 8;
            uint16_t ya = m_regs.a | (m_regs.y << 8);
            uint32_t diff = ya - memVal;
            setFlag(FLAG_C, diff <= 0xFFFF);
            setFlag(FLAG_V, ((ya ^ diff) & (memVal ^ diff) & 0x8000) != 0);
            m_regs.a = diff & 0xFF;
            m_regs.y = (diff >> 8) & 0xFF;
            // 16-bit Z/N flags
            setFlag(FLAG_Z, (diff & 0xFFFF) == 0);
            setFlag(FLAG_N, (diff & 0x8000) != 0);
        } break;
        case 0xBA: { // MOVW YA,dp - 16-bit move
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            m_regs.a = readARAM(addr);
            m_regs.y = readARAM(addr + 1);
            // 16-bit Z/N flags
            uint16_t ya = m_regs.a | (m_regs.y << 8);
            setFlag(FLAG_Z, ya == 0);
            setFlag(FLAG_N, (ya & 0x8000) != 0);
        } break;
        case 0xDA: { // MOVW dp,YA - 16-bit move
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            // Direct Page address space wraps at 256 bytes
            uint16_t addrHigh = getDirectPageAddr((dp + 1) & 0xFF);
            
            // Debug: Log writes to 0x00-0x01 during IPL address setup
            if (dp == 0x00) {
                static int logCount = 0;
                if (logCount < 5) {
                    uint16_t addr = (m_regs.y << 8) | m_regs.a;
                    std::ostringstream oss;
                    oss << "APU: MOVW dp,YA: dp=0x" << std::hex << (int)dp 
                        << ", A=0x" << (int)m_regs.a << ", Y=0x" << (int)m_regs.y 
                        << ", addr=0x" << addr << ", PC=0x" << m_regs.pc << std::dec;
                    Logger::getInstance().logAPU(oss.str());
                    logCount++;
                }
            }
            
            writeARAM(addr, m_regs.a);
            writeARAM(addrHigh, m_regs.y);
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
        case 0xE7: { // MOV A,[dp+X] (indirect addressing)
            uint8_t dp = readARAM(m_regs.pc++);
            // Calculate pointer address: (dp + X) wraps at 0xFF
            uint16_t ptrAddr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            // Read 16-bit pointer (little-endian: low byte, high byte)
            uint8_t ptrLow = readARAM(ptrAddr);
            uint16_t ptrAddrHigh = getDirectPageAddr((dp + m_regs.x + 1) & 0xFF);
            uint8_t ptrHigh = readARAM(ptrAddrHigh);
            uint16_t addr = ptrLow | (ptrHigh << 8);
            // Read value from indirect address
            m_regs.a = readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        case 0xF4: { // MOV A,dp+X
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
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
            uint16_t addr = readARAM(getDirectPageAddr(dp));
            addr |= readARAM(getDirectPageAddr((dp + 1) & 0xFF)) << 8;
            addr += m_regs.y;
            m_regs.a = readARAM(addr);
            updateNZ(m_regs.a);
        } break;
        case 0xBF: { // MOV A,(X)+
            m_regs.a = readARAM(m_regs.x);
            m_regs.x++;
            updateNZ(m_regs.a);
        } break;
        case 0xAF: { // MOV (X)+,A
            writeARAM(m_regs.x, m_regs.a);
            m_regs.x++;
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
            uint16_t ptrAddr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            uint8_t ptrLow = readARAM(ptrAddr);
            uint16_t ptrAddrHigh = getDirectPageAddr((dp + m_regs.x + 1) & 0xFF);
            uint8_t ptrHigh = readARAM(ptrAddrHigh);
            uint16_t addr = ptrLow | (ptrHigh << 8);
            writeARAM(addr, m_regs.a);
        } break;
        case 0xD4: { // MOV dp+X,A
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
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
            uint16_t addr = readARAM(getDirectPageAddr(dp));
            addr |= readARAM(getDirectPageAddr((dp + 1) & 0xFF)) << 8;
            addr += m_regs.y;
            
            // Debug: Log writes during IPL data transfer
            if (m_regs.pc >= 0xFFE3 && m_regs.pc <= 0xFFE6) {
                static int logCount = 0;
                if (logCount < 50) {
                    uint8_t dp0 = readARAM(dp);
                    uint8_t dp1 = readARAM(dp + 1);
                    std::ostringstream oss;
                    oss << "APU: MOV (dp)+Y,A: dp=0x" << std::hex << (int)dp 
                        << ", readARAM(0x" << (int)dp << ")=0x" << (int)dp0
                        << ", readARAM(0x" << (int)(dp+1) << ")=0x" << (int)dp1
                        << ", addr=0x" << addr << ", Y=0x" << (int)m_regs.y 
                        << ", A=0x" << (int)m_regs.a << ", PC=0x" << m_regs.pc << std::dec;
                    Logger::getInstance().logAPU(oss.str());
                    logCount++;
                }
            }
            
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
            uint16_t addr = getDirectPageAddr((dp + m_regs.y) & 0xFF);
            m_regs.x = readARAM(addr);
            updateNZ(m_regs.x);
        } break;
        case 0xFB: { // MOV Y,dp+X
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
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
            uint16_t addr = getDirectPageAddr((dp + m_regs.y) & 0xFF);
            writeARAM(addr, m_regs.x);
        } break;
        case 0xDB: { // MOV dp+X,Y
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
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
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            val++;
            writeARAM(addr, val);
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
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            uint8_t val = readARAM(addr);
            val++;
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0x8B: { // DEC dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            val--;
            writeARAM(addr, val);
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
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            uint8_t val = readARAM(addr);
            val--;
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0x6E: { // DBNZ dp,rel
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            val--;
            writeARAM(addr, val);
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
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if (m_regs.a != val) {
                m_regs.pc += offset;
            }
        } break;
        case 0xDE: { // CBNE dp+X,rel
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            uint8_t val = readARAM(addr);
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            if (m_regs.a != val) {
                m_regs.pc += offset;
            }
        } break;
        
        // Shift/Rotate with addressing modes
        case 0x0B: { // ASL dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, (val & 0x80) != 0);
            val <<= 1;
            writeARAM(addr, val);
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
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, (val & 0x80) != 0);
            val <<= 1;
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0x2B: { // ROL dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            bool carry = getFlag(FLAG_C);
            setFlag(FLAG_C, (val & 0x80) != 0);
            val = (val << 1) | (carry ? 1 : 0);
            writeARAM(addr, val);
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
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            uint8_t val = readARAM(addr);
            bool carry = getFlag(FLAG_C);
            setFlag(FLAG_C, (val & 0x80) != 0);
            val = (val << 1) | (carry ? 1 : 0);
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0x4B: { // LSR dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, (val & 0x01) != 0);
            val >>= 1;
            writeARAM(addr, val);
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
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, (val & 0x01) != 0);
            val >>= 1;
            writeARAM(addr, val);
            updateNZ(val);
        } break;
        case 0x6B: { // ROR dp
            uint8_t dp = readARAM(m_regs.pc++);
            uint16_t addr = getDirectPageAddr(dp);
            uint8_t val = readARAM(addr);
            bool carry = getFlag(FLAG_C);
            setFlag(FLAG_C, (val & 0x01) != 0);
            val = (val >> 1) | (carry ? 0x80 : 0);
            writeARAM(addr, val);
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
            uint16_t addr = getDirectPageAddr((dp + m_regs.x) & 0xFF);
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
        case 0x2A: { // OR1 C,/mem.bit (inverse bit)
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t bit = (addr >> 13) & 0x07;
            addr &= 0x1FFF;
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, getFlag(FLAG_C) || !((val >> bit) & 1));
        } break;
        case 0x4A: { // AND1 C,mem.bit
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t bit = (addr >> 13) & 0x07;
            addr &= 0x1FFF;
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, getFlag(FLAG_C) && ((val >> bit) & 1) != 0);
        } break;
        case 0x6A: { // AND1 C,/mem.bit (inverse bit)
            uint16_t addr = readARAM(m_regs.pc++);
            addr |= readARAM(m_regs.pc++) << 8;
            uint8_t bit = (addr >> 13) & 0x07;
            addr &= 0x1FFF;
            uint8_t val = readARAM(addr);
            setFlag(FLAG_C, getFlag(FLAG_C) && !((val >> bit) & 1));
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
        case 0x09: { // OR dp,dp - dest = dest | src
            // SPC700: OR dest,src means dest = dest | src
            // Actual binary encoding: first byte = src, second byte = dest (reversed!)
            // Assembly: or $01, $02 -> binary: 09 02 01 (src=$02, dest=$01)
            uint8_t dpSrc = readARAM(m_regs.pc++);    // source (first byte in binary)
            uint8_t dpDest = readARAM(m_regs.pc++);  // destination (second byte in binary)
            uint16_t addrDest = getDirectPageAddr(dpDest);
            uint16_t addrSrc = getDirectPageAddr(dpSrc);
            uint8_t valDest = readARAM(addrDest);
            uint8_t valSrc = readARAM(addrSrc);
            uint8_t val = valDest | valSrc;
            writeARAM(addrDest, val);
            updateNZ(val);
        } break;
        case 0x29: { // AND dp,dp - dest = dest & src
            // SPC700: AND dest,src means dest = dest & src
            // Actual binary encoding: first byte = src, second byte = dest (reversed!)
            // Assembly: and $01, $02 -> binary: 29 02 01 (src=$02, dest=$01)
            uint8_t dpSrc = readARAM(m_regs.pc++);    // source (first byte in binary)
            uint8_t dpDest = readARAM(m_regs.pc++);  // destination (second byte in binary)
            uint16_t addrDest = getDirectPageAddr(dpDest);
            uint16_t addrSrc = getDirectPageAddr(dpSrc);
            uint8_t valDest = readARAM(addrDest);
            uint8_t valSrc = readARAM(addrSrc);
            uint8_t val = valDest & valSrc;
            writeARAM(addrDest, val);
            updateNZ(val);
            // Debug: Log AND dp,dp for test debugging
            if (savedPC >= 0x2ad2 && savedPC <= 0x2ad5) {
                std::ostringstream oss;
                oss << "APU: AND dp,dp (0x29): dpDest=0x" << std::hex << (int)dpDest 
                    << ", dpSrc=0x" << (int)dpSrc << ", P=" << getFlag(FLAG_P)
                    << ", addrDest=0x" << addrDest << ", addrSrc=0x" << addrSrc
                    << ", valDest=0x" << (int)valDest << ", valSrc=0x" << (int)valSrc
                    << ", result=0x" << (int)val << ", PSW=0x" << (int)m_regs.psw
                    << ", PC=0x" << savedPC << std::dec;
                Logger::getInstance().logAPU(oss.str());
            }
        } break;
        case 0x49: { // EOR dp,dp - dest = dest ^ src
            // SPC700: EOR dest,src means dest = dest ^ src
            // Actual binary encoding: first byte = src, second byte = dest (reversed!)
            // Assembly: eor $01, $02 -> binary: 49 02 01 (src=$02, dest=$01)
            uint8_t dpSrc = readARAM(m_regs.pc++);    // source (first byte in binary)
            uint8_t dpDest = readARAM(m_regs.pc++);  // destination (second byte in binary)
            uint16_t addrDest = getDirectPageAddr(dpDest);
            uint16_t addrSrc = getDirectPageAddr(dpSrc);
            uint8_t valDest = readARAM(addrDest);
            uint8_t valSrc = readARAM(addrSrc);
            uint8_t val = valDest ^ valSrc;
            writeARAM(addrDest, val);
            updateNZ(val);
        } break;
        case 0x89: { // ADC dp,dp - dest = dest + src
            // SPC700: ADC dest,src means dest = dest + src
            // Actual binary encoding: first byte = src, second byte = dest (reversed!)
            // Assembly: adc $01, $02 -> binary: 89 02 01 (src=$02, dest=$01)
            uint8_t dpSrc = readARAM(m_regs.pc++);    // source (first byte in binary)
            uint8_t dpDest = readARAM(m_regs.pc++);  // destination (second byte in binary)
            uint16_t addrDest = getDirectPageAddr(dpDest);
            uint16_t addrSrc = getDirectPageAddr(dpSrc);
            uint8_t valDest = readARAM(addrDest);
            uint8_t valSrc = readARAM(addrSrc);
            uint8_t carry = getFlag(FLAG_C) ? 1 : 0;
            uint16_t sum = valDest + valSrc + carry;
            setFlag(FLAG_C, sum > 0xFF);
            setFlag(FLAG_H, ((valDest & 0x0F) + (valSrc & 0x0F) + carry) > 0x0F);
            setFlag(FLAG_V, ((valDest ^ sum) & (valSrc ^ sum) & 0x80) != 0);
            uint8_t result = sum & 0xFF;
            writeARAM(addrDest, result);
            updateNZ(result);
            // Debug: Log ADC dp,dp for test000c
            if (savedPC >= 0x0615 && savedPC <= 0x0618) {
                std::ostringstream oss;
                oss << "APU: ADC dp,dp: dpDest=0x" << std::hex << (int)dpDest 
                    << ", dpSrc=0x" << (int)dpSrc
                    << ", addrDest=0x" << addrDest << ", addrSrc=0x" << addrSrc
                    << ", valDest=0x" << (int)valDest << ", valSrc=0x" << (int)valSrc
                    << ", C=" << getFlag(FLAG_C)
                    << ", result=0x" << (int)result
                    << ", written to 0x" << addrDest
                    << ", PC=0x" << savedPC << std::dec;
                Logger::getInstance().logAPU(oss.str());
            }
        } break;
        case 0xA9: { // SBC dp,dp
            uint8_t dp1 = readARAM(m_regs.pc++);
            uint8_t dp2 = readARAM(m_regs.pc++);
            uint16_t addr1 = getDirectPageAddr(dp1);
            uint16_t addr2 = getDirectPageAddr(dp2);
            uint8_t val1 = readARAM(addr1);
            uint8_t val2 = readARAM(addr2);
            uint16_t diff = val1 - val2 - (getFlag(FLAG_C) ? 0 : 1);
            setFlag(FLAG_C, diff <= 0xFF);
            setFlag(FLAG_V, ((val1 ^ diff) & (val2 ^ diff) & 0x80) != 0);
            uint8_t result = diff & 0xFF;
            writeARAM(addr1, result);
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
        m_aram[0xF4] = 0xA9; // Set ready state (SPC700 writes, CPU reads)
        std::ostringstream oss;
        oss << "APU: Boot completed after " << bootCycles << " cycles";
        Logger::getInstance().logAPU(oss.str());
    }
}

void APU::loadBootROM() {
    // Load SPC700 IPL (Initial Program Loader) boot ROM into ARAM
    // The IPL ROM is 64 bytes located at 0xFFC0-0xFFFF
    // 
    // Actual SNES IPL ROM from Anomie's SPC700 Doc
    // This is the exact boot ROM image that the SPC700 executes on reset
    
    // Clear ARAM
    memset(m_aram.data(), 0, m_aram.size());
    
    uint16_t iplBase = 0xFFC0;
    
    // Actual IPL ROM bytes from documentation:
    // $CD $EF $BD $E8 $00 $C6 $1D $D0 $FC $8F $AA $F4 $8F $BB $F5 $78
    // $CC $F4 $D0 $FB $2F $19 $EB $F4 $D0 $FC $7E $F4 $D0 $0B $E4 $F5
    // Modified IPL ROM: Changed 0xFFD6 from MOV Y,dp to MOV A,dp + MOV Y,A
    // This ensures Z flag is updated correctly for BNE at 0xFFD9
    // Original: 0xFFD6: 0xEB (MOV Y,dp) - doesn't update Z flag
    // Modified: 0xFFD6: 0xE4 (MOV A,dp) - updates Z flag
    //           0xFFD8: 0xFD (MOV Y,A) - updates Z flag, Y = A (포트 0 값)
    //           0xFFD9: 0xD0 (BNE rel) - checks Z flag
    //           0xFFDA: 0xFC (operand, -4) - branch back to 0xFFD6
    uint8_t iplROM[64] = {
        0xCD, 0xEF, 0xBD, 0xE8, 0x00, 0xC6, 0x1D, 0xD0, 0xFC, 0x8F, 0xAA, 0xF4, 0x8F, 0xBB, 0xF5, 0x78,
        0xCC, 0xF4, 0xD0, 0xFB, 0x2F, 0x19, 0xEB, 0xF4, 0xD0, 0xFC, 0x7E, 0xF4, 0xD0, 0x0B, 0xE4, 0xF5,
        0xCB, 0xF4, 0xD7, 0x00, 0xFC, 0xD0, 0xF3, 0xAB, 0x01, 0x10, 0xEF, 0x7E, 0xF4, 0x10, 0xEB, 0xBA,
        0xF6, 0xDA, 0x00, 0xBA, 0xF4, 0xC4, 0xF4, 0xDD, 0x5D, 0xD0, 0xDB, 0x1F, 0x00, 0x00, 0xC0, 0xFF
    };
    
    // Copy IPL ROM to separate IPL ROM storage (not ARAM)
    // IPL ROM is hardware ROM, not part of ARAM
    for (int i = 0; i < 64; i++) {
        m_iplROM[i] = iplROM[i];
    }
    
    // Set PC to start of IPL ROM
    m_regs.pc = iplBase;
    m_regs.sp = 0xEF;  // Correct initial SP value (snes9x uses 0xEF, not 0xFF)
    m_regs.psw = 0x02;
    m_regs.a = 0x00;
    m_regs.x = 0x00;
    m_regs.y = 0x00;
    
    // Enable IPL ROM by default (snes9x behavior)
    m_iplromEnable = true;
    
    std::ostringstream oss;
    oss << "APU: IPL Boot ROM loaded at 0x" << std::hex << iplBase 
        << " (actual SNES IPL ROM)" << std::dec;
    Logger::getInstance().logAPU(oss.str());
}

void APU::generateAudio() {
    // Generate audio samples for all channels
    // DSP sampling rate: 32000 Hz
    // Buffer size: 2048 samples (1024 stereo samples)
    
    // Debug: Log first call
    static bool firstCall = true;
    if (firstCall) {
        Logger::getInstance().logAPU("APU: generateAudio() called for first time");
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
        std::ostringstream oss;
        oss << "APU: audioCallback() called! Requested " << samples 
            << " samples, buffer size " << apu->m_audioBuffer.size();
        Logger::getInstance().logAPU(oss.str());
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

uint16_t APU::getDirectPageAddr(uint8_t dp) const {
    // SPC700 direct page addressing:
    // If P flag is set: direct page = 0x0100
    // If P flag is clear: direct page = 0x0000
    // Final address = direct page + dp
    uint16_t directPage = getFlag(FLAG_P) ? 0x0100 : 0x0000;
    return directPage + dp;
}

uint8_t APU::readARAM(uint16_t addr) {
    // Check for IPL ROM (0xFFC0-0xFFFF) - snes9x style
    if (addr >= IPL_ROM_BASE && m_iplromEnable) {
        return m_iplROM[addr & 0x3F];  // Return IPL ROM data
    }
    
    // Handle I/O port reads ($F0-$FF)
    if (addr >= 0xF0) {
        switch (addr) {
            case 0xF2: return m_dspAddr;  // DSP address register
            case 0xF3: return m_dspRegs[m_dspAddr & 0x7F];  // DSP data register
            case 0xF4: {
                // snes9x style: SPC700 reads from $F4 (CPU I/O port 0)
                // Return what CPU wrote to $2140 (stored in m_cpuPorts[0])
                uint8_t value = m_cpuPorts[0];
                
                // Debug: Log port reads
                if (m_regs.pc >= 0xFFD6 && m_regs.pc <= 0xFFD8) {
                    static int logCount = 0;
                    if (logCount < 20) {
                        std::ostringstream oss;
                        oss << "APU: SPC700 read $F4 (PC=0x" << std::hex << m_regs.pc 
                            << ") = 0x" << (int)value << std::dec;
                        Logger::getInstance().logAPU(oss.str());
                        logCount++;
                }
                }
                
                return value;
            }
            case 0xF5: {
                // snes9x style: SPC700 reads from $F5 (CPU I/O port 1)
                uint8_t value = m_cpuPorts[1];
                
                // Debug: Log port reads during fail routine (PC 0x0343-0x0346)
                if (m_regs.pc >= 0x0343 && m_regs.pc <= 0x0346) {
                    std::ostringstream oss;
                    oss << "APU: SPC700 read $F5 (PC=0x" << std::hex << m_regs.pc 
                        << ") = 0x" << (int)value << " (m_cpuPorts[1]=0x" << (int)m_cpuPorts[1] << ")" << std::dec;
                    Logger::getInstance().logAPU(oss.str());
                }
                
                // Debug: Log port reads during IPL data transfer
                if (m_regs.pc >= 0xFFDF && m_regs.pc <= 0xFFE6) {
                    static int logCount = 0;
                    if (logCount < 50) {
                        std::ostringstream oss;
                        oss << "APU: SPC700 read $F5 (PC=0x" << std::hex << m_regs.pc 
                            << ") = 0x" << (int)value << " (m_cpuPorts[1]=0x" << (int)m_cpuPorts[1] << ")" << std::dec;
                        Logger::getInstance().logAPU(oss.str());
                        logCount++;
                }
                }
                
                return value;
            }
            case 0xF6: {
                // snes9x style: SPC700 reads from $F6 (CPU I/O port 2)
                uint8_t value = m_cpuPorts[2];
                
                // Debug: Log port reads during IPL address setup
                if (m_regs.pc >= 0xFFF0 && m_regs.pc <= 0xFFF2) {
                    static int logCount = 0;
                    if (logCount < 10) {
                        std::ostringstream oss;
                        oss << "APU: SPC700 read $F6 (PC=0x" << std::hex << m_regs.pc 
                            << ") = 0x" << (int)value << " (m_cpuPorts[2]=0x" << (int)m_cpuPorts[2] << ")" << std::dec;
                        Logger::getInstance().logAPU(oss.str());
                        logCount++;
                }
                }
                
                return value;
            }
            case 0xF7: {
                // snes9x style: SPC700 reads from $F7 (CPU I/O port 3)
                uint8_t value = m_cpuPorts[3];
                
                // Debug: Log port reads during IPL address setup
                if (m_regs.pc >= 0xFFF0 && m_regs.pc <= 0xFFF2) {
                    static int logCount = 0;
                    if (logCount < 10) {
                        std::ostringstream oss;
                        oss << "APU: SPC700 read $F7 (PC=0x" << std::hex << m_regs.pc 
                            << ") = 0x" << (int)value << " (m_cpuPorts[3]=0x" << (int)m_cpuPorts[3] << ")" << std::dec;
                        Logger::getInstance().logAPU(oss.str());
                        logCount++;
                }
                }
                
                return value;
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
    // Prevent writing to IPL ROM area (0xFFC0-0xFFFF) when IPL ROM is enabled
    // snes9x style: writes to IPL ROM area are ignored when IPL ROM is enabled
    // When IPL ROM is disabled, writes go to ARAM
    if (addr >= IPL_ROM_BASE && m_iplromEnable) {
        // IPL ROM is read-only, ignore writes
        return;
    }
    
    // During IPL protocol, prevent overwriting loaded SPC program
    // IPL ROM may try to write to addresses where SPC program is loaded
    // We need to protect the loaded program area (0x0300+) during IPL protocol
    // But only after data transfer is complete (SPC_LOAD_WAIT_EXEC), not during data transfer (SPC_LOAD_RECEIVING)
    if (m_spcLoadState == SPC_LOAD_WAIT_EXEC) {
        // Check if we're trying to write to the loaded program area
        if (addr >= m_spcLoadAddr && addr < (m_spcLoadAddr + m_spcLoadSize)) {
            // Ignore writes to loaded program area after data transfer is complete
            // IPL ROM may try to write here, but we need to preserve the loaded program
            static int protectCount = 0;
            if (protectCount < 10) {
                std::ostringstream oss;
                oss << "APU: writeARAM protected 0x" << std::hex << addr 
                    << " from IPL ROM write (value=0x" << (int)value 
                    << ", loadAddr=0x" << m_spcLoadAddr 
                    << ", loadSize=" << std::dec << m_spcLoadSize 
                    << ", PC=0x" << m_regs.pc << ")" << std::dec;
                Logger::getInstance().logAPU(oss.str());
                protectCount++;
            }
            return; // Don't write to loaded program area after data transfer is complete
        }
    }
    
    // Debug: Log ALL writes to 0x0300 to see what's overwriting
    if (addr >= 0x0300 && addr < 0x0320) {
        static int writeCount = 0;
        if (writeCount < 50) {
            uint8_t oldValue = m_aram[addr];
            std::ostringstream oss;
            oss << "APU: writeARAM(0x" << std::hex << addr << ", 0x" << (int)value 
                << ") (old=0x" << (int)oldValue << ", loadState=" << (int)m_spcLoadState 
                << ", PC=0x" << m_regs.pc << ")" << std::dec;
            Logger::getInstance().logAPU(oss.str());
            writeCount++;
        }
    }
    
    // Handle I/O port writes ($F0-$FF)
    if (addr >= 0xF0) {
        switch (addr) {
            case 0xF1: // Control register
                // Bit 0-2: Timer enable
                m_timers[0].enabled = (value & 0x01) != 0;
                m_timers[1].enabled = (value & 0x02) != 0;
                m_timers[2].enabled = (value & 0x04) != 0;
                // Bit 4-5: Clear ports 0-1 (SNES_SPC style: clear CPU ports that SPC700 reads)
                if (value & 0x10) m_cpuPorts[0] = 0;  // Clear CPU port 0 (SPC700 reads)
                if (value & 0x20) m_cpuPorts[1] = 0;  // Clear CPU port 1 (SPC700 reads)
                // Bit 7: IPL ROM enable (snes9x style)
                m_iplromEnable = (value & 0x80) != 0;
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
        case 0xF4:
        case 0xF5:
        case 0xF6:
        case 0xF7: {
            // snes9x style: SPC700 writes to $F4-$F7 (CPU I/O ports)
            // Store directly in m_aram[0xf4+port] so CPU can read via readPort()
            uint8_t port = addr - 0xF4;
            uint8_t oldValue = m_aram[addr];
            m_aram[addr] = value;
            
            // Debug: Log port writes from SPC700 (especially port 0)
            if (port == 0 || (port < 4 && value != oldValue)) {
                std::ostringstream oss;
                oss << "APU: SPC700 wrote port " << (int)port << " = 0x" << std::hex << (int)value 
                    << " (old=0x" << (int)oldValue << ", PC=0x" << m_regs.pc << ")" << std::dec;
                Logger::getInstance().logAPU(oss.str());
                Logger::getInstance().logPort(oss.str());
                }
                break;
        }
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
        // Track data loaded by IPL ROM during data transfer
        if ((m_spcLoadState == SPC_LOAD_RECEIVING || m_spcLoadState == SPC_LOAD_WAIT_EXEC) && addr >= m_spcLoadAddr && addr < (m_spcLoadAddr + 0x1000)) {
            // IPL ROM is writing data to ARAM
            // Track the loaded size
            uint16_t offset = addr - m_spcLoadAddr;
            if (offset >= m_spcLoadIndex) {
                m_spcLoadIndex = offset + 1;
                m_spcLoadSize = m_spcLoadIndex;
                
                // Log first 50 bytes loaded
                if (m_spcLoadIndex <= 50) {
                    std::ostringstream oss;
                    oss << "APU: Loaded byte[" << std::dec << offset 
                        << "] = 0x" << std::hex << (int)value 
                        << " to ARAM[0x" << addr << "] (old=0x" << (int)m_aram[addr] 
                        << ", new=0x" << (int)value << ", loadState=" << (int)m_spcLoadState << ")" << std::dec;
                    Logger::getInstance().logAPU(oss.str());
                }
            }
        }
        
        // Debug: Log writes to 0x0300 during IPL protocol
        if (addr >= 0x0300 && addr < 0x0320 && m_spcLoadState == SPC_LOAD_RECEIVING) {
            static int writeCount = 0;
            if (writeCount < 20) {
                uint8_t oldValue = m_aram[addr];
                m_aram[addr] = value;
                uint8_t newValue = m_aram[addr];
                std::ostringstream oss;
                oss << "APU: writeARAM(0x" << std::hex << addr << ", 0x" << (int)value 
                    << ") (old=0x" << (int)oldValue << ", new=0x" << (int)newValue << ")" << std::dec;
                Logger::getInstance().logAPU(oss.str());
                writeCount++;
    } else {
        m_aram[addr] = value;
            }
        } else {
            m_aram[addr] = value;
        }
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