# SPC700 Step-by-Step 디버깅 결과 - 치명적 문제 발견!

**분석일**: 2025-12-14  
**분석 도구**: 실시간 로그 분석  
**대상**: spctest.sfc  
**결과**: ❌ **프로그램 로드 실패**

---

## 🚨 치명적 문제 발견!

### 문제: SPC700이 IPL ROM에서 영원히 갇혀있음

**증상**:
- SPC700이 IPL ROM 영역(0xFFDA ~ 0xFFE9)에서 무한 루프
- 프로그램이 전혀 로드되지 않음
- 테스트가 단 한 번도 실행되지 않음
- PC가 0x0000~0xFFBF 영역으로 절대 이동하지 않음

**실행 흐름**:
```
PC:0xFFDA -> 0xFFDC -> 0xFFDE -> 0xFFE0 -> 0xFFE2 -> 0xFFE4 -> 0xFFE5
  ↑                                                                |
  └────────────────────────────────────────────────────────────────┘
  (무한 루프)
```

---

## 📊 실행 통계

### 로그 분석 결과

| 항목 | 값 |
|------|------|
| 로그 크기 | 12.31 MB |
| 총 라인 수 | 105,164 줄 |
| 파싱된 명령어 | 88,953 개 |
| 실행 사이클 | ~118,610 사이클 |
| **테스트 실행 횟수** | **0회** ❌ |
| **PC 범위** | **0xFFC0 ~ 0xFFFF (IPL ROM만)** |

### 실행된 PC 주소 분포

```
0xFFC0 ~ 0xFFFF: 88,953회 (100%)  ← IPL ROM 영역
0x0000 ~ 0xFFBF:      0회 (0%)    ← 프로그램 영역 (실행 안됨!)
```

---

## 🔍 세부 분석

### 1. IPL ROM 무한 루프

**마지막 20개 명령어**:
```
[Cyc:0000118589] PC:0xFFE4 | INC Y
[Cyc:0000118590] PC:0xFFE5 | BNE 0xF3
[Cyc:0000118592] PC:0xFFDA | CMP Y,dp
[Cyc:0000118593] PC:0xFFDC | BNE 0x0B
[Cyc:0000118594] PC:0xFFE9 | BPL 0xEF
[Cyc:0000118596] PC:0xFFDA | CMP Y,dp  (반복 시작)
[Cyc:0000118597] PC:0xFFDC | BNE 0x0B
[Cyc:0000118598] PC:0xFFDE | MOV A,dp
[Cyc:0000118600] PC:0xFFE0 | MOV dp,Y
[Cyc:0000118601] PC:0xFFE2 | MOV (dp)+Y,A
[Cyc:0000118602] PC:0xFFE4 | INC Y
[Cyc:0000118604] PC:0xFFE5 | BNE 0xF3
[Cyc:0000118605] PC:0xFFDA | CMP Y,dp  (다시 반복)
...
```

**분석**:
- 0xFFDA ~ 0xFFE9 사이를 무한 반복
- IPL ROM의 데이터 수신 루틴
- CPU로부터 데이터를 기다리는 중

### 2. 포트 통신 상태

**마지막 포트 값**:
```
Port 0: 0x50 (SPC -> CPU)  Y 레지스터 값 (수신한 바이트 수)
Port 1: 0xE8 (CPU -> SPC)  데이터 바이트
Port 2: 확인 불가
Port 3: 확인 불가
```

**문제점**:
- CPU가 계속 Port 1에 데이터를 쓰고 있음
- SPC가 데이터를 수신하고 있음 (Y가 증가)
- **하지만 프로그램 실행으로 전환되지 않음!**

### 3. IPL 프로토콜 분석

**정상 IPL 프로토콜**:
```
1. CPU: Port 1에 주소(low), Port 2에 주소(high) 쓰기
2. CPU: Port 3에 0x01 쓰기 (start transfer)
3. SPC: 데이터 수신 (Port 1에서 읽기)
4. CPU: Port 3에 0x00 쓰기 (end transfer)
5. CPU: Port 0에 실행 주소 쓰기
6. SPC: IPL ROM 비활성화 후 프로그램 점프
```

**현재 상태**:
```
Y = 0x51 (81 바이트 수신)
A = 0x2D (마지막 수신 바이트)
Port 0 = 0x50
Port 1 = 0xE8 (CPU가 계속 쓰는 중)
```

**추론**:
- 81바이트만 수신
- Port 3이 0x00으로 설정되지 않아 전송이 종료되지 않음
- 또는 실행 주소가 Port 0에 쓰이지 않음

---

## 🎯 근본 원인

### CPU 측 문제

**spctest.sfc의 CPU 코드 문제**:

1. **IPL 프로토콜 미완성**
   - Port 3에 종료 신호(0x00) 전송 안함
   - 또는 Port 0에 실행 주소 전송 안함

2. **데이터 전송 미완료**
   - 81바이트만 전송 (spctest 프로그램은 훨씬 큼)
   - 전송 중단 또는 타이밍 문제

3. **CPU-APU 동기화 문제**
   - CPU가 APU 응답을 기다리지 않음
   - 또는 잘못된 순서로 명령 전송

---

## 🔧 해결 방안

### 1. CPU 코드 점검

```cpp
// src/cpu/cpu.cpp 또는 main_complete.cpp
// spctest.sfc 로드 시 CPU가 IPL 프로토콜을 올바르게 수행하는지 확인

// 확인 사항:
// 1. Port 0에 0xCC 쓰기 (대기 신호)
// 2. Port 1에 0x01 쓰기 (IPL ROM 활성화)
// 3. 데이터 전송
// 4. Port 3에 0x00 쓰기 (전송 종료)
// 5. Port 0에 실행 주소 쓰기
```

### 2. APU 프로토콜 검증

```cpp
// src/apu/apu.cpp
// IPL ROM 비활성화 조건 확인

void APU::writePort(uint8_t port, uint8_t value) {
    if (port == 3) {
        // 0x00이 쓰일 때 전송 완료 처리
        if (value == 0x00 && m_spcLoadState == SPC_LOAD_RECEIVING) {
            m_spcLoadState = SPC_LOAD_WAIT_EXEC;
            // 실행 주소 대기
        }
    }
    
    if (port == 0 && m_spcLoadState == SPC_LOAD_WAIT_EXEC) {
        // Port 0에 실행 주소가 쓰일 때
        m_spcExecAddr = (value << 8) | ...;
        m_iplromEnable = false;  // IPL ROM 비활성화
        m_pc = m_spcExecAddr;     // 실행 주소로 점프
    }
}
```

### 3. 로그 추가

```cpp
// 디버깅을 위한 로그 추가
if (port == 3) {
    printf("APU: Port 3 write: 0x%02X, loadState=%d\n", value, m_spcLoadState);
}

if (m_pc >= 0xFFC0 && m_bootComplete) {
    static int warnCount = 0;
    if (warnCount++ < 10) {
        printf("WARNING: Still in IPL ROM after boot! PC=0x%04X\n", m_pc);
    }
}
```

---

## 📈 테스트 재실행 계획

### Step 1: Port 3 로깅 활성화

```cpp
// APU::writePort() 에 로깅 추가
printf("APU: CPU wrote port %d = 0x%02X, loadState=%d, PC=0x%04X\n", 
       port, value, m_spcLoadState, m_pc);
```

### Step 2: 재실행

```powershell
.\build_complete.bat
.\snes_emu_complete.exe spctest.sfc
```

### Step 3: 로그 확인

```powershell
# Port 3 쓰기 확인
Select-String -Path "apu_trace.log" -Pattern "Port 3"

# IPL ROM 탈출 확인
Select-String -Path "apu_trace.log" -Pattern "PC:0x0[0-9a-f]{3} " | Select-Object -First 10
```

---

## 🎓 학습 내용

### 발견한 것

1. **step-by-step 분석 불필요**
   - SPC가 프로그램조차 실행하지 않음
   - IPL ROM 단계에서 멈춤

2. **진짜 문제는 프로그램 로딩**
   - 테스트 실패 이전에 로딩 실패
   - CPU-APU 프로토콜 문제

3. **로그 분석의 중요성**
   - PC 분포만 봐도 문제 즉시 파악
   - "테스트 0x42 분석"은 불가능 (실행 안됨)

### 교훈

- ✅ 로그 파일 크기보다 **PC 범위** 확인이 먼저
- ✅ "마지막 테스트"보다 **"첫 테스트"**가 실행됐는지 확인
- ✅ Step-by-step보다 **전체 흐름** 파악이 먼저

---

## ✅ 다음 단계

### 즉시 해야 할 일

1. **APU::writePort() 로깅 강화**
   - Port 3 쓰기 감지
   - loadState 전환 추적

2. **CPU 코드 점검**
   - spctest.sfc가 올바른 IPL 프로토콜 사용하는지
   - 실행 주소 전송 확인

3. **IPL 프로토콜 재구현**
   - Port 3 = 0x00 처리
   - Port 0 = 실행 주소 처리
   - IPL ROM 비활성화 타이밍

### 성공 기준

- ✅ SPC PC가 0x0200 (또는 프로그램 시작 주소)로 이동
- ✅ Port 2에 테스트 번호 (0x00) 기록
- ✅ 첫 테스트 진입

---

**결론**: 마지막 테스트(0x42)는 커녕, **첫 테스트(0x00)도 실행되지 않았습니다!**  
**근본 원인**: IPL 프로토콜 미완성으로 프로그램 로드 실패

**상태**: 🔴 Critical - 프로그램 실행 불가










