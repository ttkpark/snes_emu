# SNES Background Modes - 상세 가이드

## 📋 목차
1. [개요](#개요)
2. [Mode 0 (4색 4레이어)](#mode-0-4색-4레이어)
3. [Mode 1 (16색 2레이어 + 4색 1레이어)](#mode-1-16색-2레이어--4색-1레이어)
4. [Mode 2 (16색 2레이어, Offset-per-tile)](#mode-2-16색-2레이어-offset-per-tile)
5. [Mode 3 (256색 1레이어 + 16색 1레이어)](#mode-3-256색-1레이어--16색-1레이어)
6. [Mode 4 (256색 1레이어 + 4색 1레이어, Offset-per-tile)](#mode-4-256색-1레이어--4색-1레이어-offset-per-tile)
7. [Mode 5 (16색 2레이어, Hi-Res)](#mode-5-16색-2레이어-hi-res)
8. [Mode 6 (16색 1레이어, Hi-Res + Offset)](#mode-6-16색-1레이어-hi-res--offset)
9. [Mode 7 (256색, 회전/확대/축소)](#mode-7-256색-회전확대축소)
10. [Mode 선택 가이드](#mode-선택-가이드)

---

## 개요

SNES는 **8가지 배경 모드**를 지원하며, 각 모드는 색상 깊이와 레이어 수가 다릅니다.

### 모드 요약

| Mode | BG1 | BG2 | BG3 | BG4 | 특징 |
|------|-----|-----|-----|-----|------|
| 0 | 2bpp | 2bpp | 2bpp | 2bpp | 4개 레이어, 각 4색 |
| 1 | 4bpp | 4bpp | 2bpp | - | 2개 16색 + 1개 4색 (가장 흔함) |
| 2 | 4bpp | 4bpp | Opt | - | Offset-per-tile |
| 3 | 8bpp | 4bpp | - | - | 256색 배경 |
| 4 | 8bpp | 2bpp | Opt | - | 256색 + Offset-per-tile |
| 5 | 4bpp | 2bpp | - | - | Hi-Res (512×448) |
| 6 | 4bpp | Opt | - | - | Hi-Res + Offset-per-tile |
| 7 | 8bpp | - | - | - | 회전/확대/축소 |

---

## Mode 0 (4색 4레이어)

### 스펙
- **4개 배경**: BG1, BG2, BG3, BG4
- **색상**: 각 2bpp (4색)
- **팔레트**: 각 배경당 8개 (4색 × 8 = 32색/배경)
- **타일 크기**: 8×8

### 사용 사례
- 레이어가 많이 필요한 게임
- 색상이 적어도 되는 UI

### 예제
```asm
; Mode 0 설정
LDA #$00
STA $2105    ; BGMODE = Mode 0

; 각 배경 타일 크기 (8×8)
LDA #$00
STA $2107    ; BG1SC
STA $2108    ; BG2SC
STA $2109    ; BG3SC
STA $210A    ; BG4SC
```

---

## Mode 1 (16색 2레이어 + 4색 1레이어)

### 스펙
- **BG1, BG2**: 4bpp (16색)
- **BG3**: 2bpp (4색)
- **가장 많이 사용**되는 모드

### 우선순위
```
기본:
BG1 > BG2 > OBJ > BG3

BG3 Priority 설정 ($2105 Bit 3 = 1):
BG3 > BG1 > BG2 > OBJ
```

### 예제
```asm
; Mode 1 설정
LDA #$01
STA $2105    ; BGMODE = Mode 1

; BG3 Priority 켜기
LDA #$09     ; Mode 1 + BG3 Priority
STA $2105
```

### 실제 사용
- **Super Mario World**: 하늘(BG1), 지형(BG2), 산(BG3)
- **Chrono Trigger**: 전경(BG1), 배경(BG2), 먼 배경(BG3)

---

## Mode 2 (16색 2레이어, Offset-per-tile)

### 스펙
- **BG1, BG2**: 4bpp (16색)
- **BG3**: Offset 데이터 (화면에 표시 안 됨)

### Offset-per-tile
BG3의 타일맵 데이터를 사용하여 **BG1/BG2의 스크롤을 타일별로 변경**

```
BG3 Tilemap:
  각 타일 = X/Y 오프셋
  
BG1/BG2 렌더링 시:
  해당 위치의 BG3 오프셋만큼 이동
```

### 효과
- 물결 효과
- 열기 왜곡
- 원근감

### 예제
```asm
; Mode 2 설정
LDA #$02
STA $2105    ; BGMODE = Mode 2

; BG3에 오프셋 데이터 설정
; (각 타일에 X/Y 오프셋 값 저장)
```

---

## Mode 3 (256색 1레이어 + 16색 1레이어)

### 스펙
- **BG1**: 8bpp (256색)
- **BG2**: 4bpp (16색)

### Direct Color
**$2130 (CGWSEL) Bit 1 = 1**: Direct Color 모드
```
팔레트를 거치지 않고 타일 데이터가 직접 색상 지정:
  Bit 7-5: Blue
  Bit 4-2: Green
  Bit 1-0: Red (상위 2비트)
```

### 사용 사례
- 고품질 배경 (256색)
- 타이틀 화면

---

## Mode 4 (256색 1레이어 + 4색 1레이어, Offset-per-tile)

### 스펙
- **BG1**: 8bpp (256색)
- **BG2**: 2bpp (4색)
- **BG3**: Offset 데이터

### Mode 2와 동일한 Offset-per-tile 지원

---

## Mode 5 (16색 2레이어, Hi-Res)

### 스펙
- **BG1, BG2**: 4bpp (16색)
- **해상도**: 512×224 (가로 2배)

### 원리
```
일반:  256×224
Mode 5: 512×224 (타일이 가로로 2배)

각 타일: 16×8 (가로 늘어남)
```

### Pseudo Hi-Res
**$2133 (SETINI) Bit 3 = 1**로 진짜 512픽셀 모드 활성화 (메인/서브 화면 교대)

---

## Mode 6 (16색 1레이어, Hi-Res + Offset)

### 스펙
- **BG1**: 4bpp (16색), 512×224
- **BG2**: Offset 데이터

Mode 5 + Offset-per-tile

---

## Mode 7 (256색, 회전/확대/축소)

### 스펙
- **BG1**: 8bpp (256색)
- **해상도**: 128×128 타일 (1024×1024 픽셀)
- **변환**: 회전, 확대, 축소, 원근감

### 변환 행렬

```
[A B]   [cos θ  -sin θ]
[C D] = [sin θ   cos θ]

X' = A×X + B×Y + CenterX
Y' = C×X + D×Y + CenterY
```

### 레지스터
```
$211A (M7SEL): Mode 7 설정
$211B (M7A): A (cos θ)
$211C (M7B): B (-sin θ)
$211D (M7C): C (sin θ)
$211E (M7D): D (cos θ)
$211F (M7X): Center X
$2120 (M7Y): Center Y
```

### 예제 (45도 회전)
```asm
; cos(45°) ≈ 0.707 ≈ 181/256
; sin(45°) ≈ 0.707 ≈ 181/256

LDA #$B5     ; 181 Low
STA $211B    ; M7A
LDA #$00
STA $211B    ; M7A High

LDA #$4B     ; -181 (256-181=75, 2의 보수)
STA $211C    ; M7B
LDA #$FF
STA $211C    ; M7B High

; C, D도 동일하게 설정
```

### EXTBG (Mode 7 확장)
**$2133 (SETINI) Bit 6 = 1**: BG2 활성화 (우선순위 레이어)

### 실제 사용
- **Super Mario Kart**: 레이스 트랙
- **F-Zero**: 코스
- **Final Fantasy VI**: 비행선 월드맵

---

## Mode 선택 가이드

### 게임 장르별 추천

| 장르 | 추천 Mode | 이유 |
|------|-----------|------|
| 플랫포머 | Mode 1 | 레이어 3개, 충분한 색상 |
| RPG (필드) | Mode 1 | 전경/배경/원거리 |
| RPG (전투) | Mode 1 or 3 | 화려한 배경 |
| 레이싱 | Mode 7 | 원근감 필수 |
| 슈팅 | Mode 1 or 2 | 스크롤 효과 |
| 퍼즐 | Mode 0 or 1 | 단순한 그래픽 |

### 색상 vs 레이어

```
많은 색상 필요:
→ Mode 3, 4, 7 (256색)

많은 레이어 필요:
→ Mode 0 (4레이어)

균형:
→ Mode 1 (16색, 3레이어) ✓ 가장 흔함
```

---

## 구현 가이드

### Mode 설정

```cpp
void setBackgroundMode(int mode) {
    writePPU(0x2105, mode & 0x07);
    
    switch (mode) {
        case 0:
            // 4개 배경, 모두 2bpp
            bgColorDepth[0] = bgColorDepth[1] = 
            bgColorDepth[2] = bgColorDepth[3] = 2;
            bgEnabled[0] = bgEnabled[1] = 
            bgEnabled[2] = bgEnabled[3] = true;
            break;
            
        case 1:
            bgColorDepth[0] = bgColorDepth[1] = 4;
            bgColorDepth[2] = 2;
            bgEnabled[0] = bgEnabled[1] = bgEnabled[2] = true;
            bgEnabled[3] = false;
            break;
            
        case 7:
            bgColorDepth[0] = 8;
            bgEnabled[0] = true;
            bgEnabled[1] = bgEnabled[2] = bgEnabled[3] = false;
            mode7Enabled = true;
            break;
    }
}
```

---

## 참고 자료

- [SnesLab - Background Modes](https://sneslab.net/wiki/Background_modes)
- [SnesLab - Mode 7](https://sneslab.net/wiki/Mode_7)

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete
