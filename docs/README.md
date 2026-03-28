# SNES Emulator Development Documentation

This folder contains step-by-step documentation of the SNES emulator development process.

## Development Phases

### Phase 1: Project Setup ✅
- [01_project_setup.md](01_project_setup.md) - Project structure setup and basic directory creation
- [02_architecture_design.md](02_architecture_design.md) - SNES emulator architecture design and documentation

### Phase 2: Core Component Implementation
- [03_cpu_emulation.md](03_cpu_emulation.md) - 65c816 CPU core implementation
- [04_memory_management.md](04_memory_management.md) - Memory mapping and management system implementation
- [05_ppu_emulation.md](05_ppu_emulation.md) - PPU (Picture Processing Unit) emulation
- [06_apu_emulation.md](06_apu_emulation.md) - APU (Audio Processing Unit) emulation

### Phase 3: System Integration
- [07_input_handling.md](07_input_handling.md) - Input handling system implementation
- [08_rom_loading.md](08_rom_loading.md) - ROM loading and parsing system
- [09_debug_system.md](09_debug_system.md) - Debug screen and developer tools implementation

### Phase 4: Testing and Optimization
- [10_testing_integration.md](10_testing_integration.md) - Super Mario World testing and optimization

## Current Status

### Completed Tasks
- ✅ Project structure setup
- ✅ Basic class structure creation
- ✅ CMake build system setup
- ✅ Architecture design and documentation

### In Progress
- 🔄 65c816 CPU core implementation (basic structure completed)
- 🔄 Memory management system implementation (basic structure completed)
- 🔄 PPU emulation (basic structure completed)
- 🔄 APU emulation (basic structure completed)
- 🔄 Input handling system (basic structure completed)
- 🔄 Debug system (basic structure completed)

### Next Steps
1. Complete CPU instruction implementation
2. Complete memory mapping system
3. Implement PPU rendering engine
4. Implement APU audio processing
5. Implement ROM loading system
6. Complete debug interface
7. Super Mario World testing

## Build and Run

### Windows
```bash
# Build
build.bat

# Run
build\bin\snes_emu.exe
```

### Linux/macOS
```bash
# Build
mkdir build
cd build
cmake ..
make

# Run
./snes_emu
```

## Development Environment

- **Language**: C++17
- **GUI**: SDL2
- **Build**: CMake
- **Platform**: Windows, Linux, macOS

## Goal

To create a SNES emulator that can perfectly run Super Mario World.

## References

- [SNES Development Documentation](https://wiki.superfamicom.org/)
- [65c816 CPU Documentation](https://en.wikipedia.org/wiki/WDC_65C816)
- [SDL2 Documentation](https://wiki.libsdl.org/)
