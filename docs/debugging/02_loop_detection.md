# 무한 루프 감지 및 분석

## 개요

무한 루프는 에뮬레이터 개발에서 가장 흔한 문제 중 하나입니다. 이 문서는 무한 루프를 자동으로 감지하고 분석하는 시스템을 설명합니다.

---

## 문제 정의

### 무한 루프란?

프로그램이 동일한 코드 경로를 무한히 반복하는 상태입니다.

#### 예제 1: 단순 무한 루프
```asm
fail:
    BRA fail    ; PC:0x0357, opcode: 2F FE
                ; offset = -2, 자기 자신으로 점프
```

#### 예제 2: 조건 대기 루프
```asm
wait_port:
    LDA $F4         ; PC:0x0343, 포트 0 읽기
    CMP #$CC        ; 0xCC와 비교
    BNE wait_port   ; PC:0x0346, 같지 않으면 반복
```

### 감지 기준

1. **PC 방문 빈도**: 동일한 PC를 N회 이상 실행
2. **상태 불변성**: 레지스터 상태가 변하지 않음
3. **외부 변화 없음**: 메모리나 I/O 상태 변화 없음

---

## 알고리즘

### 기본 알고리즘

```cpp
class LoopDetector {
private:
    struct State {
        uint8_t a, x, y, sp, psw;
        
        bool operator==(const State& other) const {
            return a == other.a && x == other.x && 
                   y == other.y && sp == other.sp && 
                   psw == other.psw;
        }
    };
    
    std::map<uint32_t, int> pc_visit_count;
    std::map<uint32_t, std::vector<State>> pc_states;
    int threshold;

public:
    LoopDetector(int threshold = 100) : threshold(threshold) {}
    
    bool detectLoop(uint32_t pc, const State& current_state) {
        // 방문 횟수 증가
        pc_visit_count[pc]++;
        
        // 상태 저장
        pc_states[pc].push_back(current_state);
        
        // 임계값 초과 확인
        if (pc_visit_count[pc] > threshold) {
            // 상태 변화 분석
            auto& states = pc_states[pc];
            std::set<State> unique_states(states.begin(), states.end());
            
            if (unique_states.size() == 1) {
                // 동일한 상태로 반복 = 확실한 무한 루프
                return true;
            } else if (unique_states.size() <= 3) {
                // 몇 개의 상태만 반복 = 의심스러운 루프
                return shouldInvestigate(pc, unique_states);
            }
        }
        
        return false;
    }
    
private:
    bool shouldInvestigate(uint32_t pc, const std::set<State>& states) {
        // 상태가 2-3개만 순환하는 경우 (조건 대기 루프)
        // 추가 분석 필요
        return states.size() <= 3;
    }
};
```

### 고급 알고리즘: 패턴 인식

```cpp
class PatternDetector {
private:
    std::deque<uint32_t> pc_history;
    const size_t MAX_HISTORY = 1000;
    
public:
    bool detectPattern() {
        if (pc_history.size() < 20) return false;
        
        // 2-10 길이의 반복 패턴 찾기
        for (int pattern_len = 2; pattern_len <= 10; pattern_len++) {
            if (isRepeatingPattern(pattern_len)) {
                return true;
            }
        }
        
        return false;
    }
    
private:
    bool isRepeatingPattern(int len) {
        // 최근 len*3 길이가 len 패턴의 3번 반복인지 확인
        if (pc_history.size() < len * 3) return false;
        
        auto it = pc_history.rbegin();
        std::vector<uint32_t> pattern1(it, it + len);
        std::vector<uint32_t> pattern2(it + len, it + len*2);
        std::vector<uint32_t> pattern3(it + len*2, it + len*3);
        
        return pattern1 == pattern2 && pattern2 == pattern3;
    }
    
    void addPC(uint32_t pc) {
        pc_history.push_back(pc);
        if (pc_history.size() > MAX_HISTORY) {
            pc_history.pop_front();
        }
    }
};
```

---

## C++ 구현

### 헤더 파일: `src/debug/loop_detector.h`

```cpp
#pragma once
#include <cstdint>
#include <map>
#include <vector>
#include <set>
#include <deque>
#include <functional>

class LoopDetector {
public:
    struct State {
        uint8_t a, x, y, sp, psw;
        
        bool operator==(const State& other) const;
        bool operator<(const State& other) const;
    };
    
    struct LoopInfo {
        uint32_t pc;
        int visit_count;
        std::vector<State> unique_states;
        bool is_definite_loop;
        std::string description;
    };
    
    LoopDetector(int threshold = 100, bool auto_break = true);
    
    // 루프 감지
    bool detectLoop(uint32_t pc, const State& state);
    
    // 분석 결과
    std::vector<LoopInfo> getLoopInfo() const;
    LoopInfo getLoopAt(uint32_t pc) const;
    
    // 설정
    void setThreshold(int threshold);
    void setAutoBreak(bool enable);
    void setCallback(std::function<void(const LoopInfo&)> callback);
    
    // 리셋
    void reset();
    void clearPC(uint32_t pc);
    
    // 통계
    int getTotalLoops() const;
    uint32_t getMostFrequentPC() const;
    
private:
    std::map<uint32_t, int> pc_visit_count;
    std::map<uint32_t, std::vector<State>> pc_states;
    std::deque<uint32_t> pc_history;
    
    int threshold;
    bool auto_break;
    std::function<void(const LoopInfo&)> callback;
    
    bool isRepeatingPattern(int pattern_len);
    void triggerLoopDetected(const LoopInfo& info);
};
```

### 소스 파일: `src/debug/loop_detector.cpp`

```cpp
#include "loop_detector.h"
#include <algorithm>
#include <sstream>

bool LoopDetector::State::operator==(const State& other) const {
    return a == other.a && x == other.x && 
           y == other.y && sp == other.sp && 
           psw == other.psw;
}

bool LoopDetector::State::operator<(const State& other) const {
    if (a != other.a) return a < other.a;
    if (x != other.x) return x < other.x;
    if (y != other.y) return y < other.y;
    if (sp != other.sp) return sp < other.sp;
    return psw < other.psw;
}

LoopDetector::LoopDetector(int threshold, bool auto_break)
    : threshold(threshold), auto_break(auto_break) {}

bool LoopDetector::detectLoop(uint32_t pc, const State& state) {
    // 히스토리 업데이트
    pc_history.push_back(pc);
    if (pc_history.size() > 1000) {
        pc_history.pop_front();
    }
    
    // 방문 카운트 증가
    pc_visit_count[pc]++;
    pc_states[pc].push_back(state);
    
    // 임계값 체크
    if (pc_visit_count[pc] > threshold) {
        // 고유 상태 계산
        std::set<State> unique_states(
            pc_states[pc].begin(), 
            pc_states[pc].end()
        );
        
        LoopInfo info;
        info.pc = pc;
        info.visit_count = pc_visit_count[pc];
        info.unique_states.assign(unique_states.begin(), unique_states.end());
        
        if (unique_states.size() == 1) {
            // 확실한 무한 루프
            info.is_definite_loop = true;
            info.description = "Infinite loop detected: same state repeated";
            
            triggerLoopDetected(info);
            return true;
            
        } else if (unique_states.size() <= 3) {
            // 의심스러운 루프
            info.is_definite_loop = false;
            info.description = "Possible loop: limited state variation";
            
            triggerLoopDetected(info);
            return auto_break;  // auto_break 설정에 따라 중단
        }
    }
    
    // 패턴 감지
    if (pc_history.size() >= 30) {
        for (int len = 2; len <= 10; len++) {
            if (isRepeatingPattern(len)) {
                LoopInfo info;
                info.pc = pc;
                info.visit_count = pc_visit_count[pc];
                info.is_definite_loop = false;
                info.description = "Repeating pattern detected (length " + 
                                  std::to_string(len) + ")";
                
                triggerLoopDetected(info);
                return auto_break;
            }
        }
    }
    
    return false;
}

bool LoopDetector::isRepeatingPattern(int len) {
    if (pc_history.size() < len * 3) return false;
    
    auto it = pc_history.rbegin();
    std::vector<uint32_t> p1(it, it + len);
    std::vector<uint32_t> p2(it + len, it + len*2);
    std::vector<uint32_t> p3(it + len*2, it + len*3);
    
    return p1 == p2 && p2 == p3;
}

void LoopDetector::triggerLoopDetected(const LoopInfo& info) {
    if (callback) {
        callback(info);
    }
    
    // 로그 출력
    fprintf(stderr, "\n=== LOOP DETECTED ===\n");
    fprintf(stderr, "PC: 0x%04X\n", info.pc);
    fprintf(stderr, "Visit count: %d\n", info.visit_count);
    fprintf(stderr, "Unique states: %zu\n", info.unique_states.size());
    fprintf(stderr, "Type: %s\n", info.is_definite_loop ? "DEFINITE" : "POSSIBLE");
    fprintf(stderr, "Description: %s\n", info.description.c_str());
    
    if (info.unique_states.size() <= 5) {
        fprintf(stderr, "States:\n");
        for (const auto& s : info.unique_states) {
            fprintf(stderr, "  A:0x%02X X:0x%02X Y:0x%02X SP:0x%02X PSW:0x%02X\n",
                   s.a, s.x, s.y, s.sp, s.psw);
        }
    }
    fprintf(stderr, "=====================\n");
}

std::vector<LoopDetector::LoopInfo> LoopDetector::getLoopInfo() const {
    std::vector<LoopInfo> result;
    
    for (const auto& [pc, count] : pc_visit_count) {
        if (count > threshold) {
            LoopInfo info;
            info.pc = pc;
            info.visit_count = count;
            
            const auto& states = pc_states.at(pc);
            std::set<State> unique(states.begin(), states.end());
            info.unique_states.assign(unique.begin(), unique.end());
            info.is_definite_loop = (unique.size() == 1);
            
            result.push_back(info);
        }
    }
    
    // 방문 횟수로 정렬
    std::sort(result.begin(), result.end(), 
             [](const LoopInfo& a, const LoopInfo& b) {
                 return a.visit_count > b.visit_count;
             });
    
    return result;
}

void LoopDetector::reset() {
    pc_visit_count.clear();
    pc_states.clear();
    pc_history.clear();
}

uint32_t LoopDetector::getMostFrequentPC() const {
    if (pc_visit_count.empty()) return 0;
    
    auto max_it = std::max_element(
        pc_visit_count.begin(), 
        pc_visit_count.end(),
        [](const auto& a, const auto& b) {
            return a.second < b.second;
        }
    );
    
    return max_it->first;
}
```

---

## 에뮬레이터 통합

### main_complete.cpp 통합

```cpp
#include "debug/loop_detector.h"

int main(int argc, char** argv) {
    // ... 초기화 ...
    
    LoopDetector loop_detector(100, true);  // threshold=100, auto_break=true
    
    // 콜백 설정
    loop_detector.setCallback([&](const LoopDetector::LoopInfo& info) {
        printf("Loop detected, dumping state...\n");
        
        // 상태 덤프
        FILE* dump = fopen("loop_dump.txt", "w");
        fprintf(dump, "=== Loop Information ===\n");
        fprintf(dump, "PC: 0x%04X\n", info.pc);
        fprintf(dump, "Visit count: %d\n", info.visit_count);
        fprintf(dump, "CPU State:\n");
        fprintf(dump, "  A: 0x%04X\n", cpu->getA());
        fprintf(dump, "  X: 0x%04X\n", cpu->getX());
        fprintf(dump, "  Y: 0x%04X\n", cpu->getY());
        // ... 더 많은 정보 ...
        fclose(dump);
    });
    
    // 메인 루프
    while (running) {
        uint32_t pc = apu->getPC();
        LoopDetector::State state{
            apu->getA(),
            apu->getX(),
            apu->getY(),
            apu->getSP(),
            apu->getPSW()
        };
        
        if (loop_detector.detectLoop(pc, state)) {
            printf("=== INFINITE LOOP DETECTED ===\n");
            printf("Breaking execution...\n");
            break;
        }
        
        // 정상 실행
        apu->step();
        cpu->step();
        ppu->step();
    }
    
    // 루프 정보 출력
    auto loops = loop_detector.getLoopInfo();
    printf("\n=== Loop Analysis ===\n");
    printf("Total suspicious loops: %zu\n", loops.size());
    for (const auto& info : loops) {
        printf("PC: 0x%04X, count: %d, states: %zu\n",
               info.pc, info.visit_count, info.unique_states.size());
    }
    
    return 0;
}
```

---

## PowerShell 스크립트

### analyze_loop.ps1 (이미 작성됨)

```powershell
param(
    [string]$TraceFile = "apu_trace.log",
    [int]$ThresholdCount = 100,
    [int]$TailLines = 10000
)

# (이전에 작성한 코드)
```

### 사용법

```powershell
# 기본 사용
.\analyze_loop.ps1

# 낮은 임계값으로 민감하게 감지
.\analyze_loop.ps1 -ThresholdCount 50

# 더 많은 로그 분석
.\analyze_loop.ps1 -TailLines 50000

# 특정 트레이스 파일 분석
.\analyze_loop.ps1 -TraceFile "old_apu_trace.log"
```

---

## 사용 예제

### 시나리오 1: spctest.sfc 무한 루프

#### 문제
```
[Cyc:0000885366] SPC700 PC:0x0357 | 2f fe | BRA rel
```

#### 분석
```powershell
PS> .\analyze_loop.ps1 -ThresholdCount 50

=== Infinite Loop Detector ===
Analyzing last 10000 lines...

Top 10 Most Executed PC Addresses:
[LOOP] PC: 0x0357 - Executed 63 times
   Unique states: 1
   WARNING: Always in same state: A:56 X:34 Y:56 PSW:01
   This is likely an infinite loop!
   Instruction: 2f fe - BRA

Recommendations:
   1. Check the instruction at the looping PC address
   2. Verify flag calculations
   3. Check if the expected state change is happening
```

#### 해결
```cpp
// 0x0357은 fail 루틴
// 원인: 포트 초기화 문제

// 수정
memory->write(0x2140, 0xCC);
memory->write(0x2141, 0x01);
```

### 시나리오 2: 조건 대기 루프

#### 문제
```
wait_vblank:
    LDA $4212
    AND #$80
    BEQ wait_vblank
```

#### 분석
```
[LOOP] PC: 0x8024 - Executed 300 times
   Unique states: 2
   States:
      A:00 X:00 Y:00 PSW:02  (Z=1)
      A:80 X:00 Y:00 PSW:80  (N=1)
```

#### 판단
- 이것은 정상적인 VBlank 대기 루프
- 상태가 변하고 있음 (A가 0x00과 0x80 사이 변화)
- 무한 루프 아님

---

## 고급 기능

### 1. 조건부 루프 감지

```cpp
loop_detector.setCallback([&](const LoopDetector::LoopInfo& info) {
    // 특정 PC만 처리
    if (info.pc == 0x0357) {
        // fail 루틴 진입 = 테스트 실패
        exit(1);
    }
});
```

### 2. 타임아웃 기반 감지

```cpp
auto start_time = std::chrono::steady_clock::now();
const int MAX_SECONDS = 60;

while (running) {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
        now - start_time
    ).count();
    
    if (elapsed > MAX_SECONDS) {
        printf("TIMEOUT: Execution exceeded %d seconds\n", MAX_SECONDS);
        
        // 루프 분석
        auto loops = loop_detector.getLoopInfo();
        if (!loops.empty()) {
            printf("Most frequent PC: 0x%04X (%d visits)\n",
                   loops[0].pc, loops[0].visit_count);
        }
        
        break;
    }
    
    // ... 실행 ...
}
```

### 3. 메모리 변화 추적

```cpp
class MemoryWatchingLoopDetector : public LoopDetector {
private:
    std::map<uint32_t, std::vector<uint8_t>> memory_snapshots;
    std::vector<uint32_t> watch_addresses;
    
public:
    void addWatch(uint32_t addr) {
        watch_addresses.push_back(addr);
    }
    
    bool detectLoop(uint32_t pc, const State& state, Memory* mem) {
        // 감시 중인 메모리 캡처
        std::vector<uint8_t> snapshot;
        for (uint32_t addr : watch_addresses) {
            snapshot.push_back(mem->read(addr));
        }
        memory_snapshots[pc].push_back(snapshot);
        
        // 기본 루프 감지
        if (LoopDetector::detectLoop(pc, state)) {
            // 메모리도 변하지 않는지 확인
            auto& snapshots = memory_snapshots[pc];
            std::set<std::vector<uint8_t>> unique_mem(
                snapshots.begin(), snapshots.end()
            );
            
            if (unique_mem.size() == 1) {
                // 레지스터도 메모리도 변하지 않음 = 확실한 무한 루프
                return true;
            }
        }
        
        return false;
    }
};
```

---

## 성능 고려사항

### 오버헤드 측정

```cpp
// 벤치마크
auto start = std::chrono::high_resolution_clock::now();

for (int i = 0; i < 1000000; i++) {
    loop_detector.detectLoop(0x8000 + (i % 100), state);
}

auto end = std::chrono::high_resolution_clock::now();
auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

printf("1M loop checks: %lld us (%.2f ns per check)\n",
       duration.count(), 
       duration.count() * 1000.0 / 1000000.0);
```

### 최적화 팁

1. **임계값 조정**: 높은 임계값 = 낮은 오버헤드
2. **히스토리 제한**: `MAX_HISTORY` 줄이기
3. **샘플링**: 매 N 사이클마다만 체크
4. **조건부 활성화**: 의심스러운 경우만 활성화

---

## 참고 자료

- `analyze_loop.ps1` - PowerShell 구현
- `docs/case_studies/spctest_debugging.md` - 실제 사례
- `docs/debugging/03_trace_logging.md` - 트레이스 로깅

---

**최종 업데이트**: 2025-12-14  
**구현 상태**: C++ 설계 완료, PowerShell 스크립트 구현됨  
**테스트 상태**: spctest.sfc에서 검증됨










