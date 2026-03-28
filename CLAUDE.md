# SNES Emulator - Agent Context

## Project Overview
SNES (Super Nintendo Entertainment System) emulator written in C++17 with SDL2.
**Goal**: Perfectly run Super Mario World.

## Build & Run

```bash
# Normal build (MSVC + SDL2)
./build_complete.bat

# Interactive debug build (adds simple_debugger, rom_loader, rom_mapper, video/audio output)
./build_interactive.bat

# Run with test ROM
./snes_emu_complete.exe spctest.sfc

# Run with game ROM
./snes_emu_complete.exe "Super Mario World (Europe) (Rev 1).sfc"
```

**Build system**: MSVC (`cl`) via `vcvars64.bat`, NOT CMake (CMakeLists.txt exists but is unused).
**Dependencies**: SDL2 (headers in `include/`, lib in `lib/`, `SDL2.dll` in root).

## Source Structure

```
src/
  main_complete.cpp    - Entry point, main emulation loop
  cpu/cpu.cpp|h        - 65c816 CPU (16-bit, SNES main processor)
  memory/memory.cpp|h  - Memory map (LoROM/HiROM/ExHiROM), DMA, HDMA
  ppu/ppu.cpp|h        - Picture Processing Unit (video, scanline rendering)
  apu/apu.cpp|h        - Audio Processing Unit (SPC700 CPU + DSP + BRR)
  input/
    simple_input.cpp|h - Keyboard/gamepad input (used in normal build)
    input.cpp|h        - Extended input handler
  debug/
    logger.cpp|h       - Logging system
    debugger.cpp|h     - Full debugger
    simple_debugger.cpp|h - Interactive step debugger
  emulation/
    core/snes_core.cpp|h  - High-level emulation core wrapper
    rom/rom_loader.cpp|h  - ROM file loading
    rom/rom_mapper.cpp|h  - ROM address mapping
  io/
    video/video_output.cpp|h - SDL2 video output
    audio/audio_output.cpp|h - SDL2 audio output
```

## Architecture

Components are connected in `main_complete.cpp`:
- `Memory` is the bus: holds references to CPU, PPU, APU, Input
- CPU reads/writes go through Memory, which routes to PPU registers ($2100-$213F), APU ports ($2140-$2143), etc.
- Main loop runs master clock ticks: PPU every 4, CPU every 6, APU every 24 master cycles

## Current State (as of last update)

### Working
- 65c816 CPU: basic instruction set, emulation mode, reset vector
- Memory: LoROM/HiROM mapping, ROM loading, I/O register routing
- PPU: SDL2 window, scanline counter, register writes, VRAM
- APU/SPC700: IPL ROM boot, CPU<->SPC port communication, instruction execution
- Input: keyboard mapping to SNES controller

### Active Work: SPC700 Instruction Verification
The SPC700 (APU processor) is being validated against `spctest.sfc` test ROM.
- **Block 1 tests (0x00-0xBE) all pass** - covers most SPC700 instructions
- **Block 2 fails at test 0x41** - IPL multi-block transfer timing issue
- spctest uses multi-block transfer: block 1 loads to 0x0300, runs tests, then SPC re-enables IPL ROM (JMP $FFC0) for block 2

### Key bugs fixed (dev branch):
1. **Opcode fetch PC not incrementing** - all instruction handlers were reading opcode byte as operand
2. **writePort() software state machine** - was overriding IPL ROM's natural execution, modifying PC and ARAM directly
3. **readARAM/writeARAM I/O range** - `addr >= 0xF0` matched ALL high addresses (0xFFF0 etc.), fixed to `0x00F0-0x00FF`
4. **Data mirroring in writePort** - IPL ROM's page transfer can't reach high ARAM; data is also mirrored directly

### Known issues
- **Opcode 0x64**: SPC700 spec says CMP A,dp (2 bytes), but spctest.sfc framework requires 3-byte CMP dp,#imm behavior. Currently 3-byte for spctest compat.
- **Block 2 test 0x41**: fails due to IPL multi-block transfer timing. The SPC re-enters IPL ROM for the second block, but something goes wrong with the data or execution context. The IPL ROM's `BPL` at $FFE9 limits single-block transfers to pages $00-$7F; data above $8000 requires multi-block coordination.
- **Data mirroring abandoned**: attempted to mirror CPU port writes directly to ARAM, but timing conflicts with IPL ROM's own writes caused data corruption. Pure IPL ROM approach used instead.

### How spctest.sfc works
- SPC700 boots via IPL ROM, receives test program from CPU via ports
- Each test writes its number to Port 2 (0x00, 0x01, 0x02, ...)
- **Success**: Port 0 = 0xFF, PC loops at 0x0350
- **Failure**: Port 2 = 0x02 (fail flag), PC loops at 0x0357

### Debugging test failures
```bash
# Build and run
./build_complete.bat && ./snes_emu_complete.exe spctest.sfc

# Check which test failed (look at last port 2 write before fail)
# Log files are generated in root: apu_trace.log, cpu_trace.log, port_comm.log

# Check last APU state
Get-Content apu_trace.log -Tail 50

# Find last test number
Get-Content apu_trace.log | Select-String "wrote port 2"
```

## Hardware Reference Docs
Located in `docs/hardware/` - these are the authoritative SNES hardware specs:
- `apu_spc700_instructions.md` - SPC700 opcode reference (critical for APU work)
- `apu_dsp_registers.md` - DSP register map
- `apu_ipl_rom.md` - IPL boot ROM protocol
- `cpu_65816_opcodes.md` - 65c816 opcode reference
- `mem_memory_map_complete.md` - Full SNES memory map
- `ppu_registers_bitmap.md` - PPU register bit layouts

## Key Design Decisions
- APU uses separate 64KB ARAM (`m_aram`), not shared with main memory
- CPU<->APU communication: CPU writes to `m_cpuPorts[0-3]`, SPC reads via `$F4-$F7`; SPC writes to ARAM `$F4-$F7`, CPU reads via `readPort()`
- IPL ROM is hardware ROM overlaying ARAM at `$FFC0-$FFFF`, toggled by `$F1` bit 7
- Build artifacts (.obj, .exe, .log) are gitignored

## Remaining Milestones
1. **SPC700 full test pass** - all spctest.sfc tests must pass
2. **PPU rendering** - backgrounds, sprites, Mode 0-7
3. **DMA/HDMA** - bulk data transfer
4. **System integration** - CPU/PPU/APU timing sync, NMI/IRQ
5. **Super Mario World boot** - title screen and gameplay
