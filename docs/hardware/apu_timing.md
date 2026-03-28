# SNES APU Timing - SPC700 및 DSP 타이밍

## 📋 목차
1. [개요](#개요)
2. [SPC700 클럭](#spc700-클럭)
3. [DSP 샘플링](#dsp-샘플링)
4. [CPU-APU 동기화](#cpu-apu-동기화)
5. [구현 가이드](#구현-가이드)

---

## 개요

APU는 SPC700 CPU와 S-DSP로 구성되며, 각각 독립적인 클럭을 가집니다.

---

## SPC700 클럭

**주파수**: Master Clock / 24 = **1.024 MHz**

**사이클 시간**: ~977 ns/사이클

---

## DSP 샘플링

**샘플레이트**: **32,000 Hz**

**Master Clock 분주**: 21.477 MHz / 32000 Hz = **671.15 clocks/sample**

---

## CPU-APU 동기화

CPU와 APU는 **비동기**로 동작합니다.

**포트 통신** ($2140-$2143, $F4-$F7)을 통해 데이터 교환

---

## 구현 가이드

```cpp
class APU {
private:
    int cycles = 0;
    
public:
    void run(int masterCycles) {
        cycles += masterCycles;
        
        while (cycles >= 24) {
            spc700.step();
            cycles -= 24;
        }
        
        // DSP 샘플링
        dspCycles += masterCycles;
        if (dspCycles >= 671) {
            dsp.generateSample();
            dspCycles -= 671;
        }
    }
};
```

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete










