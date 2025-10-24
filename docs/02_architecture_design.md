# 02. SNES 에뮬레이터 아키텍처 설계

## 목표
- SNES 하드웨어 아키텍처 분석 및 설계
- 전체 시스템 구조 정의
- 컴포넌트 간 상호작용 설계
- 성능 최적화 전략 수립

## SNES 하드웨어 아키텍처

### 1. CPU (65c816)
- **클록 속도**: 21.477 MHz (NTSC) / 21.281 MHz (PAL)
- **레지스터**: A, X, Y, SP, PC, P (상태 플래그)
- **주소 공간**: 24비트 (16MB)
- **에뮬레이션 모드**: 8비트/16비트 레지스터 크기

### 2. PPU (Picture Processing Unit)
- **해상도**: 256x224 (NTSC) / 256x239 (PAL)
- **색상**: 15비트 RGB (32,768색)
- **배경**: 4개 레이어 (BG0-BG3)
- **스프라이트**: 128개 (최대 32개/스캔라인)
- **모드**: 7가지 배경 모드

### 3. APU (Audio Processing Unit)
- **채널**: 8개 DSP 채널
- **샘플링**: 32kHz
- **효과**: 에코, 피치 모듈레이션, ADSR 엔벨로프

### 4. 메모리 맵
```
0x000000-0x00FFFF: System RAM (128KB)
0x010000-0x01FFFF: Extended RAM
0x020000-0x03FFFF: Reserved
0x040000-0x04FFFF: I/O Registers
0x050000-0x05FFFF: Expansion
0x060000-0x06FFFF: Expansion
0x070000-0x07FFFF: Expansion
0x080000-0x0FFFFF: ROM (512KB)
0x100000-0x1FFFFF: ROM (1MB)
0x200000-0x2FFFFF: ROM (2MB)
0x300000-0x3FFFFF: ROM (4MB)
0x400000-0x4FFFFF: ROM (8MB)
0x500000-0x5FFFFF: ROM (16MB)
0x600000-0x6FFFFF: ROM (32MB)
0x700000-0x7FFFFF: ROM (64MB)
0x800000-0xFFFFFF: ROM (128MB)
```

## 시스템 아키텍처

### 1. 메인 에뮬레이터 클래스
```cpp
class SNESEmulator {
    std::unique_ptr<CPU> m_cpu;
    std::unique_ptr<PPU> m_ppu;
    std::unique_ptr<APU> m_apu;
    std::unique_ptr<Memory> m_memory;
    std::unique_ptr<Input> m_input;
    std::unique_ptr<Debugger> m_debugger;
};
```

### 2. 컴포넌트 간 상호작용
- **CPU ↔ Memory**: 메모리 읽기/쓰기
- **CPU ↔ PPU**: 레지스터 접근, 인터럽트
- **CPU ↔ APU**: 레지스터 접근, 인터럽트
- **PPU ↔ Memory**: VRAM, CGRAM, OAM 접근
- **APU ↔ Memory**: DSP 레지스터, 샘플 데이터

### 3. 타이밍 동기화
- **CPU 클록**: 21.477 MHz
- **PPU 클록**: 5.37 MHz (CPU의 1/4)
- **APU 클록**: 1.79 MHz (CPU의 1/12)
- **프레임**: 60 FPS (NTSC) / 50 FPS (PAL)

## 성능 최적화 전략

### 1. JIT 컴파일
- 자주 실행되는 코드 블록을 네이티브 코드로 변환
- CPU 명령어 캐싱

### 2. 메모리 최적화
- 메모리 풀링
- 캐시 친화적 데이터 구조
- 가상 메모리 매핑

### 3. 렌더링 최적화
- 더티 렉트 업데이트
- 하드웨어 가속 활용
- 프레임 스킵

## 디버그 시스템

### 1. 실시간 디버깅
- CPU 상태 모니터링
- 메모리 덤프
- 브레이크포인트
- 디스어셈블리

### 2. 성능 프로파일링
- 클록 사이클 카운팅
- 메모리 접근 통계
- 렌더링 성능

## 다음 단계
03_cpu_emulation.md - 65c816 CPU 코어 구현
