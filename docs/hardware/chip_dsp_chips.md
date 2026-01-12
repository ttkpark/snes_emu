# DSP-1/2/3/4 - Digital Signal Processor Chips

## 📋 목차
1. [개요](#개요)
2. [DSP-1](#dsp-1)
3. [DSP-2](#dsp-2)
4. [DSP-3](#dsp-3)
5. [DSP-4](#dsp-4)
6. [게임 목록](#게임-목록)

---

## 개요

**DSP-1/2/3/4**는 **수학 연산 보조 칩**으로, 주로 3D 계산, 회전, 확대/축소에 사용됩니다.

---

## DSP-1

### 기능
- **행렬 곱셈** (3D 회전)
- **벡터 내적/외적**
- **삼각함수** (sin, cos)
- **거리 계산**

### 주요 게임
- **Super Mario Kart** (1992)
- **Pilotwings** (1990)

### 레지스터
```
$3000: Command
$3001-$3003: Input
$3004-$3007: Output
```

### 명령어 예제
```
Command $01: 3D 회전
Input:
  - Matrix (3×3)
  - Vector (X, Y, Z)
Output:
  - Transformed Vector
```

---

## DSP-2

### 기능
- **비트맵 변환**
- **압축 해제**

### 주요 게임
- **Dungeon Master** (1993)

---

## DSP-3

### 기능
- **좌표 변환**
- **스프라이트 배치 계산**

### 주요 게임
- **SD Gundam GX** (1994)

---

## DSP-4

### 기능
- **코사인 테이블**
- **3D 투영**

### 주요 게임
- **Top Gear 3000** (1995)

---

## 게임 목록

| DSP | 게임 | 용도 |
|-----|------|------|
| DSP-1 | Super Mario Kart | 코스 회전 |
| DSP-1 | Pilotwings | 비행 시뮬레이션 |
| DSP-2 | Dungeon Master | 비트맵 처리 |
| DSP-3 | SD Gundam GX | 스프라이트 계산 |
| DSP-4 | Top Gear 3000 | 3D 투영 |

---

## 구현 가이드

```cpp
class DSP1 {
public:
    void executeCommand(uint8_t cmd) {
        switch (cmd) {
            case 0x01: // 3D Rotation
                matrixMultiply();
                break;
            case 0x0C: // Distance
                calculateDistance();
                break;
        }
    }
};
```

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete
