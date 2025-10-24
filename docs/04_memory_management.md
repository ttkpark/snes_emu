# 04. 메모리 매핑 및 관리 시스템

## 목표
- SNES 메모리 맵 완전 구현
- ROM 헤더 파싱 및 로딩
- 메모리 접근 최적화
- 디버그 메모리 덤프 기능

## SNES 메모리 구조

### 1. 메모리 영역
- **Work RAM**: 128KB (0x000000-0x01FFFF)
- **ROM**: 최대 128MB (0x080000-0xFFFFFF)
- **Save RAM**: 32KB (0x700000-0x70FFFF)
- **I/O Registers**: 64KB (0x040000-0x04FFFF)

### 2. 메모리 맵핑
```
0x000000-0x00FFFF: System RAM (128KB)
0x010000-0x01FFFF: Extended RAM
0x020000-0x03FFFF: Reserved
0x040000-0x04FFFF: I/O Registers
0x050000-0x05FFFF: Expansion
0x060000-0x06FFFF: Expansion
0x070000-0x07FFFF: Expansion
0x080000-0x0FFFFF: ROM (512KB)
0x100000-0x1FFFFF: ROM (1MB)
0x200000-0x2FFFFF: ROM (2MB)
0x300000-0x3FFFFF: ROM (4MB)
0x400000-0x4FFFFF: ROM (8MB)
0x500000-0x5FFFFF: ROM (16MB)
0x600000-0x6FFFFF: ROM (32MB)
0x700000-0x7FFFFF: ROM (64MB)
0x800000-0xFFFFFF: ROM (128MB)
```

## 구현 계획

### 1. 메모리 클래스 구조
```cpp
class Memory {
    std::vector<uint8_t> m_workRAM;      // 128KB
    std::vector<uint8_t> m_rom;          // ROM 데이터
    std::vector<uint8_t> m_sram;         // Save RAM
    
    // 메모리 맵핑
    uint32_t translateAddress(uint32_t logicalAddress);
    bool isValidAddress(uint32_t address) const;
};
```

### 2. ROM 헤더 파싱
```cpp
struct ROMHeader {
    std::string title;           // 게임 제목
    uint8_t romType;            // ROM 타입
    uint8_t romSize;            // ROM 크기
    uint8_t sramSize;           // Save RAM 크기
    uint8_t countryCode;        // 국가 코드
    uint8_t licenseCode;        // 라이선스 코드
    uint8_t version;             // 버전
    uint16_t checksum;          // 체크섬
    uint16_t checksumComplement; // 체크섬 보수
};
```

### 3. 메모리 접근 최적화
- **가상 메모리 매핑**: 논리 주소를 물리 주소로 변환
- **캐시 시스템**: 자주 접근하는 메모리 영역 캐싱
- **메모리 풀링**: 동적 할당 최소화

### 4. 디버그 기능
- **메모리 덤프**: 특정 영역의 메모리 내용 출력
- **메모리 맵**: 사용 중인 메모리 영역 표시
- **접근 통계**: 메모리 접근 빈도 분석

## ROM 로딩 시스템

### 1. ROM 파일 형식
- **SMC**: Super Magicom 형식
- **SFC**: Super Famicom 형식
- **SWC**: Super Wild Card 형식

### 2. 헤더 파싱
```cpp
bool parseROMHeader() {
    // ROM 헤더 읽기
    // 체크섬 검증
    // 메모리 맵 설정
    return true;
}
```

### 3. 메모리 맵 설정
- ROM 크기에 따른 메모리 맵 동적 설정
- Save RAM 크기 설정
- I/O 레지스터 매핑

## 성능 최적화

### 1. 메모리 접근 최적화
- **인라인 함수**: 자주 호출되는 함수 인라인화
- **브랜치 예측**: 조건부 분기 최적화
- **캐시 친화적**: 메모리 접근 패턴 최적화

### 2. 메모리 풀링
- **고정 크기 블록**: 자주 할당/해제되는 메모리
- **스택 할당**: 임시 메모리 할당
- **메모리 정렬**: 캐시 라인 정렬

## 테스트 계획

### 1. 메모리 접근 테스트
- 각 메모리 영역별 접근 테스트
- 권한 검증 테스트
- 타이밍 테스트

### 2. ROM 로딩 테스트
- 다양한 ROM 형식 테스트
- 헤더 파싱 정확성 테스트
- 체크섬 검증 테스트

## 예상 소요 시간
1-2일

## 다음 단계
05_ppu_emulation.md - PPU (Picture Processing Unit) 에뮬레이션
