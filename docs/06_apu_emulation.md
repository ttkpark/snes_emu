# 06. APU (Audio Processing Unit) 에뮬레이션

## 목표
- SNES APU 완전 구현
- 8개 DSP 채널 지원
- 실시간 오디오 출력
- 에코 및 필터 효과

## SNES APU 특징

### 1. DSP 채널
- **채널 0-7**: 8개 독립적인 오디오 채널
- **샘플링**: 32kHz
- **비트 깊이**: 16비트
- **효과**: 에코, 피치 모듈레이션, ADSR 엔벨로프

### 2. 오디오 효과
- **ADSR**: Attack, Decay, Sustain, Release
- **에코**: 지연 및 피드백
- **필터**: 저역 통과 필터
- **피치 모듈레이션**: 채널 간 피치 조절

### 3. 메모리 구조
- **DSP 레지스터**: 128개 레지스터
- **샘플 데이터**: 64KB
- **에코 버퍼**: 64KB

## 구현 계획

### 1. APU 클래스 구조
```cpp
class APU {
    // DSP 레지스터들
    APURegisters m_registers;
    
    // 채널 정보
    Channel m_channels[8];
    
    // 오디오 설정
    float m_volume;
    int m_sampleRate;
    int m_channels;
    
    // 오디오 버퍼
    std::vector<int16_t> m_audioBuffer;
};
```

### 2. DSP 처리
```cpp
void processDSP() {
    for (int channel = 0; channel < 8; channel++) {
        if (m_channels[channel].KON) {
            // 키 온 처리
            startNote(channel);
        }
        
        if (m_channels[channel].KOF) {
            // 키 오프 처리
            stopNote(channel);
        }
        
        // 채널 처리
        processChannel(channel);
    }
    
    // 에코 처리
    processEcho();
}
```

### 3. 채널 처리
```cpp
void processChannel(int channel) {
    // ADSR 엔벨로프 업데이트
    updateEnvelope(channel);
    
    // 피치 업데이트
    updatePitch(channel);
    
    // 샘플 생성
    int16_t sample = getSample(channel);
    
    // 필터 적용
    applyFilter(sample);
    
    // 볼륨 적용
    sample = (int16_t)(sample * m_channels[channel].VOL / 127.0f);
    
    // 오디오 버퍼에 추가
    m_audioBuffer.push_back(sample);
}
```

### 4. ADSR 엔벨로프
```cpp
void updateEnvelope(int channel) {
    Channel& ch = m_channels[channel];
    
    switch (ch.envelopeState) {
        case ATTACK:
            ch.ENVX += ch.ADSR1.attackRate;
            if (ch.ENVX >= 127) {
                ch.ENVX = 127;
                ch.envelopeState = DECAY;
            }
            break;
            
        case DECAY:
            ch.ENVX -= ch.ADSR1.decayRate;
            if (ch.ENVX <= ch.ADSR1.sustainLevel) {
                ch.ENVX = ch.ADSR1.sustainLevel;
                ch.envelopeState = SUSTAIN;
            }
            break;
            
        case SUSTAIN:
            // 서스테인 레벨 유지
            break;
            
        case RELEASE:
            ch.ENVX -= ch.ADSR2.releaseRate;
            if (ch.ENVX <= 0) {
                ch.ENVX = 0;
                ch.envelopeState = IDLE;
            }
            break;
    }
}
```

## 오디오 출력

### 1. SDL 오디오 설정
```cpp
bool initializeAudio() {
    SDL_AudioSpec desired, obtained;
    
    desired.freq = 32000;
    desired.format = AUDIO_S16SYS;
    desired.channels = 2;
    desired.samples = 1024;
    desired.callback = audioCallback;
    desired.userdata = this;
    
    if (SDL_OpenAudio(&desired, &obtained) < 0) {
        return false;
    }
    
    SDL_PauseAudio(0);
    return true;
}
```

### 2. 오디오 콜백
```cpp
static void audioCallback(void* userdata, Uint8* stream, int len) {
    APU* apu = static_cast<APU*>(userdata);
    int16_t* samples = reinterpret_cast<int16_t*>(stream);
    int sampleCount = len / sizeof(int16_t);
    
    for (int i = 0; i < sampleCount; i++) {
        samples[i] = apu->generateSample();
    }
}
```

### 3. 샘플 생성
```cpp
int16_t generateSample() {
    int16_t sample = 0;
    
    for (int channel = 0; channel < 8; channel++) {
        if (m_channels[channel].envelopeState != IDLE) {
            sample += getChannelSample(channel);
        }
    }
    
    // 마스터 볼륨 적용
    sample = (int16_t)(sample * m_volume);
    
    // 클리핑 방지
    if (sample > 32767) sample = 32767;
    if (sample < -32768) sample = -32768;
    
    return sample;
}
```

## 에코 시스템

### 1. 에코 버퍼
```cpp
class EchoBuffer {
    std::vector<int16_t> m_buffer;
    int m_writePosition;
    int m_readPosition;
    int m_delay;
    float m_feedback;
    
public:
    void setDelay(int delay) { m_delay = delay; }
    void setFeedback(float feedback) { m_feedback = feedback; }
    
    int16_t process(int16_t input) {
        int16_t delayed = m_buffer[m_readPosition];
        int16_t output = input + (int16_t)(delayed * m_feedback);
        
        m_buffer[m_writePosition] = output;
        
        m_writePosition = (m_writePosition + 1) % m_buffer.size();
        m_readPosition = (m_readPosition + 1) % m_buffer.size();
        
        return output;
    }
};
```

### 2. FIR 필터
```cpp
void applyFIRFilter(int16_t& sample) {
    int32_t filtered = 0;
    
    for (int i = 0; i < 8; i++) {
        filtered += m_firBuffer[i] * m_registers.FIR[i];
    }
    
    sample = (int16_t)(filtered >> 15);
}
```

## 최적화 전략

### 1. 오디오 최적화
- **샘플 캐싱**: 자주 사용되는 샘플 캐싱
- **인터폴레이션**: 선형 보간으로 품질 향상
- **SIMD**: 벡터 연산 활용

### 2. 메모리 최적화
- **순환 버퍼**: 에코 버퍼 최적화
- **메모리 풀링**: 오디오 버퍼 풀링
- **캐시 친화적**: 메모리 접근 패턴 최적화

## 테스트 계획

### 1. 오디오 품질 테스트
- 각 채널별 오디오 출력 테스트
- ADSR 엔벨로프 테스트
- 에코 효과 테스트

### 2. 성능 테스트
- CPU 사용률 테스트
- 메모리 사용량 테스트
- 지연 시간 테스트

## 예상 소요 시간
2-3일

## 다음 단계
07_input_handling.md - 입력 처리 시스템 구현
