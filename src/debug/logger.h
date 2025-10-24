#pragma once
#include <fstream>
#include <string>
#include <mutex>
#include <deque>

// Define to enable logging (comment out to disable for performance)
#define ENABLE_LOGGING  // Disabled for performance

class Logger {
public:
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }
    
    void logCPU(const std::string& message);
    void logAPU(const std::string& message);
    void logPPU(const std::string& message);
    
    void flush();
    void setMaxEntries(int max) { m_maxEntries = max; }
    void setLoggingEnabled(bool enabled) { m_loggingEnabled = enabled; }
    bool isLoggingEnabled() const { return m_loggingEnabled; }
    
private:
    Logger();
    ~Logger();
    
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    void writeLog(const std::string& filename, const std::string& message);
    
    std::ofstream m_cpuLog;
    std::ofstream m_apuLog;
    std::ofstream m_ppuLog;
    
    std::deque<std::string> m_cpuBuffer;
    std::deque<std::string> m_apuBuffer;
    std::deque<std::string> m_ppuBuffer;
    
    std::mutex m_cpuMutex;
    std::mutex m_apuMutex;
    std::mutex m_ppuMutex;
    
    int m_maxEntries;
    bool m_loggingEnabled;
};

