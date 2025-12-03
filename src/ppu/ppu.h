#pragma once
#include <cstdint>
#include <vector>
#include <string>
#ifdef USE_SDL
#include <SDL.h>
#endif

class Memory;
class CPU;

// Rendered pixel information (color and priority)
struct PixelInfo {
    uint32_t color;    // RGBA color
    uint8_t priority;  // Priority (0-3, higher is more important)
};

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
    uint32_t renderBackgroundMode6(int x);
    uint32_t renderTestPattern(int x);
    
    bool isFrameReady() const { return m_frameReady; }
    void clearFrameReady() { m_frameReady = false; }
    
    int getScanline() const { return m_scanline; }
    bool isNMIEnabled() const { return m_nmiEnabled; }
    bool isForcedBlank() const { return m_forcedBlank; }
    uint8_t getBrightness() const { return m_brightness; }
    // Diagnostic: recent reads of RDNMI ($4210)
    const char* getRDNMIHistoryString();
    
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
    
    // H/V Counter latch
    uint16_t m_latchedH;   // Latched horizontal counter (dot position)
    uint16_t m_latchedV;   // Latched vertical counter (scanline)
    bool m_hvLatchRead;    // Whether H/V latch has been read (for $213C/$213D)
    bool m_hvLatchHRead;   // Whether H counter low byte has been read
    bool m_hvLatchVRead;   // Whether V counter low byte has been read
    // Ring buffer to store recent RDNMI read values for diagnostics
    static const int RDNMI_HISTORY_SIZE = 64;
    uint8_t m_rdnmiHistory[RDNMI_HISTORY_SIZE] = {0};
    int m_rdnmiHistoryIndex = 0;
    // Cached printable buffer for quick logging
    char m_rdnmiHistoryStr[3 * RDNMI_HISTORY_SIZE + 1] = {0};
    
    // BG tile/tilemap addresses (individual variables)
    uint16_t m_bg1TileAddr;
    uint16_t m_bg1MapAddr;
    uint16_t m_bg2TileAddr;
    uint16_t m_bg2MapAddr;
    uint16_t m_bg3TileAddr;
    uint16_t m_bg3MapAddr;
    uint16_t m_bg4TileAddr;
    uint16_t m_bg4MapAddr;
    
    // BG tile/tilemap addresses (array form, used in renderBGx)
    uint16_t m_bgMapAddr[4];   // Tilemap addresses for BG1-4
    uint16_t m_bgTileAddr[4];  // Tile data addresses for BG1-4
    
    // BG tilemap size settings (from BGSC registers, bit 7)
    // 0 = 32x32 tiles, 1 = 64x64 tiles
    bool m_bgMapSize[4];  // Tilemap size for BG1-4
    
    // BG tile size settings (from BGMODE register, bits 3-6)
    // 0 = 8x8 tiles, 1 = 16x16 tiles
    bool m_bgTileSize[4];  // Tile size for BG1-4 (BG1=bit5, BG2=bit6, BG3=bit3, BG4=bit4)
    
    // Mosaic settings
    uint8_t m_mosaicSize;      // Mosaic size (bits 0-3 of $2106)
    bool m_mosaicEnabled[4];   // Mosaic enable for each BG (bits 4-7 of $2106)
    
    // BG priority settings [bgIndex][priorityGroup]
    // priorityGroup: 0=low priority, 1=high priority
    uint8_t m_bgPriority[4][2];
    
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
    
    // Window settings
    uint8_t m_w12sel;      // Window settings for BG1/BG2 ($2123)
    uint8_t m_w34sel;      // Window settings for BG3/BG4 ($2124)
    uint8_t m_wobjsel;     // Window settings for OBJ ($2125)
    uint16_t m_wh0, m_wh1; // Window 1 left/right ($2126-$2127)
    uint16_t m_wh2, m_wh3; // Window 2 left/right ($2128-$2129)
    uint8_t m_wbglog;      // Window mask logic for BGs ($212A)
    uint8_t m_wobjlog;     // Window mask logic for OBJ ($212B)
    uint8_t m_tmw;         // Window mask for main screen ($212E)
    uint8_t m_tsw;         // Window mask for sub screen ($212F)
    
    // Color Math settings
    uint8_t m_cgws;        // Color Math control ($2130)
    uint8_t m_cgadsub;     // Color Math settings ($2131)
    
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
    PixelInfo renderBG3Pixel(int x, int y);
    PixelInfo renderBG4Pixel(int x, int y);
    uint32_t renderSprites(int x, int y);
    PixelInfo renderSpritePixel(int x, int y);  // Returns sprite pixel with priority
    uint32_t getColor(uint8_t paletteIndex, uint8_t colorIndex);
    uint32_t getColor(uint8_t paletteIndex, uint8_t colorIndex, int bpp);
    void decodeTile(const uint8_t* tileData, uint8_t output[64], int bpp);
    
    // BG layer rendering (unified function)
    PixelInfo renderBGx(int bgIndex, int tileX, int tileY, int pixelX, int pixelY, int bpp = 2);
    
    // Window functions
    bool isWindowEnabled(int x, int bgIndex, bool isSprite);
    bool checkWindowMask(int x, uint8_t windowSettings);
    
    // Sub Screen and Color Math
    uint32_t renderSubScreen(int x);
    uint32_t applyColorMath(uint32_t mainColor, uint32_t subColor);
    
    // Helper functions
    uint16_t getVRAMIncrementSize() const;
    void incrementVRAMAddress();
    
    // Debug functions
    void dumpVRAM(const std::string& filename = "vram_dump.bin");
    void dumpVRAMHex(const std::string& filename = "vram_dump.txt");
    void dumpCGRAM(const std::string& filename = "cgram_dump.txt");
    
    // Text rendering functions
    void renderText(int x, int y, const std::string& text, uint32_t color);
    void drawChar(int x, int y, char c, uint32_t color);
};
