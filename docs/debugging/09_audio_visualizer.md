# Audio Visualizer - 오디오 파형 분석

## 📋 목차
1. [개요](#개요)
2. [오실로스코프](#오실로스코프)
3. [Voice 분석](#voice-분석)
4. [구현 예제](#구현-예제)

---

## 개요

오디오 비주얼라이저는 DSP의 8개 Voice와 최종 출력 파형을 시각화합니다.

---

## 오실로스코프

### 파형 표시

```cpp
class Oscilloscope {
private:
    static const int BUFFER_SIZE = 512;
    float sampleBuffer[BUFFER_SIZE];
    int bufferPos = 0;
    
public:
    void addSample(int16_t sample) {
        sampleBuffer[bufferPos] = sample / 32768.0f;  // -1.0 ~ +1.0
        bufferPos = (bufferPos + 1) % BUFFER_SIZE;
    }
    
    void render(int width, int height) {
        int centerY = height / 2;
        
        for (int x = 0; x < width; x++) {
            int idx = (bufferPos + x * BUFFER_SIZE / width) % BUFFER_SIZE;
            float sample = sampleBuffer[idx];
            int y = centerY - (int)(sample * centerY);
            
            drawPixel(x, y, 0xFF00FF00);  // Green
        }
    }
};
```

---

## Voice 분석

### 개별 Voice 모니터

```cpp
class VoiceMonitor {
public:
    void displayVoiceInfo(int voice) {
        printf("=== Voice %d ===\n", voice);
        
        int base = voice * 16;
        
        // 볼륨
        int8_t volL = (int8_t)dsp.registers[base + 0x00];
        int8_t volR = (int8_t)dsp.registers[base + 0x01];
        printf("Volume: L=%d, R=%d\n", volL, volR);
        
        // 피치
        uint16_t pitch = dsp.registers[base + 0x02] |
                        (dsp.registers[base + 0x03] << 8);
        printf("Pitch: %d (%.2f Hz)\n", pitch, pitch * 32000.0f / 4096.0f);
        
        // 샘플 번호
        uint8_t srcn = dsp.registers[base + 0x04];
        printf("Sample: %d\n", srcn);
        
        // ADSR
        uint8_t adsr1 = dsp.registers[base + 0x05];
        uint8_t adsr2 = dsp.registers[base + 0x06];
        bool adsrEnable = adsr1 & 0x80;
        printf("ADSR: %s\n", adsrEnable ? "Enabled" : "Disabled");
        
        // 엔벨로프
        uint8_t envx = dsp.registers[base + 0x08];
        printf("Envelope: %d/127\n", envx);
        
        // 출력
        int8_t outx = (int8_t)dsp.registers[base + 0x09];
        printf("Output: %d\n", outx);
    }
    
    void renderVoiceWaveform(int voice) {
        // Voice의 출력 샘플 수집
        const int SAMPLES = 256;
        int16_t samples[SAMPLES];
        
        for (int i = 0; i < SAMPLES; i++) {
            samples[i] = getVoiceOutput(voice);
        }
        
        // 파형 그리기
        for (int x = 0; x < SAMPLES - 1; x++) {
            int y1 = 128 + samples[x] / 256;
            int y2 = 128 + samples[x + 1] / 256;
            drawLine(x, y1, x + 1, y2, 0xFFFFFF00);  // Yellow
        }
    }
};
```

---

## 구현 예제

### ImGui 오디오 뷰어

```cpp
class AudioViewerWindow {
private:
    bool showWaveform = true;
    bool showVoices[8] = {true, true, true, true, true, true, true, true};
    
public:
    void render() {
        ImGui::Begin("Audio Viewer");
        
        // 메인 출력
        if (ImGui::CollapsingHeader("Main Output")) {
            ImGui::Checkbox("Show Waveform", &showWaveform);
            
            if (showWaveform) {
                ImGui::PlotLines("Left", leftSamples, 512, 0,
                                nullptr, -1.0f, 1.0f, ImVec2(0, 80));
                ImGui::PlotLines("Right", rightSamples, 512, 0,
                                nullptr, -1.0f, 1.0f, ImVec2(0, 80));
            }
        }
        
        // Voice별 표시
        if (ImGui::CollapsingHeader("Voices")) {
            for (int v = 0; v < 8; v++) {
                ImGui::PushID(v);
                ImGui::Checkbox("##voice", &showVoices[v]);
                ImGui::SameLine();
                
                if (ImGui::TreeNode("Voice", "Voice %d", v)) {
                    displayVoiceControls(v);
                    
                    if (showVoices[v]) {
                        ImGui::PlotLines("##waveform",
                                        voiceSamples[v], 256, 0,
                                        nullptr, -1.0f, 1.0f,
                                        ImVec2(0, 60));
                    }
                    
                    ImGui::TreePop();
                }
                
                ImGui::PopID();
            }
        }
        
        ImGui::End();
    }
    
    void displayVoiceControls(int voice) {
        int base = voice * 16;
        
        // 볼륨 슬라이더
        int volL = (int8_t)dsp.registers[base + 0x00];
        int volR = (int8_t)dsp.registers[base + 0x01];
        ImGui::SliderInt("Vol L", &volL, -128, 127);
        ImGui::SliderInt("Vol R", &volR, -128, 127);
        
        // 피치
        int pitch = dsp.registers[base + 0x02] |
                   (dsp.registers[base + 0x03] << 8);
        ImGui::Text("Pitch: %d", pitch);
        
        // 엔벨로프 바
        int envx = dsp.registers[base + 0x08];
        ImGui::ProgressBar(envx / 127.0f, ImVec2(0, 0));
        ImGui::SameLine();
        ImGui::Text("ENVX: %d", envx);
    }
};
```

### 스펙트럼 분석

```cpp
class SpectrumAnalyzer {
private:
    float fftOutput[512];
    
public:
    void analyze(const int16_t* samples, int count) {
        // FFT 수행 (간소화)
        for (int i = 0; i < 512; i++) {
            fftOutput[i] = 0.0f;
        }
        
        // ... FFT 계산 ...
    }
    
    void render() {
        for (int i = 0; i < 256; i++) {
            float magnitude = fftOutput[i];
            int height = (int)(magnitude * 100);
            
            // 주파수 막대 그리기
            fillRect(i * 2, 200 - height, 2, height, 0xFF00FF00);
        }
    }
};
```

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete










