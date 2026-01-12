# 65C816 완전한 명령어 세트

## 개요

65C816은 8/16비트 하이브리드 CPU로, **256개의 opcode**를 가지고 있습니다. 이 문서는 모든 명령어의 동작, 플래그 변화, 사이클 수를 상세히 설명합니다.

---

## CPU 모드

### Native Mode vs Emulation Mode

65C816은 두 가지 모드로 동작합니다:

**Native Mode** (e=0):
- 16비트 A, X, Y 레지스터 지원
- 모든 주소 지정 모드 사용 가능
- 24비트 주소 공간 (16MB)

**Emulation Mode** (e=1):
- 6502 호환 모드
- 8비트 레지스터만
- 16비트 주소 공간 (64KB)
- Stack은 항상 $01xx

### m/x 플래그

**m 플래그** (P의 bit 5):
- m=0: A는 16비트
- m=1: A는 8비트

**x 플래그** (P의 bit 4):
- x=0: X, Y는 16비트
- x=1: X, Y는 8비트

---

## 프로세서 상태 레지스터 (P)

```
Native Mode:
7  bit  0
---- ----
NVMX DIZC
|||| ||||
|||| |||+- Carry
|||| ||+-- Zero
|||| |+--- IRQ Disable
|||| +---- Decimal Mode
|||+------ Index Register Size (0=16bit, 1=8bit)
||+------- Memory/Accumulator Size (0=16bit, 1=8bit)
|+-------- Overflow
+--------- Negative

Emulation Mode:
7  bit  0
---- ----
NV1B DIZC
  |||
  ||+------ Break (BRK 명령어)
  |+------- Unused (always 1)
```

---

## 로드/스토어 명령어

### LDA - Load Accumulator

| Opcode | Mode | Syntax | Bytes | Cycles | Flags |
|--------|------|--------|-------|--------|-------|
| A9 | Immediate | LDA #$nn | 2-3 | 2-3 | N, Z |
| A5 | Direct Page | LDA $nn | 2 | 3-4 | N, Z |
| B5 | DP Indexed,X | LDA $nn,X | 2 | 4-5 | N, Z |
| AD | Absolute | LDA $nnnn | 3 | 4 | N, Z |
| BD | Absolute,X | LDA $nnnn,X | 3 | 4-5 | N, Z |
| B9 | Absolute,Y | LDA $nnnn,Y | 3 | 4-5 | N, Z |
| AF | Absolute Long | LDA $nnnnnn | 4 | 5 | N, Z |
| BF | Absolute Long,X | LDA $nnnnnn,X | 4 | 5 | N, Z |
| A1 | DP Indexed Indirect,X | LDA ($nn,X) | 2 | 6-7 | N, Z |
| B1 | DP Indirect Indexed,Y | LDA ($nn),Y | 2 | 5-6 | N, Z |
| B7 | DP Indirect Long Indexed,Y | LDA [$nn],Y | 2 | 6-7 | N, Z |
| A3 | Stack Relative | LDA $nn,S | 2 | 4 | N, Z |
| B3 | SR Indirect Indexed,Y | LDA ($nn,S),Y | 2 | 7 | N, Z |

**동작**:
```
A = [address]
N = bit 7 (or 15) of result
Z = (result == 0)
```

**사이클 주의사항**:
- +1 if m=0 (16-bit)
- +1 if DP low byte != 0
- +1 if page boundary crossed (indexed modes)

### LDX - Load X Register

| Opcode | Mode | Syntax | Bytes | Cycles | Flags |
|--------|------|--------|-------|--------|-------|
| A2 | Immediate | LDX #$nn | 2-3 | 2-3 | N, Z |
| A6 | Direct Page | LDX $nn | 2 | 3-4 | N, Z |
| B6 | DP Indexed,Y | LDX $nn,Y | 2 | 4-5 | N, Z |
| AE | Absolute | LDX $nnnn | 3 | 4 | N, Z |
| BE | Absolute,Y | LDX $nnnn,Y | 3 | 4-5 | N, Z |

**동작**: 
```
X = [address]
```

### LDY - Load Y Register

| Opcode | Mode | Syntax | Bytes | Cycles | Flags |
|--------|------|--------|-------|--------|-------|
| A0 | Immediate | LDY #$nn | 2-3 | 2-3 | N, Z |
| A4 | Direct Page | LDY $nn | 2 | 3-4 | N, Z |
| B4 | DP Indexed,X | LDY $nn,X | 2 | 4-5 | N, Z |
| AC | Absolute | LDY $nnnn | 3 | 4 | N, Z |
| BC | Absolute,X | LDY $nnnn,X | 3 | 4-5 | N, Z |

### STA - Store Accumulator

| Opcode | Mode | Syntax | Bytes | Cycles |
|--------|------|--------|-------|--------|
| 85 | Direct Page | STA $nn | 2 | 3-4 |
| 95 | DP Indexed,X | STA $nn,X | 2 | 4-5 |
| 8D | Absolute | STA $nnnn | 3 | 4 |
| 9D | Absolute,X | STA $nnnn,X | 3 | 5 |
| 99 | Absolute,Y | STA $nnnn,Y | 3 | 5 |
| 8F | Absolute Long | STA $nnnnnn | 4 | 5 |
| 9F | Absolute Long,X | STA $nnnnnn,X | 4 | 5 |
| 81 | DP Indexed Indirect,X | STA ($nn,X) | 2 | 6-7 |
| 91 | DP Indirect Indexed,Y | STA ($nn),Y | 2 | 6 |
| 97 | DP Indirect Long Indexed,Y | STA [$nn],Y | 2 | 6-7 |
| 83 | Stack Relative | STA $nn,S | 2 | 4 |
| 93 | SR Indirect Indexed,Y | STA ($nn,S),Y | 2 | 7 |

**동작**: 
```
[address] = A
```

### STX, STY, STZ

STX/STY는 LDX/LDY와 유사한 주소 모드.
STZ는 메모리를 0으로 설정 (6502에는 없음).

---

## 산술 명령어

### ADC - Add with Carry

| Opcode | Mode | Cycles | Flags |
|--------|------|--------|-------|
| 69 | Immediate | 2-3 | N, V, Z, C |
| 65 | Direct Page | 3-4 | N, V, Z, C |
| ... | (LDA와 같은 주소 모드) | | |

**동작** (Binary Mode):
```
result = A + operand + C
A = result
N = bit 7 (or 15) of result
V = signed overflow occurred
Z = (result == 0)
C = unsigned carry occurred
```

**동작** (Decimal Mode, D=1):
```
BCD addition
Each nibble: 0-9
Result adjusted for BCD
```

**Decimal Mode 예제**:
```
A = $09, operand = $01, C = 0
Result = $09 + $01 = $10 (in BCD)
A = $10, C = 0

A = $99, operand = $01, C = 0
Result = $99 + $01 = $00 (in BCD), C = 1
A = $00, C = 1
```

### SBC - Subtract with Carry

| Opcode | Mode | Cycles | Flags |
|--------|------|--------|-------|
| E9 | Immediate | 2-3 | N, V, Z, C |
| E5 | Direct Page | 3-4 | N, V, Z, C |
| ... | (LDA와 같은 주소 모드) | | |

**동작** (Binary Mode):
```
result = A - operand - (1 - C)
A = result
N = bit 7 (or 15) of result
V = signed overflow occurred
Z = (result == 0)
C = no borrow occurred (A >= operand)
```

**주의**: C는 borrow의 **반대**입니다!
- C=1: borrow 없음
- C=0: borrow 발생

### INC - Increment

| Opcode | Mode | Syntax | Cycles | Flags |
|--------|------|--------|--------|-------|
| 1A | Accumulator | INC A | 2 | N, Z |
| E6 | Direct Page | INC $nn | 5-6 | N, Z |
| F6 | DP Indexed,X | INC $nn,X | 6-7 | N, Z |
| EE | Absolute | INC $nnnn | 6 | N, Z |
| FE | Absolute,X | INC $nnnn,X | 7 | N, Z |

**동작**:
```
[address] = [address] + 1
```

### DEC - Decrement

(INC와 동일한 주소 모드)

### INX, INY, DEX, DEY

**Implied 모드**:
```
INX: X = X + 1  (Opcode E8, 2 cycles)
INY: Y = Y + 1  (Opcode C8, 2 cycles)
DEX: X = X - 1  (Opcode CA, 2 cycles)
DEY: Y = Y - 1  (Opcode 88, 2 cycles)
```

---

## 논리 명령어

### AND - Logical AND

| Opcode | Mode | Cycles | Flags |
|--------|------|--------|-------|
| 29 | Immediate | 2-3 | N, Z |
| 25 | Direct Page | 3-4 | N, Z |
| ... | (LDA와 같은 주소 모드) | | |

**동작**:
```
A = A & operand
```

### ORA - Logical OR

(AND와 동일한 주소 모드)

**동작**:
```
A = A | operand
```

### EOR - Exclusive OR

(AND와 동일한 주소 모드)

**동작**:
```
A = A ^ operand
```

### BIT - Bit Test

| Opcode | Mode | Syntax | Cycles | Flags |
|--------|------|--------|--------|-------|
| 89 | Immediate | BIT #$nn | 2-3 | Z |
| 24 | Direct Page | BIT $nn | 3-4 | N, V, Z |
| 2C | Absolute | BIT $nnnn | 4 | N, V, Z |
| 34 | DP Indexed,X | BIT $nn,X | 4-5 | N, V, Z |
| 3C | Absolute,X | BIT $nnnn,X | 4-5 | N, V, Z |

**동작** (Immediate):
```
Z = (A & operand) == 0
```

**동작** (Memory):
```
Z = (A & operand) == 0
N = bit 7 (or 15) of operand
V = bit 6 (or 14) of operand
```

---

## 시프트 및 회전

### ASL - Arithmetic Shift Left

| Opcode | Mode | Syntax | Cycles | Flags |
|--------|------|--------|--------|-------|
| 0A | Accumulator | ASL A | 2 | N, Z, C |
| 06 | Direct Page | ASL $nn | 5-6 | N, Z, C |
| 0E | Absolute | ASL $nnnn | 6 | N, Z, C |
| 16 | DP Indexed,X | ASL $nn,X | 6-7 | N, Z, C |
| 1E | Absolute,X | ASL $nnnn,X | 7 | N, Z, C |

**동작**:
```
C = bit 7 (or 15)
result = value << 1
N = bit 7 (or 15) of result
Z = (result == 0)
```

### LSR - Logical Shift Right

(ASL과 동일한 주소 모드)

**동작**:
```
C = bit 0
result = value >> 1
N = 0 (always)
Z = (result == 0)
```

### ROL - Rotate Left

(ASL과 동일한 주소 모드)

**동작**:
```
temp = bit 7 (or 15)
result = (value << 1) | C
C = temp
N = bit 7 (or 15) of result
Z = (result == 0)
```

### ROR - Rotate Right

(ASL과 동일한 주소 모드)

**동작**:
```
temp = bit 0
result = (value >> 1) | (C << 7 or 15)
C = temp
N = C (old carry becomes bit 7/15)
Z = (result == 0)
```

---

## 비교 명령어

### CMP - Compare Accumulator

| Opcode | Mode | Cycles | Flags |
|--------|------|--------|-------|
| C9 | Immediate | 2-3 | N, Z, C |
| C5 | Direct Page | 3-4 | N, Z, C |
| ... | (LDA와 같은 주소 모드) | | |

**동작**:
```
temp = A - operand
N = bit 7 (or 15) of temp
Z = (A == operand)
C = (A >= operand)  [unsigned]
```

**A는 변경되지 않습니다!**

### CPX - Compare X Register

| Opcode | Mode | Cycles | Flags |
|--------|------|--------|-------|
| E0 | Immediate | 2-3 | N, Z, C |
| E4 | Direct Page | 3-4 | N, Z, C |
| EC | Absolute | 4 | N, Z, C |

### CPY - Compare Y Register

| Opcode | Mode | Cycles | Flags |
|--------|------|--------|-------|
| C0 | Immediate | 2-3 | N, Z, C |
| C4 | Direct Page | 3-4 | N, Z, C |
| CC | Absolute | 4 | N, Z, C |

---

## 분기 명령어

### 조건부 분기

| Opcode | Mnemonic | Condition | Cycles |
|--------|----------|-----------|--------|
| 90 | BCC | C=0 (Carry Clear) | 2-4 |
| B0 | BCS | C=1 (Carry Set) | 2-4 |
| F0 | BEQ | Z=1 (Equal) | 2-4 |
| 30 | BMI | N=1 (Minus) | 2-4 |
| D0 | BNE | Z=0 (Not Equal) | 2-4 |
| 10 | BPL | N=0 (Plus) | 2-4 |
| 50 | BVC | V=0 (Overflow Clear) | 2-4 |
| 70 | BVS | V=1 (Overflow Set) | 2-4 |

**Syntax**: `BCC label`

**Offset**: -128 ~ +127 (signed 8-bit)

**Cycles**:
- 2: Branch not taken
- 3: Branch taken (Native mode)
- 4: Branch taken (Emulation mode)

### BRA - Branch Always

| Opcode | Mnemonic | Cycles |
|--------|----------|--------|
| 80 | BRA | 3-4 |

**동작**: 항상 분기 (6502에는 없음)

### BRL - Branch Long

| Opcode | Mnemonic | Offset | Cycles |
|--------|----------|--------|--------|
| 82 | BRL | 16-bit | 4 |

**Offset**: -32768 ~ +32767

---

## 점프 및 서브루틴

### JMP - Jump

| Opcode | Mode | Syntax | Cycles |
|--------|------|--------|--------|
| 4C | Absolute | JMP $nnnn | 3 |
| 6C | Indirect | JMP ($nnnn) | 5 |
| 7C | Absolute Indexed Indirect | JMP ($nnnn,X) | 6 |
| 5C | Absolute Long | JMP $nnnnnn | 4 |
| DC | Absolute Indirect Long | JMP [$nnnn] | 6 |

### JML - Jump Long

| Opcode | Mode | Cycles |
|--------|------|--------|
| 5C | Absolute Long | 4 |
| DC | Absolute Indirect Long | 6 |

**동작**: 24비트 주소로 점프

### JSR - Jump to Subroutine

| Opcode | Mode | Syntax | Cycles |
|--------|------|--------|--------|
| 20 | Absolute | JSR $nnnn | 6 |
| FC | Absolute Indexed Indirect | JSR ($nnnn,X) | 8 |
| 22 | Absolute Long | JSL $nnnnnn | 8 |

**동작** (JSR):
```
Push PC+2 (return address)
PC = address
```

**동작** (JSL):
```
Push PBR (Program Bank)
Push PC+3 (return address)
PC = address
PBR = bank
```

### RTS - Return from Subroutine

| Opcode | Mnemonic | Cycles |
|--------|----------|--------|
| 60 | RTS | 6 |

**동작**:
```
Pull PC
PC = PC + 1
```

### RTL - Return from Subroutine Long

| Opcode | Mnemonic | Cycles |
|--------|----------|--------|
| 6B | RTL | 6 |

**동작**:
```
Pull PC
Pull PBR
```

---

## 스택 명령어

### PHA, PHX, PHY - Push

| Opcode | Mnemonic | Cycles |
|--------|----------|--------|
| 48 | PHA | 3-4 |
| DA | PHX | 3-4 |
| 5A | PHY | 3-4 |

**동작**:
```
Push register to stack
SP = SP - 1 (or -2 if 16-bit)
```

### PLA, PLX, PLY - Pull

| Opcode | Mnemonic | Cycles | Flags |
|--------|----------|--------|-------|
| 68 | PLA | 4-5 | N, Z |
| FA | PLX | 4-5 | N, Z |
| 7A | PLY | 4-5 | N, Z |

**동작**:
```
SP = SP + 1 (or +2 if 16-bit)
Pull from stack to register
```

### PHD, PLD - Push/Pull Direct Page

| Opcode | Mnemonic | Cycles |
|--------|----------|--------|
| 0B | PHD | 4 |
| 2B | PLD | 5 |

### PHB, PLB - Push/Pull Data Bank

| Opcode | Mnemonic | Cycles |
|--------|----------|--------|
| 8B | PHB | 3 |
| AB | PLB | 4 |

### PHK - Push Program Bank

| Opcode | Mnemonic | Cycles |
|--------|----------|--------|
| 4B | PHK | 3 |

### PHP, PLP - Push/Pull Processor Status

| Opcode | Mnemonic | Cycles |
|--------|----------|--------|
| 08 | PHP | 3 |
| 28 | PLP | 4 |

---

## 레지스터 전송

### Transfer Instructions

| Opcode | Mnemonic | From→To | Cycles | Flags |
|--------|----------|---------|--------|-------|
| AA | TAX | A→X | 2 | N, Z |
| A8 | TAY | A→Y | 2 | N, Z |
| BA | TSX | S→X | 2 | N, Z |
| 8A | TXA | X→A | 2 | N, Z |
| 9A | TXS | X→S | 2 | - |
| 98 | TYA | Y→A | 2 | N, Z |
| 5B | TCD | C(A)→D | 2 | N, Z |
| 1B | TCS | C(A)→S | 2 | - |
| 7B | TDC | D→C(A) | 2 | N, Z |
| 3B | TSC | S→C(A) | 2 | N, Z |
| EB | XBA | swap A bytes | 3 | N, Z |

**XBA 예제**:
```
A = $1234
XBA
A = $3412
```

---

## 플래그 제어

### 플래그 설정/클리어

| Opcode | Mnemonic | Operation | Cycles |
|--------|----------|-----------|--------|
| 18 | CLC | C = 0 | 2 |
| D8 | CLD | D = 0 | 2 |
| 58 | CLI | I = 0 | 2 |
| B8 | CLV | V = 0 | 2 |
| 38 | SEC | C = 1 | 2 |
| F8 | SED | D = 1 | 2 |
| 78 | SEI | I = 1 | 2 |

### REP - Reset Processor Status Bits

| Opcode | Mnemonic | Syntax | Cycles |
|--------|----------|--------|--------|
| C2 | REP | REP #$nn | 3 |

**동작**:
```
P = P & ~operand
```

**예제**:
```asm
REP #$30    ; Clear m and x (16-bit A, X, Y)
REP #$20    ; Clear m only (16-bit A)
```

### SEP - Set Processor Status Bits

| Opcode | Mnemonic | Syntax | Cycles |
|--------|----------|--------|--------|
| E2 | SEP | SEP #$nn | 3 |

**동작**:
```
P = P | operand
```

**예제**:
```asm
SEP #$30    ; Set m and x (8-bit A, X, Y)
SEP #$20    ; Set m only (8-bit A)
```

---

## 블록 이동

### MVN - Block Move Negative

| Opcode | Syntax | Cycles |
|--------|--------|--------|
| 54 | MVN srcbank,destbank | 7 per byte |

**동작**:
```
do {
    [DBR:Y] = [srcbank:X]
    X = X + 1
    Y = Y + 1
    A = A - 1
} while (A != 0xFFFF)
```

**Setup**:
```
A = number of bytes - 1
X = source offset
Y = destination offset
```

**예제**:
```asm
LDA #$00FF     ; Transfer 256 bytes
LDX #$0000     ; Source offset
LDY #$0000     ; Dest offset
MVN $7E,$7F    ; Move from $7E:xxxx to $7F:xxxx
```

### MVP - Block Move Positive

| Opcode | Syntax | Cycles |
|--------|--------|--------|
| 44 | MVP srcbank,destbank | 7 per byte |

**동작**: MVN과 동일하지만 X, Y를 감소

---

## 인터럽트 및 특수

### BRK - Software Break

| Opcode | Mnemonic | Cycles |
|--------|----------|--------|
| 00 | BRK | 7-8 |

**동작** (Native):
```
Push PBR
Push PC+2
Push P
I = 1
D = 0
PC = [$FFE6/$FFE7]
PBR = 0
```

**동작** (Emulation):
```
Push PC+2
Push P (with B=1)
I = 1
D = 0
PC = [$FFFE/$FFFF]
```

### RTI - Return from Interrupt

| Opcode | Mnemonic | Cycles |
|--------|----------|--------|
| 40 | RTI | 6-7 |

**동작** (Native):
```
Pull P
Pull PC
Pull PBR
```

### COP - Coprocessor

| Opcode | Mnemonic | Cycles |
|--------|----------|--------|
| 02 | COP | 7-8 |

**동작**: BRK와 유사하지만 벡터는 $FFE4/$FFE5

### WAI - Wait for Interrupt

| Opcode | Mnemonic | Cycles |
|--------|----------|--------|
| CB | WAI | 3+ |

**동작**: IRQ 또는 NMI까지 대기

### STP - Stop the Clock

| Opcode | Mnemonic | Cycles |
|--------|----------|--------|
| DB | STP | 3+ |

**동작**: 리셋까지 CPU 중단

### NOP - No Operation

| Opcode | Mnemonic | Cycles |
|--------|----------|--------|
| EA | NOP | 2 |

---

## WDM - Reserved

| Opcode | Mnemonic | Cycles |
|--------|----------|--------|
| 42 | WDM | 2 |

**동작**: 아무것도 안 함 (미래 확장용 예약)

---

## 5A22 특수 레지스터

SNES의 5A22 CPU는 추가 하드웨어 레지스터를 가지고 있습니다.

### 곱셈 레지스터

**$4202 - WRMPYA (Multiplicand A)**
```
8-bit unsigned multiplicand
```

**$4203 - WRMPYB (Multiplicand B)**
```
8-bit unsigned multiplier
Write triggers multiplication
Result available in 8 cycles
```

**$4216-$4217 - RDMPY (Product)**
```
16-bit result = WRMPYA × WRMPYB
Low byte: $4216
High byte: $4217
```

**예제**:
```asm
LDA #$12
STA $4202      ; Multiplicand = 18

LDA #$03
STA $4203      ; Multiplier = 3
               ; Triggers: 18 × 3 = 54

; Wait 8 cycles
NOP
NOP
NOP
NOP

LDA $4216      ; Low byte = $36
LDA $4217      ; High byte = $00
; Result = $0036 = 54
```

### 나눗셈 레지스터

**$4204-$4206 - WRDIV (Dividend)**
```
16-bit unsigned dividend
$4204: Low byte
$4205: High byte
Write to $4206 triggers division
```

**$4206 - WRDIVB (Divisor)**
```
8-bit unsigned divisor
Write triggers division
Result available in 16 cycles
```

**$4214-$4215 - RDDIV (Quotient)**
```
16-bit quotient
```

**$4216-$4217 - RDMPY (Remainder)**
```
16-bit remainder
```

**예제**:
```asm
LDA #$64       ; 100
STA $4204      ; Dividend low
LDA #$00
STA $4205      ; Dividend high = $0064

LDA #$07       ; Divisor = 7
STA $4206      ; Trigger division

; Wait 16 cycles
NOP
NOP
NOP
NOP
NOP
NOP
NOP
NOP

LDA $4214      ; Quotient low = $0E (14)
LDA $4215      ; Quotient high = $00

LDA $4216      ; Remainder low = $02 (2)
LDA $4217      ; Remainder high = $00
; 100 ÷ 7 = 14 remainder 2
```

---

## 사이클 계산 규칙

### 기본 규칙

1. **m=0 (16-bit A)**: Immediate 모드 +1 cycle
2. **x=0 (16-bit X/Y)**: X/Y Immediate +1 cycle
3. **DP low byte ≠ 0**: Direct Page 모드 +1 cycle
4. **Page boundary crossed**: Indexed 모드 +1 cycle
5. **Emulation mode**: Branch taken +1 cycle

### 예제

```asm
; Native mode, m=0, x=0
REP #$30

LDA #$1234     ; 3 cycles (2 base + 1 for m=0)
LDX #$5678     ; 3 cycles (2 base + 1 for x=0)

; DP = $2000 (low byte ≠ 0)
LDA $10        ; 4 cycles (3 base + 1 for DP)

; Page boundary
LDA $20FF,X    ; 5 cycles (4 base + 1 for boundary)
```

---

## Decimal Mode 상세

### BCD 연산

**ADC (Decimal)**:
```
A = $09, operand = $08, C = 0
Binary: $09 + $08 = $11
BCD: 9 + 8 = 17
Result: A = $17, C = 0

A = $99, operand = $01, C = 0
Binary: $99 + $01 = $9A
BCD: 99 + 1 = 100
Result: A = $00, C = 1
```

**SBC (Decimal)**:
```
A = $50, operand = $25, C = 1
Binary: $50 - $25 = $2B
BCD: 50 - 25 = 25
Result: A = $25, C = 1

A = $10, operand = $20, C = 1
Binary: $10 - $20 = $F0 (underflow)
BCD: 10 - 20 = -10 = 90 (with borrow)
Result: A = $90, C = 0
```

---

## 에뮬레이터 구현 가이드

### Opcode 디코딩

```cpp
class CPU65816 {
public:
    void executeInstruction() {
        uint8_t opcode = readByte(pc++);
        
        switch (opcode) {
            case 0xA9:  // LDA #imm
                operandLDA_Immediate();
                break;
                
            case 0xA5:  // LDA dp
                operandLDA_DirectPage();
                break;
                
            // ... 256 cases ...
        }
    }
    
private:
    void operandLDA_Immediate() {
        if (getFlag(FLAG_M)) {
            // 8-bit mode
            uint8_t value = readByte(pc++);
            a = (a & 0xFF00) | value;
            cycles += 2;
        } else {
            // 16-bit mode
            uint16_t value = readWord(pc);
            pc += 2;
            a = value;
            cycles += 3;
        }
        
        setNZ(a, getFlag(FLAG_M));
    }
    
    void setNZ(uint16_t value, bool is8bit) {
        if (is8bit) {
            setFlag(FLAG_N, value & 0x80);
            setFlag(FLAG_Z, (value & 0xFF) == 0);
        } else {
            setFlag(FLAG_N, value & 0x8000);
            setFlag(FLAG_Z, value == 0);
        }
    }
};
```

### ADC 구현 (Decimal Mode 포함)

```cpp
void CPU65816::executeADC(uint16_t operand) {
    bool is8bit = getFlag(FLAG_M);
    bool decimal = getFlag(FLAG_D);
    bool carry_in = getFlag(FLAG_C);
    
    if (decimal) {
        // BCD mode
        if (is8bit) {
            uint8_t a8 = a & 0xFF;
            uint8_t op8 = operand & 0xFF;
            
            int low = (a8 & 0x0F) + (op8 & 0x0F) + carry_in;
            if (low > 9) low += 6;
            
            int high = (a8 >> 4) + (op8 >> 4) + (low > 15 ? 1 : 0);
            if (high > 9) high += 6;
            
            uint8_t result = ((high & 0x0F) << 4) | (low & 0x0F);
            
            setFlag(FLAG_C, high > 15);
            setFlag(FLAG_Z, result == 0);
            setFlag(FLAG_N, result & 0x80);
            
            a = (a & 0xFF00) | result;
        } else {
            // 16-bit BCD (4 nibbles)
            // ... similar logic ...
        }
    } else {
        // Binary mode
        if (is8bit) {
            uint8_t a8 = a & 0xFF;
            uint8_t op8 = operand & 0xFF;
            
            uint16_t result = a8 + op8 + carry_in;
            
            setFlag(FLAG_C, result > 0xFF);
            setFlag(FLAG_Z, (result & 0xFF) == 0);
            setFlag(FLAG_N, result & 0x80);
            setFlag(FLAG_V, (~(a8 ^ op8) & (a8 ^ result)) & 0x80);
            
            a = (a & 0xFF00) | (result & 0xFF);
        } else {
            uint16_t a16 = a;
            uint16_t op16 = operand;
            
            uint32_t result = a16 + op16 + carry_in;
            
            setFlag(FLAG_C, result > 0xFFFF);
            setFlag(FLAG_Z, (result & 0xFFFF) == 0);
            setFlag(FLAG_N, result & 0x8000);
            setFlag(FLAG_V, (~(a16 ^ op16) & (a16 ^ result)) & 0x8000);
            
            a = result & 0xFFFF;
        }
    }
}
```

---

## 참고 자료

- **65816 Programming Manual**: Western Design Center
- **Fullsnes**: https://problemkaputt.de/fullsnes.htm#cpu65xx
- **65816 Opcode Table**: http://www.6502.org/tutorials/65c816opcodes.html
- **SNES Development Manual Book II**: Chapter 2-3

---

**최종 업데이트**: 2025-12-14  
**우선순위**: ⭐⭐⭐ CRITICAL  
**상태**: 완성










