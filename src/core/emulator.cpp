#include "emulator.h"
#include "cpu/cpu.h"
#include "ppu/ppu.h"
#include "apu/apu.h"
#include "memory/memory.h"
#include "input/input.h"
#include "debug/debugger.h"
#include <iostream>
#include <chrono>

SNESEmulator::SNESEmulator() 
    : m_window(nullptr)
    , m_renderer(nullptr)
    , m_running(false)
    , m_paused(false)
    , m_debugMode(false)
    , m_frameCount(0)
    , m_lastFrameTime(0) {
}

SNESEmulator::~SNESEmulator() {
    cleanup();
}

bool SNESEmulator::initialize() {
    // SDL 윈도우 생성
    m_window = SDL_CreateWindow(
        "SNES Emulator",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1024, 768,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    
    if (!m_window) {
        std::cerr << "Window creation failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // 렌더러 생성
    m_renderer = SDL_CreateRenderer(m_window, -1, SDL_RENDERER_ACCELERATED);
    if (!m_renderer) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << std::endl;
        return false;
    }
    
    // 컴포넌트 초기화
    m_memory = std::make_unique<Memory>();
    m_cpu = std::make_unique<CPU>(m_memory.get());
    m_ppu = std::make_unique<PPU>(m_memory.get());
    m_apu = std::make_unique<APU>(m_memory.get());
    m_input = std::make_unique<Input>();
    m_debugger = std::make_unique<Debugger>(m_cpu.get(), m_memory.get());
    
    // Set CPU pointers for cycle tracking
    m_ppu->setCPU(m_cpu.get());
    m_apu->setCPU(m_cpu.get());
    
    std::cout << "Emulator initialization complete" << std::endl;
    return true;
}

void SNESEmulator::cleanup() {
    if (m_renderer) {
        SDL_DestroyRenderer(m_renderer);
        m_renderer = nullptr;
    }
    
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
}

void SNESEmulator::run() {
    if (!initialize()) {
        throw std::runtime_error("에뮬레이터 초기화 실패");
    }
    
    m_running = true;
    mainLoop();
}

void SNESEmulator::run(const std::string& romPath) {
    if (!initialize()) {
        throw std::runtime_error("Emulator initialization failed");
    }
    
    std::cout << "Loading ROM: " << romPath << std::endl;
    if (!loadROM(romPath)) {
        throw std::runtime_error("Failed to load ROM: " + romPath);
    }
    
    std::cout << "ROM loaded successfully!" << std::endl;
    
    m_running = true;
    mainLoop();
}

void SNESEmulator::mainLoop() {
    auto lastTime = std::chrono::high_resolution_clock::now();
    
    while (m_running) {
        auto currentTime = std::chrono::high_resolution_clock::now();
        auto deltaTime = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - lastTime);
        lastTime = currentTime;
        
        handleEvents();
        
        if (!m_paused) {
            update();
        }
        
        render();
        
        // 60 FPS 제한
        SDL_Delay(16); // 약 60 FPS
    }
}

void SNESEmulator::handleEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                m_running = false;
                break;
                
            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_F1) {
                    m_debugMode = !m_debugMode;
                    std::cout << "디버그 모드: " << (m_debugMode ? "ON" : "OFF") << std::endl;
                } else if (event.key.keysym.sym == SDLK_SPACE) {
                    m_paused = !m_paused;
                    std::cout << "일시정지: " << (m_paused ? "ON" : "OFF") << std::endl;
                }
                break;
        }
    }
}

void SNESEmulator::update() {
    // CPU 실행 (한 프레임당 약 21,477 클록 사이클)
    for (int i = 0; i < 21477; ++i) {
        m_cpu->step();
    }
    
    // PPU 업데이트
    m_ppu->update();
    
    // APU 업데이트
    m_apu->update();
    
    m_frameCount++;
}

void SNESEmulator::render() {
    // 화면 지우기
    SDL_SetRenderDrawColor(m_renderer, 0, 0, 0, 255);
    SDL_RenderClear(m_renderer);
    
    // PPU에서 화면 렌더링
    m_ppu->render(m_renderer);
    
    // 디버그 모드일 때 디버그 정보 표시
    if (m_debugMode) {
        m_debugger->render(m_renderer);
    }
    
    // 화면 업데이트
    SDL_RenderPresent(m_renderer);
}

bool SNESEmulator::loadROM(const std::string& romPath) {
    std::cout << "ROM 로딩: " << romPath << std::endl;
    
    if (!m_memory) {
        std::cerr << "Memory not initialized" << std::endl;
        return false;
    }
    
    if (!m_memory->loadROM(romPath)) {
        std::cerr << "Failed to load ROM" << std::endl;
        return false;
    }
    
    // CPU 리셋 후 ROM 시작 주소로 점프
    m_cpu->reset();
    
    // ROM의 리셋 벡터 읽기 (0xFFFC-0xFFFD)
    uint16_t resetVector = m_memory->read16(0xFFFC);
    m_cpu->setPC(resetVector);
    
    std::cout << "ROM loaded successfully. Reset vector: 0x" << std::hex << resetVector << std::endl;
    
    return true;
}

void SNESEmulator::reset() {
    std::cout << "에뮬레이터 리셋" << std::endl;
    m_cpu->reset();
    m_ppu->reset();
    m_apu->reset();
    m_frameCount = 0;
}

void SNESEmulator::pause() {
    m_paused = true;
}

void SNESEmulator::resume() {
    m_paused = false;
}

void SNESEmulator::setDebugMode(bool enabled) {
    m_debugMode = enabled;
}
