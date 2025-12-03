#include "ppu.h"
#include "../cpu/cpu.h"
#include "../debug/logger.h"
#include <cstdio>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <set>
#include <fstream>
#ifdef USE_SDL
#include <SDL.h>
#endif

PPU::PPU() 
    : m_cpu(nullptr)
    , m_brightness(15)
    , m_bgMode(0)
    , m_forcedBlank(false)
    , m_nmiEnabled(false)
    , m_nmiFlag(false)
    , m_latchedH(0)
    , m_latchedV(0)
    , m_hvLatchRead(false)
    , m_hvLatchHRead(false)
    , m_hvLatchVRead(false)
    , m_bg1TileAddr(0)
    , m_bg1MapAddr(0)
    , m_bg2TileAddr(0)
    , m_bg2MapAddr(0)
    , m_bg3TileAddr(0)
    , m_bg3MapAddr(0)
    , m_bg4TileAddr(0)
    , m_bg4MapAddr(0)
    , m_bg1ScrollX(0)
    , m_bg1ScrollY(0)
    , m_bg2ScrollX(0)
    , m_bg2ScrollY(0)
    , m_bg3ScrollX(0)
    , m_bg3ScrollY(0)
    , m_bg4ScrollX(0)
    , m_bg4ScrollY(0)
    , m_scrollLatchX(false)
    , m_scrollPrevX(0)
    , m_scrollLatchY(false)
    , m_scrollPrevY(0)
    , m_mainScreenDesignation(0)
    , m_subScreenDesignation(0)
    , m_colorMath(0)
    , m_w12sel(0)
    , m_w34sel(0)
    , m_wobjsel(0)
    , m_wh0(0)
    , m_wh1(0)
    , m_wh2(0)
    , m_wh3(0)
    , m_wbglog(0)
    , m_wobjlog(0)
    , m_tmw(0)
    , m_tsw(0)
    , m_cgws(0)
    , m_cgadsub(0)
    , m_objSize(0)
    , m_vramAddress(0)
    , m_vramIncrement(0)
    , m_vramMapping(0)
    , m_vramReadBuffer(0)
    , m_cgramAddress(0)
    , m_oamAddress(0)
    , m_scanline(0)
    , m_dot(0)
    , m_frameReady(false)
#ifdef USE_SDL
    , m_window(nullptr)
    , m_renderer(nullptr)
    , m_texture(nullptr)
#endif
    , m_videoInitialized(false)
    , m_framebuffer(nullptr) // RGBA framebuffer
{
    m_vram.resize(64 * 1024, 0);
    m_cgram.resize(512, 0);
    m_oam.resize(544, 0);
    m_framebuffer = new uint32_t[SCREEN_WIDTH * SCREEN_HEIGHT];
    
    // Initialize BG address arrays
    m_bgMapAddr[0] = 0;  // BG1
    m_bgMapAddr[1] = 0;  // BG2
    m_bgMapAddr[2] = 0;  // BG3
    m_bgMapAddr[3] = 0;  // BG4
    
    m_bgTileAddr[0] = 0; // BG1
    m_bgTileAddr[1] = 0; // BG2
    m_bgTileAddr[2] = 0; // BG3
    m_bgTileAddr[3] = 0; // BG4
    
    // Initialize BG tilemap sizes (default: 32x32)
    m_bgMapSize[0] = false; // BG1
    m_bgMapSize[1] = false; // BG2
    m_bgMapSize[2] = false; // BG3
    m_bgMapSize[3] = false; // BG4
    
    m_bgTileSize[0] = false; // BG1: 8x8 tiles (default)
    m_bgTileSize[1] = false; // BG2: 8x8 tiles (default)
    m_bgTileSize[2] = false; // BG3: 8x8 tiles (default)
    m_bgTileSize[3] = false; // BG4: 8x8 tiles (default)
    
    // Initialize Mosaic settings
    m_mosaicSize = 0;
    m_mosaicEnabled[0] = false; // BG1
    m_mosaicEnabled[1] = false; // BG2
    m_mosaicEnabled[2] = false; // BG3
    m_mosaicEnabled[3] = false; // BG4
    
    // Initialize BG priority (Mode 0 defaults)
    // Mode 0: BG1=3/0, BG2=2/0, BG3=1/0, BG4=0/0 (high priority / low priority)
    m_bgPriority[0][0] = 0;  // BG1 low priority
    m_bgPriority[0][1] = 3;  // BG1 high priority
    m_bgPriority[1][0] = 0;  // BG2 low priority  
    m_bgPriority[1][1] = 2;  // BG2 high priority
    m_bgPriority[2][0] = 0;  // BG3 low priority
    m_bgPriority[2][1] = 1;  // BG3 high priority
    m_bgPriority[3][0] = 0;  // BG4 low priority
    m_bgPriority[3][1] = 0;  // BG4 high priority
    
    // Set initial background color in CGRAM to black (0x0000)
    m_cgram[0] = 0x00;
    m_cgram[1] = 0x00;
}
PPU::~PPU(){
    delete[] m_framebuffer;
    m_framebuffer = nullptr;
}

bool PPU::initVideo() {
#ifdef USE_SDL
    // Initialize SDL video subsystem
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        std::cerr << "Failed to initialize SDL video: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create window
    m_window = SDL_CreateWindow("SNES Emulator", 
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               SCREEN_WIDTH * 2, SCREEN_HEIGHT * 2, 
                               SDL_WINDOW_SHOWN);
    if (!m_window) {
        std::cerr << "Failed to create window: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create renderer
    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer) {
        std::cerr << "Failed to create renderer: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // Create texture for SNES framebuffer
    m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_RGBA8888,
                                  SDL_TEXTUREACCESS_STREAMING,
                                  SCREEN_WIDTH, SCREEN_HEIGHT);
    if (!m_texture) {
        std::cerr << "Failed to create texture: " << SDL_GetError() << std::endl;
        return false;
    }

#endif
    
    // Initialize framebuffer with black (background color)
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int index = y * SCREEN_WIDTH + x;
            // Initialize to black (0x000000FF = opaque black in 0xRRGGBBAA format)
            m_framebuffer[index] = 0x000000FF; // Black (0xRRGGBBAA format)
        }
    }

    m_videoInitialized = true;
    std::cout << "PPU: Video initialized - " << SCREEN_WIDTH << "x" << SCREEN_HEIGHT << std::endl;
    return true;
}
int frameCount = 0;

void PPU::renderFrame() {
    #ifdef USE_SDL
    if (!m_videoInitialized || !m_texture || !m_renderer) {
        return;
    }
    
    Uint32* pixels = NULL;
    int pitch = 0;
    
    if (SDL_LockTexture(m_texture, NULL, (void**)&pixels, &pitch) == 0) {
        // Copy framebuffer to texture
        // Framebuffer is 256x224 RGBA8888
        memcpy(pixels, m_framebuffer, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(Uint32));
        
        SDL_UnlockTexture(m_texture);
    }

    SDL_RenderClear(m_renderer); 
    SDL_RenderCopy(m_renderer, m_texture, NULL, NULL);
    SDL_RenderPresent(m_renderer);
    #endif

    frameCount++;
    if (frameCount % 300 == 0) {  // Only log every 5 seconds (300 frames at 60fps)
        std::cout << "PPU: Rendered frame " << frameCount << std::endl;
    }
}

void PPU::cleanup() {
    // Dump VRAM before cleanup (program exit)
    dumpVRAMHex("vram_dump.txt");
    dumpCGRAM("cgram_dump.txt");
    std::cout << "PPU: VRAM and CGRAM dumps saved on program exit" << std::endl;
    
#ifdef USE_SDL
    if (m_texture) {
        SDL_DestroyTexture(m_texture);
        m_texture = nullptr;
    }
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
#endif
    m_videoInitialized = false;
}

void PPU::step() {
    // Log PPU events
    static int frameCount = 0;
    static int logCount = 0;
    static int stepCount = 0;
    
    // Increment dot counter (341 dots per scanline)
    m_dot++;
    
    if (m_dot >= 341) {
        m_dot = 0;
        
        // Render scanline when it completes
        if (m_scanline < SCREEN_HEIGHT) {
            renderScanline();
        }
        
        m_scanline++;
    
    // V-Blank start at scanline 225 (0xE1)
    if (m_scanline == 225) {
        m_nmiFlag = true;  // Set NMI flag when VBlank starts
        
        
        if ((m_nmiEnabled) && m_cpu) {
            m_cpu->triggerNMI();
        }
    }
    
    // Note: Do NOT keep m_nmiFlag set during entire VBlank period
    // RDNMI readRegister() will check current scanline directly
    
    }
    
    // V-Blank period (scanlines 225-261)
    // SNES has 262 scanlines total: 224 visible + 1 pre-render + 37 VBlank
    if (m_scanline >= 262) {
        m_scanline = 0;
        m_dot = 0;
        m_frameReady = true;
        m_nmiFlag = false;  // Clear NMI flag at frame start (VBlank ends)
        frameCount++;
        
        // Dump VRAM after first frame (for debugging)
        static bool vramDumped = false;
        if (!vramDumped && frameCount >= 1) {
            dumpVRAMHex("vram_dump.txt");
            dumpCGRAM("cgram_dump.txt");
            vramDumped = true;
            std::cout << "PPU: VRAM and CGRAM dumps completed after frame " << frameCount << std::endl;
        }
        
        // Log frame completion
        if (logCount < 500) {
            std::ostringstream oss;
            oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                << "Event: Frame Complete | "
                << "Total Scanlines: 262";
            Logger::getInstance().logPPU(oss.str());
            Logger::getInstance().flush();  // Flush at frame end
            logCount++;
        }
    }
}

void PPU::renderScanline() {
#ifdef DEBUG_PPU_RENDER
    // Debug: Print when this is called
    static int callCount = 0;
    if (callCount < 5) {
        std::cout << "renderScanline() called for scanline " << m_scanline << std::endl;
        callCount++;
    }
#endif
    
    // Safety: if display is enabled but brightness is 0, bump to visible level
    if (!m_forcedBlank && m_brightness == 0) {
        m_brightness = 15;
    }
    // Check if forced blank is enabled
    if (m_forcedBlank) {
        // During forced blank, keep previous frame data instead of filling with black
        // This prevents flickering when CPU is slow
        return;
    }
    
    //std::cout << "  Forced blank is OFF - rendering graphics" << std::endl;
    
    // Render actual SNES graphics
    static int renderCallCount = 0;
    if (renderCallCount < 5 && m_scanline < 5) {
        std::cout << "PPU: renderScanline() called - scanline=" << m_scanline 
                  << ", bgMode=" << (int)m_bgMode 
                  << ", mainScreenDesignation=0x" << std::hex << (int)m_mainScreenDesignation << std::dec << std::endl;
        renderCallCount++;
    }
    
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        uint32_t mainColor = 0x000000FF; // Default to black (0xRRGGBBAA format)
        uint32_t subColor = 0x000000FF;
        
        // Hardcoded: Only render tiles on the left half of the screen (x < 128)
        // Right half will remain black (no tiles)
        if (x < SCREEN_WIDTH / 2) {
            // Render background layers for Main Screen (left half only)
            if (m_bgMode == 0) {
                // Mode 0: 4 layers of 2bpp tiles
                mainColor = renderBackgroundMode0(x);
            } else if (m_bgMode == 1) {
                // Mode 1: BG1/BG2 4bpp, BG3 2bpp
                mainColor = renderBackgroundMode1(x);
            } else if (m_bgMode == 6) {
                // Mode 6: BG1 4bpp hires with offset per tile
                mainColor = renderBackgroundMode6(x);
            } else {
                // Other modes: simple test pattern
                mainColor = renderTestPattern(x);
            }
        } else {
            // Right half: no tiles, keep black background
            mainColor = 0x000000FF; // Black (0xRRGGBBAA format)
        }
        
        // Render Sub Screen if enabled
        if (m_subScreenDesignation != 0) {
            subColor = renderSubScreen(x);
        }
        
        // Render sprites and composite with background
        PixelInfo spritePixel = renderSpritePixel(x, m_scanline);
        
        // Composite sprites with background based on priority
        uint32_t bgColor = mainColor;
        if (spritePixel.color != 0) {
            // Check sprite priority against background
            // In Mode 0, sprites have priority based on their attributes
            // For simplicity, assume sprite priority is higher than most BGs
            // TODO: Implement proper priority comparison based on sprite attributes
            bool spriteBehind = false;
            // Simple priority check: if BG has very high priority (3), sprite might be behind
            // This is a simplified version - full implementation needs OAM extended data
            if (mainColor != 0 && spritePixel.priority <= 1) {
                // Check if any BG has priority >= 3
                // For now, always show sprite if it exists and is not transparent
                spriteBehind = false;
            }
            
            if (!spriteBehind) {
                bgColor = spritePixel.color;
            }
        }
        
        // Apply window masking
        // Check if this pixel should be masked by window
        bool windowMasked = false;
        // Window masking is complex - simplified version for now
        // Full implementation needs to check window settings for each layer
        
        // Apply Color Math if enabled
        uint32_t finalColor = bgColor;
        if ((m_cgws & 0x20) != 0 && m_subScreenDesignation != 0) {
            // Color Math enabled
            finalColor = applyColorMath(bgColor, subColor);
        } else {
            finalColor = bgColor;
        }
        
#ifdef DEBUG_PPU_RENDER
        if (m_scanline == 0 && x == 0) {
            std::ostringstream oss;
            oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                << "Scanline:" << std::setw(3) << m_scanline << " | "
                << "Mode: " << (int)m_bgMode << std::dec << std::endl;
            Logger::getInstance().logPPU(oss.str());
        }
#endif
        
        // Apply brightness
        if (m_brightness < 15) {
            // Extract RGB from 0xRRGGBBAA format
            uint8_t r = ((finalColor >> 24) & 0xFF);
            uint8_t g = ((finalColor >> 16) & 0xFF);
            uint8_t b = ((finalColor >> 8) & 0xFF);
            uint8_t a = (finalColor & 0xFF);
            
            r = (r * m_brightness) / 15;
            g = (g * m_brightness) / 15;
            b = (b * m_brightness) / 15;
            
            finalColor = (r << 24) | (g << 16) | (b << 8) | a;
        }
        
        m_framebuffer[m_scanline * SCREEN_WIDTH + x] = finalColor;
    }
    
#ifdef DEBUG_PPU_RENDER
    // Debug: Print first few pixels of first scanline
    if (m_scanline == 0 && callCount <= 5) {
        std::ostringstream oss;
        for (int i = 0; i < 5; i++) {
            oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
            << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
            << "  First 5 pixels: " << std::hex << "0x" << m_framebuffer[m_scanline * SCREEN_WIDTH + i] << " ";
        }
        oss << std::dec << std::endl;   
        Logger::getInstance().logPPU(oss.str());
        Logger::getInstance().flush();  // Flush at frame end
    }
#endif
    
}

void PPU::renderBackground() {
    // TODO: Implement proper background rendering
    // For now, just fill with background color
}
// Member variables defined in PPU class:
// m_bgMapAddr[4]: Tilemap start addresses for BG1, BG2, BG3, BG4
// m_bgTileAddr[4]: Tile data start addresses for BG1, BG2, BG3, BG4
// m_bgPriority[4][2]: Two priority levels for each background (e.g., BG1_PRIO_A, BG1_PRIO_B)
// m_cgram: CGRAM (Palette RAM)

// Internal utility function to calculate pixel information for BG1~BG4:
// Supports both 2bpp and 4bpp tile decoding based on BG mode
// tileX, tileY, pixelX, pixelY are already scrolled coordinates from the caller
PixelInfo PPU::renderBGx(int bgIndex, int tileX, int tileY, int pixelX, int pixelY, int bpp) {
    // Determine tile size based on bpp
    const int TILE_SIZE_BYTES = (bpp == 4) ? 32 : 16;
    
    // Check if this BG uses 16x16 tiles
    bool is16x16 = m_bgTileSize[bgIndex];
    int physicalTileSize = is16x16 ? 16 : 8;
    
    // Get scroll values for fine scrolling calculation
    // Note: scrollX/Y here are the RAW scroll values (not converted to signed yet)
    // The caller (renderBackgroundMode0) has already applied scroll to calculate tileX/Y and pixelX/Y
    // So we only need to apply fine scrolling (sub-pixel offset) within the current tile
    int scrollX, scrollY;
    switch (bgIndex) {
        case 0: scrollX = m_bg1ScrollX; scrollY = m_bg1ScrollY; break;
        case 1: scrollX = m_bg2ScrollX; scrollY = m_bg2ScrollY; break;
        case 2: scrollX = m_bg3ScrollX; scrollY = m_bg3ScrollY; break;
        case 3: scrollX = m_bg4ScrollX; scrollY = m_bg4ScrollY; break;
        default: scrollX = 0; scrollY = 0; break;
    }
    
    // Convert to signed for fine scroll calculation
    int signedScrollX = (scrollX & 0x03FF);
    int signedScrollY = (scrollY & 0x03FF);
    if (signedScrollX >= 512) signedScrollX -= 1024;
    if (signedScrollY >= 512) signedScrollY -= 1024;
    
    // Detailed logging for text line (scanline 1, BG1 only)
    // Text appears at scanline 1 (BG1VOFS=-1 hides scanline 0)
    bool shouldLog = false;//(bgIndex == 0 && m_scanline == 1);
    static int logCount = 0;
    const int MAX_LOG_PIXELS = 256;  // Log first 256 pixels of the text line
    
    // Handle tilemap wrapping (32x32 or 64x64 tilemap)
    int tilemapWidth = m_bgMapSize[bgIndex] ? 64 : 32;
    // Wrap tile coordinates to tilemap size (handle negative values correctly)
    // For negative values, use modulo arithmetic to ensure positive wrap
    int wrappedTileX = ((tileX % tilemapWidth) + tilemapWidth) % tilemapWidth;
    int wrappedTileY = ((tileY % tilemapWidth) + tilemapWidth) % tilemapWidth;
    
    // 1. Calculate tilemap address (32x32 or 64x64 tilemap)
    // SNES tilemap entries are 2 bytes each
    // Formula: mapAddr = base + (tileY * tilemapWidth + tileX) * 2
    // BG1SC register sets the base address (2KB aligned, stored in bits 0-4)
    uint16_t mapAddr = m_bgMapAddr[bgIndex] + (wrappedTileY * tilemapWidth + wrappedTileX) * 2;
    
    // VRAM boundary check
    if (mapAddr + 1 >= m_vram.size()) {
        // Debug: Log out of bounds access
        static int oobCount = 0;
        if (oobCount < 5) {
            std::cout << "PPU: renderBGx - mapAddr out of bounds: 0x" << std::hex << mapAddr 
                      << " (BG" << bgIndex << ", tileX=" << std::dec << tileX 
                      << ", tileY=" << tileY << ")" << std::endl;
            oobCount++;
        }
        return {0, 0};
    }
    
    // 2. Read tilemap entry
    uint16_t tileEntry = m_vram[mapAddr] | (m_vram[mapAddr + 1] << 8);
    
    // Detailed logging for text line
    if (shouldLog && logCount < MAX_LOG_PIXELS && (tileEntry & 0x03FF) != 0x0000) {
        std::ostringstream oss;
        int screenX = is16x16 ? (tileX * 16 + pixelX) : (tileX * 8 + pixelX);
        oss << "[PIXEL_LOG] x=" << screenX << " (tileX=" << tileX << ", pixelX=" << pixelX 
            << ", tileSize=" << physicalTileSize << "), y=" << m_scanline << " (tileY=" << tileY << ", pixelY=" << pixelY << ")" << std::endl;
        oss << "  [STEP1] Scroll: scrollX=" << scrollX << ", scrollY=" << scrollY 
            << " (raw: m_bg1ScrollX=0x" << std::hex << m_bg1ScrollX << ", m_bg1ScrollY=0x" << m_bg1ScrollY << std::dec << ")" << std::endl;
        oss << "  [STEP2] Tilemap: mapAddr=0x" << std::hex << mapAddr 
            << ", tileEntry=0x" << tileEntry << std::dec
            << " (wrappedTileX=" << wrappedTileX << ", wrappedTileY=" << wrappedTileY << ")" << std::endl;
        Logger::getInstance().logPPU(oss.str());
    }
    
    // 3. Extract attributes
    uint16_t tileNumber = tileEntry & 0x03FF;
    // Palette bits: 2bpp uses bit 10 only (1 bit), 4bpp uses bits 10-12 (3 bits)
    uint8_t palette = (tileEntry >> 10) & ((bpp == 4) ? 0x07 : 0x01);
    bool hFlip = (tileEntry >> 14) & 1;
    bool vFlip = (tileEntry >> 15) & 1;
    uint8_t priorityGroup = (tileEntry >> 13) & 1;
    
    // Detailed logging for text line
    if (shouldLog && logCount < MAX_LOG_PIXELS && (tileEntry & 0x03FF) != 0x0000) {
        std::ostringstream oss;
        oss << "  [STEP3] Attributes: tileNumber=" << tileNumber 
            << ", palette=" << (int)palette << ", hFlip=" << hFlip 
            << ", vFlip=" << vFlip << ", priority=" << (int)priorityGroup
            << ", is16x16=" << is16x16 << std::endl;
        Logger::getInstance().logPPU(oss.str());
    } 
    
    // Fine scrolling is now handled in renderBackgroundMode0 before calling renderBGx
    // So pixelX and pixelY here are already fine-scrolled and tileX/Y are adjusted
    // We just use them directly (no additional fine scrolling needed)
    int scrolledPixelX = pixelX;
    int scrolledPixelY = pixelY;
    
    // Detailed logging for text line
    if (shouldLog && logCount < MAX_LOG_PIXELS && (tileEntry & 0x03FF) != 0x0000) {
        std::ostringstream oss;
        oss << "  [STEP4] Fine scroll: Already applied in renderBackgroundMode0" << std::endl;
        oss << "  [STEP5] Scrolled pixel: scrolledPixelX=" << scrolledPixelX 
            << ", scrolledPixelY=" << scrolledPixelY << std::endl;
        Logger::getInstance().logPPU(oss.str());
    }
    
    // 4. Handle 16x16 tiles: Convert to 8x8 sub-tile coordinates
    uint16_t subTileNumber = tileNumber;
    int subPixelX = scrolledPixelX;
    int subPixelY = scrolledPixelY;
    
    if (is16x16) {
        // 16x16 tile is composed of 4 8x8 sub-tiles:
        // ┌─────┬─────┐
        // │  0  │  1  │  tileNumber, tileNumber+1
        // ├─────┼─────┤
        // │  2  │  3  │  tileNumber+16, tileNumber+17
        // └─────┴─────┘
        
        // Determine which sub-tile (0-3) we're in
        int subTileX = scrolledPixelX / 8;
        int subTileY = scrolledPixelY / 8;
        subPixelX = scrolledPixelX % 8;
        subPixelY = scrolledPixelY % 8;
        
        // Calculate sub-tile number
        // For 16x16 tiles, sub-tiles are arranged in VRAM:
        // - Sub-tile 0: tileNumber (left-top)
        // - Sub-tile 1: tileNumber + 1 (right-top)
        // - Sub-tile 2: tileNumber + 16 (left-bottom) - Note: This is VRAM layout, not tilemap
        // - Sub-tile 3: tileNumber + 17 (right-bottom)
        // However, SNES actually uses tilemap width for sub-tile 2/3:
        // - Sub-tile 2: tileNumber + (tilemap width in tiles)
        // - Sub-tile 3: tileNumber + (tilemap width in tiles) + 1
        
        // Apply flip BEFORE calculating sub-tile position
        if (hFlip) {
            subTileX = 1 - subTileX;  // Flip horizontally within 16x16 tile
            subPixelX = 7 - subPixelX;
        }
        if (vFlip) {
            subTileY = 1 - subTileY;  // Flip vertically within 16x16 tile
            subPixelY = 7 - subPixelY;
        }
        
        // Calculate sub-tile number based on position
        if (subTileX == 0 && subTileY == 0) {
            // Sub-tile 0 (left-top): use original tileNumber
            subTileNumber = tileNumber;
        } else if (subTileX == 1 && subTileY == 0) {
            // Sub-tile 1 (right-top): tileNumber + 1
            subTileNumber = tileNumber + 1;
        } else if (subTileX == 0 && subTileY == 1) {
            // Sub-tile 2 (left-bottom): tileNumber + tilemap width
            subTileNumber = tileNumber + tilemapWidth;
        } else { // subTileX == 1 && subTileY == 1
            // Sub-tile 3 (right-bottom): tileNumber + tilemap width + 1
            subTileNumber = tileNumber + tilemapWidth + 1;
        }
        
        // After calculating sub-tile, apply flip to pixel coordinates within 8x8 sub-tile
        // (We already applied flip to sub-tile position above, now flip the pixel)
        // Actually, we already flipped subPixelX/Y above, so we're good
    } else {
        // 8x8 tile: Apply flip directly
        if (hFlip) {
            subPixelX = 7 - scrolledPixelX;
        }
        if (vFlip) {
            subPixelY = 7 - scrolledPixelY;
        }
    }
    
    // Detailed logging for text line
    if (shouldLog && logCount < MAX_LOG_PIXELS && (tileEntry & 0x03FF) != 0x0000) {
        std::ostringstream oss;
        oss << "  [STEP6] After 16x16 handling: subTileNumber=" << subTileNumber
            << ", subPixelX=" << subPixelX << ", subPixelY=" << subPixelY
            << " (is16x16=" << is16x16 << ", hFlip=" << hFlip << ", vFlip=" << vFlip << ")" << std::endl;
        Logger::getInstance().logPPU(oss.str());
    }
    
    // 5. Calculate tile data address (for the 8x8 sub-tile)
    uint16_t tileAddr = m_bgTileAddr[bgIndex] + subTileNumber * TILE_SIZE_BYTES;
    
    // Detailed logging for text line
    if (shouldLog && logCount < MAX_LOG_PIXELS && (tileEntry & 0x03FF) != 0x0000) {
        std::ostringstream oss;
        oss << "  [STEP7] Tile data: tileAddr=0x" << std::hex << tileAddr 
            << " (m_bgTileAddr[0]=0x" << m_bgTileAddr[bgIndex] 
            << ", tileNumber=" << std::dec << tileNumber 
            << ", TILE_SIZE=" << TILE_SIZE_BYTES << ")" << std::endl;
        Logger::getInstance().logPPU(oss.str());
    }
    
    // VRAM boundary check
    if (tileAddr + TILE_SIZE_BYTES > m_vram.size()) {
        // Debug: Log out of bounds access
        static int oobTileCount = 0;
        if (oobTileCount < 5) {
            std::cout << "PPU: renderBGx - tileAddr out of bounds: 0x" << std::hex << tileAddr 
                      << " (BG" << bgIndex << ", tileNumber=" << std::dec << tileNumber 
                      << ", bpp=" << bpp << ")" << std::endl;
            oobTileCount++;
        }
        return {0, 0};
    }
    
    // 6. Decode tile data (2bpp or 4bpp)
    // Use sub-tile pixel coordinates (always 8x8 sub-tile, even for 16x16 tiles)
    uint8_t pixelIndex = 0;
    int line_offset = subPixelY*2;  // Always 0-7 for 8x8 sub-tile
    int bitPos = 7 - subPixelX;  // Always 0-7 for 8x8 sub-tile
    
    // Debug: Log first few tile reads
    static int tileReadCount = 0;
    if (tileReadCount < 20 && bgIndex == 0 && tileNumber < 200) {
        std::cout << "PPU: renderBGx - BG" << bgIndex << " Tile " << tileNumber 
                  << " @ 0x" << std::hex << tileAddr << " (bpp=" << std::dec << bpp 
                  << ", pixelX=" << pixelX << ", pixelY=" << pixelY << ")" << std::endl;
        tileReadCount++;
    }
    
    if (bpp == 2) {
        // 2bpp: 2 bitplanes, 16 bytes per tile
        uint8_t plane0_byte = m_vram[tileAddr + line_offset];
        uint8_t plane1_byte = m_vram[tileAddr + line_offset + 1];
        
        // Detailed logging for text line
        if (shouldLog && logCount < MAX_LOG_PIXELS && (tileEntry & 0x03FF) != 0x0000) {
            std::ostringstream oss;
            oss << "  [STEP8] Tile decode: line_offset=" << line_offset 
                << ", bitPos=" << bitPos << std::endl;
            oss << "    plane0_byte=0x" << std::hex << (int)plane0_byte 
                << " (VRAM[0x" << (tileAddr + line_offset) << "])" << std::dec << std::endl;
            oss << "    plane1_byte=0x" << std::hex << (int)plane1_byte 
                << " (VRAM[0x" << (tileAddr + line_offset + 8) << "])" << std::dec << std::endl;
            Logger::getInstance().logPPU(oss.str());
        }
        
        pixelIndex = ((plane0_byte >> bitPos) & 1) | 
                     (((plane1_byte >> bitPos) & 1) << 1);
        
        // Detailed logging for text line
        if (shouldLog && logCount < MAX_LOG_PIXELS && (tileEntry & 0x03FF) != 0x0000) {
            std::ostringstream oss;
            oss << "  [STEP9] Pixel index: pixelIndex=" << (int)pixelIndex 
                << " (plane0_bit=" << ((plane0_byte >> bitPos) & 1) 
                << ", plane1_bit=" << ((plane1_byte >> bitPos) & 1) << ")" << std::endl;
            Logger::getInstance().logPPU(oss.str());
        }
    } else {
        // 4bpp: 4 bitplanes, 32 bytes per tile
        // Plane 0: bytes 0-7
        // Plane 1: bytes 8-15
        // Plane 2: bytes 16-23
        // Plane 3: bytes 24-31
        uint8_t plane0_byte = m_vram[tileAddr + line_offset];
        uint8_t plane1_byte = m_vram[tileAddr + line_offset + 8];
        uint8_t plane2_byte = m_vram[tileAddr + line_offset + 16];
        uint8_t plane3_byte = m_vram[tileAddr + line_offset + 24];
        
        pixelIndex = ((plane0_byte >> bitPos) & 1) |
                     (((plane1_byte >> bitPos) & 1) << 1) |
                     (((plane2_byte >> bitPos) & 1) << 2) |
                     (((plane3_byte >> bitPos) & 1) << 3);
    }
    
    // 7. Determine transparency and final color/priority
    if (pixelIndex == 0) {
        return {0, 0}; // Transparent (background color)
    }
    
    uint32_t color = getColor(palette, pixelIndex, bpp);
    uint8_t priority = m_bgPriority[bgIndex][priorityGroup];
    
    // Detailed logging for text line
    if (shouldLog && logCount < MAX_LOG_PIXELS && (tileEntry & 0x03FF) != 0x0000) {
        std::ostringstream oss;
        oss << "  [STEP10] Color: palette=" << (int)palette 
            << ", pixelIndex=" << (int)pixelIndex << ", bpp=" << bpp << std::endl;
        oss << "    Final color=0x" << std::hex << color << std::dec << std::endl;
        oss << "    Priority=" << (int)priority << std::endl;
        oss << "---" << std::endl;
        Logger::getInstance().logPPU(oss.str());
        logCount++;
    }
    
    // Debug: Log successful pixel rendering
    static int pixelRenderCount = 0;
    if (pixelRenderCount < 30 && bgIndex == 0 && tileNumber < 200 && pixelIndex != 0) {
        std::cout << "PPU: renderBGx - BG" << bgIndex << " rendered pixel: tile=" << tileNumber 
                  << ", palette=" << (int)palette << ", pixelIndex=" << (int)pixelIndex 
                  << ", color=0x" << std::hex << color << std::dec << std::endl;
        pixelRenderCount++;
    }

    return {color, priority};
}

// Main rendering function
uint32_t PPU::renderBackgroundMode0(int x) {
    int y = m_scanline; // Current scanline (m_scanline is a PPU member variable)
    
    // Debug: Log first few calls
    static int mode0CallCount = 0;
    if (mode0CallCount < 10 && x < 5 && y < 5) {
        std::cout << "PPU: renderBackgroundMode0 - x=" << x << ", y=" << y 
                  << ", bg1TileAddr=0x" << std::hex << m_bgTileAddr[0] 
                  << ", bg1MapAddr=0x" << m_bgMapAddr[0] << std::dec << std::endl;
        mode0CallCount++;
    }
    
    // Store pixel information for 4 layers
    PixelInfo bgPixels[4]; // BG1, BG2, BG3, BG4

    // 1. Calculate pixel information for each background layer
    // Each BG has its own scroll values
    // Mode 0: All BGs are 2bpp
    // BG1 (Index 0)
    {
        // BG scroll registers are 10-bit signed (-512 to 511)
        // Values >= 512 are treated as negative
        int scrollX = (m_bg1ScrollX & 0x03FF);
        int scrollY = (m_bg1ScrollY & 0x03FF);
        if (scrollX >= 512) scrollX -= 1024;
        if (scrollY >= 512) scrollY -= 1024;
        int bgX = x + scrollX;
        int bgY = y + scrollY;
        
        // Handle negative coordinates (when scrolling up/left)
        // Calculate tile and pixel coordinates
        int tileX = bgX / 8;
        int tileY = bgY / 8;
        int pixelX = bgX % 8;
        int pixelY = bgY % 8;
        
        // Fix negative modulo (C++ modulo can be negative)
        if (pixelX < 0) {
            pixelX += 8;
            tileX -= 1;  // Adjust tileX when pixelX wraps
        }
        if (pixelY < 0) {
            pixelY += 8;
            tileY -= 1;  // Adjust tileY when pixelY wraps
        }
        
        // Fine scrolling: The scroll value has already been applied to bgX/bgY
        // Fine scrolling only affects sub-pixel positioning within the current tile
        // For scrollY=-1 (coarse scroll), we already have tileY=0, pixelY=0
        // Fine scrolling should NOT change tileY when scroll is already an integer pixel value
        // 
        // Actually, SNES fine scrolling works differently:
        // - Fine scroll is the sub-8-pixel offset within a tile
        // - For scrollY=-1, the fine part is -1, which means we show row 7 of previous tile row
        // - But bgY calculation already handles this by making bgY=0
        // - So we should NOT apply fine scrolling again here
        //
        // For now, skip fine scrolling since bgX/bgY already include the scroll offset
        // The tileX/Y and pixelX/Y calculated from bgX/bgY are already correct
        bgPixels[0] = renderBGx(0, tileX, tileY, pixelX, pixelY, 2);
    }
    
    // BG2 (Index 1)
    {
        // BG scroll registers are 10-bit signed (-512 to 511)
        // Values >= 512 are treated as negative
        int scrollX = (m_bg2ScrollX & 0x03FF);
        int scrollY = (m_bg2ScrollY & 0x03FF);
        if (scrollX >= 512) scrollX -= 1024;
        if (scrollY >= 512) scrollY -= 1024;
        int bgX = x + scrollX;
        int bgY = y + scrollY;
        
        // Handle negative coordinates (when scrolling up/left)
        bool is16x16 = m_bgTileSize[1];
        int tileSize = is16x16 ? 16 : 8;
        int tileX = bgX / tileSize;
        int tileY = bgY / tileSize;
        int pixelX = bgX % tileSize;
        int pixelY = bgY % tileSize;
        
        // Fix negative modulo (C++ modulo can be negative)
        if (pixelX < 0) {
            pixelX += tileSize;
            tileX -= 1;
        }
        if (pixelY < 0) {
            pixelY += tileSize;
            tileY -= 1;
        }
        bgPixels[1] = renderBGx(1, tileX, tileY, pixelX, pixelY, 2);
    }
    
    // BG3 (Index 2)
    {
        // BG scroll registers are 10-bit signed (-512 to 511)
        // Values >= 512 are treated as negative
        int scrollX = (m_bg3ScrollX & 0x03FF);
        int scrollY = (m_bg3ScrollY & 0x03FF);
        if (scrollX >= 512) scrollX -= 1024;
        if (scrollY >= 512) scrollY -= 1024;
        int bgX = x + scrollX;
        int bgY = y + scrollY;
        
        // Handle negative coordinates (when scrolling up/left)
        bool is16x16 = m_bgTileSize[2];
        int tileSize = is16x16 ? 16 : 8;
        int tileX = bgX / tileSize;
        int tileY = bgY / tileSize;
        int pixelX = bgX % tileSize;
        int pixelY = bgY % tileSize;
        
        // Fix negative modulo (C++ modulo can be negative)
        if (pixelX < 0) {
            pixelX += tileSize;
            tileX -= 1;
        }
        if (pixelY < 0) {
            pixelY += tileSize;
            tileY -= 1;
        }
        bgPixels[2] = renderBGx(2, tileX, tileY, pixelX, pixelY, 2);
    }
    
    // BG4 (Index 3)
    {
        // BG scroll registers are 10-bit signed (-512 to 511)
        // Values >= 512 are treated as negative
        int scrollX = (m_bg4ScrollX & 0x03FF);
        int scrollY = (m_bg4ScrollY & 0x03FF);
        if (scrollX >= 512) scrollX -= 1024;
        if (scrollY >= 512) scrollY -= 1024;
        int bgX = x + scrollX;
        int bgY = y + scrollY;
        bool is16x16 = m_bgTileSize[3];
        int tileSize = is16x16 ? 16 : 8;
        int tileX = bgX / tileSize;
        int tileY = bgY / tileSize;
        int pixelX = bgX % tileSize;
        int pixelY = bgY % tileSize;
        
        // Fix negative modulo (C++ modulo can be negative)
        if (pixelX < 0) {
            pixelX += tileSize;
            tileX -= 1;
        }
        if (pixelY < 0) {
            pixelY += tileSize;
            tileY -= 1;
        }
        bgPixels[3] = renderBGx(3, tileX, tileY, pixelX, pixelY, 2);
    }

    // 2. Check Main Screen Designation (TM register) - only render enabled BGs
    // Bit 0 = BG1, Bit 1 = BG2, Bit 2 = BG3, Bit 3 = BG4
    
    // Debug: Warn if Main Screen Designation is 0 (all BGs disabled)
    static bool warnedMainScreenZero = false;
    if (m_mainScreenDesignation == 0 && !warnedMainScreenZero && m_scanline == 0 && x == 0) {
        std::cout << "PPU WARNING: Main Screen Designation is 0 - all BGs are disabled!" << std::endl;
        warnedMainScreenZero = true;
    }
    
    // 3. Determine pixel priority (select pixel with highest priority)
    // Sprite rendering results should also be considered, but here we only handle backgrounds.
    
    uint32_t finalColor = 0;
    uint8_t maxPriority = 0;
    bool foundPixel = false;

    for (int i = 0; i < 4; ++i) {
        // Check if this BG is enabled in Main Screen Designation
        if (!(m_mainScreenDesignation & (1 << i))) {
            continue; // Skip disabled BG
        }
        
        if (bgPixels[i].color != 0) { // Only consider non-transparent pixels
            // Select pixel with highest priority (>= instead of > to handle priority 0)
            if (!foundPixel || bgPixels[i].priority >= maxPriority) {
                maxPriority = bgPixels[i].priority;
                finalColor = bgPixels[i].color;
                foundPixel = true;
            }
        }
    }

    // If no BG pixel was found, use background color (CGRAM[0])
    if (finalColor == 0) {
        uint16_t bgColor = m_cgram[0] | (m_cgram[1] << 8);
        // Always use black background
        if (bgColor == 0) {
            return 0x000000FF; // Black (0xRRGGBBAA format)
        }
        // Extract RGB components (5 bits each)
        uint8_t r = (bgColor & 0x1F) << 3;
        uint8_t g = ((bgColor >> 5) & 0x1F) << 3;
        uint8_t b = ((bgColor >> 10) & 0x1F) << 3;
        // Convert to 32-bit RGBA (0xRRGGBBAA format)
        return (r << 24) | (g << 16) | (b << 8) | 0xFF;
    }

    // Return final color. (Background color before Sprite or Color Math is applied)
    return finalColor;
}

uint32_t PPU::renderBackgroundMode1(int x) {
    int y = m_scanline;
    
    // Mode 1: BG1/BG2=4bpp, BG3=2bpp
    // Apply scrolling to screen coordinates
    int bg1X = x + m_bg1ScrollX;
    int bg1Y = y + m_bg1ScrollY;
    int bg2X = x + m_bg2ScrollX;
    int bg2Y = y + m_bg2ScrollY;
    int bg3X = x + m_bg3ScrollX;
    int bg3Y = y + m_bg3ScrollY;
    
    // Calculate tile and pixel coordinates for each BG
    int bg1TileX = bg1X / 8, bg1TileY = bg1Y / 8, bg1PixelX = bg1X % 8, bg1PixelY = bg1Y % 8;
    int bg2TileX = bg2X / 8, bg2TileY = bg2Y / 8, bg2PixelX = bg2X % 8, bg2PixelY = bg2Y % 8;
    int bg3TileX = bg3X / 8, bg3TileY = bg3Y / 8, bg3PixelX = bg3X % 8, bg3PixelY = bg3Y % 8;
    
    // Store pixel information for 3 layers
    PixelInfo bgPixels[3]; // BG1, BG2, BG3
    
    // BG1 (4bpp)
    bgPixels[0] = renderBGx(0, bg1TileX, bg1TileY, bg1PixelX, bg1PixelY, 4);
    
    // BG2 (4bpp)
    bgPixels[1] = renderBGx(1, bg2TileX, bg2TileY, bg2PixelX, bg2PixelY, 4);
    
    // BG3 (2bpp)
    bgPixels[2] = renderBGx(2, bg3TileX, bg3TileY, bg3PixelX, bg3PixelY, 2);
    
    // Determine pixel priority (select pixel with highest priority)
    uint32_t finalColor = 0;
    uint8_t maxPriority = 0;
    bool foundPixel = false;
    
    for (int i = 0; i < 3; ++i) {
        // Check if this BG is enabled in Main Screen Designation
        if (!(m_mainScreenDesignation & (1 << i))) {
            continue; // Skip disabled BG
        }
        
        if (bgPixels[i].color != 0) { // Only consider non-transparent pixels
            // Select pixel with highest priority (>= instead of > to handle priority 0)
            if (!foundPixel || bgPixels[i].priority >= maxPriority) {
                maxPriority = bgPixels[i].priority;
                finalColor = bgPixels[i].color;
                foundPixel = true;
            }
        }
    }
    
    // If no BG pixel was found, use background color (CGRAM[0])
    if (finalColor == 0) {
        uint16_t bgColor = m_cgram[0] | (m_cgram[1] << 8);
        if (bgColor == 0) {
            return 0x000000FF; // Default to black (0xRRGGBBAA format)
        }
        uint8_t r = (bgColor & 0x1F) << 3;
        uint8_t g = ((bgColor >> 5) & 0x1F) << 3;
        uint8_t b = ((bgColor >> 10) & 0x1F) << 3;
        return (r << 24) | (g << 16) | (b << 8) | 0xFF; // 0xRRGGBBAA format
    }
    
    return finalColor;
}

uint32_t PPU::renderBackgroundMode6(int x) {
    int y = m_scanline;
    
    // Mode 6: BG1 4bpp hires with offset per tile from BG3 tilemap
    // This is a special mode where BG1 uses 4bpp tiles in hires mode (512 pixels wide)
    // and BG3 tilemap provides per-tile offset values
    
    // In hires mode, screen is effectively 512 pixels wide, but we render at 256
    // Each pixel in hires mode corresponds to 2 pixels in normal mode
    // For simplicity, we'll render at normal width but use hires tile addressing
    
    // Calculate BG1 coordinates (hires mode - effectively 512 pixels wide)
    int bg1X = (x * 2) + m_bg1ScrollX;  // Double width for hires
    int bg1Y = y + m_bg1ScrollY;
    
    // Calculate tile coordinates (support 16x16 tiles)
    bool bg1Is16x16 = m_bgTileSize[0];
    int bg1TileSize = bg1Is16x16 ? 16 : 8;
    int bg1TileX = bg1X / bg1TileSize;
    int bg1TileY = bg1Y / bg1TileSize;
    int bg1PixelX = bg1X % bg1TileSize;
    int bg1PixelY = bg1Y % bg1TileSize;
    
    // Fix negative modulo
    if (bg1PixelX < 0) { bg1PixelX += bg1TileSize; bg1TileX -= 1; }
    if (bg1PixelY < 0) { bg1PixelY += bg1TileSize; bg1TileY -= 1; }
    
    // Read offset from BG3 tilemap (BG3 acts as offset table)
    int bg3TileX = bg1TileX;
    int bg3TileY = bg1TileY;
    
    // Handle tilemap wrapping for BG3
    int bg3TilemapWidth = m_bgMapSize[2] ? 64 : 32;
    int wrappedBG3TileX = bg3TileX & (bg3TilemapWidth - 1);
    int wrappedBG3TileY = bg3TileY & (bg3TilemapWidth - 1);
    
    uint16_t bg3MapAddr = m_bgMapAddr[2] + (wrappedBG3TileY * bg3TilemapWidth + wrappedBG3TileX) * 2;
    
    if (bg3MapAddr + 1 >= m_vram.size()) {
        // If BG3 tilemap is invalid, render BG1 normally
        PixelInfo bg1Pixel = renderBGx(0, bg1TileX, bg1TileY, bg1PixelX, bg1PixelY, 4);
        if (bg1Pixel.color != 0 && (m_mainScreenDesignation & 0x01)) {
            return bg1Pixel.color;
        }
        // Return background color
        uint16_t bgColor = m_cgram[0] | (m_cgram[1] << 8);
        if (bgColor == 0) {
            return 0x000000FF; // Black (0xRRGGBBAA format)
        }
        uint8_t r = (bgColor & 0x1F) << 3;
        uint8_t g = ((bgColor >> 5) & 0x1F) << 3;
        uint8_t b = ((bgColor >> 10) & 0x1F) << 3;
        return (r << 24) | (g << 16) | (b << 8) | 0xFF; // 0xRRGGBBAA format
    }
    
    // Read offset from BG3 tilemap entry
    uint16_t bg3TileEntry = m_vram[bg3MapAddr] | (m_vram[bg3MapAddr + 1] << 8);
    // In Mode 6, BG3 tilemap entry provides offset
    // Lower 9 bits: X offset (signed)
    // Upper 7 bits: Y offset (signed, but only 7 bits used)
    int16_t offsetX = (int16_t)((bg3TileEntry & 0x01FF) << 7) >> 7;  // Sign extend 9-bit
    // Y offset is in bits 9-15, but only 7 bits, so we need to sign extend from bit 8
    int16_t offsetY = (int16_t)(((bg3TileEntry >> 9) & 0x7F) << 9) >> 9;  // Sign extend 7-bit
    
    // Apply offset to BG1 coordinates
    int offsetBG1X = bg1X + offsetX;
    int offsetBG1Y = bg1Y + offsetY;
    
    int offsetBG1TileX = offsetBG1X / 8;
    int offsetBG1TileY = offsetBG1Y / 8;
    int offsetBG1PixelX = offsetBG1X % 8;
    int offsetBG1PixelY = offsetBG1Y % 8;
    
    // Render BG1 with offset
    PixelInfo bg1Pixel = renderBGx(0, offsetBG1TileX, offsetBG1TileY, offsetBG1PixelX, offsetBG1PixelY, 4);
    
    if (bg1Pixel.color != 0 && (m_mainScreenDesignation & 0x01)) {
        return bg1Pixel.color;
    }
    
    // Return background color
    uint16_t bgColor = m_cgram[0] | (m_cgram[1] << 8);
    if (bgColor == 0) {
        return 0x000000FF; // Black (0xRRGGBBAA format)
    }
    uint8_t r = (bgColor & 0x1F) << 3;
    uint8_t g = ((bgColor >> 5) & 0x1F) << 3;
    uint8_t b = ((bgColor >> 10) & 0x1F) << 3;
    return (0xFF << 24) | (r << 16) | (g << 8) | b;
}

uint32_t PPU::renderTestPattern(int x) {
    // Simple test pattern with different colors
    if (m_scanline < 50) {
        return 0xFF0000FF; // Blue
    } else if (m_scanline < 100) {
        return 0xFF00FF00; // Green  
    } else if (m_scanline < 150) {
        return 0xFFFF0000; // Red
    } else {
        return 0xFFFFFF00; // Yellow
    }
}

uint32_t PPU::getColor(uint8_t paletteIndex, uint8_t colorIndex) {
    return getColor(paletteIndex, colorIndex, 4); // Default to 4bpp
}

uint32_t PPU::getColor(uint8_t paletteIndex, uint8_t colorIndex, int bpp) {
    // SNES uses 15-bit color (5 bits per channel)
    // CGRAM stores colors as little-endian 16-bit values
    
    uint16_t cgramIndex;
    if (bpp == 2) {
        // 2bpp: Each palette has 4 colors, starting at paletteIndex * 4
        cgramIndex = (paletteIndex * 4 + colorIndex) * 2;
    } else {
        // 4bpp: Each palette has 16 colors, starting at paletteIndex * 16
        cgramIndex = (paletteIndex * 16 + colorIndex) * 2;
    }
    
    if (cgramIndex >= m_cgram.size()) {
        // Return test colors if CGRAM is not initialized
        if (paletteIndex == 0) return 0xFF0000FF; // Blue
        if (paletteIndex == 1) return 0xFF00FF00; // Green  
        if (paletteIndex == 2) return 0xFFFF0000; // Red
        return 0x000000FF; // Black (0xRRGGBBAA format) // Black
    }
    
    uint16_t snesColor = m_cgram[cgramIndex] | (m_cgram[cgramIndex + 1] << 8);
    
    // If CGRAM is empty (all zeros), use test colors
    if (snesColor == 0) {
        if (paletteIndex == 0) return 0xFF0000FF; // Blue
        if (paletteIndex == 1) return 0xFF00FF00; // Green
        if (paletteIndex == 2) return 0xFFFF0000; // Red
        if (paletteIndex == 6) return 0xFFFF00FF; // Magenta
        return 0x000000FF; // Black (0xRRGGBBAA format) // Black
    }
    
    // Extract RGB components (5 bits each)
    uint8_t r = (snesColor & 0x1F) << 3;
    uint8_t g = ((snesColor >> 5) & 0x1F) << 3;
    uint8_t b = ((snesColor >> 10) & 0x1F) << 3;
    
    // Convert to 32-bit RGBA 
    // SDL_PIXELFORMAT_RGBA8888 format: 0xRRGGBBAA
    return (r << 24) | (g << 16) | (b << 8) | 0xFF;
}

void PPU::decodeTile(const uint8_t* tileData, uint8_t output[64], int bpp) {
    // SNES tile format: 2bpp, 4bpp, or 8bpp
    // For now, implement 4bpp (16 colors)
    // 32 bytes per tile: 8 pairs of bitplanes
    
    // Simplified: always use 4bpp for now
    (void)bpp;  // Suppress unused parameter warning
    
    for (int y = 0; y < 8; y++) {
        uint8_t plane0 = tileData[y * 2];
        uint8_t plane1 = tileData[y * 2 + 1];
        uint8_t plane2 = tileData[16 + y * 2];
        uint8_t plane3 = tileData[16 + y * 2 + 1];
        
        for (int x = 0; x < 8; x++) {
            int bit = 7 - x;
            uint8_t pixel = 
                ((plane0 >> bit) & 1) |
                (((plane1 >> bit) & 1) << 1) |
                (((plane2 >> bit) & 1) << 2) |
                (((plane3 >> bit) & 1) << 3);
            output[y * 8 + x] = pixel;
        }
    }
}
uint32_t PPU::renderBG1(int x, int y) {
    // BG1 rendering for a single pixel
    int tileX = x / 8;
    int tileY = y / 8;
    int pixelX = x % 8;
    int pixelY = y % 8;
    
    // --- 1. VRAM Boundary Check for Tilemap Entry ---
    // Get tilemap entry from VRAM
    uint16_t mapAddr = m_bg1MapAddr + (tileY * 32 + tileX) * 2; // 2 bytes per tilemap entry
    
    if (mapAddr + 1 >= m_vram.size()) {
        std::cout << "DEBUG_RENDER: FAIL - Tilemap address out of bounds at (tileX=" << tileX << ", tileY=" << tileY << ")! mapAddr=0x" << std::hex << mapAddr << std::dec << std::endl;
        return 0;
    }
    
    // Read tilemap entry (16-bit)
    uint16_t tileEntry = m_vram[mapAddr] | (m_vram[mapAddr + 1] << 8);
    
    // Debug: Print first few tilemap entries (Existing Debug)
    static int tilemapDebugCount = 0;
    if (tilemapDebugCount < 1000 && tileX < 5 && tileY < 5) {
        std::cout << "Tilemap[" << tileX << "," << tileY << "] at 0x" << std::hex << mapAddr 
                  << " = 0x" << tileEntry << std::dec << std::endl;
        tilemapDebugCount++;
    }
    
    uint16_t tileNumber = tileEntry & 0x03FF;
    uint8_t palette = (tileEntry >> 10) & 0x07;
    bool hFlip = (tileEntry & 0x4000) != 0;
    bool vFlip = (tileEntry & 0x8000) != 0;
    
    // Use byte address from m_bgTileAddr[0] instead of word address from m_bg1TileAddr
    // m_bgTileAddr[0] is already converted to byte address (word * 2)
    // Mode 0 uses 2bpp tiles (16 bytes per tile), not 4bpp (32 bytes)
    const int TILE_SIZE_BYTES = 16; // 2bpp for Mode 0
    uint16_t tileAddr = m_bgTileAddr[0] + tileNumber * TILE_SIZE_BYTES;
    
    if (tileAddr + TILE_SIZE_BYTES > m_vram.size()) {
        std::cout << "DEBUG_RENDER: FAIL - Tile data address out of bounds for tile number " << tileNumber << "! tileAddr=0x" << std::hex << tileAddr << std::dec << std::endl;
        return 0;
    }
    
    // Decode tile (2bpp for Mode 0)
    // 2bpp format: Plane 0 = bytes 0-7, Plane 1 = bytes 8-15
    // Apply flipping first
    int finalX = hFlip ? (7 - pixelX) : pixelX;
    int finalY = vFlip ? (7 - pixelY) : pixelY;
    
    // Calculate pixel index from tile data
    int line_offset = finalY;
    int bitPos = 7 - finalX;
    
    uint8_t plane0_byte = m_vram[tileAddr + line_offset];
    uint8_t plane1_byte = m_vram[tileAddr + line_offset + 8];
    
    uint8_t pixelIndex = ((plane0_byte >> bitPos) & 1) | 
                         (((plane1_byte >> bitPos) & 1) << 1);

    // --- 3. Transparency Check and Successful Render Log ---
    if (pixelIndex != 0) { // Not transparent
        // Successful rendering condition met
        std::cout << "DEBUG_RENDER: SUCCESS - Rendered pixel at (x=" << x << ", y=" << y << ") | Tile: " << tileNumber
                  << ", Palette: " << (int)palette << ", Index: " << (int)pixelIndex << std::endl;
        return getColor(palette, pixelIndex, 2); // 2bpp for Mode 0
    }
    
    // Log for transparent pixel
    // Note: This log can be very noisy, so it's commented out by default.
    // std::cout << "DEBUG_RENDER: SKIP - Pixel transparent at (x=" << x << ", y=" << y << ")" << std::endl;
    
    return 0; // Transparent
}

uint32_t PPU::renderBG2(int x, int y) {
    // BG2 rendering for a single pixel
    int tileX = x / 8;
    int tileY = y / 8;
    int pixelX = x % 8;
    int pixelY = y % 8;
    
    // Get tilemap entry from VRAM
    uint16_t mapAddr = m_bg2MapAddr + (tileY * 32 + tileX) * 2; // 2 bytes per tilemap entry
    
    if (mapAddr + 1 >= m_vram.size()) return 0;
    
    // Read tilemap entry (16-bit)
    uint16_t tileEntry = m_vram[mapAddr] | (m_vram[mapAddr + 1] << 8);
    
    // Extract tile number and attributes
    uint16_t tileNumber = tileEntry & 0x03FF;  // 10 bits for tile number
    uint8_t palette = (tileEntry >> 10) & 0x07; // 3 bits for palette
    bool hFlip = (tileEntry & 0x4000) != 0;    // Horizontal flip
    bool vFlip = (tileEntry & 0x8000) != 0;    // Vertical flip
    
    // Calculate tile data address
    uint16_t tileAddr = m_bg2TileAddr + tileNumber * 32; // 32 bytes per tile
    
    if (tileAddr + 32 > m_vram.size()) return 0;
    
    // Decode tile
    uint8_t tileData[32];
    for (int i = 0; i < 32; i++) {
        tileData[i] = m_vram[tileAddr + i];
    }
    
    uint8_t pixels[64];
    decodeTile(tileData, pixels, 4);
    
    // Apply flipping
    int finalX = hFlip ? (7 - pixelX) : pixelX;
    int finalY = vFlip ? (7 - pixelY) : pixelY;
    
    uint8_t pixelIndex = pixels[finalY * 8 + finalX];
    if (pixelIndex != 0) { // Not transparent
        return getColor(palette, pixelIndex);
    }
    
    return 0; // Transparent
}

uint32_t PPU::renderBG3(int x, int y) {
    // BG3 rendering for a single pixel
    int tileX = x / 8;
    int tileY = y / 8;
    int pixelX = x % 8;
    int pixelY = y % 8;
    
    // Get tilemap entry from VRAM
    uint16_t mapAddr = m_bg3MapAddr + (tileY * 32 + tileX) * 2; // 2 bytes per tilemap entry
    
    if (mapAddr + 1 >= m_vram.size()) return 0;
    
    // Read tilemap entry (16-bit)
    uint16_t tileEntry = m_vram[mapAddr] | (m_vram[mapAddr + 1] << 8);
    
    // Extract tile number and attributes
    uint16_t tileNumber = tileEntry & 0x03FF;  // 10 bits for tile number
    uint8_t palette = (tileEntry >> 10) & 0x01; // 1 bit for palette (2bpp)
    bool hFlip = (tileEntry & 0x4000) != 0;    // Horizontal flip
    bool vFlip = (tileEntry & 0x8000) != 0;    // Vertical flip
    
    // Calculate tile data address (2bpp = 16 bytes per tile)
    uint16_t tileAddr = m_bg3TileAddr + tileNumber * 16;
    
    if (tileAddr + 16 > m_vram.size()) return 0;
    
    // Decode 2bpp tile
    uint8_t pixels[64];
    for (int py = 0; py < 8; py++) {
        uint8_t plane0 = m_vram[tileAddr + py];
        uint8_t plane1 = m_vram[tileAddr + py + 8];
        
        for (int px = 0; px < 8; px++) {
            int bit = 7 - px;
            uint8_t pixel = ((plane0 >> bit) & 1) | (((plane1 >> bit) & 1) << 1);
            pixels[py * 8 + px] = pixel;
        }
    }
    
    // Apply flipping
    int finalX = hFlip ? (7 - pixelX) : pixelX;
    int finalY = vFlip ? (7 - pixelY) : pixelY;
    
    uint8_t pixelIndex = pixels[finalY * 8 + finalX];
    if (pixelIndex != 0) { // Not transparent
        return getColor(palette, pixelIndex, 2); // 2bpp for BG3
    }
    
    return 0; // Transparent
}

uint32_t PPU::renderBG4(int x, int y) {
    // BG4 rendering for a single pixel
    int tileX = x / 8;
    int tileY = y / 8;
    int pixelX = x % 8;
    int pixelY = y % 8;
    
    // Get tilemap entry from VRAM
    uint16_t mapAddr = m_bg4MapAddr + (tileY * 32 + tileX) * 2; // 2 bytes per tilemap entry
    
    if (mapAddr + 1 >= m_vram.size()) return 0;
    
    // Read tilemap entry (16-bit)
    uint16_t tileEntry = m_vram[mapAddr] | (m_vram[mapAddr + 1] << 8);
    
    // Extract tile number and attributes
    uint16_t tileNumber = tileEntry & 0x03FF;  // 10 bits for tile number
    uint8_t palette = (tileEntry >> 10) & 0x01; // 1 bit for palette (2bpp)
    bool hFlip = (tileEntry & 0x4000) != 0;    // Horizontal flip
    bool vFlip = (tileEntry & 0x8000) != 0;    // Vertical flip
    
    // Calculate tile data address (2bpp = 16 bytes per tile)
    uint16_t tileAddr = m_bg4TileAddr + tileNumber * 16;
    
    if (tileAddr + 16 > m_vram.size()) return 0;
    
    // Decode 2bpp tile
    uint8_t pixels[64];
    for (int py = 0; py < 8; py++) {
        uint8_t plane0 = m_vram[tileAddr + py];
        uint8_t plane1 = m_vram[tileAddr + py + 8];
        
        for (int px = 0; px < 8; px++) {
            int bit = 7 - px;
            uint8_t pixel = ((plane0 >> bit) & 1) | (((plane1 >> bit) & 1) << 1);
            pixels[py * 8 + px] = pixel;
        }
    }
    
    // Apply flipping
    int finalX = hFlip ? (7 - pixelX) : pixelX;
    int finalY = vFlip ? (7 - pixelY) : pixelY;
    
    uint8_t pixelIndex = pixels[finalY * 8 + finalX];
    if (pixelIndex != 0) { // Not transparent
        return getColor(palette, pixelIndex, 2); // 2bpp for BG4
    }
    
    return 0; // Transparent
}

PixelInfo PPU::renderBG3Pixel(int x, int y) {
    // BG3 rendering returning PixelInfo for Mode 1
    int tileX = x / 8;
    int tileY = y / 8;
    int pixelX = x % 8;
    int pixelY = y % 8;
    
    // Get tilemap entry from VRAM
    uint16_t mapAddr = m_bg3MapAddr + (tileY * 32 + tileX) * 2;
    
    if (mapAddr + 1 >= m_vram.size()) return {0, 0};
    
    // Read tilemap entry (16-bit)
    uint16_t tileEntry = m_vram[mapAddr] | (m_vram[mapAddr + 1] << 8);
    
    // Extract tile number and attributes
    uint16_t tileNumber = tileEntry & 0x03FF;
    uint8_t palette = (tileEntry >> 10) & 0x01; // 1 bit for palette (2bpp)
    bool hFlip = (tileEntry & 0x4000) != 0;
    bool vFlip = (tileEntry & 0x8000) != 0;
    uint8_t priorityGroup = (tileEntry >> 13) & 1;
    
    // Calculate tile data address (2bpp = 16 bytes per tile)
    uint16_t tileAddr = m_bg3TileAddr + tileNumber * 16;
    
    if (tileAddr + 16 > m_vram.size()) return {0, 0};
    
    // Decode 2bpp tile
    uint8_t pixels[64];
    for (int py = 0; py < 8; py++) {
        uint8_t plane0 = m_vram[tileAddr + py];
        uint8_t plane1 = m_vram[tileAddr + py + 8];
        
        for (int px = 0; px < 8; px++) {
            int bit = 7 - px;
            uint8_t pixel = ((plane0 >> bit) & 1) | (((plane1 >> bit) & 1) << 1);
            pixels[py * 8 + px] = pixel;
        }
    }
    
    // Apply flipping
    int finalX = hFlip ? (7 - pixelX) : pixelX;
    int finalY = vFlip ? (7 - pixelY) : pixelY;
    
    uint8_t pixelIndex = pixels[finalY * 8 + finalX];
    if (pixelIndex != 0) { // Not transparent
        uint32_t color = getColor(palette, pixelIndex, 2); // 2bpp for BG3
        uint8_t priority = m_bgPriority[2][priorityGroup];
        return {color, priority};
    }
    
    return {0, 0}; // Transparent
}

PixelInfo PPU::renderBG4Pixel(int x, int y) {
    // BG4 rendering returning PixelInfo for Mode 1
    int tileX = x / 8;
    int tileY = y / 8;
    int pixelX = x % 8;
    int pixelY = y % 8;
    
    // Get tilemap entry from VRAM
    uint16_t mapAddr = m_bg4MapAddr + (tileY * 32 + tileX) * 2;
    
    if (mapAddr + 1 >= m_vram.size()) return {0, 0};
    
    // Read tilemap entry (16-bit)
    uint16_t tileEntry = m_vram[mapAddr] | (m_vram[mapAddr + 1] << 8);
    
    // Extract tile number and attributes
    uint16_t tileNumber = tileEntry & 0x03FF;
    uint8_t palette = (tileEntry >> 10) & 0x01; // 1 bit for palette (2bpp)
    bool hFlip = (tileEntry & 0x4000) != 0;
    bool vFlip = (tileEntry & 0x8000) != 0;
    uint8_t priorityGroup = (tileEntry >> 13) & 1;
    
    // Calculate tile data address (2bpp = 16 bytes per tile)
    uint16_t tileAddr = m_bg4TileAddr + tileNumber * 16;
    
    if (tileAddr + 16 > m_vram.size()) return {0, 0};
    
    // Decode 2bpp tile
    uint8_t pixels[64];
    for (int py = 0; py < 8; py++) {
        uint8_t plane0 = m_vram[tileAddr + py];
        uint8_t plane1 = m_vram[tileAddr + py + 8];
        
        for (int px = 0; px < 8; px++) {
            int bit = 7 - px;
            uint8_t pixel = ((plane0 >> bit) & 1) | (((plane1 >> bit) & 1) << 1);
            pixels[py * 8 + px] = pixel;
        }
    }
    
    // Apply flipping
    int finalX = hFlip ? (7 - pixelX) : pixelX;
    int finalY = vFlip ? (7 - pixelY) : pixelY;
    
    uint8_t pixelIndex = pixels[finalY * 8 + finalX];
    if (pixelIndex != 0) { // Not transparent
        uint32_t color = getColor(palette, pixelIndex, 2); // 2bpp for BG4
        uint8_t priority = m_bgPriority[3][priorityGroup];
        return {color, priority};
    }
    
    return {0, 0}; // Transparent
}


void PPU::writeRegister(uint16_t address, uint8_t value) {
    // Log VRAM writes (0x2116-0x2119) for debugging
    if(!(address >= 0x2116 && address <= 0x2119)) {
    std::ostringstream oss;
    oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
        << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
        << "Scanline:" << std::setw(3) << m_scanline << " | "
        << "PPU Write: [$" << std::hex << (int)address << "] = $" << (int)value << std::dec << std::endl;
    Logger::getInstance().logPPU(oss.str());
        // Don't flush here - too frequent, causes performance issues
    }
    switch (address) {
        case 0x2100: { // INIDISP - Screen Display
            m_brightness = value & 0x0F;
            m_forcedBlank = (value & 0x80) != 0;
            static int displayChangeCount = 0;
            if (displayChangeCount < 1000) {
                
                std::ostringstream oss;
                oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                    << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                    << "PPU: INIDISP=$" << std::hex << (int)value << std::dec 
                          << " - Forced blank " << (m_forcedBlank ? "ON" : "OFF") 
                          << ", brightness=" << (int)m_brightness << std::endl;
                Logger::getInstance().logPPU(oss.str());
                Logger::getInstance().flush();
                displayChangeCount++;
            }
            

            break;
        }
            
        case 0x4200: { // NMITIMEN - Interrupt Enable
            m_nmiEnabled = (value & 0x80) != 0;
            static int nmiEnableCount = 0;
            if (nmiEnableCount < 300) {
                
                std::ostringstream oss;
                oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                    << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                    << "PPU: NMI " << (m_nmiEnabled ? "ENABLED" : "DISABLED") << std::endl;
                Logger::getInstance().logPPU(oss.str());
                Logger::getInstance().flush();  // Flush at frame end
                nmiEnableCount++;
            }
            break;
        }
            
        case 0x420B: { // MDMAEN - DMA Enable
            // DMA not implemented yet, just acknowledge the write
            static int dmaEnCount = 0;
            if (dmaEnCount < 300) {
                std::ostringstream oss;
                oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                    << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                    << "PPU: DMA Enable=$"  << std::hex << (int)value << std::dec << std::endl;
                Logger::getInstance().logPPU(oss.str());
                Logger::getInstance().flush();  // Flush at frame end
                dmaEnCount++;
            }
            break;
        }
            
        case 0x420C: { // HDMAEN - HDMA Enable
            // HDMA not implemented yet, just acknowledge the write
            static int hdmaEnCount = 0;
            if (hdmaEnCount < 300) {
                std::ostringstream oss;
                oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                    << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                    << "PPU: HDMA Enable=$" << std::hex << (int)value << std::dec << std::endl;
                Logger::getInstance().logPPU(oss.str());
                Logger::getInstance().flush();  // Flush at frame end
                hdmaEnCount++;
            }
            break;
        }
            
        case 0x2101: { // OBSEL - Object Size and Base Address
            m_objSize = value;
            static int obselCount = 0;
            if (obselCount < 300) {
                std::ostringstream oss;
                oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                    << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                    << "PPU: OBSEL=$" << std::hex << (int)value << std::dec << std::endl;
                Logger::getInstance().logPPU(oss.str());
                Logger::getInstance().flush();  // Flush at frame end
                obselCount++;
            }
            break;
        }
            
        case 0x2105: { // BGMODE - BG Mode and Character Size
            m_bgMode = value & 0x07;
            
            // Extract tile size settings (bits 3-6)
            // Bit 3: BG3 Tile Size (0=8x8, 1=16x16)
            // Bit 4: BG4 Tile Size (0=8x8, 1=16x16)
            // Bit 5: BG1 Tile Size (0=8x8, 1=16x16)
            // Bit 6: BG2 Tile Size (0=8x8, 1=16x16)
            m_bgTileSize[2] = (value & 0x08) != 0;  // BG3 (bit 3)
            m_bgTileSize[3] = (value & 0x10) != 0;  // BG4 (bit 4)
            m_bgTileSize[0] = (value & 0x20) != 0;  // BG1 (bit 5)
            m_bgTileSize[1] = (value & 0x40) != 0;  // BG2 (bit 6)
            
            // Update priority settings based on BG mode
            switch (m_bgMode) {
                case 0: // Mode 0: All BGs are 2bpp (4 colors)
                    m_bgPriority[0][0] = 0; m_bgPriority[0][1] = 3; // BG1
                    m_bgPriority[1][0] = 0; m_bgPriority[1][1] = 2; // BG2
                    m_bgPriority[2][0] = 0; m_bgPriority[2][1] = 1; // BG3
                    m_bgPriority[3][0] = 0; m_bgPriority[3][1] = 0; // BG4
                    break;
                case 1: // Mode 1: BG1/BG2=4bpp, BG3=2bpp
                    m_bgPriority[0][0] = 1; m_bgPriority[0][1] = 3; // BG1
                    m_bgPriority[1][0] = 0; m_bgPriority[1][1] = 2; // BG2
                    m_bgPriority[2][0] = 0; m_bgPriority[2][1] = 1; // BG3
                    m_bgPriority[3][0] = 0; m_bgPriority[3][1] = 0; // BG4 (not used)
                    break;
                default:
                    // Add other modes as needed
                    break;
            }
            
            static int bgModeCount = 0;
            if (bgModeCount < 10) {
                std::ostringstream oss;
                oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                    << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                    << "PPU: BGMODE=$" << std::hex << (int)value << std::dec 
                          << " - BG Mode=" << (int)m_bgMode << std::endl;
                Logger::getInstance().logPPU(oss.str());
                Logger::getInstance().flush();  // Flush at frame end
                bgModeCount++;
            }
            break;
        }
            
        case 0x2106: { // MOSAIC - Mosaic Size and Enable
            // Bits 0-3: Mosaic size (0 = disabled, 1-15 = size)
            // Bits 4-7: Mosaic enable for BG1-4
            m_mosaicSize = value & 0x0F;
            m_mosaicEnabled[0] = (value & 0x10) != 0; // BG1
            m_mosaicEnabled[1] = (value & 0x20) != 0; // BG2
            m_mosaicEnabled[2] = (value & 0x40) != 0; // BG3
            m_mosaicEnabled[3] = (value & 0x80) != 0; // BG4
            break;
        }
            
        case 0x2116: { // VMADDL - VRAM Address Low
            m_vramAddress = (m_vramAddress & 0xFF00) | value;
            break;
        }
            
        case 0x2117: { // VMADDH - VRAM Address High
            m_vramAddress = (m_vramAddress & 0x00FF) | (value << 8);
            static int vramAddrCount = 0;
            if (vramAddrCount < 3) {
                
                std::ostringstream oss;
                oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                    << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                    << "PPU: VRAM Address=0x" << std::hex << m_vramAddress << std::dec << std::endl;
                Logger::getInstance().logPPU(oss.str());
                Logger::getInstance().flush();  // Flush at frame end
                vramAddrCount++;
            }
            break;
        }
            
        case 0x2118: { // VMDATAL - VRAM Data Low
            writeVRAM(m_vramAddress * 2, value);  // Convert word address to byte address
            // Increment only if VMAIN bit 7 is 0 (increment after low byte)
            if ((m_vramReadBuffer & 0x80) == 0) {
                incrementVRAMAddress();
            }
            break;
        }
            
        case 0x2119: { // VMDATAH - VRAM Data High
            writeVRAM(m_vramAddress * 2 + 1, value);  // Convert word address to byte address
            // Increment only if VMAIN bit 7 is 1 (increment after high byte)
            if (m_vramReadBuffer & 0x80) {
                incrementVRAMAddress();
            }
            break;
        }
            
        case 0x211A: { // M7SEL - Mode 7 Settings
            // Mode 7 not fully implemented yet, but accept writes silently
            break;
        }
            
        case 0x211B: { // M7A - Mode 7 Matrix A (low byte)
            // Mode 7 not fully implemented yet, but accept writes silently
            break;
        }
            
        case 0x211C: { // M7B - Mode 7 Matrix B (low byte)
            // Mode 7 not fully implemented yet, but accept writes silently
            break;
        }
            
        case 0x211D: { // M7C - Mode 7 Matrix C (low byte)
            // Mode 7 not fully implemented yet, but accept writes silently
            break;
        }
            
        case 0x211E: { // M7D - Mode 7 Matrix D (low byte)
            // Mode 7 not fully implemented yet, but accept writes silently
            break;
        }
            
        case 0x211F: { // M7X - Mode 7 Center X (low byte)
            // Mode 7 not fully implemented yet, but accept writes silently
            break;
        }
            
        case 0x2120: { // M7Y - Mode 7 Center Y (low byte)
            // Mode 7 not fully implemented yet, but accept writes silently
            break;
        }
            
        case 0x2121: { // CGADD - CGRAM Address
            m_cgramAddress = value;
            static int cgaddCount = 0;
            if (cgaddCount < 3) {
                
                std::ostringstream oss;
                oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                    << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                    << "PPU: CGRAM Address=0x" << std::hex << (int)m_cgramAddress << std::dec << std::endl;
                Logger::getInstance().logPPU(oss.str());
                Logger::getInstance().flush();  // Flush at frame end
                cgaddCount++;
            }
            break;
        }
            
        case 0x2122: { // CGDATA - CGRAM Data
            writeCGRAM(m_cgramAddress, value);
            m_cgramAddress++;
            m_cgramAddress &= 0x01FF;
            if (m_cgramAddress <= 10) {
                std::ostringstream oss;
                oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                    << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                    << "CGRAM[" << std::dec << (m_cgramAddress - 1) 
                          << "] = 0x" << std::hex << (int)value << std::dec << std::endl;
                Logger::getInstance().logPPU(oss.str());
                Logger::getInstance().flush();  // Flush at frame end
            }
            break;
        }
            
        case 0x2107: { // BG1SC - BG1 Tilemap Address
            m_bg1MapAddr = (value & 0xFC) << 8;
            m_bgMapAddr[0] = m_bg1MapAddr; // Sync array
            m_bgMapSize[0] = (value & 0x80) != 0; // Bit 7: 0=32x32, 1=64x64
            static int bg1MapCount = 0;
            if (bg1MapCount < 3) {
                std::ostringstream oss;
                oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                    << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                    << "PPU: BG1SC=$" << std::hex << (int)value << std::dec 
                          << " - BG1 Map Addr=0x" << std::hex << m_bg1MapAddr 
                          << ", Size=" << (m_bgMapSize[0] ? "64x64" : "32x32") << std::dec << std::endl;
                Logger::getInstance().logPPU(oss.str());
                Logger::getInstance().flush();  // Flush at frame end
                bg1MapCount++;
            }
            break;
        }
            
        case 0x2108: { // BG2SC - BG2 Tilemap Address
            m_bg2MapAddr = (value & 0xFC) << 8;
            m_bgMapAddr[1] = m_bg2MapAddr; // Sync array
            m_bgMapSize[1] = (value & 0x80) != 0; // Bit 7: 0=32x32, 1=64x64
            break;
        }
            
        case 0x2109: { // BG3SC - BG3 Tilemap Address
            m_bg3MapAddr = (value & 0xFC) << 8;
            m_bgMapAddr[2] = m_bg3MapAddr; // Sync array
            m_bgMapSize[2] = (value & 0x80) != 0; // Bit 7: 0=32x32, 1=64x64
            break;
        }
            
        case 0x210A: { // BG4SC - BG4 Tilemap Address
            m_bg4MapAddr = (value & 0xFC) << 8;
            m_bgMapAddr[3] = m_bg4MapAddr; // Sync array
            m_bgMapSize[3] = (value & 0x80) != 0; // Bit 7: 0=32x32, 1=64x64
            break;
        }
            
        case 0x210B: { // BG12NBA - BG1 and BG2 Tile Data Address
            // BG12NBA stores WORD addresses, convert to byte addresses
            m_bg1TileAddr = (value & 0x0F) << 12;
            m_bg2TileAddr = ((value & 0xF0) >> 4) << 12;
            // Convert word addresses to byte addresses (word * 2 = byte)
            m_bgTileAddr[0] = m_bg1TileAddr * 2; // Sync array
            m_bgTileAddr[1] = m_bg2TileAddr * 2; // Sync array
            static int bg12nbaCount = 0;
            if (bg12nbaCount < 10) {
                
                std::ostringstream oss;
                oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                    << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                    << "PPU: BG12NBA=$" << std::hex << (int)value << std::dec 
                          << " - BG1 tiles at 0x" << std::hex << m_bg1TileAddr 
                          << ", BG2 tiles at 0x" << m_bg2TileAddr << std::dec << std::endl;
                Logger::getInstance().logPPU(oss.str());
                Logger::getInstance().flush();  // Flush at frame end
                bg12nbaCount++;
            }
            break;
        }
            
        case 0x210C: { // BG34NBA - BG3 and BG4 Tile Data Address
            // BG34NBA stores WORD addresses, convert to byte addresses
            m_bg3TileAddr = (value & 0x0F) << 12;
            m_bg4TileAddr = ((value & 0xF0) >> 4) << 12;
            // Convert word addresses to byte addresses (word * 2 = byte)
            m_bgTileAddr[2] = m_bg3TileAddr * 2; // Sync array
            m_bgTileAddr[3] = m_bg4TileAddr * 2; // Sync array
            break;
        }
            
        case 0x210D: { // BG1HOFS - BG1 Horizontal Scroll (Low)
            // SNES scroll registers: First write = low byte, second write = high byte
            // On first write, update low byte. On second write, update high byte and combine.
            if (m_scrollLatchX) {
                // Second write: update high byte
                m_bg1ScrollX = (m_bg1ScrollX & 0x00FF) | (value << 8);
            } else {
                // First write: update low byte (but store for next write)
                m_bg1ScrollX = (m_bg1ScrollX & 0xFF00) | value;
            }
            m_scrollPrevX = value;
            m_scrollLatchX = !m_scrollLatchX;
            static int bg1HofsCount = 0;
            if (bg1HofsCount < 3) {
                std::cout << "PPU: BG1HOFS=$" << std::hex << (int)value << ", ScrollX=0x" << m_bg1ScrollX << std::dec << std::endl;
                bg1HofsCount++;
            }
            break;
        }
            
        case 0x210E: { // BG1VOFS - BG1 Vertical Scroll (Low)
            // BG1VOFS is 10-bit (0-1023)
            if (m_scrollLatchY) {
                m_bg1ScrollY = (m_bg1ScrollY & 0x00FF) | ((value & 0x03) << 8);
            } else {
                m_bg1ScrollY = (m_bg1ScrollY & 0xFF00) | value;
            }
            m_bg1ScrollY &= 0x03FF;  // Mask to 10-bit
            m_scrollPrevY = value;
            m_scrollLatchY = !m_scrollLatchY;
            break;
        }
            
        case 0x210F: { // BG2HOFS - BG2 Horizontal Scroll (Low)
            if (m_scrollLatchX) {
                m_bg2ScrollX = (m_bg2ScrollX & 0x00FF) | (value << 8);
            } else {
                m_bg2ScrollX = (m_bg2ScrollX & 0xFF00) | value;
            }
            m_scrollPrevX = value;
            m_scrollLatchX = !m_scrollLatchX;
            break;
        }
            
        case 0x2110: { // BG2VOFS - BG2 Vertical Scroll (Low)
            if (m_scrollLatchY) {
                m_bg2ScrollY = (m_bg2ScrollY & 0x00FF) | (value << 8);
            } else {
                m_bg2ScrollY = (m_bg2ScrollY & 0xFF00) | value;
            }
            m_scrollPrevY = value;
            m_scrollLatchY = !m_scrollLatchY;
            break;
        }
            
        case 0x2111: { // BG3HOFS - BG3 Horizontal Scroll (Low)
            if (m_scrollLatchX) {
                m_bg3ScrollX = (m_bg3ScrollX & 0x00FF) | (value << 8);
            } else {
                m_bg3ScrollX = (m_bg3ScrollX & 0xFF00) | value;
            }
            m_scrollPrevX = value;
            m_scrollLatchX = !m_scrollLatchX;
            break;
        }
            
        case 0x2112: { // BG3VOFS - BG3 Vertical Scroll (Low)
            if (m_scrollLatchY) {
                m_bg3ScrollY = (m_bg3ScrollY & 0x00FF) | (value << 8);
            } else {
                m_bg3ScrollY = (m_bg3ScrollY & 0xFF00) | value;
            }
            m_scrollPrevY = value;
            m_scrollLatchY = !m_scrollLatchY;
            break;
        }
            
        case 0x2113: { // BG4HOFS - BG4 Horizontal Scroll (Low)
            if (m_scrollLatchX) {
                m_bg4ScrollX = (m_bg4ScrollX & 0x00FF) | (value << 8);
            } else {
                m_bg4ScrollX = (m_bg4ScrollX & 0xFF00) | value;
            }
            m_scrollPrevX = value;
            m_scrollLatchX = !m_scrollLatchX;
            break;
        }
            
        case 0x2114: { // BG4VOFS - BG4 Vertical Scroll (Low)
            if (m_scrollLatchY) {
                m_bg4ScrollY = (m_bg4ScrollY & 0x00FF) | (value << 8);
            } else {
                m_bg4ScrollY = (m_bg4ScrollY & 0xFF00) | value;
            }
            m_scrollPrevY = value;
            m_scrollLatchY = !m_scrollLatchY;
            break;
        }
            
        case 0x2115: { // VMAIN - VRAM Address Increment Mode
            m_vramIncrement = value & 0x03;  // Bits 0-1: increment size (00=1, 01=32, 10/11=128)
            m_vramMapping = (value >> 2) & 0x03;  // Bits 2-3: address mapping
            m_vramReadBuffer = value;  // Store full value including bit 7 (increment timing)
            static int vmainCount = 0;
            if (vmainCount < 5) {
                std::cout << "PPU: VMAIN=$" << std::hex << (int)value << std::dec 
                          << " - Inc size=" << (m_vramIncrement == 0 ? 1 : (m_vramIncrement == 1 ? 32 : 128))
                          << ", Inc after " << ((value & 0x80) ? "high" : "low") << " byte" << std::endl;
                vmainCount++;
            }
            break;
        }
            
        case 0x2102: // OAMADDL - OAM Address (low byte)
            m_oamAddress = (m_oamAddress & 0xFF00) | value;
            break;
            
        case 0x2103: // OAMADDH - OAM Address (high byte)
            m_oamAddress = (m_oamAddress & 0x00FF) | (value << 8);
            break;
            
        case 0x2104: // OAMDATA - OAM Data Write
            writeOAM(m_oamAddress++, value);
            m_oamAddress &= 0x01FF; // OAM is 512 bytes
            break;
            
        case 0x212C: { // TM - Main Screen Designation
            m_mainScreenDesignation = value;
            static int tmCount = 0;
            if (tmCount < 20) {
                std::ostringstream oss;
                oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                    << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                    << "PPU: TM (Main Screen)=$" << std::hex << (int)value << std::dec 
                    << " (BG1=" << ((value & 0x01) ? "ON" : "OFF")
                    << ", BG2=" << ((value & 0x02) ? "ON" : "OFF")
                    << ", BG3=" << ((value & 0x04) ? "ON" : "OFF")
                    << ", BG4=" << ((value & 0x08) ? "ON" : "OFF") << ")" << std::endl;
                Logger::getInstance().logPPU(oss.str());
                Logger::getInstance().flush();
                std::cout << oss.str();
                tmCount++;
            }
            break;
        }
            
        case 0x212D: { // TS - Sub Screen Designation
            m_subScreenDesignation = value;
            break;
        }
            
        case 0x2123: // W12SEL - Window Mask Settings for BG1 and BG2
            m_w12sel = value;
            break;
            
        case 0x2124: // W34SEL - Window Mask Settings for BG3 and BG4
            m_w34sel = value;
            break;
            
        case 0x2125: // WOBJSEL - Window Mask Settings for OBJ and Color Window
            m_wobjsel = value;
            break;
            
        case 0x2126: // WH0 - Window 1 Left Position
            m_wh0 = value;
            break;
            
        case 0x2127: // WH1 - Window 1 Right Position
            m_wh1 = value;
            break;
            
        case 0x2128: // WH2 - Window 2 Left Position
            m_wh2 = value;
            break;
            
        case 0x2129: // WH3 - Window 2 Right Position
            m_wh3 = value;
            break;
            
        case 0x212A: // WBGLOG - Window Mask Logic for BGs
            m_wbglog = value;
            break;
            
        case 0x212B: // WOBJLOG - Window Mask Logic for OBJs
            m_wobjlog = value;
            break;
            
        case 0x212E: // TMW - Window Mask for Main Screen
            m_tmw = value;
            break;
            
        case 0x212F: // TSW - Window Mask for Sub Screen
            m_tsw = value;
            break;
            
        case 0x2130: // CGWSEL - Color Math Control
            m_cgws = value;
            break;
            
        case 0x2131: // CGADSUB - Color Math Settings
            m_cgadsub = value;
            break;
            
        case 0x2132: // COLDATA - Fixed Color Data
            // Ignore for now
            break;
            
        case 0x2133: // SETINI - Screen Mode/Video Select
            // Ignore for now
            break;
            
        default:
            // Log unimplemented register writes
            static std::set<uint16_t> loggedRegs;
            if (loggedRegs.find(address) == loggedRegs.end() && loggedRegs.size() < 20) {
                std::cout << "PPU: Unimplemented write to $" << std::hex << address 
                          << " = $" << (int)value << std::dec << std::endl;
                loggedRegs.insert(address);
            }
            break;
    }
}

uint8_t PPU::readRegister(uint16_t address) {
    switch (address) {
        case 0x2137: { // SLHV - Software Latch for H/V Counter
            // Latch current H/V counter values
            m_latchedH = m_dot;
            m_latchedV = m_scanline;
            m_hvLatchRead = false;
            m_hvLatchHRead = false;
            m_hvLatchVRead = false;
            return 0;
        }
            
        case 0x213C: { // OPHCT - Horizontal Counter (Low/High)
            // First read: return low byte of latched horizontal counter
            // Second read: return high byte (bit 8 only)
            if (!m_hvLatchHRead) {
                // First read: return low byte
                m_hvLatchHRead = true;
                m_hvLatchRead = true;
                return m_latchedH & 0xFF;
            } else {
                // Second read: return high byte (bit 8 only)
                m_hvLatchHRead = false;
                return (m_latchedH >> 8) & 0x01;
            }
        }
        
        case 0x213D: { // OPVCT - Vertical Counter (Low/High)
            // First read: return low byte of latched vertical counter
            // Second read: return high byte (bit 8 only)
            if (!m_hvLatchVRead) {
                // First read: return low byte
                m_hvLatchVRead = true;
                m_hvLatchRead = true;
                return m_latchedV & 0xFF;
            } else {
                // Second read: return high byte (bit 8 only)
                m_hvLatchVRead = false;
                return (m_latchedV >> 8) & 0x01;
            }
        }
        
        case 0x213E: { // OPHCTH/OPVCTH - H/V Counter High (alternate read)
            // This is read after $213C or $213D
            // Return high byte of the last read counter
            // If H was last read, return H high byte
            // If V was last read, return V high byte
            if (m_hvLatchHRead) {
                // H was last read, return H high byte
                m_hvLatchHRead = false;
                return (m_latchedH >> 8) & 0x01;
            } else if (m_hvLatchVRead) {
                // V was last read, return V high byte
                m_hvLatchVRead = false;
                return (m_latchedV >> 8) & 0x01;
            }
            return 0;
        }
            
        case 0x213F: { // STAT78 - PPU Status Flag and Version
            // Bit 0: PPU version (0 = version 1, 1 = version 2)
            // Bit 6: Interlace field (0 = odd field, 1 = even field)
            // Bit 7: PPU2 latch flag (0 = not latched, 1 = latched)
            uint8_t result = 0x01; // Version 1, odd field, not latched
            // If H/V latch was just read, set bit 7
            if (m_hvLatchRead) {
                result |= 0x80;
            }
            return result;
        }
            
        case 0x2139: { // VMDATALREAD - VRAM Data Read (low byte)
            // VRAM read has a prefetch buffer - first read loads buffer, subsequent reads return buffered value
            uint8_t result = m_vramReadBuffer;
            m_vramReadBuffer = readVRAM(m_vramAddress * 2);
            // Increment after low byte read if bit 7 of VMAIN is 0
            if ((m_vramReadBuffer & 0x80) == 0) {
                uint16_t inc = (m_vramIncrement == 0) ? 1 : (m_vramIncrement == 1) ? 32 : 128;
                m_vramAddress += inc;
            }
            return result;
        }
            
        case 0x213A: { // VMDATAHREAD - VRAM Data Read (high byte)
            uint8_t result = readVRAM(m_vramAddress * 2 + 1);
            // Increment after high byte read if bit 7 of VMAIN is 1
            if ((m_vramReadBuffer & 0x80) != 0) {
                uint16_t inc = (m_vramIncrement == 0) ? 1 : (m_vramIncrement == 1) ? 32 : 128;
                m_vramAddress += inc;
            }
            return result;
        }
            
        case 0x4210: { // RDNMI - NMI Flag and Version
            // Bit 7: NMI flag (cleared on read)
            // Bits 0-3: CPU version
            uint8_t result = 0x02;  // CPU version 2
            
            // SNES RDNMI behavior:
            // - Bit 7 is set when VBlank occurs (scanline 225)
            // - Reading RDNMI clears the flag, BUT:
            //   - If we're still in VBlank period (scanlines 225-261), bit 7 should remain set
            //   - This allows wait loops to detect VBlank period
            // - The flag is cleared when VBlank ends (scanline >= 262)
            
            bool inVBlank = (m_scanline >= 225 && m_scanline < 262);
            
            // Set bit 7 if we're in VBlank period OR if the flag was previously set
            // The flag persists during the entire VBlank period even after reads
            if (inVBlank || m_nmiFlag) {
                result |= 0x80;
            }
            
            // RDNMI read logging removed to reduce log spam
            // VBlank state changes are logged in cpu.cpp via RDNMI-VBLANK and RDNMI-VBLANK-END
            
            // Clear the stored flag on read ONLY if we're outside VBlank period
            // If we're still in VBlank, keep the flag set so subsequent reads also return bit 7 = 1
            if (!inVBlank) {
                m_nmiFlag = false;
            }
            // Record to history ring buffer
            m_rdnmiHistory[m_rdnmiHistoryIndex] = result;
            m_rdnmiHistoryIndex = (m_rdnmiHistoryIndex + 1) % RDNMI_HISTORY_SIZE;
            // Build printable string lazily (hex bytes newest last)
            int pos = 0;
            for (int i = 0; i < RDNMI_HISTORY_SIZE; ++i) {
                int idx = (m_rdnmiHistoryIndex + i) % RDNMI_HISTORY_SIZE;
                uint8_t v = m_rdnmiHistory[idx];
                int written = snprintf(m_rdnmiHistoryStr + pos, sizeof(m_rdnmiHistoryStr) - pos, "%s%02X",
                                       (i == 0 ? "" : " "), v);
                if (written < 0) break;
                pos += written;
                if (pos >= (int)sizeof(m_rdnmiHistoryStr) - 1) break;
            }
            m_rdnmiHistoryStr[sizeof(m_rdnmiHistoryStr) - 1] = '\0';
            return result;
        }
            
        default:
            return 0;
    }
}

const char* PPU::getRDNMIHistoryString() {
    return m_rdnmiHistoryStr;
}

void PPU::writeVRAM(uint16_t address, uint8_t value) {
    if (address < m_vram.size()) {
        m_vram[address] = value;
        
        // Convert ASCII hex digits to values helper
        auto hexToValue = [](uint8_t c) -> uint8_t {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            return 0;
        };
        
        // Log test number updates (VRAM address 0x00DC-0x00E3 is where test number is displayed)
        // Test number is written as 4 hex digits at word addresses 0x006E-0x0071 (byte addresses 0x00DC-0x00E3)
        // Only log when the last digit (0x00E2) is written to avoid duplicate logs
        if (address == 0x00E2 && (address & 0x01) == 0) {
            // Read the 4 hex digits to reconstruct test number
            uint8_t digit0 = m_vram[0x00DC] & 0x7F;  // First digit (high byte of test number)
            uint8_t digit1 = m_vram[0x00DE] & 0x7F;  // Second digit
            uint8_t digit2 = m_vram[0x00E0] & 0x7F;  // Third digit
            uint8_t digit3 = value & 0x7F;  // Fourth digit (just written)
            
            uint8_t val0 = hexToValue(digit0);
            uint8_t val1 = hexToValue(digit1);
            uint8_t val2 = hexToValue(digit2);
            uint8_t val3 = hexToValue(digit3);
            uint16_t testNum = (val0 << 12) | (val1 << 8) | (val2 << 4) | val3;
            
            std::ostringstream oss;
            oss << "[TEST NUMBER UPDATE] Test Number: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << testNum 
                << " (" << std::dec << testNum << ")" << std::endl;
            Logger::getInstance().logPPU(oss.str());
            Logger::getInstance().flush();
        }
        
        // Log failure information when "Failed" text is written (0x0064-0x006F)
        static bool failureLogged = false;
        if (address >= 0x0064 && address <= 0x006F && (address & 0x01) == 0) {
            // Check if "Failed" text is complete
            if (address == 0x006E) {  // Last character of "Failed"
                uint8_t f = m_vram[0x0064] & 0x7F;
                uint8_t a1 = m_vram[0x0066] & 0x7F;
                uint8_t i = m_vram[0x0068] & 0x7F;
                uint8_t l = m_vram[0x006A] & 0x7F;
                uint8_t e = m_vram[0x006C] & 0x7F;
                uint8_t d = value & 0x7F;
                
                if (f == 'F' && a1 == 'a' && i == 'i' && l == 'l' && e == 'e' && d == 'd') {
                    failureLogged = false;  // Reset flag for new failure
                }
            }
        }
        
        // Log register values when they are written after failure
        // Y register is written last (0x01CA-0x01CD), so detect failure when Y register low byte (0x01CC) is written
        // Note: 0x01CA = Y high digit, 0x01CC = Y low digit, 0x01CD = attribute byte
        if (address == 0x01CC && (address & 0x01) == 0 && !failureLogged) {
            // Check if "Failed" text exists
            uint8_t f = m_vram[0x0064] & 0x7F;
            uint8_t a1 = m_vram[0x0066] & 0x7F;
            uint8_t i = m_vram[0x0068] & 0x7F;
            uint8_t l = m_vram[0x006A] & 0x7F;
            uint8_t e = m_vram[0x006C] & 0x7F;
            uint8_t d = m_vram[0x006E] & 0x7F;
            
            if (f == 'F' && a1 == 'a' && i == 'i' && l == 'l' && e == 'e' && d == 'd') {
                // A register: 0x014A-0x014D (2 hex digits)
                uint8_t a_high = m_vram[0x014A] & 0x7F;
                uint8_t a_low = m_vram[0x014C] & 0x7F;
                uint8_t a_val = (hexToValue(a_high) << 4) | hexToValue(a_low);
                
                // X register: 0x018A-0x018D
                uint8_t x_high = m_vram[0x018A] & 0x7F;
                uint8_t x_low = m_vram[0x018C] & 0x7F;
                uint8_t x_val = (hexToValue(x_high) << 4) | hexToValue(x_low);
                
                // Y register: 0x01CA-0x01CD
                uint8_t y_high = m_vram[0x01CA] & 0x7F;
                uint8_t y_low = value & 0x7F;  // Just written
                uint8_t y_val = (hexToValue(y_high) << 4) | hexToValue(y_low);
                
                // PSW register: 0x020A-0x020D
                uint8_t p_high = m_vram[0x020A] & 0x7F;
                uint8_t p_low = m_vram[0x020C] & 0x7F;
                uint8_t p_val = (hexToValue(p_high) << 4) | hexToValue(p_low);
                
                // Get current test number
                uint8_t t0 = m_vram[0x00DC] & 0x7F;
                uint8_t t1 = m_vram[0x00DE] & 0x7F;
                uint8_t t2 = m_vram[0x00E0] & 0x7F;
                uint8_t t3 = m_vram[0x00E2] & 0x7F;
                uint16_t testNum = (hexToValue(t0) << 12) | (hexToValue(t1) << 8) | (hexToValue(t2) << 4) | hexToValue(t3);
                
                std::ostringstream oss;
                oss << "[TEST FAILURE] Test Number: 0x" << std::hex << std::uppercase << std::setfill('0') << std::setw(4) << testNum 
                    << " (" << std::dec << testNum << ")" << std::endl;
                oss << "  Registers: A=0x" << std::hex << std::setw(2) << (int)a_val 
                    << " X=0x" << std::setw(2) << (int)x_val
                    << " Y=0x" << std::setw(2) << (int)y_val
                    << " PSW=0x" << std::setw(2) << (int)p_val << std::dec << std::endl;
                Logger::getInstance().logPPU(oss.str());
                Logger::getInstance().flush();
                failureLogged = true;
                
                // Dump VRAM immediately when failure is detected to capture failure state
                dumpVRAMHex("vram_dump.txt");
                std::cout << "PPU: VRAM dump saved after test failure detection" << std::endl;
            }
        }
        
        #ifdef ENABLE_LOGGING
        // Log VRAM writes for debugging
        std::ostringstream oss;
        oss << "VRAM Write: [0x" << std::hex << std::setfill('0') << std::setw(4) << address 
            << "] = 0x" << std::setw(2) << (int)value;
        Logger::getInstance().logPPU(oss.str());
        #endif
    }
}

uint8_t PPU::readVRAM(uint16_t address) {
    if (address < m_vram.size()) {
        return m_vram[address];
    }
    return 0;
}

void PPU::writeCGRAM(uint8_t address, uint8_t value) {
    if (address < m_cgram.size()) {
        m_cgram[address] = value;
    }
}

void PPU::writeOAM(uint16_t address, uint8_t value) {
    if (address < m_oam.size()) {
        m_oam[address] = value;
    }
}

uint32_t PPU::renderSprites(int x, int y) {
    // SNES OAM (Object Attribute Memory) structure:
    // Each sprite is 4 bytes: X, Y, Tile, Attributes
    // 128 sprites maximum, 32 sprites per scanline maximum
    
    // Extract sprite size settings from OBSEL register
    uint8_t spriteSize = (m_objSize >> 5) & 0x03; // Bits 5-6
    uint8_t nameBase = (m_objSize & 0x07) << 12;  // Bits 0-2, shifted to get base address
    
    // Determine sprite dimensions based on size setting
    int spriteWidth, spriteHeight;
    switch (spriteSize) {
        case 0: spriteWidth = 8;  spriteHeight = 8;  break;  // Small
        case 1: spriteWidth = 8;  spriteHeight = 16; break;  // Small + Large
        case 2: spriteWidth = 8;  spriteHeight = 32; break;  // Small + Large + Large
        case 3: spriteWidth = 16; spriteHeight = 32; break;  // Large + Large + Large
        default: spriteWidth = 8; spriteHeight = 8; break;
    }
    
    // Check all sprites to see if any are at this pixel position
    for (int sprite = 0; sprite < 128; sprite++) {
        int oamAddr = sprite * 4;
        
        if (oamAddr + 3 >= m_oam.size()) continue;
        
        // Read sprite data
        uint8_t spriteX = m_oam[oamAddr];
        uint8_t spriteY = m_oam[oamAddr + 1];
        uint8_t tileNumber = m_oam[oamAddr + 2];
        uint8_t attributes = m_oam[oamAddr + 3];
        
        // Skip if sprite is not visible (Y = 0 or Y > 224)
        if (spriteY == 0 || spriteY > 224) continue;
        
        // Check if sprite is on this scanline
        if (y >= spriteY && y < spriteY + spriteHeight) {
            // Check if sprite is at this X position
            if (x >= spriteX && x < spriteX + spriteWidth) {
                // Calculate pixel position within sprite
                int pixelX = x - spriteX;
                int pixelY = y - spriteY;
                
                // Apply horizontal flip if needed
                if (attributes & 0x40) {
                    pixelX = spriteWidth - 1 - pixelX;
                }
                
                // Apply vertical flip if needed
                if (attributes & 0x80) {
                    pixelY = spriteHeight - 1 - pixelY;
                }
                
                // Calculate tile number based on sprite size
                int tileX = pixelX / 8;
                int tileY = pixelY / 8;
                int tilePixelX = pixelX % 8;
                int tilePixelY = pixelY % 8;
                
                // Calculate actual tile number for multi-tile sprites
                uint16_t actualTileNumber = tileNumber;
                if (spriteWidth > 8) {
                    actualTileNumber += tileX;
                }
                if (spriteHeight > 8) {
                    actualTileNumber += tileY * (spriteWidth / 8);
                }
                
                // Get tile data address using name base
                uint16_t tileAddr = nameBase + actualTileNumber * 32; // 32 bytes per 8x8 tile
                
                if (tileAddr + 32 <= m_vram.size()) {
                    // Decode tile
                    uint8_t tileData[32];
                    for (int i = 0; i < 32; i++) {
                        tileData[i] = m_vram[tileAddr + i];
                    }
                    
                    uint8_t pixels[64];
                    decodeTile(tileData, pixels, 4);
                    
                    uint8_t pixelIndex = pixels[tilePixelY * 8 + tilePixelX];
                    if (pixelIndex != 0) { // Not transparent
                        // Use palette 8-15 for sprites (palette 0-7 are for backgrounds)
                        uint8_t palette = 8 + ((attributes >> 1) & 0x07);
                        return getColor(palette, pixelIndex);
                    }
                }
            }
        }
    }
    
    return 0; // No sprite pixel at this position
}

// Render sprite pixel with priority information
PixelInfo PPU::renderSpritePixel(int x, int y) {
    // SNES OAM structure and extended OAM
    // Each sprite has priority information in extended OAM
    // For now, return priority 2 for sprites (between BG priorities)
    
    uint32_t spriteColor = renderSprites(x, y);
    if (spriteColor != 0) {
        // Sprites typically have priority 2-3
        // Priority is determined by sprite number (lower number = higher priority)
        // and extended OAM attributes
        return {spriteColor, 2}; // Default sprite priority
    }
    
    return {0, 0}; // No sprite
}

// Check if window masking applies to this pixel
bool PPU::isWindowEnabled(int x, int bgIndex, bool isSprite) {
    // Window settings are complex - simplified version
    // Full implementation needs to check window 1 and window 2 settings
    // for each layer based on W12SEL, W34SEL, WOBJSEL
    
    // For now, return false (no window masking)
    // TODO: Implement full window logic
    (void)x;
    (void)bgIndex;
    (void)isSprite;
    return false;
}

// Check window mask based on window settings
bool PPU::checkWindowMask(int x, uint8_t windowSettings) {
    // Simplified window check
    // Window 1: if x is within [WH0, WH1), window is active based on bits 0-1
    // Window 2: if x is within [WH2, WH3), window is active based on bits 4-5
    // Logic: bits 2-3 (window 1), bits 6-7 (window 2)
    
    bool inWindow1 = (x >= m_wh0 && x < m_wh1);
    bool inWindow2 = (x >= m_wh2 && x < m_wh3);
    
    // Window settings: bits 0-1 = Window 1, bits 4-5 = Window 2
    bool window1Enable = (windowSettings & 0x03) != 0;
    bool window2Enable = ((windowSettings >> 4) & 0x03) != 0;
    
    // Combine windows based on logic
    bool result = false;
    if (window1Enable && inWindow1) {
        result = true;
    }
    if (window2Enable && inWindow2) {
        // Window logic: OR or AND based on settings
        result = true; // Simplified
    }
    
    return result;
}

// Render Sub Screen
uint32_t PPU::renderSubScreen(int x) {
    int y = m_scanline;
    
    // Sub Screen uses same background layers but different designation
    // For now, render same as main screen
    // TODO: Implement proper sub screen rendering with different settings
    
    if (m_bgMode == 0) {
        return renderBackgroundMode0(x);
    } else if (m_bgMode == 1) {
        return renderBackgroundMode1(x);
    }
    
    return 0x000000FF; // Black (0xRRGGBBAA format)
}

// Apply Color Math (Add/Sub mode)
uint32_t PPU::applyColorMath(uint32_t mainColor, uint32_t subColor) {
    // Color Math modes:
    // CGWS bit 0-1: Math mode
    // CGADSUB: Controls which layers participate and add/subtract mode
    
    if ((m_cgws & 0x03) == 0) {
        // Math disabled
        return mainColor;
    }
    
    // Extract RGB components
    uint8_t mainR = (mainColor & 0xFF);
    uint8_t mainG = ((mainColor >> 8) & 0xFF);
    uint8_t mainB = ((mainColor >> 16) & 0xFF);
    
    uint8_t subR = (subColor & 0xFF);
    uint8_t subG = ((subColor >> 8) & 0xFF);
    uint8_t subB = ((subColor >> 16) & 0xFF);
    
    uint8_t finalR, finalG, finalB;
    
    bool subtractMode = (m_cgadsub & 0x80) != 0;
    
    if (subtractMode) {
        // Subtract: Main - Sub
        finalR = (mainR > subR) ? (mainR - subR) : 0;
        finalG = (mainG > subG) ? (mainG - subG) : 0;
        finalB = (mainB > subB) ? (mainB - subB) : 0;
    } else {
        // Add: Main + Sub (clamped to 255)
        finalR = (mainR + subR > 255) ? 255 : (mainR + subR);
        finalG = (mainG + subG > 255) ? 255 : (mainG + subG);
        finalB = (mainB + subB > 255) ? 255 : (mainB + subB);
    }
    
    // Half brightness mode (bit 0 of CGWS)
    if ((m_cgws & 0x01) != 0) {
        finalR = (finalR * mainR) / 256;
        finalG = (finalG * mainG) / 256;
        finalB = (finalB * mainB) / 256;
    }
    
    return (0xFF << 24) | (finalB << 16) | (finalG << 8) | finalR;
}

void PPU::incrementVRAMAddress() {
    // VRAM address increment based on VMAIN register
    uint16_t increment = 1;
    
    switch (m_vramIncrement) {
        case 0: increment = 1; break;
        case 1: increment = 32; break;
        case 2: increment = 128; break;
        case 3: increment = 128; break;
    }
    
    m_vramAddress += increment;
    
    // Wrap around at 64KB boundary (VRAM is 64KB = 0x10000)
    // m_vramAddress is uint16_t, so it automatically wraps at 65536
    // But we need to ensure it stays within valid range
    if (m_vramAddress > 0xFFFF) {
        m_vramAddress &= 0xFFFF;
    }
}

void PPU::loadROMData(const std::vector<uint8_t>& romData) {
    std::ostringstream oss;
    oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
        << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
        << "PPU: Loading ROM data into VRAM..." << std::endl;
    Logger::getInstance().logPPU(oss.str());
    Logger::getInstance().flush();
    oss.str("");
    oss.clear();
    oss << "ROM size: " << romData.size() << " bytes" << std::endl;
    Logger::getInstance().logPPU(oss.str());
    
    // DO NOT copy ROM into VRAM automatically!
    // VRAM should only be written by CPU through DMA or direct writes to $2118/2119
    // The ROM data will be loaded by the game's initialization code via DMA
    oss.str("");
    oss.clear();
    oss << "VRAM initialized to zero, waiting for CPU to load graphics via DMA" << std::endl;
    Logger::getInstance().logPPU(oss.str());
    Logger::getInstance().flush();
    
    // Verify first few bytes
    oss.str("");
    oss.clear();
    oss << "First 16 VRAM bytes: ";
    for (int i = 0; i < 16; i++) {
        oss << std::hex << (int)m_vram[i] << " ";
    }
    oss << std::dec << std::endl;
    Logger::getInstance().logPPU(oss.str());
    oss.str("");
    for (int i = 0; i < 16; i++) {
        oss << std::hex << (int)m_vram[i] << " ";
    }
    oss << std::dec << std::endl;
    Logger::getInstance().logPPU(oss.str());
    
    // Initialize CGRAM with some test colors
    for (int i = 0; i < 256; i++) {
        // Create a simple color palette
        uint16_t color = 0;
        if (i < 16) {
            // First palette: grayscale
            uint8_t intensity = (i * 255) / 15;
            color = (intensity >> 3) | ((intensity >> 3) << 5) | ((intensity >> 3) << 10);
        } else if (i < 32) {
            // Second palette: red tones
            uint8_t red = ((i - 16) * 255) / 15;
            color = (red >> 3) | (0 << 5) | (0 << 10);
        } else if (i < 48) {
            // Third palette: green tones
            uint8_t green = ((i - 32) * 255) / 15;
            color = (0) | ((green >> 3) << 5) | (0 << 10);
        } else if (i < 64) {
            // Fourth palette: blue tones
            uint8_t blue = ((i - 48) * 255) / 15;
            color = (0) | (0 << 5) | ((blue >> 3) << 10);
        } else {
            // Other palettes: mixed colors
            color = (i % 32) | ((i % 32) << 5) | ((i % 32) << 10);
        }
        
        m_cgram[i * 2] = color & 0xFF;
        m_cgram[i * 2 + 1] = (color >> 8) & 0xFF;
    }
    
    oss.str("");
    oss.clear();
    oss << "Initialized CGRAM with test colors" << std::endl;
    Logger::getInstance().logPPU(oss.str());
    Logger::getInstance().flush();
}

void PPU::dumpVRAM(const std::string& filename) {
    std::ofstream file(filename, std::ios::binary);
    if (file.is_open()) {
        file.write(reinterpret_cast<const char*>(m_vram.data()), m_vram.size());
        file.close();
        std::cout << "PPU: VRAM dumped to " << filename << " (" << m_vram.size() << " bytes)" << std::endl;
    } else {
        std::cerr << "PPU: Failed to open " << filename << " for writing" << std::endl;
    }
}

void PPU::dumpVRAMHex(const std::string& filename) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << "VRAM Dump (64KB = 65536 bytes)" << std::endl;
        file << "========================================" << std::endl;
        file << std::hex << std::setfill('0');
        
        // Dump in 16-byte rows
        for (size_t i = 0; i < m_vram.size(); i += 16) {
            file << std::setw(4) << i << ": ";
            
            // Hex bytes
            for (size_t j = 0; j < 16 && (i + j) < m_vram.size(); j++) {
                file << std::setw(2) << (int)m_vram[i + j] << " ";
            }
            
            // ASCII representation
            file << " | ";
            for (size_t j = 0; j < 16 && (i + j) < m_vram.size(); j++) {
                uint8_t byte = m_vram[i + j];
                file << (byte >= 32 && byte < 127 ? (char)byte : '.');
            }
            
            file << std::endl;
        }
        
        file << std::dec;
        file.close();
        std::cout << "PPU: VRAM hex dump saved to " << filename << std::endl;
        
        // Also dump key regions with detailed analysis
        std::cout << "\nPPU: Key VRAM regions analysis:" << std::endl;
        std::cout << "========================================" << std::endl;
        
        // BG1 Tilemap (0x0000-0x07FF) - 32x32 tiles = 2048 bytes
        std::cout << "BG1 Tilemap (0x0000-0x07FF, 2048 bytes):" << std::endl;
        size_t nonZeroCount = 0;
        size_t tilemapEntries = 0;
        for (size_t i = 0; i < 0x800; i++) {
            if (m_vram[i] != 0) {
                nonZeroCount++;
            }
            if (i % 2 == 0 && i + 1 < 0x800) {
                uint16_t entry = m_vram[i] | (m_vram[i + 1] << 8);
                if (entry != 0) {
                    tilemapEntries++;
                }
            }
        }
        std::cout << "  Non-zero bytes: " << nonZeroCount << " / 2048" << std::endl;
        std::cout << "  Non-zero tilemap entries: " << tilemapEntries << " / 1024" << std::endl;
        std::cout << "  Status: " << (nonZeroCount > 0 ? "HAS DATA" : "ALL ZEROS") << std::endl;
        
        // BG1 Tiles at 0x4000 (2bpp = 16 bytes per tile)
        std::cout << "\nBG1 Tiles at 0x4000 (2bpp, 16 bytes/tile):" << std::endl;
        nonZeroCount = 0;
        size_t tileCount = 0;
        for (size_t i = 0x4000; i < 0x8000 && i < m_vram.size(); i += 16) {
            bool tileHasData = false;
            for (size_t j = 0; j < 16 && (i + j) < m_vram.size(); j++) {
                if (m_vram[i + j] != 0) {
                    nonZeroCount++;
                    tileHasData = true;
                }
            }
            if (tileHasData) tileCount++;
        }
        std::cout << "  Non-zero bytes: " << nonZeroCount << " / " << (0x8000 - 0x4000) << std::endl;
        std::cout << "  Tiles with data: " << tileCount << " / " << ((0x8000 - 0x4000) / 16) << std::endl;
        std::cout << "  Status: " << (nonZeroCount > 0 ? "HAS DATA" : "ALL ZEROS") << std::endl;
        
        // BG1 Tiles at 0x8000 (alternative location)
        std::cout << "\nBG1 Tiles at 0x8000 (2bpp, 16 bytes/tile):" << std::endl;
        nonZeroCount = 0;
        tileCount = 0;
        for (size_t i = 0x8000; i < 0xC000 && i < m_vram.size(); i += 16) {
            bool tileHasData = false;
            for (size_t j = 0; j < 16 && (i + j) < m_vram.size(); j++) {
                if (m_vram[i + j] != 0) {
                    nonZeroCount++;
                    tileHasData = true;
                }
            }
            if (tileHasData) tileCount++;
        }
        std::cout << "  Non-zero bytes: " << nonZeroCount << " / " << (0xC000 - 0x8000) << std::endl;
        std::cout << "  Tiles with data: " << tileCount << " / " << ((0xC000 - 0x8000) / 16) << std::endl;
        std::cout << "  Status: " << (nonZeroCount > 0 ? "HAS DATA" : "ALL ZEROS") << std::endl;
        
        // Overall VRAM statistics
        std::cout << "\nOverall VRAM statistics:" << std::endl;
        size_t totalNonZero = 0;
        for (size_t i = 0; i < m_vram.size(); i++) {
            if (m_vram[i] != 0) totalNonZero++;
        }
        std::cout << "  Total non-zero bytes: " << totalNonZero << " / " << m_vram.size() 
                  << " (" << (totalNonZero * 100 / m_vram.size()) << "%)" << std::endl;
        std::cout << "========================================\n" << std::endl;
    } else {
        std::cerr << "PPU: Failed to open " << filename << " for writing" << std::endl;
    }
}

// Simple 8x8 bitmap font for ASCII characters
static const uint8_t font8x8[96][8] = {
    // Space (0x20)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // ! (0x21)
    {0x18, 0x3C, 0x3C, 0x18, 0x18, 0x00, 0x18, 0x00},
    // " (0x22)
    {0x36, 0x36, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // # (0x23)
    {0x36, 0x7F, 0x36, 0x36, 0x7F, 0x36, 0x36, 0x00},
    // $ (0x24)
    {0x0C, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x0C, 0x00},
    // % (0x25)
    {0x00, 0x63, 0x33, 0x18, 0x0C, 0x66, 0x63, 0x00},
    // & (0x26)
    {0x1C, 0x36, 0x1C, 0x6E, 0x3B, 0x33, 0x6E, 0x00},
    // ' (0x27)
    {0x06, 0x06, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00},
    // ( (0x28)
    {0x18, 0x0C, 0x06, 0x06, 0x06, 0x0C, 0x18, 0x00},
    // ) (0x29)
    {0x06, 0x0C, 0x18, 0x18, 0x18, 0x0C, 0x06, 0x00},
    // * (0x2A)
    {0x00, 0x66, 0x3C, 0xFF, 0x3C, 0x66, 0x00, 0x00},
    // + (0x2B)
    {0x00, 0x0C, 0x0C, 0x7F, 0x0C, 0x0C, 0x00, 0x00},
    // , (0x2C)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x06, 0x00},
    // - (0x2D)
    {0x00, 0x00, 0x00, 0x7F, 0x00, 0x00, 0x00, 0x00},
    // . (0x2E)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C, 0x00},
    // / (0x2F)
    {0x60, 0x30, 0x18, 0x0C, 0x06, 0x03, 0x01, 0x00},
    // 0 (0x30)
    {0x3E, 0x63, 0x73, 0x7B, 0x6F, 0x67, 0x3E, 0x00},
    // 1 (0x31)
    {0x0C, 0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x3F, 0x00},
    // 2 (0x32)
    {0x1E, 0x33, 0x30, 0x1C, 0x06, 0x33, 0x3F, 0x00},
    // 3 (0x33)
    {0x1E, 0x33, 0x30, 0x1C, 0x30, 0x33, 0x1E, 0x00},
    // 4 (0x34)
    {0x38, 0x3C, 0x36, 0x33, 0x7F, 0x30, 0x78, 0x00},
    // 5 (0x35)
    {0x3F, 0x03, 0x1F, 0x30, 0x30, 0x33, 0x1E, 0x00},
    // 6 (0x36)
    {0x1C, 0x06, 0x03, 0x1F, 0x33, 0x33, 0x1E, 0x00},
    // 7 (0x37)
    {0x3F, 0x33, 0x30, 0x18, 0x0C, 0x0C, 0x0C, 0x00},
    // 8 (0x38)
    {0x1E, 0x33, 0x33, 0x1E, 0x33, 0x33, 0x1E, 0x00},
    // 9 (0x39)
    {0x1E, 0x33, 0x33, 0x3E, 0x30, 0x18, 0x0E, 0x00},
    // : (0x3A)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x0C, 0x00},
    // ; (0x3B)
    {0x00, 0x0C, 0x0C, 0x00, 0x00, 0x0C, 0x06, 0x00},
    // < (0x3C)
    {0x18, 0x0C, 0x06, 0x03, 0x06, 0x0C, 0x18, 0x00},
    // = (0x3D)
    {0x00, 0x00, 0x7F, 0x00, 0x00, 0x7F, 0x00, 0x00},
    // > (0x3E)
    {0x06, 0x0C, 0x18, 0x30, 0x18, 0x0C, 0x06, 0x00},
    // ? (0x3F)
    {0x1E, 0x33, 0x30, 0x18, 0x0C, 0x00, 0x0C, 0x00},
    // @ (0x40)
    {0x3E, 0x63, 0x7B, 0x7B, 0x7B, 0x03, 0x1E, 0x00},
    // A (0x41)
    {0x0C, 0x1E, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x00},
    // B (0x42)
    {0x3F, 0x66, 0x66, 0x3E, 0x66, 0x66, 0x3F, 0x00},
    // C (0x43)
    {0x3C, 0x66, 0x03, 0x03, 0x03, 0x66, 0x3C, 0x00},
    // D (0x44)
    {0x1F, 0x36, 0x66, 0x66, 0x66, 0x36, 0x1F, 0x00},
    // E (0x45)
    {0x7F, 0x06, 0x06, 0x3E, 0x06, 0x06, 0x7F, 0x00},
    // F (0x46)
    {0x7F, 0x06, 0x06, 0x3E, 0x06, 0x06, 0x06, 0x00},
    // G (0x47)
    {0x3C, 0x66, 0x03, 0x03, 0x73, 0x66, 0x7C, 0x00},
    // H (0x48)
    {0x33, 0x33, 0x33, 0x3F, 0x33, 0x33, 0x33, 0x00},
    // I (0x49)
    {0x1E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},
    // J (0x4A)
    {0x78, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E, 0x00},
    // K (0x4B)
    {0x67, 0x66, 0x36, 0x1E, 0x36, 0x66, 0x67, 0x00},
    // L (0x4C)
    {0x06, 0x06, 0x06, 0x06, 0x06, 0x06, 0x7F, 0x00},
    // M (0x4D)
    {0x63, 0x77, 0x7F, 0x7F, 0x6B, 0x63, 0x63, 0x00},
    // N (0x4E)
    {0x63, 0x67, 0x6F, 0x7B, 0x73, 0x63, 0x63, 0x00},
    // O (0x4F)
    {0x1C, 0x36, 0x63, 0x63, 0x63, 0x36, 0x1C, 0x00},
    // P (0x50)
    {0x3F, 0x66, 0x66, 0x3E, 0x06, 0x06, 0x0F, 0x00},
    // Q (0x51)
    {0x1E, 0x33, 0x33, 0x33, 0x3B, 0x1E, 0x38, 0x00},
    // R (0x52)
    {0x3F, 0x66, 0x66, 0x3E, 0x36, 0x66, 0x67, 0x00},
    // S (0x53)
    {0x1E, 0x33, 0x07, 0x0E, 0x38, 0x33, 0x1E, 0x00},
    // T (0x54)
    {0x3F, 0x2D, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},
    // U (0x55)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x3F, 0x00},
    // V (0x56)
    {0x33, 0x33, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},
    // W (0x57)
    {0x63, 0x63, 0x63, 0x6B, 0x7F, 0x77, 0x63, 0x00},
    // X (0x58)
    {0x63, 0x63, 0x36, 0x1C, 0x1C, 0x36, 0x63, 0x00},
    // Y (0x59)
    {0x33, 0x33, 0x33, 0x1E, 0x0C, 0x0C, 0x1E, 0x00},
    // Z (0x5A)
    {0x7F, 0x63, 0x31, 0x18, 0x4C, 0x66, 0x7F, 0x00},
    // [ (0x5B)
    {0x1E, 0x06, 0x06, 0x06, 0x06, 0x06, 0x1E, 0x00},
    // \ (0x5C)
    {0x03, 0x06, 0x0C, 0x18, 0x30, 0x60, 0x40, 0x00},
    // ] (0x5D)
    {0x1E, 0x18, 0x18, 0x18, 0x18, 0x18, 0x1E, 0x00},
    // ^ (0x5E)
    {0x08, 0x1C, 0x36, 0x63, 0x00, 0x00, 0x00, 0x00},
    // _ (0x5F)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7F},
    // ` (0x60)
    {0x0C, 0x0C, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00},
    // a (0x61)
    {0x00, 0x00, 0x1E, 0x30, 0x3E, 0x33, 0x6E, 0x00},
    // b (0x62)
    {0x07, 0x06, 0x06, 0x3E, 0x66, 0x66, 0x3B, 0x00},
    // c (0x63)
    {0x00, 0x00, 0x1E, 0x33, 0x03, 0x33, 0x1E, 0x00},
    // d (0x64)
    {0x38, 0x30, 0x30, 0x3E, 0x33, 0x33, 0x6E, 0x00},
    // e (0x65)
    {0x00, 0x00, 0x1E, 0x33, 0x3F, 0x03, 0x1E, 0x00},
    // f (0x66)
    {0x1C, 0x36, 0x06, 0x0F, 0x06, 0x06, 0x0F, 0x00},
    // g (0x67)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x1F},
    // h (0x68)
    {0x07, 0x06, 0x36, 0x6E, 0x66, 0x66, 0x67, 0x00},
    // i (0x69)
    {0x0C, 0x00, 0x0E, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},
    // j (0x6A)
    {0x30, 0x00, 0x30, 0x30, 0x30, 0x33, 0x33, 0x1E},
    // k (0x6B)
    {0x07, 0x06, 0x66, 0x36, 0x1E, 0x36, 0x67, 0x00},
    // l (0x6C)
    {0x0E, 0x0C, 0x0C, 0x0C, 0x0C, 0x0C, 0x1E, 0x00},
    // m (0x6D)
    {0x00, 0x00, 0x33, 0x7F, 0x7F, 0x6B, 0x63, 0x00},
    // n (0x6E)
    {0x00, 0x00, 0x1F, 0x33, 0x33, 0x33, 0x33, 0x00},
    // o (0x6F)
    {0x00, 0x00, 0x1E, 0x33, 0x33, 0x33, 0x1E, 0x00},
    // p (0x70)
    {0x00, 0x00, 0x3B, 0x66, 0x66, 0x3E, 0x06, 0x0F},
    // q (0x71)
    {0x00, 0x00, 0x6E, 0x33, 0x33, 0x3E, 0x30, 0x78},
    // r (0x72)
    {0x00, 0x00, 0x3B, 0x6E, 0x66, 0x06, 0x0F, 0x00},
    // s (0x73)
    {0x00, 0x00, 0x3E, 0x03, 0x1E, 0x30, 0x1F, 0x00},
    // t (0x74)
    {0x08, 0x0C, 0x3E, 0x0C, 0x0C, 0x2C, 0x18, 0x00},
    // u (0x75)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x33, 0x6E, 0x00},
    // v (0x76)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x1E, 0x0C, 0x00},
    // w (0x77)
    {0x00, 0x00, 0x63, 0x6B, 0x7F, 0x7F, 0x36, 0x00},
    // x (0x78)
    {0x00, 0x00, 0x63, 0x36, 0x1C, 0x36, 0x63, 0x00},
    // y (0x79)
    {0x00, 0x00, 0x33, 0x33, 0x33, 0x3E, 0x30, 0x1F},
    // z (0x7A)
    {0x00, 0x00, 0x3F, 0x19, 0x0C, 0x26, 0x3F, 0x00},
    // { (0x7B)
    {0x38, 0x0C, 0x0C, 0x07, 0x0C, 0x0C, 0x38, 0x00},
    // | (0x7C)
    {0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x18, 0x00},
    // } (0x7D)
    {0x07, 0x0C, 0x0C, 0x38, 0x0C, 0x0C, 0x07, 0x00},
    // ~ (0x7E)
    {0x6E, 0x3B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

void PPU::drawChar(int x, int y, char c, uint32_t color) {
    if (c < 32 || c >= 127) {
        c = '?'; // Unknown character
    }
    
    const uint8_t* fontData = font8x8[c - 32];
    
    for (int row = 0; row < 8; row++) {
        if (y + row < 0 || y + row >= SCREEN_HEIGHT) continue;
        
        uint8_t fontRow = fontData[row];
        for (int col = 0; col < 8; col++) {
            if (x + col < 0 || x + col >= SCREEN_WIDTH) continue;
            
            // Fix bit order: MSB first (bit 7 = leftmost pixel)
            if (fontRow & (1 << (7 - col))) {
                int index = (y + row) * SCREEN_WIDTH + (x + col);
                m_framebuffer[index] = color;
            }
        }
    }
}

void PPU::renderText(int x, int y, const std::string& text, uint32_t color) {
    int currentX = x;
    for (size_t i = 0; i < text.length(); i++) {
        drawChar(currentX, y, text[i], color);
        currentX += 8; // 8 pixels per character
    }
}

void PPU::dumpCGRAM(const std::string& filename) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << "CGRAM Dump (512 bytes = 256 colors)" << std::endl;
        file << "========================================" << std::endl;
        file << std::hex << std::setfill('0');
        
        for (size_t i = 0; i < m_cgram.size(); i += 2) {
            uint16_t color = m_cgram[i] | (m_cgram[i + 1] << 8);
            uint8_t r = (color & 0x1F) << 3;
            uint8_t g = ((color >> 5) & 0x1F) << 3;
            uint8_t b = ((color >> 10) & 0x1F) << 3;
            
            file << "CGRAM[" << std::setw(3) << (i / 2) << "] = 0x" << std::setw(4) << color
                 << " (RGB: " << std::setw(3) << (int)r << ", " << std::setw(3) << (int)g << ", " << std::setw(3) << (int)b << ")" << std::endl;
        }
        
        file << std::dec;
        file.close();
        std::cout << "PPU: CGRAM dump saved to " << filename << std::endl;
    } else {
        std::cerr << "PPU: Failed to open " << filename << " for writing" << std::endl;
    }
}
