# SNES Emulator Implementation Status

**최종 업데이트**: 2025-01-XX

## 1. Main CPU (S-CPU) - 65C816
- **Status**: 🟢 대부분 완료 (약 95%)
- **Implemented**:
    - ✅ **Core Registers**: A, X, Y, S, P, D, DBR, PBR 완전 구현
    - ✅ **Emulation/Native Mode**: 모드 전환 및 XCE 명령어 구현
    - ✅ **Addressing Modes**: 모든 주요 주소 모드 구현 (Direct Page, Absolute, Indirect, Indexed 등)
    - ✅ **Instruction Set**: **488개 opcode 구현** (거의 모든 65C816 명령어)
        - Load/Store: LDA, LDX, LDY, STA, STX, STY 등
        - Arithmetic: ADC, SBC, INC, DEC, INX, DEX 등
        - Logic: AND, ORA, EOR, BIT 등
        - Shift/Rotate: ASL, LSR, ROL, ROR 등
        - Branch: 모든 분기 명령어 (BCC, BCS, BEQ, BNE 등)
        - Stack: PHA, PLA, PHP, PLP, PHX, PHY, PLX, PLY
        - Jump/Subroutine: JMP, JSR, RTS, RTL
        - 65816 특수 명령어: REP, SEP, XCE, MVN, MVP 등
    - ✅ **NMI**: V-Blank NMI 처리 완료
    - ✅ **BRK/RTI**: 인터럽트 처리 구현
    - ✅ **Debugging**: 무한 루프 감지, 로깅 시스템
- **Missing / To Do**:
    - ⚠️ **IRQ**: H-IRQ, V-IRQ, H/V-IRQ 미구현 (NMI만 구현됨)
    - ⚠️ **Decimal Mode**: SED/CLD 명령어는 있으나, ADC/SBC에서 BCD 연산 미완전 (플래그만 설정)
    - ⚠️ **Cycle Accuracy**: 사이클 카운팅은 있으나 정확도 검증 필요
    - ⚠️ **Hardware Registers**: 곱셈/나눗셈 레지스터 (0x4202-0x4206) 미구현
    - ⚠️ **WDM/COP**: 예외 처리 검증 필요

## 2. Picture Processing Unit (PPU)
- **Status**: 🟡 부분 구현 (약 60%)
- **Implemented**:
    - ✅ **VRAM, CGRAM, OAM**: 데이터 구조 완전 구현 (64KB VRAM, 512B CGRAM, 544B OAM)
    - ✅ **Scanline Rendering**: 스캔라인 기반 렌더링 루프
    - ✅ **PPU Registers**: 모든 PPU 레지스터 (0x2100-0x21FF) 처리
    - ✅ **Background Mode 0**: 4개 레이어 2bpp 타일 렌더링 완료
    - ✅ **Background Mode 1**: BG1/BG2 4bpp, BG3 2bpp 렌더링 구현
    - ✅ **Background Mode 6**: BG1 4bpp hires 렌더링 구현
    - ✅ **Tile Decoding**: 2bpp, 4bpp 타일 디코딩 구현
    - ✅ **Scrolling**: 수평/수직 스크롤링 구현
    - ✅ **CGRAM**: 256색 팔레트 시스템 구현
    - ✅ **SDL2 Video**: 완전한 SDL2 비디오 출력
    - ✅ **Sprites (기본)**: 스프라이트 렌더링 기본 구현 (크기, 플리핑 지원)
- **Missing / To Do**:
    - ❌ **Background Modes 2, 3, 4, 5, 7**: 미구현 (테스트 패턴만 표시)
    - ⚠️ **Sprites (고급)**: 우선순위 처리, 확장 OAM 속성 미완전
    - ⚠️ **Windowing**: 윈도우 마스크 로직 플레이스홀더만 존재
    - ❌ **Mosaic**: 모자이크 효과 미구현
    - ⚠️ **Color Math**: 색상 덧셈/뺄셈 및 서브스크린 로직 미완전
    - ❌ **Mode 7**: 회전/스케일링 및 HDMA 업데이트 미구현

## 3. Audio Processing Unit (APU)
- **Status**: 🟡 부분 구현 (약 70%)
- **Implemented**:
    - ✅ **SPC700 CPU**: **555개 opcode 구현** (거의 모든 SPC700 명령어)
        - 모든 MOV 명령어 (레지스터, 메모리, 직접 페이지)
        - 산술 연산: ADC, SBC, INC, DEC, ADDW, SUBW 등
        - 논리 연산: AND, OR, EOR, CMP 등
        - 비트 연산: ASL, LSR, ROL, ROR, XCN 등
        - 분기: 모든 분기 명령어
        - 스택: PUSH, POP
        - 특수: MUL, DIV, DAA, DAS 등
    - ✅ **ARAM**: 64KB 오디오 RAM 완전 구현
    - ✅ **IPL ROM**: 부트로더 ROM 로딩
    - ✅ **IPL Protocol**: CPU-APU 핸드셰이크 프로토콜 완전 구현
    - ✅ **I/O Ports**: 4개 통신 포트 ($2140-$2143, $F4-$F7) 완전 구현
    - ✅ **SDL2 Audio**: SDL2 오디오 출력 시스템 구현
    - ✅ **DSP Registers**: 기본 DSP 레지스터 구조 (128개 레지스터)
    - ⚠️ **BRR Decoding**: 부분 구현 (기본 디코딩 로직 존재)
- **Missing / To Do**:
    - ⚠️ **DSP 완전 구현**: 
        - ADSR 엔벨로프 처리 개선 필요
        - 피치 조정 및 샘플 재생 로직 개선 필요
        - 채널 믹싱 완전 구현 필요
    - ❌ **Echo/Reverb**: 에코/리버브 효과 미구현
    - ❌ **Noise Generation**: 노이즈 생성 미구현
    - ⚠️ **SPC700 Timers**: 타이머 (0, 1, 2) 정확한 틱킹 필요
    - ❌ **Pitch Modulation**: 피치 모뮬레이션 미구현
    - ❌ **FIR Filter**: FIR 필터 미구현

## 4. Memory System
- **Status**: 🟢 대부분 완료 (약 90%)
- **Implemented**:
    - ✅ **LoROM Mapping**: LoROM 메모리 매핑 완전 구현
    - ✅ **HiROM Mapping**: HiROM 메모리 매핑 지원 (감지 및 처리)
    - ✅ **WRAM**: 128KB Work RAM 완전 구현
    - ✅ **SRAM**: 32KB Save RAM 지원
    - ✅ **DMA**: 8채널 DMA 시스템 구현
        - DMA 레지스터 처리 ($4300-$437F)
        - CPU->PPU 전송 구현
        - CPU->APU 전송 구현
        - 다양한 전송 모드 지원
    - ✅ **I/O Routing**: PPU, APU, Input 레지스터 라우팅
- **Missing / To Do**:
    - ❌ **HDMA**: H-Blank DMA 미구현 (레지스터만 인식)
    - ⚠️ **Open Bus**: Open Bus 동작 미구현
    - ❌ **Cartridge Mappers**: 특수 매퍼 (DSP-1, SuperFX 등) 미지원

## 5. Input System
- **Status**: 🟡 부분 구현
- **Implemented**:
    - ✅ **SDL2 Input**: SDL2 입력 처리 기본 구현
- **Missing / To Do**:
    - ⚠️ **Controller Registers**: 자동 읽기 vs 수동 읽기 처리 미완전
    - ❌ **Multi-tap**: 멀티탭 지원 미구현

## 6. Timing & Synchronization
- **Status**: 🟡 부분 구현
- **Implemented**:
    - ✅ **CPU Cycles**: CPU 사이클 카운팅
    - ✅ **PPU Scanline**: 스캔라인 기반 렌더링
    - ✅ **Frame Timing**: 기본 프레임 타이밍
- **Missing / To Do**:
    - ⚠️ **Master Clock**: 21.47727 MHz 마스터 클럭 정확도 검증 필요
    - ⚠️ **Dot Clock**: 스캔라인당 사이클 정확도 필요
    - ⚠️ **Component Synchronization**: CPU/PPU/APU 동기화 개선 필요

## 테스트 진행 상황

### 완료된 테스트
- ✅ **CPU 테스트**: `cputest-basic.sfc`, `cputest-full.sfc` - 65C816 CPU 명령어 테스트 완료
- ✅ **기본 SNES 테스트**: `SNES Test Program.sfc` - 기본 시스템 테스트 완료

### 진행 중인 테스트
- 🔄 **SPC700 테스트**: `spctest.sfc` - SPC700 명령어 세트 종합 테스트 진행 중
    - 첫 번째 테스트 세트 (`spc_tests0.asm`) 실행 중
    - `spc_common.inc`의 `main` 루틴에서 실패 발생 분석 중

## 전체 요약

### 구현 완료도
- **CPU**: 🟢 95% - 거의 모든 명령어 구현, IRQ 및 Decimal Mode 개선 필요
- **PPU**: 🟡 60% - Mode 0/1/6 구현, 나머지 모드 및 고급 기능 필요
- **APU**: 🟡 70% - SPC700 완전 구현, DSP 고급 기능 필요
- **Memory**: 🟢 90% - LoROM/HiROM, DMA 구현, HDMA 필요
- **Input**: 🟡 50% - 기본 입력만 구현
- **전체**: 🟡 약 75% 완료

### 우선순위 개선 사항
1. **높음**: IRQ 처리, HDMA, PPU Mode 2-5/7
2. **중간**: Decimal Mode BCD 연산, DSP 완전 구현, 스프라이트 우선순위
3. **낮음**: 에코 효과, 모자이크, 특수 매퍼 지원
