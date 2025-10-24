#include "logger.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>

Logger::Logger() : m_maxEntries(1000), m_loggingEnabled(false) {
#ifdef ENABLE_LOGGING
    m_loggingEnabled = true;
    m_cpuLog.open("cpu_trace.log", std::ios::out | std::ios::trunc);
    m_apuLog.open("apu_trace.log", std::ios::out | std::ios::trunc);
    m_ppuLog.open("ppu_trace.log", std::ios::out | std::ios::trunc);
    
    if (!m_cpuLog.is_open()) {
        std::cerr << "Failed to open cpu_trace.log" << std::endl;
    }
    if (!m_apuLog.is_open()) {
        std::cerr << "Failed to open apu_trace.log" << std::endl;
    }
    if (!m_ppuLog.is_open()) {
        std::cerr << "Failed to open ppu_trace.log" << std::endl;
    }
    
    if (m_loggingEnabled) {
        // Write headers
        if (m_cpuLog.is_open()) {
            m_cpuLog << "=== CPU Execution Trace ===" << std::endl;
            m_cpuLog << "Format: [Cyc:XXXXXXXXXX F:XXXX] PC | Opcode | Instruction | A | X | Y | SP | P | DBR | PBR | D | Flags" << std::endl;
            m_cpuLog << "Cyc = CPU Cycle Count (monotonic), F = Frame Count" << std::endl;
            m_cpuLog << "========================================" << std::endl;
        }
        
        if (m_apuLog.is_open()) {
            m_apuLog << "=== APU (SPC700) Execution Trace ===" << std::endl;
            m_apuLog << "Format: [Frame] PC | Opcode | A | X | Y | SP | PSW | Instruction" << std::endl;
            m_apuLog << "========================================" << std::endl;
        }
        
        if (m_ppuLog.is_open()) {
            m_ppuLog << "=== PPU Rendering Trace ===" << std::endl;
            m_ppuLog << "Format: [Frame] Scanline | Event | Details" << std::endl;
            m_ppuLog << "========================================" << std::endl;
        }
    }
#endif
}

Logger::~Logger() {
    flush();
    
    if (m_cpuLog.is_open()) {
        m_cpuLog.close();
    }
    if (m_apuLog.is_open()) {
        m_apuLog.close();
    }
    if (m_ppuLog.is_open()) {
        m_ppuLog.close();
    }
}

void Logger::logCPU(const std::string& message) {
    if (!m_loggingEnabled) return;
    
    std::lock_guard<std::mutex> lock(m_cpuMutex);
    
    // Write directly to file for continuous logging
    if (m_cpuLog.is_open()) {
        m_cpuLog << message << std::endl;
    }
}

void Logger::logAPU(const std::string& message) {
    if (!m_loggingEnabled) return;
    
    std::lock_guard<std::mutex> lock(m_apuMutex);
    
    // Write directly to file for continuous logging
    if (m_apuLog.is_open()) {
        m_apuLog << message << std::endl;
    }
}

void Logger::logPPU(const std::string& message) {
    if (!m_loggingEnabled) return;
    
    std::lock_guard<std::mutex> lock(m_ppuMutex);
    
    // Write directly to file for continuous logging
    if (m_ppuLog.is_open()) {
        m_ppuLog << message << std::endl;
    }
}

void Logger::flush() {
    // Flush all logs to disk
    {
        std::lock_guard<std::mutex> lock(m_cpuMutex);
        if (m_cpuLog.is_open()) {
            m_cpuLog.flush();
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(m_apuMutex);
        if (m_apuLog.is_open()) {
            m_apuLog.flush();
        }
    }
    
    {
        std::lock_guard<std::mutex> lock(m_ppuMutex);
        if (m_ppuLog.is_open()) {
            m_ppuLog.flush();
        }
    }
}

