# 실시간 APU 모니터 사용 가이드

## 🎯 개요

Python으로 만든 실시간 로그 모니터입니다. **별도 빌드 없이** 즉시 사용 가능합니다!

로그 파일을 실시간으로 읽으면서 브레이크포인트에서 자동으로 멈춥니다.

---

## 🚀 사용 방법

### 1단계: 에뮬레이터 실행 (백그라운드)

```powershell
Start-Process -FilePath ".\snes_emu_complete.exe" -ArgumentList "spctest.sfc"
```

### 2단계: 모니터 실행

```powershell
python realtime_monitor.py
```

또는 브레이크포인트 미리 설정:

```powershell
# 테스트 0x41에 브레이크포인트
python realtime_monitor.py apu_trace.log --test 41

# PC:0x1313에 브레이크포인트
python realtime_monitor.py apu_trace.log --pc 1313
```

---

## 💻 사용 예제

### 예제 1: 테스트 0x41 모니터링

```powershell
PS> Start-Process .\snes_emu_complete.exe -ArgumentList spctest.sfc
PS> python realtime_monitor.py --test 41

=== Real-Time APU Monitor ===
Monitoring: apu_trace.log
Waiting for log file...
Log file found. Monitoring started.

✓ Breakpoint set on test 0x41


=== BREAKPOINT: Test 0x41 ===
Port 2 write: 0x41

>>> Paused (type 'h' for help)
(monitor) s
Stepping...

[Cyc:0000875345] PC:0x1306
  A:0x41 X:0x02 Y:0x02 SP:0xEF PSW:0x03
  Instruction: MOV $01,#$12
  Opcode: 8f 12 01

>>> Paused
(monitor) s
[Cyc:0000875346] PC:0x1309
  A:0x41 X:0x02 Y:0x02 SP:0xEF PSW:0x03
  Instruction: MOV A,#$00
  Opcode: e8 00

>>> Paused
(monitor) c
Continuing...
```

### 예제 2: 특정 PC 모니터링

```powershell
PS> python realtime_monitor.py --pc 1313

=== BREAKPOINT: PC:0x1313 ===
[Cyc:0000875354] PC:0x1313
  A:0x12 X:0x34 Y:0x56 SP:0xEF PSW:0x00
  Instruction: CMP $01,#$3F
  Opcode: 64 01

>>> Paused
(monitor) hist
=== Recent History ===
1. [Cyc:0000875346] PC:0x1309 | MOV A,#$00
2. [Cyc:0000875348] PC:0x130B | PUSH A
3. [Cyc:0000875349] PC:0x130C | MOV A,#$12
4. [Cyc:0000875350] PC:0x130E | MOV X,#$34
5. [Cyc:0000875352] PC:0x1310 | MOV Y,#$56
6. [Cyc:0000875353] PC:0x1312 | POP PSW
7. [Cyc:0000875354] PC:0x1313 | CMP $01,#$3F

>>> Paused
(monitor) s
```

### 예제 3: 동적 브레이크포인트 추가

```powershell
PS> python realtime_monitor.py

=== Real-Time APU Monitor ===
Monitoring started.

(press Ctrl+C to enter interactive mode)
^C
>>> Paused
(monitor) bt 42
✓ Breakpoint set on test 0x42

(monitor) bp 2
✓ Breakpoint set on port 2 write

(monitor) c
Continuing...


=== BREAKPOINT: Port 2 write (value=0x42) ===

>>> Paused
(monitor) s
```

---

## 📋 명령어

| 명령어 | 설명 | 예제 |
|--------|------|------|
| `s` / `step` | 한 명령어 실행 | `s` |
| `c` / `continue` | 계속 실행 | `c` |
| `b <addr>` | PC 브레이크포인트 (hex) | `b 1313` |
| `bt <test>` | 테스트 브레이크포인트 (hex) | `bt 42` |
| `bp <port>` | 포트 쓰기 브레이크포인트 | `bp 2` |
| `hist` | 최근 히스토리 | `hist` |
| `h` / `help` | 도움말 | `h` |
| `q` / `quit` | 종료 | `q` |

---

## 🎯 장점

### ✅ 즉시 사용 가능
- 빌드 불필요
- Python만 있으면 OK

### ✅ 실시간 모니터링
- 로그 파일을 실시간으로 읽음
- 브레이크포인트에서 자동 정지

### ✅ 유연성
- 동적 브레이크포인트 추가
- 실행 중 언제든 멈춤 (Ctrl+C)

### ✅ 가볍고 빠름
- 최소한의 메모리 사용
- 빠른 응답

---

## 🔧 고급 사용법

### PowerShell과 함께 사용

```powershell
# 터미널 2개 열기

# 터미널 1: 에뮬레이터
.\snes_emu_complete.exe spctest.sfc

# 터미널 2: 모니터
python realtime_monitor.py --test 41
```

### 특정 조건에서만 멈추기

```python
# realtime_monitor.py 수정
def check_breakpoint(self, data):
    # 커스텀 조건
    if data.get('pc') == 0x1313 and data.get('a') == 0x12:
        print("\n=== CUSTOM BREAKPOINT: PC:0x1313 && A:0x12 ===")
        return True
    return False
```

---

## 📊 비교: C++ vs Python

| 기능 | C++ Interactive | Python Monitor |
|------|----------------|----------------|
| 빌드 필요 | ✅ 필요 | ❌ 불필요 |
| 실시간 | ✅ 즉시 | ⚠️ 약간 지연 |
| 브레이크포인트 | ✅ 정확 | ✅ 정확 |
| step 실행 | ✅ 정확 | ✅ 정확 |
| 사용 편의성 | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ |
| 커스터마이징 | ⭐⭐⭐ | ⭐⭐⭐⭐⭐ |

---

## 🎓 추천 사용 시나리오

### Python Monitor 사용
- 빠른 테스트
- 프로토타이핑
- 간단한 디버깅
- 커스텀 조건 추가

### C++ Interactive 사용
- 정밀한 디버깅
- 실시간 성능 중요
- 복잡한 브레이크포인트
- 프로덕션 디버깅

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: 즉시 사용 가능 ✅










