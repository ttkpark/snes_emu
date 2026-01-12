# SNES Emulator Quick Reference for Agents

## 🚀 Quick Commands

### Build and Run
```powershell
# Build
.\build_complete.bat

# Run (no timeout)
.\snes_emu_complete.exe spctest.sfc

# Background execution + monitoring
.\monitor_execution.ps1 -TimeoutSeconds 30 -DetectLoop
```

### Log Analysis
```powershell
# Detect infinite loop
.\analyze_loop.ps1

# Check last 50 lines
Get-Content apu_trace.log -Tail 50

# Search for specific pattern
Get-Content apu_trace.log | Select-String "fail|0x0357"

# Check port communication
Get-Content port_comm.log | Select-String "port"
```

---

## 📍 Key Addresses and Constants

### SPC700 Memory Map
```
0x0000-0x00EF: Zero Page + Stack
0x0100-0x01FF: Stack + I/O
0x0200-0xFFBF: RAM (64KB)
0xFFC0-0xFFFF: IPL ROM (boot code)

I/O Ports:
0x00F4-0x00F7: CPU communication ports
0x00F0-0x00FF: DSP registers
```

### CPU-APU Ports
```
CPU side: 0x2140-0x2143
APU side: 0x00F4-0x00F7

Port 0: Control/Status
Port 1: Command
Port 2: Test result
Port 3: Reserved
```

### spctest.sfc Key Addresses
```
0x0300: main routine start
0x0350: success routine (infinite loop)
0x0355: fail routine start
0x0357: fail infinite loop (BRA -2)
```

---

## 🔍 Debugging Checklist

### When Infinite Loop Detected

```powershell
# 1. Check which address
Get-Content apu_trace.log -Tail 10

# 2. If 0x0357 -> fail routine
#    Port communication problem or instruction bug

# 3. Check port values
Get-Content port_comm.log | Select-String "port 2"

# 4. Check state before loop entry
Get-Content apu_trace.log | Select-String "0x035" -Context 10,0
```

### CMP Instruction Verification

```powershell
# Check CMP execution log
Get-Content apu_trace.log | Select-String "cmp" -CaseSensitive | Select-Object -First 20

# Check flag changes
# Track PSW:0xXX values
```

**CMP Flag Calculation Formula**:
```cpp
temp = A - operand;
C = (A >= operand) ? 1 : 0;  // NOT (A < operand)!
Z = (temp == 0) ? 1 : 0;
N = (temp & 0x80) != 0;
```

### Branch Instruction Verification

```powershell
# Check BNE execution
Get-Content apu_trace.log | Select-String "bne" -CaseSensitive

# Branch conditions:
# BNE: Branch if Z=0
# BEQ: Branch if Z=1
# BCC: Branch if C=0
# BCS: Branch if C=1
```

---

## 🐛 Common Bug Patterns

### 1. Port 0 Initialized to 0x00

**Symptom**: SPC reads 0x00 from port 0 (expects 0xCC)

**Cause**: APU executes before CPU writes port value

**Solution**:
```cpp
// src/main_complete.cpp
// Initialize ports before starting APU
memory->write(0x2140, 0xCC);
memory->write(0x2141, 0x01);

// Reset APU
apu->reset();
```

### 2. CMP Flag Inversion

**Symptom**: BNE branches in wrong direction

**Cause**: Carry flag calculated backwards

**Correct Implementation**:
```cpp
// ❌ Wrong code
setFlag(FLAG_C, a < operand);

// ✅ Correct code
setFlag(FLAG_C, a >= operand);
```

### 3. Branch Offset Unsigned Handling

**Symptom**: Backward branch jumps forward

**Cause**: Signed offset treated as unsigned

**Correct Implementation**:
```cpp
// ❌ Wrong code
uint8_t offset = readByte(pc++);
pc += offset;

// ✅ Correct code
int8_t offset = (int8_t)readByte(pc++);
pc += offset;
```

---

## 📊 PSW (Processor Status Word) Flags

```
Bit 7: N - Negative  (bit 7 of result)
Bit 6: V - Overflow  (signed overflow)
Bit 5: P - Direct    (Direct page selection)
Bit 4: B - Break     (BRK instruction executed)
Bit 3: H - Half      (BCD half-carry)
Bit 2: I - IRQ       (Interrupt enable)
Bit 1: Z - Zero      (result is 0)
Bit 0: C - Carry     (unsigned carry/borrow)
```

### Flag Calculation Example

```
A = 0x56, operand = 0x01
CMP A, #0x01:
  temp = 0x56 - 0x01 = 0x55
  N = 0 (bit 7 of 0x55 = 0)
  Z = 0 (0x56 != 0x01)
  C = 1 (0x56 >= 0x01)
  Result: N=0, Z=0, C=1 in PSW

BNE fail:
  Branches because Z = 0 (Not Equal)
  -> This is a bug! If A is 0x56 and operand is 0x01, they are different
```

---

## 🔧 Code Modification Verification

### 1. Build and Test
```powershell
# Auto build + test
.\build_complete.bat && .\snes_emu_complete.exe spctest.sfc

# Check results
Get-Content apu_trace.log -Tail 20
```

### 2. Log Comparison
```powershell
# Backup current log
Copy-Item apu_trace.log apu_trace_before.log

# Run again after code modification
.\build_complete.bat && .\snes_emu_complete.exe spctest.sfc

# Compare
.\compare_traces.ps1 -GoldenTrace apu_trace_before.log -CurrentTrace apu_trace.log
```

### 3. Success Determination
```powershell
# Check if port 0 is 0xFF (success)
Get-Content port_comm.log | Select-String "port 0 = 0xff"

# Or check if last PC is 0x0352 (success routine)
Get-Content apu_trace.log -Tail 5 | Select-String "0x0352"
```

---

## 📝 Test ROM Status Codes

### Port 2 Values
```
0x02: Test failed (first marker)
0x03: Entered fail routine (second marker)
0xf3, 0xf4, ...: Test in progress (test number)
```

### Port 0 Values
```
0xCC: CPU initialization value (start signal)
0x00: SPC ready
0xFF: All tests successful!
```

### PC Addresses
```
0x0300: main start
0x0350-0x0354: success routine
0x0355-0x0359: fail routine
0x0357: Infinite loop (BRA -2) - test failure
```

---

## 💡 Agent Development Tips

### 1. Always Set Timeout
```powershell
# To avoid infinite loops
.\monitor_execution.ps1 -TimeoutSeconds 60
```

### 2. Backup Log Files
```powershell
# Keep previous execution logs
$timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
Copy-Item apu_trace.log "apu_trace_$timestamp.log"
```

### 3. Incremental Testing
```powershell
# Modify one thing at a time and test
# 1. Modify only CMP instruction -> test
# 2. Modify only BNE instruction -> test
# 3. Modify only port communication -> test
```

### 4. Adjust Log Level
```cpp
// Too many logs are slow
// Detailed logging only in problem areas

if (pc >= 0x0300 && pc <= 0x0400) {
    // Detailed logging only in main routine area
    fprintf(trace_log, "...");
}
```

---

## 🎯 Current Problem Resolution Order

### Priority 1: Verify Port Initialization
```cpp
// src/main_complete.cpp or snes_core.cpp
// Set port values before starting APU
memory->write(0x2140, 0xCC);
memory->write(0x2141, 0x01);
```

### Priority 2: Verify CMP Instruction
```cpp
// src/apu/apu.cpp
case 0x68:  // CMP A, #imm
    operand = readByte(pc++);
    temp = a - operand;
    setFlag(FLAG_C, a >= operand);  // ← Check this part!
    setFlag(FLAG_Z, temp == 0);
    setFlag(FLAG_N, temp & 0x80);
    break;
```

### Priority 3: Add Port Read/Write Logging
```cpp
// Log all port accesses to port_comm.log
fprintf(port_log, "[Cyc:%lu] SPC read port %d = 0x%02X\n", ...);
fprintf(port_log, "[Cyc:%lu] CPU wrote port %d = 0x%02X\n", ...);
```

---

## 📚 Additional References

### Project Documentation
- `docs/AGENT_DEVELOPMENT_GUIDE.md` - Comprehensive guide
- `docs/hardware/spc700_instructions.md` - Instruction details
- `docs/test_roms/spctest_expected.md` - Test ROM documentation

### Scripts
- `monitor_execution.ps1` - Execution monitoring
- `analyze_loop.ps1` - Infinite loop analysis
- `compare_traces.ps1` - Log comparison

### Log Files
- `apu_trace.log` - SPC700 execution log
- `cpu_trace.log` - 65C816 execution log
- `port_comm.log` - Port communication log

---

**Written**: 2025-12-14  
**Version**: 1.0  
**Purpose**: Quick reference for Agents
