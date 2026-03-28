# Automated Testing - 자동화 테스트 시스템

## 📋 목차
1. [개요](#개요)
2. [테스트 ROM 실행](#테스트-rom-실행)
3. [회귀 테스트](#회귀-테스트)
4. [CI/CD 통합](#cicd-통합)

---

## 개요

자동화 테스트 시스템은 에뮬레이터의 정확성을 검증하고 회귀(regression)를 방지합니다.

---

## 테스트 ROM 실행

### 테스트 러너

```cpp
class TestRunner {
private:
    struct TestResult {
        std::string name;
        bool passed;
        std::string message;
        double executionTime;
    };
    
    std::vector<TestResult> results;
    
public:
    bool runTest(const char* romPath, int maxFrames = 600) {
        Emulator emu;
        emu.loadROM(romPath);
        
        auto startTime = getTime();
        
        for (int frame = 0; frame < maxFrames; frame++) {
            emu.runFrame();
            
            // 테스트 완료 체크
            if (checkTestComplete(emu)) {
                bool passed = checkTestPassed(emu);
                
                TestResult result;
                result.name = romPath;
                result.passed = passed;
                result.message = getTestMessage(emu);
                result.executionTime = getTime() - startTime;
                
                results.push_back(result);
                return passed;
            }
        }
        
        // 타임아웃
        TestResult result;
        result.name = romPath;
        result.passed = false;
        result.message = "Timeout";
        result.executionTime = getTime() - startTime;
        
        results.push_back(result);
        return false;
    }
    
    bool checkTestComplete(Emulator& emu) {
        // APU Port 0을 체크 (많은 테스트 ROM이 사용)
        uint8_t port0 = emu.readAPUPort(0);
        return (port0 == 0x00);  // 0x00 = 테스트 완료
    }
    
    bool checkTestPassed(Emulator& emu) {
        uint8_t port2 = emu.readAPUPort(2);
        return (port2 == 0x00);  // 0x00 = 성공, 그 외 = 실패
    }
    
    void printReport() {
        int passed = 0, failed = 0;
        
        printf("=== Test Results ===\n");
        for (auto& r : results) {
            printf("[%s] %s (%.2f s) - %s\n",
                   r.passed ? "PASS" : "FAIL",
                   r.name.c_str(),
                   r.executionTime,
                   r.message.c_str());
            
            if (r.passed) passed++;
            else failed++;
        }
        
        printf("\nSummary: %d passed, %d failed (%.1f%%)\n",
               passed, failed,
               passed * 100.0 / (passed + failed));
    }
};
```

---

## 회귀 테스트

### 스크린샷 비교

```cpp
class RegressionTester {
private:
    struct Snapshot {
        std::string name;
        std::vector<uint8_t> pixels;
        int width, height;
    };
    
public:
    Snapshot captureScreen(Emulator& emu) {
        Snapshot snap;
        snap.width = 256;
        snap.height = 224;
        snap.pixels = emu.getFramebuffer();
        return snap;
    }
    
    bool compareSnapshots(const Snapshot& a, const Snapshot& b, double threshold = 0.01) {
        if (a.width != b.width || a.height != b.height) {
            return false;
        }
        
        int diffPixels = 0;
        for (size_t i = 0; i < a.pixels.size(); i++) {
            if (a.pixels[i] != b.pixels[i]) {
                diffPixels++;
            }
        }
        
        double diffRatio = (double)diffPixels / a.pixels.size();
        return (diffRatio < threshold);
    }
    
    void saveSnapshot(const Snapshot& snap, const char* filename) {
        // PNG로 저장
        stbi_write_png(filename, snap.width, snap.height, 4,
                      snap.pixels.data(), snap.width * 4);
    }
    
    bool runRegressionTest(const char* romPath, int frame, const char* refImage) {
        Emulator emu;
        emu.loadROM(romPath);
        
        // 특정 프레임까지 실행
        for (int i = 0; i < frame; i++) {
            emu.runFrame();
        }
        
        // 스크린샷 캡처
        Snapshot current = captureScreen(emu);
        
        // 참조 이미지 로드
        int width, height, channels;
        uint8_t* refData = stbi_load(refImage, &width, &height, &channels, 4);
        
        Snapshot reference;
        reference.width = width;
        reference.height = height;
        reference.pixels.assign(refData, refData + width * height * 4);
        stbi_image_free(refData);
        
        // 비교
        bool passed = compareSnapshots(current, reference);
        
        if (!passed) {
            // 차이 이미지 저장
            saveSnapshot(current, "regression_diff.png");
        }
        
        return passed;
    }
};
```

---

## CI/CD 통합

### GitHub Actions 워크플로우

```yaml
# .github/workflows/test.yml
name: Automated Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest
    
    steps:
    - uses: actions/checkout@v2
    
    - name: Install Dependencies
      run: |
        sudo apt-get update
        sudo apt-get install -y build-essential cmake libsdl2-dev
    
    - name: Build
      run: |
        mkdir build
        cd build
        cmake ..
        make -j4
    
    - name: Download Test ROMs
      run: |
        wget https://example.com/test_roms.zip
        unzip test_roms.zip -d test_roms/
    
    - name: Run Tests
      run: |
        cd build
        ./snes_emu_test --rom ../test_roms/cputest.sfc
        ./snes_emu_test --rom ../test_roms/spctest.sfc
    
    - name: Upload Test Results
      uses: actions/upload-artifact@v2
      with:
        name: test-results
        path: build/test_results/
```

### CMake 테스트 설정

```cmake
# CMakeLists.txt
enable_testing()

# 테스트 ROM 추가
add_test(
    NAME cputest
    COMMAND snes_emu_test --rom ${CMAKE_SOURCE_DIR}/test_roms/cputest.sfc
)

add_test(
    NAME spctest
    COMMAND snes_emu_test --rom ${CMAKE_SOURCE_DIR}/test_roms/spctest.sfc
)

# 회귀 테스트
add_test(
    NAME regression_mario
    COMMAND snes_emu_test --regression ${CMAKE_SOURCE_DIR}/test_roms/mario.sfc --frame 60 --ref ${CMAKE_SOURCE_DIR}/reference/mario_f60.png
)
```

### 테스트 메인 함수

```cpp
// test_main.cpp
int main(int argc, char* argv[]) {
    TestRunner runner;
    
    // 명령줄 인자 파싱
    std::string romPath;
    bool regressionMode = false;
    std::string refImage;
    int frame = 0;
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--rom") == 0 && i + 1 < argc) {
            romPath = argv[++i];
        } else if (strcmp(argv[i], "--regression") == 0) {
            regressionMode = true;
            romPath = argv[++i];
        } else if (strcmp(argv[i], "--frame") == 0 && i + 1 < argc) {
            frame = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--ref") == 0 && i + 1 < argc) {
            refImage = argv[++i];
        }
    }
    
    if (romPath.empty()) {
        printf("Usage: %s --rom <path> [--regression --frame N --ref <image>]\n", argv[0]);
        return 1;
    }
    
    bool passed;
    if (regressionMode) {
        RegressionTester tester;
        passed = tester.runRegressionTest(romPath.c_str(), frame, refImage.c_str());
    } else {
        passed = runner.runTest(romPath.c_str());
    }
    
    runner.printReport();
    
    return passed ? 0 : 1;
}
```

---

## 테스트 ROM 목록

### CPU 테스트
- `cputest-basic.sfc` - 기본 명령어
- `cputest-full.sfc` - 전체 명령어 세트
- `decimal.sfc` - Decimal Mode

### PPU 테스트
- `pputest-sprites.sfc` - 스프라이트
- `pputest-backgrounds.sfc` - 배경
- `pputest-mode7.sfc` - Mode 7

### APU 테스트
- `spctest.sfc` - SPC700 명령어
- `dsptest.sfc` - DSP 기능

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete










