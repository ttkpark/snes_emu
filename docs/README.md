# SNES 에뮬레이터 개발 문서

이 폴더는 SNES 에뮬레이터 개발 과정을 단계별로 문서화한 것입니다.

## 개발 단계

### 1단계: 프로젝트 설정 ✅
- [01_project_setup.md](01_project_setup.md) - 프로젝트 구조 설정 및 기본 디렉토리 생성
- [02_architecture_design.md](02_architecture_design.md) - SNES 에뮬레이터 아키텍처 설계 및 문서화

### 2단계: 핵심 컴포넌트 구현
- [03_cpu_emulation.md](03_cpu_emulation.md) - 65c816 CPU 코어 구현
- [04_memory_management.md](04_memory_management.md) - 메모리 매핑 및 관리 시스템 구현
- [05_ppu_emulation.md](05_ppu_emulation.md) - PPU (Picture Processing Unit) 에뮬레이션
- [06_apu_emulation.md](06_apu_emulation.md) - APU (Audio Processing Unit) 에뮬레이션

### 3단계: 시스템 통합
- [07_input_handling.md](07_input_handling.md) - 입력 처리 시스템 구현
- [08_rom_loading.md](08_rom_loading.md) - ROM 로딩 및 파싱 시스템
- [09_debug_system.md](09_debug_system.md) - 디버그 화면 및 개발자 도구 구현

### 4단계: 테스트 및 최적화
- [10_testing_integration.md](10_testing_integration.md) - Super Mario World 테스트 및 최적화

## 현재 상태

### 완료된 작업
- ✅ 프로젝트 구조 설정
- ✅ 기본 클래스 구조 생성
- ✅ CMake 빌드 시스템 설정
- ✅ 아키텍처 설계 및 문서화

### 진행 중인 작업
- 🔄 65c816 CPU 코어 구현 (기본 구조 완성)
- 🔄 메모리 관리 시스템 구현 (기본 구조 완성)
- 🔄 PPU 에뮬레이션 (기본 구조 완성)
- 🔄 APU 에뮬레이션 (기본 구조 완성)
- 🔄 입력 처리 시스템 (기본 구조 완성)
- 🔄 디버그 시스템 (기본 구조 완성)

### 다음 단계
1. CPU 명령어 완전 구현
2. 메모리 매핑 시스템 완성
3. PPU 렌더링 엔진 구현
4. APU 오디오 처리 구현
5. ROM 로딩 시스템 구현
6. 디버그 인터페이스 완성
7. Super Mario World 테스트

## 빌드 및 실행

### Windows
```bash
# 빌드
build.bat

# 실행
build\bin\snes_emu.exe
```

### Linux/macOS
```bash
# 빌드
mkdir build
cd build
cmake ..
make

# 실행
./snes_emu
```

## 개발 환경

- **언어**: C++17
- **GUI**: SDL2
- **빌드**: CMake
- **플랫폼**: Windows, Linux, macOS

## 목표

Super Mario World를 완벽하게 실행할 수 있는 SNES 에뮬레이터를 만드는 것입니다.

## 참고 자료

- [SNES 개발 문서](https://wiki.superfamicom.org/)
- [65c816 CPU 문서](https://en.wikipedia.org/wiki/WDC_65C816)
- [SDL2 문서](https://wiki.libsdl.org/)
