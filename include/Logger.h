#ifndef LOGGER_H
#define LOGGER_H

#include <iostream>
#include <sstream>
#include <mutex>
#include <atomic>
#include <chrono>
#include <iomanip>

enum class LogLevel { DEBUG, INFO, WARN, ERROR };

class Logger {
public:
    static Logger& instance();
    
    void set_level(LogLevel level) { level_.store(level); }
    LogLevel get_level() const { return level_.load(); }
    
    template<typename... Args>
    void log(LogLevel level, const char* file, int line, Args&&... args);
    
    template<typename... Args>
    void debug(const char* file, int line, Args&&... args);
    
    template<typename... Args>
    void info(const char* file, int line, Args&&... args);
    
    template<typename... Args>
    void warn(const char* file, int line, Args&&... args);
    
    template<typename... Args>
    void error(const char* file, int line, Args&&... args);
    
private:
    Logger() = default;
    
    template<typename T>
    void format(std::ostream& os, T&& value) {
        os << value;
    }
    
    template<typename T, typename... Args>
    void format(std::ostream& os, T&& value, Args&&... args) {
        os << value;
        format(os, std::forward<Args>(args)...);
    }
    
    std::atomic<LogLevel> level_{LogLevel::INFO};
    std::mutex mutex_;
};

#define LOG_DEBUG(...) Logger::instance().debug(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_INFO(...) Logger::instance().info(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_WARN(...) Logger::instance().warn(__FILE__, __LINE__, __VA_ARGS__)
#define LOG_ERROR(...) Logger::instance().error(__FILE__, __LINE__, __VA_ARGS__)

#endif
