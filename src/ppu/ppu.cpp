#include "ppu.h"
#include "../cpu/cpu.h"
#include "../debug/logger.h"
#include <cstdio>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <set>
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
    
    // Set initial background color in CGRAM
    m_cgram[0] = 0x1F;
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
    
    // Initialize framebuffer with test pattern
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            int index = y * SCREEN_WIDTH + x;
            // Create a test pattern
            uint8_t r = (x * 255) / SCREEN_WIDTH;
            uint8_t g = (y * 255) / SCREEN_HEIGHT;
            uint8_t b = ((x + y) * 255) / (SCREEN_WIDTH + SCREEN_HEIGHT);
            // SDL_PIXELFORMAT_RGBA8888: 0xAABBGGRR (little-endian)
            m_framebuffer[index] = (0xFF << 24) | (b << 16) | (g << 8) | r;
        }
    }

    m_videoInitialized = true;
    std::cout << "PPU: Video initialized - " << SCREEN_WIDTH << "x" << SCREEN_HEIGHT << std::endl;
    return true;
}
int frameCount = 0;

void PPU::renderFrame() {
    // Debug: Print when called (reduced output)
    static int frameRenderCount = 0;
    if (frameRenderCount < 2) {
        std::cout << "renderFrame() called, frame " << frameRenderCount << std::endl;
        frameRenderCount++;
    }

    #ifdef USE_SDL
    /*Uint32* pixels = NULL;
    int pitch = SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(Uint32); 
    
    if (SDL_LockTexture(m_texture, NULL, (void**)&pixels, &pitch) == 0) {
    
        memcpy(pixels, m_framebuffer, SCREEN_WIDTH * SCREEN_HEIGHT * sizeof(Uint32));
    
        SDL_UnlockTexture(m_texture);
        //std::cout << " buffer written to texture" << std::endl;
    }
    else {
        //std::cout << " failed to lock texture" << std::endl;
    }

    SDL_RenderClear(m_renderer); 
    SDL_RenderCopy(m_renderer, m_texture, NULL, NULL);
    SDL_RenderPresent(m_renderer);
    */
    #endif

    
    // Reduced debugging output for performance
    
    frameCount++;
    if (frameCount % 300 == 0) {  // Only log every 5 seconds (300 frames at 60fps)
        std::cout << "PPU: Rendered frame " << frameCount << std::endl;
    }
}

void PPU::cleanup() {
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
    
    // Log scanline progress every 1000 steps for debugging
    if (stepCount++ % 1000 == 0 && logCount < 1000) {
        std::ostringstream oss;
        oss << "[PPU-STEP] Step=" << std::dec << stepCount
            << " Dot=" << m_dot
            << " Scanline=" << m_scanline
            << " CPU_Cycles=" << (m_cpu ? m_cpu->getCycles() : 0);
        Logger::getInstance().logPPU(oss.str());
        logCount++;
    }
    
    if (m_dot >= 341) {
        m_dot = 0;
        
        // Render scanline when it completes
        if (m_scanline < SCREEN_HEIGHT) {
            renderScanline();
            
            // Log key scanlines
            /*if (logCount < 500 && (m_scanline % 32 == 0 || m_scanline == 0)) {
                std::ostringstream oss;
                oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                    << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                    << "Scanline:" << std::setw(3) << m_scanline << " | "
                    << "Event: Rendering | "
                    << "BGMode:" << (int)m_bgMode << " | "
                    << "Brightness:" << (int)m_brightness << " | "
                    << "ForcedBlank:" << (m_forcedBlank ? "ON" : "OFF");
                Logger::getInstance().logPPU(oss.str());
                logCount++;
            }*/
        }
        
        m_scanline++;
    
    // V-Blank start at scanline 225 (0xE1)
    if (m_scanline == 225) {
        m_nmiFlag = true;  // Set NMI flag when VBlank starts
        
        // V-Blank start logging disabled to reduce log spam
        // if (logCount < 500) {
        //     std::ostringstream oss;
        //     oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
        //         << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
        //         << "Scanline:" << std::setw(3) << m_scanline << " | "
        //         << "Event: V-Blank Start | "
        //         << "NMI:" << (m_nmiEnabled ? "Enabled" : "Disabled");
        //     Logger::getInstance().logPPU(oss.str());
        //     logCount++;
        // }
        
        // Trigger NMI if enabled
        /*static int forceNmiCount = 0;
        if (forceNmiCount < 5) {
            if (forceNmiCount == 0) {
                std::cout << "PPU: FORCING NMI for testing (game hasn't enabled it)" << std::endl;
            }
            forceNmiCount++;
        }*/
        
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
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        uint32_t mainColor = 0xFF000000; // Default to black
        uint32_t subColor = 0xFF000000;
        
        // Render background layers for Main Screen
        if (m_bgMode == 0) {
            // Mode 0: 4 layers of 2bpp tiles
            mainColor = renderBackgroundMode0(x);
        } else if (m_bgMode == 1) {
            // Mode 1: BG1/BG2 4bpp, BG3 2bpp
            mainColor = renderBackgroundMode1(x);
        } else {
            // Other modes: simple test pattern
            mainColor = renderTestPattern(x);
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
            uint8_t r = (finalColor & 0xFF);
            uint8_t g = ((finalColor >> 8) & 0xFF);
            uint8_t b = ((finalColor >> 16) & 0xFF);
            
            r = (r * m_brightness) / 15;
            g = (g * m_brightness) / 15;
            b = (b * m_brightness) / 15;
            
            finalColor = (0xFF << 24) | (b << 16) | (g << 8) | r;
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
// This function uses corrected 2bpp tile decoding logic.
// tileX, tileY, pixelX, pixelY are already scrolled coordinates from the caller
PixelInfo PPU::renderBGx(int bgIndex, int tileX, int tileY, int pixelX, int pixelY) {
    const int TILE_SIZE_BYTES = 16;
    
    // Get scroll values for fine scrolling calculation
    int scrollX, scrollY;
    switch (bgIndex) {
        case 0: scrollX = m_bg1ScrollX; scrollY = m_bg1ScrollY; break;
        case 1: scrollX = m_bg2ScrollX; scrollY = m_bg2ScrollY; break;
        case 2: scrollX = m_bg3ScrollX; scrollY = m_bg3ScrollY; break;
        case 3: scrollX = m_bg4ScrollX; scrollY = m_bg4ScrollY; break;
        default: scrollX = 0; scrollY = 0; break;
    }
    
    // Handle tilemap wrapping (32x32 or 64x64 tilemap)
    int tilemapWidth = m_bgMapSize[bgIndex] ? 64 : 32;
    int wrappedTileX = tileX & (tilemapWidth - 1);  // Wrap to tilemap width
    int wrappedTileY = tileY & (tilemapWidth - 1);  // Wrap to tilemap height
    
    // 1. Calculate tilemap address (32x32 or 64x64 tilemap)
    uint16_t mapAddr = m_bgMapAddr[bgIndex] + (wrappedTileY * tilemapWidth + wrappedTileX) * 2;
    
#ifdef DEBUG_PPU_RENDER
    static bool isPixelTransparent[4] = {false, false, false, false};
    if(m_scanline == 0 && tileX == 0 && pixelX == 0) {
        if(isPixelTransparent[bgIndex] == false){
            std::ostringstream oss;
            oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                << "BGIdx:" << bgIndex << ", No Valid Pixel Found at last BG "<< std::endl;
            Logger::getInstance().logPPU(oss.str());
        }
        isPixelTransparent[bgIndex] = false;
    }
#endif
    // VRAM boundary check
    if (mapAddr + 1 >= m_vram.size()) return {0, 0};
    
    // 2. Read tilemap entry
    uint16_t tileEntry = m_vram[mapAddr] | (m_vram[mapAddr + 1] << 8);
    
    // 3. Extract attributes
    uint16_t tileNumber = tileEntry & 0x03FF;
    uint8_t palette = (tileEntry >> 10) & (bgIndex < 2 ? 0x07 : 0x01); 
    bool hFlip = (tileEntry >> 14) & 1;
    bool vFlip = (tileEntry >> 15) & 1;
    uint8_t priorityGroup = (tileEntry >> 13) & 1; 
    
    // Apply fine scrolling to pixel coordinates
    int fineScrollX = scrollX & 7;
    int fineScrollY = scrollY & 7;
    
    int scrolledPixelX = pixelX - fineScrollX;
    int scrolledPixelY = pixelY - fineScrollY;
    
    // Handle pixel wrapping within tile
    if (scrolledPixelX < 0) scrolledPixelX += 8;
    if (scrolledPixelY < 0) scrolledPixelY += 8;
    
    // 4. Apply flip
    int finalX = hFlip ? (7 - scrolledPixelX) : scrolledPixelX;
    int finalY = vFlip ? (7 - scrolledPixelY) : scrolledPixelY;
    
    // 5. Calculate tile data address (2bpp = 16 bytes/tile)
    uint16_t tileAddr = m_bgTileAddr[bgIndex] + tileNumber * TILE_SIZE_BYTES;
    
    // VRAM boundary check
    if (tileAddr + TILE_SIZE_BYTES > m_vram.size()) return {0, 0};
    
    // 6. Decode 2bpp tile data (corrected logic applied)
    int line_offset = finalY; 
    
    uint8_t plane0_byte = m_vram[tileAddr + line_offset];
    uint8_t plane1_byte = m_vram[tileAddr + line_offset + 8];

    int bitPos = 7 - finalX; 

    uint8_t pixelIndex = ((plane0_byte >> bitPos) & 1) | 
                         (((plane1_byte >> bitPos) & 1) << 1); 
    
    // 7. Determine transparency and final color/priority
    if (pixelIndex == 0) {
        return {0, 0}; // Transparent (background color)
    }
    
    // --- DEBUG DUMP LOGIC START ---
#ifdef DEBUG_PPU_RENDER
    if (!isPixelTransparent[bgIndex]) {
        isPixelTransparent[bgIndex] = true;
        uint16_t base_color_index = palette * 4; 
        uint16_t cgram_addr = (base_color_index + pixelIndex) * 2; 

        static int dump_count = 0;
        if (dump_count < 10000) {
            std::ostringstream oss;
            oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
            << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
            << "--- MEM DUMP: BG" << bgIndex <<" Pixel (" << m_scanline << "," << pixelY << "," << pixelY << ") ---" << std::endl;
            
            // 1. Tilemap Entry Dump (VRAM: CHR/ATTR)
            oss << "  [Tilemap (CHR/ATTR)] VRAM Addr: 0x" << std::hex << mapAddr << " (2 bytes)" << std::endl;
            oss << "    Entry: 0x" << tileEntry << " -> Tile #" << tileNumber << ", Palette #" << (int)palette << ", PriGrp " << (int)priorityGroup << std::dec << std::endl;

            oss << "  [Tile Data (Pattern)] VRAM Addr: 0x" << std::hex << tileAddr << " (16 bytes base)" << std::dec << std::endl;
            oss << "    Line " << finalY << " (Offset +" << line_offset << "): Plane0=0x" << std::hex << (int)plane0_byte << ", Plane1=0x" << (int)plane1_byte << std::dec << std::endl;
            oss << "    Pixel Index (7-" << finalX << " bit): " << (int)pixelIndex << std::endl;

            // 3. Palette Data Dump (CGRAM)
            oss << "  [Palette (CGRAM)] CGRAM Addr: 0x" << std::hex << cgram_addr << " (2 bytes)" << std::dec << std::endl;
            if (cgram_addr + 1 < m_cgram.size()) {
                uint8_t cgram_low = m_cgram[cgram_addr];
                uint8_t cgram_high = m_cgram[cgram_addr + 1];
                uint16_t cgram_entry = cgram_low | (cgram_high << 8);
                oss << "    CGRAM[0x" << std::hex << cgram_addr << "]=" << (int)cgram_low << ", [0x" << cgram_addr + 1 << "]=" << (int)cgram_high << std::dec << std::endl;
                oss << "    15-bit Color: 0x" << std::hex << cgram_entry << std::dec << std::endl;
            }
            Logger::getInstance().logPPU(oss.str());
            dump_count++;
        }
    }
#endif
    // --- DEBUG DUMP LOGIC END ---
    
    uint32_t color = getColor(palette, pixelIndex);
    
    uint8_t priority = m_bgPriority[bgIndex][priorityGroup]; 

    return {color, priority};
}

// Main rendering function
uint32_t PPU::renderBackgroundMode0(int x) {
    int y = m_scanline; // Current scanline (m_scanline is a PPU member variable)
    
    // Store pixel information for 4 layers
    PixelInfo bgPixels[4]; // BG1, BG2, BG3, BG4

    // 1. Calculate pixel information for each background layer
    // Each BG has its own scroll values
    // BG1 (Index 0)
    {
        int bgX = x + m_bg1ScrollX;
        int bgY = y + m_bg1ScrollY;
        int tileX = bgX / 8;
        int tileY = bgY / 8;
        int pixelX = bgX % 8;
        int pixelY = bgY % 8;
        bgPixels[0] = renderBGx(0, tileX, tileY, pixelX, pixelY);
    }
    
    // BG2 (Index 1)
    {
        int bgX = x + m_bg2ScrollX;
        int bgY = y + m_bg2ScrollY;
        int tileX = bgX / 8;
        int tileY = bgY / 8;
        int pixelX = bgX % 8;
        int pixelY = bgY % 8;
        bgPixels[1] = renderBGx(1, tileX, tileY, pixelX, pixelY);
    }
    
    // BG3 (Index 2)
    {
        int bgX = x + m_bg3ScrollX;
        int bgY = y + m_bg3ScrollY;
        int tileX = bgX / 8;
        int tileY = bgY / 8;
        int pixelX = bgX % 8;
        int pixelY = bgY % 8;
        bgPixels[2] = renderBGx(2, tileX, tileY, pixelX, pixelY);
    }
    
    // BG4 (Index 3)
    {
        int bgX = x + m_bg4ScrollX;
        int bgY = y + m_bg4ScrollY;
        int tileX = bgX / 8;
        int tileY = bgY / 8;
        int pixelX = bgX % 8;
        int pixelY = bgY % 8;
        bgPixels[3] = renderBGx(3, tileX, tileY, pixelX, pixelY);
    }

    // 2. Check Main Screen Designation (TM register) - only render enabled BGs
    // Bit 0 = BG1, Bit 1 = BG2, Bit 2 = BG3, Bit 3 = BG4
    
    // 3. Determine pixel priority (select pixel with highest priority)
    // Sprite rendering results should also be considered, but here we only handle backgrounds.
    
    uint32_t finalColor = 0;
    uint8_t maxPriority = 0;

    for (int i = 0; i < 4; ++i) {
        // Check if this BG is enabled in Main Screen Designation
        if (!(m_mainScreenDesignation & (1 << i))) {
            continue; // Skip disabled BG
        }
        
        if (bgPixels[i].color != 0) { // Only consider non-transparent pixels
            if (bgPixels[i].priority > maxPriority) {
                maxPriority = bgPixels[i].priority;
                finalColor = bgPixels[i].color;
            }
        }
    }

    // If no BG pixel was found, use background color (CGRAM[0])
    if (finalColor == 0) {
        uint16_t bgColor = m_cgram[0] | (m_cgram[1] << 8);
        if (bgColor == 0) {
            // Default to black if CGRAM[0] is not set
            return 0xFF000000;
        }
        // Extract RGB components (5 bits each)
        uint8_t r = (bgColor & 0x1F) << 3;
        uint8_t g = ((bgColor >> 5) & 0x1F) << 3;
        uint8_t b = ((bgColor >> 10) & 0x1F) << 3;
        // Convert to 32-bit RGBA
        return (0xFF << 24) | (b << 16) | (g << 8) | r;
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
    bgPixels[0] = renderBGx(0, bg1TileX, bg1TileY, bg1PixelX, bg1PixelY);
    
    // BG2 (4bpp)
    bgPixels[1] = renderBGx(1, bg2TileX, bg2TileY, bg2PixelX, bg2PixelY);
    
    // BG3 (2bpp) - need special handling
    bgPixels[2] = renderBG3Pixel(bg3TileX * 8 + bg3PixelX, bg3TileY * 8 + bg3PixelY);
    
    // Determine pixel priority (select pixel with highest priority)
    uint32_t finalColor = 0;
    uint8_t maxPriority = 0;
    
    for (int i = 0; i < 3; ++i) {
        if (bgPixels[i].color != 0) { // Only consider non-transparent pixels
            if (bgPixels[i].priority > maxPriority) {
                maxPriority = bgPixels[i].priority;
                finalColor = bgPixels[i].color;
            }
        }
    }
    
    return finalColor;
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
        return 0xFF000000; // Black
    }
    
    uint16_t snesColor = m_cgram[cgramIndex] | (m_cgram[cgramIndex + 1] << 8);
    
    // If CGRAM is empty (all zeros), use test colors
    if (snesColor == 0) {
        if (paletteIndex == 0) return 0xFF0000FF; // Blue
        if (paletteIndex == 1) return 0xFF00FF00; // Green
        if (paletteIndex == 2) return 0xFFFF0000; // Red
        if (paletteIndex == 6) return 0xFFFF00FF; // Magenta
        return 0xFF000000; // Black
    }
    
    // Extract RGB components (5 bits each)
    uint8_t r = (snesColor & 0x1F) << 3;
    uint8_t g = ((snesColor >> 5) & 0x1F) << 3;
    uint8_t b = ((snesColor >> 10) & 0x1F) << 3;
    
    // Convert to 32-bit RGBA 
    // SDL_PIXELFORMAT_RGBA8888 format: 0xAABBGGRR (little-endian)
    return (0xFF << 24) | (b << 16) | (g << 8) | r;
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
    
    uint16_t tileAddr = m_bg1TileAddr + tileNumber * 32;
    
    if (tileAddr + 32 > m_vram.size()) {
        std::cout << "DEBUG_RENDER: FAIL - Tile data address out of bounds for tile number " << tileNumber << "! tileAddr=0x" << std::hex << tileAddr << std::dec << std::endl;
        return 0;
    }
    
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

    // --- 3. Transparency Check and Successful Render Log ---
    if (pixelIndex != 0) { // Not transparent
        // Successful rendering condition met
        std::cout << "DEBUG_RENDER: SUCCESS - Rendered pixel at (x=" << x << ", y=" << y << ") | Tile: " << tileNumber
                  << ", Palette: " << (int)palette << ", Index: " << (int)pixelIndex << std::endl;
        return getColor(palette, pixelIndex);
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
            // Mode 7 not implemented yet
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
            m_bg1TileAddr = (value & 0x0F) << 12;
            m_bg2TileAddr = ((value & 0xF0) >> 4) << 12;
            m_bgTileAddr[0] = m_bg1TileAddr; // Sync array
            m_bgTileAddr[1] = m_bg2TileAddr; // Sync array
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
            m_bg3TileAddr = (value & 0x0F) << 12;
            m_bg4TileAddr = ((value & 0xF0) >> 4) << 12;
            m_bgTileAddr[2] = m_bg3TileAddr; // Sync array
            m_bgTileAddr[3] = m_bg4TileAddr; // Sync array
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
            if (m_scrollLatchY) {
                m_bg1ScrollY = (m_bg1ScrollY & 0x00FF) | (value << 8);
            } else {
                m_bg1ScrollY = (m_bg1ScrollY & 0xFF00) | value;
            }
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
            if (tmCount < 3) {
                std::cout << "PPU: TM (Main Screen)=$" << std::hex << (int)value << std::dec << std::endl;
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
    
    return 0xFF000000; // Black
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
    
    // Wrap around at 64KB boundary
    if (m_vramAddress >= 65536) {
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
