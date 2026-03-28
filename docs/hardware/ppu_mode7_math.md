# Mode 7 Mathematics - 회전/확대/축소 행렬 계산

## 📋 목차
1. [개요](#개요)
2. [변환 행렬](#변환-행렬)
3. [회전](#회전)
4. [확대/축소](#확대축소)
5. [원근감](#원근감)
6. [실전 예제](#실전-예제)

---

## 개요

Mode 7은 **2D 아핀 변환**을 사용하여 회전, 확대, 축소, 원근감 효과를 구현합니다.

---

## 변환 행렬

### 기본 식

```
[X']   [A B] [X - CX]   [CX]
[Y'] = [C D] [Y - CY] + [CY]

X' = A × (X - CX) + B × (Y - CY) + CX
Y' = C × (X - CX) + D × (Y - CY) + CY
```

---

## 회전

### 회전 행렬 (각도 θ)

```
[A B]   [ cos θ  sin θ]
[C D] = [-sin θ  cos θ]
```

### 계산 예제

```cpp
// θ = 45도
float angle = 45.0f * PI / 180.0f;
int16_t A = (int16_t)(cos(angle) * 256);  // 181
int16_t B = (int16_t)(sin(angle) * 256);  // 181
int16_t C = -B;  // -181
int16_t D = A;   // 181
```

---

## 확대/축소

### 확대 행렬

```
[A B]   [Scale  0    ]
[C D] = [0      Scale]
```

### 예제

```
2배 확대: A=512, B=0, C=0, D=512 (256 = 1.0)
0.5배 축소: A=128, B=0, C=0, D=128
```

---

## 원근감

HDMA로 매 스캔라인마다 Scale 값을 변경:

```cpp
for (int y = 0; y < 224; y++) {
    float distance = y + 1;
    int16_t scale = (int16_t)(256 * 100 / distance);
    hdmaTable[y] = scale;
}
```

---

## 실전 예제

### F-Zero 레이스 트랙

```cpp
// 카메라 회전 + 원근감
for (int scanline = 0; scanline < 224; scanline++) {
    float angle = cameraAngle;
    float distance = scanline + horizon;
    float scale = 256.0f / distance;
    
    int16_t A = (int16_t)(cos(angle) * scale);
    int16_t B = (int16_t)(sin(angle) * scale);
    int16_t C = -B;
    int16_t D = A;
    
    // HDMA로 각 라인마다 설정
    setMode7Matrix(scanline, A, B, C, D);
}
```

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete
