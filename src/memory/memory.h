#pragma once
#include <cstdint>
#include <vector>
#include <string>

class CPU;
class PPU;
class APU;
class SimpleInput;

class Memory {
public:
    Memory();
    
    void setCPU(CPU* cpu) { m_cpu = cpu; }
    void setPPU(PPU* ppu) { m_ppu = ppu; }
    void setAPU(APU* apu) { m_apu = apu; }
    void setInput(SimpleInput* input) { m_input = input; }
    
    uint8_t read8(uint32_t address);
    uint16_t read16(uint32_t address);
    uint32_t read24(uint32_t address);
    void write8(uint32_t address, uint8_t value);
    void write16(uint32_t address, uint16_t value);
    
    bool loadROM(const std::vector<uint8_t>& romData);
    
    // DMA operations
    void performDMA(uint8_t channel);
    int32_t m_dmaCyclesPending = 0; // Master cycles consumed by DMA

    // HDMA operations
    void initHDMA();
    void performHDMA(int scanline);
    uint8_t m_hdmaEnable = 0; // HDMAEN register ($420C)
    
    // Auto-Joypad operations
    void performAutoJoypadRead();
    void tickAutoJoypadBusy();  // call once per scanline to decrement busy countdown
    
    // ROM mapping detection
    enum class ROMMapping {
        LoROM,
        HiROM,
        ExLoROM,
        ExHiROM,
        Unknown
    };
    
    ROMMapping detectROMMapping();
    uint32_t getROMAddress(uint32_t address, ROMMapping mapping);
    
    // Set ROM mapping type from external analysis
    void setROMMapping(ROMMapping mapping);
    ROMMapping getROMMapping() const { return m_romMapping; }
    
    void setHeaderPhysicalAddress(uint32_t address) ;
    uint32_t getHeaderPhysicalAddress() const;

    // WRAM accessor for diagnostics
    const std::vector<uint8_t>& getWRAM() const { return m_wram; }

    // WRAM data port ($2180-$2183) public interface for PPU routing
    uint8_t  readWRAMPort();
    void     writeWRAMPort(uint8_t value);
    void     setWRAMAddress(uint32_t addr);  // sets m_wramPortAddr (17-bit)

private:
    std::vector<uint8_t> m_rom;
    std::vector<uint8_t> m_wram;  // 128KB
    std::vector<uint8_t> m_sram;  // 32KB
    
    ROMMapping m_romMapping;
    uint32_t m_headerPhysicalAddress;
    CPU* m_cpu;                    // CPU reference for cycle tracking
    PPU* m_ppu;                    // PPU reference for register writes
    APU* m_apu;                    // APU reference for port access
    SimpleInput* m_input;          // Input reference for controller reads
    
    // DMA registers (8 channels) — mirrors real SNES hardware layout
    struct DMAChannel {
        uint8_t control;        // $43x0 - Direction, mode
        uint8_t destAddr;       // $43x1 - Destination (PPU register)
        uint16_t sourceAddr;    // $43x2-3 - Source address
        uint8_t sourceBank;     // $43x4 - Source bank
        uint16_t size;          // $43x5-6 - Transfer size (also HDMA line count)
        uint16_t hdmaIndirect;  // $43x7-8 - HDMA indirect table address
        uint8_t hdmaIndBank;    // $43x9 - HDMA indirect bank
        uint8_t hdmaLineCount;  // $43xA - HDMA current line count
    } m_dmaChannels[8];

    // HDMA per-channel state
    struct HDMAChannel {
        uint32_t tableAddr;    // Current table pointer (24-bit: bank<<16 | addr)
        uint8_t  lineCounter;  // Lines remaining for current table entry
        bool     repeatMode;   // true = same data every scanline; false = new data each line
        bool     doTransfer;   // Whether to write data this scanline
        bool     terminated;   // true = table header 0x00 seen; channel inactive
    } m_hdmaChannels[8];
    
    // Auto-Joypad state
    bool m_autoJoypadEnabled;
    uint16_t m_joypadData[4];  // 4 controllers, 16 bits each
    bool m_autoJoypadBusy = false;      // $4212 bit0: auto-joypad read in progress
    int  m_autoJoypadBusyCountdown = 0; // ticks remaining for busy signal
    uint8_t m_wrioPrev = 0xFF;          // Previous $4201 (WRIO) value for HV latch edge detection

    // I/O registers ($4200-$43FF)
    uint8_t m_ioRegisters[0x200];

    // WRAM data port state ($2180-$2183)
    uint32_t m_wramPortAddr = 0;  // 17-bit WRAM address, auto-increments on $2180 access

    // CPU math hardware ($4202-$4206 / $4214-$4216)
    // Unsigned 8x8 multiply: WRMPYA($4202) * WRMPYB($4203) = RDMPY($4216-$4217)
    // Unsigned 16/8 divide: WRDIVL/H($4204-$4205) / WRDIVB($4206) = RDDIV($4214-$4215), RDMPY($4216-$4217)
    uint8_t  m_mathA = 0xFF;      // $4202 WRMPYA (multiply A operand, preserved after trigger)
    uint16_t m_mathDividend = 0;  // $4204-$4205 WRDIVL/H
    uint16_t m_mathRDDIV = 0;    // $4214-$4215 RDDIVL/H (divide quotient / multiply product high)
    uint16_t m_mathRDMPY = 0;    // $4216-$4217 RDMPYL/H (multiply product / divide remainder)

    uint32_t translateAddress(uint32_t address);
};
