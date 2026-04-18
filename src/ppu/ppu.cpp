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

// Forward declaration of static OPT helper (defined later in this file)
static void getOPTOffsets(const std::vector<uint8_t>& vram,
                           uint16_t bg3MapAddr, uint8_t bg3MapSize,
                           int screenX, int screenY, int bgTileSize,
                           int& outOffsetX, int& outOffsetY);

// Forward declaration of static backdrop helper
static uint32_t getBGColorFromCGRAM(const std::vector<uint8_t>& cgram);

PPU::PPU() 
    : m_cpu(nullptr)
    , m_brightness(15)
    , m_bgMode(0)
    , m_forcedBlank(false)
    , m_nmiEnabled(false)
    , m_nmiFlag(false)
    , m_nmiAlreadyFiredThisVBlank(false)
    , m_timeOver(false)
    , m_rangeOver(false)
    , m_fieldBit(false)
    , m_irqMode(0)
    , m_htimer(0x01FF)
    , m_vtimer(0x01FF)
    , m_irqFlag(false)
    , m_irqFiredThisDot(false)
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

    , m_scrollPrevX(0)

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
    , m_coldata(0)
    , m_coldataR(0)
    , m_coldataG(0)
    , m_coldataB(0)
    , m_setini(0)
    , m_m7hofs_prev(0)
    , m_m7vofs_prev(0)
    , m_m7hofs(0)
    , m_m7vofs(0)
    , m_m7sel(0)
    , m_m7a(0x0100)  // Identity matrix: A=1.0, D=1.0
    , m_m7b(0)
    , m_m7c(0)
    , m_m7d(0x0100)
    , m_m7x(0)
    , m_m7y(0)
    , m_m7Latch(0)
    , m_m7aLatch(false), m_m7bLatch(false), m_m7cLatch(false), m_m7dLatch(false), m_m7xLatch(false), m_m7yLatch(false)
    , m_m7aPrev(0), m_m7bPrev(0), m_m7cPrev(0), m_m7dPrev(0), m_m7xPrev(0), m_m7yPrev(0)
    , m_objSize(0)
    , m_vramAddress(0)
    , m_vramIncrement(0)
    , m_vramMapping(0)
    , m_vramReadBuffer(0)
    , m_vramReadBufferH(0)
    , m_vramIncrAfterHigh(false)
    , m_cgramAddress(0)
    , m_oamAddress(0)
    , m_oamLatchByte(0)
    , m_oamLatchValid(false)
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
    // Initialize all 128 sprites as off-screen (Y=0xEC=236 puts sprite below visible area)
    for (int i = 0; i < 128; i++) {
        m_oam[i * 4 + 1] = 0xEC;  // Y position byte
    }
    m_framebuffer = new uint32_t[SCREEN_WIDTH * SCREEN_HEIGHT];
    m_tileCache2bpp.resize(m_vram.size() / 16);
    m_tileCache4bpp.resize(m_vram.size() / 32);
    m_tileCache8bpp.resize(m_vram.size() / 64);
    
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
    // Mode 0 composite priority order (higher = drawn on top):
    // SP3=15 > BG1hi=13 > BG2hi=12 > SP2=11 > BG1lo=9 > BG2lo=8 > SP1=7 > BG3hi=5 > BG4hi=4 > SP0=3 > BG3lo=2 > BG4lo=1 > BD=0
    m_bgPriority[0][0] = 9;   // BG1 low priority
    m_bgPriority[0][1] = 13;  // BG1 high priority
    m_bgPriority[1][0] = 8;   // BG2 low priority
    m_bgPriority[1][1] = 12;  // BG2 high priority
    m_bgPriority[2][0] = 2;   // BG3 low priority
    m_bgPriority[2][1] = 5;   // BG3 high priority
    m_bgPriority[3][0] = 1;   // BG4 low priority
    m_bgPriority[3][1] = 4;   // BG4 high priority
    
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
    // Our framebuffer stores pixels as (A<<24)|(B<<16)|(G<<8)|R in the uint32
    // = bytes [R, G, B, A] in memory (little-endian) = SDL_PIXELFORMAT_ABGR8888
    m_texture = SDL_CreateTexture(m_renderer, SDL_PIXELFORMAT_ABGR8888,
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
            // Initialize to black (RGBA8888 byte order: on little-endian, uint32 = 0xAABBGGRR)
            m_framebuffer[index] = 0xFF000000; // Black (opaque)
        }
    }

    m_videoInitialized = true;
    std::cout << "PPU: Video initialized - " << SCREEN_WIDTH << "x" << SCREEN_HEIGHT << std::endl;
    return true;
}
int frameCount = 0;

int PPU::getFrameCount() const { return frameCount; }

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
    // frameCount is declared globally above renderFrame() - do not shadow it
    static int logCount = 0;
    static bool warnedTiming = false;
    if (!warnedTiming) {
        std::cout << "[WARN] PPU timing is simplified (341 dots/scanline, 262 lines). Precise NTSC/PAL timing, H/V-blank DMA windows, and mid-scanline effects are not emulated yet." << std::endl;
        warnedTiming = true;
    }

    // Increment dot counter (341 dots per scanline)
    m_dot++;
    m_irqFiredThisDot = false;

    // H/V Timer IRQ check
    // Note: m_irqFlag persists until $4211 is read, but hardware re-fires the IRQ
    // line on each matching scanline/dot regardless.  We allow re-triggering per
    // scanline by checking !m_irqFiredThisDot to avoid double-firing within one dot.
    if (m_irqMode != 0 && !m_irqFiredThisDot) {
        bool hMatch = (m_dot == (int)m_htimer);
        bool vMatch = (m_scanline == (int)m_vtimer);
        bool fire = false;
        if (m_irqMode == 1 && hMatch)                fire = true; // H-IRQ: every scanline at htimer dot
        else if (m_irqMode == 2 && vMatch && m_dot == 0) fire = true; // V-IRQ: specified scanline, dot=0
        else if (m_irqMode == 3 && hMatch && vMatch)      fire = true; // H+V-IRQ: both match
        if (fire) {
            m_irqFlag = true;
            m_irqFiredThisDot = true;
            if (m_cpu) m_cpu->triggerIRQ();
        }
    }

    if (m_dot >= 341) {
        m_dot = 0;

        // Render scanline when it completes.
        // SNES hardware: scanline 0 is the overscan / first VBlank-ended line and does
        // NOT produce visible output (bsnes/ares skip vcounter=0). Visible scanlines are
        // 1..224, rendered into framebuffer rows 0..223. This matches the 1-line shift
        // observed against snes9x reference captures for TC-01 menu.
        if (m_scanline >= 1 && m_scanline <= SCREEN_HEIGHT) {
            renderScanline();
        }

        m_scanline++;

    // V-Blank start: scanline 225 normally, 240 when $2133 bit2 (OVERSCAN) is set
    {
        int vblankStart = getVBlankStart();
        if (m_scanline == vblankStart) {
            m_nmiFlag = true;  // Set NMI flag when VBlank starts
            m_nmiAlreadyFiredThisVBlank = false;  // Reset per-VBlank guard
            // Log NMI state at key frames only (avoid flood)
            if (frameCount < 5 || frameCount == 100 || frameCount == 200 ||
                frameCount == 400 || frameCount == 600 || frameCount == 1000 ||
                frameCount == 2000 || frameCount == 3000 || frameCount == 5000) {
                fprintf(stderr, "[VBLANK-NMI] F:%d scan=%d nmiEnabled=%d irqMode=%d\n",
                    frameCount, m_scanline, m_nmiEnabled, m_irqMode);
            }
            // Log all NMI enable/disable transitions
            static bool prevNmiLogged = false;
            if (m_nmiEnabled != prevNmiLogged) {
                uint32_t pc = m_cpu ? ((m_cpu->getPBR() << 16) | m_cpu->getPC()) : 0;
                fprintf(stderr, "[NMI-TRANSITION] F:%d %d->%d cpuPC=$%06X\n",
                    frameCount, prevNmiLogged, m_nmiEnabled, pc);
                prevNmiLogged = m_nmiEnabled;
            }
            // Sample CPU PC during stuck windows to identify wait loop
            if (frameCount == 200 || frameCount == 250 || frameCount == 300 ||
                frameCount == 400 || frameCount == 500 ||
                frameCount == 1400 || frameCount == 1410 || frameCount == 1415 ||
                frameCount == 1418 || frameCount == 1420 || frameCount == 1422 ||
                frameCount == 1425 || frameCount == 1430) {
                if (m_cpu) {
                    uint32_t pc = (m_cpu->getPBR() << 16) | m_cpu->getPC();
                    fprintf(stderr, "[CPU-PC-SAMPLE] F:%d pc=$%06X nmiEnabled=%d\n",
                        frameCount, pc, m_nmiEnabled);
                }
            }
            // Toggle interlace field bit each frame when $2133 bit0 (INTERLACE) is set
            if (m_setini & 0x01) {
                m_fieldBit = !m_fieldBit;
            }
            if ((m_nmiEnabled) && m_cpu) {
                m_cpu->triggerNMI();
                m_nmiAlreadyFiredThisVBlank = true;
            }
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
        m_nmiAlreadyFiredThisVBlank = false;  // Reset for new frame
        m_timeOver  = false; // Clear sprite overflow flags each frame
        m_rangeOver = false;
        frameCount++;
        
        // Dump VRAM after frame 15 (after NMI fills VRAM with data)
        static bool vramDumped = false;
        if (!vramDumped && frameCount >= 15) {
            dumpVRAMHex("vram_dump.txt");
            dumpCGRAM("cgram_dump.txt");
            vramDumped = true;
            std::cout << "PPU: VRAM and CGRAM dumps completed after frame " << frameCount << std::endl;
        }
        // Dump VRAM at multiple Character Test frames to see when tilemap content appears
        if (frameCount == 675 || frameCount == 685 || frameCount == 700 || frameCount == 750) {
            char fname[64];
            snprintf(fname, sizeof(fname), "vram_f%d.txt", frameCount);
            dumpVRAMHex(fname);
        }
        // --- TC-01 main menu diagnosis: CGRAM/VRAM dumps at critical frames ---
        // Captures state at: TC-01 main menu (60), TC-02 entry (120), TC-02 mid (200),
        // TC-03 (300), later screens (450, 600)
        {
            bool dumpNow = (frameCount == 60 || frameCount == 120 ||
                            frameCount == 200 || frameCount == 300 ||
                            frameCount == 450 || frameCount == 600 ||
                            frameCount == 1000 || frameCount == 1600 ||
                            frameCount == 2000);
            if (dumpNow) {
                char vfn[64], cfn[64];
                snprintf(vfn, sizeof(vfn), "vram_dump_f%03d.txt", frameCount);
                snprintf(cfn, sizeof(cfn), "cgram_dump_f%03d.txt", frameCount);
                dumpVRAMHex(vfn);
                dumpCGRAM(cfn);

                // Append compact summary to single rolling file for cross-frame comparison
                FILE* sum = fopen("ppu_diag_summary.txt", "a");
                if (sum) {
                    fprintf(sum, "=== Frame %d ===\n", frameCount);
                    fprintf(sum, "  Mode=%u BGmode=%u INIDISP=force%s bright=%u\n",
                            (unsigned)m_bgMode, (unsigned)m_bgMode,
                            m_forcedBlank ? "BLANK" : "ON",
                            (unsigned)m_brightness);
                    fprintf(sum, "  TM=$%02X TS=$%02X\n",
                            (unsigned)m_mainScreenDesignation,
                            (unsigned)m_subScreenDesignation);
                    for (int i = 0; i < 4; i++) {
                        fprintf(sum, "  BG%d: MapAddr(byte)=0x%04X TileAddr(byte)=0x%04X MapSize=%u TileSz=%s\n",
                                i + 1,
                                (unsigned)m_bgMapAddr[i],
                                (unsigned)m_bgTileAddr[i],
                                (unsigned)m_bgMapSize[i],
                                m_bgTileSize[i] ? "16x16" : "8x8");
                    }
                    fprintf(sum, "  CGRAM[0..7] bytes:");
                    for (int i = 0; i < 16; i++) fprintf(sum, " %02X", m_cgram[i]);
                    fprintf(sum, "\n  CGRAM[0] color word = 0x%04X (R=%u G=%u B=%u)\n",
                            (unsigned)(m_cgram[0] | (m_cgram[1] << 8)),
                            (unsigned)(m_cgram[0] & 0x1F),
                            (unsigned)(((m_cgram[0] | (m_cgram[1] << 8)) >> 5) & 0x1F),
                            (unsigned)(((m_cgram[0] | (m_cgram[1] << 8)) >> 10) & 0x1F));

                    // BG1 tilemap first row (32 tiles) — parsed
                    uint32_t base = m_bgMapAddr[0];
                    fprintf(sum, "  BG1 tilemap @0x%04X first row (32 tiles) parsed:\n", (unsigned)base);
                    for (int col = 0; col < 32; col++) {
                        uint32_t off = base + col * 2;
                        if (off + 1 >= m_vram.size()) break;
                        uint16_t entry = m_vram[off] | (m_vram[off + 1] << 8);
                        uint16_t tileNum = entry & 0x03FF;
                        uint8_t pal = (entry >> 10) & 0x07;
                        uint8_t pri = (entry >> 13) & 0x01;
                        uint8_t hf = (entry >> 14) & 0x01;
                        uint8_t vf = (entry >> 15) & 0x01;
                        fprintf(sum, "    [%2d] 0x%04X -> tile=%3u pal=%u pri=%u h=%u v=%u\n",
                                col, entry, tileNum, pal, pri, hf, vf);
                    }

                    // For Mode 0: BG1 uses palettes 0-7 (4 colors each, 2bpp).
                    // For Mode 1: BG1 uses palettes 0-7 (16 colors each, 4bpp).
                    // Log palettes 0..7 as reference:
                    fprintf(sum, "  CGRAM palettes 0..7 (first 16 entries each):\n");
                    for (int p = 0; p < 8; p++) {
                        fprintf(sum, "    pal%d:", p);
                        for (int k = 0; k < 16; k++) {
                            int idx = (p * 16 + k) * 2;
                            if (idx + 1 >= (int)m_cgram.size()) break;
                            uint16_t c = m_cgram[idx] | (m_cgram[idx + 1] << 8);
                            fprintf(sum, " %04X", c);
                        }
                        fprintf(sum, "\n");
                    }
                    fclose(sum);
                }

                // Mirror key fields to stderr for quick visibility
                fprintf(stderr, "[DIAG F%03d] Mode=%u TM=$%02X CGRAM[0]=0x%04X "
                                "BG1map=0x%04X BG1tile=0x%04X BG1scroll=(%d,%d)\n",
                        frameCount, (unsigned)m_bgMode,
                        (unsigned)m_mainScreenDesignation,
                        (unsigned)(m_cgram[0] | (m_cgram[1] << 8)),
                        (unsigned)m_bgMapAddr[0],
                        (unsigned)m_bgTileAddr[0],
                        (int)m_bg1ScrollX, (int)m_bg1ScrollY);

                // BG1 tilemap first row: raw + palette info to stderr
                {
                    uint32_t base = m_bgMapAddr[0];
                    fprintf(stderr, "[DIAG F%03d] BG1 tilemap[0..15] entries:", frameCount);
                    for (int col = 0; col < 16; col++) {
                        uint32_t off = base + col * 2;
                        if (off + 1 >= m_vram.size()) break;
                        uint16_t entry = m_vram[off] | (m_vram[off + 1] << 8);
                        fprintf(stderr, " %04X(t=%u,p=%u)",
                                entry, (unsigned)(entry & 0x3FF),
                                (unsigned)((entry >> 10) & 0x7));
                    }
                    fprintf(stderr, "\n");
                }
            }
        }
        // Dump CGRAM and VRAM at start of Character Test (frame 685) for BG4 palette analysis
        if (frameCount == 685) {
            dumpCGRAM("cgram_chartest.txt");
            fprintf(stderr, "[CGRAM685] BG4pal(192-207):");
            for (int i = 192; i < 208; i++) fprintf(stderr, " %02X", m_cgram[i]);
            fprintf(stderr, "\n[CGRAM685] SPRpal0(256-271):");
            for (int i = 256; i < 272; i++) fprintf(stderr, " %02X", m_cgram[i]);
            fprintf(stderr, "\n[CGRAM685] BG1pal0(0-15):");
            for (int i = 0; i < 16; i++) fprintf(stderr, " %02X", m_cgram[i]);
            fprintf(stderr, "\n[CGRAM685] BG1pal6(48-63):");
            for (int i = 48; i < 64; i++) fprintf(stderr, " %02X", m_cgram[i]);
            fprintf(stderr, "\n");
            // Dump BG4 tilemap: m_bgMapAddr[3] is a BYTE offset into m_vram
            {
                uint32_t base = m_bgMapAddr[3]; // byte address
                fprintf(stderr, "[VRAM685] BG4tilemap@byte0x%04X (first 64 entries):\n", base);
                for (int row = 0; row < 4; row++) {
                    fprintf(stderr, "  +%03X:", row*32);
                    for (int col = 0; col < 16; col++) {
                        uint32_t off = base + (row*16 + col)*2;
                        uint8_t lo = (off < m_vram.size()) ? m_vram[off] : 0xEE;
                        uint8_t hi = (off+1 < m_vram.size()) ? m_vram[off+1] : 0xEE;
                        fprintf(stderr, " %02X%02X", hi, lo);
                    }
                    fprintf(stderr, "\n");
                }
            }
            // Dump BG1 tilemap: m_bgMapAddr[0] is a BYTE offset into m_vram
            {
                uint32_t base = m_bgMapAddr[0];
                fprintf(stderr, "[VRAM685] BG1tilemap@byte0x%04X (first 32 entries):\n", base);
                for (int row = 0; row < 2; row++) {
                    fprintf(stderr, "  +%03X:", row*32);
                    for (int col = 0; col < 16; col++) {
                        uint32_t off = base + (row*16 + col)*2;
                        uint8_t lo = (off < m_vram.size()) ? m_vram[off] : 0xEE;
                        uint8_t hi = (off+1 < m_vram.size()) ? m_vram[off+1] : 0xEE;
                        fprintf(stderr, " %02X%02X", hi, lo);
                    }
                    fprintf(stderr, "\n");
                }
                // Also log BG1 tile data address
                fprintf(stderr, "[VRAM685] BG1tiledata@byte0x%04X BG4tiledata@byte0x%04X\n",
                    m_bgTileAddr[0], m_bgTileAddr[3]);
            }
        }

        // Log BG register state once after NMI starts
        static bool bgRegsDumped = false;
        if (!bgRegsDumped && m_nmiEnabled && frameCount > 10) {
            bgRegsDumped = true;
            std::cout << "=== BG Register State (frame " << frameCount << ") ===" << std::endl;
            std::cout << "  Mode=" << (int)m_bgMode << " TM=$" << std::hex << (int)m_mainScreenDesignation << std::dec << std::endl;
            for (int i = 0; i < 4; i++) {
                std::cout << "  BG" << (i+1) << ": MapAddr=0x" << std::hex << m_bgMapAddr[i]
                    << " TileAddr=0x" << m_bgTileAddr[i] << std::dec
                    << " MapSize=" << (int)m_bgMapSize[i]
                    << " TileSize=" << (m_bgTileSize[i] ? "16x16" : "8x8")
                    << " Scroll=(" << (int)(m_bg1ScrollX + i*0) << "," << (int)(m_bg1ScrollY + i*0) << ")" << std::endl;
            }
            std::cout << "===================" << std::endl;
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
    // Diagnostic: dump PPU state at key frames
    // Color Test window diagnostic
    if (m_scanline == 0 && (frameCount == 1850 || frameCount == 1900 || frameCount == 1950 || frameCount == 2000)) {
        fprintf(stderr, "[COLORTEST] F:%d TM=%02X TS=%02X Mode=%d W12=%02X W34=%02X WOBJ=%02X WH=(%d-%d,%d-%d) TMW=%02X TSW=%02X CGWS=%02X CGAD=%02X FIX=(%d,%d,%d) SETINI=%02X\n",
            frameCount, m_mainScreenDesignation, m_subScreenDesignation, m_bgMode,
            m_w12sel, m_w34sel, m_wobjsel,
            (int)m_wh0, (int)m_wh1, (int)m_wh2, (int)m_wh3,
            m_tmw, m_tsw, m_cgws, m_cgadsub,
            (int)m_coldataR, (int)m_coldataG, (int)m_coldataB, m_setini);
    }
    if (m_scanline == 0 && (frameCount == 80 || frameCount == 685 || frameCount == 700)) {
        // OAM first 2 sprites
        uint8_t oam0_x = m_oam[0], oam0_y = m_oam[1], oam0_t = m_oam[2], oam0_a = m_oam[3];
        // BG4 address: m_bgMapAddr[3]
        // CGRAM sprite palettes start at byte 256; log 8 bytes (first 4 colors of spr pal 0)
        uint8_t c256 = m_cgram[256], c257 = m_cgram[257], c258 = m_cgram[258], c259 = m_cgram[259];
        uint8_t c260 = m_cgram[260], c261 = m_cgram[261], c262 = m_cgram[262], c263 = m_cgram[263];
        // BG scrolls + tile addresses for comparison
        fprintf(stderr, "[DIAG3] F:%d TM=%02X Mode=%d BG1map=%04X BG1tile=%04X BG2map=%04X BG2tile=%04X BG1scrl=(%d,%d) BG4scrl=(%d,%d) OBJsz=%02X OAM0=(%02X,%02X,t%02X,a%02X)\n",
            frameCount, m_mainScreenDesignation, m_bgMode,
            m_bgMapAddr[0], m_bgTileAddr[0],
            m_bgMapAddr[1], m_bgTileAddr[1],
            (int)m_bg1ScrollX, (int)m_bg1ScrollY,
            (int)m_bg4ScrollX, (int)m_bg4ScrollY,
            m_objSize,
            oam0_x, oam0_y, oam0_t, oam0_a);
    }
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
        // SNES hardware: forced blank outputs black (transparent backdrop).
        // Visible scanlines 1..SCREEN_HEIGHT map to framebuffer rows 0..SCREEN_HEIGHT-1.
        int fbRow = m_scanline - 1;
        if (fbRow >= 0 && fbRow < SCREEN_HEIGHT) {
            for (int x = 0; x < SCREEN_WIDTH; x++) {
                m_framebuffer[fbRow * SCREEN_WIDTH + x] = 0xFF000000; // opaque black
            }
        }
        return;
    }

    // Render actual SNES graphics
    // SNES priority order (highest to lowest):
    // Sprite priority 3, BG1.high, BG2.high, Sprite priority 2,
    // BG1.low, BG2.low, Sprite priority 1, BG3.high, BG4.high,
    // Sprite priority 0, BG3.low, BG4.low, backdrop (CGRAM[0])

    int y = m_scanline;

    // ---- STAT77 sprite overflow pre-scan ----
    // Count sprites on this scanline and their pixel widths for Time/Range Over flags
    {
        uint8_t sizeMode = (m_objSize >> 5) & 0x07;
        if (sizeMode > 5) sizeMode = 0;
        static const int smallW[6] = {8,8,8,16,16,32};
        static const int smallH[6] = {8,8,8,16,16,32};
        static const int largeW[6] = {16,32,64,32,64,64};
        static const int largeH[6] = {16,32,64,32,64,64};
        int sprOnLine = 0, sprPixels = 0;
        for (int s = 0; s < 128; s++) {
            int base = s * 4;
            if (base + 3 >= (int)m_oam.size()) break;
            uint8_t spY = m_oam[base + 1];
            int extIdx = 0x200 + s / 4;
            uint8_t extBits = (extIdx < (int)m_oam.size()) ? (m_oam[extIdx] >> ((s%4)*2)) & 0x03 : 0;
            bool xBit8 = (extBits & 0x01) != 0;
            // SNES hardware: sprites with X bit8=1 (X >= 256, entirely off-screen right)
            // are excluded from sprite evaluation (not counted for Time Over or Range Over)
            if (xBit8) continue;
            bool largeSprite = (extBits & 0x02) != 0;
            int sw = largeSprite ? largeW[sizeMode] : smallW[sizeMode];
            int sh = largeSprite ? largeH[sizeMode] : smallH[sizeMode];
            // Correct SNES sprite Y formula: lineOffset = (scanline - spriteY) & 0xFF
            // Sprite is on this scanline if lineOffset < height
            int lineOffset = (y - (int)spY) & 0xFF;
            if (lineOffset < sh) {
                sprOnLine++;
                sprPixels += sw;
            }
        }
        // Range Over (bit6): >32 sprites found on this scanline
        if (sprOnLine > 32) m_rangeOver = true;
        // Time Over (bit7): sprite pixel count exceeds rendering budget (>272 pixels)
        if (sprPixels > 272) m_timeOver = true;

        // --- Diagnostic: per-scanline count of sprites intersecting this line ---
        // Key gauge for "gate 1 (OAM empty / all offscreen)" vs later bugs.
        // Limited to the post-OBJ-enable window.
        if (frameCount >= 670 && frameCount <= 680) {
            static int sprOnLineFrame = -1;
            static int sprOnLineLogged = 0;
            if (sprOnLineFrame != frameCount) { sprOnLineFrame = frameCount; sprOnLineLogged = 0; }
            if (sprOnLine > 0 && sprOnLineLogged < 16) {
                fprintf(stderr, "[SPR-ONLINE] F:%d scan=%d count=%d TM=$%02X OBSEL=$%02X\n",
                    frameCount, y, sprOnLine, m_mainScreenDesignation, m_objSize);
                sprOnLineLogged++;
            }
        }
        // Diagnostic: log when overflow newly detected during sprite test frames
        if ((sprOnLine > 32 || sprPixels > 272) && frameCount >= 500 && frameCount <= 700) {
            // Find which sprites caused the overflow
            static int diagOvfCount2 = 0;
            if (diagOvfCount2 < 5) {
                diagOvfCount2++;
                fprintf(stderr, "[OVF2] F:%d SL:%d sprOnLine=%d sprPixels=%d sizeMode=%d\n",
                    frameCount, y, sprOnLine, sprPixels, sizeMode);
                // Log first 5 sprites that are on this scanline
                int logged = 0;
                for (int s2 = 0; s2 < 128 && logged < 10; s2++) {
                    int b2 = s2 * 4;
                    uint8_t sy = m_oam[b2+1];
                    int extI = 0x200 + s2/4;
                    uint8_t eb = (extI < (int)m_oam.size()) ? (m_oam[extI] >> ((s2%4)*2)) & 0x03 : 0;
                    bool xb8 = (eb & 0x01) != 0;
                    bool lg = (eb & 0x02) != 0;
                    int sw2 = lg ? largeW[sizeMode] : smallW[sizeMode];
                    int sh2 = lg ? largeH[sizeMode] : smallH[sizeMode];
                    int lo = (y - (int)sy) & 0xFF;
                    if (lo < sh2 && !xb8) {
                        fprintf(stderr, "  spr%d: Y=%d ext=%02X lo=%d sw=%d\n", s2, sy, eb, lo, sw2);
                        logged++;
                    }
                }
            }
        }
    }

    // Pre-compute window membership for this scanline (x-loop uses these)
    // Window 1: pixels where wh0 <= x <= wh1
    // Window 2: pixels where wh2 <= x <= wh3

    for (int x = 0; x < SCREEN_WIDTH; x++) {
        // ---- Window computation ----
        bool inWin1 = (x >= (int)m_wh0 && x <= (int)m_wh1);
        bool inWin2 = (x >= (int)m_wh2 && x <= (int)m_wh3);

        // Per-layer window masking helper lambda
        // wsel bits: [7:6]=win2 inv/enable [5:4]=win1 inv/enable for layer B
        //            [3:2]=win2 inv/enable [1:0]=win1 inv/enable for layer A
        // For a 2-layer register (e.g. W12SEL): layer A = lower nibble, layer B = upper nibble
        // Each pair: bit1=enable, bit0=invert
        // For OBJ/Color (WOBJSEL): obj = lower nibble, color = upper nibble
        // Combined via WBGLOG/WOBJLOG: 2-bit logic per layer (OR/AND/XOR/XNOR)
        // windowMaskLayer: returns true if this pixel is MASKED for the given layer.
        // layerBit: 0=BG1, 1=BG2, 2=BG3, 3=BG4, 4=OBJ, 5=Color (for color math window)
        // isSprite: true for OBJ layer
        // W12SEL ($2123) bits:  [1:0]=BG1-W1 [3:2]=BG1-W2  [5:4]=BG2-W1 [7:6]=BG2-W2
        //   Each 2-bit pair: bit1=enable, bit0=invert
        // W34SEL ($2124) same layout for BG3/BG4
        // WOBJSEL ($2125): [3:0]=OBJ  [7:4]=Color window
        // WBGLOG ($212A): bits[1:0]=BG1 [3:2]=BG2 [5:4]=BG3 [7:6]=BG4 (0=OR,1=AND,2=XOR,3=XNOR)
        // WOBJLOG ($212B): bits[1:0]=OBJ [3:2]=Color
        auto windowMaskLayer = [&](int layerBit, bool /*isSprite*/) -> bool {
            uint8_t wsel;
            uint8_t wlogic;
            int wselShift;   // bit offset within wsel for W1 pair of this layer
            int wlogicShift; // bit offset within wlogic for 2-bit logic of this layer

            switch (layerBit) {
                case 0: wsel=m_w12sel; wselShift=0; wlogic=m_wbglog;  wlogicShift=0; break; // BG1
                case 1: wsel=m_w12sel; wselShift=4; wlogic=m_wbglog;  wlogicShift=2; break; // BG2
                case 2: wsel=m_w34sel; wselShift=0; wlogic=m_wbglog;  wlogicShift=4; break; // BG3
                case 3: wsel=m_w34sel; wselShift=4; wlogic=m_wbglog;  wlogicShift=6; break; // BG4
                case 4: wsel=m_wobjsel;wselShift=0; wlogic=m_wobjlog; wlogicShift=0; break; // OBJ
                case 5: wsel=m_wobjsel;wselShift=4; wlogic=m_wobjlog; wlogicShift=2; break; // Color
                default: return false;
            }

            // W1 pair: bits [wselShift+1 : wselShift]
            bool w1en  = (wsel >> (wselShift + 1)) & 1;
            bool w1inv = (wsel >> (wselShift + 0)) & 1;
            // W2 pair: bits [wselShift+3 : wselShift+2]
            bool w2en  = (wsel >> (wselShift + 3)) & 1;
            bool w2inv = (wsel >> (wselShift + 2)) & 1;

            bool w1inside = w1en && (inWin1 ^ w1inv);
            bool w2inside = w2en && (inWin2 ^ w2inv);

            uint8_t logic = (wlogic >> wlogicShift) & 0x03; // 0=OR,1=AND,2=XOR,3=XNOR

            bool masked;
            if (!w1en && !w2en) {
                masked = false;
            } else if (w1en && !w2en) {
                masked = w1inside;
            } else if (!w1en && w2en) {
                masked = w2inside;
            } else {
                switch (logic) {
                    case 0:  masked = w1inside || w2inside;   break; // OR
                    case 1:  masked = w1inside && w2inside;   break; // AND
                    case 2:  masked = w1inside ^  w2inside;   break; // XOR
                    case 3:  masked = !(w1inside ^ w2inside); break; // XNOR
                    default: masked = false; break;
                }
            }
            return masked;
        };

        // ---- BG layer pixels (mode-dependent) ----
        // We need per-layer PixelInfo for priority compositing
        // For all modes we use sampleBGLayer / renderBGx helpers
        PixelInfo bgLayerPixels[4] = {{0,0},{0,0},{0,0},{0,0}};
        uint32_t backdropColor = getBGColorFromCGRAM(m_cgram);

        // helper: sign-extend 10-bit scroll
        auto signScroll = [](uint16_t v) -> int {
            int s = v & 0x03FF;
            if (s >= 512) s -= 1024;
            return s;
        };

        switch (m_bgMode) {
            case 0: {
                bgLayerPixels[0] = sampleBGLayer(0, x, y, signScroll(m_bg1ScrollX), signScroll(m_bg1ScrollY), 2, m_bgTileSize[0]);
                bgLayerPixels[1] = sampleBGLayer(1, x, y, signScroll(m_bg2ScrollX), signScroll(m_bg2ScrollY), 2, m_bgTileSize[1]);
                bgLayerPixels[2] = sampleBGLayer(2, x, y, signScroll(m_bg3ScrollX), signScroll(m_bg3ScrollY), 2, m_bgTileSize[2]);
                bgLayerPixels[3] = sampleBGLayer(3, x, y, signScroll(m_bg4ScrollX), signScroll(m_bg4ScrollY), 2, m_bgTileSize[3]);
                break;
            }
            case 1: {
                bgLayerPixels[0] = sampleBGLayer(0, x, y, signScroll(m_bg1ScrollX), signScroll(m_bg1ScrollY), 4, m_bgTileSize[0]);
                bgLayerPixels[1] = sampleBGLayer(1, x, y, signScroll(m_bg2ScrollX), signScroll(m_bg2ScrollY), 4, m_bgTileSize[1]);
                bgLayerPixels[2] = sampleBGLayer(2, x, y, signScroll(m_bg3ScrollX), signScroll(m_bg3ScrollY), 2, m_bgTileSize[2]);
                break;
            }
            case 2: {
                int optX=0, optY=0;
                getOPTOffsets(m_vram, m_bgMapAddr[2], m_bgMapSize[2], x, y, m_bgTileSize[0]?16:8, optX, optY);
                bgLayerPixels[0] = sampleBGLayer(0, x, y, signScroll(m_bg1ScrollX)+optX, signScroll(m_bg1ScrollY)+optY, 4, m_bgTileSize[0]);
                bgLayerPixels[1] = sampleBGLayer(1, x, y, signScroll(m_bg2ScrollX)+optX, signScroll(m_bg2ScrollY)+optY, 4, m_bgTileSize[1]);
                break;
            }
            case 3: {
                bgLayerPixels[0] = sampleBGLayer(0, x, y, signScroll(m_bg1ScrollX), signScroll(m_bg1ScrollY), 8, m_bgTileSize[0]);
                bgLayerPixels[1] = sampleBGLayer(1, x, y, signScroll(m_bg2ScrollX), signScroll(m_bg2ScrollY), 4, m_bgTileSize[1]);
                break;
            }
            case 4: {
                int optX=0, optY=0;
                getOPTOffsets(m_vram, m_bgMapAddr[2], m_bgMapSize[2], x, y, m_bgTileSize[0]?16:8, optX, optY);
                bgLayerPixels[0] = sampleBGLayer(0, x, y, signScroll(m_bg1ScrollX)+optX, signScroll(m_bg1ScrollY)+optY, 8, m_bgTileSize[0]);
                bgLayerPixels[1] = sampleBGLayer(1, x, y, signScroll(m_bg2ScrollX)+optX, signScroll(m_bg2ScrollY)+optY, 2, m_bgTileSize[1]);
                break;
            }
            case 5: {
                int hiresX = x * 2;
                bgLayerPixels[0] = sampleBGLayer(0, hiresX, y, signScroll(m_bg1ScrollX), signScroll(m_bg1ScrollY), 4, m_bgTileSize[0]);
                bgLayerPixels[1] = sampleBGLayer(1, hiresX, y, signScroll(m_bg2ScrollX), signScroll(m_bg2ScrollY), 2, m_bgTileSize[1]);
                break;
            }
            case 6: {
                int optX=0, optY=0;
                getOPTOffsets(m_vram, m_bgMapAddr[2], m_bgMapSize[2], x, y, 8, optX, optY);
                int hiresX = x * 2;
                bgLayerPixels[0] = sampleBGLayer(0, hiresX, y, signScroll(m_bg1ScrollX)+optX, signScroll(m_bg1ScrollY)+optY, 4, m_bgTileSize[0]);
                break;
            }
            case 7: {
                uint32_t c7 = renderBackgroundMode7(x);
                bgLayerPixels[0] = {c7, (c7 == backdropColor) ? (uint8_t)0 : (uint8_t)1};
                break;
            }
            default:
                break;
        }

        // ---- Sprite pixel ----
        PixelInfo spritePixel = renderSpritePixel(x, y);

        // Apply window masking to each BG layer and sprite
        for (int i = 0; i < 4; i++) {
            if (bgLayerPixels[i].color != 0) {
                bool masked = false;
                // Main screen window: TMW bit i enables windowing for that BG
                if (m_tmw & (1 << i)) {
                    masked = windowMaskLayer(i, false);
                }
                if (masked) bgLayerPixels[i] = {0, 0};
            }
        }
        if (spritePixel.color != 0) {
            bool masked = false;
            if (m_tmw & 0x10) { // bit4 = OBJ window mask enable
                masked = windowMaskLayer(4, true);
            }
            if (masked) spritePixel = {0, 0};
        }

        // ---- SNES priority compositing ----
        // Priority table per mode (main screen order, highest priority first):
        // The SNES priority system: each layer has a priority bit from tilemap (0 or 1)
        // which maps to a fixed numeric priority. Sprites have OAM priority 0-3.
        //
        // For simplicity we use the priority values already stored in bgLayerPixels
        // (set by renderBGx from m_bgPriority) and sprite priority from OAM.
        // Higher number = drawn on top.
        //
        // SNES composite order (priority number mapping):
        // Mode 1: Sprite3=12, BG1hi=11, BG2hi=10, Sprite2=9, BG1lo=8, BG2lo=7,
        //         Sprite1=6, BG3hi=5, Sprite0=4, BG3lo=3, backdrop=0
        //   (BG3 high priority mode: BG3hi=12 if BGMODE bit3 set, but simplified here)
        //
        // We map BG priority groups to a 0-15 composite scale per mode.

        // Composite: pick highest-priority opaque pixel
        uint32_t mainPixel = backdropColor;
        uint8_t mainPriority = 0;
        bool foundAnyBG = false;
        // Backdrop: color math bit5 of CGADSUB
        bool colorMathApplies = (m_cgadsub & 0x20) != 0;

        // Check each BG layer (enabled by TM register)
        // BG layers are checked in order i=0 (BG1, highest) to i=3 (BG4, lowest)
        // First non-transparent pixel found wins if same priority; higher priority always wins
        for (int i = 0; i < 4; i++) {
            if (!(m_mainScreenDesignation & (1 << i))) continue;
            if (bgLayerPixels[i].color == 0) continue;
            if (!foundAnyBG || bgLayerPixels[i].priority > mainPriority) {
                mainPriority = bgLayerPixels[i].priority;
                mainPixel = bgLayerPixels[i].color;
                foundAnyBG = true;
                // Color math applies if bit i of CGADSUB is set
                colorMathApplies = (m_cgadsub & (1 << i)) != 0;
            }
        }

        // Composite sprite on top if higher priority
        // Sprite priority 3 > all BGs, priority 0 < most BGs
        // Map OAM priorities 0-3 to composite scale:
        // Mode 0: SP0=3, SP1=7, SP2=11, SP3=15
        // Interleaved with BG priorities: BG1lo=9, BG2lo=8, BG3lo=2, BG4lo=1, BG1hi=13, BG2hi=12, BG3hi=5, BG4hi=4

        // --- Diagnostic: explain why sprite compositing is/isn't engaged ---
        // Logs on first non-zero sprite pixel that reaches compositing gate, regardless of outcome.
        if (spritePixel.color != 0 && frameCount >= 670 && frameCount <= 705) {
            static int sprGateFrame = -1;
            if (sprGateFrame != frameCount) {
                sprGateFrame = frameCount;
                bool tmOK = (m_mainScreenDesignation & 0x10) != 0;
                fprintf(stderr, "[SPR-GATE] F:%d scan=%d x=%d sprColor=$%08X oamPri=%d mainPri=%d TMbit4=%d bgWin?=%d\n",
                    frameCount, y, x, spritePixel.color, spritePixel.priority, mainPriority,
                    tmOK ? 1 : 0, foundAnyBG ? 1 : 0);
            }
        }

        if (spritePixel.color != 0 && (m_mainScreenDesignation & 0x10)) {
            static const uint8_t spriteCompPriority[4] = {3, 7, 11, 15};
            uint8_t spritePri = spriteCompPriority[spritePixel.priority & 3];
            if (spritePri > mainPriority) {
                mainPriority = spritePri;
                mainPixel = spritePixel.color;
                // Sprite color math: bit 4 of CGADSUB
                colorMathApplies = (m_cgadsub & 0x10) != 0;
            }
            // Diagnostic: first sprite pixel drawn per frame to confirm path is alive
            static int sprDiagFrame = -1;
            if (sprDiagFrame != frameCount) {
                sprDiagFrame = frameCount;
                fprintf(stderr, "[SPR-HIT] F:%d scan=%d x=%d color=%08X oamPri=%d compPri=%d TM=$%02X OBSEL=$%02X\n",
                    frameCount, y, x, spritePixel.color, spritePixel.priority, spritePri,
                    m_mainScreenDesignation, m_objSize);
            }
        }

        // ---- Color Math ----
        // $2130 CGWSEL layout: MMCC DD.S
        //   MM (bits 7-6): Prevent Color Math — 0=never prevent, 1=outside win, 2=inside win, 3=always prevent
        //   CC (bits 5-4): Clip main screen to black before math — 0=never, 1=outside win, 2=inside win, 3=always
        //   DD (bits 3-2): Color Math Enable area — 0=always, 1=inside win only, 2=outside win only, 3=never
        //   bit 1: Sub-screen source — 0=fixed color, 1=sub screen
        //   bit 0: Direct Color Mode (Mode 3/4/7 256-col)
        // $2131 CGADSUB: bit7=subtract, bit6=half, bits5-0=per-layer enable

        // Evaluate the color math window once (layer 5 = color window)
        bool inColorWin = windowMaskLayer(5, false);

        // Prevent Color Math check (MM bits 7-6)
        uint8_t preventCondition = (m_cgws >> 6) & 0x03;
        bool preventMath = false;
        switch (preventCondition) {
            case 0: preventMath = false;        break; // never prevent
            case 1: preventMath = !inColorWin;  break; // prevent outside window
            case 2: preventMath = inColorWin;   break; // prevent inside window
            case 3: preventMath = true;         break; // always prevent
        }

        // Clip main screen to black (CC bits 5-4) — applied before color math
        uint32_t clippedMain = mainPixel;
        uint8_t clipCondition = (m_cgws >> 4) & 0x03;
        bool doClip = false;
        switch (clipCondition) {
            case 0: doClip = false;        break; // never clip
            case 1: doClip = !inColorWin;  break; // clip outside window
            case 2: doClip = inColorWin;   break; // clip inside window
            case 3: doClip = true;         break; // always clip
        }
        if (doClip) {
            clippedMain = 0xFF000000; // clip to black (alpha=1, RGB=0)
        }

        // Color Math Enable area (DD bits 3-2): where math is performed
        uint8_t mathCondition = (m_cgws >> 2) & 0x03;
        bool inMathArea = false;
        switch (mathCondition) {
            case 0: inMathArea = true;        break; // always
            case 1: inMathArea = inColorWin;  break; // inside window only
            case 2: inMathArea = !inColorWin; break; // outside window only
            case 3: inMathArea = false;       break; // never
        }

        bool doColorMath = colorMathApplies && !preventMath && inMathArea;

        // When color math is NOT performed, the main pixel passes through unmodified.
        // Clip-to-black only affects the main screen input when color math IS applied.
        uint32_t finalColor = mainPixel;

        if (doColorMath) {
            // Apply clip-to-black only as input to color math
            uint32_t mathInput = clippedMain;
            // Subscreen source: CGWSEL bit1 (0=fixed color, 1=subscreen)
            uint32_t subColor;
            if (m_cgws & 0x02) {
                // Use subscreen pixel
                subColor = renderSubScreen(x);
            } else {
                // Use fixed color ($2132 COLDATA): 5-bit R/G/B expanded to 8-bit
                uint8_t fr = (m_coldataR << 3) | (m_coldataR >> 2);
                uint8_t fg = (m_coldataG << 3) | (m_coldataG >> 2);
                uint8_t fb = (m_coldataB << 3) | (m_coldataB >> 2);
                subColor = fr | (fg << 8) | (fb << 16) | (0xFF << 24);
            }
            finalColor = applyColorMath(mathInput, subColor);
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
            uint8_t a = ((finalColor >> 24) & 0xFF);
            r = (r * m_brightness) / 15;
            g = (g * m_brightness) / 15;
            b = (b * m_brightness) / 15;
            finalColor = r | (g << 8) | (b << 16) | (a << 24);
        }

        // Map scanline 1..SCREEN_HEIGHT to framebuffer rows 0..SCREEN_HEIGHT-1
        int fbRow = m_scanline - 1;
        m_framebuffer[fbRow * SCREEN_WIDTH + x] = finalColor;
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
    // Determine tile size based on bpp (2bpp=16, 4bpp=32, 8bpp=64 bytes per 8x8 tile)
    const int TILE_SIZE_BYTES = (bpp == 8) ? 64 : (bpp == 4) ? 32 : 16;
    
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
    
    // Handle tilemap wrapping based on BGSC bits 0-1
    // 0=32x32, 1=64x32, 2=32x64, 3=64x64
    int tilemapWidth = (m_bgMapSize[bgIndex] & 1) ? 64 : 32;
    int tilemapHeight = (m_bgMapSize[bgIndex] & 2) ? 64 : 32;
    int wrappedTileX = ((tileX % tilemapWidth) + tilemapWidth) % tilemapWidth;
    int wrappedTileY = ((tileY % tilemapHeight) + tilemapHeight) % tilemapHeight;
    
    // 1. Calculate tilemap address
    // SNES tilemaps are organized as 32x32 tile "screens" (2KB each)
    // Layout depends on size bits: 0=1screen, 1=H+H, 2=V/V, 3=H+H/V+V
    uint16_t screenX = wrappedTileX / 32;
    uint16_t screenY = wrappedTileY / 32;
    uint16_t localX = wrappedTileX % 32;
    uint16_t localY = wrappedTileY % 32;
    // Screen offset: each 32x32 screen = 2048 bytes (32*32*2)
    uint16_t screenOffset = 0;
    if (m_bgMapSize[bgIndex] & 1) screenOffset += screenX * 2048; // horizontal
    if (m_bgMapSize[bgIndex] & 2) screenOffset += screenY * 2048 * ((m_bgMapSize[bgIndex] & 1) ? 2 : 1); // vertical
    uint16_t mapAddr = m_bgMapAddr[bgIndex] + screenOffset + (localY * 32 + localX) * 2;
    
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
    // Palette bits from tilemap vary by bpp:
    //   2bpp: bits 10-12 (3 bits → 0-7)
    //   4bpp: bits 10-12 (3 bits → 0-7)
    //   8bpp: palette bits ignored (direct color)
    // For Mode 0 (2bpp), add BG-specific base palette offset to route each BG
    // into its own CGRAM region:
    //   BG1 → palette 0-7   (CGRAM entries 0-31)
    //   BG2 → palette 8-15  (CGRAM entries 32-63)
    //   BG3 → palette 16-23 (CGRAM entries 64-95)
    //   BG4 → palette 24-31 (CGRAM entries 96-127)
    uint8_t palette = (tileEntry >> 10) & 0x07;
    if (bpp == 2) {
        palette = (uint8_t)((palette & 0x07) + (bgIndex & 0x03) * 8);
    }
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
        
        // For 16x16 tiles, sub-tiles are arranged in VRAM as:
        //   top-left  = tileNumber
        //   top-right = tileNumber + 1
        //   bot-left  = tileNumber + 16  (wraps within 10-bit tile index)
        //   bot-right = tileNumber + 17
        // The offset is always 16 (SNES hardware spec), not tilemap width.

        // Apply flip BEFORE calculating sub-tile position
        if (hFlip) {
            subTileX = 1 - subTileX;
            subPixelX = 7 - subPixelX;
        }
        if (vFlip) {
            subTileY = 1 - subTileY;
            subPixelY = 7 - subPixelY;
        }

        // Map sub-tile (0 or 1) in each axis to VRAM tile index
        subTileNumber = (tileNumber + subTileX + subTileY * 16) & 0x03FF;
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
    
    TileCache* tileCache = getTileCacheEntry(tileAddr, bpp);
    if (!tileCache) {
        return {0, 0};
    }
    
    uint8_t pixelIndex = tileCache->pixels[subPixelY][subPixelX];
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
    
    // Pixel rendered successfully

    return {color, priority};
}

// Main rendering function
uint32_t PPU::renderBackgroundMode0(int x) {
    int y = m_scanline; // Current scanline (m_scanline is a PPU member variable)
    
    // Rendering Mode 0 background
    
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
        return getBGColorFromCGRAM(m_cgram);
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
        return getBGColorFromCGRAM(m_cgram);
    }

    return finalColor;
}

// ============================================================
// Helper: get background color from CGRAM[0]
// ============================================================
static uint32_t getBGColorFromCGRAM(const std::vector<uint8_t>& cgram) {
    // SNES CGRAM color format: 15-bit BGR (bits [14:10]=B, [9:5]=G, [4:0]=R)
    // Expand 5-bit channel to 8-bit via (v<<3)|(v>>2) so 0x1F -> 0xFF (not 0xF8)
    uint16_t c = cgram[0] | (cgram[1] << 8);
    uint8_t rv = (c & 0x1F);
    uint8_t gv = (c >> 5) & 0x1F;
    uint8_t bv = (c >> 10) & 0x1F;
    uint8_t r = (rv << 3) | (rv >> 2);
    uint8_t g = (gv << 3) | (gv >> 2);
    uint8_t b = (bv << 3) | (bv >> 2);
    // Framebuffer layout: little-endian uint32 bytes [R, G, B, A] = SDL_PIXELFORMAT_ABGR8888
    return r | (g << 8) | (b << 16) | (0xFFu << 24);
}

// Member helper: apply scroll, compute tile coords, call renderBGx
PixelInfo PPU::sampleBGLayer(int bgIndex, int screenX, int screenY,
                              int scrollX, int scrollY, int bpp, bool is16x16) {
    int tileSize = is16x16 ? 16 : 8;
    int bgX = screenX + scrollX;
    int bgY = screenY + scrollY;
    int tileX = bgX / tileSize;
    int tileY = bgY / tileSize;
    int pixelX = bgX % tileSize;
    int pixelY = bgY % tileSize;
    if (pixelX < 0) { pixelX += tileSize; tileX -= 1; }
    if (pixelY < 0) { pixelY += tileSize; tileY -= 1; }
    return renderBGx(bgIndex, tileX, tileY, pixelX, pixelY, bpp);
}

// Helper: composite layers by priority and main-screen designation, return final RGBA
static uint32_t compositeLayers(const PixelInfo* pixels, int count,
                                 uint8_t mainScreenDesignation,
                                 const std::vector<uint8_t>& cgram) {
    uint32_t finalColor = 0;
    uint8_t maxPriority = 0;
    bool found = false;
    for (int i = 0; i < count; ++i) {
        if (!(mainScreenDesignation & (1 << i))) continue;
        if (pixels[i].color != 0) {
            if (!found || pixels[i].priority >= maxPriority) {
                maxPriority = pixels[i].priority;
                finalColor = pixels[i].color;
                found = true;
            }
        }
    }
    if (finalColor == 0) return getBGColorFromCGRAM(cgram);
    return finalColor;
}

// ============================================================
// OPT (Offset-Per-Tile) helper
// SNES Mode 2/4/6: BG3 tilemap provides per-tile H/V offsets
// Returns offsetX and offsetY for the tile at screen position (screenX, screenY)
// bgTileSize: tile width/height of the target BG (8 or 16 pixels)
// ============================================================
static void getOPTOffsets(const std::vector<uint8_t>& vram,
                           uint16_t bg3MapAddr, uint8_t bg3MapSize,
                           int screenX, int screenY, int bgTileSize,
                           int& outOffsetX, int& outOffsetY)
{
    // The OPT column index is determined by which tile column the current pixel falls in.
    // For an 8-pixel tile, tile column = (screenX) / 8 (NOT scrolled).
    // For a 16-pixel tile, tile column = (screenX) / 16.
    int tileCol = screenX / bgTileSize;  // tile column in screen space

    // BG3 tilemap lookup: row 0 maps to tile column 0, etc.
    int bg3TilemapWidth = (bg3MapSize & 1) ? 64 : 32;
    int bg3TilemapHeight = (bg3MapSize & 2) ? 64 : 32;
    int wrappedCol = ((tileCol % bg3TilemapWidth) + bg3TilemapWidth) % bg3TilemapWidth;
    // Row in the OPT table comes from the screen row / tile height (unscrolled)
    int tileRow = screenY / bgTileSize;
    int wrappedRow = ((tileRow % bg3TilemapHeight) + bg3TilemapHeight) % bg3TilemapHeight;

    uint16_t screenXoff = wrappedCol / 32;
    uint16_t screenYoff = wrappedRow / 32;
    uint16_t localCol = wrappedCol % 32;
    uint16_t localRow = wrappedRow % 32;
    uint16_t screenOffset = 0;
    if (bg3MapSize & 1) screenOffset += screenXoff * 2048;
    if (bg3MapSize & 2) screenOffset += screenYoff * 2048 * ((bg3MapSize & 1) ? 2 : 1);
    uint16_t mapAddr = bg3MapAddr + screenOffset + (localRow * 32 + localCol) * 2;

    outOffsetX = 0;
    outOffsetY = 0;
    if (mapAddr + 1 >= (uint16_t)vram.size()) return;

    uint16_t entry = vram[mapAddr] | (vram[mapAddr + 1] << 8);
    // Bits 10-12 of the OPT entry select which offset is applied:
    // Bit 13 (priority flag in normal tilemap): if set, entry provides V-offset;
    // otherwise H-offset.  Both entries exist (even column = H, odd column = V in some
    // docs), but the simplest correct interpretation used by most emulators:
    //   entry.bit13 == 0 → H offset value in bits 0-9 (10-bit signed, but SNES uses
    //                       bits 0-8 for unsigned 0..511; treat as signed 9-bit)
    //   entry.bit13 == 1 → V offset value in bits 0-9
    // Actual hardware: two consecutive OPT entries per column — first is H, second is V.
    // Here we use the single-entry model for simplicity (matches most games).
    bool isVOffset = (entry >> 13) & 1;
    int16_t offsetVal = (int16_t)((entry & 0x01FF) << 7) >> 7; // sign-extend 9-bit

    if (!isVOffset) {
        outOffsetX = offsetVal;
    } else {
        outOffsetY = offsetVal;
    }
}

// ============================================================
// Mode 2: 4bpp BG1 + 4bpp BG2 + Offset-Per-Tile (BG3 provides offsets)
// ============================================================
uint32_t PPU::renderBackgroundMode2(int x) {
    int y = m_scanline;

    // Priority: BG1.high(3), BG2.high(2), BG1.low(1), BG2.low(0)
    // (Mode 2 uses the same two-layer 4bpp scheme as Mode 1 for BG1/BG2)

    // --- OPT: read per-tile offsets from BG3 tilemap ---
    int optOffsetX = 0, optOffsetY = 0;
    getOPTOffsets(m_vram, m_bgMapAddr[2], m_bgMapSize[2],
                  x, y, m_bgTileSize[0] ? 16 : 8,
                  optOffsetX, optOffsetY);

    // BG1 (4bpp) with OPT
    int bg1ScrollX = (int)(m_bg1ScrollX & 0x03FF); if (bg1ScrollX >= 512) bg1ScrollX -= 1024;
    int bg1ScrollY = (int)(m_bg1ScrollY & 0x03FF); if (bg1ScrollY >= 512) bg1ScrollY -= 1024;
    PixelInfo bg1 = sampleBGLayer(0, x, y,
                                   bg1ScrollX + optOffsetX, bg1ScrollY + optOffsetY,
                                   4, m_bgTileSize[0]);

    // BG2 (4bpp) with OPT
    int bg2ScrollX = (int)(m_bg2ScrollX & 0x03FF); if (bg2ScrollX >= 512) bg2ScrollX -= 1024;
    int bg2ScrollY = (int)(m_bg2ScrollY & 0x03FF); if (bg2ScrollY >= 512) bg2ScrollY -= 1024;
    PixelInfo bg2 = sampleBGLayer(1, x, y,
                                   bg2ScrollX + optOffsetX, bg2ScrollY + optOffsetY,
                                   4, m_bgTileSize[1]);

    PixelInfo layers[4] = {bg1, bg2, {0,0}, {0,0}};
    return compositeLayers(layers, 2, m_mainScreenDesignation, m_cgram);
}

// ============================================================
// Mode 3: 8bpp BG1 + 4bpp BG2
// ============================================================
uint32_t PPU::renderBackgroundMode3(int x) {
    int y = m_scanline;

    int bg1ScrollX = (int)(m_bg1ScrollX & 0x03FF); if (bg1ScrollX >= 512) bg1ScrollX -= 1024;
    int bg1ScrollY = (int)(m_bg1ScrollY & 0x03FF); if (bg1ScrollY >= 512) bg1ScrollY -= 1024;
    PixelInfo bg1 = sampleBGLayer(0, x, y, bg1ScrollX, bg1ScrollY, 8, m_bgTileSize[0]);

    int bg2ScrollX = (int)(m_bg2ScrollX & 0x03FF); if (bg2ScrollX >= 512) bg2ScrollX -= 1024;
    int bg2ScrollY = (int)(m_bg2ScrollY & 0x03FF); if (bg2ScrollY >= 512) bg2ScrollY -= 1024;
    PixelInfo bg2 = sampleBGLayer(1, x, y, bg2ScrollX, bg2ScrollY, 4, m_bgTileSize[1]);

    // Mode 3 priorities: BG1.high=3, BG2.high=2, BG1.low=1, BG2.low=0
    // renderBGx already assigns priority from m_bgPriority; update them for Mode 3
    // (priority array is set during BGMODE write; here we trust those values)
    PixelInfo layers[4] = {bg1, bg2, {0,0}, {0,0}};
    return compositeLayers(layers, 2, m_mainScreenDesignation, m_cgram);
}

// ============================================================
// Mode 4: 8bpp BG1 + 2bpp BG2 + Offset-Per-Tile
// ============================================================
uint32_t PPU::renderBackgroundMode4(int x) {
    int y = m_scanline;

    // OPT uses BG3 tilemap just like Mode 2 (even though BG3 has no visible layer)
    int optOffsetX = 0, optOffsetY = 0;
    getOPTOffsets(m_vram, m_bgMapAddr[2], m_bgMapSize[2],
                  x, y, m_bgTileSize[0] ? 16 : 8,
                  optOffsetX, optOffsetY);

    int bg1ScrollX = (int)(m_bg1ScrollX & 0x03FF); if (bg1ScrollX >= 512) bg1ScrollX -= 1024;
    int bg1ScrollY = (int)(m_bg1ScrollY & 0x03FF); if (bg1ScrollY >= 512) bg1ScrollY -= 1024;
    PixelInfo bg1 = sampleBGLayer(0, x, y,
                                   bg1ScrollX + optOffsetX, bg1ScrollY + optOffsetY,
                                   8, m_bgTileSize[0]);

    int bg2ScrollX = (int)(m_bg2ScrollX & 0x03FF); if (bg2ScrollX >= 512) bg2ScrollX -= 1024;
    int bg2ScrollY = (int)(m_bg2ScrollY & 0x03FF); if (bg2ScrollY >= 512) bg2ScrollY -= 1024;
    PixelInfo bg2 = sampleBGLayer(1, x, y,
                                   bg2ScrollX + optOffsetX, bg2ScrollY + optOffsetY,
                                   2, m_bgTileSize[1]);

    PixelInfo layers[4] = {bg1, bg2, {0,0}, {0,0}};
    return compositeLayers(layers, 2, m_mainScreenDesignation, m_cgram);
}

// ============================================================
// Mode 5: Hi-res 512x224 — 4bpp BG1 + 2bpp BG2
// The SNES renders at 512px wide in this mode (interlaced/hi-res).
// We map each output pixel x to hi-res pixel 2*x (even sub-pixel).
// ============================================================
uint32_t PPU::renderBackgroundMode5(int x) {
    int y = m_scanline;

    // In hi-res mode the effective pixel width is 512; we sample the even sub-pixel.
    int hiresX = x * 2;

    int bg1ScrollX = (int)(m_bg1ScrollX & 0x03FF); if (bg1ScrollX >= 512) bg1ScrollX -= 1024;
    int bg1ScrollY = (int)(m_bg1ScrollY & 0x03FF); if (bg1ScrollY >= 512) bg1ScrollY -= 1024;
    PixelInfo bg1 = sampleBGLayer(0, hiresX, y, bg1ScrollX, bg1ScrollY, 4, m_bgTileSize[0]);

    int bg2ScrollX = (int)(m_bg2ScrollX & 0x03FF); if (bg2ScrollX >= 512) bg2ScrollX -= 1024;
    int bg2ScrollY = (int)(m_bg2ScrollY & 0x03FF); if (bg2ScrollY >= 512) bg2ScrollY -= 1024;
    PixelInfo bg2 = sampleBGLayer(1, hiresX, y, bg2ScrollX, bg2ScrollY, 2, m_bgTileSize[1]);

    PixelInfo layers[4] = {bg1, bg2, {0,0}, {0,0}};
    return compositeLayers(layers, 2, m_mainScreenDesignation, m_cgram);
}

// ============================================================
// Mode 7: Affine-transform single BG
// 128×128 tile map, 8bpp tiles (64 bytes each)
// VRAM layout:
//   [0x0000-0x3FFF] tilemap: word-interleaved; even bytes = tile index, odd bytes unused
//   [0x4000-0x7FFF] tile character data: 8×8 × 8bpp = 64 bytes/tile × 256 tiles
// Scroll is taken from BG1 scroll registers (m_bg1ScrollX / m_bg1ScrollY),
// which are also written to $210D/$210E as m7hofs/m7vofs on real hardware.
// ============================================================
uint32_t PPU::renderBackgroundMode7(int x) {
    int y = m_scanline;

    // --- Matrix parameters (8.8 fixed-point stored as int16_t) ---
    // m_m7a..d are int16_t holding the 16-bit signed value written to $211B-$211E.
    // Each has 8 fractional bits, so "1.0" = 0x0100.
    int32_t A = m_m7a;
    int32_t B = m_m7b;
    int32_t C = m_m7c;
    int32_t D = m_m7d;

    // Center of rotation/scaling (13-bit signed)
    int32_t cx = m_m7x;
    int32_t cy = m_m7y;

    // Scroll (HOFS/VOFS for Mode 7, 13-bit signed)
    int32_t hofs = (int32_t)m_m7hofs;
    int32_t vofs = (int32_t)m_m7vofs;

    // M7SEL bit 0 = H-flip, bit 1 = V-flip: flip screen pixel BEFORE the transform
    int32_t px = (m_m7sel & 0x01) ? (255 - x) : x;
    int32_t py = (m_m7sel & 0x02) ? (255 - y) : y;

    // Transform: source map coordinates (8 fractional bits)
    // org_x = [A*(px + hofs - cx) + B*(py + vofs - cy)] >> 8 + cx
    // org_y = [C*(px + hofs - cx) + D*(py + vofs - cy)] >> 8 + cy
    int32_t originX = hofs - cx;
    int32_t originY = vofs - cy;

    int32_t srcX = A * originX + B * originY + A * px + B * py + (cx << 8);
    int32_t srcY = C * originX + D * originY + C * px + D * py + (cy << 8);

    // Convert from fixed-point (>> 8) to integer map pixel coordinates
    int32_t mapX = srcX >> 8;
    int32_t mapY = srcY >> 8;

    // M7SEL bits 7-6: outside-field handling
    //   bit7 (R): Screen Over — 0=wrap map, 1=outside (use bit6 to determine fill)
    //   bit6 (C): Empty Space Fill — 0=transparent, 1=fill with tile 0
    bool screenOver = (m_m7sel & 0x80) != 0;  // bit7: 1 = don't wrap
    bool fillTile0  = (m_m7sel & 0x40) != 0;  // bit6: 1 = fill with tile 0 instead of transparent
    bool outOfBounds = false;
    if (mapX < 0 || mapX >= 1024 || mapY < 0 || mapY >= 1024) {
        if (!screenOver) {
            // Wrap (tile space is 128×128 tiles × 8px = 1024px)
            mapX = ((mapX % 1024) + 1024) % 1024;
            mapY = ((mapY % 1024) + 1024) % 1024;
        } else if (!fillTile0) {
            // Outside = transparent (show backdrop)
            return getBGColorFromCGRAM(m_cgram);
        } else {
            // Outside = fill with tile 0 colour data
            outOfBounds = true;
        }
    }

    // --- VRAM Mode 7 layout (word-interleaved) ---
    // Both the tilemap and tile pixel data share VRAM starting at word address 0x0000.
    // VRAM byte address = word_address * 2.
    //   Even bytes (offset 0): tilemap — tile index (0-255) at map position (tx, ty)
    //                          word address = ty * 128 + tx
    //   Odd bytes (offset 1):  tile pixel data — 8bpp colour index for tile N pixel (px, py)
    //                          word address = N * 64 + py * 8 + px
    uint8_t colorIndex;
    if (outOfBounds) {
        // Use tile 0 pixel data for fill (mapX/Y are out-of-range; sample tile 0 pixel)
        int pixX = ((mapX % 8) + 8) % 8;  // positive modulo 8 for sub-pixel within tile 0
        int pixY = ((mapY % 8) + 8) % 8;
        // Tile 0 pixel data: word address = 0*64 + pixY*8 + pixX; odd byte
        uint32_t tileDataByteAddr = (uint32_t)(pixY * 8 + pixX) * 2 + 1;
        if (tileDataByteAddr >= m_vram.size()) return getBGColorFromCGRAM(m_cgram);
        colorIndex = m_vram[tileDataByteAddr];
    } else {
        int tileX = (mapX >> 3) & 127;  // tile column (0-127)
        int tileY = (mapY >> 3) & 127;  // tile row    (0-127)
        int pixX  = mapX & 7;
        int pixY  = mapY & 7;

        // Tilemap: word address = tileY*128 + tileX; even byte = tile index
        uint32_t tilemapByteAddr = (uint32_t)(tileY * 128 + tileX) * 2;
        if (tilemapByteAddr >= m_vram.size()) return getBGColorFromCGRAM(m_cgram);
        uint8_t tileIndex = m_vram[tilemapByteAddr];

        // Tile pixel data: word address = tileIndex*64 + pixY*8 + pixX; odd byte = 8bpp colour
        uint32_t tileDataByteAddr = (uint32_t)(tileIndex * 64 + pixY * 8 + pixX) * 2 + 1;
        if (tileDataByteAddr >= m_vram.size()) return getBGColorFromCGRAM(m_cgram);
        colorIndex = m_vram[tileDataByteAddr];
    }

    if (colorIndex == 0) return getBGColorFromCGRAM(m_cgram);  // transparent

    // Mode 7 is 8bpp direct color (palette 0)
    uint16_t cgramIdx = (uint16_t)(colorIndex * 2);
    if (cgramIdx + 1 >= (uint16_t)m_cgram.size()) return getBGColorFromCGRAM(m_cgram);
    uint16_t snesColor = m_cgram[cgramIdx] | (m_cgram[cgramIdx + 1] << 8);
    // SNES 15-bit BGR -> 24-bit RGB with proper 5->8bit expansion
    uint8_t rv = (snesColor & 0x1F);
    uint8_t gv = (snesColor >> 5) & 0x1F;
    uint8_t bv = (snesColor >> 10) & 0x1F;
    uint8_t r = (rv << 3) | (rv >> 2);
    uint8_t g = (gv << 3) | (gv >> 2);
    uint8_t b = (bv << 3) | (bv >> 2);
    return r | (g << 8) | (b << 16) | (0xFFu << 24);
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
        // Return background color (CGRAM[0]) with proper 5->8bit expansion
        return getBGColorFromCGRAM(m_cgram);
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

    // Return background color (CGRAM[0]) via unified helper (5->8bit expansion)
    return getBGColorFromCGRAM(m_cgram);
}

uint32_t PPU::renderTestPattern(int x) {
    // Simple test pattern with different colors (RGBA8888 byte order)
    if (m_scanline < 50) {
        return 0xFFFF0000; // Blue  (R=0,G=0,B=255,A=255)
    } else if (m_scanline < 100) {
        return 0xFF00FF00; // Green (R=0,G=255,B=0,A=255)
    } else if (m_scanline < 150) {
        return 0xFF0000FF; // Red   (R=255,G=0,B=0,A=255)
    } else {
        return 0xFF00FFFF; // Yellow(R=255,G=255,B=0,A=255)
    }
}

uint32_t PPU::getColor(uint8_t paletteIndex, uint8_t colorIndex) {
    return getColor(paletteIndex, colorIndex, 4); // Default to 4bpp
}

uint32_t PPU::getColor(uint8_t paletteIndex, uint8_t colorIndex, int bpp) {
    // SNES CGRAM: 256 entries (512 bytes), each color is 15-bit RGB
    // 2bpp: paletteIndex selects from 4-color groups
    // 4bpp: paletteIndex selects from 16-color groups

    uint16_t cgramIndex;
    if (bpp == 2) {
        cgramIndex = (paletteIndex * 4 + colorIndex) * 2;
    } else if (bpp == 4) {
        cgramIndex = (paletteIndex * 16 + colorIndex) * 2;
    } else {
        cgramIndex = colorIndex * 2;
    }

    if (cgramIndex + 1 >= m_cgram.size()) {
        return 0xFF000000; // Black (opaque, RGBA8888 byte order)
    }

    uint16_t snesColor = m_cgram[cgramIndex] | (m_cgram[cgramIndex + 1] << 8);

    // Extract RGB components (5 bits each) and scale to 8 bits
    // Use (v<<3)|(v>>2) to map 0x1F→0xFF correctly (not just 0xF8)
    uint8_t rv = (snesColor & 0x1F);
    uint8_t gv = (snesColor >> 5) & 0x1F;
    uint8_t bv = (snesColor >> 10) & 0x1F;
    uint8_t r = (rv << 3) | (rv >> 2);
    uint8_t g = (gv << 3) | (gv >> 2);
    uint8_t b = (bv << 3) | (bv >> 2);

    // RGBA8888 byte order: little-endian uint32 = 0xAABBGGRR
    return r | (g << 8) | (b << 16) | (0xFF << 24);
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
            bool prevFB = m_forcedBlank;
            m_brightness = value & 0x0F;
            m_forcedBlank = (value & 0x80) != 0;
            // Log every change in forced-blank state (stderr for immediate visibility)
            if (m_forcedBlank != prevFB || (frameCount >= 170 && frameCount <= 180)) {
                static int displayChangeCount = 0;
                if (displayChangeCount < 500) {
                    fprintf(stderr, "[INIDISP] F:%d $2100=%02X fb=%d->%d bright=%d scan=%d\n",
                            frameCount, value, (int)prevFB, (int)m_forcedBlank, (int)m_brightness, m_scanline);
                    displayChangeCount++;
                }
            } else {
                static int displayChangeCount2 = 0;
                if (displayChangeCount2 < 3) {
                    std::ostringstream oss;
                    oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0')
                        << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                        << "PPU: INIDISP=$" << std::hex << (int)value << std::dec
                              << " - Forced blank " << (m_forcedBlank ? "ON" : "OFF")
                              << ", brightness=" << (int)m_brightness << std::endl;
                    Logger::getInstance().logPPU(oss.str());
                    Logger::getInstance().flush();
                    displayChangeCount2++;
                }
            }
            

            break;
        }
            
        case 0x4200: { // NMITIMEN - Interrupt Enable
            bool prevNmiEnabled = m_nmiEnabled;
            m_nmiEnabled = (value & 0x80) != 0;
            m_irqMode = (value >> 4) & 0x03;  // bits 5-4: 0=off,1=H,2=V,3=H+V

            // SNES hardware: NMI is edge-triggered on the enable bit.
            // If NMI enable transitions 0->1 while VBlank NMI flag ($4210 bit7)
            // is already set, an NMI fires immediately. This is correct hardware
            // behavior per fullsnes docs. ROMs that re-enable NMI during VBlank
            // without first reading $4210 (to clear the flag) will get a second
            // NMI, which is expected. Well-behaved ROMs read $4210 in their NMI
            // handler to acknowledge VBlank before re-enabling NMI.
            // Only fire NMI on 0→1 edge if not already fired this VBlank.
            // This prevents double-NMI when the NMI handler re-enables $4200 while
            // the VBlank flag is still set (which would cause infinite re-entry).
            if (m_nmiEnabled && !prevNmiEnabled && m_nmiFlag && m_cpu
                && !m_nmiAlreadyFiredThisVBlank) {
                m_cpu->triggerNMI();
                m_nmiAlreadyFiredThisVBlank = true;
            }

            static int nmiEnableCount = 0;
            if (nmiEnableCount < 20) {
                std::cout << "PPU: NMI " << (m_nmiEnabled ? "ENABLED" : "DISABLED")
                          << " IRQmode=" << (int)m_irqMode
                          << " (value=$" << std::hex << (int)value << std::dec << ")"
                          << " scan=" << m_scanline
                          << " nmiFlag=" << m_nmiFlag
                          << " alreadyFired=" << m_nmiAlreadyFiredThisVBlank
                          << std::endl;
                nmiEnableCount++;
            }
            break;
        }

        case 0x4207: // HTIMEL - H-IRQ timer low byte
            m_htimer = (m_htimer & 0x0100) | value;
            break;

        case 0x4208: // HTIMEH - H-IRQ timer high bit
            m_htimer = (m_htimer & 0x00FF) | ((value & 0x01) << 8);
            break;

        case 0x4209: // VTIMEL - V-IRQ timer low byte
            m_vtimer = (m_vtimer & 0x0100) | value;
            break;

        case 0x420A: // VTIMEH - V-IRQ timer high bit
            m_vtimer = (m_vtimer & 0x00FF) | ((value & 0x01) << 8);
            break;
            
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
            uint8_t oldObjSize = m_objSize;
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
            // Always log OBSEL changes to stderr for test debugging (only when value changes)
            if (value != oldObjSize) {
                uint8_t sizeMode = (value >> 5) & 0x07;
                uint8_t nameSelect = (value >> 3) & 0x03;
                uint8_t charBase = value & 0x07;
                fprintf(stderr, "[OBSEL] F:%d OBSEL=$%02X (size=%d nameSel=%d base=%d) was=$%02X\n",
                    frameCount, value, sizeMode, nameSelect, charBase, oldObjSize);
            }
            break;
        }
            
        case 0x2105: { // BGMODE - BG Mode and Character Size
            m_bgMode = value & 0x07;
            
            // Extract tile size settings (bits 4-7)
            // Bit 3: Mode 1 BG3 Priority (NOT tile size)
            // Bit 4: BG1 Tile Size (0=8x8, 1=16x16)
            // Bit 5: BG2 Tile Size (0=8x8, 1=16x16)
            // Bit 6: BG3 Tile Size (0=8x8, 1=16x16)
            // Bit 7: BG4 Tile Size (0=8x8, 1=16x16)
            m_bgTileSize[0] = (value & 0x10) != 0;  // BG1 (bit 4)
            m_bgTileSize[1] = (value & 0x20) != 0;  // BG2 (bit 5)
            m_bgTileSize[2] = (value & 0x40) != 0;  // BG3 (bit 6)
            m_bgTileSize[3] = (value & 0x80) != 0;  // BG4 (bit 7)
            
            // Update priority settings based on BG mode
            // Composite priority scale: SP3=15, SP2=11, SP1=7, SP0=3
            switch (m_bgMode) {
                case 0: // Mode 0: SP3>BG1hi>BG2hi>SP2>BG1lo>BG2lo>SP1>BG3hi>BG4hi>SP0>BG3lo>BG4lo>BD
                    m_bgPriority[0][0] = 9;  m_bgPriority[0][1] = 13; // BG1 lo/hi
                    m_bgPriority[1][0] = 8;  m_bgPriority[1][1] = 12; // BG2 lo/hi
                    m_bgPriority[2][0] = 2;  m_bgPriority[2][1] = 5;  // BG3 lo/hi
                    m_bgPriority[3][0] = 1;  m_bgPriority[3][1] = 4;  // BG4 lo/hi
                    break;
                case 1: // Mode 1: SP3>BG1hi>BG2hi>SP2>BG1lo>BG2lo>SP1>BG3hi>SP0>BG3lo>BD
                    m_bgPriority[0][0] = 9;  m_bgPriority[0][1] = 13; // BG1 lo/hi
                    m_bgPriority[1][0] = 8;  m_bgPriority[1][1] = 12; // BG2 lo/hi
                    m_bgPriority[2][0] = 2;  m_bgPriority[2][1] = 5;  // BG3 lo/hi (hi beats SP0=3)
                    m_bgPriority[3][0] = 0;  m_bgPriority[3][1] = 0;  // BG4 not used
                    break;
                default:
                    // Other modes: use Mode 0 defaults
                    m_bgPriority[0][0] = 9;  m_bgPriority[0][1] = 13;
                    m_bgPriority[1][0] = 8;  m_bgPriority[1][1] = 12;
                    m_bgPriority[2][0] = 2;  m_bgPriority[2][1] = 5;
                    m_bgPriority[3][0] = 1;  m_bgPriority[3][1] = 4;
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
            // Real SNES: writing $2116 (low byte) only updates VMADD low byte.
            // No prefetch reload on $2116 write alone.
            uint16_t prevVMADD16 = m_vramAddress;
            m_vramAddress = (m_vramAddress & 0xFF00) | value;
            if (frameCount >= 145 && frameCount <= 165) {
                static int va16L = 0; if (va16L < 200) {
                    fprintf(stderr, "[VT] F:%d $2116=%02X addr:0x%04X->0x%04X pref=%02X\n",
                            frameCount, value, prevVMADD16, m_vramAddress, m_vramReadBuffer);
                    va16L++;
                }
            }
            break;
        }

        case 0x2117: { // VMADDH - VRAM Address High
            // Real SNES: writing $2117 (high byte) updates VMADD and IMMEDIATELY
            // reloads the prefetch buffer from the new VMADD address.
            uint16_t prevVMADD17 = m_vramAddress;
            m_vramAddress = (m_vramAddress & 0x00FF) | ((uint16_t)value << 8);
            {
                uint16_t m = applyVRAMMapping(m_vramAddress);
                m_vramReadBuffer  = readVRAM(m * 2);
                m_vramReadBufferH = readVRAM(m * 2 + 1);
            }
            if (frameCount >= 145 && frameCount <= 165) {
                static int va17H = 0; if (va17H < 200) {
                    fprintf(stderr, "[VT] F:%d $2117=%02X addr:0x%04X->0x%04X pref=%02X\n",
                            frameCount, value, prevVMADD17, m_vramAddress, m_vramReadBuffer);
                    va17H++;
                    // Dump first 16 VRAM bytes when VMADD resets to 0
                    if (m_vramAddress == 0) {
                        fprintf(stderr, "[VT] VRAM[0..15].lo:");
                        for (int di = 0; di < 16; di++) fprintf(stderr, " %02X", m_vram[di*2]);
                        fprintf(stderr, "\n");
                    }
                }
            }
            break;
        }

        case 0x2118: { // VMDATAL - VRAM Data Low
            // SNES hardware: VRAM writes are blocked during active display (scanlines 1-224)
            // unless force-blank (INIDISP bit7) is set. Address still increments.
            {
              bool activeDisplay = !m_forcedBlank && (m_scanline >= 1 && m_scanline <= 224);
              uint16_t mw = applyVRAMMapping(m_vramAddress);
              if (!activeDisplay) {
                  writeVRAM(mw * 2, value);
              }
              // Trace VRAM writes to BG1 tilemap (word 0x2000-0x23FF) and BG4 tilemap (word 0x5000-0x53FF) during Character Test
              if (frameCount >= 670 && frameCount <= 800) {
                  bool inBG1map = (mw >= 0x2000 && mw <= 0x23FF);
                  bool inBG4map = (mw >= 0x5000 && mw <= 0x53FF);
                  if (inBG1map || inBG4map) {
                      static int vwMap = 0;
                      if (vwMap < 500) {
                          fprintf(stderr, "[VRAM_MAP] F:%d %s word=0x%04X data=%02X blk=%d scan=%d\n",
                              frameCount, inBG1map ? "BG1" : "BG4", mw, value, (int)activeDisplay, m_scanline);
                          vwMap++;
                      }
                  }
              }
            }
            if (!m_vramIncrAfterHigh) incrementVRAMAddress();
            break;
        }

        case 0x2119: { // VMDATAH - VRAM Data High
            // SNES hardware: VRAM writes are blocked during active display unless force-blank
            {
              bool activeDisplay = !m_forcedBlank && (m_scanline >= 1 && m_scanline <= 224);
              uint16_t mw = applyVRAMMapping(m_vramAddress);
              if (!activeDisplay) {
                  writeVRAM(mw * 2 + 1, value);
              }
              if (frameCount >= 140 && frameCount <= 200 && m_vramAddress < 0x10) {
                  static int vw19 = 0; if (vw19 < 500) { fprintf(stderr, "[VT] F:%d $2119 word=0x%04X->0x%04X data=%02X blocked=%d\n", frameCount, m_vramAddress, mw, value, (int)activeDisplay); vw19++; }
              }
            }
            if (m_vramIncrAfterHigh) incrementVRAMAddress();
            break;
        }
            
        case 0x211A: { // M7SEL - Mode 7 Settings
            m_m7sel = value;
            break;
        }
            
        case 0x211B: { // M7A - Mode 7 Matrix A
            // SNES: writes to M7A-M7D use a single shared latch byte
            // First write = low byte stored in latch; second write = high byte, combine
            m_m7a = (int16_t)((value << 8) | m_m7Latch);
            m_m7Latch = value;
            break;
        }

        case 0x211C: { // M7B - Mode 7 Matrix B
            m_m7b = (int16_t)((value << 8) | m_m7Latch);
            m_m7Latch = value;
            break;
        }

        case 0x211D: { // M7C - Mode 7 Matrix C
            m_m7c = (int16_t)((value << 8) | m_m7Latch);
            m_m7Latch = value;
            break;
        }

        case 0x211E: { // M7D - Mode 7 Matrix D
            m_m7d = (int16_t)((value << 8) | m_m7Latch);
            m_m7Latch = value;
            break;
        }

        case 0x211F: { // M7X - Mode 7 Center X (13-bit signed)
            // Center registers use same latch but sign-extend to 13 bits
            int16_t raw = (int16_t)((value << 8) | m_m7Latch);
            m_m7x = (int16_t)((raw << 3) >> 3);  // sign-extend 13-bit
            m_m7Latch = value;
            break;
        }

        case 0x2120: { // M7Y - Mode 7 Center Y (13-bit signed)
            int16_t raw = (int16_t)((value << 8) | m_m7Latch);
            m_m7y = (int16_t)((raw << 3) >> 3);  // sign-extend 13-bit
            m_m7Latch = value;
            break;
        }
            
        case 0x2121: { // CGADD - CGRAM Address
            // $2121 receives a COLOR INDEX (word address, 0-255).
            // Internal byte address = colorIndex * 2, because each CGRAM entry is 2 bytes.
            m_cgramAddress = (uint16_t)value << 1;
            {
                static int cgaddCount = 0;
                if (frameCount >= 173 && frameCount <= 176 && cgaddCount < 100) {
                    fprintf(stderr, "[CG_A] F:%d $2121=%02X cgramAddr=%03X scan=%d\n",
                            frameCount, value, (int)m_cgramAddress, m_scanline);
                    cgaddCount++;
                } else if (cgaddCount < 3) {
                    std::ostringstream oss;
                    oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0')
                        << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                        << "PPU: CGRAM Address=0x" << std::hex << (int)m_cgramAddress
                        << " (colorIndex=" << std::dec << (int)value << ")" << std::endl;
                    Logger::getInstance().logPPU(oss.str());
                    Logger::getInstance().flush();
                    cgaddCount++;
                }
            }
            break;
        }
            
        case 0x2122: { // CGDATA - CGRAM Data
            // SNES hardware: CGRAM writes are blocked during active display unless force-blank
            // Address still increments on each write.
            {
              bool activeDisplay = !m_forcedBlank && (m_scanline >= 1 && m_scanline <= 224);
              if (!activeDisplay) {
                  writeCGRAM(m_cgramAddress, value);
              }
              m_cgramAddress++;
              m_cgramAddress &= 0x01FF;
              // Log Color Test palette loads (F:1700-2100)
              if (frameCount >= 1700 && frameCount <= 2100 && (m_cgramAddress - 1) % 2 == 0) {
                  static int colorLogCount = 0;
                  if (colorLogCount < 1000) {
                      fprintf(stderr, "[CGRAM-CT] F:%u $2121=%03X $2122=%02X\n",
                              (unsigned)frameCount, (unsigned)(m_cgramAddress - 1), (unsigned)value);
                      colorLogCount++;
                  }
              }
              if (m_cgramAddress <= 10) {
                  std::ostringstream oss;
                  oss << "[Cyc:" << std::dec << std::setw(10) << std::setfill('0')
                      << (m_cpu ? m_cpu->getCycles() : 0) << " F:" << std::setw(4) << std::setfill('0') << frameCount << "] "
                      << "CGRAM[" << std::dec << (m_cgramAddress - 1)
                            << "] = 0x" << std::hex << (int)value
                            << (activeDisplay ? " (BLOCKED-active)" : "") << std::dec << std::endl;
                  Logger::getInstance().logPPU(oss.str());
                  Logger::getInstance().flush();  // Flush at frame end
              }
            }
            break;
        }
            
        case 0x2107: { // BG1SC - BG1 Tilemap Address
            m_bg1MapAddr = ((value & 0xFC) << 8) * 2; // Convert word addr to byte addr
            m_bgMapAddr[0] = m_bg1MapAddr; // Sync array
            m_bgMapSize[0] = (value & 0x03); // Bits 0-1: tilemap size
            // Trace every $2107 write during test frames
            if (frameCount >= 540 && frameCount <= 570) {
                fprintf(stderr, "[BG1SC] F:%d scan=%d val=$%02X mapAddr=$%04X\n",
                    frameCount, m_scanline, value, m_bg1MapAddr);
            }
            break;
        }
            
        case 0x2108: { // BG2SC - BG2 Tilemap Address
            m_bg2MapAddr = ((value & 0xFC) << 8) * 2;
            m_bgMapAddr[1] = m_bg2MapAddr;
            m_bgMapSize[1] = (value & 0x03);
            break;
        }

        case 0x2109: { // BG3SC - BG3 Tilemap Address
            m_bg3MapAddr = ((value & 0xFC) << 8) * 2;
            m_bgMapAddr[2] = m_bg3MapAddr;
            m_bgMapSize[2] = (value & 0x03);
            break;
        }

        case 0x210A: { // BG4SC - BG4 Tilemap Address
            m_bg4MapAddr = ((value & 0xFC) << 8) * 2;
            m_bgMapAddr[3] = m_bg4MapAddr;
            m_bgMapSize[3] = (value & 0x03);
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
            
        case 0x210D: { // BG1HOFS / M7HOFS - BG1 Horizontal Scroll (also Mode 7 HOFS)
            // SNES scroll write protocol: two consecutive writes set the 10-bit value
            // First write:  value goes into bits[7:0]; latch = value
            // bsnes formula: scroll = (data << 8) | (latch & ~7) | (scroll >> 8 & 7)
            // The latch is a single shared PPU latch (written by any BG scroll register)
            {
                m_bg1ScrollX = (value << 8) | (m_scrollPrevX & ~7) | ((m_bg1ScrollX >> 8) & 7);
                // Mode 7 HOFS (13-bit signed): = ((value << 8) | prev) sign-extended from 13
                int m7h = ((int)value << 8) | (int)m_m7hofs_prev;
                m7h = (m7h << 19) >> 19;  // sign-extend 13-bit (shift 32-13=19)
                m_m7hofs = (int16_t)m7h;
                m_m7hofs_prev = value;
                m_scrollPrevX = value;
            }
            static int bg1HofsCount = 0;
            if (bg1HofsCount < 3) {
                std::cout << "PPU: BG1HOFS=$" << std::hex << (int)value << ", ScrollX=0x" << m_bg1ScrollX << std::dec << std::endl;
                bg1HofsCount++;
            }
            break;
        }

        case 0x210E: { // BG1VOFS / M7VOFS - BG1 Vertical Scroll (also Mode 7 VOFS)
            // bsnes formula: scroll = (data << 8) | latch
            m_bg1ScrollY = (value << 8) | m_scrollPrevY;
            // Mode 7 VOFS (13-bit signed)
            int m7v = ((int)value << 8) | (int)m_m7vofs_prev;
            m7v = (m7v << 19) >> 19;  // sign-extend 13-bit
            m_m7vofs = (int16_t)m7v;
            m_m7vofs_prev = value;
            m_scrollPrevY = value;
            break;
        }
            
        case 0x210F: { // BG2HOFS - BG2 Horizontal Scroll
            // bsnes formula: scroll = (data << 8) | (latch & ~7) | (scroll >> 8 & 7)
            m_bg2ScrollX = (value << 8) | (m_scrollPrevX & ~7) | ((m_bg2ScrollX >> 8) & 7);
            m_scrollPrevX = value;
            break;
        }

        case 0x2110: { // BG2VOFS - BG2 Vertical Scroll
            // bsnes formula: scroll = (data << 8) | latch
            m_bg2ScrollY = (value << 8) | m_scrollPrevY;
            m_scrollPrevY = value;
            break;
        }

        case 0x2111: { // BG3HOFS - BG3 Horizontal Scroll
            m_bg3ScrollX = (value << 8) | (m_scrollPrevX & ~7) | ((m_bg3ScrollX >> 8) & 7);
            m_scrollPrevX = value;
            break;
        }

        case 0x2112: { // BG3VOFS - BG3 Vertical Scroll
            m_bg3ScrollY = (value << 8) | m_scrollPrevY;
            m_scrollPrevY = value;
            break;
        }

        case 0x2113: { // BG4HOFS - BG4 Horizontal Scroll
            m_bg4ScrollX = (value << 8) | (m_scrollPrevX & ~7) | ((m_bg4ScrollX >> 8) & 7);
            m_scrollPrevX = value;
            break;
        }

        case 0x2114: { // BG4VOFS - BG4 Vertical Scroll
            m_bg4ScrollY = (value << 8) | m_scrollPrevY;
            m_scrollPrevY = value;
            break;
        }
            
        case 0x2115: { // VMAIN - VRAM Address Increment Mode
            m_vramIncrement = value & 0x03;  // Bits 0-1: increment size (00=1, 01=32, 10/11=128)
            m_vramMapping = (value >> 2) & 0x03;  // Bits 2-3: address mapping
            m_vramIncrAfterHigh = (value & 0x80) != 0;  // bit7: increment timing
            if (frameCount >= 130 && frameCount <= 175) {
                static int vmainTrace = 0;
                if (vmainTrace < 200) {
                    fprintf(stderr, "[VT] F:%d $2115=%02X inc=%d map=%d afterH=%d\n",
                            frameCount, value,
                            m_vramIncrement == 0 ? 1 : m_vramIncrement == 1 ? 32 : 128,
                            m_vramMapping, m_vramIncrAfterHigh ? 1 : 0);
                    vmainTrace++;
                }
            }
            break;
        }
            
        case 0x2102: { // OAMADDL - OAM Address low byte
            // OAMADDL = bits[7:0] of OAM WORD address.
            // Internal byte address = word_address * 2.
            // We store byte address in m_oamAddress (0-543).
            // Preserve bit9 (from OAMADDH) and replace low 8 word-addr bits → byte bits [8:1].
            m_oamAddress = (m_oamAddress & 0x0200) | ((uint16_t)value << 1);
            break;
        }

        case 0x2103: { // OAMADDH - OAM Address high bit + priority rotation
            // Bit 0: MSB of OAM word address → byte address bit 9 (= 512).
            // Bit 7: OAM priority rotation enable (not implemented).
            // $2102 sets bits[8:1] of the byte address (low 8 bits of word addr << 1).
            // $2103 bit0 is word-address bit 8, which maps to byte-address bit 9 (<<9).
            // Preserve low 9 bits from $2102 write, replace bit 9 from $2103 bit 0.
            m_oamAddress = (m_oamAddress & 0x01FF) | (((uint16_t)value & 0x01) << 9);
            break;
        }

        case 0x2104: { // OAMDATA - OAM Data Write
            writeOAM(m_oamAddress, value);
            m_oamAddress++;
            if (m_oamAddress >= 544) m_oamAddress = 0; // OAM is 544 bytes (0x000-0x21F)
            break;
        }

        case 0x212C: { // TM - Main Screen Designation
            uint8_t oldTM = m_mainScreenDesignation;
            m_mainScreenDesignation = value;
            // Trace every TM change to stderr so we can see when OBJ layer (bit4) enables.
            if (value != oldTM) {
                fprintf(stderr, "[TM] F:%d scan=%d val=$%02X (BG1=%d BG2=%d BG3=%d BG4=%d OBJ=%d) was=$%02X\n",
                    frameCount, m_scanline, value,
                    (value >> 0) & 1, (value >> 1) & 1, (value >> 2) & 1,
                    (value >> 3) & 1, (value >> 4) & 1, oldTM);
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
            
        case 0x2132: { // COLDATA - Fixed Color Data
            // Bits [4:0] = 5-bit color intensity
            // Bit  [5]   = apply to Blue
            // Bit  [6]   = apply to Green
            // Bit  [7]   = apply to Red
            m_coldata = value;
            uint8_t intensity = value & 0x1F;
            if (value & 0x80) m_coldataR = intensity;
            if (value & 0x40) m_coldataG = intensity;
            if (value & 0x20) m_coldataB = intensity;
            break;
        }
            
        case 0x2133: { // SETINI - Screen Mode/Video Select
            m_setini = value;
            static bool warnedSETINI = false;
            if (!warnedSETINI) {
                std::cout << "[WARN] SETINI($2133) - Interlace/Overscan/External Sync not implemented - only registers are stored." << std::endl;
                if (value & 0x40) { // Bit 6: EXTBG (Mode 7 extended BG2)
                    std::cout << "[WARN] Mode 7 EXTBG (SETINI bit 6) not implemented - BG2 priority layer does not work." << std::endl;
                }
                warnedSETINI = true;
            }
            break;
        }
            
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
    uint8_t regResult = readRegisterImpl(address);
    // Diagnostic: log specific PPU reads during Electronics Test frames
    if (frameCount >= 130 && frameCount <= 210) {
        // Log H/V counter latch and status registers
        if (address == 0x2137 || address == 0x213C || address == 0x213D ||
            address == 0x213E || address == 0x213F) {
            static int hvLogCount = 0;
            if (hvLogCount < 15000) {
                fprintf(stderr, "[PPU_R] F:%d $%04X=%02X scan=%d dot=%d\n",
                        frameCount, address, regResult, m_scanline, m_dot);
                hvLogCount++;
            }
        }
    }
    return regResult;
}

void PPU::triggerHVLatch() {
    // On real SNES hardware the auto-joypad process pulses the /LATCH pin at VBlank start,
    // which has the same effect as a software read of $2137: latches H/V counters and
    // sets the latch flag (bit7 of $213F / STAT78).
    m_latchedH = m_dot;
    m_latchedV = m_scanline;
    m_hvLatchRead = true;   // bit7 of $213F will return 1 until next $213F read
    m_hvLatchHRead = false; // reset $213C/$213D byte-select flip-flops
    m_hvLatchVRead = false;
}

uint8_t PPU::readRegisterImpl(uint16_t address) {
    switch (address) {
        case 0x2137: { // SLHV - Software Latch for H/V Counter
            // Reading $2137 latches H/V counters AND sets bit7 of $213F (latch flag)
            m_latchedH = m_dot;
            m_latchedV = m_scanline;
            m_hvLatchRead = true;    // SET flag: latch occurred (bit7 of $213F)
            m_hvLatchHRead = false;  // reset read toggles for fresh $213C/$213D reads
            m_hvLatchVRead = false;
            return 0;
        }

        case 0x213C: { // OPHCT - Horizontal Counter (Low/High)
            // First read: return low byte; second read: return bit 8
            // Does NOT affect the latch flag ($213F bit7)
            if (!m_hvLatchHRead) {
                m_hvLatchHRead = true;
                return m_latchedH & 0xFF;
            } else {
                m_hvLatchHRead = false;
                return (m_latchedH >> 8) & 0x01;
            }
        }

        case 0x213D: { // OPVCT - Vertical Counter (Low/High)
            // First read: return low byte; second read: return bit 8
            // Does NOT affect the latch flag ($213F bit7)
            if (!m_hvLatchVRead) {
                m_hvLatchVRead = true;
                return m_latchedV & 0xFF;
            } else {
                m_hvLatchVRead = false;
                return (m_latchedV >> 8) & 0x01;
            }
        }
        
        case 0x2134: // MPYL - Mode 7 multiply result low byte
        case 0x2135: // MPYM - Mode 7 multiply result mid byte
        case 0x2136: { // MPYH - Mode 7 multiply result high byte
            // Result = (int16_t)M7A * (int8_t)(M7B & 0xFF), 24-bit signed
            int32_t product = (int32_t)(int16_t)m_m7a * (int32_t)(int8_t)(m_m7b & 0xFF);
            if (address == 0x2134) return (uint8_t)(product & 0xFF);
            if (address == 0x2135) return (uint8_t)((product >> 8) & 0xFF);
            return (uint8_t)((product >> 16) & 0xFF);
        }

        case 0x2138: { // OAMDATAREAD - OAM Read
            uint8_t result = (m_oamAddress < (uint16_t)m_oam.size()) ? m_oam[m_oamAddress] : 0;
            m_oamAddress++;
            if (m_oamAddress >= 544) m_oamAddress = 0;
            return result;
        }

        case 0x213B: { // CGDATAREAD - CGRAM Read
            uint8_t result = (m_cgramAddress < (uint16_t)m_cgram.size()) ? m_cgram[m_cgramAddress] : 0;
            {
                static int cgRdLog = 0;
                // Log F:174 CGRAM reads for addresses 0x100+ (where DMA wrote) or near test failure
                if (frameCount == 174 && m_scanline >= 115 && cgRdLog < 600) {
                    fprintf(stderr, "[CG_R] F:%d $213B addr=%03X data=%02X scan=%d\n",
                            frameCount, (int)m_cgramAddress, result, m_scanline);
                    cgRdLog++;
                }
            }
            m_cgramAddress = (m_cgramAddress + 1) & 0x01FF;
            return result;
        }

        case 0x213E: { // STAT77 - PPU1 Status and Version
            // Bit 7: Time Over  (sprite pixel rendering budget exceeded, >272px)
            // Bit 6: Range Over (>32 sprites on a scanline)
            // Bits 3-0: PPU1 version (1 on most hardware)
            uint8_t stat = 0x01;
            if (m_timeOver)  stat |= 0x80;  // Bit 7 = Time Over
            if (m_rangeOver) stat |= 0x40;  // Bit 6 = Range Over
            // SNES hardware: reading $213E clears overflow flags
            m_timeOver  = false;
            m_rangeOver = false;
            fprintf(stderr, "[STAT77] F:%d SL:%d STAT=$%02X (TO=%d RO=%d) oam[0..3]=%02X %02X %02X %02X oam[4..7]=%02X %02X %02X %02X\n",
                frameCount, m_scanline, stat,
                (stat>>7)&1, (stat>>6)&1,
                m_oam.size()>0 ? m_oam[0] : 0xFF,
                m_oam.size()>1 ? m_oam[1] : 0xFF,
                m_oam.size()>2 ? m_oam[2] : 0xFF,
                m_oam.size()>3 ? m_oam[3] : 0xFF,
                m_oam.size()>4 ? m_oam[4] : 0xFF,
                m_oam.size()>5 ? m_oam[5] : 0xFF,
                m_oam.size()>6 ? m_oam[6] : 0xFF,
                m_oam.size()>7 ? m_oam[7] : 0xFF);
            // Also dump high table (OAM[512-543]) when overflow is present
            if (stat & 0xC0) {
                fprintf(stderr, "[STAT77] HIGH TABLE: ");
                for (int hti = 512; hti < 544 && hti < (int)m_oam.size(); hti++) {
                    fprintf(stderr, "%02X ", m_oam[hti]);
                }
                fprintf(stderr, "\n");
                // Also dump sprite 4-7 OAM low table bytes
                fprintf(stderr, "[STAT77] SPR4-7: ");
                for (int si = 16; si < 32 && si < (int)m_oam.size(); si++) {
                    fprintf(stderr, "%02X ", m_oam[si]);
                }
                fprintf(stderr, "\n");
            }
            return stat;
        }

        case 0x213F: { // STAT78 - PPU2 Status and Version
            // Bit 7: FIELD - interlace field (toggles each VBlank when $2133 bit0=INTERLACE is set)
            // Bit 6: External latch flag (set by $2137 read or /LATCH pin); cleared on read
            // Bits 3-0: PPU2 version (3 on most hardware)
            // NOTE: Reading $213F also resets the $213C/$213D byte-select flip-flops
            // (same effect as reading $2137 on the toggle bits, per hardware behaviour)
            uint8_t result = 0x03; // Version 3
            if (m_fieldBit)    result |= 0x80; // Bit 7: interlace field (FIELD)
            if (m_hvLatchRead) result |= 0x40; // Bit 6: latch flag (cleared on read)
            m_hvLatchRead  = false; // clear latch flag on read
            m_hvLatchHRead = false; // reset $213C byte-select toggle
            m_hvLatchVRead = false; // reset $213D byte-select toggle
            return result;
        }
            
        case 0x2139: { // VMDATALREAD - VRAM Data Read (low byte)
            // Return prefetched low byte; if afterH=0 (trigger=low): refetch from CURRENT addr, then increment
            uint8_t result = m_vramReadBuffer;
            if (!m_vramIncrAfterHigh) {
                // SNES hardware: refetch MDR from CURRENT address first, then increment
                uint16_t m = applyVRAMMapping(m_vramAddress);
                m_vramReadBuffer  = readVRAM(m * 2);
                m_vramReadBufferH = readVRAM(m * 2 + 1);
                uint16_t inc = (m_vramIncrement == 0) ? 1 : (m_vramIncrement == 1) ? 32 : 128;
                m_vramAddress += inc;
            }
            // afterH=1: NO increment, NO refill - return cached prefetch only
            return result;
        }

        case 0x213A: { // VMDATAHREAD - VRAM Data Read (high byte)
            // Return prefetched high byte; if afterH=1 (trigger=high): refetch from CURRENT addr, then increment
            uint8_t result = m_vramReadBufferH;
            if (m_vramIncrAfterHigh) {
                // SNES hardware: refetch MDR from CURRENT address first, then increment
                uint16_t m2 = applyVRAMMapping(m_vramAddress);
                m_vramReadBuffer  = readVRAM(m2 * 2);
                m_vramReadBufferH = readVRAM(m2 * 2 + 1);
                uint16_t inc = (m_vramIncrement == 0) ? 1 : (m_vramIncrement == 1) ? 32 : 128;
                m_vramAddress += inc;
            }
            // afterH=0: NO increment, NO refill - return cached prefetch only
            return result;
        }
            
        case 0x4211: { // TIMEUP - H/V IRQ Flag
            // Bit 7: IRQ pending flag (set when H/V timer fires, cleared on read)
            uint8_t result = m_irqFlag ? 0x80 : 0x00;
            m_irqFlag = false;  // Clear on read
            return result;
        }

        case 0x4210: { // RDNMI - NMI Flag and Version
            // Bit 7: NMI flag (cleared on read - SNES hardware behavior)
            // Bits 0-3: CPU version
            uint8_t result = 0x02;  // CPU version 2

            // SNES RDNMI hardware behavior:
            // - Bit 7 is set ONCE when VBlank starts (scanline 225)
            // - Bit 7 is automatically cleared after reading $4210
            // - This means polling $4210 twice during same VBlank: 1st read = 0x82, 2nd read = 0x02
            // - Flag is also cleared at frame start (redundant safety)
            // - This allows wait-for-VBlank loops to work correctly across frame boundaries

            if (m_nmiFlag) {
                result |= 0x80;
                m_nmiFlag = false;  // Clear on read (hardware auto-clears)
                static int rdnmiClearCount = 0;
                if (rdnmiClearCount < 20) {
                    fprintf(stderr, "[RDNMI-CLEAR] F:%d scan=%d → $82 cleared (NMI ack)\n",
                        frameCount, m_scanline);
                    rdnmiClearCount++;
                }
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
        invalidateTileCache(address);
        
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

void PPU::writeCGRAM(uint16_t address, uint8_t value) {
    // address is a byte address (0-511); each CGRAM color entry is 2 bytes
    if (address < m_cgram.size()) {
        m_cgram[address] = value;
    }
}

void PPU::writeOAM(uint16_t address, uint8_t value) {
    if (address >= 0x0200) {
        // High table (0x0200-0x021F): single-byte writes, no latch
        if (address < (uint16_t)m_oam.size()) {
            m_oam[address] = value;
        }
    } else {
        // Low table (0x0000-0x01FF): write-twice latch
        if (!(address & 1)) {
            // Even address: buffer in latch, don't write yet
            m_oamLatchByte  = value;
            m_oamLatchValid = true;
        } else {
            // Odd address: commit latched even byte + this odd byte
            uint16_t evenAddr = address & ~1u;
            if (evenAddr < (uint16_t)m_oam.size())
                m_oam[evenAddr] = m_oamLatchByte;
            if (address < (uint16_t)m_oam.size())
                m_oam[address] = value;
            m_oamLatchValid = false;
        }
    }
}

void PPU::invalidateTileCache(uint16_t address) {
    auto invalidate = [&](std::vector<TileCache>& cache, uint16_t tileSize) {
        uint32_t idx = address / tileSize;
        if (idx < cache.size()) {
            cache[idx].valid = false;
        }
    };

    invalidate(m_tileCache2bpp, 16);
    invalidate(m_tileCache4bpp, 32);
    invalidate(m_tileCache8bpp, 64);
}

TileCache* PPU::getTileCacheEntry(uint16_t tileAddr, int bpp) {
    uint32_t idx = 0;
    std::vector<TileCache>* cache = nullptr;
    uint16_t tileSize = 0;

    switch (bpp) {
        case 2:
            tileSize = 16;
            cache = &m_tileCache2bpp;
            break;
        case 4:
            tileSize = 32;
            cache = &m_tileCache4bpp;
            break;
        case 8:
            tileSize = 64;
            cache = &m_tileCache8bpp;
            break;
        default:
            return nullptr;
    }

    idx = tileAddr / tileSize;
    if (idx >= cache->size()) {
        return nullptr;
    }

    TileCache& entry = (*cache)[idx];
    if (!entry.valid) {
        const uint8_t* tileData = &m_vram[tileAddr];
        switch (bpp) {
            case 2: decode2bpp(tileData, entry.pixels); break;
            case 4: decode4bpp(tileData, entry.pixels); break;
            case 8: decode8bpp(tileData, entry.pixels); break;
        }
        entry.valid = true;
    }

    return &entry;
}

void PPU::decode2bpp(const uint8_t* tileData, uint8_t output[8][8]) {
    for (int y = 0; y < 8; ++y) {
        uint8_t plane0 = tileData[y * 2 + 0];
        uint8_t plane1 = tileData[y * 2 + 1];

        for (int x = 0; x < 8; ++x) {
            int bitPos = 7 - x;
            uint8_t bit0 = (plane0 >> bitPos) & 1;
            uint8_t bit1 = (plane1 >> bitPos) & 1;
            output[y][x] = (bit1 << 1) | bit0;
        }
    }
}

void PPU::decode4bpp(const uint8_t* tileData, uint8_t output[8][8]) {
    for (int y = 0; y < 8; ++y) {
        uint8_t plane0 = tileData[y * 2 + 0];
        uint8_t plane1 = tileData[y * 2 + 1];
        uint8_t plane2 = tileData[16 + y * 2 + 0];
        uint8_t plane3 = tileData[16 + y * 2 + 1];

        for (int x = 0; x < 8; ++x) {
            int bitPos = 7 - x;
            uint8_t bit0 = (plane0 >> bitPos) & 1;
            uint8_t bit1 = (plane1 >> bitPos) & 1;
            uint8_t bit2 = (plane2 >> bitPos) & 1;
            uint8_t bit3 = (plane3 >> bitPos) & 1;
            output[y][x] = (bit3 << 3) | (bit2 << 2) | (bit1 << 1) | bit0;
        }
    }
}

void PPU::decode8bpp(const uint8_t* tileData, uint8_t output[8][8]) {
    for (int y = 0; y < 8; ++y) {
        uint8_t planes[8];
        for (int p = 0; p < 8; ++p) {
            uint16_t base = (p / 2) * 16;
            planes[p] = tileData[base + y * 2 + (p & 1)];
        }

        for (int x = 0; x < 8; ++x) {
            int bitPos = 7 - x;
            uint8_t color = 0;
            for (int p = 0; p < 8; ++p) {
                color |= ((planes[p] >> bitPos) & 1) << p;
            }
            output[y][x] = color;
        }
    }
}

// ============================================================
// Sprite rendering — full SNES OAM implementation
// OAM layout:
//   [0..511]  = 128 sprites × 4 bytes: X(8), Y(8), tile(8), attr(8)
//     attr byte: [7]=vflip [6]=hflip [5:4]=priority [3]=name_table [2:0]=palette
//   [512..543] = 32 bytes of extended data: 4 sprites per byte
//     bits [1:0] = sprite 4i+0: [1]=size [0]=X bit8
//     bits [3:2] = sprite 4i+1: [3]=size [2]=X bit8
//     bits [5:4] = sprite 4i+2: [5]=size [4]=X bit8
//     bits [7:6] = sprite 4i+3: [7]=size [6]=X bit8
//
// OBSEL ($2101):
//   bits [2:0] = name base (sprite base tile address = bits * 0x2000 / 2 words)
//               (name base selects which 4KB block in VRAM for name table 0)
//   bits [4:3] = name select (gap between name table 0 and 1, in units of 0x1000 words)
//   bits [7:5] = size select (0=8/16, 1=8/32, 2=8/64, 3=16/32, 4=16/64, 5=32/64)
//
// Sprite sizes (small/large):
//   0: 8x8  / 16x16
//   1: 8x8  / 32x32
//   2: 8x8  / 64x64
//   3: 16x16/ 32x32
//   4: 16x16/ 64x64
//   5: 32x32/ 64x64
// ============================================================
uint32_t PPU::renderSprites(int x, int y) {
    // Delegated to renderSpritePixel — kept for API compatibility
    PixelInfo p = renderSpritePixel(x, y);
    return p.color;
}

PixelInfo PPU::renderSpritePixel(int x, int y) {
    // ---- Decode OBSEL ($2101) ----
    // Standard SNES formula: sprite tile base byte address = (bits & 7) << 14
    // name select gap (bytes) = (N+1) << 13
    uint32_t nameBase0 = ((uint32_t)(m_objSize & 0x07)) << 14;
    uint8_t nameSelect = (m_objSize >> 3) & 0x03;
    uint32_t nameBase1 = (nameBase0 + (((uint32_t)(nameSelect) + 1) << 13)) & 0xFFFF;

    // --- Diagnostic: dump OAM + sprite eval context once per frame for target frames ---
    // Limited to frames 670-705 (post TC-02 OBJ enable) to avoid log flooding.
    // Fires only at scanline 0 + x irrelevant here since we're per-pixel; key off a static frame guard.
    {
        static int sprDumpFrame = -1;
        if (frameCount >= 670 && frameCount <= 705 && sprDumpFrame != frameCount && y == 0 && x == 0) {
            sprDumpFrame = frameCount;
            fprintf(stderr, "[OAM-CTX] F:%d OBSEL=$%02X nameBase0=$%05X nameBase1=$%05X sizeMode=%d TM=$%02X\n",
                frameCount, m_objSize, nameBase0, nameBase1, (m_objSize >> 5) & 7, m_mainScreenDesignation);
            // First 16 OAM entries (X,Y,tile,attr) + first 4 ext bytes
            for (int s = 0; s < 16; s++) {
                int b = s * 4;
                if (b + 3 >= (int)m_oam.size()) break;
                int ei = 512 + (s / 4);
                uint8_t eb = (ei < (int)m_oam.size()) ? ((m_oam[ei] >> ((s % 4) * 2)) & 0x03) : 0;
                fprintf(stderr, "  [OAM-DUMP] s%02d X=%02X Y=%02X T=%02X A=%02X ext=%X\n",
                    s, m_oam[b+0], m_oam[b+1], m_oam[b+2], m_oam[b+3], eb);
            }
        }
    }

    // Size select: bits[7:5] — 8 possible modes (0-7)
    // Extended with modes 6 (16x32/32x64) and 7 (16x32/32x32) per official docs
    uint8_t sizeMode = (m_objSize >> 5) & 0x07;
    static const int smallW[8] = {8, 8, 8, 16, 16, 32, 16, 16};
    static const int smallH[8] = {8, 8, 8, 16, 16, 32, 32, 32};
    static const int largeW[8] = {16, 32, 64, 32, 64, 64, 32, 32};
    static const int largeH[8] = {16, 32, 64, 32, 64, 64, 64, 32};

    // Track highest priority sprite pixel encountered so far (SNES composites all sprites,
    // with OAM-earlier sprites winning ties at the same priority).
    // Iterate OAM 0..127; first visible opaque pixel at a given priority wins.
    PixelInfo best = {0, 0};
    bool haveBest = false;
    // Counters for diagnostic summary (per-pixel call)
    int dbgPassYX = 0;   // sprites that passed Y+X bounds test
    int dbgTransp = 0;   // passed bounds but pixel was transparent
    int dbgOpaque = 0;   // passed bounds AND opaque

    for (int s = 0; s < 128; s++) {
        int oamBase = s * 4;
        if (oamBase + 3 >= (int)m_oam.size()) break;

        uint8_t attr   = m_oam[oamBase + 3];
        uint8_t tileN  = m_oam[oamBase + 2];
        uint8_t spY    = m_oam[oamBase + 1];
        uint8_t spXlow = m_oam[oamBase + 0];

        // Extended OAM byte index: byte 512 + s/4, bit pair s%4*2
        int extIdx  = 512 + (s / 4);
        int extShift = (s % 4) * 2;
        uint8_t extBits = 0;
        if (extIdx < (int)m_oam.size()) {
            extBits = (m_oam[extIdx] >> extShift) & 0x03;
        }
        bool isLarge  = (extBits >> 1) & 1;
        bool xBit8    = (extBits >> 0) & 1;

        // Full 9-bit signed X position
        int sprX = (int)spXlow | ((int)xBit8 << 8);
        if (sprX >= 256) sprX -= 512;  // sign-extend: 0x100..0x1FF → -256..-1

        // Sprite dimensions
        int sw = isLarge ? largeW[sizeMode] : smallW[sizeMode];
        int sh = isLarge ? largeH[sizeMode] : smallH[sizeMode];

        // SNES sprite Y formula: lineOffset = (scanline - spY) & 0xFF
        // Sprite is on this scanline if lineOffset < height
        int lineOffset = (y - (int)spY) & 0xFF;
        if (lineOffset >= sh) continue;

        // Check if pixel x is within the sprite's horizontal range
        if (x < sprX || x >= sprX + sw) continue;

        // Pixel offset within sprite
        int px = x - sprX;
        int py = lineOffset;
        dbgPassYX++;

        // OAM byte 3 attribute layout (per bsnes/ares):
        //   bit 0: name select (selects name table 0 or 1)
        //   bits 1-3: palette (0-7, selects from 8 sprite palettes)
        //   bits 4-5: priority (0-3)
        //   bit 6: horizontal flip
        //   bit 7: vertical flip
        bool nameTableBit = attr & 0x01;
        bool hFlip = (attr >> 6) & 1;
        bool vFlip = (attr >> 7) & 1;

        if (hFlip) px = sw - 1 - px;
        if (vFlip) py = sh - 1 - py;

        // Which 8x8 sub-tile are we in?
        int subTileX = px / 8;
        int subTileY = py / 8;
        int tilePixelX = px % 8;
        int tilePixelY = py % 8;

        // Tile number: SNES sprite sub-tile layout is a 16×16 grid where:
        //   row    = (tileN >> 4) + subTileY, wrapped in high nibble (16 rows)
        //   column = (tileN & 0x0F) + subTileX, wrapped in low nibble (16 cols)
        // Each nibble wraps independently so moving right past the 16th column does NOT
        // bleed into the next row of tile numbers. This matches bsnes/ares behavior.
        uint8_t tileRow = ((tileN >> 4) + (uint8_t)subTileY) & 0x0F;
        uint8_t tileCol = ((tileN & 0x0F) + (uint8_t)subTileX) & 0x0F;
        uint8_t actualTile = (uint8_t)((tileRow << 4) | tileCol);

        // Select name table based on OAM attr bit 0
        uint32_t nameBase = nameTableBit ? nameBase1 : nameBase0;

        // Byte address of this 8×8 tile in VRAM (4bpp = 32 bytes/tile)
        uint32_t tileByteAddr = (nameBase + (uint32_t)actualTile * 32) & 0xFFFF; // wrap at 64KB
        if (tileByteAddr + 32 > m_vram.size()) continue;

        // Decode pixel from 4bpp tile
        TileCache* tc = getTileCacheEntry((uint16_t)(tileByteAddr & 0xFFFF), 4);
        if (!tc) continue;

        uint8_t pixelIndex = tc->pixels[tilePixelY][tilePixelX];

        // --- Diagnostic: sprite evaluation + pixel value, first few per frame ---
        {
            static int sprEvalFrame = -1;
            static int sprEvalCount = 0;
            if (frameCount >= 670 && frameCount <= 705) {
                if (sprEvalFrame != frameCount) { sprEvalFrame = frameCount; sprEvalCount = 0; }
                if (sprEvalCount < 32) {
                    fprintf(stderr, "[SPR-EVAL] F:%d scan=%d x=%d s=%d sprX=%d spY=%d sw=%d sh=%d tileN=$%02X actT=$%02X tileAddr=$%05X px=%d,%d pix=%d attr=$%02X\n",
                        frameCount, y, x, s, sprX, spY, sw, sh, tileN, actualTile, tileByteAddr,
                        tilePixelX, tilePixelY, pixelIndex, attr);
                    sprEvalCount++;
                }
            }
        }

        if (pixelIndex == 0) { dbgTransp++; continue; }  // transparent
        dbgOpaque++;

        // Priority: bits[5:4] of attr (0-3, 3 = highest)
        uint8_t priority = (attr >> 4) & 0x03;

        // SNES composites sprites so that at a given OAM priority, the OAM-earlier
        // sprite wins. Across different priorities, higher priority wins regardless
        // of OAM order. Keep the current best if its priority is higher, otherwise
        // adopt this one. Use strict '>' so that among same-priority opaque pixels
        // the OAM-earlier sprite (already recorded as 'best') wins.
        if (haveBest && priority <= best.priority) continue;

        // Palette: bits[1:3] of attr select sprite sub-palette (0-7)
        // Sprite palettes occupy CGRAM entries 128..255 (palette indices 8..15 in 4bpp terms)
        uint8_t palette = 8 + ((attr >> 1) & 0x07);
        uint32_t color = getColor(palette, pixelIndex, 4);

        // --- Diagnostic: CGRAM lookup result ---
        {
            static int sprPxFrame = -1;
            static int sprPxCount = 0;
            if (frameCount >= 670 && frameCount <= 705) {
                if (sprPxFrame != frameCount) { sprPxFrame = frameCount; sprPxCount = 0; }
                if (sprPxCount < 8) {
                    uint16_t cgIdx = (palette * 16 + pixelIndex) * 2;
                    uint16_t cgw = (cgIdx + 1 < m_cgram.size()) ? (m_cgram[cgIdx] | (m_cgram[cgIdx+1] << 8)) : 0;
                    fprintf(stderr, "[SPR-CGRAM] F:%d scan=%d x=%d pal=%d pix=%d cgIdx=%u cgRaw=$%04X color=$%08X pri=%d\n",
                        frameCount, y, x, palette, pixelIndex, cgIdx, cgw, color, priority);
                    sprPxCount++;
                }
            }
        }

        best = {color, priority};
        haveBest = true;

        // Early-exit: priority 3 is the maximum; no later sprite can override it.
        if (priority == 3) break;
    }

    // --- Diagnostic: per-frame summary of sprite pixel pipeline ---
    // Prints ONCE per frame at first pixel that had at least one Y+X pass, so we can tell:
    //   passYX > 0 && opaque == 0 → tile data is all 0 (gate 2: empty VRAM / wrong base)
    //   passYX == 0              → no sprite covers this pixel (likely OAM empty or all offscreen)
    //   opaque > 0               → we'll have best; priority/compositing is the issue downstream
    if (frameCount >= 670 && frameCount <= 705 && dbgPassYX > 0) {
        static int sprSumFrame = -1;
        static int sprSumCount = 0;
        if (sprSumFrame != frameCount) { sprSumFrame = frameCount; sprSumCount = 0; }
        if (sprSumCount < 4) {
            fprintf(stderr, "[SPR-SUM] F:%d scan=%d x=%d passYX=%d transp=%d opaque=%d haveBest=%d bestColor=$%08X\n",
                frameCount, y, x, dbgPassYX, dbgTransp, dbgOpaque, haveBest ? 1 : 0, best.color);
            sprSumCount++;
        }
    }

    return haveBest ? best : PixelInfo{0, 0};
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

// Render Sub Screen — used as source for color math
// Sub screen renders the same BG layers as main screen, but using m_subScreenDesignation
// and without window masking (windows on sub screen use TSW register).
uint32_t PPU::renderSubScreen(int x) {
    int y = m_scanline;

    auto signScroll = [](uint16_t v) -> int {
        int s = v & 0x03FF;
        if (s >= 512) s -= 1024;
        return s;
    };

    PixelInfo bgLayerPixels[4] = {{0,0},{0,0},{0,0},{0,0}};

    switch (m_bgMode) {
        case 0:
            bgLayerPixels[0] = sampleBGLayer(0, x, y, signScroll(m_bg1ScrollX), signScroll(m_bg1ScrollY), 2, m_bgTileSize[0]);
            bgLayerPixels[1] = sampleBGLayer(1, x, y, signScroll(m_bg2ScrollX), signScroll(m_bg2ScrollY), 2, m_bgTileSize[1]);
            bgLayerPixels[2] = sampleBGLayer(2, x, y, signScroll(m_bg3ScrollX), signScroll(m_bg3ScrollY), 2, m_bgTileSize[2]);
            bgLayerPixels[3] = sampleBGLayer(3, x, y, signScroll(m_bg4ScrollX), signScroll(m_bg4ScrollY), 2, m_bgTileSize[3]);
            break;
        case 1:
            bgLayerPixels[0] = sampleBGLayer(0, x, y, signScroll(m_bg1ScrollX), signScroll(m_bg1ScrollY), 4, m_bgTileSize[0]);
            bgLayerPixels[1] = sampleBGLayer(1, x, y, signScroll(m_bg2ScrollX), signScroll(m_bg2ScrollY), 4, m_bgTileSize[1]);
            bgLayerPixels[2] = sampleBGLayer(2, x, y, signScroll(m_bg3ScrollX), signScroll(m_bg3ScrollY), 2, m_bgTileSize[2]);
            break;
        case 2: {
            int optX=0, optY=0;
            getOPTOffsets(m_vram, m_bgMapAddr[2], m_bgMapSize[2], x, y, m_bgTileSize[0]?16:8, optX, optY);
            bgLayerPixels[0] = sampleBGLayer(0, x, y, signScroll(m_bg1ScrollX)+optX, signScroll(m_bg1ScrollY)+optY, 4, m_bgTileSize[0]);
            bgLayerPixels[1] = sampleBGLayer(1, x, y, signScroll(m_bg2ScrollX)+optX, signScroll(m_bg2ScrollY)+optY, 4, m_bgTileSize[1]);
            break;
        }
        case 3:
            bgLayerPixels[0] = sampleBGLayer(0, x, y, signScroll(m_bg1ScrollX), signScroll(m_bg1ScrollY), 8, m_bgTileSize[0]);
            bgLayerPixels[1] = sampleBGLayer(1, x, y, signScroll(m_bg2ScrollX), signScroll(m_bg2ScrollY), 4, m_bgTileSize[1]);
            break;
        case 4: {
            int optX=0, optY=0;
            getOPTOffsets(m_vram, m_bgMapAddr[2], m_bgMapSize[2], x, y, m_bgTileSize[0]?16:8, optX, optY);
            bgLayerPixels[0] = sampleBGLayer(0, x, y, signScroll(m_bg1ScrollX)+optX, signScroll(m_bg1ScrollY)+optY, 8, m_bgTileSize[0]);
            bgLayerPixels[1] = sampleBGLayer(1, x, y, signScroll(m_bg2ScrollX)+optX, signScroll(m_bg2ScrollY)+optY, 2, m_bgTileSize[1]);
            break;
        }
        case 5: {
            int hiresX = x * 2;
            bgLayerPixels[0] = sampleBGLayer(0, hiresX, y, signScroll(m_bg1ScrollX), signScroll(m_bg1ScrollY), 4, m_bgTileSize[0]);
            bgLayerPixels[1] = sampleBGLayer(1, hiresX, y, signScroll(m_bg2ScrollX), signScroll(m_bg2ScrollY), 2, m_bgTileSize[1]);
            break;
        }
        case 6: {
            int optX=0, optY=0;
            getOPTOffsets(m_vram, m_bgMapAddr[2], m_bgMapSize[2], x, y, 8, optX, optY);
            int hiresX = x * 2;
            bgLayerPixels[0] = sampleBGLayer(0, hiresX, y, signScroll(m_bg1ScrollX)+optX, signScroll(m_bg1ScrollY)+optY, 4, m_bgTileSize[0]);
            break;
        }
        case 7: {
            uint32_t c7 = renderBackgroundMode7(x);
            uint32_t backdropSub = getBGColorFromCGRAM(m_cgram);
            bgLayerPixels[0] = {c7, (c7 == backdropSub) ? (uint8_t)0 : (uint8_t)1};
            break;
        }
        default:
            break;
    }

    // Apply sub-screen window masking (TSW register, $212F)
    // Sub-screen window logic mirrors main screen but uses m_tsw for enable bits.
    // We reuse the same windowMaskLayer logic inline here.
    bool inWin1sub = (x >= (int)m_wh0 && x <= (int)m_wh1);
    bool inWin2sub = (x >= (int)m_wh2 && x <= (int)m_wh3);
    auto subWindowMask = [&](int layerBit) -> bool {
        uint8_t wsel;
        uint8_t wlogic;
        int wselShift, wlogicShift;
        switch (layerBit) {
            case 0: wsel=m_w12sel; wselShift=0; wlogic=m_wbglog;  wlogicShift=0; break;
            case 1: wsel=m_w12sel; wselShift=4; wlogic=m_wbglog;  wlogicShift=2; break;
            case 2: wsel=m_w34sel; wselShift=0; wlogic=m_wbglog;  wlogicShift=4; break;
            case 3: wsel=m_w34sel; wselShift=4; wlogic=m_wbglog;  wlogicShift=6; break;
            case 4: wsel=m_wobjsel;wselShift=0; wlogic=m_wobjlog; wlogicShift=0; break;
            default: return false;
        }
        bool w1en  = (wsel >> (wselShift + 1)) & 1;
        bool w1inv = (wsel >> (wselShift + 0)) & 1;
        bool w2en  = (wsel >> (wselShift + 3)) & 1;
        bool w2inv = (wsel >> (wselShift + 2)) & 1;
        bool w1inside = w1en && (inWin1sub ^ w1inv);
        bool w2inside = w2en && (inWin2sub ^ w2inv);
        uint8_t logic = (wlogic >> wlogicShift) & 0x03;
        bool masked;
        if (!w1en && !w2en) masked = false;
        else if (w1en && !w2en) masked = w1inside;
        else if (!w1en && w2en) masked = w2inside;
        else {
            switch (logic) {
                case 0: masked = w1inside || w2inside;   break;
                case 1: masked = w1inside && w2inside;   break;
                case 2: masked = w1inside ^  w2inside;   break;
                case 3: masked = !(w1inside ^ w2inside); break;
                default: masked = false; break;
            }
        }
        return masked;
    };

    for (int i = 0; i < 4; i++) {
        if (bgLayerPixels[i].color != 0 && (m_tsw & (1 << i))) {
            if (subWindowMask(i)) bgLayerPixels[i] = {0, 0};
        }
    }

    // Composite sub screen using sub-screen designation (TS register)
    uint32_t subPixel = getBGColorFromCGRAM(m_cgram);
    uint8_t maxPri = 0;
    bool found = false;
    for (int i = 0; i < 4; i++) {
        if (!(m_subScreenDesignation & (1 << i))) continue;
        if (bgLayerPixels[i].color == 0) continue;
        if (!found || bgLayerPixels[i].priority >= maxPri) {
            maxPri = bgLayerPixels[i].priority;
            subPixel = bgLayerPixels[i].color;
            found = true;
        }
    }
    return subPixel;
}

// Apply Color Math (Add/Sub mode)
// CGADSUB ($2131):
//   bit 7 = subtract mode (0=add, 1=subtract)
//   bit 6 = half mode (divide result by 2)
//   bits 5-0 = layer enable (bit5=backdrop, bit4=OBJ, bit3=BG4, bit2=BG3, bit1=BG2, bit0=BG1)
// CGWSEL ($2130): MMCC DD.S
//   bits 7-6 (MM) = Prevent Color Math area (0=never, 1=outside win, 2=inside win, 3=always)
//   bits 5-4 (CC) = Clip main screen to black (0=never, 1=outside win, 2=inside win, 3=always)
//   bits 3-2 (DD) = Color Math Enable area (0=always, 1=inside win only, 2=outside win only, 3=never)
//   bit 1 = subscreen source (0=fixed color, 1=subscreen)
//   bit 0 = direct color mode for 256-color BGs (not implemented here)
uint32_t PPU::applyColorMath(uint32_t mainColor, uint32_t subColor) {
    // Extract 5-bit SNES color channels from 8-bit RGBA
    // Our framebuffer is R|G<<8|B<<16|A<<24 (little-endian RGBA8888)
    // Convert back to 5-bit for SNES arithmetic, then back to 8-bit
    uint8_t mainR5 = (mainColor & 0xFF) >> 3;
    uint8_t mainG5 = ((mainColor >> 8) & 0xFF) >> 3;
    uint8_t mainB5 = ((mainColor >> 16) & 0xFF) >> 3;

    uint8_t subR5  = (subColor & 0xFF) >> 3;
    uint8_t subG5  = ((subColor >> 8) & 0xFF) >> 3;
    uint8_t subB5  = ((subColor >> 16) & 0xFF) >> 3;

    bool subtractMode = (m_cgadsub & 0x80) != 0;
    bool halfMode     = (m_cgadsub & 0x40) != 0;

    int finalR5, finalG5, finalB5;

    if (subtractMode) {
        // Subtract: clamp to 0
        finalR5 = (int)mainR5 - (int)subR5;
        finalG5 = (int)mainG5 - (int)subG5;
        finalB5 = (int)mainB5 - (int)subB5;
        if (finalR5 < 0) finalR5 = 0;
        if (finalG5 < 0) finalG5 = 0;
        if (finalB5 < 0) finalB5 = 0;
    } else {
        // Add: clamp to 31
        finalR5 = (int)mainR5 + (int)subR5;
        finalG5 = (int)mainG5 + (int)subG5;
        finalB5 = (int)mainB5 + (int)subB5;
        if (finalR5 > 31) finalR5 = 31;
        if (finalG5 > 31) finalG5 = 31;
        if (finalB5 > 31) finalB5 = 31;
    }

    if (halfMode) {
        // Divide by 2 (arithmetic right shift)
        finalR5 >>= 1;
        finalG5 >>= 1;
        finalB5 >>= 1;
    }

    // Convert 5-bit back to 8-bit
    uint8_t r = (uint8_t)(finalR5 << 3) | (finalR5 >> 2);
    uint8_t g = (uint8_t)(finalG5 << 3) | (finalG5 >> 2);
    uint8_t b = (uint8_t)(finalB5 << 3) | (finalB5 >> 2);

    return (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | (0xFF << 24);
}

// Apply VRAM address translation (VMAIN bits 3-2) to a WORD address.
// Translation modes interleave tile plane bits for certain DMA patterns.
// Source: snes9x PPUADDR_Translation
uint16_t PPU::applyVRAMMapping(uint16_t addr) const {
    // SNES VRAM has 32768 word addresses (15-bit: 0x0000-0x7FFF).
    // Mask to 15 bits first to prevent out-of-bounds access when address
    // wraps beyond 0x7FFF (e.g., after a 65536-byte clear DMA).
    addr &= 0x7FFF;
    switch (m_vramMapping) {
        case 0: return addr;
        case 1: return (addr & 0xFF00) | ((addr & 0x001F) << 3) | ((addr >> 5) & 0x0007);
        case 2: return (addr & 0xFE00) | ((addr & 0x003F) << 3) | ((addr >> 6) & 0x0007);
        case 3: return (addr & 0xFC00) | ((addr & 0x007F) << 3) | ((addr >> 7) & 0x0007);
    }
    return addr;
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
    
    // Do NOT pre-fill CGRAM with test colors — the game will initialize CGRAM via DMA/CPU writes.
    // Pre-filling would override the game's palette and cause wrong colors.
    // CGRAM is already zero-initialized from the constructor (black = transparent backdrop).
    oss.str("");
    oss.clear();
    oss << "CGRAM left at zero — game will write palette via $2121/$2122" << std::endl;
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
