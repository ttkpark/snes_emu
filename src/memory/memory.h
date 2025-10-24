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
    void write8(uint32_t address, uint8_t value);
    void write16(uint32_t address, uint16_t value);
    
    bool loadROM(const std::vector<uint8_t>& romData);
    
    // DMA operations
    void performDMA(uint8_t channel);
    
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
    
    // DMA registers (8 channels)
    struct DMAChannel {
        uint8_t control;      // $43x0 - Direction, mode
        uint8_t destAddr;     // $43x1 - Destination (PPU register)
        uint16_t sourceAddr;  // $43x2-3 - Source address
        uint8_t sourceBank;   // $43x4 - Source bank
        uint16_t size;        // $43x5-6 - Transfer size
    } m_dmaChannels[8];
    
    uint32_t translateAddress(uint32_t address);
};
