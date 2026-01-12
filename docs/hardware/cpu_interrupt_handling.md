# SNES Interrupt Handling - NMI/IRQ/Reset

## 📋 목차
1. [개요](#개요)
2. [인터럽트 종류](#인터럽트-종류)
3. [NMI (Non-Maskable Interrupt)](#nmi-non-maskable-interrupt)
4. [IRQ (Interrupt Request)](#irq-interrupt-request)
5. [Reset](#reset)
6. [인터럽트 우선순위](#인터럽트-우선순위)
7. [인터럽트 처리 시퀀스](#인터럽트-처리-시퀀스)
8. [구현 가이드](#구현-가이드)
9. [디버깅](#디버깅)

---

## 개요

SNES는 3가지 하드웨어 인터럽트를 지원합니다: **NMI** (VBlank), **IRQ** (타이머/사용자 정의), **Reset** (시스템 리셋).

### 인터럽트 벡터

**Native 모드** (65816):
```
$00:FFEA-FFEB: COP (Software Interrupt)
$00:FFEC-FFED: BRK (Break)
$00:FFEE-FFEF: ABORT (External Abort)
$00:FFF0-FFF1: NMI (Non-Maskable)
$00:FFF4-FFF5: IRQ (Interrupt Request)
$00:FFFC-FFFD: Reset
```

**Emulation 모드** (6502 호환):
```
$00:FFF8-FFF9: ABORT
$00:FFFA-FFFB: NMI
$00:FFFE-FFFF: IRQ/BRK
$00:FFFC-FFFD: Reset
```

---

## 인터럽트 종류

### 비교표

| 인터럽트 | 발생 조건 | 마스킹 가능 | 주 용도 |
|----------|-----------|-------------|---------|
| **NMI** | VBlank 시작 | 부분적 ($4200 Bit 7) | 화면 업데이트, DMA |
| **IRQ** | H/V 타이머 | 완전 (I 플래그) | 래스터 효과, 타이밍 |
| **Reset** | 리셋 버튼 | 불가능 | 시스템 초기화 |
| **BRK** | 명령어 | 완전 (I 플래그) | 디버깅 |
| **COP** | 명령어 | 불가능 | OS 호출 |

---

## NMI (Non-Maskable Interrupt)

### 발생 조건

**VBlank 시작 시** 자동 발생 (스캔라인 225, NTSC 기준)

```
화면 렌더링 (224 라인)
    ↓
VBlank 시작 (라인 225) → NMI 발생!
    ↓
VBlank 기간 (~4.5ms, ~70 라인)
    ↓
다음 프레임 시작
```

### 활성화

**$4200 (NMITIMEN)** - NMI Enable:
```
Bit 7: NMI Enable (1=활성화, 0=비활성화)
Bit 5-4: H/V-IRQ Enable
Bit 0: Auto-Joypad
```

**예제**:
```asm
; NMI 활성화
LDA #$81     ; Bit7=NMI, Bit0=Auto-Joypad
STA $4200

; NMI 비활성화
LDA #$00
STA $4200
```

### NMI 핸들러

```asm
NMI_Handler:
    ; 1. 레지스터 저장
    PHB          ; Data Bank
    PHD          ; Direct Page
    REP #$30     ; 16비트 모드
    PHA          ; Accumulator
    PHX          ; X
    PHY          ; Y
    
    ; 2. NMI 플래그 읽기 (자동 클리어)
    LDA $4210    ; RDNMI
    
    ; 3. VBlank 작업
    JSR UpdateGraphics
    JSR HandleInput
    JSR UpdateAudio
    
    ; 4. 레지스터 복원
    PLY
    PLX
    PLA
    PLD
    PLB
    
    RTI
```

### $4210 (RDNMI)

**읽기 전용**:
```
Bit 7: NMI Flag (1=NMI 발생, 읽으면 자동 클리어)
Bit 6-4: PPU Version
Bit 3-0: (사용 안 함)
```

**중요**: $4210 읽기 시 NMI 플래그가 **자동으로 클리어**됩니다!

```cpp
uint8_t readRDNMI() {
    uint8_t value = (nmiFlag ? 0x80 : 0) | (ppuVersion << 4);
    nmiFlag = false;  // 자동 클리어!
    return value;
}
```

### NMI 타이밍

```
VBlank 시작 (스캔라인 225)
    ↓
~1 스캔라인 후 NMI 발생 (스캔라인 225-226)
    ↓
CPU가 현재 명령어 완료
    ↓
NMI 시퀀스 시작 (7 사이클)
    ↓
NMI 핸들러 실행
```

**VBlank 기간**: ~4.5ms (~70 스캔라인)

---

## IRQ (Interrupt Request)

### 발생 조건

1. **H-Timer** (수평 카운터)
2. **V-Timer** (수직 카운터)
3. **H/V-Timer 조합**

### 활성화

**$4200 (NMITIMEN)**:
```
Bit 5: V-IRQ Enable
Bit 4: H-IRQ Enable

조합:
00: IRQ 끔
01: H-IRQ (매 스캔라인마다 H 위치에서)
10: V-IRQ (특정 스캔라인에서)
11: H/V-IRQ (특정 스캔라인의 특정 H 위치)
```

### 타이머 설정

**$4207-$4208 (HTIME)** - H-Timer:
```
$4207: HTIMEL (Low 8비트)
$4208: HTIMEH (High 1비트, 총 9비트)

값 범위: 0-339 (H-Counter 위치)
```

**$4209-$420A (VTIME)** - V-Timer:
```
$4209: VTIMEL (Low 8비트)
$420A: VTIMEH (High 1비트, 총 9비트)

값 범위: 0-261 (스캔라인 번호, NTSC)
```

### 예제: H-IRQ (래스터 효과)

```asm
; 스캔라인 100에서 IRQ 발생
LDA #100
STA $4209    ; VTIMEL
LDA #$00
STA $420A    ; VTIMEH

; V-IRQ 활성화
LDA #$A0     ; Bit7=NMI, Bit5=V-IRQ
STA $4200

IRQ_Handler:
    PHA
    
    ; IRQ 플래그 읽기 (클리어)
    LDA $4211    ; TIMEUP
    
    ; 배경 스크롤 변경 (파동 효과)
    LDA WaveOffset
    STA $210D    ; BG1HOFS
    
    PLA
    RTI
```

### $4211 (TIMEUP)

**읽기 전용**:
```
Bit 7: IRQ Flag (1=IRQ 발생, 읽으면 자동 클리어)
Bit 6-0: (사용 안 함)
```

### CPU I 플래그

IRQ는 **I 플래그**로 마스킹 가능:

```asm
SEI    ; I=1, IRQ 비활성화
CLI    ; I=0, IRQ 활성화
```

**NMI**는 I 플래그로 마스킹 **불가능**!

---

## Reset

### 발생 조건

1. 전원 켜기 (Power-On)
2. Reset 버튼 누르기

### Reset 시퀀스

```
1. CPU를 Emulation 모드로 전환
2. I, D 플래그 설정 (IRQ 비활성화, Decimal 비활성화)
3. Stack Pointer = $01FF
4. Direct Page = $0000
5. Data Bank = $00
6. Program Bank = $00
7. PC = [$FFFC-FFFD] (Reset 벡터 읽기)
8. 모든 레지스터 초기화
```

**Reset 벡터**:
```asm
; ROM 끝에 벡터 테이블
.org $FFFC
.dw ResetHandler    ; $FFFC-FFFD

ResetHandler:
    SEI              ; IRQ 끔
    CLC
    XCE              ; Native 모드로 전환
    
    REP #$30         ; 16비트 모드
    LDA #$1FFF
    TCS              ; Stack = $1FFF
    
    LDX #$0000
    TXD              ; Direct Page = $0000
    
    ; 레지스터 초기화
    JSR InitPPU
    JSR InitAPU
    JSR InitCPU
    
    ; NMI 활성화
    LDA #$81
    STA $4200
    
    CLI              ; IRQ 활성화
    
    JMP MainLoop
```

---

## 인터럽트 우선순위

### 우선순위 (높음 → 낮음)

1. **Reset** (최우선)
2. **ABORT** (External, 거의 사용 안 함)
3. **NMI**
4. **IRQ** / **BRK**
5. **COP**

### 동시 발생 시

```
NMI와 IRQ가 동시 발생
→ NMI 먼저 처리
→ NMI 핸들러 종료 후 IRQ 처리 (I 플래그가 0이면)
```

---

## 인터럽트 처리 시퀀스

### 하드웨어 시퀀스

```
1. 현재 명령어 완료
2. PC + 1 → Stack (3바이트)
3. P (Status) → Stack (1바이트)
4. I 플래그 설정 (IRQ 비활성화)
5. D 플래그 클리어 (Decimal 끔)
6. PB (Program Bank) = $00
7. PC = [인터럽트 벡터]
8. 핸들러 실행
```

**총 사이클**: 7 사이클 (Native), 8 사이클 (Emulation)

### Stack 구조

**Native 모드** (65816):
```
[SP+4]: PBR (Program Bank)
[SP+3]: PCH (PC High)
[SP+2]: PCL (PC Low)
[SP+1]: P (Status)
```

**Emulation 모드** (6502):
```
[SP+2]: PCH
[SP+1]: PCL
[SP]:   P
```

### RTI (Return from Interrupt)

```asm
RTI    ; Stack → P, PC, PBR
```

**동작**:
```
1. Stack → P
2. Stack → PC (2바이트)
3. Stack → PBR (Native만)
4. 인터럽트 전 상태로 복귀
```

---

## 구현 가이드

### CPU 인터럽트 시스템

```cpp
class CPU {
private:
    bool nmiPending = false;
    bool irqPending = false;
    bool resetPending = false;
    
    bool nmiEnabled = false;
    bool irqEnabled = false;
    
public:
    void checkInterrupts() {
        // Reset 최우선
        if (resetPending) {
            handleReset();
            resetPending = false;
            return;
        }
        
        // NMI
        if (nmiPending) {
            handleNMI();
            nmiPending = false;
            return;
        }
        
        // IRQ (I 플래그 체크)
        if (irqPending && !I_flag) {
            handleIRQ();
            irqPending = false;
        }
    }
    
    void handleNMI() {
        // 현재 명령어 완료 대기
        finishCurrentInstruction();
        
        // Stack에 저장
        push24(PC);      // PC (3바이트, Native)
        push8(P);        // Status
        
        // 플래그 설정
        I_flag = true;   // IRQ 비활성화
        D_flag = false;  // Decimal 끔
        
        // NMI 벡터 읽기
        uint16_t vector = nativeMode ? 0xFFEA : 0xFFFA;
        PC = readWord(vector);
        PBR = 0x00;
        
        cycles += 7;  // NMI 오버헤드
    }
    
    void handleIRQ() {
        // NMI와 동일하지만 벡터가 다름
        finishCurrentInstruction();
        
        push24(PC);
        push8(P);
        
        I_flag = true;
        D_flag = false;
        
        uint16_t vector = nativeMode ? 0xFFEE : 0xFFFE;
        PC = readWord(vector);
        PBR = 0x00;
        
        cycles += 7;
    }
    
    void handleReset() {
        // Emulation 모드로
        nativeMode = false;
        E_flag = true;
        
        // 플래그 초기화
        I_flag = true;
        D_flag = false;
        
        // 레지스터 초기화
        SP = 0x01FF;
        D = 0x0000;
        DBR = 0x00;
        PBR = 0x00;
        
        // Reset 벡터
        PC = readWord(0xFFFC);
    }
};
```

### PPU NMI 생성

```cpp
class PPU {
private:
    bool nmiEnabled = false;
    bool nmiFlag = false;
    int scanline = 0;
    
public:
    void runScanline() {
        scanline++;
        
        // VBlank 시작 (스캔라인 225, NTSC)
        if (scanline == 225) {
            nmiFlag = true;
            
            if (nmiEnabled) {
                cpu.triggerNMI();
            }
        }
        
        // VBlank 끝 (스캔라인 262)
        if (scanline >= 262) {
            scanline = 0;
            nmiFlag = false;
        }
    }
    
    uint8_t readRDNMI() {
        uint8_t value = (nmiFlag ? 0x80 : 0) | 0x01;  // PPU Version
        nmiFlag = false;  // 읽으면 클리어
        return value;
    }
    
    void writeNMITIMEN(uint8_t value) {
        bool wasEnabled = nmiEnabled;
        nmiEnabled = (value & 0x80) != 0;
        
        // NMI가 활성화되고 이미 VBlank면 즉시 NMI
        if (!wasEnabled && nmiEnabled && nmiFlag) {
            cpu.triggerNMI();
        }
    }
};
```

### H/V-IRQ 타이머

```cpp
class Timer {
private:
    int hTimer = 0;
    int vTimer = 0;
    bool hIRQEnabled = false;
    bool vIRQEnabled = false;
    bool irqFlag = false;
    
public:
    void runCycle(int hPos, int vPos) {
        bool trigger = false;
        
        if (hIRQEnabled && vIRQEnabled) {
            // H/V 조합
            if (hPos == hTimer && vPos == vTimer) {
                trigger = true;
            }
        } else if (vIRQEnabled) {
            // V만
            if (vPos == vTimer && hPos == 0) {
                trigger = true;
            }
        } else if (hIRQEnabled) {
            // H만 (매 스캔라인)
            if (hPos == hTimer) {
                trigger = true;
            }
        }
        
        if (trigger && !irqFlag) {
            irqFlag = true;
            cpu.triggerIRQ();
        }
    }
    
    uint8_t readTIMEUP() {
        uint8_t value = (irqFlag ? 0x80 : 0);
        irqFlag = false;  // 읽으면 클리어
        return value;
    }
};
```

---

## 디버깅

### 인터럽트 로그

```cpp
void logInterrupt(const char* type) {
    printf("[Cycle %lld] %s: PC=%02X:%04X, P=%02X, SP=%04X\n",
           totalCycles, type, PBR, PC, P, SP);
}
```

### NMI 타이밍 검증

```cpp
void validateNMITiming() {
    if (scanline != 225) {
        printf("ERROR: NMI at wrong scanline: %d\n", scanline);
    }
}
```

---

## 일반적인 문제

### 1. NMI 플래그 클리어 안 함

```cpp
// 잘못된 예
void NMI_Handler() {
    // $4210 읽지 않음 → NMI 플래그가 남아있음 → 무한 NMI!
}

// 올바른 예
void NMI_Handler() {
    uint8_t status = read(0x4210);  // 반드시 읽기!
    // ...
}
```

### 2. RTI 없이 반환

```asm
; 잘못됨
NMI_Handler:
    ; ...
    RTS    ; ← 잘못됨! RTI를 써야 함

; 올바름
NMI_Handler:
    ; ...
    RTI    ; ← 올바름
```

### 3. IRQ와 I 플래그

```cpp
// IRQ가 동작 안 함?
// → I 플래그 확인!
if (I_flag) {
    // IRQ 마스킹됨
}
```

---

## 참고 자료

- [SnesLab - Interrupts](https://sneslab.net/wiki/Interrupts)
- [SnesLab - NMI](https://sneslab.net/wiki/NMI)
- [SnesLab - IRQ](https://sneslab.net/wiki/IRQ)
- [Fullsnes - Interrupts](https://problemkaputt.de/fullsnes.htm#cpuinterrupts)

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete
