# S-PPU1 하드웨어 정보

## 개요

S-PPU1은 2-chip SNES 콘솔의 두 PPU 중 첫 번째입니다. 파트 번호는 21322입니다. 배경을 생성하고 회전/스케일링을 적용하는 역할을 담당합니다.

## 다이 구조

S-PPU1 다이는 여러 기능 영역으로 나뉘어 있습니다:

### 확인된 영역

#### 영역 1 & 2: OAM (Object Attribute Memory)
- 스프라이트 속성 저장
- 위치, 크기, 우선순위, 팔레트 정보 관리

#### 영역 5 & 6: Sprite Line Buffer
- 스프라이트 렌더링용 라인 버퍼
- 현재 스캔라인의 스프라이트 데이터 임시 저장

#### 영역 10: Sprite + Layer 결합
- 스프라이트와 배경 레이어를 합성
- 우선순위 처리
- 최종 픽셀 출력 생성

#### 영역 12: Registers (추정)
- PPU 제어 레지스터
- 상태 레지스터
- I/O 인터페이스

### 미확인 영역

다이의 나머지 영역들은 정확한 기능이 추측 단계입니다. 추가 분석이 필요합니다.

## 주요 기능

### 1. 배경 생성
- 최대 4개의 배경 레이어 (모드에 따라 다름)
- 타일맵 기반 렌더링
- 스크롤 지원

### 2. Mode 7 지원
- 회전/스케일링 배경
- 아핀 변환 매트릭스
- 원근감 효과

### 3. 스프라이트 처리
- 최대 128개의 스프라이트
- 스캔라인당 최대 32개
- 다양한 크기 지원 (8x8 ~ 64x64)

### 4. 레이어 합성
- 스프라이트와 배경 우선순위
- 투명도 처리
- 컬러 연산 (S-PPU2와 협력)

## S-PPU2와의 관계

S-PPU1은 S-PPU2와 함께 작동합니다:
- **S-PPU1**: 배경 생성, Mode 7, 기본 스프라이트 처리
- **S-PPU2**: 컬러 처리, 윈도우, 특수 효과, 최종 출력

## 프로그래밍 노트

### OAM 액세스
```
주소 범위: $2102-$2103 (주소 설정)
데이터: $2104 (읽기/쓰기)

OAM 구조:
- Low table: 스프라이트 0-127 (각 4바이트)
  Byte 0: X position (low 8 bits)
  Byte 1: Y position
  Byte 2: Tile number
  Byte 3: Attributes (priority, palette, flip)
  
- High table: 스프라이트 0-127 (각 2비트)
  Bit 0: X position (high bit)
  Bit 1: Size toggle
```

### Mode 7 레지스터
```
$211A: Mode 7 Matrix A
$211B: Mode 7 Matrix B
$211C: Mode 7 Matrix C
$211D: Mode 7 Matrix D
$211E: Mode 7 Center X
$211F: Mode 7 Center Y
```

### 배경 레지스터
```
$2107-$210A: BG Tilemap Address
$210B-$210C: BG Character Address
$210D-$2114: BG Scroll Positions
```

## 타이밍 고려사항

### VBlank 기간
- S-PPU1 작업은 VBlank 동안 수행되어야 합니다
- OAM, VRAM 전송은 VBlank에만 안전

### HDMA와의 상호작용
- HDMA로 레지스터 변경 가능
- 스캔라인별 효과 구현 가능

## 디버깅

### 일반적인 문제

1. **스프라이트 깜빡임**
   - 원인: 스캔라인당 32개 제한 초과
   - 해결: 스프라이트 배치 최적화

2. **배경 스크롤 문제**
   - 원인: 스크롤 레지스터 업데이트 타이밍
   - 해결: VBlank에서 업데이트

3. **Mode 7 왜곡**
   - 원인: 잘못된 매트릭스 값
   - 해결: 매트릭스 계산 검증

### 테스트 방법

```cpp
// OAM 테스트
void test_oam() {
    // OAM 주소 설정
    write_register(0x2102, 0x00);
    write_register(0x2103, 0x00);
    
    // 스프라이트 0 데이터 쓰기
    write_register(0x2104, 128);  // X position
    write_register(0x2104, 120);  // Y position
    write_register(0x2104, 0x00); // Tile number
    write_register(0x2104, 0x00); // Attributes
}

// Mode 7 테스트
void test_mode7() {
    // Identity matrix (1:1 스케일)
    write_register(0x211A, 0x00); write_register(0x211A, 0x01); // A = 1.0
    write_register(0x211B, 0x00); write_register(0x211B, 0x00); // B = 0.0
    write_register(0x211C, 0x00); write_register(0x211C, 0x00); // C = 0.0
    write_register(0x211D, 0x00); write_register(0x211D, 0x01); // D = 1.0
}
```

## 에뮬레이션 고려사항

### 정확도 요구사항

1. **OAM 액세스**
   - 타이밍 정확도 중요
   - 스프라이트 우선순위 올바르게 처리

2. **Mode 7**
   - 매트릭스 연산 정확도
   - 정수 오버플로우 처리

3. **레이어 우선순위**
   - 복잡한 우선순위 규칙 구현
   - 스프라이트-배경 상호작용

### 최적화 팁

1. **타일 캐싱**
   - 자주 사용되는 타일 캐시
   - VRAM 변경 감지

2. **라인 기반 렌더링**
   - 스캔라인별 렌더링
   - HDMA 효과 지원

3. **스프라이트 평가**
   - 조기 컬링
   - 스캔라인당 32개 제한 효율적 처리

## 참고 자료

### 공식 문서
- SNES Development Manual Book I, Chapter 22: PPU
- Part Number: 21322 Datasheet

### 커뮤니티 리소스
- SnesLab Wiki: https://sneslab.net/wiki/S-PPU1
- NESdev Forums: PPU 토론
- fullsnes by Martin Korth: PPU 섹션

### 관련 문서
- S-PPU2 (companion chip)
- Mode 7 상세 가이드
- VRAM 구조
- OAM 포맷

---

**출처**: SnesLab Wiki, SNES Development Manual  
**최종 업데이트**: 2025-12-14  
**상태**: 기본 문서 - 추가 분석 필요










