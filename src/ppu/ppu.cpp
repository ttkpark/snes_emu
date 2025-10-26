#include "ppu.h"
#include "../cpu/cpu.h"
#include "../debug/logger.h"
#include <cstring>
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
    
    // Increment dot counter (341 dots per scanline)
    m_dot++;
    
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
        m_nmiFlag = true;  // Set NMI flag
        
        // Log V-Blank start
        if (logCount < 500) {
            std::ostringstream oss;
            oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
                << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                << "Scanline:" << std::setw(3) << m_scanline << " | "
                << "Event: V-Blank Start | "
                << "NMI:" << (m_nmiEnabled ? "Enabled" : "Disabled");
            Logger::getInstance().logPPU(oss.str());
            logCount++;
        }
        
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
    
    }
    
    // V-Blank period (scanlines 225-261)
    if (m_scanline >= 262) {
        m_scanline = 0;
        m_dot = 0;
        m_frameReady = true;
        m_nmiFlag = false;  // Clear NMI flag at frame start
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
    // Debug: Print when this is called
    static int callCount = 0;
    if (callCount < 5) {
        std::cout << "renderScanline() called for scanline " << m_scanline << std::endl;
        callCount++;
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
        uint32_t pixelColor = 0xFF000000; // Default to black
        
        // Render background layers
        if (m_bgMode == 0) {
            // Mode 0: 4 layers of 2bpp tiles
            pixelColor = renderBackgroundMode0(x);
        } else if (m_bgMode == 1) {
            // Mode 1: BG1/BG2 4bpp, BG3 2bpp
            pixelColor = renderBackgroundMode1(x);
        } else {
            // Other modes: simple test pattern
            pixelColor = renderTestPattern(x);
        }
        
        // Debug: Print first few pixels
        /*if (x < 5 && m_scanline == 0) {
            std::cout << "    renderScanline: x=" << x << " bgMode=" << (int)m_bgMode << " pixelColor=0x" << std::hex << pixelColor << std::dec << std::endl;
        }*/
        
        // Apply brightness
        if (m_brightness < 15) {
            uint8_t r = (pixelColor & 0xFF);
            uint8_t g = ((pixelColor >> 8) & 0xFF);
            uint8_t b = ((pixelColor >> 16) & 0xFF);
            
            r = (r * m_brightness) / 15;
            g = (g * m_brightness) / 15;
            b = (b * m_brightness) / 15;
            
            pixelColor = (0xFF << 24) | (b << 16) | (g << 8) | r;
        }
        
        m_framebuffer[m_scanline * SCREEN_WIDTH + x] = pixelColor;
    }
    
    // Debug: Print first few pixels of first scanline
    if (m_scanline == 0 && callCount <= 5) {
        std::cout << "  First 5 pixels: ";
        for (int i = 0; i < 5; i++) {
            std::cout << std::hex << "0x" << m_framebuffer[i] << " ";
        }
        std::cout << std::dec << std::endl;
    }
    
    #ifdef USE_SDL
    Uint32* pixels = NULL;
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
    SDL_RenderCopy(m_renderer, m_texture, NULL, NULL); // 전체 화면에 텍스처를 그림
    SDL_RenderPresent(m_renderer);
    #endif
}

void PPU::renderBackground() {
    // TODO: Implement proper background rendering
    // For now, just fill with background color
}

uint32_t PPU::renderBackgroundMode0(int x) {
    // Debug: Print when function is called
    if (x < 5 && m_scanline == 0) {
        std::cout << "    renderBackgroundMode0() called for x=" << x << std::endl;
    }
    
    // Mode 0: 4 layers of 2bpp tiles
    // Render actual SNES background from VRAM
    
    // Calculate tile coordinates (8x8 tiles)
    int tileX = x / 8;
    int tileY = m_scanline / 8;
    
    // Calculate pixel within tile
    int pixelX = x % 8;
    int pixelY = m_scanline % 8;
    
    // Get tile data from VRAM
    // BG1 tile map starts at m_bg1MapAddr
    uint16_t mapAddr = m_bg1MapAddr + (tileY * 32 + tileX) * 2;
    
    // Read tile map entry (16-bit)
    uint16_t tileEntry = 0;
    if (mapAddr < m_vram.size() - 1) {
        tileEntry = m_vram[mapAddr] | (m_vram[mapAddr + 1] << 8);
    }
    
    // Extract tile number and attributes
    uint16_t tileNumber = tileEntry & 0x03FF;
    uint8_t palette = (tileEntry >> 10) & 0x07;
    bool hFlip = (tileEntry >> 14) & 1;
    bool vFlip = (tileEntry >> 15) & 1;
    
    // Apply flipping
    if (hFlip) pixelX = 7 - pixelX;
    if (vFlip) pixelY = 7 - pixelY;
    
    // Get tile data address (2bpp = 32 bytes per tile)
    uint16_t tileAddr = m_bg1TileAddr + tileNumber * 32;
    
    // Read tile data
    uint8_t tileData[32];
    for (int i = 0; i < 32; i++) {
        if (tileAddr + i < m_vram.size()) {
            tileData[i] = m_vram[tileAddr + i];
        } else {
            tileData[i] = 0;
        }
    }
    
    // Decode 2bpp tile pixel
    uint8_t pixelValue = 0;
    if (pixelY < 8 && pixelX < 8) {
        uint8_t plane0 = tileData[pixelY * 4 + pixelX / 4];
        uint8_t plane1 = tileData[pixelY * 4 + pixelX / 4 + 16];
        
        // Extract 2-bit pixel value
        int bitPos = (pixelX % 4) * 2;
        pixelValue = ((plane0 >> bitPos) & 1) | (((plane1 >> bitPos) & 1) << 1);
    }
    
    // Get color from palette
    uint32_t color = getColor(palette, pixelValue);
    
    // Debug: Print first few pixels
    if (x < 5 && m_scanline == 0) {
        std::cout << "    renderBackgroundMode0: x=" << x << " tileNumber=" << tileNumber 
                  << " palette=" << (int)palette << " pixelValue=" << (int)pixelValue 
                  << " color=0x" << std::hex << color << std::dec << std::endl;
        std::cout << "    VRAM tile data: ";
        for (int i = 0; i < 8; i++) {
            std::cout << std::hex << (int)tileData[i] << " ";
        }
        std::cout << std::dec << std::endl;
    }
    
    return color;
}

uint32_t PPU::renderBackgroundMode1(int x) {
    // Debug: Print when function is called
    if (x < 5 && m_scanline == 0) {
        std::cout << "    renderBackgroundMode1() called for x=" << x << std::endl;
    }
    
    // Mode 1: BG1/BG2 4bpp, BG3 2bpp
    // For now, render a simple test pattern
    return renderTestPattern(x);
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
    // SNES uses 15-bit color (5 bits per channel)
    // CGRAM stores colors as little-endian 16-bit values
    // Each palette has 16 colors (for 4bpp), starting at paletteIndex * 16
    uint16_t cgramIndex = (paletteIndex * 16 + colorIndex) * 2;
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
    
    // Get tilemap entry from VRAM
    uint16_t mapAddr = m_bg1MapAddr + (tileY * 32 + tileX) * 2; // 2 bytes per tilemap entry
    
    if (mapAddr + 1 >= m_vram.size()) return 0;
    
    // Read tilemap entry (16-bit)
    uint16_t tileEntry = m_vram[mapAddr] | (m_vram[mapAddr + 1] << 8);
    
    // Debug: Print first few tilemap entries
    static int tilemapDebugCount = 0;
    if (tilemapDebugCount < 10 && tileX < 5 && tileY < 5) {
        std::cout << "Tilemap[" << tileX << "," << tileY << "] at 0x" << std::hex << mapAddr 
                  << " = 0x" << tileEntry << std::dec << std::endl;
        tilemapDebugCount++;
    }
    
    // Extract tile number and attributes
    uint16_t tileNumber = tileEntry & 0x03FF;  // 10 bits for tile number
    uint8_t palette = (tileEntry >> 10) & 0x07; // 3 bits for palette
    bool hFlip = (tileEntry & 0x4000) != 0;    // Horizontal flip
    bool vFlip = (tileEntry & 0x8000) != 0;    // Vertical flip
    
    // Calculate tile data address
    uint16_t tileAddr = m_bg1TileAddr + tileNumber * 32; // 32 bytes per tile
    
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


void PPU::writeRegister(uint16_t address, uint8_t value) {
    std::ostringstream oss;
    oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0') 
        << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
        << "Scanline:" << std::setw(3) << m_scanline << " | "
        << "PPU Write: [$" << std::hex << (int)address << "] = $" << (int)value << std::dec << std::endl;
    Logger::getInstance().logPPU(oss.str());
    Logger::getInstance().flush();  // Flush at frame end

    switch (address) {
        case 0x2100: { // INIDISP - Screen Display
            m_brightness = value & 0x0F;
            m_forcedBlank = (value & 0x80) != 0;
            static int displayChangeCount = 0;
            if (displayChangeCount < 10) {
                std::cout << "PPU: INIDISP=$" << std::hex << (int)value << std::dec 
                          << " - Forced blank " << (m_forcedBlank ? "ON" : "OFF") 
                          << ", brightness=" << (int)m_brightness << std::endl;
                displayChangeCount++;
            }
            

            break;
        }
            
        case 0x4200: { // NMITIMEN - Interrupt Enable
            m_nmiEnabled = (value & 0x80) != 0;
            static int nmiEnableCount = 0;
            if (nmiEnableCount < 3) {
                std::cout << "PPU: NMI " << (m_nmiEnabled ? "ENABLED" : "DISABLED") << std::endl;
                nmiEnableCount++;
            }
            break;
        }
            
        case 0x420B: { // MDMAEN - DMA Enable
            // DMA not implemented yet, just acknowledge the write
            static int dmaEnCount = 0;
            if (dmaEnCount < 3) {
                std::cout << "PPU: DMA Enable=$" << std::hex << (int)value << std::dec << std::endl;
                dmaEnCount++;
            }
            break;
        }
            
        case 0x420C: { // HDMAEN - HDMA Enable
            // HDMA not implemented yet, just acknowledge the write
            static int hdmaEnCount = 0;
            if (hdmaEnCount < 3) {
                std::cout << "PPU: HDMA Enable=$" << std::hex << (int)value << std::dec << std::endl;
                hdmaEnCount++;
            }
            break;
        }
            
        case 0x2101: { // OBSEL - Object Size and Base Address
            m_objSize = value;
            static int obselCount = 0;
            if (obselCount < 3) {
                std::cout << "PPU: OBSEL=$" << std::hex << (int)value << std::dec << std::endl;
                obselCount++;
            }
            break;
        }
            
        case 0x2105: { // BGMODE - BG Mode and Character Size
            m_bgMode = value & 0x07;
            static int bgModeCount = 0;
            if (bgModeCount < 10) {
                std::cout << "PPU: BGMODE=$" << std::hex << (int)value << std::dec 
                          << " - BG Mode=" << (int)m_bgMode << std::endl;
                bgModeCount++;
            }
            break;
        }
            
        case 0x2106: // MOSAIC - Mosaic Size and Enable
            // Ignore for now
            break;
            
        case 0x2116: { // VMADDL - VRAM Address Low
            m_vramAddress = (m_vramAddress & 0xFF00) | value;
            break;
        }
            
        case 0x2117: { // VMADDH - VRAM Address High
            m_vramAddress = (m_vramAddress & 0x00FF) | (value << 8);
            static int vramAddrCount = 0;
            if (vramAddrCount < 3) {
                std::cout << "PPU: VRAM Address=0x" << std::hex << m_vramAddress << std::dec << std::endl;
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
                std::cout << "PPU: CGRAM Address=0x" << std::hex << (int)m_cgramAddress << std::dec << std::endl;
                cgaddCount++;
            }
            break;
        }
            
        case 0x2122: { // CGDATA - CGRAM Data
            writeCGRAM(m_cgramAddress, value);
            m_cgramAddress++;
            m_cgramAddress &= 0x01FF;
            if (m_cgramAddress <= 10) {
                std::cout << "CGRAM[" << std::dec << (m_cgramAddress - 1) 
                          << "] = 0x" << std::hex << (int)value << std::dec << std::endl;
            }
            break;
        }
            
        case 0x2107: { // BG1SC - BG1 Tilemap Address
            m_bg1MapAddr = (value & 0xFC) << 8;
            static int bg1MapCount = 0;
            if (bg1MapCount < 3) {
                std::cout << "PPU: BG1SC=$" << std::hex << (int)value << std::dec 
                          << " - BG1 Map Addr=0x" << std::hex << m_bg1MapAddr << std::dec << std::endl;
                bg1MapCount++;
            }
            break;
        }
            
        case 0x2108: // BG2SC - BG2 Tilemap Address
            m_bg2MapAddr = (value & 0xFC) << 8;
            break;
            
        case 0x2109: // BG3SC - BG3 Tilemap Address
            m_bg3MapAddr = (value & 0xFC) << 8;
            break;
            
        case 0x210A: // BG4SC - BG4 Tilemap Address
            m_bg4MapAddr = (value & 0xFC) << 8;
            break;
            
        case 0x210B: { // BG12NBA - BG1 and BG2 Tile Data Address
            m_bg1TileAddr = (value & 0x0F) << 12;
            m_bg2TileAddr = ((value & 0xF0) >> 4) << 12;
            static int bg12nbaCount = 0;
            if (bg12nbaCount < 10) {
                std::cout << "PPU: BG12NBA=$" << std::hex << (int)value << std::dec 
                          << " - BG1 tiles at 0x" << std::hex << m_bg1TileAddr 
                          << ", BG2 tiles at 0x" << m_bg2TileAddr << std::dec << std::endl;
                bg12nbaCount++;
            }
            break;
        }
            
        case 0x210C: { // BG34NBA - BG3 and BG4 Tile Data Address
            m_bg3TileAddr = (value & 0x0F) << 12;
            m_bg4TileAddr = ((value & 0xF0) >> 4) << 12;
            break;
        }
            
        case 0x210D: { // BG1HOFS - BG1 Horizontal Scroll
            // Scroll registers use a write-twice mechanism
            m_bg1ScrollX = (m_bg1ScrollX & 0xFF00) | m_scrollPrevX;
            m_scrollPrevX = value;
            m_scrollLatchX = !m_scrollLatchX;
            static int bg1HofsCount = 0;
            if (bg1HofsCount < 3) {
                std::cout << "PPU: BG1HOFS=$" << std::hex << (int)value << ", ScrollX=0x" << m_bg1ScrollX << std::dec << std::endl;
                bg1HofsCount++;
            }
            break;
        }
            
        case 0x210E: { // BG1VOFS - BG1 Vertical Scroll
            m_bg1ScrollY = (m_bg1ScrollY & 0xFF00) | m_scrollPrevY;
            m_scrollPrevY = value;
            m_scrollLatchY = !m_scrollLatchY;
            break;
        }
            
        case 0x210F: // BG2HOFS - BG2 Horizontal Scroll
            m_bg2ScrollX = (m_bg2ScrollX & 0xFF00) | m_scrollPrevX;
            m_scrollPrevX = value;
            break;
            
        case 0x2110: // BG2VOFS - BG2 Vertical Scroll
            m_bg2ScrollY = (m_bg2ScrollY & 0xFF00) | m_scrollPrevY;
            m_scrollPrevY = value;
            break;
            
        case 0x2111: // BG3HOFS - BG3 Horizontal Scroll
            m_bg3ScrollX = (m_bg3ScrollX & 0xFF00) | m_scrollPrevX;
            m_scrollPrevX = value;
            break;
            
        case 0x2112: // BG3VOFS - BG3 Vertical Scroll
            m_bg3ScrollY = (m_bg3ScrollY & 0xFF00) | m_scrollPrevY;
            m_scrollPrevY = value;
            break;
            
        case 0x2113: // BG4HOFS - BG4 Horizontal Scroll
            m_bg4ScrollX = (m_bg4ScrollX & 0xFF00) | m_scrollPrevX;
            m_scrollPrevX = value;
            break;
            
        case 0x2114: // BG4VOFS - BG4 Vertical Scroll
            m_bg4ScrollY = (m_bg4ScrollY & 0xFF00) | m_scrollPrevY;
            m_scrollPrevY = value;
            break;
            
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
            
        case 0x212D: // TS - Sub Screen Designation
            m_subScreenDesignation = value;
            break;
            
        case 0x212E: // TMW - Window Mask for Main Screen
        case 0x212F: // TSW - Window Mask for Sub Screen
        case 0x2123: // W12SEL - Window Mask Settings for BG1 and BG2
        case 0x2124: // W34SEL - Window Mask Settings for BG3 and BG4
        case 0x2125: // WOBJSEL - Window Mask Settings for OBJ and Color Window
        case 0x2126: // WH0 - Window 1 Left Position
        case 0x2127: // WH1 - Window 1 Right Position
        case 0x2128: // WH2 - Window 2 Left Position
        case 0x2129: // WH3 - Window 2 Right Position
        case 0x212A: // WBGLOG - Window Mask Logic for BGs
        case 0x212B: // WOBJLOG - Window Mask Logic for OBJs
            // Ignore window settings for now
            break;
            
        case 0x2130: // CGWSEL - Color Math Control
        case 0x2131: // CGADSUB - Color Math Settings
            m_colorMath = value;
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
        case 0x2137: // SLHV - Software Latch for H/V Counter
            return 0;
            
        case 0x213F: // STAT78 - PPU Status Flag and Version
            return 0x01; // Version 1
            
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
            if (m_nmiFlag) {
                result |= 0x80;  // Set NMI flag
                m_nmiFlag = false;  // Clear on read
            }
            return result;
        }
            
        default:
            return 0;
    }
}

void PPU::writeVRAM(uint16_t address, uint8_t value) {
    if (address < m_vram.size()) {
        m_vram[address] = value;
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
    
    // For now, implement a simple sprite rendering
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
        
        // Check if this pixel is within the sprite bounds
        // SNES sprites are typically 8x8 or 16x16 pixels
        int spriteWidth = 8;  // Default 8x8
        int spriteHeight = 8;
        
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
                
                // Get tile data address (simplified - assume tiles start at 0x0000)
                uint16_t tileAddr = tileNumber * 32; // 32 bytes per 8x8 tile
                
                if (tileAddr + 32 <= m_vram.size()) {
                    // Decode tile
                    uint8_t tileData[32];
                    for (int i = 0; i < 32; i++) {
                        tileData[i] = m_vram[tileAddr + i];
                    }
                    
                    uint8_t pixels[64];
                    decodeTile(tileData, pixels, 4);
                    
                    uint8_t pixelIndex = pixels[pixelY * 8 + pixelX];
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
    if (m_vramAddress >= 0x10000) {
        m_vramAddress &= 0xFFFF;
    }
}

void PPU::loadROMData(const std::vector<uint8_t>& romData) {
    std::cout << "Loading ROM data into VRAM..." << std::endl;
    std::cout << "ROM size: " << romData.size() << " bytes" << std::endl;
    std::cout << "VRAM size: " << m_vram.size() << " bytes" << std::endl;
    
    // DO NOT copy ROM into VRAM automatically!
    // VRAM should only be written by CPU through DMA or direct writes to $2118/2119
    // The ROM data will be loaded by the game's initialization code via DMA
    std::cout << "VRAM initialized to zero, waiting for CPU to load graphics via DMA" << std::endl;
    
    // Verify first few bytes
    std::cout << "First 16 VRAM bytes: ";
    for (int i = 0; i < 16; i++) {
        std::cout << std::hex << (int)m_vram[i] << " ";
    }
    std::cout << std::dec << std::endl;
    
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
    
    std::cout << "Initialized CGRAM with test colors" << std::endl;
}
