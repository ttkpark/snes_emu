# Port Communication Analysis - CPU-APU 통신 디버깅

## 📋 목차
1. [개요](#개요)
2. [포트 구조](#포트-구조)
3. [통신 패턴 분석](#통신-패턴-분석)
4. [일반적인 프로토콜](#일반적인-프로토콜)
5. [디버깅 도구](#디버깅-도구)
6. [문제 해결](#문제-해결)

---

## 개요

CPU와 APU는 **4개의 포트** ($2140-$2143, $F4-$F7)를 통해 통신합니다. 이 문서는 포트 통신을 분석하고 디버깅하는 방법을 설명합니다.

---

## 포트 구조

### CPU 측 레지스터

```
$2140 (APUIO0): CPU → APU Port 0
$2141 (APUIO1): CPU → APU Port 1
$2142 (APUIO2): CPU → APU Port 2
$2143 (APUIO3): CPU → APU Port 3
```

### SPC700 측 레지스터

```
$F4: SPC → CPU Port 0
$F5: SPC → CPU Port 1
$F6: SPC → CPU Port 2
$F7: SPC → CPU Port 3
```

---

## 통신 패턴 분석

### 핸드셰이킹 패턴

```cpp
// CPU → APU 명령 전송
void sendCommandToAPU(uint8_t cmd, uint8_t data) {
    // 1. 명령 쓰기
    write($2140, cmd);
    write($2141, data);
    
    // 2. ACK 대기
    while (read($2140) != 0xAA) {
        // SPC가 0xAA 응답할 때까지 대기
    }
}

// SPC700 측
void receiveCommand() {
    // 1. 명령 읽기
    uint8_t cmd = read($F4);
    uint8_t data = read($F5);
    
    // 2. 처리
    processCommand(cmd, data);
    
    // 3. ACK 전송
    write($F4, 0xAA);
}
```

---

## 일반적인 프로토콜

### IPL ROM 업로드

```
1. CPU → Port 0: 0xCC (Ready 신호)
2. CPU ← Port 0: 0xCC (SPC Ready)
3. CPU → Port 0: 0x01 (전송 시작)
4. CPU → Port 1: 주소 Low
5. CPU → Port 2: 주소 High
6. Loop:
   CPU → Port 1: 데이터
   CPU ← Port 0: ACK
7. CPU → Port 0: 0x00 (전송 끝)
```

### 음악 재생

```
1. CPU → Port 0: 음악 번호
2. CPU → Port 1: 볼륨
3. SPC ← 명령 처리
4. SPC → Port 0: 0x00 (완료)
```

---

## 디버깅 도구

### 포트 로거

```cpp
class PortLogger {
private:
    struct PortEvent {
        uint64_t cycle;
        uint8_t port;
        uint8_t value;
        bool isCPU;  // true=CPU, false=SPC
    };
    
    std::vector<PortEvent> log;
    
public:
    void logWrite(uint8_t port, uint8_t value, bool isCPU, uint64_t cycle) {
        log.push_back({cycle, port, value, isCPU});
        
        printf("[%10lld] %s wrote Port %d = 0x%02X\n",
               cycle, isCPU ? "CPU" : "SPC", port, value);
    }
    
    void saveToFile(const char* filename) {
        FILE* f = fopen(filename, "w");
        fprintf(f, "Cycle,Source,Port,Value\n");
        for (auto& e : log) {
            fprintf(f, "%lld,%s,%d,0x%02X\n",
                    e.cycle, e.isCPU ? "CPU" : "SPC", e.port, e.value);
        }
        fclose(f);
    }
    
    void analyze() {
        printf("=== Port Communication Analysis ===\n");
        
        // 패턴 검출
        detectHandshakes();
        detectStalls();
        detectProtocols();
    }
    
private:
    void detectHandshakes() {
        // CPU가 쓰고 SPC가 응답하는 패턴 찾기
        for (size_t i = 0; i < log.size() - 1; i++) {
            if (log[i].isCPU && !log[i+1].isCPU &&
                log[i].port == log[i+1].port) {
                printf("Handshake: Port %d, CPU:0x%02X → SPC:0x%02X (delay: %lld cycles)\n",
                       log[i].port, log[i].value, log[i+1].value,
                       log[i+1].cycle - log[i].cycle);
            }
        }
    }
    
    void detectStalls() {
        // CPU가 같은 포트를 반복 읽는 경우 (대기)
        int repeatCount = 0;
        uint8_t lastPort = 0xFF;
        
        for (auto& e : log) {
            if (e.isCPU && e.port == lastPort) {
                repeatCount++;
                if (repeatCount > 100) {
                    printf("WARNING: CPU stalled on Port %d at cycle %lld\n",
                           e.port, e.cycle);
                    break;
                }
            } else {
                repeatCount = 0;
                lastPort = e.port;
            }
        }
    }
    
    void detectProtocols() {
        // IPL 업로드 패턴 검출
        if (log.size() > 3 &&
            log[0].value == 0xCC && log[1].value == 0xCC) {
            printf("Detected: IPL ROM upload protocol\n");
        }
    }
};
```

### 실시간 모니터

```cpp
class PortMonitor {
private:
    uint8_t cpuPorts[4] = {0};
    uint8_t spcPorts[4] = {0};
    
public:
    void display() {
        printf("┌─────────────────────────────────┐\n");
        printf("│   CPU-APU Port Communication    │\n");
        printf("├─────────────────────────────────┤\n");
        printf("│ Port │  CPU → APU │ APU → CPU  │\n");
        printf("├──────┼────────────┼────────────┤\n");
        for (int i = 0; i < 4; i++) {
            printf("│  %d   │    0x%02X    │    0x%02X    │\n",
                   i, cpuPorts[i], spcPorts[i]);
        }
        printf("└─────────────────────────────────┘\n");
    }
    
    void update(uint8_t port, uint8_t value, bool isCPU) {
        if (isCPU) {
            cpuPorts[port] = value;
        } else {
            spcPorts[port] = value;
        }
    }
};
```

---

## 문제 해결

### 1. CPU가 무한 대기

**증상**: CPU가 포트 응답을 기다리며 멈춤

```cpp
// CPU 코드
while (read($2140) != 0xAA) {
    // 무한 루프!
}
```

**원인**:
- SPC가 응답하지 않음
- SPC가 다른 값을 보냄
- 포트 읽기 타이밍 문제

**해결**:
```cpp
// 타임아웃 추가
int timeout = 10000;
while (read($2140) != 0xAA && timeout-- > 0) {
    // ...
}
if (timeout == 0) {
    printf("ERROR: SPC timeout!\n");
}
```

### 2. SPC가 명령을 받지 못함

**증상**: SPC가 포트 값을 읽지 않음

**원인**:
- SPC가 다른 작업 중
- 포트 인터럽트 비활성화
- 타이밍 문제

**해결**:
```asm
; SPC700: 포트 체크 루프
CheckPorts:
    MOV A, $F4
    CMP A, LastPort0
    BNE NewCommand
    ; ...
```

### 3. 데이터 동기화 실패

**증상**: CPU와 SPC의 포트 값이 다름

**원인**:
- 쓰기/읽기 순서 문제
- 캐시 무효화 안 됨

**해결**:
```cpp
// 쓰기 후 읽기 확인
write($2140, value);
uint8_t readback = read($2140);
if (readback != value) {
    printf("Port write failed!\n");
}
```

---

## 예제: spctest.sfc 분석

### 포트 로그

```
[00000100] CPU wrote Port 0 = 0xCC
[00000150] SPC wrote Port 0 = 0xCC
[00000200] CPU wrote Port 0 = 0x00
[00000250] SPC wrote Port 2 = 0x00  ← 테스트 시작
[00001000] SPC wrote Port 2 = 0xF0  ← 테스트 0 시작
[00010000] SPC wrote Port 2 = 0x03  ← FAIL!
[00010001] SPC at PC:0x0357 (infinite loop)
```

### 분석 결과

```
1. IPL 업로드 성공 (0xCC 핸드셰이크)
2. 테스트 시작 (Port 2 = 0x00)
3. 첫 번째 테스트 실패 (Port 2 = 0x03)
4. fail 루틴 진입 (PC:0x0357)
```

---

## PowerShell 스크립트

### 포트 로그 분석

```powershell
# analyze_ports.ps1
$log = Get-Content "port_log.txt"

$cpuWrites = @{}
$spcWrites = @{}

foreach ($line in $log) {
    if ($line -match '\[(\d+)\] (CPU|SPC) wrote Port (\d) = (0x[0-9A-F]+)') {
        $cycle = [int64]$matches[1]
        $source = $matches[2]
        $port = [int]$matches[3]
        $value = $matches[4]
        
        if ($source -eq "CPU") {
            $cpuWrites[$port] = @{Cycle=$cycle; Value=$value}
        } else {
            $spcWrites[$port] = @{Cycle=$cycle; Value=$value}
        }
    }
}

# 핸드셰이크 검출
Write-Host "=== Handshakes ==="
for ($p = 0; $p -lt 4; $p++) {
    if ($cpuWrites[$p] -and $spcWrites[$p]) {
        $delay = $spcWrites[$p].Cycle - $cpuWrites[$p].Cycle
        Write-Host "Port $p: CPU -> SPC (delay: $delay cycles)"
    }
}
```

---

## 참고 자료

- [SnesLab - APU Ports](https://sneslab.net/wiki/APU_ports)
- [Fullsnes - CPU-APU Communication](https://problemkaputt.de/fullsnes.htm#snesapuaudioprocessingunit)

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete










