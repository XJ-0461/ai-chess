#pragma once

#include <string>
#include <string_view>

#include <spdlog/spdlog.h>

namespace chess::log {

enum class LogLevel {
    TRACE = 0,
    DEBUG = 1,
    INFO = 2,
    WARNING = 3,
    ERROR = 4,
    CRITICAL = 5,
    OFF = 6
};

// Convert string to LogLevel
constexpr LogLevel LogLevelFromString(const std::string_view level_str) {
    if (level_str == "trace") return LogLevel::TRACE;
    if (level_str == "debug") return LogLevel::DEBUG;
    if (level_str == "info") return LogLevel::INFO;
    if (level_str == "warning") return LogLevel::WARNING;
    if (level_str == "error") return LogLevel::ERROR;
    if (level_str == "critical") return LogLevel::CRITICAL;
    if (level_str == "off") return LogLevel::OFF;
    return LogLevel::INFO; // default
}

// Convert LogLevel to string
constexpr std::string_view LogLevelToString(const LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "trace";
        case LogLevel::DEBUG: return "debug";
        case LogLevel::INFO: return "info";
        case LogLevel::WARNING: return "warning";
        case LogLevel::ERROR: return "error";
        case LogLevel::CRITICAL: return "critical";
        case LogLevel::OFF: return "off";
        default: return "info";
    }
}

// Convert LogLevel to spdlog::level::level_enum
constexpr spdlog::level::level_enum LogLevelToSpdlog(const LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return spdlog::level::trace;
        case LogLevel::DEBUG: return spdlog::level::debug;
        case LogLevel::INFO: return spdlog::level::info;
        case LogLevel::WARNING: return spdlog::level::warn;
        case LogLevel::ERROR: return spdlog::level::err;
        case LogLevel::CRITICAL: return spdlog::level::critical;
        case LogLevel::OFF: return spdlog::level::off;
        default: return spdlog::level::info;
    }
}

// Convert spdlog::level::level_enum to LogLevel
constexpr LogLevel LogLevelFromSpdlog(const spdlog::level::level_enum level) {
    switch (level) {
        case spdlog::level::trace: return LogLevel::TRACE;
        case spdlog::level::debug: return LogLevel::DEBUG;
        case spdlog::level::info: return LogLevel::INFO;
        case spdlog::level::warn: return LogLevel::WARNING;
        case spdlog::level::err: return LogLevel::ERROR;
        case spdlog::level::critical: return LogLevel::CRITICAL;
        case spdlog::level::off: return LogLevel::OFF;
        default: return LogLevel::INFO;
    }
}

} // namespace chess::log
