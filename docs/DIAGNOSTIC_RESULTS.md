# SNES Emulator Diagnostic Results

**Date**: 2025-10-23

## Summary

Performed comprehensive diagnostic analysis of CPU, PPU, and APU state during emulation.

## CPU Status ✅ WORKING

- **Reset**: Correctly reads vector from $FFFC/D → jumps to $8000
- **Execution**: Running normally at $8000-$8012 (waiting loop)
- **NMI Handler**: Executing at $B138 when triggered
- **Instruction Execution**: 10,000 instructions over 10 frames = ~1000 per frame
- **RTI**: Correctly restoring PC and PBR

### CPU Activity Pattern
```
1. Boot → $8000 (initialization loop)
2. NMI triggered → $B138 (PPU setup, DMA)
3. RTI → return to $8012
4. Loop waiting for next NMI
```

### Key CPU Operations Observed
- REP/SEP: Mode switching (M/X flags)
- DMA transfers: $420B writes
- PPU register writes: $2100-$2114 range
- Proper stack operations with PBR preservation

## PPU Status ⚠️ PARTIAL

### Working Components
- Video initialization: ✅
- SDL window/renderer/texture creation: ✅
- Register writes being captured: ✅
- DMA transfers: ✅
- Frame rendering calls: ✅

### Issues Fixed
1. **Pixel Format Mismatch**
   - Problem: Code used 0xRRGGBBAA, SDL expects 0xAABBGGRR (little-endian)
   - Fix: Changed all pixel writes to use correct format
   - Locations fixed:
     - `initVideo()` test pattern (line 115)
     - `renderScanline()` (line 295)
     - Debug logging (line 153-155)

2. **Rendering Pipeline**
   - `renderScanline()` called during emulation
   - `renderFrame()` uploads to SDL texture
   - SDL_RenderPresent() displays frame

### PPU Register Activity
```
$2102/$2103: OAM address = $0000
$2116/$2117: VRAM address = $6000
$210D-$2114: Scroll registers (all = 0)
$2132: Color math
$4300-$4306: DMA channel 0 config
$4370-$4376: DMA channel 7 config
$420B: DMA enable
```

## APU Status ✅ BASIC WORKING

- IPL ROM loaded at $FFC0
- Boot handshake completed (signature 0xBBAA detected)
- SPC700 instructions executing
- Audio buffer initialized (32kHz, 2 channels, 1024 samples)
- Currently generating placeholder sine wave

## Memory Map Verification

### ROM Access Pattern
```
$FFFC/D → $0000/$8000 (reset vector)
$FFEA/B → $38/$B1 (NMI vector = $B138)
$7FEA/B → Same data (LoROM mirror)
```

### I/O Access Pattern
```
$2100-$21FF: PPU registers ✅
$4200-$421F: CPU I/O ✅
$4300-$437F: DMA registers ✅
```

## Performance Analysis

- **Frame Rate**: Limited to ~60 FPS by SDL_Delay(16ms)
- **Instructions/Frame**: ~1000
- **CPU Cycles/Frame**: ~1000 (1:1 ratio, needs adjustment)
- **Actual SNES**: ~89,342 cycles per frame at 21.477 MHz

### Performance Issues
1. CPU cycle timing too slow (should be ~89k per frame)
2. Main loop needs cycle-accurate timing
3. Logging significantly impacts performance

## Next Steps

1. **Immediate**: Verify pixel format fix resolves black screen
2. **Short-term**:
   - Implement proper BG layer rendering
   - Add sprite rendering
   - Fix CPU cycle timing
3. **Medium-term**:
   - Implement proper APU audio generation
   - Add game controller support
   - Save state functionality

## Test Results

### Diagnostic Mode (10 frames)
- Total instructions: 10,000
- Final PC: $B20E
- Final PBR: $00
- CPU Cycles: 10,000
- No infinite loops detected
- NMI handling successful

### Log Files Generated
- `cpu_trace.log`: CPU instruction trace
- `ppu_trace.log`: PPU register access
- `apu_trace.log`: APU operation trace



