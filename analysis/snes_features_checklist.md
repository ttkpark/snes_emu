# SNES Emulator Feature Checklist

**최종 업데이트**: 2025-01-XX

## 1. Main CPU (S-CPU)
- [x] **Core**: Ricoh 5A22 (based on 65c816) ✅
- [x] **Registers**: Accumulator (A), X, Y, Stack Pointer (S), Direct Page (D), Program Bank (K), Data Bank (B), Status (P) ✅
- [x] **Emulation Mode**: 6502 emulation mode vs Native mode ✅
- [x] **Addressing Modes**: All 24 65c816 addressing modes ✅
- [x] **Instruction Set**: All 256 opcodes + 16-bit extensions ✅ (488개 opcode 구현)
- [~] **ALU**: Decimal mode ⚠️ (플래그만 설정, BCD 연산 미완전), 16-bit arithmetic ✅
- [~] **DMA/HDMA**:
    - [x] General Purpose DMA (Channels 0-7) ✅
    - [ ] H-Blank DMA (HDMA) ❌
- [~] **Interrupts**:
    - [x] NMI (V-Blank) ✅
    - [ ] IRQ (H-IRQ, V-IRQ, H/V-IRQ) ❌
    - [x] Reset Vector ✅
- [ ] **Timers/Multiplication**:
    - [ ] Hardware Multiplication/Division registers (0x4202, 0x4203, etc.) ❌
    - [~] Programmable I/O ports ⚠️ (부분 구현)

## 2. Picture Processing Unit (PPU)
- [~] **Backgrounds (BG)**:
    - [x] BG Mode 0 ✅
    - [x] BG Mode 1 ✅
    - [ ] BG Mode 2 ❌
    - [ ] BG Mode 3 ❌
    - [ ] BG Mode 4 ❌
    - [ ] BG Mode 5 ❌
    - [x] BG Mode 6 ✅
    - [ ] BG Mode 7 ❌
    - [x] Tilemaps and Character data ✅
    - [x] Scrolling (H/V) ✅
- [~] **Sprites (OBJ)**:
    - [x] Sizes (8x8, 16x16, etc.) ✅
    - [~] Priority ⚠️ (기본 구현, 확장 OAM 미완전)
    - [x] Flipping (H/V) ✅
- [~] **VRAM Management**:
    - [~] VRAM access during V-Blank/Forced Blank ⚠️ (기본 구현)
    - [x] Address translation ✅
- [~] **Color/Palettes**:
    - [x] CGRAM (256 colors) ✅
    - [~] Color Math (Add/Sub, Halving) ⚠️ (부분 구현)
- [~] **Windowing**:
    - [~] Window masks for BG and OBJ ⚠️ (플레이스홀더만 존재)
- [ ] **Mosaic Effect** ❌
- [ ] **Mode 7**:
    - [ ] Rotation/Scaling ❌
    - [ ] HDMA updates for perspective ❌

## 3. Audio Processing Unit (APU)
- [x] **S-SMP (Sony SPC700)**:
    - [x] Instruction set (similar to 6502 but different) ✅ (555개 opcode 구현)
    - [~] Timers (3 timers) ⚠️ (기본 구현, 정확도 개선 필요)
    - [x] IPL ROM (Bootloader) ✅
- [~] **S-DSP**:
    - [~] 8 Voices ⚠️ (기본 구조만)
    - [~] BRR Sample Decoding ⚠️ (부분 구현)
    - [ ] Pitch modulation ❌
    - [~] ADSR Envelope control ⚠️ (기본 구현, 개선 필요)
    - [ ] Echo buffer ❌
    - [ ] FIR Filter ❌
- [x] **Communication**:
    - [x] 4 Communication Ports (0x2140-0x2143) between S-CPU and S-SMP ✅

## 4. Memory Map
- [x] **LoROM Mapping** ✅
- [x] **HiROM Mapping** ✅
- [x] **WRAM** (128KB) ✅
- [x] **SRAM** (Save RAM) ✅
- [ ] **Open Bus Behavior** ❌

## 5. Input/Output
- [~] **Joypads**: Standard controller reading (Auto-read and manual) ⚠️ (기본 구현)
- [ ] **Expansion Port** (optional for basic emulator) ❌

## 6. Timing
- [~] **Master Clock**: 21.47727 MHz ⚠️ (기본 구현, 정확도 검증 필요)
- [~] **Dot Clock**: Cycles per scanline ⚠️ (기본 구현)
- [~] **Frame Timing**: NTSC (60Hz) / PAL (50Hz) ⚠️ (기본 구현)

## 범례
- ✅ 완료
- ⚠️ 부분 구현
- ❌ 미구현
