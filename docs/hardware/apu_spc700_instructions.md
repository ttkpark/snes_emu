# SPC700 명령어 세트 상세 문서

## 개요

SPC700은 8비트 프로세서로, Sony가 SNES APU를 위해 설계했습니다. 이 문서는 모든 SPC700 opcode의 동작과 플래그 계산을 상세히 설명합니다.

---

## 프로세서 상태 워드 (PSW)

```
Bit 7: N - Negative flag
Bit 6: V - Overflow flag
Bit 5: P - Direct page flag
Bit 4: B - Break flag
Bit 3: H - Half-carry flag (BCD 연산용)
Bit 2: I - Interrupt enable flag
Bit 1: Z - Zero flag
Bit 0: C - Carry flag
```

---

## 16비트 명령어 (MOVW, INCW, DECW, ADDW, SUBW, CMPW)

### MOVW YA, dp (0xBA)
```
Operation: YA = [dp] | ([dp+1] << 8)
Cycles: 5
Flags: N, Z

16비트 Direct Page 읽기
Y = 상위 바이트, A = 하위 바이트
```

### MOVW dp, YA (0xDA)
```
Operation: [dp] = A, [dp+1] = Y
Cycles: 5
Flags: -

16비트 Direct Page 쓰기
```

### INCW dp (0x3A)
```
Operation: word = [dp] | ([dp+1] << 8)
           word++
           [dp] = word & 0xFF
           [dp+1] = (word >> 8) & 0xFF
Cycles: 6
Flags: N, Z
```

### DECW dp (0x1A)
```
Operation: word = [dp] | ([dp+1] << 8)
           word--
           [dp] = word & 0xFF
           [dp+1] = (word >> 8) & 0xFF
Cycles: 6
Flags: N, Z
```

### ADDW YA, dp (0x7A)
```
Operation: YA = YA + [dp_word]
Cycles: 5
Flags: N, V, H, Z, C

16비트 덧셈
```

### SUBW YA, dp (0x9A)
```
Operation: YA = YA - [dp_word]
Cycles: 5
Flags: N, V, H, Z, C

16비트 뺄셈
```

### CMPW YA, dp (0x5A)
```
Operation: temp = YA - [dp_word]
Cycles: 4
Flags: N, Z, C

16비트 비교 (YA는 변경 안 됨)
```

---

## 비교 명령어 (CMP)

### CMP A, #imm (0x68)
```
Operation: temp = A - operand
Flags:
  N = bit 7 of (A - operand)
  Z = (A == operand) ? 1 : 0
  C = (A >= operand) ? 1 : 0  [unsigned comparison]
  
Note: A 레지스터는 변경되지 않음
```

#### 예제 1: A >= operand
```
A = 0x56, operand = 0x01
temp = 0x56 - 0x01 = 0x55

결과:
  N = 0 (0x55의 bit 7 = 0)
  Z = 0 (0x56 != 0x01)
  C = 1 (0x56 >= 0x01, unsigned)
```

#### 예제 2: A < operand
```
A = 0x30, operand = 0x80
temp = 0x30 - 0x80 = 0xB0 (underflow)

결과:
  N = 1 (0xB0의 bit 7 = 1)
  Z = 0 (0x30 != 0x80)
  C = 0 (0x30 < 0x80, unsigned)
```

#### 예제 3: A == operand
```
A = 0x56, operand = 0x56
temp = 0x56 - 0x56 = 0x00

결과:
  N = 0 (0x00의 bit 7 = 0)
  Z = 1 (0x56 == 0x56)
  C = 1 (0x56 >= 0x56, unsigned)
```

### CMP A, dp (0x64)
```
Operation: temp = A - [dp]
Flags: Same as CMP A, #imm
```

### CMP A, dp+X (0x74)
```
Operation: temp = A - [dp+X]
Flags: Same as CMP A, #imm
```

### CMP X, #imm (0xC8)
```
Operation: temp = X - operand
Flags: Same as CMP A, but uses X register
```

### CMP Y, #imm (0xAD)
```
Operation: temp = Y - operand
Flags: Same as CMP A, but uses Y register
```

---

## 분기 명령어 (Branch Instructions)

### BEQ rel (0xF0) - Branch if Equal
```
Operation: if (Z == 1) PC = PC + signed_offset
Cycles: 2 if not taken, 4 if taken
```

### BNE rel (0xD0) - Branch if Not Equal
```
Operation: if (Z == 0) PC = PC + signed_offset
Cycles: 2 if not taken, 4 if taken
```

### BCS rel (0xB0) - Branch if Carry Set
```
Operation: if (C == 1) PC = PC + signed_offset
Cycles: 2 if not taken, 4 if taken
```

### BCC rel (0x90) - Branch if Carry Clear
```
Operation: if (C == 0) PC = PC + signed_offset
Cycles: 2 if not taken, 4 if taken
```

### BMI rel (0x30) - Branch if Minus (Negative)
```
Operation: if (N == 1) PC = PC + signed_offset
Cycles: 2 if not taken, 4 if taken
```

### BPL rel (0x10) - Branch if Plus (Positive)
```
Operation: if (N == 0) PC = PC + signed_offset
Cycles: 2 if not taken, 4 if taken
```

### BVS rel (0x70) - Branch if Overflow Set
```
Operation: if (V == 1) PC = PC + signed_offset
Cycles: 2 if not taken, 4 if taken
```

### BVC rel (0x50) - Branch if Overflow Clear
```
Operation: if (V == 0) PC = PC + signed_offset
Cycles: 2 if not taken, 4 if taken
```

### BRA rel (0x2F) - Branch Always
```
Operation: PC = PC + signed_offset
Cycles: 4
Note: signed_offset은 -128 ~ +127 범위
```

#### BRA 예제
```
PC = 0x0357, opcode = 0x2F, operand = 0xFE

signed_offset = (int8_t)0xFE = -2
new_PC = 0x0357 + 2 (opcode size) + (-2) = 0x0357

결과: 자기 자신으로 점프 (무한 루프)
```

---

## 로드/스토어 명령어

### MOV A, #imm (0xE8)
```
Operation: A = operand
Flags:
  N = bit 7 of operand
  Z = (operand == 0) ? 1 : 0
```

### MOV A, dp (0xE4)
```
Operation: A = [dp]
Flags: N, Z updated based on loaded value
```

### MOV A, dp+X (0xF4)
```
Operation: A = [dp+X]
Flags: N, Z updated based on loaded value
```

### MOV A, !abs (0xE5)
```
Operation: A = [abs]
Flags: N, Z updated based on loaded value
```

### MOV A, [dp+X] (0xE7) - Indirect indexed
```
Operation: addr = [dp+X] | ([dp+X+1] << 8)
           A = [addr]
Flags: N, Z updated based on loaded value
```

### MOV dp, A (0xC4)
```
Operation: [dp] = A
Flags: None affected
```

### MOV dp+X, A (0xD4)
```
Operation: [dp+X] = A
Flags: None affected
```

### MOV !abs, A (0xC5)
```
Operation: [abs] = A
Flags: None affected
```

---

## 산술 명령어

### ADC A, #imm (0x88) - Add with Carry
```
Operation: A = A + operand + C
Flags:
  N = bit 7 of result
  V = signed overflow occurred
  H = half-carry (bit 3 carry)
  Z = (result == 0) ? 1 : 0
  C = unsigned carry occurred
```

#### 예제
```
A = 0x50, operand = 0x60, C = 0

result = 0x50 + 0x60 + 0 = 0xB0

Flags:
  N = 1 (0xB0의 bit 7 = 1)
  V = 1 (0x50과 0x60은 양수인데 결과가 음수 - overflow)
  Z = 0 (0xB0 != 0)
  C = 0 (no unsigned overflow, 0xB0 < 0x100)
```

### SBC A, #imm (0xA8) - Subtract with Carry
```
Operation: A = A - operand - (1 - C)
Flags: N, V, H, Z, C updated
Note: C가 1이면 borrow 없음, C가 0이면 borrow 있음
```

### INC A (0xBC) - Increment Accumulator
```
Operation: A = A + 1
Flags:
  N = bit 7 of result
  Z = (result == 0) ? 1 : 0
```

### DEC A (0x9C) - Decrement Accumulator
```
Operation: A = A - 1
Flags:
  N = bit 7 of result
  Z = (result == 0) ? 1 : 0
```

### INC dp (0xAB)
```
Operation: [dp] = [dp] + 1
Flags: N, Z updated based on result
```

### DEC dp (0x8B)
```
Operation: [dp] = [dp] - 1
Flags: N, Z updated based on result
```

---

## 논리 연산 명령어

### AND A, #imm (0x28)
```
Operation: A = A & operand
Flags:
  N = bit 7 of result
  Z = (result == 0) ? 1 : 0
```

### OR A, #imm (0x08)
```
Operation: A = A | operand
Flags: N, Z updated
```

### EOR A, #imm (0x48) - Exclusive OR
```
Operation: A = A ^ operand
Flags: N, Z updated
```

---

## 비트 조작 명령어

### SET1 dp.bit (0x02, 0x22, 0x42, 0x62, 0x82, 0xA2, 0xC2, 0xE2)
```
Operation: [dp] = [dp] | (1 << bit)
Flags: None affected
```

### CLR1 dp.bit (0x12, 0x32, 0x52, 0x72, 0x92, 0xB2, 0xD2, 0xF2)
```
Operation: [dp] = [dp] & ~(1 << bit)
Flags: None affected
```

### BBC dp.bit, rel (0x13, 0x33, 0x53, 0x73, 0x93, 0xB3, 0xD3, 0xF3)
```
Branch if Bit Clear
Operation: if (([dp] & (1 << bit)) == 0) PC = PC + signed_offset
Flags: None affected
```

### BBS dp.bit, rel (0x03, 0x23, 0x43, 0x63, 0x83, 0xA3, 0xC3, 0xE3)
```
Branch if Bit Set
Operation: if (([dp] & (1 << bit)) != 0) PC = PC + signed_offset
Flags: None affected
```

---

## 스택 명령어

### PUSH A (0x2D)
```
Operation: [0x0100 + SP] = A
           SP = SP - 1
Flags: None affected
```

### PUSH X (0x4D)
```
Operation: [0x0100 + SP] = X
           SP = SP - 1
Flags: None affected
```

### PUSH Y (0x6D)
```
Operation: [0x0100 + SP] = Y
           SP = SP - 1
Flags: None affected
```

### PUSH PSW (0x0D)
```
Operation: [0x0100 + SP] = PSW
           SP = SP - 1
Flags: None affected
```

### POP A (0xAE)
```
Operation: SP = SP + 1
           A = [0x0100 + SP]
Flags: None affected
```

### POP X (0xCE)
```
Operation: SP = SP + 1
           X = [0x0100 + SP]
Flags: None affected
```

### POP Y (0xEE)
```
Operation: SP = SP + 1
           Y = [0x0100 + SP]
Flags: None affected
```

### POP PSW (0x8E)
```
Operation: SP = SP + 1
           PSW = [0x0100 + SP]
Flags: All flags restored
```

---

## 서브루틴 호출

### CALL !abs (0x3F)
```
Operation: [0x0100 + SP] = PCH
           [0x0100 + SP - 1] = PCL
           SP = SP - 2
           PC = abs
Flags: None affected
Cycles: 8
```

### RET (0x6F)
```
Operation: SP = SP + 1
           PCL = [0x0100 + SP]
           SP = SP + 1
           PCH = [0x0100 + SP]
Flags: None affected
Cycles: 5
```

### RETI (0x7F) - Return from Interrupt
```
Operation: POP PSW
           POP PC
Flags: Restored from stack
Cycles: 6
```

---

## 플래그 제어 명령어

### CLRC (0x60) - Clear Carry
```
Operation: C = 0
Flags: C cleared
```

### SETC (0x80) - Set Carry
```
Operation: C = 1
Flags: C set
```

### CLRP (0x20) - Clear Direct Page
```
Operation: P = 0 (Direct page = 0x0000)
Flags: P cleared
```

### SETP (0x40) - Set Direct Page
```
Operation: P = 1 (Direct page = 0x0100)
Flags: P set
```

### CLRV (0xE0) - Clear Overflow and Half-carry
```
Operation: V = 0, H = 0
Flags: V and H cleared
```

### EI (0xA0) - Enable Interrupts
```
Operation: I = 1
Flags: I set
```

### DI (0xC0) - Disable Interrupts
```
Operation: I = 0
Flags: I cleared
```

---

## 특수 명령어

### NOP (0x00) - No Operation
```
Operation: None
Flags: None affected
Cycles: 2
```

### SLEEP (0xEF)
```
Operation: CPU enters low-power mode until interrupt
Flags: None affected
Note: 에뮬레이터는 이를 NOP처럼 처리하거나 중단점으로 사용 가능
```

### STOP (0xFF)
```
Operation: CPU halts until hardware reset
Flags: None affected
Note: 에뮬레이터는 이를 중단점으로 사용
```

---

## 💡 디버깅 팁

### 1. CMP 명령어 검증
```cpp
// Carry 플래그는 borrow의 반대임을 주의!
uint8_t temp = a - operand;
setFlag(FLAG_C, a >= operand);  // NOT (a < operand)
setFlag(FLAG_Z, temp == 0);
setFlag(FLAG_N, temp & 0x80);
```

### 2. 분기 명령어 오프셋 계산
```cpp
// operand는 signed 8-bit
int8_t offset = (int8_t)operand;
// PC는 이미 다음 명령어를 가리키고 있음
uint16_t target = pc + offset;
```

### 3. 무한 루프 패턴
```
BRA -2  (0x2F 0xFE)  -> 자기 자신으로 점프
```
이는 일반적으로 fail 루틴이나 무한 대기 루프입니다.

---

## 참고 자료

- **Fullsnes**: https://problemkaputt.de/fullsnes.htm#snescpuoverview
- **SPC700 Instruction Set**: http://www.romhacking.net/documents/226/
- **Anomie's SNES Documents**: https://www.romhacking.net/documents/196/

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**작성자**: SNES 에뮬레이터 개발팀
