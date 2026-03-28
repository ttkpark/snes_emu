# SPC700 IPL ROM - Boot Logic & Handshaking Protocol

**작성일**: 2025-12-14  
**목적**: APU 부팅 시 IPL ROM의 동작 원리와 구현 방법  
**상태**: Critical - 이것이 없으면 사운드 프로그램 로드 불가

---

## 📋 개요

SNES의 APU(SPC700)는 전원이 켜지면 **내장 64바이트 IPL ROM**을 실행합니다. 이 펌웨어는 메인 CPU로부터 프로그램을 다운로드하는 **부트로더(Bootloader)** 역할을 합니다.

### IPL ROM의 위치

```
주소 범위: 0xFFC0 - 0xFFFF (64 bytes)
초기 PC:   0xFFC0 (리셋 벡터)
```

---

## 🔄 IPL 프로토콜 (Handshaking)

### 전체 흐름

```
┌─────────────────────────────────────────────────────────────┐
│  Step 1: 초기화 & Ready 신호                                │
├─────────────────────────────────────────────────────────────┤
│  SPC: Port 0 = 0xAA, Port 1 = 0xBB (Ready 신호)           │
│  CPU: Port 0이 0xAA인지 확인 (SPC 준비 대기)               │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 2: 전송 시작 신호                                      │
├─────────────────────────────────────────────────────────────┤
│  CPU: Port 0 = 0xCC (대기 신호)                            │
│  CPU: Port 1 = 0x01 (시작 신호)                            │
│  SPC: Port 0이 0xCC인지 확인                                │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 3: 주소 설정                                           │
├─────────────────────────────────────────────────────────────┤
│  CPU: Port 2 = 주소 Low Byte                                │
│  CPU: Port 3 = 주소 High Byte                               │
│  SPC: 목적지 주소 = (Port 3 << 8) | Port 2                 │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 4: 데이터 전송 (반복)                                  │
├─────────────────────────────────────────────────────────────┤
│  CPU: Port 1 = 데이터 바이트                                │
│  CPU: Port 0 = Port 0 + 1 (카운터 증가)                    │
│  SPC: Port 0이 변경될 때까지 대기                           │
│  SPC: Port 1에서 데이터 읽기 → RAM에 저장                  │
│  SPC: Port 0 = Port 0 + 1 (ACK)                             │
│  ... 반복 ...                                                │
└─────────────────────────────────────────────────────────────┘
                              ↓
┌─────────────────────────────────────────────────────────────┐
│  Step 5: 전송 완료 & 실행 ⭐                                │
├─────────────────────────────────────────────────────────────┤
│  CPU: Port 3 = 0x00 (전송 완료 신호) ⭐⭐⭐                 │
│  CPU: Port 0 = 실행 주소 Low                                │
│  CPU: Port 1 = 실행 주소 High                               │
│  SPC: IPL ROM 비활성화 ⭐⭐⭐                                │
│  SPC: PC = (Port 1 << 8) | Port 0                          │
│  SPC: 프로그램 실행 시작! 🎉                                │
└─────────────────────────────────────────────────────────────┘
```

---

## 💻 에뮬레이터 구현

### 방법 1: HLE (High-Level Emulation) - 추천 ⭐

IPL ROM의 **실제 동작**을 시뮬레이션합니다. 가장 빠르고 정확합니다.

```cpp
// src/apu/apu.cpp

void APU::reset() {
    // SPC700 레지스터 초기화
    m_regs.a = 0x00;
    m_regs.x = 0x00;
    m_regs.y = 0x00;
    m_regs.sp = 0xEF;
    m_regs.pc = 0xFFC0;  // IPL ROM 시작
    m_regs.psw = 0x02;
    
    // IPL ROM 활성화
    m_iplromEnable = true;
    
    // ARAM 초기화 (0으로 클리어)
    std::fill(m_aram.begin(), m_aram.end(), 0x00);
    
    // 포트 초기화 - Ready 신호
    m_aram[0xF4] = 0xAA;  // Port 0 → CPU
    m_aram[0xF5] = 0xBB;  // Port 1 → CPU
    m_aram[0xF6] = 0x00;  // Port 2
    m_aram[0xF7] = 0x00;  // Port 3
    
    m_cpuPorts[0] = 0x00;  // CPU → Port 0
    m_cpuPorts[1] = 0x00;  // CPU → Port 1
    m_cpuPorts[2] = 0x00;  // CPU → Port 2
    m_cpuPorts[3] = 0x00;  // CPU → Port 3
    
    // 로드 상태 초기화
    m_spcLoadState = SPC_LOAD_IDLE;
    m_spcLoadAddr = 0x0000;
    m_spcLoadSize = 0x0000;
    m_spcLoadIndex = 0x0000;
    m_spcExecAddr = 0x0000;
    
    m_bootComplete = false;
    m_spc700Cycles = 0;
    
    printf("APU: Reset complete - IPL ROM enabled, PC=0x%04X\n", m_regs.pc);
}

void APU::writePort(uint8_t port, uint8_t value) {
    uint8_t old_value = m_cpuPorts[port];
    m_cpuPorts[port] = value;
    
    printf("APU: CPU wrote port %d = 0x%02X (old=0x%02X, loadState=%d, SPC700 PC=0x%04X)\n",
           port, value, old_value, m_spcLoadState, m_regs.pc);
    
    // IPL 프로토콜 처리
    switch (m_spcLoadState) {
        case SPC_LOAD_IDLE:
            // Step 2: 전송 시작 신호 감지
            if (port == 0 && value == 0xCC) {
                printf("APU: Transfer start signal (0xCC) detected\n");
                m_spcLoadState = SPC_LOAD_WAIT_BBAA;
            }
            break;
            
        case SPC_LOAD_WAIT_BBAA:
            // Step 2-2: Port 1이 0x01인지 확인
            if (port == 1 && value == 0x01) {
                printf("APU: Start signal confirmed (Port 1 = 0x01)\n");
                m_spcLoadState = SPC_LOAD_WAIT_CC;
            }
            break;
            
        case SPC_LOAD_WAIT_CC:
            // Step 3: 주소 설정 대기
            if (port == 2) {
                m_spcLoadAddr = (m_spcLoadAddr & 0xFF00) | value;
                printf("APU: Load address low = 0x%02X (addr=0x%04X)\n", 
                       value, m_spcLoadAddr);
            }
            else if (port == 3) {
                m_spcLoadAddr = (m_spcLoadAddr & 0x00FF) | (value << 8);
                printf("APU: Load address high = 0x%02X (addr=0x%04X)\n", 
                       value, m_spcLoadAddr);
                
                // 주소 설정 완료, 데이터 수신 시작
                m_spcLoadState = SPC_LOAD_RECEIVING;
                m_spcLoadIndex = 0;
                printf("APU: Ready to receive data at 0x%04X\n", m_spcLoadAddr);
            }
            break;
            
        case SPC_LOAD_RECEIVING:
            // Step 4: 데이터 전송
            if (port == 1) {
                // Port 1에 데이터가 쓰임 → RAM에 저장
                uint16_t dest_addr = m_spcLoadAddr + m_spcLoadIndex;
                if (dest_addr < 0xFFC0) {  // IPL ROM 영역은 제외
                    m_aram[dest_addr] = value;
                    m_spcLoadIndex++;
                    
                    if (m_spcLoadIndex % 256 == 0) {
                        printf("APU: Received %d bytes\n", m_spcLoadIndex);
                    }
                }
                
                // SPC가 ACK를 보낸 것처럼 Port 0 업데이트
                m_aram[0xF4] = m_cpuPorts[0];
            }
            else if (port == 0) {
                // Port 0 카운터 업데이트
                m_aram[0xF4] = value;
            }
            else if (port == 3 && value == 0x00) {
                // ⭐⭐⭐ Step 5: 전송 완료 신호!
                printf("APU: Transfer complete (Port 3 = 0x00), received %d bytes\n", 
                       m_spcLoadIndex);
                m_spcLoadState = SPC_LOAD_WAIT_EXEC;
                m_spcLoadSize = m_spcLoadIndex;
            }
            break;
            
        case SPC_LOAD_WAIT_EXEC:
            // ⭐⭐⭐ Step 5-2: 실행 주소 수신
            if (port == 0) {
                m_spcExecAddr = (m_spcExecAddr & 0xFF00) | value;
                printf("APU: Exec address low = 0x%02X\n", value);
            }
            else if (port == 1) {
                m_spcExecAddr = (m_spcExecAddr & 0x00FF) | (value << 8);
                printf("APU: Exec address high = 0x%02X (exec=0x%04X)\n", 
                       value, m_spcExecAddr);
                
                // ⭐⭐⭐ IPL ROM 비활성화 & 프로그램 점프!
                m_iplromEnable = false;
                m_regs.pc = m_spcExecAddr;
                m_spcLoadState = SPC_LOAD_COMPLETE;
                m_bootComplete = true;
                
                printf("========================================\n");
                printf("APU: IPL BOOT COMPLETE!\n");
                printf("  Loaded %d bytes to 0x%04X\n", m_spcLoadSize, m_spcLoadAddr);
                printf("  Jumping to 0x%04X\n", m_spcExecAddr);
                printf("  IPL ROM disabled\n");
                printf("========================================\n");
            }
            break;
            
        case SPC_LOAD_COMPLETE:
            // 프로그램 실행 중 - 일반 포트 통신
            // Port 2는 보통 테스트 번호로 사용
            if (port == 2) {
                printf("APU: SPC700 wrote port 2 = 0x%02X (test number?)\n", value);
            }
            break;
    }
}

uint8_t APU::readPort(uint8_t port) {
    // SPC가 쓴 값을 CPU가 읽음
    return m_aram[0xF4 + port];
}

uint8_t APU::read(uint16_t address) {
    // IPL ROM 읽기
    if (m_iplromEnable && address >= IPL_ROM_BASE) {
        return m_iplROM[address - IPL_ROM_BASE];
    }
    
    // 일반 ARAM 읽기
    return m_aram[address];
}

void APU::write(uint16_t address, uint8_t value) {
    // Port 쓰기 (SPC → CPU)
    if (address >= 0xF4 && address <= 0xF7) {
        uint8_t port = address - 0xF4;
        m_aram[address] = value;
        
        printf("APU: SPC700 wrote port %d = 0x%02X (old=0x%02X, PC=0x%04X)\n",
               port, value, m_aram[address], m_regs.pc);
    }
    else if (address == 0xF1) {
        // Control register - IPL ROM enable/disable
        bool old_ipl = m_iplromEnable;
        m_iplromEnable = (value & 0x80) != 0;
        
        if (old_ipl != m_iplromEnable) {
            printf("APU: IPL ROM %s (Control=$F1=0x%02X)\n",
                   m_iplromEnable ? "enabled" : "disabled", value);
        }
        
        m_aram[address] = value;
    }
    else {
        // 일반 ARAM 쓰기
        m_aram[address] = value;
    }
}
```

---

## 🧪 테스트 & 검증

### 성공 기준

```
✓ "IPL BOOT COMPLETE!" 메시지 출력
✓ "Jumping to 0x0200" (또는 다른 주소)
✓ SPC PC가 IPL ROM 영역(0xFFC0~)을 벗어남
✓ Port 2에 테스트 번호 출력 (0x00, 0xF0, 0xF1, ...)
```

### 실패 시 체크리스트

```
❌ "Transfer complete (Port 3 = 0x00)" 메시지 없음
   → CPU가 Port 3에 0x00을 쓰지 않음

❌ "Jumping to ..." 메시지 없음
   → CPU가 Port 0/1에 실행 주소를 쓰지 않음

❌ SPC PC가 0xFFDA~0xFFE9 사이를 무한 반복
   → IPL ROM이 비활성화되지 않음

❌ Port 2 값이 0xF0~0x42 범위로 변하지 않음
   → 프로그램이 실행되지 않음
```

### 디버깅 명령어

```powershell
# PC 분포 확인
python pc_distribution_analyzer.py

# 포트 통신 확인
.\analyze_ports.ps1

# IPL 벗어났는지 확인
Select-String -Path "apu_trace.log" -Pattern "Jumping to"
```

---

## 📚 참고: 실제 IPL ROM 코드

실제 SNES의 IPL ROM 디스어셈블리 (참고용):

```asm
; 0xFFC0: 초기화
FFC0: CD EF       MOV X, #$EF
FFC2: BD          MOV SP, X
FFC3: E8 00       MOV A, #$00

; 0xFFC5-FFC9: RAM 클리어
FFC5: C6 1D       MOV (X), A
FFC6: 1D          DEC X
FFC7: D0 FC       BNE $FFC5
FFC9: 8F AA F4    MOV $F4, #$AA   ; Port 0 = 0xAA (Ready)

; 0xFFCC-FFD5: 대기 루프
FFCC: 8F BB F5    MOV $F5, #$BB   ; Port 1 = 0xBB
FFCF: 78 CC F4    CMP $F4, #$CC   ; Port 0 == 0xCC?
FFD2: D0 FB       BNE $FFCF       ; 아니면 대기
FFD4: 2F 19       BRA $FFEF

; 0xFFDA-FFE9: 데이터 수신 루프
FFDA: 7E F4       CMP Y, $F4      ; Port 0 변화 체크
FFDC: D0 0B       BNE $FFE9
FFDE: E4 F5       MOV A, $F5      ; Port 1에서 읽기
FFE0: CB F4       MOV $F4, Y      ; Port 0 = Y (ACK)
FFE2: D7 00       MOV ($00)+Y, A  ; RAM에 저장
FFE4: FC          INC Y
FFE5: D0 F3       BNE $FFDA
FFE7: AB 01       INC $01
FFE9: 10 EF       BPL $FFDA
FFEB: 7E F4       CMP Y, $F4
FFED: 10 EB       BPL $FFDA

; 0xFFEF-FFF6: 실행 주소로 점프
FFEF: BA F6       MOVW YA, $F6    ; Port 2/3
FFF1: DA 00       MOVW $00, YA
FFF3: BA F4       MOVW YA, $F4    ; Port 0/1
FFF5: C4 F4       MOV $F4, A
FFF7: DD          MOV A, Y
FFF8: 5D          MOV X, A
FFF9: D0 DB       BNE $FFD6
FFFB: 1F 00 00    JMP ($0000+X)   ; 점프!

; 0xFFFE: 리셋 벡터
FFFE: C0 FF       ; Reset vector = $FFC0
```

---

## 🎯 핵심 포인트

### 반드시 구현해야 할 것 ⭐⭐⭐

1. **Port 3 = 0x00 감지**
   - 전송 완료 신호
   - `SPC_LOAD_RECEIVING` → `SPC_LOAD_WAIT_EXEC` 전환

2. **Port 0/1로 실행 주소 수신**
   - Port 0 = Low Byte
   - Port 1 = High Byte

3. **IPL ROM 비활성화**
   - `m_iplromEnable = false`
   - 이제 PC가 ARAM을 읽음

4. **PC 점프**
   - `m_regs.pc = m_spcExecAddr`
   - 프로그램 실행 시작!

### 자주 하는 실수 ❌

```cpp
// ❌ 잘못된 구현
if (port == 0 && value == something) {
    m_iplromEnable = false;  // 조건이 너무 일반적!
}

// ✓ 올바른 구현
if (port == 0 && m_spcLoadState == SPC_LOAD_WAIT_EXEC) {
    // 실행 주소 수신 후에만!
    m_iplromEnable = false;
}
```

---

**최종 업데이트**: 2025-12-14  
**구현 상태**: HLE 방식 코드 작성 완료  
**테스트 상태**: spctest.sfc로 검증 필요  
**중요도**: ⭐⭐⭐⭐⭐ (없으면 사운드 프로그램 로드 불가)










