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
    
    // Communication ports $2140-$2143
    uint8_t m_ports[4];
    
    // APU State
    bool m_ready;
    bool m_initialized;
    bool m_bootComplete;
    int m_bootStep;
    int m_spc700Cycles;  // Cycle counter for accurate timing
    
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
        
        // ADSR/Gain envelope state
        EnvelopeState envState;
        uint16_t envLevel;          // Envelope level (0x0000-0x7FFF)
        uint8_t sustainLevel;       // Sustain level from ADSR2
        bool keyOn;                 // Key On flag
        
        AudioChannel() 
            : enabled(false), volume(0), frequency(0), waveform(0), phase(0)
            , currentSample(0), sourceAddr(0), currentAddr(0)
            , brrBytePos(0), brrNibblePos(0), brrHeader(0)
            , envState(ENV_DIRECT), envLevel(0), sustainLevel(0), keyOn(false) {
            samplePrev[0] = 0;
            samplePrev[1] = 0;
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
    
    // SPC700 helper functions
    void setFlag(uint8_t flag, bool value);
    bool getFlag(uint8_t flag) const;
    void updateNZ(uint8_t value);
    uint8_t readARAM(uint16_t addr);
    void writeARAM(uint16_t addr, uint8_t value);
    void push(uint8_t value);
    uint8_t pop();
};