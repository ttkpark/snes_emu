# 01. 프로젝트 설정 및 기본 구조

## 목표
- SNES 에뮬레이터 프로젝트의 기본 디렉토리 구조 생성
- CMake 빌드 시스템 설정
- SDL2 의존성 설정
- 기본 소스 파일 구조 생성

## 작업 내용

### 1. 디렉토리 구조 생성
```
snes_emu/
├── src/
│   ├── core/
│   ├── cpu/
│   ├── ppu/
│   ├── apu/
│   ├── memory/
│   ├── input/
│   └── debug/
├── include/
├── docs/
├── tests/
├── roms/
└── build/
```

### 2. CMakeLists.txt 설정
- C++17 표준 설정
- SDL2 링킹
- 컴파일러 플래그 설정
- 디버그/릴리즈 빌드 설정

### 3. 기본 소스 파일 생성
- main.cpp (진입점)
- 기본 클래스 헤더 파일들
- 네임스페이스 구조

### 4. 의존성 관리
- SDL2 설치 가이드
- Windows/Linux/macOS 지원

## 예상 소요 시간
30분

## 다음 단계
02_architecture_design.md - 전체 아키텍처 설계
