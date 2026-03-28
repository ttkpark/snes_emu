# Zero to Hero: SNES 에뮬레이터 개발 로드맵

**작성일**: 2025-12-14  
**목적**: 빈 프로젝트에서 상용 게임 구동까지 최단 경로  
**기반**: 24개 하드웨어 문서 + IPL ROM 구현

---

## 🎯 전체 로드맵 개요

```
┌──────────────────────────────────────────────────────┐
│  Stage 1: Foundation (기반 시스템)                    │
│  ├─ ROM 로딩                                          │
│  ├─ 메모리 맵핑                                       │
│  └─ 버스 시스템                                       │
├──────────────────────────────────────────────────────┤
│  Stage 2: CPU (프로세서)                              │
│  ├─ 65816 명령어 세트                                 │
│  ├─ 주소 지정 모드                                    │
│  └─ CPU 테스트 통과                                   │
├──────────────────────────────────────────────────────┤
│  Stage 3: Basic Graphics (기본 그래픽)                │
│  ├─ 타일 뷰어                                         │
│  ├─ VRAM 포맷                                         │
│  └─ 첫 화면 출력                                      │
├──────────────────────────────────────────────────────┤
│  Stage 4: Background & Sprite (배경과 스프라이트)     │
│  ├─ DMA 구현                                          │
│  ├─ 배경 렌더링                                       │
│  └─ 스프라이트 렌더링                                 │
├──────────────────────────────────────────────────────┤
│  Stage 5: Input & Interrupt (입력과 인터럽트)         │
│  ├─ NMI/IRQ                                           │
│  ├─ 컨트롤러 입력                                     │
│  └─ 게임 조작 가능                                    │
├──────────────────────────────────────────────────────┤
│  Stage 6: Sound (사운드) ⭐ 현재 단계                 │
│  ├─ SPC700 CPU                                        │
│  ├─ IPL ROM 프로토콜 ⭐⭐⭐                           │
│  ├─ DSP                                               │
│  └─ 배경 음악 재생                                    │
├──────────────────────────────────────────────────────┤
│  Stage 7: Polish (완성도)                             │
│  ├─ HDMA                                              │
│  ├─ Color Math                                        │
│  └─ 특수 칩 지원                                      │
└──────────────────────────────────────────────────────┘
```

---

## 📚 Stage 1: Foundation (1-2주)

### 목표
ROM 파일을 읽고 메모리 구조를 잡습니다.

### 참조 문서
- `docs/hardware/sys_rom_header.md`
- `docs/hardware/mem_memory_map_complete.md`

### 구현 체크리스트

#### 1.1 ROM 로더

```cpp
class Cartridge {
public:
    bool loadROM(const std::string& filename);
    
    // ROM 헤더 정보
    std::string getTitle();
    ROMType getType();      // LoROM, HiROM
    int getROMSize();
    int getRAMSize();
    
private:
    std::vector<uint8_t> m_rom;
    ROMHeader m_header;
};
```

**할 일**:
- [ ] ROM 파일 읽기 (`.sfc`, `.smc` 형식)
- [ ] 헤더 512바이트 건너뛰기 (SMC 형식)
- [ ] 내부 헤더 파싱 ($00FFB0-$00FFDF)
- [ ] LoROM/HiROM 판별
- [ ] 체크섬 검증

#### 1.2 메모리 버스

```cpp
class Bus {
public:
    uint8_t read(uint32_t address);
    void write(uint32_t address, uint8_t value);
    
private:
    Memory* m_memory;
    Cartridge* m_cartridge;
    PPU* m_ppu;
    APU* m_apu;
    
    uint8_t routeRead(uint32_t address);
    void routeWrite(uint32_t address, uint8_t value);
};
```

**할 일**:
- [ ] 주소 라우팅 (WRAM, ROM, I/O 분기)
- [ ] 뱅크 시스템 (24비트 주소 → 16비트 변환)
- [ ] 미러링 처리

#### 1.3 메모리 할당

```cpp
class Memory {
public:
    Memory();
    
    uint8_t readWRAM(uint32_t address);
    void writeWRAM(uint32_t address, uint8_t value);
    
private:
    std::vector<uint8_t> m_wram;  // 128KB
    std::vector<uint8_t> m_sram;  // 배터리 백업 RAM
};
```

**할 일**:
- [ ] WRAM 128KB 할당 ($7E0000-$7FFFFF)
- [ ] SRAM 할당 (게임마다 다름)
- [ ] 세이브/로드 기능

### 테스트 방법

```cpp
// ROM 로딩 테스트
Cartridge cart;
assert(cart.loadROM("test.sfc"));
assert(cart.getTitle() == "TEST ROM");
assert(cart.getType() == ROMType::LoROM);

// 메모리 테스트
bus.write(0x7E0000, 0x42);
assert(bus.read(0x7E0000) == 0x42);
```

---

## 🧠 Stage 2: CPU (2-4주)

### 목표
CPU 테스트 ROM을 통과합니다. **가장 지루하지만 가장 중요한 단계!**

### 참조 문서
- `docs/hardware/cpu_65816_opcodes.md`
- `docs/hardware/cpu_addressing_modes.md`
- `docs/hardware/cpu_timing.md`
- `docs/hardware/cpu_interrupt_handling.md`

### 구현 체크리스트

#### 2.1 CPU 코어

```cpp
class CPU {
public:
    void reset();
    void step();
    
    // 레지스터
    uint16_t A;      // Accumulator (8/16비트)
    uint16_t X, Y;   // Index registers
    uint16_t SP;     // Stack Pointer
    uint8_t P;       // Processor Status
    uint8_t DBR;     // Data Bank
    uint8_t PBR;     // Program Bank
    uint16_t PC;     // Program Counter
    
private:
    void executeOpcode(uint8_t opcode);
};
```

**할 일**:
- [ ] Fetch-Decode-Execute 루프
- [ ] 256개 opcode 구현 (중복 제외 ~170개)
- [ ] 8/16비트 모드 전환 (M, X 플래그)
- [ ] 사이클 카운팅

#### 2.2 주소 지정 모드 (25가지)

```cpp
enum class AddressingMode {
    Implied,
    Accumulator,
    Immediate,
    Absolute,
    AbsoluteLong,
    Direct,
    DirectIndexedX,
    DirectIndexedY,
    DirectIndirect,
    DirectIndirectLong,
    AbsoluteIndexedX,
    AbsoluteIndexedY,
    AbsoluteLongIndexedX,
    DirectIndexedIndirectX,
    DirectIndirectIndexedY,
    DirectIndirectLongIndexedY,
    StackRelative,
    StackRelativeIndirectIndexedY,
    AbsoluteIndirect,
    AbsoluteIndirectLong,
    AbsoluteIndexedIndirect,
    ProgramCounterRelative,
    ProgramCounterRelativeLong,
    BlockMove,
    Implied8bit,
};

uint32_t CPU::getEffectiveAddress(AddressingMode mode);
```

**할 일**:
- [ ] 각 주소 모드 구현
- [ ] 페이지 경계 넘을 때 +1 사이클
- [ ] 뱅크 경계 처리

#### 2.3 인터럽트

```cpp
void CPU::handleNMI();
void CPU::handleIRQ();
void CPU::handleBRK();
void CPU::handleCOP();
void CPU::handleABORT();
```

**할 일**:
- [ ] NMI (V-Blank)
- [ ] IRQ (타이머, 기타)
- [ ] 벡터 테이블 ($00FFE4-$00FFFF)
- [ ] 인터럽트 우선순위

### 테스트 방법

```
1. Klaus's 65816 Test Suite 다운로드
2. 에뮬레이터에서 실행
3. PC가 무한 루프에 걸리면 → 성공!
4. PC가 다른 곳으로 이동하면 → 실패 (명령어 버그)
```

**예상 소요 시간**: 2-4주 (버그 수정 포함)

---

## 🖼️ Stage 3: Basic Graphics (1주)

### 목표
화면에 **깨진 타일이라도** 띄웁니다. 이게 되면 그래픽 데이터가 로딩되고 있다는 증거!

### 참조 문서
- `docs/hardware/ppu_vram_format.md` ⭐ 핵심
- `docs/hardware/ppu_s-ppu1.md`

### 구현 체크리스트

#### 3.1 VRAM 할당

```cpp
class PPU {
public:
    void reset();
    uint16_t readVRAM(uint16_t address);
    void writeVRAM(uint16_t address, uint16_t value);
    
private:
    std::vector<uint8_t> m_vram;  // 64KB
    uint16_t m_vramAddress;
};
```

**할 일**:
- [ ] VRAM 64KB 할당
- [ ] VRAM 레지스터 ($2115-$2119)
- [ ] 주소 증가 모드

#### 3.2 타일 디코더

```cpp
// ppu_vram_format.md의 코드 사용!
void decodeTile2BPP(const uint8_t* vram, int tileIndex, uint8_t* output) {
    for (int row = 0; row < 8; row++) {
        uint8_t plane0 = vram[tileIndex * 16 + row * 2];
        uint8_t plane1 = vram[tileIndex * 16 + row * 2 + 1];
        
        for (int col = 0; col < 8; col++) {
            int bit = 7 - col;
            uint8_t color = ((plane0 >> bit) & 1) |
                          (((plane1 >> bit) & 1) << 1);
            output[row * 8 + col] = color;
        }
    }
}
```

**할 일**:
- [ ] 2BPP 디코더
- [ ] 4BPP 디코더
- [ ] 8BPP 디코더

#### 3.3 타일 뷰어 (디버그 도구)

```cpp
void TileViewer::render() {
    for (int tileY = 0; tileY < 32; tileY++) {
        for (int tileX = 0; tileX < 32; tileX++) {
            int tileIndex = tileY * 32 + tileX;
            
            uint8_t tileData[64];
            decodeTile2BPP(vram, tileIndex, tileData);
            
            // SDL로 화면에 그리기
            drawTile(tileX * 8, tileY * 8, tileData);
        }
    }
}
```

**할 일**:
- [ ] 전체 VRAM을 타일로 표시하는 창
- [ ] 팔레트 적용
- [ ] 타일 번호 표시

### 테스트 방법

```cpp
// Super Mario World ROM 로드
// VRAM이 채워지면 타일 뷰어에 마리오 그래픽이 보여야 함
```

**성공 기준**: 타일 뷰어에 뭔가 의미 있는 그래픽이 보임 (깨져도 OK)

---

## 🌍 Stage 4: Background & Sprite (2-3주)

### 목표
*Super Mario World* 타이틀 화면이 보여야 합니다.

### 참조 문서
- `docs/hardware/ppu_background_modes.md`
- `docs/hardware/ppu_registers_bitmap.md`
- `docs/hardware/mem_dma_hdma_complete.md`

### 구현 체크리스트

#### 4.1 DMA

```cpp
class DMA {
public:
    void transfer(int channel);
    
private:
    struct DMAChannel {
        uint8_t control;
        uint8_t destReg;
        uint16_t srcAddr;
        uint8_t srcBank;
        uint16_t size;
    };
    
    DMAChannel m_channels[8];
};
```

**할 일**:
- [ ] 8개 DMA 채널
- [ ] CPU → PPU 전송
- [ ] CPU → WRAM 전송
- [ ] 자동 증가/고정 모드

#### 4.2 배경 렌더링

```cpp
void PPU::renderBackground(int bgNum) {
    // 1. 타일맵 읽기
    uint16_t tilemapAddr = getTilemapAddress(bgNum);
    
    // 2. 각 타일 렌더링
    for (int y = 0; y < 32; y++) {
        for (int x = 0; x < 32; x++) {
            uint16_t tileData = readVRAM(tilemapAddr + y * 64 + x * 2);
            
            int tileNum = tileData & 0x3FF;
            int palette = (tileData >> 10) & 7;
            bool flipX = (tileData >> 14) & 1;
            bool flipY = (tileData >> 15) & 1;
            
            renderTile(x * 8, y * 8, tileNum, palette, flipX, flipY);
        }
    }
}
```

**할 일**:
- [ ] Mode 1 (가장 흔함)
- [ ] Mode 0
- [ ] Mode 7 (회전/확대)
- [ ] 스크롤 ($210D-$2114)

#### 4.3 스프라이트

```cpp
void PPU::renderSprites() {
    // OAM 읽기
    for (int i = 0; i < 128; i++) {
        uint8_t x = m_oam[i * 4 + 0];
        uint8_t y = m_oam[i * 4 + 1];
        uint8_t tile = m_oam[i * 4 + 2];
        uint8_t attr = m_oam[i * 4 + 3];
        
        if (y >= 240) continue;  // 화면 밖
        
        renderSprite(x, y, tile, attr);
    }
}
```

**할 일**:
- [ ] OAM 512바이트 + 32바이트
- [ ] 8x8, 16x16, 32x32, 64x64 스프라이트
- [ ] 우선순위
- [ ] 수평/수직 플립

### 테스트 방법

```
Super Mario World 로드
→ 타이틀 화면이 보여야 함
→ "SUPER MARIO WORLD" 로고
→ 마리오 스프라이트
```

---

## 🎮 Stage 5: Input & Interrupt (1주)

### 목표
게임을 조작할 수 있습니다!

### 참조 문서
- `docs/hardware/io_controller_input.md`
- `docs/hardware/cpu_interrupt_handling.md`

### 구현 체크리스트

#### 5.1 NMI (V-Blank)

```cpp
void Emulator::frame() {
    // 224 스캔라인 렌더링
    for (int scanline = 0; scanline < 224; scanline++) {
        ppu->renderScanline(scanline);
        cpu->runCycles(CYCLES_PER_SCANLINE);
    }
    
    // V-Blank 시작 → NMI 발생
    cpu->triggerNMI();
    
    // V-Blank 기간
    for (int scanline = 224; scanline < 262; scanline++) {
        cpu->runCycles(CYCLES_PER_SCANLINE);
    }
}
```

**할 일**:
- [ ] 정확한 타이밍 (262 스캔라인)
- [ ] NMI 플래그 ($4210)
- [ ] Auto-joypad read ($4218-$421F)

#### 5.2 컨트롤러 입력

```cpp
class Input {
public:
    uint16_t getController(int port);  // 0 or 1
    
private:
    uint16_t m_controller[2];
    
    enum Button {
        BTN_B      = 0x8000,
        BTN_Y      = 0x4000,
        BTN_SELECT = 0x2000,
        BTN_START  = 0x1000,
        BTN_UP     = 0x0800,
        BTN_DOWN   = 0x0400,
        BTN_LEFT   = 0x0200,
        BTN_RIGHT  = 0x0100,
        BTN_A      = 0x0080,
        BTN_X      = 0x0040,
        BTN_L      = 0x0020,
        BTN_R      = 0x0010,
    };
};
```

**할 일**:
- [ ] 키보드 매핑
- [ ] Auto-read 구현
- [ ] 2P 지원

### 테스트 방법

```
Super Mario World 실행
→ Start 버튼으로 시작
→ 방향키로 이동
→ B 버튼으로 점프
```

---

## 🔊 Stage 6: Sound (3-4주) ⭐ **현재 단계**

### 목표
배경 음악이 나와야 합니다. **가장 어려운 단계!**

### 참조 문서
- `docs/hardware/apu_spc700_instructions.md`
- `docs/hardware/apu_dsp_registers.md`
- `docs/hardware/apu_ipl_rom.md` ⭐⭐⭐ **필수!**
- `docs/hardware/apu_timing.md`

### 구현 체크리스트

#### 6.1 SPC700 CPU

```cpp
class SPC700 {
public:
    void reset();
    void step();
    
    // 레지스터
    uint8_t A, X, Y, SP, PSW;
    uint16_t PC;
    
private:
    void executeOpcode(uint8_t opcode);
};
```

**할 일**:
- [ ] 256개 opcode (65816과 다름!)
- [ ] 25가지 주소 모드
- [ ] 사이클 카운팅

#### 6.2 IPL ROM 프로토콜 ⭐⭐⭐

```cpp
void APU::writePort(uint8_t port, uint8_t value) {
    // apu_ipl_rom.md의 코드 참조!
    
    switch (m_spcLoadState) {
        case SPC_LOAD_IDLE:
            if (port == 0 && value == 0xCC) {
                m_spcLoadState = SPC_LOAD_WAIT_BBAA;
            }
            break;
            
        case SPC_LOAD_RECEIVING:
            if (port == 3 && value == 0x00) {
                m_spcLoadState = SPC_LOAD_WAIT_EXEC;
            }
            break;
            
        case SPC_LOAD_WAIT_EXEC:
            if (port == 0) {
                m_iplromEnable = false;  // ⭐ 핵심!
                m_regs.pc = (m_cpuPorts[1] << 8) | m_cpuPorts[0];
            }
            break;
    }
}
```

**할 일**:
- [ ] 5단계 프로토콜 구현
- [ ] Port 3 = 0x00 처리 ⭐
- [ ] Port 0 = 실행 주소 처리 ⭐
- [ ] IPL ROM 비활성화 ⭐

#### 6.3 DSP

```cpp
class DSP {
public:
    void generateSamples(int16_t* output, int count);
    
private:
    struct Voice {
        int16_t* sampleBuffer;
        int samplePos;
        int pitch;
        int volume;
        int envelope;
    };
    
    Voice m_voices[8];
    
    void mixVoices(int16_t* output, int count);
};
```

**할 일**:
- [ ] 8개 음성 채널
- [ ] BRR 디코딩
- [ ] ADSR 엔벨로프
- [ ] 믹싱

#### 6.4 CPU-APU 동기화

```cpp
void Emulator::frame() {
    const int CPU_CYCLES_PER_FRAME = 262 * 341;  // ~89342
    const int APU_CYCLES_PER_FRAME = 64000;      // 32kHz
    
    int cpu_cycles = 0;
    int apu_cycles = 0;
    
    while (cpu_cycles < CPU_CYCLES_PER_FRAME) {
        cpu_cycles += cpu->step();
        
        // APU는 CPU보다 약간 느림
        while (apu_cycles * 21 < cpu_cycles * 16) {
            apu_cycles += apu->step();
        }
    }
    
    // 오디오 샘플 생성
    apu->generateAudio();
}
```

**할 일**:
- [ ] 정확한 타이밍 비율 (21:16)
- [ ] 오디오 버퍼 관리
- [ ] SDL 오디오 콜백

### 테스트 방법

```powershell
# 1. PC 분포 확인
python pc_distribution_analyzer.py

# 성공 기준:
# PROGRAM [0x0000-0xFFBF]:  > 90% ✓
# IPL_ROM [0xFFC0-0xFFFF]:  < 10% ✓

# 2. 포트 통신 확인
.\analyze_ports.ps1

# 3. spctest.sfc 실행
.\snes_emu_complete.exe spctest.sfc

# 성공: "IPL BOOT COMPLETE!" + 테스트 진행
```

---

## ✨ Stage 7: Polish (2-4주)

### 목표
그래픽이 깨지는 게임들을 수정합니다.

### 참조 문서
- `docs/hardware/ppu_s-ppu2.md`
- `docs/hardware/mem_hdma_effects.md`
- `docs/hardware/chip_*.md`

### 구현 체크리스트

#### 7.1 HDMA

```cpp
class HDMA {
public:
    void startFrame();
    void doScanline(int scanline);
    
private:
    struct HDMAChannel {
        bool enabled;
        uint8_t mode;
        uint16_t tableAddr;
        uint8_t lineCounter;
    };
    
    HDMAChannel m_channels[8];
};
```

**할 일**:
- [ ] 스캔라인마다 레지스터 변경
- [ ] 간접 모드
- [ ] 물결 효과, 그라디언트 등

#### 7.2 Window & Color Math

```cpp
void PPU::renderPixel(int x, int y) {
    uint16_t color = getBackgroundColor(x, y);
    
    // Window 체크
    if (isInsideWindow(x, y)) {
        color = applyWindow(color);
    }
    
    // Color Math (반투명)
    if (colorMathEnabled()) {
        color = blendColors(color, getSubscreen(x, y));
    }
    
    setPixel(x, y, color);
}
```

**할 일**:
- [ ] Window 1, 2
- [ ] Color Math (add/sub)
- [ ] Subscreen

#### 7.3 특수 칩

```cpp
// DSP-1 (Mario Kart)
class DSP1 {
public:
    uint8_t read();
    void write(uint8_t value);
    
private:
    void processCommand();
    // 삼각함수, 거리 계산 등
};

// SuperFX (Star Fox)
class SuperFX {
public:
    void step();
    
private:
    // 16비트 RISC CPU
};
```

**할 일**:
- [ ] DSP-1, DSP-2
- [ ] SuperFX
- [ ] SA-1

---

## 📊 전체 일정 요약

| Stage | 기간 | 난이도 | 현재 상태 |
|-------|------|--------|-----------|
| 1. Foundation | 1-2주 | ⭐ | ✅ 완료 |
| 2. CPU | 2-4주 | ⭐⭐⭐ | ✅ 완료 |
| 3. Basic Graphics | 1주 | ⭐⭐ | ✅ 완료 |
| 4. Background & Sprite | 2-3주 | ⭐⭐⭐ | ✅ 완료 |
| 5. Input & Interrupt | 1주 | ⭐ | ✅ 완료 |
| **6. Sound** | **3-4주** | **⭐⭐⭐⭐** | **🔴 진행 중** |
| 7. Polish | 2-4주 | ⭐⭐⭐ | ⬜ 대기 |

**총 예상 기간**: 12-20주 (3-5개월)

---

## 🎯 현재 위치와 다음 단계

### 현재 상황 (Stage 6: Sound)

```
✅ SPC700 CPU 구현 완료
✅ 포트 통신 기본 구조 완료
❌ IPL ROM 프로토콜 미완성 ← 🔴 현재 문제
⬜ DSP 구현 대기
⬜ 오디오 출력 대기
```

### 즉시 해야 할 일 ⭐⭐⭐

1. **`src/apu/apu.cpp` 수정**
   - `docs/hardware/apu_ipl_rom.md`의 코드 적용
   - Port 3 = 0x00 처리
   - Port 0 = 실행 주소 처리
   - IPL ROM 비활성화

2. **빌드 & 테스트**
   ```batch
   .\build_complete.bat
   .\snes_emu_complete.exe spctest.sfc
   ```

3. **검증**
   ```powershell
   python pc_distribution_analyzer.py
   # PROGRAM > 90%면 성공!
   ```

### 성공 후 다음 단계

```
1. DSP 구현 (2주)
2. BRR 디코딩 (1주)
3. 오디오 믹싱 (1주)
4. Stage 6 완료! 🎉
```

---

## 🎓 학습 자료

### 추천 순서

```
1. ROM 로딩 → sys_rom_header.md
2. CPU 구현 → cpu_65816_opcodes.md
3. 그래픽 → ppu_vram_format.md ⭐ 실전 코드
4. 사운드 → apu_ipl_rom.md ⭐ 현재 필수
5. 고급 → mem_hdma_effects.md
```

### 유용한 도구

- **PC 분포 분석기**: `pc_distribution_analyzer.py`
- **포트 통신 분석기**: `analyze_ports.ps1`
- **실시간 모니터**: `realtime_monitor.py`
- **타일 뷰어**: 직접 구현

---

## 🏆 완성 기준

### Minimal (최소)
- Super Mario World 플레이 가능
- 음악 재생
- 세이브/로드

### Full (완전)
- 95% 게임 호환성
- 정확한 타이밍
- 특수 칩 지원
- 디버거 내장

---

**작성일**: 2025-12-14  
**기반**: 24개 하드웨어 문서 + IPL ROM  
**현재 단계**: Stage 6 (Sound) - IPL ROM 프로토콜 구현 중  
**다음 마일스톤**: "IPL BOOT COMPLETE!" 메시지 출력

**Happy Emulating! 🎮**










