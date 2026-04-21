#include "Logger.h"
#include <iostream>

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

const char* level_to_string(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
    }
    return "UNKNOWN";
}

template<typename... Args>
void Logger::log(LogLevel level, const char* file, int line, Args&&... args) {
    if (level < level_.load()) return;
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::cout << "[" << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S")
              << "." << std::setfill('0') << std::setw(3) << ms.count() << "] "
              << "[" << level_to_string(level) << "] "
              << "[" << file << ":" << line << "] ";
    
    format(std::cout, std::forward<Args>(args)...);
    std::cout << "\n";
}

template<typename... Args>
void Logger::debug(const char* file, int line, Args&&... args) {
    log(LogLevel::DEBUG, file, line, std::forward<Args>(args)...);
}

template<typename... Args>
void Logger::info(const char* file, int line, Args&&... args) {
    log(LogLevel::INFO, file, line, std::forward<Args>(args)...);
}

template<typename... Args>
void Logger::warn(const char* file, int line, Args&&... args) {
    log(LogLevel::WARN, file, line, std::forward<Args>(args)...);
}

template<typename... Args>
void Logger::error(const char* file, int line, Args&&... args) {
    log(LogLevel::ERROR, file, line, std::forward<Args>(args)...);
}
