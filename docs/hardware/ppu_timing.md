# SNES PPU Timing - 스캔라인 및 렌더링 타이밍

## 📋 목차
1. [개요](#개요)
2. [프레임 구조](#프레임-구조)
3. [스캔라인 타이밍](#스캔라인-타이밍)
4. [VBlank](#vblank)
5. [HBlank](#hblank)
6. [VRAM 액세스 타이밍](#vram-액세스-타이밍)
7. [렌더링 파이프라인](#렌더링-파이프라인)
8. [구현 가이드](#구현-가이드)

---

## 개요

PPU는 스캔라인 단위로 동작하며, 각 스캔라인은 **1364 master cycles**입니다.

### 핵심 타이밍

| 항목 | NTSC | PAL |
|------|------|-----|
| **프레임 레이트** | 60.098 Hz | 50.007 Hz |
| **총 스캔라인** | 262 | 312 |
| **표시 라인** | 224 | 239 |
| **VBlank 라인** | 38 | 73 |
| **스캔라인 사이클** | 1364 master | 1364 master |

---

## 프레임 구조

### NTSC (60Hz)

```
스캔라인 0-224:   표시 영역 (225 라인)
스캔라인 225-262: VBlank (38 라인)
총: 262 라인

프레임 시간: 16.639 ms
```

### PAL (50Hz)

```
스캔라인 0-239:   표시 영역 (240 라인)
스캔라인 240-312: VBlank (73 라인)
총: 312 라인

프레임 시간: 19.997 ms
```

---

## 스캔라인 타이밍

### 1 스캔라인 = 1364 master cycles

```
H-Counter: 0-339 (340 위치)
  0-255:   표시 픽셀 (256픽셀)
  256-339: HBlank (84 위치)
```

### H-Counter 구조

```
[  표시 영역  ][    HBlank    ]
0──────────255 256────────339
    1024 clk       340 clk
```

---

## VBlank

### 시작 타이밍

**NTSC**: 스캔라인 225  
**PAL**: 스캔라인 240

### VBlank 기간

```
NTSC: 38 라인 × 1364 cycles = 51832 cycles
    ≈ 2.4 ms

PAL:  73 라인 × 1364 cycles = 99572 cycles
    ≈ 4.6 ms
```

### VBlank 용도

```
1. VRAM 업데이트 (타일, 타일맵)
2. OAM 업데이트 (스프라이트)
3. CGRAM 업데이트 (팔레트)
4. 레지스터 설정
5. DMA/HDMA
6. 컨트롤러 입력
```

### $4212 (HVBJOY) - VBlank/HBlank 상태

```
Bit 7: V-Blank (1=VBlank 중)
Bit 6: H-Blank (1=HBlank 중)
Bit 0: Auto-Joypad Busy
```

**예제**:
```asm
:waitVBlank
    LDA $4212
    AND #$80     ; Bit 7
    BEQ :waitVBlank
```

---

## HBlank

### 기간

**84 H-Counter 위치** (340 master cycles, ~15.8 μs)

### HBlank 용도

```
1. HDMA 전송
2. 래스터 효과 (레지스터 변경)
3. 짧은 DMA
```

### 주의사항

HBlank는 **매우 짧아서** 대량 데이터 전송 불가! VBlank를 사용하세요.

---

## VRAM 액세스 타이밍

### 안전한 액세스 시간

**Force Blank** ($2100 Bit 7 = 1):
- 언제든지 VRAM 액세스 가능
- 화면 끔 (검정)

**VBlank**:
- VRAM 안전하게 액세스 가능
- 화면 켜진 상태

**HBlank**:
- 짧은 액세스만 가능
- HDMA 우선

### VBlank 외 액세스

```
렌더링 중 VRAM 쓰기:
→ 타이밍 충돌 가능
→ 화면에 노이즈
→ 권장하지 않음!
```

---

## 렌더링 파이프라인

### 스캔라인 처리 순서

```
1. 배경 타일 페치 (BG1-4)
2. 스프라이트 페치
3. 우선순위 계산
4. Color Math
5. 픽셀 출력
```

### 타이밍 다이어그램

```
스캔라인 N:
[0────255] 렌더링 + 출력
[256──339] HBlank
    ↓ HDMA
스캔라인 N+1:
```

---

## 구현 가이드

### PPU 클럭

```cpp
class PPU {
private:
    int hCounter = 0;  // 0-339
    int vCounter = 0;  // 0-261 (NTSC)
    
public:
    void runCycle() {
        hCounter++;
        
        if (hCounter >= 340) {
            hCounter = 0;
            vCounter++;
            
            // 스캔라인 처리
            if (vCounter < 225) {
                renderScanline(vCounter);
            } else if (vCounter == 225) {
                enterVBlank();
            } else if (vCounter >= 262) {
                vCounter = 0;
                exitVBlank();
            }
        }
        
        // HBlank 체크
        if (hCounter == 256) {
            enterHBlank();
        } else if (hCounter == 0) {
            exitHBlank();
        }
    }
};
```

---

## 참고 자료

- [SnesLab - PPU Timing](https://sneslab.net/wiki/PPU_timing)

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete










