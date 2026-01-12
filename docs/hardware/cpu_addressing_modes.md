# 65C816 CPU 주소 지정 모드

## 개요

65C816 CPU는 22가지 주소 지정 모드를 지원하여 매우 유연한 명령어 세트를 제공합니다. 이 문서는 각 모드의 동작 방식과 사용 예제를 설명합니다.

---

## 1. Implied (묵시적)

### 설명
오퍼랜드가 명령어 자체에 내재되어 있습니다.

### 형식
```
INSTRUCTION
```

### 예제
```asm
DEX         ; X 레지스터 감소
INY         ; Y 레지스터 증가
NOP         ; 아무것도 하지 않음
RTS         ; 서브루틴에서 리턴
```

### 사이클
1-6 사이클 (명령어에 따라 다름)

---

## 2. Accumulator (누산기)

### 설명
누산기(A)에 직접 연산을 수행합니다.

### 형식
```
INSTRUCTION A
```

### 예제
```asm
ASL A       ; 누산기를 왼쪽으로 시프트
LSR A       ; 누산기를 오른쪽으로 시프트
ROL A       ; 누산기를 왼쪽으로 회전
ROR A       ; 누산기를 오른쪽으로 회전
```

### 사이클
2 사이클

---

## 3. Immediate (즉시)

### 설명
오퍼랜드가 명령어에 직접 포함됩니다.

### 형식
```
INSTRUCTION #value
```

### 예제
```asm
LDA #$55    ; A에 $55 로드
LDX #$1234  ; X에 $1234 로드 (16비트 모드)
CMP #$80    ; A와 $80 비교
ADC #$10    ; A에 $10 더하기
```

### 사이클
- 8비트 모드: 2 사이클
- 16비트 모드: 3 사이클

### 주의사항
```asm
; m 플래그 상태에 따라 크기 결정
SEP #$20    ; m=1 (8비트 A)
LDA #$FF    ; 1바이트 오퍼랜드

REP #$20    ; m=0 (16비트 A)
LDA #$FFFF  ; 2바이트 오퍼랜드
```

---

## 4. Absolute (절대)

### 설명
16비트 주소를 직접 지정합니다.

### 형식
```
INSTRUCTION $addr
```

### 예제
```asm
LDA $2000   ; $2000에서 로드
STA $7E0100 ; $7E0100에 저장
JMP $8000   ; $8000으로 점프
```

### 사이클
- 읽기: 4 사이클
- 쓰기: 4 사이클
- RMW: 6 사이클

---

## 5. Absolute Long (절대 롱)

### 설명
24비트 주소를 지정하여 16MB 전체 주소 공간에 액세스합니다.

### 형식
```
INSTRUCTION $bank:addr
```

### 예제
```asm
LDA $00FF1234   ; 뱅크 $00, 주소 $FF1234에서 로드
STA $7EFFFF     ; 뱅크 $7E, 주소 $FFFF에 저장
JML $018000     ; 뱅크 $01, 주소 $8000으로 점프
```

### 사이클
- 읽기: 5 사이클
- 쓰기: 5 사이클

---

## 6. Direct Page (다이렉트 페이지)

### 설명
Direct Page 레지스터(D)를 기준으로 한 8비트 오프셋을 사용합니다.

### 형식
```
INSTRUCTION $offset
```

### 예제
```asm
; D = $0000 가정
LDA $10     ; $0010에서 로드
STA $20     ; $0020에 저장

; D = $0100으로 설정
LDA #$0100
TCD
LDA $10     ; 이제 $0110에서 로드
```

### 사이클
- D의 하위 바이트가 $00: 3 사이클
- D의 하위 바이트가 $00 아님: 4 사이클

### 최적화 팁
```asm
; Direct Page를 $xx00으로 정렬하면 1 사이클 절약
LDA #$2000
TCD         ; D = $2000 (정렬됨)
LDA $10     ; 3 사이클

LDA #$2001
TCD         ; D = $2001 (정렬 안됨)
LDA $10     ; 4 사이클
```

---

## 7. Direct Page Indexed (다이렉트 페이지 인덱스)

### 설명
Direct Page 오프셋에 X 또는 Y 레지스터를 더합니다.

### 형식
```
INSTRUCTION $offset,X
INSTRUCTION $offset,Y
```

### 예제
```asm
; D = $0000, X = $05
LDA $10,X   ; $0015에서 로드
STA $20,Y   ; $0020 + Y에 저장
```

### 사이클
- D 정렬됨: 4 사이클
- D 정렬 안됨: 5 사이클

---

## 8. Absolute Indexed (절대 인덱스)

### 설명
16비트 주소에 X 또는 Y 레지스터를 더합니다.

### 형식
```
INSTRUCTION $addr,X
INSTRUCTION $addr,Y
```

### 예제
```asm
; X = $0010
LDA $2000,X ; $2010에서 로드
STA $3000,Y ; $3000 + Y에 저장
```

### 사이클
- 페이지 경계 넘지 않음: 4 사이클
- 페이지 경계 넘음: 5 사이클

---

## 9. Absolute Long Indexed (절대 롱 인덱스)

### 설명
24비트 주소에 X 레지스터를 더합니다.

### 형식
```
INSTRUCTION $bank:addr,X
```

### 예제
```asm
; X = $0100
LDA $7E0000,X   ; $7E0100에서 로드
STA $808000,X   ; $808000 + X에 저장
```

### 사이클
5 사이클

---

## 10. Stack Relative (스택 상대)

### 설명
스택 포인터(S)를 기준으로 한 오프셋을 사용합니다.

### 형식
```
INSTRUCTION $offset,S
```

### 예제
```asm
; 함수 파라미터 액세스
LDA $03,S   ; S + $03에서 로드 (첫 번째 파라미터)
LDA $05,S   ; S + $05에서 로드 (두 번째 파라미터)
```

### 사이클
4 사이클

### 사용 예: 함수 호출
```asm
; 호출자
LDA #$1234
PHA         ; 파라미터 푸시
JSR func
PLA         ; 스택 정리

func:
    LDA $03,S   ; 리턴 주소(2바이트) 건너뛰고 파라미터 액세스
    RTS
```

---

## 11. Stack Relative Indirect Indexed

### 설명
스택 상대 주소에서 포인터를 가져오고 Y를 더합니다.

### 형식
```
INSTRUCTION ($offset,S),Y
```

### 예제
```asm
; 간접 파라미터 액세스
LDA ($03,S),Y   ; [S+$03]가 가리키는 주소 + Y에서 로드
```

### 사이클
7 사이클

---

## 12. Indirect (간접)

### 설명
포인터를 사용하여 효과 주소를 결정합니다.

### 형식
```
JMP ($addr)     ; 16비트 간접
```

### 예제
```asm
; 점프 테이블
JMP ($8000)     ; $8000에 저장된 주소로 점프

; 주소 $8000: $1234
; 결과: PC = $1234
```

### 사이클
5 사이클 (JMP)

---

## 13. Absolute Indirect Long (절대 간접 롱)

### 설명
24비트 포인터를 사용합니다.

### 형식
```
JML [$addr]
```

### 예제
```asm
JML [$8000]     ; $8000-$8002에 저장된 24비트 주소로 점프
```

### 사이클
6 사이클

---

## 14. Absolute Indexed Indirect

### 설명
X 레지스터를 더한 후 포인터를 읽습니다.

### 형식
```
JMP ($addr,X)
JSR ($addr,X)
```

### 예제
```asm
; X = $04
JMP ($2000,X)   ; $2004-$2005에서 주소 읽고 점프
```

### 사이클
6 사이클

---

## 15. Direct Page Indirect (다이렉트 페이지 간접)

### 설명
Direct Page 내의 포인터를 사용합니다.

### 형식
```
INSTRUCTION ($offset)
```

### 예제
```asm
; D = $0000
; $0010: $2000
LDA ($10)   ; $2000에서 로드
```

### 사이클
- D 정렬됨: 5 사이클
- D 정렬 안됨: 6 사이클

---

## 16. Direct Page Indexed Indirect

### 설명
X를 더한 후 Direct Page에서 포인터를 읽습니다.

### 형식
```
INSTRUCTION ($offset,X)
```

### 예제
```asm
; D = $0000, X = $02
; $0012: $3000
LDA ($10,X) ; $3000에서 로드
```

### 사이클
- D 정렬됨: 6 사이클
- D 정렬 안됨: 7 사이클

---

## 17. Direct Page Indirect Indexed

### 설명
Direct Page에서 포인터를 읽고 Y를 더합니다.

### 형식
```
INSTRUCTION ($offset),Y
```

### 예제
```asm
; D = $0000, Y = $10
; $0020: $4000
LDA ($20),Y ; $4010에서 로드
```

### 사이클
- D 정렬됨, 페이지 경계 안 넘음: 5 사이클
- D 정렬됨, 페이지 경계 넘음: 6 사이클
- D 정렬 안됨: +1 사이클

---

## 18. Direct Page Indirect Long

### 설명
Direct Page에서 24비트 포인터를 읽습니다.

### 형식
```
INSTRUCTION [$offset]
```

### 예제
```asm
; D = $0000
; $0010-$0012: $7E0100
LDA [$10]   ; $7E0100에서 로드
```

### 사이클
- D 정렬됨: 6 사이클
- D 정렬 안됨: 7 사이클

---

## 19. Direct Page Indirect Long Indexed

### 설명
Direct Page에서 24비트 포인터를 읽고 Y를 더합니다.

### 형식
```
INSTRUCTION [$offset],Y
```

### 예제
```asm
; D = $0000, Y = $50
; $0010-$0012: $7F0000
LDA [$10],Y ; $7F0050에서 로드
```

### 사이클
- D 정렬됨: 6 사이클
- D 정렬 안됨: 7 사이클

---

## 20. Program Counter Relative (PC 상대)

### 설명
프로그램 카운터에서 부호 있는 오프셋을 더합니다. 분기 명령어에 사용됩니다.

### 형식
```
INSTRUCTION label
```

### 예제
```asm
BEQ equal   ; Z=1이면 equal로 분기
BNE notequal; Z=0이면 notequal로 분기
BRA always  ; 무조건 always로 분기
```

### 오프셋 범위
-128 ~ +127 바이트

### 사이클
- 분기 안함: 2 사이클
- 분기함 (Emulation 모드): 3 사이클
- 분기함 (Native 모드): 3 사이클

---

## 21. Program Counter Relative Long

### 설명
더 큰 오프셋을 사용하는 PC 상대 분기입니다.

### 형식
```
BRL label
```

### 오프셋 범위
-32768 ~ +32767 바이트

### 예제
```asm
BRL faraway ; 멀리 떨어진 레이블로 분기
```

### 사이클
4 사이클

---

## 22. Block Move (블록 이동)

### 설명
메모리 블록을 한 뱅크에서 다른 뱅크로 이동합니다.

### 형식
```
MVN srcbank,destbank    ; X에서 Y로 증가하며 복사
MVP srcbank,destbank    ; X에서 Y로 감소하며 복사
```

### 예제
```asm
; $7E0000-$7E00FF를 $7F0000-$7F00FF로 복사
LDX #$0000      ; 소스 주소
LDY #$0000      ; 목적지 주소
LDA #$00FF      ; 복사할 바이트 수 - 1
MVN $7E,$7F     ; 복사 실행
```

### 동작
```
; MVN (증가):
for (A+1 회) {
    [Y++] = [X++]
}

; MVP (감소):
for (A+1 회) {
    [Y--] = [X--]
}
```

### 사이클
A + 1 바이트당 7 사이클

---

## 주소 지정 모드 선택 가이드

### 속도 우선
```asm
; 빠름 -> 느림
Direct Page < Absolute < Absolute Long
LDA $10     ; 3-4 사이클
LDA $2000   ; 4 사이클
LDA $7E0000 ; 5 사이클
```

### 코드 크기 우선
```asm
; 작음 -> 큼
Direct Page (2바이트) < Absolute (3바이트) < Absolute Long (4바이트)
LDA $10         ; 2바이트
LDA $2000       ; 3바이트
LDA $7E0000     ; 4바이트
```

### 유연성 우선
```asm
; 인덱스 모드 활용
LDA table,X     ; 배열 액세스
LDA ($10),Y     ; 간접 액세스
```

---

## 에뮬레이션 고려사항

### 1. m/x 플래그 추적
```cpp
// m=0: A는 16비트
// m=1: A는 8비트
// x=0: X,Y는 16비트
// x=1: X,Y는 8비트

if (m_flag) {
    // 8비트 모드
    uint8_t value = readByte(operand);
    a = (a & 0xFF00) | value;
} else {
    // 16비트 모드
    uint16_t value = readWord(operand);
    a = value;
}
```

### 2. Direct Page 정렬 체크
```cpp
uint16_t d = getD();
bool aligned = (d & 0xFF) == 0;

// 정렬되지 않으면 +1 사이클
if (!aligned) {
    cycles++;
}
```

### 3. 페이지 경계 체크
```cpp
uint16_t base_addr = readWord(pc);
uint16_t indexed_addr = base_addr + x;

// 페이지 경계를 넘으면 +1 사이클
if ((base_addr & 0xFF00) != (indexed_addr & 0xFF00)) {
    cycles++;
}
```

---

## 참고 자료

- **65C816 Programming Manual** - Western Design Center
- **SNES Development Manual Book II** - Chapter 3: Programming
- **Wiki**: https://wiki.superfamicom.org/65816-reference

---

**최종 업데이트**: 2025-12-14  
**출처**: 웹 검색 결과 종합  
**상태**: 완성










