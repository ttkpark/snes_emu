# Performance Profiling - 성능 분석 도구

## 📋 목차
1. [개요](#개요)
2. [프레임 타이밍](#프레임-타이밍)
3. [핫스팟 분석](#핫스팟-분석)
4. [구현 예제](#구현-예제)

---

## 개요

성능 프로파일러는 에뮬레이터의 병목 지점을 찾고 최적화합니다.

---

## 프레임 타이밍

### FPS 측정

```cpp
class FPSCounter {
private:
    int frameCount = 0;
    double lastTime = 0.0;
    double fps = 0.0;
    
public:
    void update() {
        frameCount++;
        double currentTime = getTime();
        
        if (currentTime - lastTime >= 1.0) {
            fps = frameCount / (currentTime - lastTime);
            frameCount = 0;
            lastTime = currentTime;
        }
    }
    
    double getFPS() const { return fps; }
};
```

### 프레임 시간 분석

```cpp
class FrameTimer {
private:
    struct FrameData {
        double cpuTime;
        double ppuTime;
        double apuTime;
        double totalTime;
    };
    
    std::vector<FrameData> history;
    
public:
    void beginFrame() {
        frameStart = getTime();
    }
    
    void endFrame() {
        FrameData data;
        data.cpuTime = cpuTimer.elapsed();
        data.ppuTime = ppuTimer.elapsed();
        data.apuTime = apuTimer.elapsed();
        data.totalTime = getTime() - frameStart;
        
        history.push_back(data);
        if (history.size() > 60) {
            history.erase(history.begin());
        }
    }
    
    void printStats() {
        double avgCPU = 0, avgPPU = 0, avgAPU = 0;
        for (auto& f : history) {
            avgCPU += f.cpuTime;
            avgPPU += f.ppuTime;
            avgAPU += f.apuTime;
        }
        
        int count = history.size();
        printf("Average frame time:\n");
        printf("  CPU: %.2f ms (%.1f%%)\n", avgCPU / count * 1000,
               avgCPU / (avgCPU + avgPPU + avgAPU) * 100);
        printf("  PPU: %.2f ms (%.1f%%)\n", avgPPU / count * 1000,
               avgPPU / (avgCPU + avgPPU + avgAPU) * 100);
        printf("  APU: %.2f ms (%.1f%%)\n", avgAPU / count * 1000,
               avgAPU / (avgCPU + avgPPU + avgAPU) * 100);
    }
};
```

---

## 핫스팟 분석

### 함수별 시간 측정

```cpp
class Profiler {
private:
    struct FunctionStats {
        std::string name;
        int callCount = 0;
        double totalTime = 0.0;
        double minTime = DBL_MAX;
        double maxTime = 0.0;
    };
    
    std::map<std::string, FunctionStats> stats;
    
public:
    class ScopedTimer {
    private:
        Profiler& profiler;
        std::string name;
        double startTime;
        
    public:
        ScopedTimer(Profiler& p, const std::string& n)
            : profiler(p), name(n) {
            startTime = getTime();
        }
        
        ~ScopedTimer() {
            double elapsed = getTime() - startTime;
            profiler.record(name, elapsed);
        }
    };
    
    void record(const std::string& name, double time) {
        auto& s = stats[name];
        s.name = name;
        s.callCount++;
        s.totalTime += time;
        s.minTime = std::min(s.minTime, time);
        s.maxTime = std::max(s.maxTime, time);
    }
    
    void printReport() {
        printf("=== Profiling Report ===\n");
        printf("%-30s %10s %12s %12s %12s %12s\n",
               "Function", "Calls", "Total (ms)", "Avg (ms)", "Min (ms)", "Max (ms)");
        printf("------------------------------------------------------------------------------------\n");
        
        // 시간 순으로 정렬
        std::vector<FunctionStats> sorted;
        for (auto& p : stats) {
            sorted.push_back(p.second);
        }
        std::sort(sorted.begin(), sorted.end(),
                 [](const FunctionStats& a, const FunctionStats& b) {
                     return a.totalTime > b.totalTime;
                 });
        
        for (auto& s : sorted) {
            printf("%-30s %10d %12.2f %12.4f %12.4f %12.4f\n",
                   s.name.c_str(), s.callCount,
                   s.totalTime * 1000,
                   s.totalTime / s.callCount * 1000,
                   s.minTime * 1000,
                   s.maxTime * 1000);
        }
    }
};

// 사용 예제
void CPU::executeInstruction() {
    Profiler::ScopedTimer timer(profiler, "CPU::executeInstruction");
    // ...
}
```

---

## 구현 예제

### ImGui 프로파일러

```cpp
class ProfilerWindow {
private:
    bool showGraph = true;
    int historySize = 60;
    
public:
    void render() {
        ImGui::Begin("Performance Profiler");
        
        // FPS
        ImGui::Text("FPS: %.1f", fpsCounter.getFPS());
        ImGui::Text("Frame Time: %.2f ms", frameTimer.getLastFrameTime() * 1000);
        
        // 컴포넌트별 시간
        if (ImGui::CollapsingHeader("Component Breakdown")) {
            ImGui::Checkbox("Show Graph", &showGraph);
            
            if (showGraph) {
                float cpuTimes[60], ppuTimes[60], apuTimes[60];
                for (int i = 0; i < 60; i++) {
                    cpuTimes[i] = frameTimer.getHistory()[i].cpuTime * 1000;
                    ppuTimes[i] = frameTimer.getHistory()[i].ppuTime * 1000;
                    apuTimes[i] = frameTimer.getHistory()[i].apuTime * 1000;
                }
                
                ImGui::PlotLines("CPU", cpuTimes, 60, 0, nullptr, 0, 20, ImVec2(0, 80));
                ImGui::PlotLines("PPU", ppuTimes, 60, 0, nullptr, 0, 20, ImVec2(0, 80));
                ImGui::PlotLines("APU", apuTimes, 60, 0, nullptr, 0, 20, ImVec2(0, 80));
            }
        }
        
        // 함수별 통계
        if (ImGui::CollapsingHeader("Function Statistics")) {
            ImGui::BeginChild("FunctionStats", ImVec2(0, 300), true);
            
            ImGui::Columns(6);
            ImGui::Text("Function"); ImGui::NextColumn();
            ImGui::Text("Calls"); ImGui::NextColumn();
            ImGui::Text("Total (ms)"); ImGui::NextColumn();
            ImGui::Text("Avg (ms)"); ImGui::NextColumn();
            ImGui::Text("Min (ms)"); ImGui::NextColumn();
            ImGui::Text("Max (ms)"); ImGui::NextColumn();
            ImGui::Separator();
            
            for (auto& stat : profiler.getStats()) {
                ImGui::Text("%s", stat.name.c_str()); ImGui::NextColumn();
                ImGui::Text("%d", stat.callCount); ImGui::NextColumn();
                ImGui::Text("%.2f", stat.totalTime * 1000); ImGui::NextColumn();
                ImGui::Text("%.4f", stat.totalTime / stat.callCount * 1000); ImGui::NextColumn();
                ImGui::Text("%.4f", stat.minTime * 1000); ImGui::NextColumn();
                ImGui::Text("%.4f", stat.maxTime * 1000); ImGui::NextColumn();
            }
            
            ImGui::Columns(1);
            ImGui::EndChild();
        }
        
        // 제어
        if (ImGui::Button("Reset Statistics")) {
            profiler.reset();
        }
        ImGui::SameLine();
        if (ImGui::Button("Save Report")) {
            profiler.saveReport("profile_report.txt");
        }
        
        ImGui::End();
    }
};
```

### 메모리 사용량 추적

```cpp
class MemoryProfiler {
private:
    size_t peakUsage = 0;
    std::map<std::string, size_t> allocations;
    
public:
    void* allocate(size_t size, const char* tag) {
        void* ptr = malloc(size);
        allocations[tag] += size;
        
        size_t total = getTotalAllocated();
        peakUsage = std::max(peakUsage, total);
        
        return ptr;
    }
    
    void deallocate(void* ptr, size_t size, const char* tag) {
        free(ptr);
        allocations[tag] -= size;
    }
    
    size_t getTotalAllocated() const {
        size_t total = 0;
        for (auto& p : allocations) {
            total += p.second;
        }
        return total;
    }
    
    void printReport() {
        printf("=== Memory Usage ===\n");
        printf("Current: %.2f MB\n", getTotalAllocated() / 1024.0 / 1024.0);
        printf("Peak: %.2f MB\n", peakUsage / 1024.0 / 1024.0);
        printf("\nBy Category:\n");
        
        for (auto& p : allocations) {
            printf("  %s: %.2f MB\n", p.first.c_str(), p.second / 1024.0 / 1024.0);
        }
    }
};
```

---

**작성일**: 2025-12-14  
**버전**: 1.0  
**상태**: Complete










