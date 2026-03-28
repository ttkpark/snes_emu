# SNES 에뮬레이터 디버거 종합 설계

**생성일**: 2025-12-14  
**버전**: 1.0  
**상태**: 설계 완료, 구현 준비

---

## 📋 목차

1. [개요](#개요)
2. [아키텍처](#아키텍처)
3. [핵심 컴포넌트](#핵심-컴포넌트)
4. [사용자 인터페이스](#사용자-인터페이스)
5. [구현 계획](#구현-계획)
6. [사용 예제](#사용-예제)

---

## 개요

### 목적
SNES 에뮬레이터 개발 및 디버깅을 위한 **통합 디버깅 시스템**

### 주요 기능
- ✅ **무한 루프 자동 감지** (`02_loop_detection.md`)
- 🔄 **브레이크포인트 시스템** (PC, 메모리, 조건부)
- 🔄 **실행 트레이스 로깅** (CPU, APU, PPU, 메모리)
- 🔄 **메모리 검사 도구** (덤프, 검색, 비교, 워치)
- 🔄 **포트 통신 분석** (CPU-APU 동기화)
- 🔄 **타이밍 분석** (사이클 추적, NMI/IRQ)
- 🔄 **CLI 디버거** (REPL 인터페이스)
- 🔄 **스크립팅 지원** (자동화 테스트)

### 설계 원칙
1. **Agent 친화적**: 모든 도구가 CLI로 작동
2. **모듈화**: 각 컴포넌트 독립적
3. **성능 최소 영향**: 기본 오버헤드 < 5%
4. **확장 가능**: 새 기능 쉽게 추가
5. **자동화 가능**: PowerShell/Lua 스크립트 지원

---

## 아키텍처

### 계층 구조

```
┌─────────────────────────────────────────────────────────────┐
│                    User Interface Layer                      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ CLI Debugger │  │  PowerShell  │  │ Lua Scripting│      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────┐
│                    Debugger Core Layer                       │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │  Breakpoint  │  │ Loop Detector│  │ Trace Logger │      │
│  │   Manager    │  │              │  │              │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │   Memory     │  │Port Analyzer │  │Timing Tracker│      │
│  │  Inspector   │  │              │  │              │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
                              │
┌─────────────────────────────────────────────────────────────┐
│                   Emulator Core Layer                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │     CPU      │  │     APU      │  │     PPU      │      │
│  │   (65C816)   │  │  (SPC700)    │  │              │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │    Memory    │  │    Input     │  │    Timers    │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

### 데이터 흐름

```
User Command → Debugger Core → Emulator Core
                     │
                     ├→ Event Callbacks
                     │   (Breakpoint, Loop, etc.)
                     │
                     ├→ Trace Logging
                     │   (File Output)
                     │
                     └→ Analysis & Report
                         (CLI/PowerShell)
```

---

## 핵심 컴포넌트

### 1. Debugger Manager (메인 인터페이스)

```cpp
class Debugger {
public:
    // 생성자
    Debugger(CPU* cpu, APU* apu, PPU* ppu, Memory* memory);
    
    // 실행 제어
    void run();                          // 계속 실행
    void step(int count = 1);            // n 명령어 실행
    void stepOver();                     // 함수 호출 건너뛰기
    void stepOut();                      // 함수에서 나가기
    void pause();                        // 일시정지
    void reset();                        // 리셋
    
    // 브레이크포인트
    int addBreakpoint(uint32_t address, BreakpointType type = BreakType::PC);
    int addConditionalBreakpoint(uint32_t address, 
                                  std::function<bool(CPUState&)> condition);
    void removeBreakpoint(int id);
    void listBreakpoints();
    
    // 메모리 검사
    void dumpMemory(uint32_t start, uint32_t length);
    void searchMemory(uint8_t value, uint32_t start, uint32_t end);
    void watchMemory(uint32_t address);
    void compareMemory(const std::string& snapshot_name);
    
    // 레지스터
    void printRegisters();
    void setRegister(const std::string& reg, uint32_t value);
    
    // 디스어셈블리
    void disassemble(uint32_t address, int count = 10);
    
    // 트레이스
    void enableTrace(bool enable, TraceFilter filter = TraceFilter::ALL);
    void setTraceFile(const std::string& filename);
    
    // 분석 도구
    std::vector<LoopInfo> analyzeLoops();
    PortAnalysisReport analyzePortCommunication();
    TimingReport analyzeTiming();
    
    // 스크립팅
    void loadScript(const std::string& filename);
    void executeCommand(const std::string& command);
    
private:
    // 컴포넌트
    CPU* m_cpu;
    APU* m_apu;
    PPU* m_ppu;
    Memory* m_memory;
    
    // 디버깅 도구
    BreakpointManager m_breakpoints;
    LoopDetector m_loop_detector;
    TraceLogger m_trace_logger;
    MemoryInspector m_memory_inspector;
    PortAnalyzer m_port_analyzer;
    TimingTracker m_timing_tracker;
    
    // 상태
    bool m_running;
    bool m_paused;
    std::map<std::string, MemorySnapshot> m_snapshots;
};
```

### 2. Breakpoint Manager

```cpp
enum class BreakpointType {
    PC,              // PC 주소
    MEMORY_READ,     // 메모리 읽기
    MEMORY_WRITE,    // 메모리 쓰기
    MEMORY_EXECUTE,  // 메모리 실행
    CONDITIONAL,     // 조건부
    TEMPORARY        // 일회성
};

struct Breakpoint {
    int id;
    BreakpointType type;
    uint32_t address;
    bool enabled;
    int hit_count;
    int ignore_count;  // n번 무시 후 중단
    std::function<bool(CPUState&)> condition;
};

class BreakpointManager {
public:
    int addBreakpoint(uint32_t address, BreakpointType type);
    int addConditionalBreakpoint(uint32_t address, 
                                  std::function<bool(CPUState&)> condition);
    void removeBreakpoint(int id);
    void enableBreakpoint(int id, bool enable);
    
    // 체크
    bool checkBreakpoint(uint32_t address, BreakpointType type, CPUState& state);
    bool checkMemoryBreakpoint(uint32_t address, BreakpointType type);
    
    // 조회
    std::vector<Breakpoint> listBreakpoints();
    Breakpoint getBreakpoint(int id);
    
private:
    std::map<int, Breakpoint> m_breakpoints;
    int m_next_id;
};
```

### 3. Loop Detector (이미 설계됨)

`02_loop_detection.md` 참조

```cpp
class LoopDetector {
public:
    struct State { uint8_t a, x, y, sp, psw; };
    struct LoopInfo {
        uint32_t pc;
        int visit_count;
        std::vector<State> unique_states;
        bool is_definite_loop;
        std::string description;
    };
    
    LoopDetector(int threshold = 100, bool auto_break = true);
    bool detectLoop(uint32_t pc, const State& state);
    std::vector<LoopInfo> getLoopInfo() const;
    void setCallback(std::function<void(const LoopInfo&)> callback);
};
```

### 4. Trace Logger

```cpp
enum class TraceLevel {
    DEBUG,   // 모든 세부사항
    INFO,    // 일반 정보
    WARN,    // 경고
    ERROR    // 오류
};

enum class TraceComponent {
    CPU = 1 << 0,
    APU = 1 << 1,
    PPU = 1 << 2,
    MEMORY = 1 << 3,
    INPUT = 1 << 4,
    ALL = 0xFF
};

struct TraceFilter {
    TraceComponent components;
    uint32_t pc_range_start;
    uint32_t pc_range_end;
    TraceLevel min_level;
    bool enabled;
};

class TraceLogger {
public:
    TraceLogger();
    
    // 로깅
    void logCPU(uint32_t pc, const std::string& instruction, CPUState& state);
    void logAPU(uint32_t pc, const std::string& instruction, APUState& state);
    void logPPU(const std::string& event);
    void logMemory(uint32_t address, uint8_t value, bool is_write);
    
    // 필터
    void setFilter(const TraceFilter& filter);
    void enableComponent(TraceComponent component, bool enable);
    void setPCRange(uint32_t start, uint32_t end);
    
    // 출력
    void setOutputFile(const std::string& filename);
    void flush();
    
    // 조건부 로깅
    void setConditionalLogging(std::function<bool(uint32_t pc)> condition);
    
private:
    std::ofstream m_file;
    TraceFilter m_filter;
    std::function<bool(uint32_t)> m_condition;
    uint64_t m_cycle_count;
};
```

### 5. Memory Inspector

```cpp
struct MemorySnapshot {
    std::string name;
    std::chrono::system_clock::time_point timestamp;
    std::vector<uint8_t> data;
};

struct MemoryWatch {
    uint32_t address;
    uint8_t last_value;
    int change_count;
    std::vector<std::pair<uint64_t, uint8_t>> history;  // cycle, value
};

class MemoryInspector {
public:
    // 덤프
    void dump(uint32_t start, uint32_t length, DumpFormat format = DumpFormat::HEX);
    void dumpToFile(const std::string& filename, uint32_t start, uint32_t length);
    
    // 검색
    std::vector<uint32_t> search(uint8_t value, uint32_t start, uint32_t end);
    std::vector<uint32_t> searchPattern(const std::vector<uint8_t>& pattern, 
                                        uint32_t start, uint32_t end);
    
    // 스냅샷
    void saveSnapshot(const std::string& name);
    void loadSnapshot(const std::string& name);
    void compareSnapshot(const std::string& name);
    std::vector<uint32_t> getDifferences(const std::string& name);
    
    // 워치
    void addWatch(uint32_t address);
    void removeWatch(uint32_t address);
    std::vector<MemoryWatch> getWatches();
    void updateWatches();
    
    // 특수 뷰
    void dumpVRAM();
    void dumpCGRAM();
    void dumpOAM();
    void dumpWorkRAM();
    
private:
    Memory* m_memory;
    std::map<std::string, MemorySnapshot> m_snapshots;
    std::map<uint32_t, MemoryWatch> m_watches;
};
```

### 6. Port Analyzer

```cpp
struct PortEvent {
    uint64_t cycle;
    uint8_t port;        // 0-3
    uint8_t value;
    bool is_write;       // true=write, false=read
    bool is_cpu;         // true=CPU, false=SPC
    uint32_t pc;
};

struct PortAnalysisReport {
    std::vector<PortEvent> events;
    int total_reads;
    int total_writes;
    int cpu_writes;
    int spc_writes;
    std::vector<std::string> anomalies;  // 동기화 문제 등
    std::string protocol_status;         // "OK", "FAILED", "TIMEOUT"
};

class PortAnalyzer {
public:
    // 이벤트 기록
    void recordRead(uint8_t port, uint8_t value, bool is_cpu, uint32_t pc);
    void recordWrite(uint8_t port, uint8_t value, bool is_cpu, uint32_t pc);
    
    // 분석
    PortAnalysisReport analyze();
    void detectAnomalies();
    void verifyProtocol(const std::string& expected_protocol);
    
    // 출력
    void dumpEvents(const std::string& filename);
    void printSummary();
    
    // 필터
    void setFilter(uint8_t port_mask);  // bit mask for ports 0-3
    
private:
    std::vector<PortEvent> m_events;
    uint64_t m_current_cycle;
};
```

### 7. Timing Tracker

```cpp
struct ComponentTiming {
    uint64_t cycles;
    uint64_t instructions;
    double frequency;  // Hz
    double time_elapsed;  // seconds
};

struct TimingReport {
    ComponentTiming cpu;
    ComponentTiming apu;
    ComponentTiming ppu;
    
    double cpu_apu_ratio;    // 기대값: ~3.5
    double cpu_ppu_ratio;    // 기대값: ~1.0
    
    int frame_count;
    double avg_frame_time;   // ms
    double current_fps;
    
    std::vector<std::string> warnings;  // 타이밍 드리프트 경고
};

class TimingTracker {
public:
    // 사이클 추적
    void recordCPUCycle();
    void recordAPUCycle();
    void recordPPUCycle();
    void recordFrame();
    
    // 분석
    TimingReport analyze();
    bool checkRatios();
    void detectDrift();
    
    // NMI/IRQ 타이밍
    void recordNMI(uint64_t cycle);
    void recordIRQ(uint64_t cycle);
    std::vector<uint64_t> getNMIHistory();
    
    // 리셋
    void reset();
    
private:
    ComponentTiming m_cpu_timing;
    ComponentTiming m_apu_timing;
    ComponentTiming m_ppu_timing;
    
    std::vector<uint64_t> m_nmi_cycles;
    std::vector<uint64_t> m_irq_cycles;
    
    std::chrono::steady_clock::time_point m_start_time;
};
```

---

## 사용자 인터페이스

### 1. CLI 디버거 (REPL)

#### 명령어 세트

```
=== 실행 제어 ===
run [rom_file]              ROM 실행
step [n]                    n 명령어 실행 (기본 1)
stepi                       명령어 1개 실행 (step into)
next                        다음 명령어 (step over)
continue / c                다음 브레이크포인트까지 실행
finish                      현재 함수 끝까지 실행
reset                       에뮬레이터 리셋
quit / q                    종료

=== 브레이크포인트 ===
break <addr>                PC 브레이크포인트
break *<addr>               메모리 실행 브레이크포인트
rbreak <addr>               메모리 읽기 브레이크포인트
wbreak <addr>               메모리 쓰기 브레이크포인트
break if <condition>        조건부 브레이크포인트
break list                  브레이크포인트 목록
break delete <id>           브레이크포인트 삭제
break enable <id>           브레이크포인트 활성화
break disable <id>          브레이크포인트 비활성화

=== 검사 ===
reg                         레지스터 표시
reg <name> <value>          레지스터 설정
print <expr>                표현식 평가
x/<fmt> <addr> <len>        메모리 덤프
  fmt: x(hex), d(dec), c(char), i(instruction)
dis [addr] [count]          디스어셈블리
info cpu                    CPU 상태
info apu                    APU 상태
info ppu                    PPU 상태

=== 메모리 ===
mem <addr> <len>            메모리 덤프
search <value> <start> <end>  메모리 검색
watch <addr>                메모리 워치 추가
unwatch <addr>              메모리 워치 삭제
snapshot save <name>        메모리 스냅샷 저장
snapshot load <name>        메모리 스냅샷 로드
snapshot compare <name>     스냅샷 비교

=== 트레이스 ===
trace on [filter]           트레이스 활성화
  filter: cpu, apu, ppu, mem, all
trace off                   트레이스 비활성화
trace save <file>           트레이스 파일 지정
trace filter <component>    컴포넌트 필터

=== 분석 ===
analyze loop                무한 루프 분석
analyze ports               포트 통신 분석
analyze timing              타이밍 분석
analyze memory              메모리 사용 분석

=== 스크립팅 ===
source <file>               스크립트 실행
load <script>               Lua 스크립트 로드
py <code>                   Python 코드 실행

=== 기타 ===
help [command]              도움말
set <option> <value>        옵션 설정
show <option>               옵션 표시
```

#### 사용 예제

```bash
# 디버거 시작
$ ./snes_emu_complete --debug

(snes-dbg) run spctest.sfc
Loading ROM: spctest.sfc (131072 bytes)
ROM loaded successfully.
Starting execution...

(snes-dbg) break 0x0357
Breakpoint 1 set at 0x0357

(snes-dbg) break if a == 0x56
Conditional breakpoint 2 set

(snes-dbg) continue
Running...

Breakpoint 1 hit at PC:0x0357
  [Cyc:0000885366] SPC700 PC:0x0357 | 2f fe | BRA rel
  A:0x56 | X:0x34 | Y:0x56 | SP:0xef | PSW:0x01

(snes-dbg) reg
CPU (65C816):
  A: 0x00CC  X: 0x0000  Y: 0x0000
  SP: 0x1FFF  DP: 0x0000  DBR: 0x00
  PC: 0x008024  PBR: 0x00
  P: 00110010 (nvMXdizC)

APU (SPC700):
  A: 0x56  X: 0x34  Y: 0x56
  SP: 0xEF  PC: 0x0357
  PSW: 00000001 (nv--dizC)

(snes-dbg) dis 0x0350 10
0x0350:  8d 03 f6    MOV  Y, #$03
0x0353:  cb f1       MOV  $F1, Y
0x0355:  8f 03 f4    MOV  $F4, #$03
0x0358:  2f fe       BRA  $0358     ← 현재 위치
0x035a:  00          NOP

(snes-dbg) analyze loop
=== Loop Analysis ===
Total suspicious loops: 1

PC: 0x0357
  Visit count: 63
  Unique states: 1
  Type: DEFINITE LOOP
  Description: Infinite loop detected
  
Recommendation: This is the 'fail' routine. Check preceding code.

(snes-dbg) analyze ports
=== Port Communication Analysis ===
Total events: 156
  CPU writes: 42
  SPC writes: 38
  CPU reads: 40
  SPC reads: 36

Last port values:
  Port 0: 0x03 (SPC wrote at cycle 885350)
  Port 1: 0x12
  Port 2: 0x02
  Port 3: 0x56

Protocol status: FAILED (Port 0 should be 0x00)

Anomalies detected:
  - Port 0 never initialized to 0xCC by CPU
  - SPC entered fail routine

(snes-dbg) quit
Debugger terminated.
```

### 2. PowerShell 인터페이스

#### 기존 스크립트 (이미 작성됨)

1. **`monitor_execution.ps1`** - 실행 모니터링
2. **`analyze_loop.ps1`** - 무한 루프 분석

#### 새로운 스크립트

##### `analyze_ports.ps1`
```powershell
param(
    [string]$LogFile = "port_comm.log",
    [switch]$Verbose
)

# 포트 통신 분석
# - 읽기/쓰기 이벤트 카운트
# - 프로토콜 검증
# - 타임라인 생성
```

##### `compare_memory.ps1`
```powershell
param(
    [string]$Snapshot1,
    [string]$Snapshot2,
    [int]$StartAddr = 0x0000,
    [int]$EndAddr = 0xFFFF
)

# 메모리 스냅샷 비교
# - 차이점 표시
# - 변경된 주소 목록
# - 통계
```

##### `verify_timing.ps1`
```powershell
param(
    [string]$TraceFile = "cpu_trace.log"
)

# 타이밍 검증
# - CPU/APU/PPU 사이클 비율
# - NMI 주기 확인
# - 타이밍 드리프트 감지
```

### 3. Lua 스크립팅

```lua
-- spctest_automation.lua

-- 브레이크포인트 설정
emu.addBreakpoint(0x0357, function()
    -- fail 루틴 진입 시
    print("FAIL: Entered fail routine")
    
    -- 상태 덤프
    local a = emu.getRegister("a")
    local x = emu.getRegister("x")
    local y = emu.getRegister("y")
    
    print(string.format("A:0x%02X X:0x%02X Y:0x%02X", a, x, y))
    
    -- 포트 값 확인
    local port0 = emu.readPort(0)
    local port1 = emu.readPort(1)
    
    print(string.format("Port0:0x%02X Port1:0x%02X", port0, port1))
    
    -- 중단
    emu.stop()
    return true
end)

-- 포트 0 쓰기 감시
emu.addMemoryWatch(0x2140, "write", function(addr, value)
    print(string.format("[CPU] Wrote Port 0 = 0x%02X", value))
end)

-- 실행
emu.run()
```

---

## 구현 계획

### Phase 1: 핵심 디버거 프레임워크 (1주)

#### Week 1
- [ ] `Debugger` 메인 클래스
- [ ] `BreakpointManager` 구현
- [ ] 기본 CLI REPL 인터페이스
- [ ] 실행 제어 (run, step, continue, reset)
- [ ] 레지스터/메모리 검사

**파일**:
- `src/debug/debugger.h`
- `src/debug/debugger.cpp`
- `src/debug/breakpoint_manager.h`
- `src/debug/breakpoint_manager.cpp`
- `src/debug/cli_debugger.h`
- `src/debug/cli_debugger.cpp`

### Phase 2: 트레이스 및 분석 (1주)

#### Week 2
- [ ] `TraceLogger` 구현
- [ ] `LoopDetector` 통합 (이미 설계됨)
- [ ] `MemoryInspector` 구현
- [ ] PowerShell 스크립트 추가

**파일**:
- `src/debug/trace_logger.h`
- `src/debug/trace_logger.cpp`
- `src/debug/memory_inspector.h`
- `src/debug/memory_inspector.cpp`
- `analyze_ports.ps1`
- `compare_memory.ps1`

### Phase 3: 고급 기능 (1주)

#### Week 3
- [ ] `PortAnalyzer` 구현
- [ ] `TimingTracker` 구현
- [ ] 조건부 브레이크포인트
- [ ] 메모리 워치

**파일**:
- `src/debug/port_analyzer.h`
- `src/debug/port_analyzer.cpp`
- `src/debug/timing_tracker.h`
- `src/debug/timing_tracker.cpp`
- `verify_timing.ps1`

### Phase 4: 스크립팅 및 자동화 (1주)

#### Week 4
- [ ] Lua 바인딩
- [ ] 스크립트 API
- [ ] 자동화 테스트 프레임워크
- [ ] 문서 완성

**파일**:
- `src/debug/script_engine.h`
- `src/debug/script_engine.cpp`
- `src/debug/lua_bindings.cpp`
- `tests/automated/spctest_auto.lua`

---

## 사용 예제

### 예제 1: spctest.sfc 디버깅

#### 시나리오
SPC700 테스트가 fail 루틴(0x0357)에서 무한 루프

#### 해결 과정

```powershell
# 1. 실행
.\snes_emu_complete.exe --debug spctest.sfc

# 2. fail 루틴에 브레이크포인트
(snes-dbg) break 0x0357
(snes-dbg) continue

# 3. 중단 시 포트 분석
(snes-dbg) analyze ports
# 출력: Port 0이 0xCC가 아님

# 4. 포트 초기화 확인
(snes-dbg) wbreak 0x2140
(snes-dbg) reset
(snes-dbg) continue

# 5. 포트 쓰기 추적
Watchpoint hit: Write to 0x2140, value=0x00
# 문제: 0xCC가 아닌 0x00 기록됨

# 6. CPU 코드 확인
(snes-dbg) dis 0x008000 20
# CPU 초기화 코드 검사

# 7. 수정 후 재테스트
```

### 예제 2: 메모리 손상 추적

```powershell
# 1. 초기 스냅샷
(snes-dbg) snapshot save initial

# 2. 실행
(snes-dbg) step 1000

# 3. 비교
(snes-dbg) snapshot compare initial
# 출력: 0x7E0100-0x7E01FF 변경됨

# 4. 해당 영역 워치
(snes-dbg) watch 0x7E0100

# 5. 계속 실행
(snes-dbg) continue

# 6. 쓰기 감지
Watchpoint hit: Write to 0x7E0100, value=0xFF
PC: 0x008234
Instruction: STA $0100
```

### 예제 3: 타이밍 검증

```powershell
# 1. 타이밍 추적 활성화
(snes-dbg) set timing-track on

# 2. 프레임 실행
(snes-dbg) step 100000

# 3. 타이밍 분석
(snes-dbg) analyze timing

# 출력:
=== Timing Analysis ===
CPU: 358000 cycles (3.58 MHz)
APU: 102400 cycles (1.024 MHz)
PPU: 358000 cycles (3.58 MHz)

CPU/APU ratio: 3.50 (expected: 3.50) ✓
CPU/PPU ratio: 1.00 (expected: 1.00) ✓

Frames: 10
Avg frame time: 16.67 ms
FPS: 60.0

No timing drift detected.
```

### 예제 4: Lua 자동화 테스트

```lua
-- test_port_protocol.lua

-- 테스트: CPU가 포트 0에 0xCC를 쓰는지 확인
function test_port_init()
    -- 포트 0 쓰기 감시
    local port0_written = false
    local written_value = 0
    
    emu.addMemoryWatch(0x2140, "write", function(addr, value)
        port0_written = true
        written_value = value
    end)
    
    -- 100 명령어 실행
    emu.step(100)
    
    -- 검증
    assert(port0_written, "CPU did not write to Port 0")
    assert(written_value == 0xCC, 
           string.format("Port 0 value is 0x%02X, expected 0xCC", written_value))
    
    print("✓ Test passed: Port 0 initialized correctly")
end

-- 테스트: SPC가 응답하는지 확인
function test_spc_response()
    -- SPC가 포트 0에 쓸 때까지 대기
    local timeout = 10000
    local cycles = 0
    
    while emu.readPort(0) ~= 0x00 and cycles < timeout do
        emu.step(1)
        cycles = cycles + 1
    end
    
    assert(cycles < timeout, "SPC did not respond within timeout")
    print(string.format("✓ Test passed: SPC responded after %d cycles", cycles))
end

-- 테스트 실행
test_port_init()
test_spc_response()

print("\n✓ All tests passed!")
```

---

## 성능 고려사항

### 오버헤드 최소화

#### 1. 조건부 컴파일
```cpp
#ifdef DEBUG_ENABLED
    debugger->checkBreakpoint(pc);
#endif
```

#### 2. 빠른 경로 체크
```cpp
bool BreakpointManager::checkBreakpoint(uint32_t address) {
    // 빠른 체크: 브레이크포인트 없으면 즉시 리턴
    if (m_breakpoints.empty()) return false;
    
    // 해시맵 조회
    auto it = m_breakpoints_by_address.find(address);
    if (it == m_breakpoints_by_address.end()) return false;
    
    // 조건 평가 (느림)
    // ...
}
```

#### 3. 트레이스 버퍼링
```cpp
class TraceLogger {
private:
    std::vector<TraceEntry> m_buffer;
    const size_t BUFFER_SIZE = 10000;
    
    void log(const TraceEntry& entry) {
        m_buffer.push_back(entry);
        
        if (m_buffer.size() >= BUFFER_SIZE) {
            flush();
        }
    }
};
```

#### 4. 선택적 로깅
```cpp
// PC 범위 필터
if (pc >= filter.pc_start && pc <= filter.pc_end) {
    logger->log(pc, instruction);
}

// 샘플링
if (cycle_count % 100 == 0) {
    logger->log(pc, instruction);
}
```

### 벤치마크 목표

| 기능 | 오버헤드 | 목표 |
|-----|---------|------|
| 브레이크포인트 체크 | < 0.1% | 활성화 시에만 |
| 트레이스 로깅 (비활성) | 0% | 컴파일 시 제거 |
| 트레이스 로깅 (활성) | < 10% | 버퍼링으로 최소화 |
| 루프 감지 | < 1% | 임계값 조정 |
| 메모리 워치 | < 0.5% | 해시맵 사용 |

---

## 확장성

### 새 디버깅 도구 추가

1. **인터페이스 정의**
```cpp
class IDebugTool {
public:
    virtual ~IDebugTool() = default;
    virtual void onCycle() = 0;
    virtual void onBreakpoint() = 0;
    virtual std::string report() = 0;
};
```

2. **도구 구현**
```cpp
class MyDebugTool : public IDebugTool {
    // 구현...
};
```

3. **Debugger에 등록**
```cpp
debugger->registerTool(std::make_unique<MyDebugTool>());
```

### 새 명령어 추가

```cpp
// src/debug/cli_debugger.cpp
void CLIDebugger::registerCommand(
    const std::string& name,
    std::function<void(const std::vector<std::string>&)> handler,
    const std::string& help
) {
    m_commands[name] = {handler, help};
}

// 사용
cli.registerCommand("mycmd", [](const auto& args) {
    // 명령어 처리
}, "My custom command");
```

---

## 결론

이 디버거 설계는 다음을 제공합니다:

1. **통합 디버깅 환경**: 모든 도구가 하나의 인터페이스
2. **Agent 친화적**: CLI 기반, 자동화 가능
3. **성능 최적화**: 최소 오버헤드 (< 5%)
4. **확장 가능**: 새 도구 쉽게 추가
5. **실용적**: 실제 디버깅 시나리오 기반

### 다음 단계

1. ✅ 설계 완료
2. [ ] Phase 1 구현 (Week 1)
3. [ ] Phase 2 구현 (Week 2)
4. [ ] Phase 3 구현 (Week 3)
5. [ ] Phase 4 구현 (Week 4)
6. [ ] 문서 완성 및 테스트

---

**작성자**: AI Agent  
**날짜**: 2025-12-14  
**버전**: 1.0  
**상태**: 설계 완료, 구현 대기










