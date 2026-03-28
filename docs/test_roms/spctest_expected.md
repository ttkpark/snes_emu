# spctest.sfc 테스트 ROM 상세 문서

## 개요

`spctest.sfc`는 SPC700 CPU의 모든 명령어를 테스트하는 종합 테스트 ROM입니다. SLEEP과 STOP을 제외한 모든 opcode를 검증합니다.

---

## 테스트 구조

### 파일 구성

```
spctest/
├── spctest.asm          - 메인 65C816 CPU 코드
├── spc_common.inc       - 공통 SPC700 루틴
├── spc_tests0.asm       - 테스트 세트 0
├── spc_tests1.asm       - 테스트 세트 1
└── spc_tests2.asm       - 테스트 세트 2
```

### CPU-APU 통신 프로토콜

#### 포트 매핑
- **CPU 측**: $2140-$2143 (메인 CPU가 읽고 쓰는 포트)
- **APU 측**: $F4-$F7 (SPC700이 읽고 쓰는 포트)

#### 프로토콜 단계

```
1. 초기화 단계
   CPU: 포트 0 <- 0xCC
   CPU: 포트 1 <- 0x01 (테스트 시작 신호)
   
   SPC: 포트 0 읽기 (0xCC 기대)
   SPC: 포트 1 읽기 (0x01 기대)
   SPC: 포트 0 <- 0x00 (준비 완료 신호)
   
   CPU: 포트 0 읽기 (0x00 기대)

2. 테스트 실행 단계
   각 테스트마다:
   - SPC: 테스트 실행
   - 성공 시: 포트 2 <- 테스트 번호 (0xf3, 0xf4, ...)
   - 실패 시: fail 루틴으로 점프

3. 완료 단계
   모든 테스트 성공 시:
   - SPC: 포트 0 <- 0xFF (완료 신호)
   
   실패 시:
   - SPC: 포트 2 <- 0x02 또는 0x03
   - SPC: 무한 루프 진입 (BRA -2 at 0x0357)
```

---

## spc_common.inc 분석

### main 루틴 (0x0300)

```asm
main:
    ; 1. 포트 0이 0xCC인지 확인
    mov   a, $f4           ; 포트 0 읽기
    cmp   a, #$cc          ; 0xCC와 비교
    bne   fail             ; 다르면 실패
    
    ; 2. 포트 1이 0x01인지 확인
    mov   a, $f5           ; 포트 1 읽기
    cmp   a, #$01          ; 0x01과 비교
    bne   fail             ; 다르면 실패
    
    ; 3. 포트 0에 0x00 쓰기 (준비 완료)
    mov   $f4, #$00
    
    ; 4. 테스트 실행
    call  init_test
    ; ... (테스트들)
    
    ; 5. 모든 테스트 성공
    bra   success
```

### init_test 루틴

```asm
init_test:
    ; 레지스터 초기화
    mov   a, #$56
    mov   x, #$34
    mov   y, #$56
    mov   sp, #$ef
    
    ; 플래그 초기화
    clrc                   ; C = 0
    clrv                   ; V = 0, H = 0
    ; PSW = 0x00 (모든 플래그 클리어)
    
    ret
```

### success 루틴

```asm
success:
    mov   $f4, #$ff        ; 포트 0 <- 0xFF (성공 신호)
-   bra   -                ; 무한 루프
```

### fail 루틴 (0x0355)

```asm
fail:
    mov   $f6, #$02        ; 포트 2 <- 0x02 (실패 마커)
    mov   $f6, #$03        ; 포트 2 <- 0x03 (fail 루틴 마커)
-   bra   -                ; 무한 루프 (0x0357: BRA -2)
```

**주의**: 0x0357 주소에서 `BRA -2` (0x2F 0xFE)가 실행되면 실패입니다.

---

## 현재 실패 분석

### 증상

```
[Cyc:0000885366] SPC700 PC:0x0357 | 2f fe | BRA rel | operand=0xfe
A:0x56 | X:0x34 | Y:0x56 | SP:0xef | PSW:0x01
```

- **PC**: 0x0357 (fail 루틴의 무한 루프)
- **포트 2**: 0x03 (fail 루틴 진입 확인)
- **레지스터**: A=0x56, X=0x34, Y=0x56 (초기 값 유지)
- **PSW**: 0x01 (Carry 플래그만 설정됨)

### 실패 지점 추정

fail 루틴에 도달했다는 것은 다음 중 하나에서 실패:

1. **포트 0 != 0xCC** (초기화 단계)
2. **포트 1 != 0x01** (초기화 단계)
3. **테스트 중 BNE fail** (조건 분기 실패)

### 디버깅 체크리스트

#### 1. 포트 통신 확인

```powershell
# 포트 읽기/쓰기 이벤트 추출
Get-Content port_comm.log | Select-String "port"

# 확인 사항:
# - CPU가 포트 0에 0xCC를 썼는가?
# - CPU가 포트 1에 0x01을 썼는가?
# - SPC가 이 값들을 올바르게 읽었는가?
```

#### 2. CMP 명령어 검증

```powershell
# CMP 명령어 실행 추적
Get-Content apu_trace.log | Select-String "cmp.*\$f" | Select-Object -First 20

# 확인 사항:
# - CMP 실행 후 Z 플래그가 올바르게 설정되었는가?
# - BNE 분기가 올바르게 동작하는가?
```

#### 3. 분기 명령어 검증

```powershell
# BNE 명령어 실행 추적
Get-Content apu_trace.log | Select-String "bne" -CaseSensitive | Select-Object -First 20

# 확인 사항:
# - Z=1일 때 BNE가 분기하지 않는가? (올바름)
# - Z=0일 때 BNE가 분기하는가? (올바름)
```

---

## 예상 실행 흐름

### 정상 실행 (성공 케이스)

```
1. [0x0300] MOV A, $F4      -> A = 0xCC (CPU가 쓴 값)
2. [0x0302] CMP A, #$CC     -> Z=1, C=1 (같음)
3. [0x0304] BNE fail        -> 분기 안함 (Z=1이므로)
4. [0x0306] MOV A, $F5      -> A = 0x01
5. [0x0308] CMP A, #$01     -> Z=1, C=1 (같음)
6. [0x030A] BNE fail        -> 분기 안함
7. [0x030C] MOV $F4, #$00   -> 포트 0 <- 0x00
8. [0x030E] CALL init_test
9. ... (테스트 실행)
10. [0x0350] BRA success    -> 성공!
```

### 실패 실행 (현재 상황)

```
1. [0x0300] MOV A, $F4      -> A = ??? (잘못된 값?)
2. [0x0302] CMP A, #$CC     -> Z=0 (다름)
3. [0x0304] BNE fail        -> 분기함! (Z=0이므로)
4. [0x0355] MOV $F6, #$02   -> 포트 2 <- 0x02
5. [0x0357] MOV $F6, #$03   -> 포트 2 <- 0x03
6. [0x0359] BRA -2          -> 무한 루프
```

또는:

```
1. [0x0306] MOV A, $F5      -> A = ??? (잘못된 값?)
2. [0x0308] CMP A, #$01     -> Z=0 (다름)
3. [0x030A] BNE fail        -> 분기함!
```

---

## 🔍 디버깅 전략

### Step 1: 포트 초기화 확인

```cpp
// src/main_complete.cpp 또는 snes_core.cpp
// CPU가 APU를 시작하기 전에 포트 초기화

// 1. CPU 측에서 포트 0에 0xCC 쓰기
memory->write(0x2140, 0xCC);

// 2. CPU 측에서 포트 1에 0x01 쓰기
memory->write(0x2141, 0x01);

// 3. APU 시작
apu->reset();

// 4. CPU가 포트 0이 0x00이 될 때까지 대기
while (memory->read(0x2140) != 0x00) {
    apu->step();
}
```

### Step 2: 포트 읽기/쓰기 로깅

```cpp
// src/apu/apu.cpp
uint8_t APU::readPort(uint8_t port) {
    uint8_t value = io_ports[port];
    
    // 로깅 추가
    fprintf(port_log, "[Cyc:%010lu] SPC read port %d = 0x%02X\n",
            total_cycles, port, value);
    fflush(port_log);
    
    return value;
}

void APU::writePort(uint8_t port, uint8_t value) {
    io_ports[port] = value;
    
    // 로깅 추가
    fprintf(port_log, "[Cyc:%010lu] SPC wrote port %d = 0x%02X\n",
            total_cycles, port, value);
    fflush(port_log);
}
```

### Step 3: CMP 명령어 상세 로깅

```cpp
// src/apu/apu.cpp - CMP 명령어 처리
case 0x68:  // CMP A, #imm
    operand = readByte(pc++);
    {
        uint8_t temp = a - operand;
        bool new_c = (a >= operand);  // unsigned 비교
        bool new_z = (temp == 0);
        bool new_n = (temp & 0x80) != 0;
        
        // 상세 로깅
        fprintf(trace_log, "   CMP: A=0x%02X - operand=0x%02X = 0x%02X\n",
                a, operand, temp);
        fprintf(trace_log, "   Flags: C=%d Z=%d N=%d (before: PSW=0x%02X)\n",
                new_c, new_z, new_n, psw);
        
        setFlag(FLAG_C, new_c);
        setFlag(FLAG_Z, new_z);
        setFlag(FLAG_N, new_n);
        
        fprintf(trace_log, "   After: PSW=0x%02X\n", psw);
    }
    cycles = 2;
    break;
```

### Step 4: 분기 명령어 로깅

```cpp
// src/apu/apu.cpp - BNE 명령어
case 0xD0:  // BNE rel
    {
        int8_t offset = (int8_t)readByte(pc++);
        bool z_flag = (psw & FLAG_Z) != 0;
        
        fprintf(trace_log, "   BNE: offset=%d, Z=%d\n", offset, z_flag);
        
        if (!z_flag) {  // Z=0이면 분기
            uint16_t target = pc + offset;
            fprintf(trace_log, "   BRANCH TAKEN: 0x%04X -> 0x%04X\n",
                    pc, target);
            pc = target;
            cycles = 4;
        } else {
            fprintf(trace_log, "   BRANCH NOT TAKEN\n");
            cycles = 2;
        }
    }
    break;
```

---

## 💡 자주 발생하는 버그

### 1. 포트 동기화 문제

**증상**: SPC가 CPU가 쓴 값을 읽기 전에 실행되거나, 그 반대

**해결책**:
```cpp
// CPU와 APU를 정확한 사이클 비율로 실행
// SNES: CPU (21.47 MHz / 6 = 3.58 MHz), APU (1.024 MHz)
// 비율: CPU 1 사이클당 APU 약 0.286 사이클

for (int i = 0; i < cpu_cycles; i++) {
    cpu->step();
    
    // APU를 올바른 비율로 실행
    apu_cycle_debt += 0.286;
    while (apu_cycle_debt >= 1.0) {
        apu->step();
        apu_cycle_debt -= 1.0;
    }
}
```

### 2. CMP Carry 플래그 반전

**잘못된 구현**:
```cpp
setFlag(FLAG_C, a < operand);  // ❌ 반대임!
```

**올바른 구현**:
```cpp
setFlag(FLAG_C, a >= operand);  // ✅
```

### 3. 분기 오프셋 계산 오류

**잘못된 구현**:
```cpp
uint8_t offset = readByte(pc++);
pc += offset;  // ❌ unsigned로 처리
```

**올바른 구현**:
```cpp
int8_t offset = (int8_t)readByte(pc++);
pc += offset;  // ✅ signed로 처리
```

---

## ✅ 성공 판정 기준

### 포트 2 값

- **0xf3, 0xf4, ...**: 테스트 진행 중 (각 테스트 번호)
- **0x02 또는 0x03**: 테스트 실패
- (포트 2 사용 안함): 테스트 진행 중

### 포트 0 값

- **0xCC**: CPU 초기화 값
- **0x00**: SPC 준비 완료
- **0xFF**: 모든 테스트 성공!

### 최종 PC 주소

- **0x0357**: fail 루틴 (무한 루프) - 실패
- **0x0352**: success 루틴 (무한 루프) - 성공

---

## 📊 테스트 통과 시나리오

```
1. CPU 초기화
   CPU: PORT[0] <- 0xCC
   CPU: PORT[1] <- 0x01

2. APU 응답
   SPC: A <- PORT[0] (0xCC)
   SPC: CMP A, #0xCC -> Z=1
   SPC: BNE fail -> 분기 안함 ✓
   
   SPC: A <- PORT[1] (0x01)
   SPC: CMP A, #0x01 -> Z=1
   SPC: BNE fail -> 분기 안함 ✓
   
   SPC: PORT[0] <- 0x00 (준비 완료)

3. 테스트 실행
   SPC: 각 테스트 실행...
   SPC: PORT[2] <- 테스트 번호

4. 성공
   SPC: PORT[0] <- 0xFF
   SPC: PC=0x0352 (success 루틴)
```

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: spctest.sfc 실패 디버깅 중










