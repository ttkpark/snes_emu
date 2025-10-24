# 08. ROM 로딩 및 파싱 시스템

## 목표
- 다양한 ROM 형식 지원
- ROM 헤더 파싱 및 검증
- 메모리 맵 설정
- 체크섬 검증

## SNES ROM 형식

### 1. 지원 형식
- **SMC**: Super Magicom 형식
- **SFC**: Super Famicom 형식
- **SWC**: Super Wild Card 형식
- **FIG**: FIG 형식
- **BIN**: 바이너리 형식

### 2. ROM 헤더 구조
```
Offset  Size  Description
0x00    21    Game Title
0x15    1     ROM Type
0x16    1     ROM Size
0x17    1     Save RAM Size
0x18    1     Country Code
0x19    1     License Code
0x1A    1     Version
0x1B    2     Checksum
0x1D    2     Checksum Complement
0x1F    1     Reserved
0x20    4     Reserved
0x24    4     Reserved
0x28    4     Reserved
0x2C    4     Reserved
0x30    4     Reserved
0x34    4     Reserved
0x38    4     Reserved
0x3C    4     Reserved
```

## 구현 계획

### 1. ROM 로더 클래스
```cpp
class ROMLoader {
    struct ROMHeader {
        std::string title;
        uint8_t romType;
        uint8_t romSize;
        uint8_t sramSize;
        uint8_t countryCode;
        uint8_t licenseCode;
        uint8_t version;
        uint16_t checksum;
        uint16_t checksumComplement;
    };
    
    ROMHeader m_header;
    std::vector<uint8_t> m_romData;
    bool m_loaded;
};
```

### 2. ROM 로딩
```cpp
bool loadROM(const std::string& filename) {
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }
    
    // 파일 크기 확인
    file.seekg(0, std::ios::end);
    size_t fileSize = file.tellg();
    file.seekg(0, std::ios::beg);
    
    // ROM 데이터 읽기
    m_romData.resize(fileSize);
    file.read(reinterpret_cast<char*>(m_romData.data()), fileSize);
    
    // 헤더 파싱
    if (!parseHeader()) {
        return false;
    }
    
    // 체크섬 검증
    if (!verifyChecksum()) {
        return false;
    }
    
    m_loaded = true;
    return true;
}
```

### 3. 헤더 파싱
```cpp
bool parseHeader() {
    if (m_romData.size() < 0x40) {
        return false;
    }
    
    // 게임 제목
    m_header.title = std::string(
        reinterpret_cast<char*>(m_romData.data() + 0x00), 21
    );
    
    // ROM 타입
    m_header.romType = m_romData[0x15];
    
    // ROM 크기
    m_header.romSize = m_romData[0x16];
    
    // Save RAM 크기
    m_header.sramSize = m_romData[0x17];
    
    // 국가 코드
    m_header.countryCode = m_romData[0x18];
    
    // 라이선스 코드
    m_header.licenseCode = m_romData[0x19];
    
    // 버전
    m_header.version = m_romData[0x1A];
    
    // 체크섬
    m_header.checksum = m_romData[0x1B] | (m_romData[0x1C] << 8);
    m_header.checksumComplement = m_romData[0x1D] | (m_romData[0x1E] << 8);
    
    return true;
}
```

### 4. 체크섬 검증
```cpp
bool verifyChecksum() {
    uint16_t calculatedChecksum = 0;
    uint16_t calculatedComplement = 0;
    
    // ROM 데이터 체크섬 계산
    for (size_t i = 0; i < m_romData.size(); i += 2) {
        uint16_t word = m_romData[i] | (m_romData[i + 1] << 8);
        calculatedChecksum += word;
    }
    
    calculatedComplement = ~calculatedChecksum;
    
    return (calculatedChecksum == m_header.checksum) &&
           (calculatedComplement == m_header.checksumComplement);
}
```

## 메모리 맵 설정

### 1. ROM 크기별 메모리 맵
```cpp
void setupMemoryMap() {
    size_t romSize = getROMSize();
    
    if (romSize <= 0x100000) {        // 1MB
        setup1MBMemoryMap();
    } else if (romSize <= 0x200000) { // 2MB
        setup2MBMemoryMap();
    } else if (romSize <= 0x400000) { // 4MB
        setup4MBMemoryMap();
    } else if (romSize <= 0x800000) { // 8MB
        setup8MBMemoryMap();
    } else {                          // 16MB+
        setup16MBMemoryMap();
    }
}
```

### 2. 1MB 메모리 맵
```cpp
void setup1MBMemoryMap() {
    // ROM: 0x800000-0x8FFFFF (1MB)
    m_memoryMaps.push_back({
        0x800000, 0x8FFFFF, "ROM", true, false
    });
    
    // Save RAM: 0x700000-0x70FFFF (64KB)
    m_memoryMaps.push_back({
        0x700000, 0x70FFFF, "Save RAM", true, true
    });
}
```

## ROM 형식 변환

### 1. SMC 형식 처리
```cpp
bool processSMCFormat() {
    // SMC 헤더 제거 (512 bytes)
    if (m_romData.size() > 512) {
        std::vector<uint8_t> romData(
            m_romData.begin() + 512, m_romData.end()
        );
        m_romData = std::move(romData);
    }
    
    return true;
}
```

### 2. FIG 형식 처리
```cpp
bool processFIGFormat() {
    // FIG 헤더 제거 (512 bytes)
    if (m_romData.size() > 512) {
        std::vector<uint8_t> romData(
            m_romData.begin() + 512, m_romData.end()
        );
        m_romData = std::move(romData);
    }
    
    return true;
}
```

## ROM 정보 표시

### 1. ROM 정보 출력
```cpp
void printROMInfo() {
    std::cout << "ROM 정보:" << std::endl;
    std::cout << "  제목: " << m_header.title << std::endl;
    std::cout << "  ROM 타입: " << std::hex << (int)m_header.romType << std::endl;
    std::cout << "  ROM 크기: " << getROMSizeString() << std::endl;
    std::cout << "  Save RAM 크기: " << getSRAMSizeString() << std::endl;
    std::cout << "  국가: " << getCountryString() << std::endl;
    std::cout << "  라이선스: " << getLicenseString() << std::endl;
    std::cout << "  버전: " << (int)m_header.version << std::endl;
    std::cout << "  체크섬: " << std::hex << m_header.checksum << std::endl;
}
```

### 2. 크기 문자열 변환
```cpp
std::string getROMSizeString() {
    size_t size = getROMSize();
    if (size <= 0x100000) return "1MB";
    if (size <= 0x200000) return "2MB";
    if (size <= 0x400000) return "4MB";
    if (size <= 0x800000) return "8MB";
    if (size <= 0x1000000) return "16MB";
    if (size <= 0x2000000) return "32MB";
    return "64MB+";
}
```

## 오류 처리

### 1. 파일 오류
```cpp
enum class ROMError {
    FILE_NOT_FOUND,
    FILE_READ_ERROR,
    INVALID_HEADER,
    CHECKSUM_MISMATCH,
    UNSUPPORTED_FORMAT
};
```

### 2. 오류 메시지
```cpp
std::string getErrorMessage(ROMError error) {
    switch (error) {
        case ROMError::FILE_NOT_FOUND:
            return "ROM 파일을 찾을 수 없습니다.";
        case ROMError::FILE_READ_ERROR:
            return "ROM 파일 읽기 오류가 발생했습니다.";
        case ROMError::INVALID_HEADER:
            return "유효하지 않은 ROM 헤더입니다.";
        case ROMError::CHECKSUM_MISMATCH:
            return "ROM 체크섬이 일치하지 않습니다.";
        case ROMError::UNSUPPORTED_FORMAT:
            return "지원하지 않는 ROM 형식입니다.";
        default:
            return "알 수 없는 오류가 발생했습니다.";
    }
}
```

## 테스트 계획

### 1. ROM 로딩 테스트
- 다양한 ROM 형식 테스트
- 헤더 파싱 정확성 테스트
- 체크섬 검증 테스트

### 2. 메모리 맵 테스트
- ROM 크기별 메모리 맵 테스트
- 메모리 접근 권한 테스트
- 주소 변환 테스트

## 예상 소요 시간
1일

## 다음 단계
09_debug_system.md - 디버그 화면 및 개발자 도구 구현
