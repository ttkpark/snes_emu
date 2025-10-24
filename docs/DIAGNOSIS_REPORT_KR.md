# SNES 에뮬레이터 문제 진단 보고서

**작성일**: 2025-10-23  
**상태**: 🔴 심각한 문제 발견

---

## 🚨 현재 증상

1. **화면**: 검은색 화면만 표시됨
2. **소리**: 사인파 잡음만 들림
3. **창 응답**: 창을 움직일 수 없음 (응답 없음)
4. **CPU**: 무한 루프에 빠져 있음 (NMI 벡터 계속 읽음)

---

## 🔍 발견된 주요 문제들

### ❌ 문제 1: CPU 리셋 로직 오류

**위치**: `src/cpu/cpu.cpp:30-48`

**현재 코드**:
```cpp
void CPU::reset() {
    uint16_t resetVector = m_memory->read16(0xFFFC);
    m_pc = 0x8000 | resetVector;  // ❌ 잘못된 로직!
    //...
}
```

**문제점**:
- `0x8000 | resetVector`는 잘못된 연산입니다
- 예: resetVector가 0x8000이면 `0x8000 | 0x8000 = 0x8000` (정상)
- 예: resetVector가 0x0000이면 `0x8000 | 0x0000 = 0x8000` (문제!)
- 리셋 벡터 값을 그대로 사용해야 합니다

**해결 방법**:
```cpp
m_pc = resetVector;  // 벡터 값 그대로 사용
```

---

### ❌ 문제 2: BRK 핸들러 오류

**위치**: `src/cpu/cpu.cpp:1787-1798`

**현재 코드**:
```cpp
case 0x00: { // BRK
    m_pc++;
    // Push PC and P
    //...
    m_pc = m_memory->read16(0xFFEE); // BRK 벡터
} break;
```

**문제점**:
- BRK 벡터가 0xFFEE인데, SNES는 0xFFE6을 사용합니다
- Emulation 모드: 0xFFFE-0xFFFF
- Native 모드: 0xFFE6-0xFFE7

**해결 방법**:
```cpp
uint16_t vectorAddr = m_emulationMode ? 0xFFFE : 0xFFE6;
m_pc = m_memory->read16(vectorAddr);
```

---

### ❌ 문제 3: 메인 루프 블로킹

**위치**: `src/main_complete.cpp:96-103`

**현재 코드**:
```cpp
for (int scanline = 0; scanline < PPU::SCREEN_HEIGHT + 30; ++scanline) {
    for (int dot = 0; dot < 341; ++dot) {
        cpu.step();  // CPU가 무한 루프에 빠지면 여기서 멈춤!
        ppu.step();
        apu.step();
    }
}
```

**문제점**:
- CPU가 무한 루프(또는 긴 루프)에 빠지면 SDL 이벤트가 처리되지 않음
- 창이 응답하지 않게 됨
- 사이클 제한이 없음

**해결 방법**:
- 매 프레임당 최대 사이클 수 제한
- 정기적으로 SDL 이벤트 처리
- CPU 실행을 타임아웃으로 제한

---

### ⚠️ 문제 4: PPU 강제 블랭크

**위치**: `src/ppu/ppu.cpp:285-291`

**현재 코드**:
```cpp
void PPU::renderScanline() {
    if (m_forcedBlank) {
        // 검은 화면 렌더링
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            m_framebuffer[m_scanline * SCREEN_WIDTH + x] = 0x000000FF;
        }
        return;
    }
    //...
}
```

**문제점**:
- `m_forcedBlank`가 초기값 `false`이지만, 게임이 설정할 수 있음
- 게임이 시작 시 강제 블랭크를 켤 수 있음
- 그래픽 데이터가 없으면 테스트 패턴을 보여줘야 하는데 안 보임

---

### ⚠️ 문제 5: APU 오디오 노이즈

**위치**: `src/apu/apu.cpp` (전체)

**문제점**:
- SPC700 초기화가 제대로 안 됨
- BRR 디코딩이 빈 데이터를 생성
- 사인파 잡음은 초기화되지 않은 오디오 버퍼

---

### ⚠️ 문제 6: NMI 벡터 무한 읽기

**로그 출력**:
```
Memory::read8(0xffea) -> ROM[0x7fea] = 0x38
Memory::read8(0xffeb) -> ROM[0x7feb] = 0xb1
(계속 반복...)
```

**의미**:
- NMI 벡터가 0xB138을 가리킴
- CPU가 계속 NMI 벡터를 읽고 있음
- 0xB138에 있는 코드가 다시 NMI를 트리거하거나 무한 루프

**원인**:
1. CPU가 0x0000 (BRK)에서 시작
2. BRK가 잘못된 벡터로 점프
3. 해당 위치에서 다시 NMI 트리거
4. 무한 루프

---

## 📋 개발 우선순위

### 🔥 긴급 (즉시 수정 필요)

#### 1. CPU 리셋 수정
```cpp
// src/cpu/cpu.cpp
void CPU::reset() {
    uint16_t resetVector = m_memory->read16(0xFFFC);
    m_pc = resetVector;  // ✅ 올바른 방법
    m_pbr = 0x00;  // Program Bank는 0
    // ... 나머지 코드
}
```

#### 2. BRK/NMI 벡터 수정
```cpp
// BRK 벡터
case 0x00: { // BRK
    m_pc++;
    // Push PC, PBR, P
    m_memory->write8(0x0100 + m_sp--, m_pbr);
    m_memory->write8(0x0100 + m_sp--, (m_pc >> 8) & 0xFF);
    m_memory->write8(0x0100 + m_sp--, m_pc & 0xFF);
    m_memory->write8(0x0100 + m_sp--, m_p | 0x10); // B flag
    m_p |= 0x04; // Set I flag
    m_pbr = 0x00;
    uint16_t vectorAddr = m_emulationMode ? 0xFFFE : 0xFFE6;
    m_pc = m_memory->read16(vectorAddr);
} break;

// NMI 벡터
void CPU::handleNMI() {
    // Push PBR, PC, P
    m_memory->write8(0x0100 + m_sp--, m_pbr);
    m_memory->write8(0x0100 + m_sp--, (m_pc >> 8) & 0xFF);
    m_memory->write8(0x0100 + m_sp--, m_pc & 0xFF);
    m_memory->write8(0x0100 + m_sp--, m_p);
    m_p |= 0x04; // Set I flag
    m_pbr = 0x00;
    uint16_t vectorAddr = m_emulationMode ? 0xFFFA : 0xFFEA;
    m_pc = m_memory->read16(vectorAddr);
}
```

#### 3. 메인 루프 사이클 제한
```cpp
// src/main_debug.cpp (이미 생성됨)
const int MAX_CYCLES_PER_FRAME = 89342;
int cyclesThisFrame = 0;

for (int scanline = 0; scanline < 262; ++scanline) {
    for (int dot = 0; dot < 341; ++dot) {
        if (cyclesThisFrame >= MAX_CYCLES_PER_FRAME) {
            break;  // 사이클 제한 도달
        }
        cpu.step();
        ppu.step();
        apu.step();
        cyclesThisFrame++;
    }
}
```

---

### 🟡 중요 (다음 단계)

#### 4. PPU 테스트 패턴 강제 표시
```cpp
// src/ppu/ppu.cpp
void PPU::renderScanline() {
    // 항상 테스트 패턴 표시 (디버깅용)
    for (int x = 0; x < SCREEN_WIDTH; x++) {
        uint32_t pixelColor;
        
        // 체크보드 패턴
        if ((x / 32 + m_scanline / 32) % 2 == 0) {
            pixelColor = 0xFF0000FF; // 빨강
        } else {
            pixelColor = 0x00FF00FF; // 초록
        }
        
        m_framebuffer[m_scanline * SCREEN_WIDTH + x] = pixelColor;
    }
}
```

#### 5. APU 무음 출력
```cpp
// src/apu/apu.cpp
// 일시적으로 APU를 비활성화하고 무음 출력
void APU::audioCallback(void* userdata, uint8_t* stream, int len) {
    memset(stream, 0, len);  // 무음
}
```

---

### 🟢 개선 (나중에)

6. ROM 헤더 파싱 (LoROM/HiROM 자동 감지)
7. DMA 완전 구현
8. PPU 모든 배경 모드 구현
9. 스프라이트 우선순위 및 투명도
10. SPC700 완전 구현

---

## 🛠️ 즉시 적용 가능한 임시 해결책

### 빠른 수정 1: Debug 빌드 사용

```bash
.\build_debug.bat
.\snes_emu_debug.exe
```

이 버전은:
- 사이클 제한 있음 (무한 루프 방지)
- 정기적인 SDL 이벤트 처리
- FPS 표시 및 PC 주소 출력

### 빠른 수정 2: 테스트 패턴 활성화

PPU를 수정하여 항상 테스트 패턴을 표시하도록 합니다.

### 빠른 수정 3: APU 비활성화

임시로 APU 초기화를 건너뛰어 잡음을 제거합니다.

---

## 📊 예상 결과

### 수정 전
- ❌ 검은 화면
- ❌ 사인파 잡음
- ❌ 창 응답 없음
- ❌ CPU 무한 루프

### 수정 후
- ✅ 체크보드 테스트 패턴 표시
- ✅ 무음 (또는 정상 오디오)
- ✅ 창 정상 응답
- ✅ CPU 정상 실행

---

## 🎯 단계별 수정 계획

### 1단계: CPU 리셋 수정 (10분)
- `cpu.cpp`의 `reset()` 함수 수정
- BRK/NMI 벡터 주소 수정
- 테스트

### 2단계: 메인 루프 수정 (10분)
- 사이클 제한 추가
- SDL 이벤트 정기 처리
- 테스트

### 3단계: PPU 테스트 패턴 (5분)
- 강제로 체크보드 패턴 표시
- 프레임버퍼 확인
- 테스트

### 4단계: APU 임시 비활성화 (5분)
- 무음 출력으로 변경
- 잡음 제거 확인
- 테스트

### 5단계: 통합 테스트 (10분)
- 모든 수정사항 적용
- 전체 테스트
- 문서화

**예상 총 시간**: 40분

---

## 💡 결론

현재 에뮬레이터의 주요 문제는 **CPU 초기화 및 벡터 처리 오류**입니다. 이로 인해:

1. CPU가 올바른 코드를 실행하지 못함
2. 무한 루프에 빠짐
3. SDL 이벤트 루프가 블로킹됨
4. 화면이 업데이트되지 않음

**해결책**: CPU 리셋 로직과 인터럽트 벡터를 수정하면 대부분의 문제가 해결될 것입니다.

---

**작성자**: AI 개발 에이전트  
**우선순위**: 🔥 긴급  
**다음 작업**: CPU 리셋 및 벡터 수정



