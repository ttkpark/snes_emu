# Step-by-Step 디버깅 최종 결과

**분석 일시**: 2025-12-14  
**요청**: 마지막 테스트(0x42)까지 중단점 걸고 하나하나 명령어를 step하면서 동작 시험 및 이상 지점 검출  
**결과**: ❌ **프로그램이 전혀 실행되지 않음**

---

## 🚨 발견된 치명적 문제

### **SPC700이 IPL ROM에서 영원히 갇혀있습니다!**

```
================================================================================
PC 범위별 실행 통계
================================================================================
IPL_ROM  [0xFFC0-0xFFFF]:   88,953회 (100.0%) #################### [갇힘!]
PROGRAM  [0x0000-0xFFBF]:        0회 (  0.0%)
```

**의미**:
- 88,953개의 명령어 **모두** IPL ROM 영역(0xFFC0~0xFFFF)에서 실행
- 프로그램 영역(0x0000~0xFFBF)에서는 **단 한 번도** 실행 안됨
- **테스트 0x00조차 시작되지 않음**
- 마지막 테스트(0x42)는 **존재하지도 않음**

---

## 📊 실행 흐름 분석

### TOP 8 가장 많이 실행된 PC

| 순위 | PC | 실행 횟수 | 비율 | 설명 |
|------|-------|-----------|------|------|
| 1 | 0xFFDA | 18,696회 | 21.0% | IPL 데이터 수신 루프 시작 |
| 2 | 0xFFDC | 18,696회 | 21.0% | 분기 판단 |
| 3 | 0xFFE9 | 10,709회 | 12.0% | 음수 체크 |
| 4 | 0xFFDE | 8,017회 | 9.0% | Port 읽기 |
| 5 | 0xFFE0 | 8,017회 | 9.0% | Port 쓰기 |
| 6 | 0xFFE2 | 8,017회 | 9.0% | 데이터 저장 |
| 7 | 0xFFE4 | 8,017회 | 9.0% | Y 증가 |
| 8 | 0xFFE5 | 8,017회 | 9.0% | 루프 분기 |

**분석**:
- 0xFFDA ~ 0xFFE9 사이를 **무한 반복**
- IPL ROM의 데이터 수신 루틴에 갇힘
- CPU로부터 데이터를 계속 기다리는 중

### 실행 패턴 (무한 루프)

```
┌──────────────────────────────────────────┐
│  0xFFDA: CMP Y,dp      ← 루프 시작      │
│  0xFFDC: BNE +0x0B                        │
│  0xFFDE: MOV A,dp       (데이터 읽기)    │
│  0xFFE0: MOV dp,Y       (Port 쓰기)      │
│  0xFFE2: MOV (dp)+Y,A   (저장)           │
│  0xFFE4: INC Y          (카운터 증가)    │
│  0xFFE5: BNE -0x0D     ────┐             │
└────────────────────────────┼─────────────┘
                              │
                              └─────────────→ 다시 0xFFDA로
```

---

## 🔍 근본 원인 분석

### IPL 프로토콜이 완료되지 않음

**정상 IPL 프로토콜 (SNES 표준)**:

```
Step 1: CPU → Port 0에 0xCC 쓰기 (대기 신호)
Step 2: CPU → Port 1에 0x01 쓰기 (시작 신호)
Step 3: CPU → Port 2/3에 주소 쓰기
Step 4: CPU → 데이터 전송 (Port 1 사용)
        SPC ← Port 1에서 데이터 읽고 메모리에 저장
Step 5: CPU → Port 3에 0x00 쓰기 ⭐ (전송 완료)
Step 6: CPU → Port 0에 실행 주소 쓰기 ⭐
Step 7: SPC → IPL ROM 비활성화
Step 8: SPC → 실행 주소로 점프 ⭐
```

**현재 상태**:

```
Step 1-4: ✓ 완료 (81바이트 전송됨)
Step 5:   ❌ Port 3에 0x00이 쓰이지 않음
Step 6:   ❌ Port 0에 실행 주소가 쓰이지 않음
Step 7:   ❌ IPL ROM이 비활성화되지 않음
Step 8:   ❌ 프로그램으로 점프 안됨
```

**증거**:
- Y = 0x51 (81바이트 수신)
- Port 0 = 0x50 (Y의 마지막 값)
- Port 1 = 0xE8 (CPU가 계속 쓰는 중)
- SPC는 여전히 IPL ROM에서 대기

---

## 🛠️ 해결 방법

### 1. CPU 코드 확인 (spctest.sfc)

**문제**: CPU가 IPL 프로토콜을 완료하지 않음

**확인 사항**:
```cpp
// spctest.sfc의 CPU 초기화 코드에서:

// ❌ 누락된 부분:
// 1. Port 3에 0x00 쓰기 (전송 완료 신호)
LDA #$00
STA $2143   ; Port 3 = 0x00

// 2. Port 0에 실행 주소 쓰기
LDA #$02    ; 실행 주소 low byte (예: 0x0200)
STA $2140   ; Port 0
LDA #$00    ; 실행 주소 high byte
STA $2141   ; Port 1
```

### 2. APU 코드 확인

**문제**: APU가 Port 0 쓰기를 감지하지 못함

**src/apu/apu.cpp 확인**:

```cpp
void APU::writePort(uint8_t port, uint8_t value) {
    m_cpuPorts[port] = value;
    
    // ⭐ 이 부분이 제대로 동작하는지 확인
    if (port == 3 && value == 0x00) {
        // 전송 완료
        m_spcLoadState = SPC_LOAD_WAIT_EXEC;
        printf("APU: Transfer complete, waiting for exec addr\n");
    }
    
    // ⭐ 이 부분이 있는지 확인!
    if (port == 0 && m_spcLoadState == SPC_LOAD_WAIT_EXEC) {
        // 실행 주소 수신
        m_spcExecAddr = (m_cpuPorts[1] << 8) | m_cpuPorts[0];
        
        // ⭐ IPL ROM 비활성화!
        m_iplromEnable = false;
        
        // ⭐ 실행 주소로 점프!
        m_regs.pc = m_spcExecAddr;
        
        m_spcLoadState = SPC_LOAD_COMPLETE;
        printf("APU: Jumping to 0x%04X\n", m_spcExecAddr);
    }
}
```

### 3. 로깅 추가

**디버깅을 위한 추가 로깅**:

```cpp
// APU::writePort()
if (port == 3) {
    printf("APU: Port 3 write: 0x%02X, loadState=%d, PC=0x%04X\n", 
           value, m_spcLoadState, m_regs.pc);
}

if (port == 0) {
    printf("APU: Port 0 write: 0x%02X, loadState=%d, PC=0x%04X\n", 
           value, m_spcLoadState, m_regs.pc);
}

// APU::step()
if (m_regs.pc >= 0xFFC0 && m_bootComplete) {
    static int warn_count = 0;
    if (warn_count++ < 5) {
        printf("WARNING: Still in IPL ROM at cycle %d, PC=0x%04X\n", 
               m_spc700Cycles, m_regs.pc);
    }
}
```

---

## 📈 테스트 계획

### Step 1: 로깅 추가 및 재빌드

```batch
REM src/apu/apu.cpp에 로깅 추가
.\build_complete.bat
```

### Step 2: 재실행 및 로그 확인

```powershell
.\snes_emu_complete.exe spctest.sfc

# Port 3 쓰기 확인
Select-String -Path "apu_trace.log" -Pattern "Port 3 write"

# IPL ROM 탈출 확인
Select-String -Path "apu_trace.log" -Pattern "Jumping to"

# PC 분포 재확인
python pc_distribution_analyzer.py
```

### Step 3: 성공 기준

```
✓ "Port 3 write: 0x00" 로그 출력
✓ "Jumping to 0x0200" (또는 다른 주소) 로그 출력
✓ PC 분포에서 PROGRAM 영역 > 0%
✓ "SPC700 wrote port 2 = 0x00" (첫 번째 테스트 번호)
```

---

## 🎓 이번 분석에서 배운 것

### 실시간 디버깅의 중요성

❌ **원래 계획**:
- "마지막 테스트(0x42)까지 중단점 걸고 step-by-step 분석"

✅ **실제 필요한 것**:
- **"첫 번째 명령어조차 실행되는지 확인"**

### PC 분포 분석의 힘

- 단 1개 명령어로 문제 파악:
  ```python
  python pc_distribution_analyzer.py
  # "100% IPL ROM" → 즉시 문제 발견!
  ```

- Step-by-step은 불필요:
  - 프로그램이 실행 안되면 step할 것도 없음
  - 전체 흐름 파악이 우선

### 로그 파일 크기의 함정

- 12.31 MB 로그 = 많은 일이 일어난 것처럼 보임
- 실제로는 같은 30개 PC를 무한 반복
- **크기보다 내용이 중요**

---

## ✅ 다음 단계

### 즉시 해야 할 일 (우선순위)

1. **APU::writePort() 구현 확인** ⭐⭐⭐
   - Port 3 = 0x00 처리
   - Port 0 = 실행 주소 처리
   - IPL ROM 비활성화

2. **src/apu/apu.cpp 수정**
   ```cpp
   // Port 0 쓰기 시 실행 주소로 점프하도록
   if (port == 0 && m_spcLoadState == SPC_LOAD_WAIT_EXEC) {
       m_iplromEnable = false;
       m_regs.pc = (m_cpuPorts[1] << 8) | m_cpuPorts[0];
   }
   ```

3. **로깅 추가 후 재테스트**
   - Port 3/0 쓰기 로그
   - IPL ROM 탈출 로그
   - PC 분포 재확인

### 성공 후 다음 단계

일단 프로그램이 실행되면:
- ✅ 테스트 0x00 ~ 0x42 실행
- ✅ 그때 step-by-step 분석
- ✅ 실패 지점 정확히 파악

---

## 📝 요약

**질문**: "마지막 테스트까지 중단점 걸고 step하면서 이상 지점 검출해줘"

**답변**: 
- ❌ 마지막 테스트(0x42)는 존재하지 않음
- ❌ 첫 번째 테스트(0x00)도 실행 안됨
- ❌ 프로그램 자체가 로드되지 않음
- ⭐ **SPC700이 IPL ROM에 갇혀있음 (100.0%)**

**이상 지점**:
- 🔴 CPU가 Port 3에 0x00을 쓰지 않음 (전송 완료 신호)
- 🔴 CPU가 Port 0에 실행 주소를 쓰지 않음
- 🔴 APU가 IPL ROM을 비활성화하지 않음
- 🔴 SPC PC가 프로그램 영역으로 이동하지 않음

**결론**: 
Step-by-step 분석 이전에 **프로그램 로딩부터 수정** 필요!

---

**상태**: 🔴 **CRITICAL** - 프로그램 실행 불가  
**다음 작업**: IPL 프로토콜 완성  
**예상 소요 시간**: 1-2시간










