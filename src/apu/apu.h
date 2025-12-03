#pragma once
#include <cstdint>
#include <vector>

#ifdef USE_SDL
// Forward declare SDL2 types to avoid including SDL in header
typedef unsigned int SDL_AudioDeviceID;
struct SDL_AudioSpec;
#endif

class CPU;

class APU {
public:
    APU();
    ~APU();
    
    // Audio initialization
    bool initAudio();
    void cleanup();

    // APU control
    void reset();
    void step();
    void update() { step(); }  // Alias for compatibility with emulator
    
    // I/O port access
    uint8_t readPort(uint8_t port);
    void writePort(uint8_t port, uint8_t value);
    
    // Status check
    bool isReady() const { return m_ready; }
    
    // CPU connection for cycle tracking
    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void generateAudio();
    
private:
    CPU* m_cpu;
    // SPC700 CPU Registers
    struct SPC700Regs {
        uint8_t a;      // Accumulator
        uint8_t x;      // X register
        uint8_t y;      // Y register
        uint8_t sp;     // Stack pointer (0x0100-0x01FF)
        uint16_t pc;    // Program counter
        uint8_t psw;    // Program Status Word (N V P B H I Z C)
        
        SPC700Regs() : a(0), x(0), y(0), sp(0xEF), pc(0xFFC0), psw(0x02) {}
    } m_regs;
    
    // SPC700 PSW flags
    enum PSWFlags {
        FLAG_C = 0x01,  // Carry
        FLAG_Z = 0x02,  // Zero
        FLAG_I = 0x04,  // Interrupt enable
        FLAG_H = 0x08,  // Half carry
        FLAG_B = 0x10,  // Break
        FLAG_P = 0x20,  // Direct page
        FLAG_V = 0x40,  // Overflow
        FLAG_N = 0x80   // Negative
    };

    // APU Memory (64KB)
    std::vector<uint8_t> m_aram; // Audio RAM
    
    // IPL ROM (64 bytes at 0xFFC0-0xFFFF)
    // This is hardware ROM, not part of ARAM
    // When IPL ROM is enabled, reads from 0xFFC0-0xFFFF return IPL ROM data
    // When IPL ROM is disabled, reads from 0xFFC0-0xFFFF return ARAM data
    static constexpr uint16_t IPL_ROM_BASE = 0xFFC0;
    static constexpr uint16_t IPL_ROM_SIZE = 64;
    uint8_t m_iplROM[IPL_ROM_SIZE];
    bool m_iplromEnable;  // IPL ROM enable flag (controlled by $F1 bit 7)
    
    // Communication ports $2140-$2143
    // snes9x style: CPU->SPC via m_cpuPorts, SPC->CPU via m_aram[0xF4+port]
    uint8_t m_cpuPorts[4];  // CPU writes, SPC700 reads (via $F4-$F7)
    // SPC700 writes to $F4-$F7 are stored in m_aram[0xF4+port], CPU reads via readPort()
    
    // APU State
    bool m_ready;
    bool m_initialized;
    bool m_bootComplete;
    int m_bootStep;
    int m_spc700Cycles;  // Cycle counter for accurate timing
    
    // SPC Program Load State
    enum SPCLoadState {
        SPC_LOAD_IDLE,
        SPC_LOAD_WAIT_BBAA,
        SPC_LOAD_WAIT_CC,
        SPC_LOAD_RECEIVING,
        SPC_LOAD_WAIT_EXEC,
        SPC_LOAD_COMPLETE
    } m_spcLoadState;
    uint16_t m_spcLoadAddr;      // Destination address for SPC program
    uint16_t m_spcLoadSize;      // Size of SPC program being loaded
    uint16_t m_spcLoadIndex;     // Current byte index being loaded (actual cumulative count)
    uint16_t m_spcExecAddr;      // Execution address for SPC program
    uint8_t m_lastPort0Value;    // Last value written to Port 0 (for tracking byte transfers)
    uint16_t m_port0WrapCount;   // Number of times Port 0 value wrapped from high to low
    
    // Timers (3 timers) - Timer 0/1: 8kHz, Timer 2: 64kHz
    struct Timer {
        uint8_t counter;    // 4-bit counter (0-15)
        uint8_t target;     // Target value set by SPC700
        bool enabled;       // Enable flag
        int divider;        // Clock divider
        int stage;          // Current stage (0-7 for timer 0/1, 0-31 for timer 2)
        
        Timer() : counter(0), target(0), enabled(false), divider(0), stage(0) {}
    } m_timers[3];
    
    // DSP address register ($F2)
    uint8_t m_dspAddr;
    
    // DSP State
    bool m_dspEnabled;
    uint8_t m_dspRegs[128]; // DSP registers
    
    // Envelope states
    enum EnvelopeState {
        ENV_ATTACK,
        ENV_DECAY,
        ENV_SUSTAIN,
        ENV_RELEASE,
        ENV_DIRECT
    };
    
    // Audio output
    struct AudioChannel {
        bool enabled;
        uint8_t volume;
        uint16_t frequency;
        uint8_t waveform;
        uint8_t phase;
        
        // BRR decoding state
        int16_t samplePrev[2];      // Previous 2 samples for BRR filter
        int16_t currentSample;      // Current decoded sample
        uint16_t sourceAddr;        // BRR data start address (from DIR register)
        uint16_t currentAddr;       // Current ARAM read address
        uint8_t brrBytePos;         // Current byte position in BRR block (0-8)
        uint8_t brrNibblePos;       // Current nibble position (0-15)
        uint8_t brrHeader;          // Current BRR block header
        
        // Pitch and sample playback state
        uint16_t pitch;             // Current pitch value (14-bit: PITCH H[6:0] + PITCH L[7:0])
        uint32_t samplePos;         // Fixed-point sample position (16.16 format)
        uint32_t sampleStep;        // Fixed-point step per output sample (16.16 format)
        int16_t brrBuffer[16];      // BRR decoded sample buffer for interpolation
        uint8_t brrBufferIndex;     // Current position in BRR buffer
        
        // ADSR/Gain envelope state
        EnvelopeState envState;
        uint16_t envLevel;          // Envelope level (0x0000-0x7FFF)
        uint8_t sustainLevel;       // Sustain level from ADSR2
        bool keyOn;                 // Key On flag
        
        AudioChannel() 
            : enabled(false), volume(0), frequency(0), waveform(0), phase(0)
            , currentSample(0), sourceAddr(0), currentAddr(0)
            , brrBytePos(0), brrNibblePos(0), brrHeader(0)
            , pitch(0), samplePos(0), sampleStep(0), brrBufferIndex(0)
            , envState(ENV_DIRECT), envLevel(0), sustainLevel(0), keyOn(false) {
            samplePrev[0] = 0;
            samplePrev[1] = 0;
            for (int i = 0; i < 16; i++) brrBuffer[i] = 0;
        }
    } m_channels[8]; // 8 audio channels
    
    // Audio buffer
    std::vector<int16_t> m_audioBuffer;
    int m_audioBufferPos;
    
    // SDL Audio
#ifdef USE_SDL
    SDL_AudioDeviceID m_audioDevice;
    SDL_AudioSpec* m_audioSpec;
    static void audioCallback(void* userdata, uint8_t* stream, int len);
#endif
    
    // Boot sequence state
    enum BootState {
        BOOT_IDLE,
        BOOT_WAIT_AA,
        BOOT_WAIT_BB,
        BOOT_WAIT_CC,
        BOOT_WAIT_DD,
        BOOT_COMPLETE
    } m_bootState;
    
    void updateState();
    void executeSPC700Instruction();
    void updateTimers();
    void updateDSP();
    void handleBootSequence();
    void loadBootROM();
    void processAudioChannel(int channel);
    
    // BRR and Envelope functions
    int16_t decodeBRR(int channel);
    void updateEnvelopeAndPitch(int channel);
    int16_t getSampleWithPitch(int channel);
    
    // DSP register handling
    void handleDSPRegisterWrite(uint8_t addr, uint8_t value, uint8_t oldValue);
    
    // SPC700 helper functions
    void setFlag(uint8_t flag, bool value);
    bool getFlag(uint8_t flag) const;
    void updateNZ(uint8_t value);
    uint16_t getDirectPageAddr(uint8_t dp) const;  // Calculate direct page address
    uint8_t readARAM(uint16_t addr);
    void writeARAM(uint16_t addr, uint8_t value);
    void push(uint8_t value);
    uint8_t pop();
};