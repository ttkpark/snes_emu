# IPL 데이터 전송 문제 분석

**작성일**: 2025-01-XX  
**상태**: Critical - SPC700 IPL ROM 데이터 전송이 중단됨

---

## 문제 요약

SPC700 IPL ROM 루틴이 데이터 전송을 완료하지 못하고 Y=0x1f(31바이트)에서 멈춤.

---

## 증상

### 1. 데이터 전송 중단
- **CPU**: Y=0x0148(328바이트)까지 데이터 전송 시도
- **SPC700**: Y=0x1f(31바이트)까지만 수신 후 멈춤
- **포트 0**: 0x1f까지 증가한 후 더 이상 업데이트되지 않음

### 2. 타이밍 불일치
```
CPU 사이클 12128: CPU가 포트 0에 0x48을 씀
CPU 사이클 12129: CPU가 포트 0을 읽을 때 0x47을 읽음 (이전 값!)
SPC700 사이클 12148: SPC700이 포트 0에 0x48을 씀 (너무 늦음!)
```

### 3. 무한 루프
- CPU가 포트 0이 0x48이 되기를 기다림
- SPC700이 포트 0에 0x48을 쓰지만, CPU가 이미 읽은 후
- CPU가 계속 0x47을 읽어 무한 루프 발생

---

## 근본 원인

### 1. 포트 0 업데이트 타이밍 문제

**현재 구현** (`src/apu/apu.cpp:770`):
```cpp
// SPC가 ACK를 보낸 것처럼 Port 0 업데이트
m_aram[0xF4] = m_cpuPorts[0];
```

**문제점**:
- CPU가 포트 0에 값을 쓰면 즉시 `m_aram[0xF4]`가 업데이트됨
- 하지만 SPC700 IPL ROM 루틴은 실제로 포트 0을 읽고 Y를 비교한 후 포트 0에 Y를 씀
- 이 과정이 CPU가 포트 0을 읽는 시점보다 늦어서 타이밍 불일치 발생

### 2. SPC700 실행 타이밍 문제

**SPC700 IPL ROM 루틴** (0xFFDA-0xFFE5):
```
0xFFDA: CMP Y,dp    ; Y와 포트 0 비교
0xFFDC: BNE         ; 다르면 분기
0xFFDE: MOV A,dp    ; 포트 1 읽기
0xFFE0: MOV dp,Y    ; 포트 0에 Y 쓰기
0xFFE2: MOV (dp)+Y,A ; ARAM에 데이터 쓰기
0xFFE4: INC Y       ; Y 증가
0xFFE5: BNE         ; 루프 계속
```

**문제점**:
- SPC700이 포트 0에 Y를 쓰는 시점이 CPU가 포트 0을 읽는 시점보다 늦음
- CPU와 SPC700의 사이클 동기화가 제대로 되지 않음

### 3. 데이터 전송 중단 원인

**가능한 원인**:
1. SPC700이 포트 1에서 데이터를 읽지 못함
2. SPC700 IPL ROM 루틴이 예상과 다른 경로로 실행됨
3. 포트 0 업데이트가 제대로 반영되지 않음

---

## 해결 방안

### 방안 1: 포트 0 업데이트 타이밍 수정

SPC700이 실제로 포트 0에 Y를 쓸 때만 `m_aram[0xF4]`를 업데이트하도록 수정:

```cpp
// writeARAM()에서 포트 0($F4)에 쓸 때
case 0xF4: {
    m_aram[0xF4] = value;
    // CPU가 즉시 읽을 수 있도록 보장
    // (이미 구현되어 있음)
}
```

### 방안 2: CPU-SPC700 사이클 동기화 개선

CPU와 SPC700의 실행 타이밍을 더 정확하게 동기화:

```cpp
// CPU가 포트 0을 읽기 전에 SPC700이 포트 0에 쓸 수 있도록
// SPC700을 먼저 실행하거나, 포트 읽기 시점을 조정
```

### 방안 3: IPL ROM 루틴 실행 확인

SPC700 IPL ROM 루틴이 정상적으로 실행되는지 확인:

```cpp
// IPL ROM 루틴이 포트 1에서 데이터를 읽고
// 포트 0에 Y를 쓰는 과정이 정상적으로 진행되는지 확인
```

---

## 디버깅 방법

### 1. 포트 0 읽기/쓰기 로그 확인
```bash
grep "SPC700 wrote port 0" apu_trace.log | tail -20
grep "CPU.*CMP.*\$2140" cpu_trace.log | tail -20
```

### 2. 사이클 타이밍 비교
```bash
# CPU 사이클과 SPC700 사이클을 비교하여
# 포트 0 읽기/쓰기 시점 확인
```

### 3. SPC700 IPL ROM 루틴 실행 추적
```bash
grep "PC:0xffda\|PC:0xffdc\|PC:0xffde\|PC:0xffe0" apu_trace.log | tail -50
```

---

## 참고 자료

- `src/apu/apu.cpp`: APU 구현
- `docs/hardware/apu_ipl_rom.md`: IPL ROM 프로토콜 문서
- `spctest/spctest.asm`: 테스트 ROM 소스 코드





