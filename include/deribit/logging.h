#ifndef HFT_DERIBIT_LOGGING_H
#define HFT_DERIBIT_LOGGING_H

#include <memory>
#include <vector>
#include <string>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace deribit {

inline std::shared_ptr<spdlog::logger> logger; // global logger instance

enum class LogLevel {
    DEBUG,
    INFO,
    STRATEGY,
    WARNING,
    ERROR,
    CRITICAL
};

/**
 * Translate the project's LogLevel enum to spdlog's level enum.
 *
 * The STRATEGY level is treated as informational but is intended for
 * separate formatting or filtering when used by higher-level code.
 *
 * @param lvl The LogLevel value to convert.
 * @return The corresponding spdlog level enum value.
 */
inline spdlog::level::level_enum to_spd(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::DEBUG:    return spdlog::level::debug;
        case LogLevel::INFO:     return spdlog::level::info;
        case LogLevel::WARNING:  return spdlog::level::warn;
        case LogLevel::ERROR:    return spdlog::level::err;
        case LogLevel::CRITICAL: return spdlog::level::critical;
        case LogLevel::STRATEGY: return spdlog::level::info;   // STRATEGY maps to info but can be formatted separately
    }
    return spdlog::level::info;
}

/**
 * Initialize the global logger instance used by the library and tests.
 *
 * This creates a console sink with colored output and a file sink that
 * appends to the provided filename. Both sinks share a common logger
 * instance which is set as the spdlog default logger.
 *
 * @param filename Path to the log file to use. Defaults to deribit.log.
 */
inline void init_logging(const std::string& filename = "deribit.log") {
    std::vector<spdlog::sink_ptr> sinks;

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    console_sink->set_pattern("[%H:%M:%S.%e] [T%t] [%^%l%$] %v");
    sinks.push_back(console_sink);

    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);
    file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [T%t] [%l] %v");
    sinks.push_back(file_sink);

    logger = std::make_shared<spdlog::logger>("deribit", begin(sinks), end(sinks));
    logger->set_level(spdlog::level::debug);
    spdlog::set_default_logger(logger);
}

/**
 * Adjust the logging level of the global logger at runtime.
 *
 * If the logger has not been initialized this function is a no-op.
 *
 * @param lvl The new log level to apply.
 */
inline void set_log_level(LogLevel lvl) {
    if (!logger) return;
    auto spd_lvl = to_spd(lvl);
    logger->set_level(spd_lvl);
    spdlog::set_level(spd_lvl);
}

/**
 * Convenience macros that forward to the shared logger instance.
 *
 * These macros provide short, familiar names for the common logging
 * operations used throughout the codebase. Strategy and timer logs are
 * formatted via an informational message wrapper so they can be filtered
 * or recognized easily in output.
 */
/* Macro helpers */
#define LOG_DEBUG(...)     deribit::logger->debug(__VA_ARGS__)
#define LOG_INFO(...)      deribit::logger->info(__VA_ARGS__)
#define LOG_WARN(...)      deribit::logger->warn(__VA_ARGS__)
#define LOG_ERROR(...)     deribit::logger->error(__VA_ARGS__)
#define LOG_STRATEGY(...)  deribit::logger->info("[STRATEGY] {}", fmt::format(__VA_ARGS__))
#define LOG_TIMER(...)     deribit::logger->info("[TIMER] {}", fmt::format(__VA_ARGS__))

#define SET_LOG_LEVEL(lvl) deribit::set_log_level(lvl)

} // namespace deribit

#endif // HFT_DERIBIT_LOGGING_H
