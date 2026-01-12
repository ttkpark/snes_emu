# HDMA Effects - 실전 활용 패턴

## 📋 목차
1. [개요](#개요)
2. [물결 효과](#물결-효과)
3. [그라데이션](#그라데이션)
4. [원근감](#원근감)
5. [래스터 바](#래스터-바)
6. [윈도우 애니메이션](#윈도우-애니메이션)

---

## 개요

HDMA를 사용하여 **스캔라인별로 레지스터를 변경**하여 다양한 특수 효과를 구현합니다.

---

## 물결 효과

### 배경 스크롤 변경

```asm
; BG1 가로 스크롤을 사인파로
HDMATable:
    .db 1, 0      ; Scanline 0: Offset 0
    .db 1, 2      ; Scanline 1: Offset 2
    .db 1, 4      ; Scanline 2: Offset 4
    .db 1, 5      ; ...
    .db 1, 4
    .db 1, 2
    .db 1, 0
    .db 1, -2
    ; ... 반복
    .db 0         ; End
```

---

## 그라데이션

### 팔레트 색상 변경

```cpp
// 하늘 → 지평선 그라데이션
for (int y = 0; y < 112; y++) {
    uint8_t blue = 31 - (y * 31 / 112);
    hdmaTable[y] = RGB555(0, 0, blue);
}
```

---

## 원근감

### Mode 7 확대/축소

```cpp
for (int y = 0; y < 224; y++) {
    int16_t scale = 256 * 50 / (y + 1);
    hdmaTableA[y] = scale;  // M7A
    hdmaTableD[y] = scale;  // M7D
}
```

---

## 래스터 바

### 수평선 효과

```asm
; 스캔라인 100에 밝은 선
HDMATable:
    .db 100, $00  ; 100 라인: 어두움
    .db 1, $FF    ; 1 라인: 밝음
    .db 123, $00  ; 나머지: 어두움
    .db 0
```

---

## 윈도우 애니메이션

### 원형 윈도우 (X-Ray 효과)

```cpp
for (int y = 0; y < 224; y++) {
    int centerY = 112;
    int radius = 64;
    int dy = abs(y - centerY);
    int dx = (int)sqrt(radius * radius - dy * dy);
    
    hdmaLeft[y] = 128 - dx;
    hdmaRight[y] = 128 + dx;
}
```

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete
