# ROM 헤더 구조 및 스펙

## 개요

SNES ROM 헤더는 게임 카트리지의 하드웨어 구성을 설명하는 데이터 구조입니다. SNES 하드웨어 자체는 필요하지 않지만, 에뮬레이터와 플래시카트가 게임을 올바르게 실행하기 위해 이 정보를 사용합니다.

## 헤더 위치

### CPU 주소 공간
- **메인 헤더**: $00FFC0-$00FFDF (인터럽트 벡터 직전)
- **확장 헤더**: $00FFB0-$00FFBF (선택적)

### ROM 파일 오프셋
메모리 맵 모드에 따라 ROM 파일 내 위치가 다릅니다:
- **LoROM**: $007FC0
- **HiROM**: $00FFC0
- **ExHiROM**: $40FFC0

> **주의**: 일부 ROM 파일은 512바이트 copier 헤더를 포함할 수 있습니다. 이는 내부 헤더와는 별개입니다.

---

## 메인 헤더 구조 ($FFC0-$FFDF)

| 주소   | 길이 | 내용                                    | 설명                              |
|--------|------|----------------------------------------|-----------------------------------|
| $FFC0  | 21   | 카트리지 제목                          | 대문자 ASCII, 나머지는 공백       |
| $FFD5  | 1    | ROM 속도 및 메모리 맵 모드             | LoROM/HiROM/ExHiROM               |
| $FFD6  | 1    | 칩셋                                   | RAM, 배터리, 코프로세서           |
| $FFD7  | 1    | ROM 크기                               | 1<<N 킬로바이트                   |
| $FFD8  | 1    | RAM 크기                               | 1<<N 킬로바이트                   |
| $FFD9  | 1    | 국가 코드                              | NTSC/PAL 결정                     |
| $FFDA  | 1    | 개발자 ID                              | $33 = 확장 헤더 사용              |
| $FFDB  | 1    | ROM 버전                               | 0 = 첫 버전                       |
| $FFDC  | 2    | 체크섬 보수                            | Checksum XOR $FFFF                |
| $FFDE  | 2    | 체크섬                                 | ROM 전체의 16비트 합              |
| $FFE0  | 32   | 인터럽트 벡터                          | Native/Emulation 모드 벡터        |

---

## $FFD5: ROM 속도 및 맵 모드

```
비트 형식: 001smmmm
           ||++++- 맵 모드
           |+----- 속도: 0=Slow (2.68MHz), 1=Fast (3.58MHz)
           +------ 고정값 (001)
```

### 맵 모드 값
| 값 | 모드      | 설명                              |
|----|-----------|-----------------------------------|
| 0  | LoROM     | 32KB 뱅크, $8000-$FFFF 매핑       |
| 1  | HiROM     | 64KB 뱅크, $0000-$FFFF 매핑       |
| 5  | ExHiROM   | 확장 HiROM (48MB+ 지원)           |

### 예제
```
$20 = 0010 0000 = LoROM, Slow
$21 = 0010 0001 = HiROM, Slow  
$25 = 0010 0101 = ExHiROM, Slow
$30 = 0011 0000 = LoROM, Fast
$31 = 0011 0001 = HiROM, Fast
```

---

## $FFD6: 칩셋 (하드웨어 구성)

```
비트 형식: CCCCSSSS
           |||++++- 서브타입
           ++++----- 코프로세서 타입
```

### 기본 값
| 값  | 의미                              |
|-----|-----------------------------------|
| $00 | ROM only                          |
| $01 | ROM + RAM                         |
| $02 | ROM + RAM + Battery               |
| $x3 | ROM + 코프로세서                  |
| $x4 | ROM + 코프로세서 + RAM            |
| $x5 | ROM + 코프로세서 + RAM + Battery  |
| $x6 | ROM + 코프로세서 + Battery        |

### 코프로세서 타입
| 상위 니블 | 코프로세서                      |
|-----------|---------------------------------|
| $0x       | DSP (DSP-1, DSP-2, DSP-3, DSP-4)|
| $1x       | GSU (SuperFX)                   |
| $2x       | OBC1                            |
| $3x       | SA-1                            |
| $4x       | S-DD1                           |
| $5x       | S-RTC                           |
| $Ex       | Other (Super Game Boy, etc.)    |
| $Fx       | Custom (see $FFBF)              |

### Custom 코프로세서 ($FFBF 값)
| 값  | 코프로세서 |
|-----|-----------|
| $00 | SPC7110   |
| $01 | ST010/011 |
| $02 | ST018     |
| $03 | CX4       |

---

## 확장 헤더 ($FFB0-$FFBF)

확장 헤더는 $FFDA에 $33을 넣어 활성화합니다.

| 주소  | 길이 | 내용                   | 설명                        |
|-------|------|------------------------|-----------------------------|
| $FFB0 | 2    | ASCII 제조사 코드      | 2글자 코드                  |
| $FFB2 | 4    | ASCII 게임 코드        | 4글자 코드                  |
| $FFB6 | 6    | 예약                   | 0으로 채움                  |
| $FFBC | 1    | 확장 플래시 크기       | 1<<N 킬로바이트             |
| $FFBD | 1    | 확장 RAM 크기          | 1<<N 킬로바이트 (GSU용?)    |
| $FFBE | 1    | 특수 버전              | 보통 0                      |
| $FFBF | 1    | 칩셋 서브타입          | $FFD6이 $F0-$FF일 때 사용   |

---

## ROM/RAM 크기 계산

### $FFD7: ROM 크기
```
실제 크기 = 1 << N 킬로바이트

예제:
$08 = 1 << 8 = 256 KB
$09 = 1 << 9 = 512 KB
$0A = 1 << 10 = 1024 KB (1 MB)
$0B = 1 << 11 = 2048 KB (2 MB)
$0C = 1 << 12 = 4096 KB (4 MB)
```

### $FFD8: RAM 크기
```
실제 크기 = 1 << N 킬로바이트

예제:
$00 = No RAM
$01 = 1 << 1 = 2 KB
$03 = 1 << 3 = 8 KB
$05 = 1 << 5 = 32 KB
```

---

## 체크섬 계산

### 기본 원리

체크섬은 ROM의 모든 바이트를 16비트로 합한 값입니다. ROM 크기가 2의 거듭제곱이 아닌 경우, 미러링을 사용하여 다음 2의 거듭제곱까지 채웁니다.

### 비-2의거듭제곱 ROM 처리

많은 SNES 게임은 2의 거듭제곱이 아닌 ROM 크기를 사용합니다 (예: 3MB = 2MB + 1MB).

#### 처리 과정
```
1. ROM 크기보다 작거나 같은 가장 큰 2의 거듭제곱 찾기
2. 나머지가 있으면:
   a. 나머지보다 크거나 같은 가장 작은 2의 거듭제곱 찾기
   b. 0으로 패딩 (필요시)
   c. 첫 번째 부분 크기에 도달할 때까지 나머지 반복
3. 최종 크기는 헤더에 지정된 2의 거듭제곱과 일치

예제: 3MB ROM (2MB + 1MB)
- 첫 번째 부분: 2MB (2^21)
- 두 번째 부분: 1MB (2^20)
- 1MB를 두 번 반복하여 총 4MB (2^22) 생성
```

### 체크섬 계산 코드

```cpp
uint16_t calculate_checksum(uint8_t* rom, size_t size) {
    // 1. 헤더의 체크섬 필드를 임시로 클리어
    rom[0xFFDC] = 0x00;  // Complement low
    rom[0xFFDD] = 0x00;  // Complement high
    rom[0xFFDE] = 0x00;  // Checksum low
    rom[0xFFDF] = 0x00;  // Checksum high
    
    // 2. 2의 거듭제곱 크기 준비 (미러링)
    size_t target_size = 1 << rom[0xFFD7];  // ROM size from header
    uint8_t* prepared_rom = prepare_rom_for_checksum(rom, size, target_size);
    
    // 3. 체크섬 계산
    uint16_t checksum = 0;
    for (size_t i = 0; i < target_size; i++) {
        checksum += prepared_rom[i];
    }
    
    return checksum;
}

// 체크섬 저장
void write_checksum(uint8_t* rom, uint16_t checksum) {
    uint16_t complement = checksum ^ 0xFFFF;
    
    rom[0xFFDC] = complement & 0xFF;
    rom[0xFFDD] = (complement >> 8) & 0xFF;
    rom[0xFFDE] = checksum & 0xFF;
    rom[0xFFDF] = (checksum >> 8) & 0xFF;
}
```

### Python 예제

```python
def calculate_checksum(rom_data):
    # 체크섬 필드 클리어
    rom_data[0xFFDC:0xFFE0] = b'\x00\x00\xFF\xFF'
    
    # ROM 크기 확인
    rom_size = 1 << rom_data[0xFFD7]
    
    # 필요시 미러링
    if len(rom_data) < rom_size:
        rom_data = prepare_mirroring(rom_data, rom_size)
    
    # 체크섬 계산
    checksum = sum(rom_data[:rom_size]) & 0xFFFF
    complement = checksum ^ 0xFFFF
    
    return checksum, complement
```

---

## 헤더 검증

에뮬레이터는 다음 방법으로 헤더의 유효성을 검증합니다:

### 1차 검증: 체크섬
```cpp
bool verify_checksum(uint8_t* rom) {
    uint16_t checksum = read_word(rom, 0xFFDE);
    uint16_t complement = read_word(rom, 0xFFDC);
    
    // 체크섬과 보수의 합은 항상 0xFFFF
    if ((checksum + complement) != 0xFFFF) {
        return false;
    }
    
    // 실제 계산 값과 비교
    uint16_t calculated = calculate_checksum(rom);
    return (calculated == checksum);
}
```

### 2차 검증: 휴리스틱
```cpp
int score_header(uint8_t* rom, uint32_t offset) {
    int score = 0;
    
    // 1. 체크섬 일치 (+8점)
    if (verify_checksum(rom + offset)) score += 8;
    
    // 2. 맵 모드와 헤더 위치 일치 (+2점)
    uint8_t map_mode = rom[offset + 0xD5] & 0x0F;
    bool location_matches = false;
    if (map_mode == 0 && offset == 0x7FC0) location_matches = true;  // LoROM
    if (map_mode == 1 && offset == 0xFFC0) location_matches = true;  // HiROM
    if (location_matches) score += 2;
    
    // 3. ROM 크기가 파일 크기보다 작지 않음 (+1점)
    uint32_t rom_size = 1 << rom[offset + 0xD7];
    if (rom_size >= file_size) score += 1;
    
    // 4. 리셋 벡터가 유효한 범위 (+1점)
    uint16_t reset_vector = read_word(rom, offset + 0x3C);
    if (reset_vector >= 0x8000) score += 1;
    
    // 5. 게임 타이틀이 ASCII 문자만 포함 (+1점)
    bool title_valid = true;
    for (int i = 0; i < 21; i++) {
        uint8_t c = rom[offset + i];
        if (c < 0x20 || c > 0x7E) {
            title_valid = false;
            break;
        }
    }
    if (title_valid) score += 1;
    
    return score;
}
```

---

## 국가 코드 ($FFD9)

| 값    | 지역          | 비디오 |
|-------|---------------|--------|
| $00   | Japan         | NTSC   |
| $01   | USA           | NTSC   |
| $02   | Europe        | PAL    |
| $03   | Sweden        | PAL    |
| $04   | Finland       | PAL    |
| $05   | Denmark       | PAL    |
| $06   | France        | PAL    |
| $07   | Netherlands   | PAL    |
| $08   | Spain         | PAL    |
| $09   | Germany       | PAL    |
| $0A   | Italy         | PAL    |
| $0B   | China         | PAL    |
| $0C   | Korea         | NTSC   |
| $0D   | Common        | ALL    |
| $0E   | Canada        | NTSC   |
| $0F   | Brazil        | PAL-M  |
| $10   | Australia     | PAL    |

---

## 실용 예제

### 홈브류 ROM 헤더 생성

```asm
; lorom-template 스타일 헤더
.segment "HEADER"
    .byte "MY HOMEBREW GAME    "  ; 21 bytes title
    .byte $20                      ; LoROM, Slow
    .byte $00                      ; ROM only
    .byte $09                      ; 512KB
    .byte $00                      ; No RAM
    .byte $01                      ; USA
    .byte $00                      ; Developer ID
    .byte $00                      ; Version 0
    .word $0000                    ; Checksum complement (filled by tool)
    .word $0000                    ; Checksum (filled by tool)

.segment "VECTORS"
    ; Native mode vectors
    .word 0, 0                     ; Reserved
    .word cop_handler              ; COP
    .word brk_handler              ; BRK
    .word abort_handler            ; ABORT
    .word nmi_handler              ; NMI
    .word reset_handler            ; RESET
    .word irq_handler              ; IRQ
    
    ; Emulation mode vectors
    .word 0, 0                     ; Reserved
    .word cop_handler              ; COP
    .word 0                        ; Reserved
    .word abort_handler            ; ABORT
    .word nmi_handler              ; NMI
    .word reset_handler            ; RESET
    .word irq_handler              ; IRQ/BRK
```

### 에뮬레이터 ROM 로딩

```cpp
bool load_rom(const char* filename) {
    // 1. 파일 읽기
    FILE* f = fopen(filename, "rb");
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    uint8_t* data = malloc(file_size);
    fread(data, 1, file_size, f);
    fclose(f);
    
    // 2. 512바이트 copier 헤더 감지
    bool has_header = ((file_size % 1024) == 512);
    uint8_t* rom = has_header ? (data + 512) : data;
    size_t rom_size = has_header ? (file_size - 512) : file_size;
    
    // 3. 헤더 위치 추정
    uint32_t header_offsets[] = { 0x7FC0, 0xFFC0, 0x40FFC0 };
    int best_score = 0;
    uint32_t best_offset = 0;
    
    for (int i = 0; i < 3; i++) {
        if (header_offsets[i] < rom_size) {
            int score = score_header(rom, header_offsets[i]);
            if (score > best_score) {
                best_score = score;
                best_offset = header_offsets[i];
            }
        }
    }
    
    // 4. 헤더 파싱
    if (best_score > 0) {
        parse_header(rom + best_offset);
        return true;
    }
    
    return false;
}
```

---

## 디버깅 팁

### 일반적인 문제

1. **체크섬 불일치**
   - 원인: ROM 수정 후 체크섬 미갱신
   - 해결: `checksum.py` 등의 도구로 재계산

2. **잘못된 맵 모드**
   - 원인: $FFD5 값 오류
   - 증상: 에뮬레이터가 잘못된 주소에서 코드 실행
   - 해결: 맵 모드 값 검증

3. **Missing Copier Header**
   - 원인: 512바이트 헤더가 있거나 없음
   - 증상: 헤더를 찾을 수 없음
   - 해결: 파일 크기 % 1024 확인

### 헤더 덤프 도구

```python
def dump_header(rom_file):
    with open(rom_file, 'rb') as f:
        data = f.read()
    
    # Copier header 감지
    offset = 512 if (len(data) % 1024 == 512) else 0
    
    # LoROM 시도
    header_offset = 0x7FC0 + offset
    
    print(f"Title: {data[header_offset:header_offset+21].decode('ascii')}")
    print(f"Map Mode: ${data[header_offset+0x15]:02X}")
    print(f"Chipset: ${data[header_offset+0x16]:02X}")
    print(f"ROM Size: {1 << data[header_offset+0x17]} KB")
    print(f"RAM Size: {1 << data[header_offset+0x18]} KB")
    print(f"Country: ${data[header_offset+0x19]:02X}")
    
    checksum = int.from_bytes(data[header_offset+0x1E:header_offset+0x20], 'little')
    complement = int.from_bytes(data[header_offset+0x1C:header_offset+0x1E], 'little')
    print(f"Checksum: ${checksum:04X}")
    print(f"Complement: ${complement:04X}")
    print(f"Sum: ${(checksum + complement):04X} (should be $FFFF)")
```

---

## 참고 자료

- **SNES Development Manual Book 1**, pages 1-2-10 to 1-2-21
- **SNESdev Wiki**: https://snes.nesdev.org/wiki/ROM_header
- **lorom-template**: https://github.com/pinobatch/lorom-template
- **checksum.py**: ROM 체크섬 계산 도구

---

**최종 업데이트**: 2025-12-14  
**출처**: SNESdev Wiki, SNES Development Manual  
**상태**: 완성
