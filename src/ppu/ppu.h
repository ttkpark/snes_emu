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

struct TileCache {
    bool valid = false;
    uint8_t pixels[8][8];
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
    uint8_t readRegisterImpl(uint16_t address);
    
    // VRAM, CGRAM, OAM access
    void writeVRAM(uint16_t address, uint8_t value);
    uint8_t readVRAM(uint16_t address);
    void writeCGRAM(uint16_t address, uint8_t value);
    void writeOAM(uint16_t address, uint8_t value);

    void invalidateTileCache(uint16_t address);
    TileCache* getTileCacheEntry(uint16_t tileAddr, int bpp);
    void decode2bpp(const uint8_t* tileData, uint8_t output[8][8]);
    void decode4bpp(const uint8_t* tileData, uint8_t output[8][8]);
    void decode8bpp(const uint8_t* tileData, uint8_t output[8][8]);
    
    // ROM data loading
    void loadROMData(const std::vector<uint8_t>& romData);
    
    // Background rendering functions
    uint32_t renderBackgroundMode1(int x);
    uint32_t renderBackgroundMode2(int x);
    uint32_t renderBackgroundMode3(int x);
    uint32_t renderBackgroundMode4(int x);
    uint32_t renderBackgroundMode5(int x);
    uint32_t renderBackgroundMode6(int x);
    uint32_t renderBackgroundMode7(int x);
    uint32_t renderTestPattern(int x);
    
    bool isFrameReady() const { return m_frameReady; }
    void clearFrameReady() { m_frameReady = false; }
    
    int getScanline() const { return m_scanline; }
    int getDot() const { return m_dot; }
    bool isNMIEnabled() const { return m_nmiEnabled; }
    bool isForcedBlank() const { return m_forcedBlank; }
    uint8_t getBrightness() const { return m_brightness; }
    uint16_t getVRAMAddress() const { return m_vramAddress; }  // Diagnostic: current VRAM word address
    uint16_t getOAMAddress() const { return m_oamAddress; }  // Diagnostic: current OAM byte address
    int getFrameCount() const;  // Diagnostic: current frame count
    uint8_t getBGMode() const { return m_bgMode; }  // Diagnostic: current BG mode
    uint8_t getMainScreenDesignation() const { return m_mainScreenDesignation; }  // Diagnostic: main screen layer enable
    // Returns VBlank start scanline based on $2133 OVERSCAN bit:
    // bit2=0 → 225 (NTSC standard), bit2=1 → 240 (OVERSCAN/PAL-like)
    int getVBlankStart() const { return (m_setini & 0x04) ? 240 : 225; }
    // Diagnostic: recent reads of RDNMI ($4210)
    const char* getRDNMIHistoryString();

    // Hardware latch trigger: called by auto-joypad process at VBlank start.
    // On real SNES the /LATCH pin fires during auto-joypad, setting bit7 of $213F.
    void triggerHVLatch();
    
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
    bool m_nmiEnabled;              // NMITIMEN register (0x4200)
    bool m_nmiFlag;                 // RDNMI register (0x4210)
    bool m_nmiAlreadyFiredThisVBlank; // Prevents double-NMI: edge-detection guard
    bool m_timeOver;        // STAT77 bit7: >32 OBJ on a scanline this frame
    bool m_rangeOver;       // STAT77 bit6: >256 OBJ pixels on a scanline this frame
    bool m_fieldBit;        // STAT78 bit6: interlace field (toggles each frame when $2133 bit0 set)

    // H/V Timer IRQ ($4200 bits 5-4, $4207-$420A, $4211)
    uint8_t m_irqMode;      // 0=off, 1=H-IRQ, 2=V-IRQ, 3=H+V-IRQ
    uint16_t m_htimer;      // H-IRQ position (0-339, $4207-$4208)
    uint16_t m_vtimer;      // V-IRQ position (0-261, $4209-$420A)
    bool m_irqFlag;         // TIMEUP: IRQ pending flag ($4211 bit7, clears on read)
    bool m_irqFiredThisDot; // Prevents re-firing on same dot
    
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
    
    // BG tilemap size settings (from BGSC registers, bits 0-1)
    // 0=32x32, 1=64x32, 2=32x64, 3=64x64
    uint8_t m_bgMapSize[4];  // Tilemap size for BG1-4
    
    // BG tile size settings (from BGMODE register, bits 4-7)
    // 0 = 8x8 tiles, 1 = 16x16 tiles
    bool m_bgTileSize[4];  // Tile size for BG1-4 (BG1=bit4, BG2=bit5, BG3=bit6, BG4=bit7)
    
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
    
    // Scroll write latch: single shared ppu1_mdr per bsnes/ares
    // (all BGnHOFS/VOFS writes share one latch; Mode 7 scroll has a separate latch)
    uint8_t m_scrollPrevX;   // shared BG scroll latch (ppu1_mdr equivalent)
    uint8_t m_scrollPrevY;   // kept separate for compatibility (real HW uses single latch)
    
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
    uint8_t m_coldata;     // Fixed Color Data ($2132): R/G/B 5-bit values packed
    uint8_t m_coldataR;    // Fixed color red component (5-bit)
    uint8_t m_coldataG;    // Fixed color green component (5-bit)
    uint8_t m_coldataB;    // Fixed color blue component (5-bit)
    uint8_t m_setini;      // Screen Mode/Video Select ($2133)

    // Mode 7 scroll latch (shared write latch for $210D/$210E in Mode 7)
    uint8_t m_m7hofs_prev; // Previous write latch for $210D (M7HOFS)
    uint8_t m_m7vofs_prev; // Previous write latch for $210E (M7VOFS)
    int16_t m_m7hofs;      // Mode 7 horizontal scroll offset (13-bit signed)
    int16_t m_m7vofs;      // Mode 7 vertical scroll offset (13-bit signed)
    
    // Mode 7 registers
    uint8_t m_m7sel;       // $211A
    int16_t m_m7a;         // $211B (signed 16-bit, 8.8 fixed point)
    int16_t m_m7b;         // $211C
    int16_t m_m7c;         // $211D
    int16_t m_m7d;         // $211E
    int16_t m_m7x;         // $211F center X (13-bit signed)
    int16_t m_m7y;         // $2120 center Y (13-bit signed)
    // Shared write latch for Mode 7 matrix/center registers
    uint8_t m_m7Latch;     // Last byte written to any M7 register (single shared latch)
    // Latches for Mode 7 16-bit writes
    bool m_m7aLatch, m_m7bLatch, m_m7cLatch, m_m7dLatch, m_m7xLatch, m_m7yLatch;
    uint8_t m_m7aPrev, m_m7bPrev, m_m7cPrev, m_m7dPrev, m_m7xPrev, m_m7yPrev;
    
    // Sprite settings ($2101)
    uint8_t m_objSize;      // Sprite size and name base
    
    // VRAM (Video RAM) - 64KB for tiles and tilemaps
    std::vector<uint8_t> m_vram;
    uint16_t m_vramAddress;
    uint8_t m_vramIncrement;    // Increment size: 0=1, 1=32, 2=128
    uint8_t m_vramMapping;      // Address mapping mode (bits 2-3 of $2115)
    uint8_t m_vramReadBuffer;    // VRAM prefetch low byte
    uint8_t m_vramReadBufferH;   // VRAM prefetch high byte
    bool m_vramIncrAfterHigh;   // VMAIN bit7: true=increment after $2119, false=after $2118
    std::vector<TileCache> m_tileCache2bpp;
    std::vector<TileCache> m_tileCache4bpp;
    std::vector<TileCache> m_tileCache8bpp;
    
    // CGRAM (Color Generator RAM) - 512 bytes for palettes
    std::vector<uint8_t> m_cgram;
    uint16_t m_cgramAddress;  // byte address (0-511); $2121 sets colorIndex*2
    
    // OAM (Object Attribute Memory) - 544 bytes for sprites
    std::vector<uint8_t> m_oam;
    uint16_t m_oamAddress;
    uint8_t  m_oamLatchByte;  // Write-twice latch for low OAM table (even-byte buffer)
    bool     m_oamLatchValid; // True when latch holds a buffered byte
    
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

    // Internal helper: apply scroll, compute tile coords, call renderBGx
    PixelInfo sampleBGLayer(int bgIndex, int screenX, int screenY,
                             int scrollX, int scrollY, int bpp, bool is16x16);
    
    // Window functions
    bool isWindowEnabled(int x, int bgIndex, bool isSprite);
    bool checkWindowMask(int x, uint8_t windowSettings);
    
    // Sub Screen and Color Math
    uint32_t renderSubScreen(int x);
    uint32_t applyColorMath(uint32_t mainColor, uint32_t subColor);
    
    // Helper functions
    uint16_t getVRAMIncrementSize() const;
    void incrementVRAMAddress();
    uint16_t applyVRAMMapping(uint16_t wordAddr) const;
    
    // Debug functions
    void dumpVRAM(const std::string& filename = "vram_dump.bin");
    void dumpVRAMHex(const std::string& filename = "vram_dump.txt");
    void dumpCGRAM(const std::string& filename = "cgram_dump.txt");
    
    // Text rendering functions
    void renderText(int x, int y, const std::string& text, uint32_t color);
    void drawChar(int x, int y, char c, uint32_t color);
};
