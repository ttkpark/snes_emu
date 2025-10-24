#pragma once
#include <cstdint>
#include <vector>
#ifdef USE_SDL
#include <SDL.h>
#endif

class Memory;
class CPU;

class PPU {
public:
    PPU();
    ~PPU();
    
    // Update PPU state for one scanline
    void step();
    
    // CPU connection for NMI
    void setCPU(CPU* cpu) { m_cpu = cpu; }
    
    // Get the current framebuffer (256x224 pixels, 32-bit RGBA)
    uint32_t* getFramebuffer() { return m_framebuffer; }
    
    // SDL2 video initialization and rendering
    bool initVideo();
    void renderFrame();
    void cleanup();
    
    // PPU register writes (called by Memory when CPU writes to $2100-$21FF)
    void writeRegister(uint16_t address, uint8_t value);
    uint8_t readRegister(uint16_t address);
    
    // VRAM, CGRAM, OAM access
    void writeVRAM(uint16_t address, uint8_t value);
    uint8_t readVRAM(uint16_t address);
    void writeCGRAM(uint8_t address, uint8_t value);
    void writeOAM(uint16_t address, uint8_t value);
    
    // ROM data loading
    void loadROMData(const std::vector<uint8_t>& romData);
    
    // Background rendering functions
    uint32_t renderBackgroundMode1(int x);
    uint32_t renderTestPattern(int x);
    
    bool isFrameReady() const { return m_frameReady; }
    void clearFrameReady() { m_frameReady = false; }
    
    int getScanline() const { return m_scanline; }
    
    // Screen dimensions
    static const int SCREEN_WIDTH = 256;
    static const int SCREEN_HEIGHT = 224;
    
private:
    
    // CPU reference for NMI
    CPU* m_cpu;
    
    // PPU registers
    uint8_t m_brightness;
    uint8_t m_bgMode;
    bool m_forcedBlank;
    bool m_nmiEnabled;      // NMITIMEN register (0x4200)
    bool m_nmiFlag;         // RDNMI register (0x4210)
    
    // BG tile/tilemap addresses
    uint16_t m_bg1TileAddr;
    uint16_t m_bg1MapAddr;
    uint16_t m_bg2TileAddr;
    uint16_t m_bg2MapAddr;
    uint16_t m_bg3TileAddr;
    uint16_t m_bg3MapAddr;
    uint16_t m_bg4TileAddr;
    uint16_t m_bg4MapAddr;
    
    // BG scroll registers ($210D-$2114)
    uint16_t m_bg1ScrollX;
    uint16_t m_bg1ScrollY;
    uint16_t m_bg2ScrollX;
    uint16_t m_bg2ScrollY;
    uint16_t m_bg3ScrollX;
    uint16_t m_bg3ScrollY;
    uint16_t m_bg4ScrollX;
    uint16_t m_bg4ScrollY;
    
    // Scroll write latches (for 16-bit writes)
    bool m_scrollLatchX;
    uint8_t m_scrollPrevX;
    bool m_scrollLatchY;
    uint8_t m_scrollPrevY;
    
    // Main/Sub screen designation ($212C-$212E)
    uint8_t m_mainScreenDesignation;
    uint8_t m_subScreenDesignation;
    uint8_t m_colorMath;
    
    // Sprite settings ($2101)
    uint8_t m_objSize;      // Sprite size and name base
    
    // VRAM (Video RAM) - 64KB for tiles and tilemaps
    std::vector<uint8_t> m_vram;
    uint16_t m_vramAddress;
    uint8_t m_vramIncrement;  // Increment size: 0=1, 1=32, 2=128
    uint8_t m_vramMapping;    // Address mapping mode (bits 2-3 of $2115)
    uint8_t m_vramReadBuffer; // VRAM read buffer
    
    // CGRAM (Color Generator RAM) - 512 bytes for palettes
    std::vector<uint8_t> m_cgram;
    uint8_t m_cgramAddress;
    
    // OAM (Object Attribute Memory) - 544 bytes for sprites
    std::vector<uint8_t> m_oam;
    uint16_t m_oamAddress;
    
    // Framebuffer - rendered output
    uint32_t* m_framebuffer;
    
    // SDL2 video components
#ifdef USE_SDL
    SDL_Window* m_window;
    SDL_Renderer* m_renderer;
    SDL_Texture* m_texture;
#endif
    bool m_videoInitialized;
    
    // Scanline rendering
    int m_scanline;
    int m_dot;  // Dot counter (0-340, 341 dots per scanline)
    bool m_frameReady;
    
    // Rendering functions
    void renderScanline();
    void renderBackground();
    uint32_t renderBackgroundMode0(int x);
    uint32_t renderBG1(int x, int y);
    uint32_t renderBG2(int x, int y);
    uint32_t renderBG3(int x, int y);
    uint32_t renderBG4(int x, int y);
    uint32_t renderSprites(int x, int y);
    uint32_t getColor(uint8_t paletteIndex, uint8_t colorIndex);
    void decodeTile(const uint8_t* tileData, uint8_t output[64], int bpp);
    
    // Helper functions
    uint16_t getVRAMIncrementSize() const;
    void incrementVRAMAddress();
};
