# SNES Emulator Development Guide for Agents

## 📋 Table of Contents
1. [Development Efficiency Strategy](#development-efficiency-strategy)
2. [Terminal-Based Debugging](#terminal-based-debugging)
3. [Data Structuring Strategy](#data-structuring-strategy)
4. [SPC700 Test Failure Analysis](#spc700-test-failure-analysis)
5. [Agent-Friendly Development Environment](#agent-friendly-development-environment)

---

## 🚀 Development Efficiency Strategy

### Problems
- Agents **cannot interact in real-time**
- GUI debuggers cannot be used by agents
- Infinite loop detection is difficult
- Log files are too large to analyze easily

### Solutions

#### 1. **Automatic Infinite Loop Detection System**

```cpp
// src/debug/loop_detector.h
class LoopDetector {
public:
    struct ExecutionSnapshot {
        uint32_t pc;
        uint8_t a, x, y, sp, psw;
        uint32_t timestamp;
    };
    
    // Automatically stop when same state repeats N times
    bool detectLoop(const ExecutionSnapshot& snap, int threshold = 100);
    
    // Automatically dump when loop is entered
    void dumpLoopContext(const std::string& filename);
};
```

**Integration method**:
```cpp
// src/main_complete.cpp
LoopDetector loop_detector;
while (running) {
    if (loop_detector.detectLoop(getCurrentState(), 100)) {
        printf("=== LOOP DETECTED at PC:0x%04X ===\n", cpu.getPC());
        loop_detector.dumpLoopContext("loop_context.txt");
        break;  // Auto exit
    }
}
```

#### 2. **Instruction Execution Limit**

```bash
# Execute maximum 1 million instructions then auto exit
.\snes_emu_complete.exe spctest.sfc --max-instructions 1000000

# Stop when reaching specific PC
.\snes_emu_complete.exe spctest.sfc --break-at 0x0357

# Conditional break (check port value)
.\snes_emu_complete.exe spctest.sfc --break-on "PORT2==0x02"
```

#### 3. **Real-Time Progress Monitoring**

```cpp
// Output progress every 10,000 cycles
if (total_cycles % 10000 == 0) {
    printf("[Progress] Cycles: %lu | PC: 0x%04X | Port2: 0x%02X\n", 
           total_cycles, cpu.getPC(), apu.getPort(2));
    fflush(stdout);
}
```

---

## 🔧 Terminal-Based Debugging

### A. Command-Line Debugger Implementation

#### File: `src/debug/cli_debugger.h`

```cpp
#pragma once
#include <string>
#include <vector>
#include <map>

class CLIDebugger {
public:
    struct BreakCondition {
        enum Type { PC_EQUALS, REGISTER_EQUALS, MEMORY_EQUALS, CUSTOM };
        Type type;
        uint32_t address;
        uint8_t value;
        std::string custom_expr;  // "A==0x56 && X==0x34"
    };
    
    // Breakpoint management
    void addBreakpoint(uint32_t pc);
    void addConditionalBreak(const std::string& condition);
    bool shouldBreak(const CPUState& state);
    
    // Step execution
    void setStepMode(bool enabled, int steps = 1);
    bool shouldStep();
    
    // Log filtering
    void setLogFilter(const std::string& component);  // "CPU", "APU", "PPU"
    void setLogLevel(const std::string& level);       // "DEBUG", "INFO", "WARN"
    
    // Memory dump
    void dumpMemoryRange(uint32_t start, uint32_t end, const std::string& filename);
    void dumpRegisters(const std::string& filename);
    
    // Execution statistics
    void printStatistics();
    
private:
    std::vector<BreakCondition> breakpoints;
    bool step_mode = false;
    int step_count = 0;
    std::map<std::string, bool> log_filters;
};
```

#### Usage Examples

```bash
# 1. Set breakpoint and run
.\snes_emu_complete.exe spctest.sfc --break 0x0357

# 2. Conditional break (when entering fail routine)
.\snes_emu_complete.exe spctest.sfc --break-on "PC==0x0357"

# 3. Step execution (1000 cycles only)
.\snes_emu_complete.exe spctest.sfc --step 1000

# 4. Port value monitoring
.\snes_emu_complete.exe spctest.sfc --watch "PORT[2]" --break-on "PORT[2]==0x02"

# 5. Log filtering (APU only)
.\snes_emu_complete.exe spctest.sfc --log-component APU --log-level INFO
```

### B. Real-Time Monitoring with PowerShell Scripts

#### File: `monitor_execution.ps1`

```powershell
# Run emulator in background and monitor in real-time
param(
    [string]$RomFile = "spctest.sfc",
    [int]$TimeoutSeconds = 60,
    [string]$TargetPattern = "PORT2==0x02"
)

# Background execution
$process = Start-Process -FilePath ".\snes_emu_complete.exe" `
                         -ArgumentList $RomFile `
                         -PassThru `
                         -NoNewWindow `
                         -RedirectStandardOutput "execution_output.txt"

$startTime = Get-Date
$lastSize = 0

Write-Host "=== Monitoring Execution ===" -ForegroundColor Green

while ($process -and !$process.HasExited) {
    Start-Sleep -Milliseconds 500
    
    # Check log file size
    if (Test-Path "apu_trace.log") {
        $currentSize = (Get-Item "apu_trace.log").Length
        
        if ($currentSize -eq $lastSize) {
            Write-Host "[WARNING] Log file stopped growing - possible hang" -ForegroundColor Yellow
            break
        }
        
        $lastSize = $currentSize
        
        # Check last line
        $lastLine = Get-Content "apu_trace.log" -Tail 1
        Write-Host "[LATEST] $lastLine" -ForegroundColor Cyan
        
        # Detect target pattern
        if ($lastLine -match $TargetPattern) {
            Write-Host "[DETECTED] Target pattern found!" -ForegroundColor Green
            break
        }
    }
    
    # Check timeout
    $elapsed = (Get-Date) - $startTime
    if ($elapsed.TotalSeconds -gt $TimeoutSeconds) {
        Write-Host "[TIMEOUT] Execution exceeded $TimeoutSeconds seconds" -ForegroundColor Red
        $process.Kill()
        break
    }
}

Write-Host "=== Execution Complete ===" -ForegroundColor Green
```

#### Usage Examples

```powershell
# Monitor execution with 60 second timeout
.\monitor_execution.ps1 -RomFile "spctest.sfc" -TimeoutSeconds 60

# Detect specific pattern
.\monitor_execution.ps1 -TargetPattern "wrote port 2 = 0x02"
```

---

## 📚 Data Structuring Strategy

### Question: "Do I need to organize SNES-related materials in docs to develop accurately?"

**Answer**: **Yes, it's very important!**

### Current Problems

1. **Scattered information**: Information is scattered across log files, markdown, and source code comments
2. **Incomplete documentation**: SPC700 instruction behavior is not clearly documented
3. **Lack of reference materials**: Expected behavior of test ROMs is not documented

### 📁 Proposed: Structured Documentation System

```
docs/
├── hardware/                    # Hardware specs (for Agent reference)
│   ├── cpu_65c816.md           # CPU instruction set + flag behavior
│   ├── spc700_instructions.md  # ⚠️ Core: Detailed description of all SPC700 opcodes
│   ├── spc700_flags.md         # PSW flag calculation rules
│   ├── dsp_registers.md        # DSP register map
│   └── memory_map.md           # Memory map (LoROM, HiROM)
│
├── test_roms/                   # Test ROM documentation
│   ├── spctest_expected.md     # ⚠️ Expected behavior of spctest.sfc
│   ├── spctest_protocol.md     # CPU-APU communication protocol
│   └── test_failure_cases.md   # Known failure cases
│
├── debugging/                   # Debugging guides
│   ├── loop_detection.md       # Infinite loop detection methods
│   ├── port_communication.md   # Port communication debugging
│   └── common_bugs.md          # Common bug patterns
│
└── agent/
    ├── AGENT_DEVELOPMENT_GUIDE.md  # This document
    └── QUICK_REFERENCE.md          # Quick reference for Agents
```

### ⚠️ **Immediately Needed Documents**

#### 1. `docs/hardware/spc700_instructions.md`

```markdown
# SPC700 Instruction Set Detailed Documentation

## CMP (Compare) - Opcode 0x68, 0x78, etc.

### Operation
```
temp = A - operand
```

### Flag Effects
- **N (Negative)**: bit 7 of result
- **Z (Zero)**: result == 0
- **C (Carry)**: A >= operand (unsigned)
  - C=1: A >= operand
  - C=0: A < operand

### Example
```
A = 0x56, operand = 0x01
temp = 0x56 - 0x01 = 0x55
N = 0 (bit 7 = 0)
Z = 0 (result != 0)
C = 1 (0x56 >= 0x01)
```

### ⚠️ Notes
- CMP does not modify A register
- Does not use borrow (C flag is opposite)
```

#### 2. `docs/test_roms/spctest_expected.md`

```markdown
# spctest.sfc Expected Behavior

## Test Protocol

### 1. Initialization Phase
1. CPU writes 0xCC to port 0
2. CPU writes 0x01 to port 1 (test start)
3. SPC reads port 0 and confirms 0xCC
4. SPC reads port 1 and confirms 0x01
5. **SPC writes 0x00 to port 0 (ready)**

### 2. Test Execution
- When each test succeeds, SPC writes test number to port 2
- **On failure: port 2 = 0x02 or 0x03**

### 3. Success Condition
- After all tests pass, port 0 = 0xFF
- On failure, port 0 = 0x00 maintained

## Current Failure Point

**Symptom**: Infinite loop at PC:0x0357 (`BRA $FE`)
**Cause**: Entered `fail` routine
**Port 2 value**: 0x03 (fail marker)

## Debugging Checklist

- [ ] Check if port 0 is 0xCC
- [ ] Check if port 1 is 0x01
- [ ] Verify CMP instruction flag calculation
- [ ] Verify BEQ/BNE branch conditions
```

---

## 🐛 SPC700 Test Failure Analysis

### Current Status

```
PC:0x0357 | 2f fe | BRA rel | operand=0xfe
A:0x56 | X:0x34 | Y:0x56 | SP:0xef | PSW:0x01
```

**Analysis**:
- `BRA $FE` = PC - 2, i.e., jump to itself (infinite loop)
- This is the `fail` routine
- PSW:0x01 = Only Carry flag is set

### Estimated Failure Causes

#### Possibility 1: CMP Instruction Flag Calculation Error

```cpp
// src/apu/apu.cpp - CMP implementation needs verification
case 0x68:  // CMP A, #imm
    operand = readByte(pc++);
    temp = a - operand;
    
    // ⚠️ Verify Carry flag calculation
    setFlag(FLAG_C, a >= operand);  // unsigned comparison
    setFlag(FLAG_Z, temp == 0);
    setFlag(FLAG_N, temp & 0x80);
    break;
```

#### Possibility 2: Port Read Timing Issue

```cpp
// SPC reads port value before CPU writes it
// Or CPU cannot read value written by SPC
```

### 🔍 Debugging Procedure

#### Step 1: Verify CMP Instruction

```bash
# 1. Extract only CMP instructions from log
Get-Content apu_trace.log | Select-String "cmp" | Select-Object -First 50

# 2. Track flag changes
Get-Content apu_trace.log | Select-String "PSW:" | Select-Object -Last 100
```

#### Step 2: Verify Port Communication

```bash
# Extract only port read/write events
Get-Content port_comm.log | Select-String "port"
```

#### Step 3: Verify Conditional Branches

```bash
# Track BEQ/BNE instructions
Get-Content apu_trace.log | Select-String "beq|bne" -CaseSensitive
```

---

## 💡 Agent-Friendly Development Environment

### 1. **Automatic Test Scripts**

#### File: `run_test_suite.ps1`

```powershell
# Automatically run all test ROMs and collect results
$testRoms = @("cputest-basic.sfc", "cputest-full.sfc", "spctest.sfc")

foreach ($rom in $testRoms) {
    Write-Host "Testing: $rom" -ForegroundColor Yellow
    
    # Execute
    .\snes_emu_complete.exe $rom --max-instructions 1000000 > "test_$rom.log" 2>&1
    
    # Analyze results
    $lastLine = Get-Content "test_$rom.log" -Tail 1
    
    if ($lastLine -match "SUCCESS") {
        Write-Host "  ✅ PASSED" -ForegroundColor Green
    } else {
        Write-Host "  ❌ FAILED" -ForegroundColor Red
    }
}
```

### 2. **Automatic Comparison Tests**

```powershell
# File: compare_traces.ps1
# Compare with normal execution log

param(
    [string]$GoldenTrace = "golden_apu_trace.log",
    [string]$CurrentTrace = "apu_trace.log"
)

# Find first difference
$golden = Get-Content $GoldenTrace
$current = Get-Content $CurrentTrace

for ($i = 0; $i -lt [Math]::Min($golden.Count, $current.Count); $i++) {
    if ($golden[$i] -ne $current[$i]) {
        Write-Host "First difference at line $i:" -ForegroundColor Red
        Write-Host "Golden:  $($golden[$i])" -ForegroundColor Yellow
        Write-Host "Current: $($current[$i])" -ForegroundColor Cyan
        break
    }
}
```

### 3. **Automatic Code Modification Verification**

```powershell
# File: verify_fix.ps1
# Automatically build and test after code modification

param(
    [string]$TestRom = "spctest.sfc"
)

Write-Host "1. Building..." -ForegroundColor Yellow
.\build_complete.bat > build_log.txt 2>&1

if ($LASTEXITCODE -ne 0) {
    Write-Host "❌ Build failed" -ForegroundColor Red
    Get-Content build_log.txt -Tail 20
    exit 1
}

Write-Host "2. Running test..." -ForegroundColor Yellow
.\snes_emu_complete.exe $TestRom --max-instructions 1000000 > test_output.txt 2>&1

Write-Host "3. Analyzing results..." -ForegroundColor Yellow
$lastLines = Get-Content test_output.txt -Tail 10

if ($lastLines -match "SUCCESS") {
    Write-Host "✅ Test PASSED!" -ForegroundColor Green
} else {
    Write-Host "❌ Test FAILED" -ForegroundColor Red
    Write-Host "Last output:" -ForegroundColor Yellow
    $lastLines | ForEach-Object { Write-Host "  $_" }
}
```

---

## 🎯 Immediately Executable Improvements

### Priority 1: Automatic Infinite Loop Detection

```cpp
// Add to src/main_complete.cpp
#include <map>

std::map<uint32_t, int> pc_visit_count;
const int LOOP_THRESHOLD = 100;

while (running) {
    uint32_t current_pc = apu.getPC();
    
    pc_visit_count[current_pc]++;
    
    if (pc_visit_count[current_pc] > LOOP_THRESHOLD) {
        printf("=== INFINITE LOOP DETECTED at PC:0x%04X ===\n", current_pc);
        printf("A:0x%02X X:0x%02X Y:0x%02X PSW:0x%02X\n",
               apu.getA(), apu.getX(), apu.getY(), apu.getPSW());
        break;
    }
}
```

### Priority 2: Instruction Counter

```cpp
// src/main_complete.cpp - command-line argument handling
int max_instructions = 1000000;  // default

if (argc >= 3 && strcmp(argv[2], "--max-instructions") == 0) {
    max_instructions = atoi(argv[3]);
}

int instruction_count = 0;
while (running && instruction_count < max_instructions) {
    // ... execution ...
    instruction_count++;
}

printf("Total instructions executed: %d\n", instruction_count);
```

### Priority 3: Progress Output

```cpp
// Output every 10,000 cycles
if (total_cycles % 10000 == 0) {
    printf("[%lu] PC:0x%04X | Port2:0x%02X\r", 
           total_cycles, apu.getPC(), apu.getPort(2));
    fflush(stdout);
}
```

---

## 📝 Conclusion

### For Agents to efficiently develop SNES emulator:

1. ✅ **Fully controllable debugging system from terminal**
2. ✅ **Automatic infinite loop detection and stop**
3. ✅ **Clear hardware spec documentation** (especially SPC700)
4. ✅ **Test ROM expected behavior documentation**
5. ✅ **Automated test and comparison scripts**

### Next Steps:

1. Write `docs/hardware/spc700_instructions.md`
2. Implement `src/debug/cli_debugger.h`
3. Create `monitor_execution.ps1` script
4. Add automatic infinite loop detection
5. Verify CMP instruction flag calculation

---

**Written**: 2025-12-14  
**Version**: 1.0  
**Target**: AI Agent Developers
