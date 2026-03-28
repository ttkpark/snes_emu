# Interactive 디버거 사용 가이드

## 🎯 개요

실시간으로 에뮬레이터 실행을 멈추고, step-by-step으로 디버깅할 수 있는 interactive 디버거입니다.

---

## 🔧 빌드 방법

```batch
.\build_interactive.bat
```

이 명령어는 `ENABLE_INTERACTIVE_DEBUG` 플래그를 활성화하여 빌드합니다.

---

## 🚀 실행 방법

### 기본 실행
```batch
.\snes_emu_debug.exe spctest.sfc
```

### 브레이크포인트 미리 설정하고 실행
main_complete.cpp에서 미리 브레이크포인트 설정 가능:

```cpp
#ifdef ENABLE_INTERACTIVE_DEBUG
    SimpleDebugger debugger;
    
    // 테스트 0x41에 브레이크포인트
    debugger.addBreakpoint(SimpleDebugger::BreakpointType::TEST_NUMBER, 0x41);
    
    // PC:0x1313에 브레이크포인트
    debugger.addBreakpoint(SimpleDebugger::BreakpointType::PC, 0x1313);
    
    // 포트 2 쓰기에 브레이크포인트
    debugger.addBreakpoint(SimpleDebugger::BreakpointType::PORT_WRITE, 2);
    
    apu->setDebugger(&debugger);
#endif
```

---

## 💻 Interactive 명령어

### 실행 제어

| 명령어 | 설명 | 예제 |
|--------|------|------|
| `s` / `step` | 한 명령어 실행 | `s` |
| `n [count]` | N개 명령어 실행 | `n 10` |
| `c` / `continue` | 다음 브레이크포인트까지 실행 | `c` |

### 브레이크포인트

| 명령어 | 설명 | 예제 |
|--------|------|------|
| `b <addr>` | PC 주소에 브레이크포인트 | `b 1313` (hex) |
| `bc <cycle>` | 사이클에 브레이크포인트 | `bc 875000` |
| `bt <test>` | 테스트 번호에 브레이크포인트 | `bt 42` (hex) |
| `bp <port>` | 포트 쓰기에 브레이크포인트 | `bp 2` |
| `d <id>` | 브레이크포인트 삭제 | `d 1` |
| `list` / `l` | 브레이크포인트 목록 | `list` |

### 정보 확인

| 명령어 | 설명 |
|--------|------|
| `r` / `reg` | 레지스터 상태 |
| `h` / `help` | 도움말 |

### 종료

| 명령어 | 설명 |
|--------|------|
| `q` / `quit` | 에뮬레이터 종료 |

---

## 📖 사용 예제

### 예제 1: 테스트 0x41 디버깅

```batch
.\snes_emu_debug.exe spctest.sfc
```

실행 후:

```
(snes-dbg) bt 41
Breakpoint 1 set on test 0x41

(snes-dbg) c
Continuing...

=== Breakpoint 1 hit on test 0x41 ===
[Cyc:0000875344] PC:0x1306 | A:0x41 X:0x02 Y:0x02 SP:0xED PSW:0x03

>>> Interactive mode (type 'h' for help)
(snes-dbg) s
[Cyc:0000875345] PC:0x1309 | A:0x41 X:0x02 Y:0x02 SP:0xEF PSW:0x03

(snes-dbg) s
[Cyc:0000875346] PC:0x130B | A:0x00 X:0x02 Y:0x02 SP:0xEF PSW:0x03

(snes-dbg) n 5
Stepping 5 instructions...
```

### 예제 2: 특정 PC에서 멈추기

```
(snes-dbg) b 1313
Breakpoint 1 set at PC:0x1313

(snes-dbg) c
Continuing...

=== Breakpoint 1 hit at PC:0x1313 ===
[Cyc:0000875354] PC:0x1313 | A:0x12 X:0x34 Y:0x56 SP:0xEF PSW:0x00

(snes-dbg) s
[Cyc:0000875356] PC:0x1316 | A:0x12 X:0x34 Y:0x56 SP:0xEF PSW:0x80
```

### 예제 3: 포트 통신 추적

```
(snes-dbg) bp 2
Breakpoint 1 set on port 2 write

(snes-dbg) c
Continuing...

=== Breakpoint 1 hit on port 2 write (value=0x41) ===
[Cyc:0000875344] PC:0x0321 | A:0x41 X:0x02 Y:0x02 SP:0xED PSW:0x03

(snes-dbg) r
Current state:
  PC:0x321
  Cycle:875344

(snes-dbg) c
Continuing...
```

### 예제 4: 특정 사이클까지 실행

```
(snes-dbg) bc 875350
Breakpoint 1 set at cycle 875350

(snes-dbg) c
Continuing...

=== Breakpoint 1 hit at cycle 875350 ===
[Cyc:0000875350] PC:0x130E | A:0x12 X:0x02 Y:0x02 SP:0xEE PSW:0x01

(snes-dbg) n 20
Stepping 20 instructions...
```

---

## 🔍 주요 장점

### 1. 로그 파일 없음
- 실시간으로 보기 때문에 로그 파일이 쌓이지 않음
- 메모리 효율적

### 2. 즉시 반응
- 원하는 지점에서 바로 멈춤
- step-by-step 실행

### 3. 유연한 브레이크포인트
- PC 주소
- 사이클 수
- 테스트 번호
- 포트 쓰기

### 4. 간단한 사용법
- 파이썬 pdb와 유사한 인터페이스
- 단순한 명령어

---

## 🎯 일반적인 워크플로우

### 테스트 실패 디버깅

```
1. 테스트 번호에 브레이크포인트
   (snes-dbg) bt 41
   
2. 실행
   (snes-dbg) c
   
3. 브레이크포인트 hit
   === Breakpoint hit on test 0x41 ===
   
4. 한 줄씩 실행하며 관찰
   (snes-dbg) s
   (snes-dbg) s
   (snes-dbg) s
   
5. 의심되는 부분에 추가 브레이크포인트
   (snes-dbg) b 1318
   
6. 계속 실행
   (snes-dbg) c
```

### 무한 루프 찾기

```
1. 사이클 제한 설정 (코드에서)
   debugger.setMaxCycles(1000000);
   
2. 실행
   (snes-dbg) c
   
3. Max cycles 도달
   === Max cycles (1000000) reached ===
   
4. 현재 PC 확인
   (snes-dbg) r
   Current state:
     PC:0x357
   
5. 해당 PC가 무한 루프!
```

---

## ⚡ 성능 팁

### 브레이크포인트를 적절히 설정
- 너무 많은 브레이크포인트는 느림
- 필요한 지점에만 설정

### step 모드 최소화
- step 모드는 매 명령어마다 멈춤
- 필요한 구간만 step으로 확인

### 사이클 제한 활용
- 무한 루프 방지
- 자동 중단

---

## 🐛 문제 해결

### Q: 브레이크포인트가 안 걸려요
**A**: 주소가 16진수인지 확인
```
잘못: b 1313
올바름: b 1313  (이미 hex로 해석됨)
```

### Q: 입력이 안돼요
**A**: 콘솔이 활성화되어 있는지 확인
- `/SUBSYSTEM:CONSOLE`로 빌드되어야 함

### Q: 너무 빨리 지나가요
**A**: 브레이크포인트를 더 앞쪽에 설정
```
(snes-dbg) bt 40  # 테스트 0x40에 미리 설정
(snes-dbg) c
```

---

## 📝 main_complete.cpp 예제 통합

```cpp
#ifdef ENABLE_INTERACTIVE_DEBUG
#include "debug/simple_debugger.h"
#endif

int main(int argc, char** argv) {
    // ... 초기화 ...
    
#ifdef ENABLE_INTERACTIVE_DEBUG
    SimpleDebugger debugger;
    
    // 원하는 브레이크포인트 설정
    debugger.addBreakpoint(SimpleDebugger::BreakpointType::TEST_NUMBER, 0x41, "Test 0x41");
    debugger.addBreakpoint(SimpleDebugger::BreakpointType::PC, 0x1313, "CMP instruction");
    
    // Max cycles 설정 (optional)
    debugger.setMaxCycles(1000000);
    
    // APU에 디버거 연결
    apu->setDebugger(&debugger);
    
    std::cout << "\n=== Interactive Debugger Enabled ===" << std::endl;
    std::cout << "Type 'h' for help when prompted" << std::endl;
    std::cout << "Initial breakpoints:" << std::endl;
    debugger.listBreakpoints();
    std::cout << "\nStarting emulation..." << std::endl;
#endif
    
    // 메인 루프
    while (running) {
        cpu->step();
        apu->step();  // 여기서 브레이크포인트 체크
        ppu->step();
    }
    
    return 0;
}
```

---

## 🎓 고급 사용법

### 조건부 브레이크포인트 (향후 추가 예정)

```cpp
debugger.addConditionalBreakpoint(0x1313, [](APUState& state) {
    return state.a == 0x12 && state.x == 0x34;
});
```

### 메모리 워치 (향후 추가 예정)

```cpp
debugger.addMemoryWatch(0x01, [](uint8_t old_val, uint8_t new_val) {
    printf("Memory[0x01] changed: 0x%02X -> 0x%02X\n", old_val, new_val);
});
```

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: 즉시 사용 가능

**다음 개선 사항**:
- [ ] 메모리 덤프 명령어
- [ ] 디스어셈블리 명령어  
- [ ] 스크립트 파일 지원
- [ ] 조건부 브레이크포인트










