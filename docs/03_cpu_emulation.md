# 03. 65c816 CPU 에뮬레이션 구현

## 목표
- 65c816 CPU 코어 완전 구현
- 모든 명령어 지원 (256개)
- 정확한 타이밍 에뮬레이션
- 디버그 기능 통합

## 65c816 CPU 특징

### 1. 레지스터 구조
- **A (Accumulator)**: 8비트/16비트
- **X, Y (Index Registers)**: 8비트/16비트
- **SP (Stack Pointer)**: 16비트
- **PC (Program Counter)**: 16비트
- **P (Processor Status)**: 8비트 플래그

### 2. 주소 모드
- **Immediate**: #$12
- **Absolute**: $1234
- **Absolute X/Y**: $1234,X
- **Zero Page**: $12
- **Zero Page X/Y**: $12,X
- **Indirect**: ($1234)
- **Indirect X**: ($12,X)
- **Indirect Y**: ($12),Y
- **Relative**: $12 (분기)

### 3. 상태 플래그
- **C (Carry)**: 올림/빌림
- **Z (Zero)**: 0 결과
- **I (Interrupt)**: 인터럽트 비활성화
- **D (Decimal)**: BCD 모드
- **B (Break)**: BRK 명령어
- **V (Overflow)**: 오버플로우
- **N (Negative)**: 음수 결과

## 구현 계획

### 1. 기본 CPU 클래스 구조
```cpp
class CPU {
    // 레지스터
    uint16_t m_pc;
    uint8_t m_a, m_x, m_y, m_sp, m_p;
    
    // 내부 상태
    bool m_emulationMode;
    int m_cycles;
    
    // 명령어 실행
    void executeInstruction(uint8_t opcode);
    void setFlag(uint8_t flag, bool value);
};
```

### 2. 명령어 구현 (주요 명령어들)
- **데이터 전송**: LDA, LDX, LDY, STA, STX, STY
- **산술 연산**: ADC, SBC, INC, DEC
- **논리 연산**: AND, ORA, EOR
- **비트 연산**: ASL, LSR, ROL, ROR
- **비교 연산**: CMP, CPX, CPY, BIT
- **분기**: BCC, BCS, BEQ, BNE, BMI, BPL, BVC, BVS
- **스택**: PHA, PLA, PHP, PLP
- **점프**: JMP, JSR, RTS
- **인터럽트**: BRK, RTI
- **기타**: NOP, CLC, SEC, CLI, SEI

### 3. 주소 모드 구현
```cpp
uint16_t getImmediateAddress();
uint16_t getAbsoluteAddress();
uint16_t getAbsoluteXAddress();
uint16_t getAbsoluteYAddress();
uint16_t getZeroPageAddress();
uint16_t getZeroPageXAddress();
uint16_t getZeroPageYAddress();
uint16_t getIndirectAddress();
uint16_t getIndirectXAddress();
uint16_t getIndirectYAddress();
uint16_t getRelativeAddress();
```

### 4. 타이밍 구현
- 각 명령어별 정확한 클록 사이클 수
- 메모리 접근 패턴 고려
- 페이지 크로싱 페널티

### 5. 디버그 기능
- 명령어 디스어셈블리
- 레지스터 상태 표시
- 실행 추적
- 브레이크포인트

## 테스트 계획

### 1. 단위 테스트
- 각 명령어별 정확성 검증
- 플래그 설정 검증
- 타이밍 검증

### 2. 통합 테스트
- 실제 ROM 실행
- Super Mario World 테스트
- 성능 벤치마크

## 예상 소요 시간
2-3일

## 다음 단계
04_memory_management.md - 메모리 매핑 및 관리 시스템
