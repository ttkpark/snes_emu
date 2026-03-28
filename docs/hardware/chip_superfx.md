# SuperFX (GSU) - Graphics Support Unit

## 📋 목차
1. [개요](#개요)
2. [하드웨어 스펙](#하드웨어-스펙)
3. [레지스터](#레지스터)
4. [명령어 세트](#명령어-세트)
5. [3D 렌더링](#3d-렌더링)
6. [게임 목록](#게임-목록)

---

## 개요

**SuperFX (GSU - Graphics Support Unit)**는 카트리지에 내장된 **RISC 코프로세서**로, 3D 그래픽과 고속 2D 변환을 담당합니다.

### 주요 게임
- **Star Fox** (1993)
- **Yoshi's Island** (1995)
- **Doom** (1995)
- **Stunt Race FX**

---

## 하드웨어 스펙

| 항목 | 스펙 |
|------|------|
| **클럭** | 10.74 MHz (GSU-1), 21.48 MHz (GSU-2) |
| **레지스터** | 16개 범용 (R0-R15) |
| **캐시** | 512바이트 |
| **ROM 액세스** | 직접 액세스 가능 |
| **RAM** | 32-128KB (게임에 따라) |

---

## 레지스터

### 범용 레지스터 (R0-R15)

```
R0:  일반
R1-R10: 일반
R11: Link Register (서브루틴 복귀)
R12: Loop Counter
R13: Loop Address
R14: ROM Buffer
R15: Program Counter (PC)
```

### 특수 레지스터

```
CFGR: Configuration
SCMR: Screen Mode
POR:  Program Offset
BRAMR: Backup RAM
VCR:  Version Code
RAMBR: RAM Bank
CBR:  Cache Base
```

---

## 명령어 세트

### 산술 연산
```
ADD, SUB, MULT, UMULT
AND, OR, XOR
INC, DEC
ASL, LSR
```

### 메모리
```
LM, SM (Load/Store Multiple)
LDB, STB (Byte)
LDW, STW (Word)
```

### 제어
```
JMP, LJMP
LINK (서브루틴 호출)
LOOP
```

### 그래픽
```
PLOT (픽셀 그리기)
COLOR (색상 설정)
MERGE (픽셀 병합)
FMULT (Fast Multiply, 3D용)
```

---

## 3D 렌더링

SuperFX는 **폴리곤 렌더링**을 소프트웨어로 수행:

```
1. 정점 변환 (FMULT 사용)
2. 투영 (3D → 2D)
3. 삼각형 래스터화 (PLOT)
4. 프레임 버퍼 출력
```

---

## 게임 목록

| 게임 | SuperFX 버전 | 클럭 |
|------|--------------|------|
| Star Fox | GSU-1 | 10.74 MHz |
| Stunt Race FX | GSU-1 | 10.74 MHz |
| Yoshi's Island | GSU-2 | 21.48 MHz |
| Doom | GSU-1 | 10.74 MHz |

---

## 구현 가이드

```cpp
class SuperFX {
private:
    uint16_t R[16];  // 레지스터
    uint8_t cache[512];
    
public:
    void step() {
        uint8_t opcode = fetchOpcode();
        executeOpcode(opcode);
    }
    
    void plot(int x, int y, uint8_t color) {
        // 프레임 버퍼에 픽셀 쓰기
    }
};
```

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete
