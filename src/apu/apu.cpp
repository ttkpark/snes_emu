#include "apu.h"
#include "../cpu/cpu.h"
#include "../debug/logger.h"
#include <iostream>
#include <cstring>
#include <cmath>
#include <sstream>
#include <iomanip>
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
    
    // Execute SPC700 instructions ONLY until boot is complete
    // After boot, we use stub mode where I/O ports handle all communication
    if (!m_bootComplete) {
        executeSPC700Instruction();
    }
    
    // Check if boot is complete by monitoring port values
    if (!m_bootComplete && m_ports[0] == 0xAA && m_ports[1] == 0xBB) {
        m_bootComplete = true;
        m_ready = true;
        m_initialized = true;
        std::cout << "APU: Boot completed after " << stepCount << " steps - Ready signature detected (0xBBAA)" << std::endl;
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
    
    if (readCount <= 50) {
        std::cout << "APU read $" << std::hex << (0x2140 + port) 
                  << " = 0x" << (int)value << " (bootComplete=" << m_bootComplete << ")" << std::dec << std::endl;
    }
    
    // After boot is complete, APU automatically echoes back what CPU wrote
    // This simulates the SNES IPL handshake protocol where:
    // 1. CPU writes a value to port 0
    // 2. APU processes it and writes the same value back
    // 3. CPU reads and confirms the value matches
    // Since we're in stub mode, we just return what was written
    return value;
}

void APU::writePort(uint8_t port, uint8_t value) {
    if (port >= 4) return;
    
    static int writeCount = 0;
    writeCount++;
    
    if (writeCount <= 20) {
        std::cout << "APU write $" << std::hex << (0x2140 + port) 
                  << " = 0x" << (int)value << " (bootComplete=" << m_bootComplete << ")" << std::dec << std::endl;
    }
    
    // Store the written value
    m_ports[port] = value;
    
    // After boot is complete, implement automatic handshake response
    // CPU expects APU to echo back the same value to indicate acknowledgment
    // This simulates the SNES IPL ROM behavior without executing actual SPC700 code
    if (m_bootComplete && port == 0) {
        // APU automatically acknowledges by keeping the value
        // CPU will read this same value and proceed
    }
    
    // Handle DSP register writes (for future audio implementation)
    if (m_bootComplete && port == 0 && value >= 0x00 && value <= 0x7F) {
        // DSP register write
        m_dspRegs[value] = m_ports[1]; // Data comes from port 1
        m_dspEnabled = true;
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

void APU::executeSPC700Instruction() {
    // Execute SPC700 instruction
    uint8_t opcode = readARAM(m_regs.pc);
    
    // Log SPC700 execution (disabled - too verbose)
    // APU logging completely disabled for performance
    
    m_regs.pc++;
    
    // SPC700 instruction execution
    switch (opcode) {
        case 0x00: // NOP
            break;
            
        case 0x8F: { // MOV dp, #imm
            uint8_t imm = readARAM(m_regs.pc++);
            uint8_t dp = readARAM(m_regs.pc++);
            writeARAM(dp, imm);
            static int movCount = 0;
            if (movCount < 5 && dp >= 0xF4 && dp <= 0xF7) {
                std::cout << "[Cyc:" << std::dec << (m_cpu ? m_cpu->getCycles() : 0) << "] "
                          << "SPC700: MOV $" << std::hex << (int)dp << ", #$" << (int)imm << std::dec << std::endl;
                movCount++;
            }
        } break;
        
        case 0x2F: { // BRA rel
            int8_t offset = (int8_t)readARAM(m_regs.pc++);
            m_regs.pc += offset;
        } break;
        
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
        
        case 0xBC: { // INC A
            m_regs.a++;
            updateNZ(m_regs.a);
        } break;
        
        case 0xC4: { // MOV dp, A
            uint8_t dp = readARAM(m_regs.pc++);
            writeARAM(dp, m_regs.a);
        } break;
        
        case 0xE4: { // MOV A, dp
            uint8_t dp = readARAM(m_regs.pc++);
            m_regs.a = readARAM(dp);
            updateNZ(m_regs.a);
        } break;
        
        case 0xEF: { // SLEEP - wait for interrupt
            // In a real SPC700, this halts execution until an interrupt occurs
            // For now, just loop back (effectively a NOP in our simplified implementation)
            m_regs.pc--;  // Stay on SLEEP instruction
        } break;
        
        default:
            // Unknown opcode - log once
            static int unknownCount = 0;
            if (unknownCount < 5) {
                std::cout << "[Cyc:" << std::dec << (m_cpu ? m_cpu->getCycles() : 0) << "] "
                          << "SPC700: Unknown opcode 0x" << std::hex << (int)opcode 
                          << " at PC=0x" << (m_regs.pc - 1) << std::dec << std::endl;
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
                // Decode BRR sample
                int16_t brrSample = decodeBRR(i);
                
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
            case 0xF4: return m_ports[0];  // CPU I/O port 0
            case 0xF5: return m_ports[1];  // CPU I/O port 1
            case 0xF6: return m_ports[2];  // CPU I/O port 2
            case 0xF7: return m_ports[3];  // CPU I/O port 3
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
                m_dspRegs[dspAddr] = value;
                m_dspEnabled = true;
                
                // Handle Key On (0x4C)
                if (dspAddr == 0x4C) {
                    for (int i = 0; i < 8; i++) {
                        if (value & (1 << i)) {
                            m_channels[i].keyOn = true;
                            m_channels[i].enabled = true;
                        }
                    }
                }
                
                // Handle Key Off (0x5C)
                else if (dspAddr == 0x5C) {
                    for (int i = 0; i < 8; i++) {
                        if (value & (1 << i)) {
                            m_channels[i].envState = ENV_RELEASE;
                        }
                    }
                }
                
                // Handle ADSR2 (sustain level)
                else if (dspAddr >= 0x06 && dspAddr <= 0x76 && (dspAddr & 0x0F) == 0x06) {
                    int channel = (dspAddr >> 4) & 0x07;
                    m_channels[channel].sustainLevel = (value >> 5) & 0x07;
                }
                break;
            }
            case 0xF4: m_ports[0] = value; break;  // CPU I/O port 0
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
        
        // Set source address from DSP registers
        uint8_t srcDir = dsp[0x5D]; // DIR register
        uint8_t srcn = dsp[channel * 0x10 + 0x04]; // SRCN register for this channel
        ch.sourceAddr = (srcDir << 8) + (srcn << 2);
        ch.currentAddr = ch.sourceAddr;
    }
    
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