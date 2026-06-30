#pragma once

#include <cstdint>

#include <spdlog/spdlog.h>

#include "MakeLog.hpp"

#if defined(CHESS_INCLUDE_LOG_LEVEL_OFF)
    static constexpr std::uint8_t kChessLogLevelThreshold = 6; // OFF - disable all logging
#elif defined(CHESS_INCLUDE_LOG_LEVEL_TRACE)
    static constexpr std::uint8_t kChessLogLevelThreshold = 0;
#elif defined(CHESS_INCLUDE_LOG_LEVEL_DEBUG)
    static constexpr std::uint8_t kChessLogLevelThreshold = 1;
#elif defined(CHESS_INCLUDE_LOG_LEVEL_INFO)
    static constexpr std::uint8_t kChessLogLevelThreshold = 2;
#elif defined(CHESS_INCLUDE_LOG_LEVEL_WARN)
    static constexpr std::uint8_t kChessLogLevelThreshold = 3;
#elif defined(CHESS_INCLUDE_LOG_LEVEL_ERROR)
    static constexpr std::uint8_t kChessLogLevelThreshold = 4;
#elif defined(CHESS_INCLUDE_LOG_LEVEL_CRITICAL)
    static constexpr std::uint8_t kChessLogLevelThreshold = 5;
#else
    static constexpr std::uint8_t kChessLogLevelThreshold = 2; // default INFO
#endif

// Extract filename only from __FILE__ (not full path)
#define CHESS_FILENAME (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : \
                         strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : \
                         __FILE__)

#define CHESS_LOG_TRACE(logger_ptr, ...) \
    if constexpr (kChessLogLevelThreshold <= 0) { \
        if (logger_ptr) { \
            auto& logger = logger_ptr; \
            __VA_ARGS__ \
        } \
    }

#define CHESS_GET_LOGGER_TRACE(logger_name_str, ...) \
    if constexpr (kChessLogLevelThreshold <= 0) { \
        const auto logger = spdlog::get(logger_name_str); \
        if (logger) { \
            __VA_ARGS__ \
        } \
    }

#define CHESS_LOG_DEBUG(logger_ptr, ...) \
    if constexpr (kChessLogLevelThreshold <= 1) { \
        if (logger_ptr) { \
            auto& logger = logger_ptr; \
            __VA_ARGS__ \
        } \
    }

#define CHESS_GET_LOGGER_DEBUG(logger_name_str, ...) \
    if constexpr (kChessLogLevelThreshold <= 1) { \
        const auto logger = spdlog::get(logger_name_str); \
        if (logger) { \
            __VA_ARGS__ \
        } \
    }

#define CHESS_LOG_INFO(logger_ptr, ...) \
    if constexpr (kChessLogLevelThreshold <= 2) { \
        if (logger_ptr) { \
            auto& logger = logger_ptr; \
            __VA_ARGS__ \
        } \
    }

#define CHESS_GET_LOGGER_INFO(logger_name_str, ...) \
    if constexpr (kChessLogLevelThreshold <= 2) { \
        const auto logger = spdlog::get(logger_name_str); \
        if (logger) { \
            __VA_ARGS__ \
        } \
    }

#define CHESS_LOG_WARN(logger_ptr, ...) \
    if constexpr (kChessLogLevelThreshold <= 3) { \
        if (logger_ptr) { \
            auto& logger = logger_ptr; \
            __VA_ARGS__ \
        } \
    }

#define CHESS_GET_LOGGER_WARN(logger_name_str, ...) \
    if constexpr (kChessLogLevelThreshold <= 3) { \
        const auto logger = spdlog::get(logger_name_str); \
        if (logger) { \
            __VA_ARGS__ \
        } \
    }

#define CHESS_LOG_ERROR(logger_ptr, ...) \
    if constexpr (kChessLogLevelThreshold <= 4) { \
        if (logger_ptr) { \
            auto& logger = logger_ptr; \
            __VA_ARGS__ \
        } \
    }

#define CHESS_GET_LOGGER_ERROR(logger_name_str, ...) \
    if constexpr (kChessLogLevelThreshold <= 4) { \
        const auto logger = spdlog::get(logger_name_str); \
        if (logger) { \
            __VA_ARGS__ \
        } \
    }

#define CHESS_LOG_CRITICAL(logger_ptr, ...) \
    if constexpr (kChessLogLevelThreshold <= 5) { \
        if (logger_ptr) { \
            auto& logger = logger_ptr; \
            __VA_ARGS__ \
        } \
    }

#define CHESS_GET_LOGGER_CRITICAL(logger_name_str, ...) \
    if constexpr (kChessLogLevelThreshold <= 5) { \
        const auto logger = spdlog::get(logger_name_str); \
        if (logger) { \
            __VA_ARGS__ \
        } \
    }

// Terse structured trace helper built on CHESS_GET_LOGGER_TRACE. The variadic
// args are MakeLog fields, wrapped in the outer braces here so call sites read
// as a flat list of {"key", value} pairs. Compiles to nothing unless the build
// includes TRACE (CHESS_INCLUDE_LOG_LEVEL_TRACE).
//
// Usage:
//   CHESS_TRACE_LOG("stdout_chess", {"operation", "enter"}, {"function", "OnTurnTransition"})
#define CHESS_TRACE_LOG(logger_name_str, ...) \
    CHESS_GET_LOGGER_TRACE(logger_name_str, \
        logger->trace(::chess::log::MakeLog({ __VA_ARGS__ })); \
    )
