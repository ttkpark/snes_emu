# SA-1 - Super Accelerator 1

## 📋 목차
1. [개요](#개요)
2. [하드웨어 스펙](#하드웨어-스펙)
3. [메모리 맵](#메모리-맵)
4. [DMA](#dma)
5. [게임 목록](#게임-목록)

---

## 개요

**SA-1**은 카트리지 내장 **65C816 CPU** (메인 CPU와 동일)로, 2배 빠른 클럭(10.74 MHz)으로 동작합니다.

### 주요 게임
- **Super Mario RPG** (1996)
- **Kirby's Dream Land 3** (1997)
- **Kirby Super Star** (1996)

---

## 하드웨어 스펙

| 항목 | 스펙 |
|------|------|
| **CPU** | 65C816 (메인 CPU와 동일) |
| **클럭** | 10.74 MHz (메인의 2-4배) |
| **내부 RAM** | 2KB I-RAM (128바이트 × 16 블록) |
| **Bitmap RAM** | 128KB (BW-RAM) |
| **곱셈/나눗셈** | 하드웨어 가속 |

---

## 메모리 맵

### SNES CPU → SA-1 통신

```
$2200-$2208: SA-1 제어 레지스터
$2209-$220F: SA-1 상태
$2210-$2217: DMA 설정
$2218-$221F: 산술 연산
$2220-$2223: 비트맵 설정
$2224-$2225: 벡터
```

### SA-1 메모리 액세스

SA-1은 ROM, SRAM, BW-RAM에 직접 액세스 가능:
```
00-3F:8000-FFFF: ROM
40-4F:0000-FFFF: BW-RAM
60-6F:0000-FFFF: SA-1 Internal RAM (I-RAM)
```

### I-RAM 충돌 처리

**SNES CPU와 SA-1이 동시에 I-RAM에 액세스할 때**:

```cpp
// 우선순위: SA-1 > SNES CPU
class SA1_IRAM {
private:
    uint8_t iram[2048];
    bool sa1Accessing = false;
    
public:
    uint8_t read(uint16_t addr, bool isSA1) {
        if (!isSA1 && sa1Accessing) {
            // SNES CPU가 읽으려 하지만 SA-1이 사용 중
            return 0x00;  // 또는 마지막 값
        }
        return iram[addr & 0x7FF];
    }
    
    void write(uint16_t addr, uint8_t value, bool isSA1) {
        if (!isSA1 && sa1Accessing) {
            // SNES CPU 쓰기 무시
            return;
        }
        iram[addr & 0x7FF] = value;
    }
};
```

**충돌 시나리오**:
1. **SA-1 우선**: SA-1이 I-RAM 액세스 중이면 SNES CPU 접근 차단
2. **Wait State**: SNES CPU는 SA-1이 끝날 때까지 대기 (또는 0x00 반환)
3. **레지스터 $2301 (SCNT)** Bit 7: I-RAM 충돌 플래그

---

## DMA

SA-1은 **4가지 DMA 타입** 지원:
1. **Character Conversion DMA** (타일 변환)
2. **Normal DMA** (일반 전송)
3. **Cumulative Sum** (누적 합산)
4. **Bitmap Conversion** (비트맵 변환)

---

## 게임 목록

| 게임 | 발매 | 주 용도 |
|------|------|---------|
| Super Mario RPG | 1996 | 3D 회전, AI |
| Kirby Super Star | 1996 | 다중 처리 |
| Kirby's Dream Land 3 | 1997 | 효과 처리 |

---

## 구현 가이드

```cpp
class SA1 {
private:
    CPU65816 cpu;  // SA-1 CPU
    uint8_t iram[2048];  // 내부 RAM
    
public:
    void run(int cycles) {
        cpu.run(cycles * 2);  // 2배 속도
    }
    
    void dmaCharacterConversion() {
        // 2bpp → 4bpp 변환
    }
};
```

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete
