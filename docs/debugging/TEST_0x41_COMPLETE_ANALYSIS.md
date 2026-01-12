# 테스트 0x41 (test0065) 완전 분석

## ROM 바이트 분석

### PC:0x1310 ~ 0x1320 영역

```
Address   Bytes           Instruction          Comment
─────────────────────────────────────────────────────────
0x1310    8d 00           MOV Y, #$00
0x1312    ??              
0x1313    64 01           
0x1314    ??              
0x1315    ??              
0x1316    d0 07           BNE +$07              ← 여기가 문제!
0x1318    78 01 02        CMP $01, #$02         ← 3바이트 명령어!
0x131B    d0 02           BNE +$02
0x131D    2f 03           BRA +$03
0x131F    5f 3c 03        JMP $033C             ← Fail 루틴
0x1322    e8 25           MOV A, #$25
0x1324    8d 00           MOV Y, #$00
...
```

## 문제 발견!

### APU 트레이스 vs 실제 ROM

#### APU 트레이스에서:
```
[Cyc:875356] PC:0x1316 | 22 | SET1 dp.bit
[Cyc:875357] PC:0x1318 | 68 03 | CMP A,#imm
```

#### 실제 ROM에서:
```
PC:0x1316: d0 07    ; BNE +$07
PC:0x1318: 78 01 02 ; CMP $01, #$02
```

**완전히 다릅니다!**

### 원인 분석

1. **PC:0x1316은 BNE 명령어입니다**
   - Opcode: 0xD0 (BNE)
   - Operand: 0x07 (상대 주소)
   - PC+2+0x07 = 0x131F

2. **APU 트레이스가 잘못된 PC를 표시하고 있습니다**
   - 트레이스는 PC:0x1316에 0x22가 있다고 했지만
   - 실제 ROM에는 0xD0가 있습니다

3. **실제 실행된 명령어**:

```
PC:0x1313 | 64 01        ; CMP $01, #??? (2바이트 명령어)
          ; But trace shows "64 01"
          ; ROM shows nothing at 0x1313!
```

### ROM 주소 오프셋 문제!

**SPC RAM 주소 0x1310은 ROM의 어디인가?**

spctest.sfc 구조:
- CPU ROM: 0x008000 ~
- SPC RAM에 로드될 데이터: 어디선가 복사됨

**SPC RAM 0x1310이 ROM의 어느 오프셋에 해당하는지 확인해야 합니다!**

---

## 재분석: 실제 테스트 코드

test0065 (0x41) 소스:

```asm
test0065:
        mov a, #$65              ; A = 0x65
        mov y, #$00              ; Y = 0x00
        call init_test           ; 테스트 초기화
        mov $01, #$7f            ; Memory[$01] = 0x7F
        mov a, #$01              ; A = 0x01
        push a                   ; PSW = 0x01 저장
        mov a, #$00              ; A = 0x00
        mov x, #$34              ; X = 0x34
        mov y, #$56              ; Y = 0x56
        pop psw                  ; PSW = 0x01 복원
        adc a, $01               ; A = A + $01 + C
                                 ; A = 0x00 + 0x7F + 1 = 0x80
        call save_results        ; 결과 저장
        cmp a, #$c8              ; PSW(result) should be 0xC8
        bne .to_fail
        cmp result_a, #$80       ; A result should be 0x80
        bne .to_fail
        cmp x, #$34              ; X unchanged
        bne .to_fail
        cmp y, #$56              ; Y unchanged
        bne .to_fail
        cmp $01, #$7f            ; Memory[$01] unchanged
        bne .to_fail
        bra .next_test
    .to_fail:
        jmp fail
    .next_test:
```

## 실제 실행 vs 예상

### 예상 (테스트 소스):

```
1. MOV A, #$65
2. MOV Y, #$00
3. CALL init_test
4. MOV $01, #$7F
5. MOV A, #$01
6. PUSH A
7. MOV A, #$00
8. MOV X, #$34
9. MOV Y, #$56
10. POP PSW              ; PSW = 0x01
11. ADC A, $01           ; A = 0x00 + 0x7F + 1 = 0x80
12. CALL save_results
13. CMP A, #$C8          ; Check PSW
14. BNE .to_fail
15. CMP result_a, #$80
16. BNE .to_fail
```

### 실제 실행 (APU 트레이스):

```
PC:0x1306 | 8f 12 01    | MOV $01, #$12    ← WRONG! Should be #$7F
PC:0x1309 | e8 00       | MOV A, #$00
PC:0x130B | 2d          | PUSH A
PC:0x130C | e8 12       | MOV A, #$12      ← WRONG! Should be #$01 then #$00
PC:0x130E | cd 34       | MOV X, #$34      ✓
PC:0x1310 | 8d 56       | MOV Y, #$56      ✓
PC:0x1312 | 8e          | POP PSW          ✓
PC:0x1313 | 64 01       | CMP $01, #???    ← WRONG instruction!
```

## 결론

### 문제 1: SPC RAM 주소 매핑

APU 트레이스의 PC:0x1313은 실제로 다른 코드를 가리키고 있습니다.

**SPC RAM이 제대로 로드되지 않았습니다!**

### 문제 2: 테스트 데이터 불일치

- 예상: `MOV $01, #$7F`
- 실제: `MOV $01, #$12`

**이전 테스트의 데이터가 남아있거나, 잘못된 테스트가 실행되었습니다!**

### 문제 3: init_test 실패?

Test0065는 `call init_test`로 시작해야 하는데, 트레이스에는:

```
PC:0x1306 | 8f 12 01  ; 바로 MOV 시작
```

**init_test가 호출되지 않았습니다!**

---

## 진짜 문제 발견!

### 테스트 번호 확인

APU 트레이스에서:
```
APU: SPC700 wrote port 2 = 0x41 (old=0x40, PC=0x321)
```

포트 2에 0x41이 쓰였습니다. 이것은 테스트 번호입니다.

하지만 **init_test에서 포트 2에 테스트 번호를 씁니다!**

```asm
init_test:
    inc test_num
    mov $F6, test_num    ; Write test number to port 2
    ...
```

그러므로:
- 포트 2 = 0x41은 **테스트 0x41이 시작되었음을 의미**
- 하지만 그 직후 실행된 코드가 잘못되었습니다

### 실제 실행 흐름

```
PC:0x1306 | 테스트 0x41 시작
          | ... (잘못된 코드)
PC:0x1313 | CMP $01, #$3F
          | Result: Z=0 (0x12 ≠ 0x3F)
PC:0x1316 | ??? (disassembly mismatch)
PC:0x1318 | CMP A, #$03
          | Result: Z=0 (0x12 ≠ 0x03)
PC:0x131A | BNE +$14
          | Z=0, so branch!
PC:0x1330 | JMP $033C (fail)
```

---

## 최종 판정

### 실패 원인

1. **SPC RAM이 제대로 로드되지 않았음**
   - ROM에서 SPC RAM으로 복사하는 과정에 오류
   - 또는 잘못된 ROM 오프셋

2. **테스트 코드 불일치**
   - PC:0x1306의 코드가 test0065 소스와 다름
   - `MOV $01, #$12` vs 예상 `MOV $01, #$7F`

3. **디스어셈블리 혼란**
   - APU 트레이스의 PC 주소가 정확하지 않음
   - 또는 SPC RAM과 ROM 매핑 문제

### 다음 단계

1. **SPC RAM 덤프**: 0x1300 ~ 0x1400 영역
2. **ROM 로드 검증**: spctest.sfc가 SPC RAM에 어떻게 로드되는지
3. **init_test 추적**: 테스트 초기화가 제대로 되는지
4. **포트 프로토콜 검증**: CPU가 SPC에 올바른 데이터를 전송했는지

---

**결론**: 테스트 0x41은 **SPC RAM 로드 문제** 또는 **ROM 매핑 문제**로 실패했습니다.  
테스트 코드 자체는 올바르지만, 실행된 코드가 예상과 다릅니다.










