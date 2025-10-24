# 10. Super Mario World 테스트 및 최적화

## 목표
- Super Mario World 완전 실행
- 성능 최적화
- 버그 수정 및 안정성 향상
- 최종 테스트 및 검증

## 테스트 계획

### 1. 기본 기능 테스트
- **ROM 로딩**: Super Mario World ROM 로딩 테스트
- **CPU 실행**: 게임 시작 및 기본 동작 테스트
- **PPU 렌더링**: 화면 출력 및 그래픽 테스트
- **APU 오디오**: 사운드 및 음악 테스트
- **입력 처리**: 컨트롤러 입력 테스트

### 2. 게임플레이 테스트
- **시작 화면**: 타이틀 화면 표시 테스트
- **메뉴 네비게이션**: 메뉴 이동 및 선택 테스트
- **레벨 로딩**: 레벨 시작 및 로딩 테스트
- **캐릭터 이동**: 마리오 이동 및 점프 테스트
- **적과의 상호작용**: 적과의 충돌 및 상호작용 테스트

### 3. 성능 테스트
- **프레임레이트**: 60 FPS 유지 테스트
- **CPU 사용률**: CPU 사용률 모니터링
- **메모리 사용량**: 메모리 사용량 모니터링
- **오디오 지연**: 오디오 지연 시간 측정

## 최적화 전략

### 1. CPU 최적화
```cpp
// JIT 컴파일러 구현
class JITCompiler {
    std::map<uint16_t, void*> m_compiledCode;
    
    void* compileInstruction(uint16_t address) {
        // 자주 실행되는 코드를 네이티브 코드로 변환
        return generateNativeCode(address);
    }
    
    void executeCompiledCode(uint16_t address) {
        if (m_compiledCode.find(address) != m_compiledCode.end()) {
            // 컴파일된 코드 실행
            executeNativeCode(m_compiledCode[address]);
        } else {
            // 인터프리터 모드로 실행
            executeInterpreted(address);
        }
    }
};
```

### 2. 메모리 최적화
```cpp
// 메모리 풀링 시스템
class MemoryPool {
    std::vector<std::unique_ptr<uint8_t[]>> m_pools;
    size_t m_poolSize;
    size_t m_currentPool;
    size_t m_currentOffset;
    
public:
    void* allocate(size_t size) {
        if (m_currentOffset + size > m_poolSize) {
            allocateNewPool();
        }
        
        void* ptr = m_pools[m_currentPool].get() + m_currentOffset;
        m_currentOffset += size;
        return ptr;
    }
    
    void reset() {
        m_currentPool = 0;
        m_currentOffset = 0;
    }
};
```

### 3. 렌더링 최적화
```cpp
// 더티 렉트 시스템
class DirtyRectSystem {
    std::vector<SDL_Rect> m_dirtyRects;
    bool m_fullScreenDirty;
    
public:
    void markDirty(int x, int y, int w, int h) {
        SDL_Rect rect = {x, y, w, h};
        m_dirtyRects.push_back(rect);
    }
    
    void markFullScreenDirty() {
        m_fullScreenDirty = true;
        m_dirtyRects.clear();
    }
    
    void renderDirtyRects(SDL_Renderer* renderer) {
        if (m_fullScreenDirty) {
            // 전체 화면 렌더링
            renderFullScreen(renderer);
        } else {
            // 더티 영역만 렌더링
            for (const auto& rect : m_dirtyRects) {
                renderRect(renderer, rect);
            }
        }
        
        clearDirtyRects();
    }
};
```

## 버그 수정

### 1. CPU 버그 수정
```cpp
// 명령어 실행 정확성 검증
void validateInstructionExecution() {
    // 각 명령어별 정확한 결과 검증
    // 플래그 설정 검증
    // 타이밍 검증
}

// 메모리 접근 버그 수정
void fixMemoryAccessBugs() {
    // 주소 변환 오류 수정
    // 메모리 권한 검증
    // 페이지 크로싱 처리
}
```

### 2. PPU 버그 수정
```cpp
// 렌더링 버그 수정
void fixRenderingBugs() {
    // 스프라이트 우선순위 수정
    // 배경 렌더링 수정
    // 색상 팔레트 수정
}

// 타이밍 버그 수정
void fixTimingBugs() {
    // 스캔라인 타이밍 수정
    // 수직 블랭킹 수정
    // 인터럽트 타이밍 수정
}
```

### 3. APU 버그 수정
```cpp
// 오디오 버그 수정
void fixAudioBugs() {
    // 채널 간섭 수정
    // ADSR 엔벨로프 수정
    // 에코 효과 수정
}

// 샘플링 버그 수정
void fixSamplingBugs() {
    // 샘플링 레이트 수정
    // 비트 깊이 수정
    // 채널 믹싱 수정
}
```

## 성능 모니터링

### 1. 성능 메트릭
```cpp
class PerformanceMonitor {
    struct Metrics {
        double frameTime;
        double cpuTime;
        double ppuTime;
        double apuTime;
        double memoryTime;
        size_t memoryUsage;
        int frameRate;
    };
    
    Metrics m_currentMetrics;
    std::vector<Metrics> m_history;
    
public:
    void updateMetrics() {
        auto start = std::chrono::high_resolution_clock::now();
        
        // CPU 시간 측정
        auto cpuStart = std::chrono::high_resolution_clock::now();
        m_cpu->update();
        auto cpuEnd = std::chrono::high_resolution_clock::now();
        m_currentMetrics.cpuTime = std::chrono::duration<double>(cpuEnd - cpuStart).count();
        
        // PPU 시간 측정
        auto ppuStart = std::chrono::high_resolution_clock::now();
        m_ppu->update();
        auto ppuEnd = std::chrono::high_resolution_clock::now();
        m_currentMetrics.ppuTime = std::chrono::duration<double>(ppuEnd - ppuStart).count();
        
        // APU 시간 측정
        auto apuStart = std::chrono::high_resolution_clock::now();
        m_apu->update();
        auto apuEnd = std::chrono::high_resolution_clock::now();
        m_currentMetrics.apuTime = std::chrono::duration<double>(apuEnd - apuStart).count();
        
        auto end = std::chrono::high_resolution_clock::now();
        m_currentMetrics.frameTime = std::chrono::duration<double>(end - start).count();
        m_currentMetrics.frameRate = 1.0 / m_currentMetrics.frameTime;
        
        m_history.push_back(m_currentMetrics);
        if (m_history.size() > 1000) {
            m_history.erase(m_history.begin());
        }
    }
};
```

### 2. 성능 리포트
```cpp
void generatePerformanceReport() {
    std::cout << "=== 성능 리포트 ===" << std::endl;
    std::cout << "평균 프레임 시간: " << getAverageFrameTime() << "ms" << std::endl;
    std::cout << "평균 프레임레이트: " << getAverageFrameRate() << " FPS" << std::endl;
    std::cout << "평균 CPU 사용률: " << getAverageCPUUsage() << "%" << std::endl;
    std::cout << "평균 메모리 사용량: " << getAverageMemoryUsage() << " MB" << std::endl;
    std::cout << "최대 프레임 시간: " << getMaxFrameTime() << "ms" << std::endl;
    std::cout << "최소 프레임레이트: " << getMinFrameRate() << " FPS" << std::endl;
}
```

## 최종 테스트

### 1. 호환성 테스트
- **다양한 ROM**: 다른 SNES 게임 테스트
- **플랫폼**: Windows, Linux, macOS 테스트
- **하드웨어**: 다양한 하드웨어 구성 테스트

### 2. 안정성 테스트
- **장시간 실행**: 24시간 연속 실행 테스트
- **메모리 누수**: 메모리 누수 검사
- **크래시 테스트**: 예외 상황 처리 테스트

### 3. 사용자 경험 테스트
- **사용자 인터페이스**: UI/UX 테스트
- **설정**: 다양한 설정 조합 테스트
- **성능**: 다양한 하드웨어에서의 성능 테스트

## 배포 준비

### 1. 빌드 최적화
```cpp
// 릴리즈 빌드 설정
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -flto")  // 링크 타임 최적화
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")  // 네이티브 최적화
```

### 2. 패키징
```cpp
// Windows용 인스톨러
// Linux용 AppImage
// macOS용 DMG
```

### 3. 문서화
```cpp
// 사용자 매뉴얼
// 개발자 문서
// API 문서
```

## 예상 소요 시간
3-4일

## 최종 목표
Super Mario World를 완벽하게 실행할 수 있는 SNES 에뮬레이터 완성!
