# SNES S-DSP (Sound DSP) Registers - 완전 가이드

## 📋 목차
1. [개요](#개요)
2. [DSP 아키텍처](#dsp-아키텍처)
3. [레지스터 액세스](#레지스터-액세스)
4. [Voice 레지스터](#voice-레지스터)
5. [BRR 샘플 디코딩](#brr-샘플-디코딩)
6. [ADSR 엔벨로프](#adsr-엔벨로프)
7. [Echo/FIR 필터](#echofir-필터)
8. [글로벌 레지스터](#글로벌-레지스터)
9. [실시간 오디오 처리](#실시간-오디오-처리)
10. [구현 가이드](#구현-가이드)

---

## 개요

S-DSP (Sony Digital Signal Processor)는 SNES의 실제 소리를 생성하는 하드웨어입니다. SPC700 CPU가 명령을 내리면, DSP가 8개 음성(Voice)을 믹싱하여 스테레오 오디오를 출력합니다.

### 핵심 특징

| 특징 | 스펙 |
|------|------|
| **샘플레이트** | 32,000 Hz |
| **비트 깊이** | 16비트 (내부), 32kHz 출력 |
| **음성 수** | 8개 동시 재생 |
| **압축 포맷** | BRR (Bit Rate Reduction, 32:9 비율) |
| **출력** | 스테레오 (L/R 독립 볼륨) |
| **이펙트** | Echo, Pitch Modulation, Noise |

---

## DSP 아키텍처

### 전체 구조

```
ARAM (64KB)
  ├─ BRR 샘플 데이터
  ├─ Echo 버퍼
  └─ 디렉토리 테이블

       ↓

┌─────────────────────────────┐
│ S-DSP                        │
├─────────────────────────────┤
│ Voice 0-7 (각 음성 독립)     │
│  ├─ BRR 디코더              │
│  ├─ ADSR 엔벨로프           │
│  ├─ 피치 제어               │
│  └─ L/R 볼륨                │
├─────────────────────────────┤
│ 믹서                         │
│  ├─ 8 Voice 믹싱            │
│  ├─ Echo 적용               │
│  └─ FIR 필터 (8탭)          │
└─────────────────────────────┘
       ↓
  L/R 오디오 출력
```

### 처리 순서

```
1. BRR 디코딩
   ↓
2. 피치 조절 (리샘플링)
   ↓
3. ADSR 엔벨로프 적용
   ↓
4. 볼륨 조절 (L/R)
   ↓
5. 8개 Voice 믹싱
   ↓
6. Echo 효과
   ↓
7. FIR 필터
   ↓
8. 최종 출력
```

---

## 레지스터 액세스

### 레지스터 주소

DSP 레지스터는 **128개** ($00-$7F)이며, SPC700에서 **2단계 액세스**로 접근합니다.

```
$F2: DSPADDR - DSP 레지스터 주소 (쓰기 전용)
$F3: DSPDATA - DSP 레지스터 데이터 (읽기/쓰기)
```

### 액세스 방법

```asm
; DSP 레지스터 읽기 예제
MOV $F2, #$0C    ; DSPADDR = $0C (Voice 0 볼륨 L)
MOV A, $F3       ; DSPDATA 읽기 → A

; DSP 레지스터 쓰기 예제
MOV $F2, #$0C    ; DSPADDR = $0C
MOV $F3, #$7F    ; DSPDATA = $7F (최대 볼륨)
```

### C++ 구현

```cpp
class DSP {
private:
    uint8_t registers[128];   // DSP 레지스터 $00-$7F
    uint8_t dspAddr;          // 현재 선택된 레지스터
    
public:
    // SPC700에서 호출
    void writeDSPADDR(uint8_t value) {
        dspAddr = value & 0x7F;  // $00-$7F만 유효
    }
    
    void writeDSPDATA(uint8_t value) {
        if (dspAddr < 0x80) {
            registers[dspAddr] = value;
            onRegisterWrite(dspAddr, value);  // 레지스터별 처리
        }
    }
    
    uint8_t readDSPDATA() {
        if (dspAddr < 0x80) {
            return registers[dspAddr];
        }
        return 0;
    }
};
```

---

## Voice 레지스터

각 Voice(0-7)는 **16개 레지스터**를 가집니다. Voice N의 레지스터는 `$N0-$NF` 주소에 있습니다.

### Voice 레지스터 맵

| 레지스터 | 이름 | 설명 | 범위 |
|----------|------|------|------|
| **$x0** | VxVOLL | Left Volume | -128 ~ +127 (부호 있음) |
| **$x1** | VxVOLR | Right Volume | -128 ~ +127 (부호 있음) |
| **$x2** | VxPITCHL | Pitch (Low) | 0-255 |
| **$x3** | VxPITCHH | Pitch (High) | 0-63 (14비트 총) |
| **$x4** | VxSRCN | Sample Source Number | 0-255 (디렉토리 인덱스) |
| **$x5** | VxADSR1 | ADSR 설정 (Attack/Decay) | Bit 단위 설정 |
| **$x6** | VxADSR2 | ADSR 설정 (Sustain/Release) | Bit 단위 설정 |
| **$x7** | VxGAIN | Gain (ADSR 대체 모드) | 0-127 |
| **$x8** | VxENVX | 현재 엔벨로프 값 (읽기 전용) | 0-127 |
| **$x9** | VxOUTX | 현재 출력 값 (읽기 전용) | -128 ~ +127 |
| **$xA-F** | - | 사용 안 함 | - |

**주의**: `x`는 Voice 번호 (0-7)

### 예: Voice 0 레지스터
```
$00: V0VOLL  (Voice 0 Left Volume)
$01: V0VOLR  (Voice 0 Right Volume)
$02: V0PITCHL
$03: V0PITCHH
$04: V0SRCN
$05: V0ADSR1
$06: V0ADSR2
$07: V0GAIN
$08: V0ENVX  (읽기 전용)
$09: V0OUTX  (읽기 전용)
```

---

### VxVOLL / VxVOLR ($x0 / $x1) - 볼륨

**범위**: -128 ~ +127 (8비트 부호 있는 정수)

```
Bit 7: 부호 (0=양수, 1=음수)
Bit 6-0: 절댓값

음수 볼륨 = 위상 반전 (Phase Inversion)
```

**예제**:
```asm
; Voice 0을 중앙에 배치 (양쪽 동일 볼륨)
MOV $F2, #$00    ; V0VOLL
MOV $F3, #$7F    ; +127 (최대)
MOV $F2, #$01    ; V0VOLR
MOV $F3, #$7F

; Voice 1을 완전히 왼쪽에
MOV $F2, #$10    ; V1VOLL
MOV $F3, #$7F
MOV $F2, #$11    ; V1VOLR
MOV $F3, #$00    ; 오른쪽 끔
```

**C++ 구현**:
```cpp
struct Voice {
    int8_t volumeL;  // -128 ~ +127
    int8_t volumeR;
    
    void setVolume(uint8_t rawL, uint8_t rawR) {
        volumeL = (int8_t)rawL;  // 부호 있는 정수로 해석
        volumeR = (int8_t)rawR;
    }
    
    // 샘플에 볼륨 적용
    int16_t applyVolume(int16_t sample, int8_t volume) {
        return (sample * volume) >> 7;  // 7비트로 정규화
    }
};
```

---

### VxPITCHL / VxPITCHH ($x2 / $x3) - 피치

**범위**: 0-16383 (14비트 고정소수점)

```
Pitch = PITCHL | (PITCHH << 8)

실제 주파수 = 32000 Hz × (Pitch / 4096)

예:
- Pitch = 4096 (0x1000) → 32kHz (원본 속도)
- Pitch = 8192 (0x2000) → 64kHz (2배속, 1옥타브 위)
- Pitch = 2048 (0x0800) → 16kHz (0.5배속, 1옥타브 아래)
```

**음계 계산**:
```cpp
// MIDI 노트 번호 → Pitch 값
uint16_t midiNoteToPitch(int midiNote, int baseMidiNote = 60) {
    // 중앙 C (MIDI 60) = 4096
    // 반음 = 2^(1/12) ≈ 1.05946
    double semitones = midiNote - baseMidiNote;
    double ratio = pow(2.0, semitones / 12.0);
    return (uint16_t)(4096.0 * ratio);
}

// 예:
// MIDI 60 (C4) → 4096
// MIDI 72 (C5) → 8192 (1옥타브 위)
// MIDI 48 (C3) → 2048 (1옥타브 아래)
```

**예제 (A440 재생)**:
```asm
; A440 (MIDI 69) 재생
; Pitch = 4096 × 2^(9/12) ≈ 6875 (0x1ADB)
MOV $F2, #$02    ; V0PITCHL
MOV $F3, #$DB
MOV $F2, #$03    ; V0PITCHH
MOV $F3, #$1A
```

---

### VxSRCN ($x4) - Sample Source Number

**범위**: 0-255

```
샘플 디렉토리 인덱스:
ARAM 주소 = DIR_BASE + (SRCN × 4)

디렉토리 엔트리 (4바이트):
  [0-1]: 샘플 시작 주소 (ARAM)
  [2-3]: 루프 포인트 주소 (ARAM)
```

**디렉토리 테이블 설정** (글로벌):
```
$5D (DIR): 디렉토리 테이블 시작 주소 / 256

실제 주소 = DIR × 256

예: DIR = $02 → 디렉토리는 $0200에 위치
```

**예제**:
```asm
; 디렉토리 테이블 설정 ($0200에 위치)
MOV $F2, #$5D    ; DIR 레지스터
MOV $F3, #$02    ; $0200

; ARAM $0200-$0203에 샘플 0 정보 쓰기:
;   [0-1]: $1000 (샘플 시작)
;   [2-3]: $1100 (루프 포인트)
; (SPC700 코드로 ARAM 쓰기)

; Voice 0에 샘플 0 재생
MOV $F2, #$04    ; V0SRCN
MOV $F3, #$00    ; 샘플 번호 0
```

---

### VxADSR1 / VxADSR2 ($x5 / $x6) - ADSR 엔벨로프

**ADSR**: Attack, Decay, Sustain, Release

```
VxADSR1 ($x5):
Bit 7: ADSR Enable (1=ADSR, 0=GAIN)
Bit 6-4: Decay Rate (0-7)
Bit 3-0: Attack Rate (0-15)

VxADSR2 ($x6):
Bit 7: Sustain Level (0-7, 실제 값 = (SL+1) × 8)
Bit 4-0: Sustain/Release Rate (0-31)
```

**ADSR 단계**:
```
1. Attack: 0 → 최대 (빠르게 상승)
   ↓
2. Decay: 최대 → Sustain Level (감소)
   ↓
3. Sustain: Sustain Level 유지
   ↓
4. Release: Sustain → 0 (키 오프 후)
```

**Rate → 시간 변환**:
```cpp
// Rate 값 → 샘플당 변화량
static const int attackRates[16] = {
    // Rate 0-15의 증가 속도 (임의 값, 실제는 복잡함)
    4096, 3072, 2560, 2048, 1536, 1280, 1024, 768,
    640, 512, 384, 320, 256, 192, 160, 128
};

static const int decayRates[8] = {
    1200, 740, 440, 290, 180, 110, 74, 37
};

// Sustain Level
int sustainLevel = (ADSR2 >> 5) * 8;  // 0, 8, 16, ..., 56
```

**예제 (피아노 ADSR)**:
```asm
; 빠른 Attack, 중간 Decay, 낮은 Sustain, 중간 Release
MOV $F2, #$05    ; V0ADSR1
MOV $F3, #$EF    ; Bit7=1 (ADSR), Attack=15, Decay=6
MOV $F2, #$06    ; V0ADSR2
MOV $F3, #$E5    ; SustainLevel=7, Rate=5
```

---

### VxGAIN ($x7) - Gain (대체 엔벨로프)

**범위**: 0-127 (ADSR1 Bit 7 = 0일 때만 사용)

```
GAIN 모드 (ADSR 대신):
Bit 7: 0 (Direct), 1 (Custom)

Direct Mode (Bit 7 = 0):
  Bit 6-0: 고정 볼륨 (0-127)

Custom Mode (Bit 7 = 1):
  Bit 6-5: 모드
    00 = Linear Decrease
    01 = Exponential Decrease
    10 = Linear Increase
    11 = Bent Increase
  Bit 4-0: Rate (0-31)
```

**예제 (고정 볼륨)**:
```asm
; GAIN 모드로 고정 볼륨 50% 재생
MOV $F2, #$05    ; V0ADSR1
MOV $F3, #$00    ; ADSR 끔 (Bit 7 = 0)
MOV $F2, #$07    ; V0GAIN
MOV $F3, #$40    ; 고정 볼륨 64 (50%)
```

---

### VxENVX / VxOUTX ($x8 / $x9) - 상태 레지스터 (읽기 전용)

**VxENVX** ($x8): 현재 엔벨로프 값 (0-127)
```cpp
// 노트가 재생 중인지 확인
uint8_t envx = readDSP(0x08);  // Voice 0
bool isPlaying = (envx > 0);
```

**VxOUTX** ($x9): 현재 출력 샘플 값 (-128 ~ +127)
```cpp
// Voice 파형 모니터링 (오실로스코프)
int8_t outx = (int8_t)readDSP(0x09);  // Voice 0
```

---

## BRR 샘플 디코딩

### BRR 포맷

**BRR** (Bit Rate Reduction): SNES 전용 4비트 ADPCM 압축

```
압축률: 32:9 (9바이트 = 16샘플)

블록 구조 (9바이트):
[0]: Header (1바이트)
  Bit 7-4: Shift (0-12)
  Bit 3-2: Filter (0-3)
  Bit 1: Loop (1=루프 포인트)
  Bit 0: End (1=샘플 끝)
[1-8]: Data (8바이트 = 16 × 4비트 샘플)
```

### Header 비트

```
Bit 7-4: Shift (왼쪽 시프트 양)
  값 > 12: 무음 처리

Bit 3-2: Filter (예측 필터)
  0: 필터 없음
  1: P1 = s[-1]
  2: P2 = 2×s[-1] - s[-2]
  3: P3 = (17×s[-1] - 15×s[-2]) / 8

Bit 1: Loop Flag (루프 포인트 마크)
Bit 0: End Flag (블록 끝)
```

### 디코딩 알고리즘

```cpp
class BRRDecoder {
private:
    int16_t p1 = 0;  // 이전 샘플
    int16_t p2 = 0;  // 이전의 이전 샘플
    
public:
    void decodeBlock(const uint8_t* brrBlock, int16_t output[16]) {
        uint8_t header = brrBlock[0];
        int shift = header >> 4;
        int filter = (header >> 2) & 3;
        
        for (int i = 0; i < 16; i++) {
            // 4비트 샘플 추출
            int byteIdx = 1 + (i / 2);
            int nibbleShift = (i & 1) ? 0 : 4;
            int8_t nibble = (brrBlock[byteIdx] >> nibbleShift) & 0x0F;
            
            // 부호 확장 (4비트 → 16비트)
            if (nibble > 7) nibble -= 16;
            
            // 시프트
            int32_t sample;
            if (shift <= 12) {
                sample = (nibble << shift) >> 1;
            } else {
                sample = (nibble < 0) ? -2048 : 0;  // 무음
            }
            
            // 필터 적용
            switch (filter) {
                case 0:  // None
                    break;
                case 1:  // P1
                    sample += p1 + (-p1 >> 4);
                    break;
                case 2:  // P2
                    sample += (p1 * 2) + ((-p1 * 3) >> 5) - p2 + (p2 >> 4);
                    break;
                case 3:  // P3
                    sample += (p1 * 2) + ((-(p1 + (p1 >> 1))) >> 4) - p2 + ((p2 * 3) >> 4);
                    break;
            }
            
            // 클램핑 (-32768 ~ +32767)
            if (sample > 32767) sample = 32767;
            if (sample < -32768) sample = -32768;
            
            // 출력 및 히스토리 업데이트
            output[i] = (int16_t)sample;
            p2 = p1;
            p1 = (int16_t)sample;
        }
    }
};
```

### 루프 처리

```cpp
struct BRRSample {
    uint16_t startAddr;  // ARAM 시작 주소
    uint16_t loopAddr;   // ARAM 루프 주소
    bool isPlaying;
    uint16_t currentAddr;
    
    void play() {
        currentAddr = startAddr;
        isPlaying = true;
    }
    
    bool processBlock(const uint8_t* aram, int16_t output[16]) {
        const uint8_t* block = &aram[currentAddr];
        uint8_t header = block[0];
        
        // 디코딩
        decoder.decodeBlock(block, output);
        
        // 다음 블록
        currentAddr += 9;
        
        // End 플래그 체크
        if (header & 0x01) {  // End
            if (header & 0x02) {  // Loop
                currentAddr = loopAddr;
                return true;  // 계속 재생
            } else {
                isPlaying = false;
                return false;  // 재생 종료
            }
        }
        
        return true;
    }
};
```

---

## ADSR 엔벨로프

### 엔벨로프 구현

```cpp
class ADSREnvelope {
private:
    enum State { ATTACK, DECAY, SUSTAIN, RELEASE, IDLE };
    
    State state = IDLE;
    int currentLevel = 0;  // 0-32767
    
    int attackRate;
    int decayRate;
    int sustainLevel;
    int releaseRate;
    
public:
    void setADSR(uint8_t adsr1, uint8_t adsr2) {
        bool adsrEnabled = adsr1 & 0x80;
        if (!adsrEnabled) return;  // GAIN 모드
        
        attackRate = attackRates[adsr1 & 0x0F];
        decayRate = decayRates[(adsr1 >> 4) & 0x07];
        sustainLevel = ((adsr2 >> 5) + 1) * 1024;  // 0-8192
        releaseRate = sustainRates[adsr2 & 0x1F];
    }
    
    void keyOn() {
        state = ATTACK;
        currentLevel = 0;
    }
    
    void keyOff() {
        state = RELEASE;
    }
    
    // 매 샘플마다 호출
    uint16_t process() {
        switch (state) {
            case ATTACK:
                currentLevel += attackRate;
                if (currentLevel >= 32767) {
                    currentLevel = 32767;
                    state = DECAY;
                }
                break;
                
            case DECAY:
                currentLevel -= decayRate;
                if (currentLevel <= sustainLevel) {
                    currentLevel = sustainLevel;
                    state = SUSTAIN;
                }
                break;
                
            case SUSTAIN:
                // 레벨 유지 (실제로는 약간 감소할 수 있음)
                break;
                
            case RELEASE:
                currentLevel -= releaseRate;
                if (currentLevel <= 0) {
                    currentLevel = 0;
                    state = IDLE;
                }
                break;
                
            case IDLE:
                currentLevel = 0;
                break;
        }
        
        return currentLevel >> 8;  // 0-127 반환
    }
};
```

---

## Echo/FIR 필터

### Echo 시스템

Echo는 **지연 버퍼**를 사용한 리버브 효과입니다.

**레지스터**:
```
$0D (EFB): Echo Feedback (-128 ~ +127)
$2D (PMON): Pitch Modulation Enable
$3D (NON): Noise Enable
$4D (EON): Echo Enable (Voice별, 8비트)
$5D (DIR): Sample Directory Address
$6D (ESA): Echo Start Address / 256
$7D (EDL): Echo Delay Length (0-15, 실제 = EDL × 2KB)
```

### FIR 필터 계수

**8탭 FIR 필터** (레지스터 $0F-$7F, 16바이트 간격):
```
$0F: FIR0  (-128 ~ +127)
$1F: FIR1
$2F: FIR2
$3F: FIR3
$4F: FIR4
$5F: FIR5
$6F: FIR6
$7F: FIR7
```

### Echo 처리 알고리즘

```cpp
class EchoProcessor {
private:
    int16_t buffer[32768];  // Echo 버퍼 (최대 30KB)
    int bufferSize;
    int writePos = 0;
    int8_t firCoeffs[8];
    int8_t feedback;
    
public:
    void setEcho(uint8_t esa, uint8_t edl, int8_t efb) {
        int echoStartAddr = esa * 256;
        bufferSize = edl * 2048 / 2;  // 샘플 단위
        feedback = efb;
    }
    
    void setFIR(int tap, int8_t coeff) {
        if (tap < 8) firCoeffs[tap] = coeff;
    }
    
    int16_t process(int16_t input) {
        // FIR 필터 (8탭)
        int32_t filtered = 0;
        for (int i = 0; i < 8; i++) {
            int readPos = (writePos - i) % bufferSize;
            if (readPos < 0) readPos += bufferSize;
            filtered += buffer[readPos] * firCoeffs[i];
        }
        filtered >>= 7;  // 정규화
        
        // Feedback
        int32_t echo = filtered + ((input * feedback) >> 7);
        
        // 클램핑
        if (echo > 32767) echo = 32767;
        if (echo < -32768) echo = -32768;
        
        // 버퍼에 쓰기
        buffer[writePos] = (int16_t)echo;
        writePos = (writePos + 1) % bufferSize;
        
        return (int16_t)filtered;
    }
};
```

### 예제 (간단한 Echo 설정)

```asm
; Echo 버퍼를 ARAM $E000에 배치, 길이 = 4KB
MOV $F2, #$6D    ; ESA
MOV $F3, #$E0    ; $E000 / 256

MOV $F2, #$7D    ; EDL
MOV $F3, #$02    ; 2 × 2KB = 4KB

MOV $F2, #$0D    ; EFB (Feedback)
MOV $F3, #$40    ; 약한 피드백

; FIR 계수 (간단한 저역 통과 필터)
MOV $F2, #$0F    ; FIR0
MOV $F3, #$7F
MOV $F2, #$1F    ; FIR1
MOV $F3, #$00
; ... (나머지는 0)

; Voice 0에 Echo 활성화
MOV $F2, #$4D    ; EON
MOV $F3, #$01    ; Bit 0 = Voice 0
```

---

## 글로벌 레지스터

### 믹서 볼륨

```
$0C (MVOLL): Main Volume Left (-128 ~ +127)
$1C (MVOLR): Main Volume Right (-128 ~ +127)
$2C (EVOLL): Echo Volume Left (-128 ~ +127)
$3C (EVOLR): Echo Volume Right (-128 ~ +127)
```

### 키 제어

```
$4C (KON): Key On (8비트, 각 비트 = Voice 0-7)
  1을 쓰면 해당 Voice 재생 시작

$5C (KOFF): Key Off (8비트)
  1을 쓰면 해당 Voice Release 단계로
```

**예제**:
```asm
; Voice 0, 1, 2 동시에 재생
MOV $F2, #$4C    ; KON
MOV $F3, #$07    ; 0b00000111 (Voice 0, 1, 2)

; Voice 1 끄기
MOV $F2, #$5C    ; KOFF
MOV $F3, #$02    ; 0b00000010 (Voice 1)
```

### 솔로/뮤트

```
$2D (PMON): Pitch Modulation Enable (Voice 1-7)
  Bit N = 1: Voice N의 피치를 Voice N-1의 출력으로 변조

$3D (NON): Noise Enable (Voice 0-7)
  Bit N = 1: Voice N은 BRR 대신 노이즈 생성

$5D (DIR): Sample Directory Page ($00-$FF, 실제 주소 = DIR × 256)

$6C (FLG): DSP Flags
  Bit 7: Soft Reset (모든 Voice 끔)
  Bit 6: Mute (오디오 출력 끔)
  Bit 4-0: Noise Frequency (0-31)
```

---

## 실시간 오디오 처리

### DSP 메인 루프

```cpp
class SNESDSP {
private:
    Voice voices[8];
    EchoProcessor echo;
    int sampleRate = 32000;
    
public:
    // 매 샘플마다 호출 (32kHz)
    void generateSample(int16_t& left, int16_t& right) {
        int32_t mixL = 0, mixR = 0;
        
        // 1. 각 Voice 처리
        for (int v = 0; v < 8; v++) {
            if (!voices[v].isPlaying) continue;
            
            // BRR 디코딩 및 리샘플링
            int16_t sample = voices[v].getSample();
            
            // ADSR 엔벨로프 적용
            uint8_t env = voices[v].envelope.process();
            sample = (sample * env) >> 7;
            
            // 볼륨 적용
            int16_t outL = (sample * voices[v].volumeL) >> 7;
            int16_t outR = (sample * voices[v].volumeR) >> 7;
            
            // 믹싱
            mixL += outL;
            mixR += outR;
        }
        
        // 2. 메인 볼륨
        mixL = (mixL * mainVolumeL) >> 7;
        mixR = (mixR * mainVolumeR) >> 7;
        
        // 3. Echo 처리
        int16_t echoL = echo.process(mixL);
        int16_t echoR = echo.process(mixR);
        
        // 4. Echo 믹싱
        mixL += (echoL * echoVolumeL) >> 7;
        mixR += (echoR * echoVolumeR) >> 7;
        
        // 5. 클램핑
        left = clamp(mixL, -32768, 32767);
        right = clamp(mixR, -32768, 32767);
    }
    
    // SDL2 오디오 콜백
    static void audioCallback(void* userdata, uint8_t* stream, int len) {
        SNESDSP* dsp = (SNESDSP*)userdata;
        int16_t* buffer = (int16_t*)stream;
        int samples = len / 4;  // 스테레오 16비트
        
        for (int i = 0; i < samples; i++) {
            int16_t left, right;
            dsp->generateSample(left, right);
            buffer[i * 2 + 0] = left;
            buffer[i * 2 + 1] = right;
        }
    }
};
```

### 리샘플링 (Pitch 처리)

```cpp
class VoiceResampler {
private:
    uint32_t position = 0;  // 16.16 고정소수점
    int16_t samples[16];    // 현재 BRR 블록
    int currentSample = 0;
    
public:
    int16_t getSample(uint16_t pitch) {
        // pitch = 4096 → 원본 속도
        // pitch = 8192 → 2배속
        
        // 현재 위치의 샘플 (선형 보간)
        int idx = position >> 16;
        int frac = position & 0xFFFF;
        
        int16_t s0 = samples[idx];
        int16_t s1 = samples[idx + 1];
        int16_t interpolated = s0 + (((s1 - s0) * frac) >> 16);
        
        // 위치 업데이트
        position += pitch;
        
        // 블록 끝나면 다음 BRR 블록 디코딩
        if ((position >> 16) >= 16) {
            position -= (16 << 16);
            decodeNextBRRBlock();
        }
        
        return interpolated;
    }
};
```

---

## 구현 가이드

### 전체 구조

```cpp
class APU {
public:
    SPC700 cpu;      // SPC700 CPU
    SNESDSP dsp;     // S-DSP
    uint8_t aram[65536];  // 64KB Audio RAM
    
    // CPU → APU 포트 ($2140-$2143)
    uint8_t cpuPorts[4];
    uint8_t spcPorts[4];
    
    void run(int cycles) {
        // SPC700 실행
        cpu.run(cycles);
        
        // DSP 샘플 생성 (타이밍 계산 필요)
        // 32kHz = 21.47 master clocks/sample
    }
    
    void writeCPUPort(int port, uint8_t value) {
        cpuPorts[port] = value;
        // SPC700이 $F4-$F7에서 읽을 수 있음
    }
    
    uint8_t readCPUPort(int port) {
        return spcPorts[port];
        // SPC700이 $F4-$F7에 쓴 값
    }
};
```

### 초기화

```cpp
void initDSP() {
    // 모든 Voice 끄기
    writeDSP(0x6C, 0x80);  // FLG: Soft Reset
    
    // 메인 볼륨 설정
    writeDSP(0x0C, 0x7F);  // MVOLL
    writeDSP(0x1C, 0x7F);  // MVOLR
    
    // Echo 끄기
    writeDSP(0x4D, 0x00);  // EON
    writeDSP(0x0D, 0x00);  // EFB
    
    // 샘플 디렉토리 설정
    writeDSP(0x5D, 0x02);  // DIR = $0200
}
```

### 간단한 재생 예제

```cpp
void playSound(int voice, int sampleNum, int pitch, int volumeL, int volumeR) {
    int base = voice * 16;
    
    // Voice 설정
    writeDSP(base + 0x04, sampleNum);  // SRCN
    writeDSP(base + 0x02, pitch & 0xFF);  // PITCHL
    writeDSP(base + 0x03, pitch >> 8);    // PITCHH
    writeDSP(base + 0x00, volumeL);    // VOLL
    writeDSP(base + 0x01, volumeR);    // VOLR
    
    // ADSR 설정
    writeDSP(base + 0x05, 0x8F);  // Fast attack
    writeDSP(base + 0x06, 0xE0);  // Slow release
    
    // Key On
    writeDSP(0x4C, 1 << voice);
}
```

---

## 디버깅

### DSP 상태 모니터

```cpp
void debugDSP() {
    printf("=== DSP Status ===\n");
    
    for (int v = 0; v < 8; v++) {
        int base = v * 16;
        uint8_t envx = readDSP(base + 0x08);
        int8_t outx = (int8_t)readDSP(base + 0x09);
        
        printf("Voice %d: ENVX=%3d, OUTX=%4d", v, envx, outx);
        
        if (envx > 0) {
            uint16_t pitch = readDSP(base + 0x02) | (readDSP(base + 0x03) << 8);
            uint8_t srcn = readDSP(base + 0x04);
            printf(" [PLAYING] Pitch=%d, Sample=%d", pitch, srcn);
        }
        printf("\n");
    }
}
```

### BRR 디코더 테스트

```cpp
void testBRRDecoder() {
    // 간단한 BRR 블록 (440Hz 사인파)
    uint8_t testBlock[9] = {
        0x00,  // Header: Shift=0, Filter=0
        0x00, 0x12, 0x34, 0x56, 0x76, 0x54, 0x32, 0x10
    };
    
    int16_t output[16];
    decoder.decodeBlock(testBlock, output);
    
    for (int i = 0; i < 16; i++) {
        printf("Sample %2d: %6d\n", i, output[i]);
    }
}
```

---

## 최적화

### 1. 고정소수점 연산
```cpp
// 부동소수점 대신 고정소수점 사용
// 16.16 형식: 상위 16비트 = 정수, 하위 16비트 = 소수
typedef int32_t fixed16_t;
#define FIXED16_ONE (1 << 16)

fixed16_t multiply(fixed16_t a, fixed16_t b) {
    return (int32_t)(((int64_t)a * b) >> 16);
}
```

### 2. Lookup Table
```cpp
// 자주 사용되는 계산은 테이블로
static int16_t pitchTable[16384];  // Pitch → 리샘플 비율
static int8_t volumeTable[128][128];  // 볼륨 곱셈
```

### 3. SIMD (SSE/AVX)
```cpp
// 8개 Voice를 병렬로 처리
void processsVoicesSSE(__m128i* samples, __m128i* volumes) {
    // SSE 명령어로 8샘플 동시 처리
}
```

---

## 참고 자료

- [SnesLab - DSP Registers](https://sneslab.net/wiki/DSP_registers)
- [SNESdev - BRR Samples](https://snes.nesdev.org/wiki/BRR_samples)
- [Fullsnes - APU](https://problemkaputt.de/fullsnes.htm#snesapuaudioprocessingunit)
- [bsnes Source](https://github.com/bsnes-emu/bsnes) - `sfc/dsp/` 디렉토리
- [Anomie's SNES Doc](https://www.romhacking.net/documents/196/) - Section 3.9

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete
