# Hardware 문서 분류 완료

**날짜**: 2025-12-14  
**작업**: 문서 파일명 범주별 정리

---

## 📁 분류 체계

모든 hardware 문서가 다음 접두사로 분류되었습니다:

- `cpu_` - CPU 관련
- `ppu_` - PPU (그래픽) 관련
- `apu_` - APU (오디오) 관련
- `mem_` - 메모리/DMA 관련
- `io_` - 입출력 관련
- `chip_` - 특수 칩 관련
- `sys_` - 시스템 관련

---

## 📋 변경된 파일 목록

### CPU 관련 (4개)
- ✅ `cpu_65816_opcodes.md` (유지)
- ✅ `cpu_addressing_modes.md` (유지)
- ✅ `cpu_timing.md` (유지)
- ✅ `cpu_interrupt_handling.md` (변경: interrupt_handling.md)

### PPU 관련 (9개)
- ✅ `ppu_registers_bitmap.md` (유지)
- ✅ `ppu_s-ppu1.md` (유지)
- ✅ `ppu_s-ppu2.md` (유지)
- ✅ `ppu_timing.md` (유지)
- ✅ `ppu_background_modes.md` (변경: background_modes.md)
- ✅ `ppu_vram_format.md` (변경: vram_format.md)
- ✅ `ppu_mode7_math.md` (변경: mode7_math.md)
- ✅ `ppu_color_math_tricks.md` (변경: color_math_tricks.md)

### APU 관련 (3개)
- ✅ `apu_timing.md` (유지)
- ✅ `apu_spc700_instructions.md` (변경: spc700_instructions.md)
- ✅ `apu_dsp_registers.md` (변경: dsp_registers.md)

### 메모리 관련 (3개)
- ✅ `mem_memory_map_complete.md` (변경: memory_map_complete.md)
- ✅ `mem_dma_hdma_complete.md` (변경: dma_hdma_complete.md)
- ✅ `mem_hdma_effects.md` (변경: hdma_effects.md)

### 입출력 관련 (1개)
- ✅ `io_controller_input.md` (변경: controller_input.md)

### 특수 칩 관련 (3개)
- ✅ `chip_superfx.md` (변경: superfx.md)
- ✅ `chip_sa1.md` (변경: sa1.md)
- ✅ `chip_dsp_chips.md` (변경: dsp_chips.md)

### 시스템 관련 (1개)
- ✅ `sys_rom_header.md` (변경: rom_header.md)

---

## 📊 통계

```
총 문서:        23개
변경된 문서:    15개
유지된 문서:     8개
```

---

## 🎯 분류별 개수

```
CPU:    4개 (17.4%)
PPU:    9개 (39.1%)
APU:    3개 (13.0%)
MEM:    3개 (13.0%)
IO:     1개 ( 4.3%)
CHIP:   3개 (13.0%)
SYS:    1개 ( 4.3%)
```

---

## 📖 사용 가이드

### 파일 찾기

**CPU 관련 문서를 찾을 때**:
```bash
ls cpu_*.md
```

**PPU 관련 문서를 찾을 때**:
```bash
ls ppu_*.md
```

**APU 관련 문서를 찾을 때**:
```bash
ls apu_*.md
```

### 알파벳순 정렬

파일 탐색기에서 이름순으로 정렬하면 자동으로 범주별로 그룹화됩니다:

```
apu_dsp_registers.md
apu_spc700_instructions.md
apu_timing.md
chip_dsp_chips.md
chip_sa1.md
chip_superfx.md
cpu_65816_opcodes.md
cpu_addressing_modes.md
cpu_interrupt_handling.md
cpu_timing.md
io_controller_input.md
mem_dma_hdma_complete.md
mem_hdma_effects.md
mem_memory_map_complete.md
ppu_background_modes.md
ppu_color_math_tricks.md
ppu_mode7_math.md
ppu_registers_bitmap.md
ppu_s-ppu1.md
ppu_s-ppu2.md
ppu_timing.md
ppu_vram_format.md
sys_rom_header.md
```

---

## 🔍 빠른 참조

| 컴포넌트 | 파일 수 | 주요 파일 |
|----------|---------|-----------|
| **CPU** | 4 | cpu_65816_opcodes.md, cpu_timing.md |
| **PPU** | 9 | ppu_registers_bitmap.md, ppu_vram_format.md |
| **APU** | 3 | apu_spc700_instructions.md, apu_dsp_registers.md |
| **Memory** | 3 | mem_memory_map_complete.md, mem_dma_hdma_complete.md |
| **I/O** | 1 | io_controller_input.md |
| **Chip** | 3 | chip_superfx.md, chip_sa1.md |
| **System** | 1 | sys_rom_header.md |

---

**작성일**: 2025-12-14  
**상태**: ✅ 완료










