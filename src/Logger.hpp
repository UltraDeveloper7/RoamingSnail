#pragma once
#include "precompiled.h"

class Logger {
public:
    enum class LogLevel {
        INFO,
        WARNING,
        ERROR
    };

    static void Init(const std::string& filename) {
        instance().log_file_.open(filename, std::ios::out | std::ios::app);
    }

    static void Log(const std::string& message, LogLevel level = LogLevel::INFO) {
        std::string level_str;
        switch (level) {
            case LogLevel::INFO: level_str = "INFO"; break;
            case LogLevel::WARNING: level_str = "WARNING"; break;
            case LogLevel::ERROR: level_str = "ERROR"; break;
        }
        instance().log_file_ << "[" << level_str << "] " << message << std::endl;
        std::cout << "[" << level_str << "] " << message << std::endl;
    }

    static void Close() {
        instance().log_file_.close();
    }

private:
    Logger() = default;
    ~Logger() = default;

    static Logger& instance() {
        static Logger instance;
        return instance;
    }

    std::ofstream log_file_;
};
