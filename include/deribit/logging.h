#ifndef HFT_DERIBIT_LOGGING_H
#define HFT_DERIBIT_LOGGING_H

#include <memory>
#include <vector>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

namespace deribit {

enum class LogLevel {
    DEBUG,
    INFO,
    STRATEGY,
    WARNING,
    ERROR,
    CRITICAL
};

inline std::shared_ptr<spdlog::logger> logger;

/* Map LogLevel -> spdlog::level */
inline spdlog::level::level_enum to_spd(LogLevel lvl) {
    switch (lvl) {
        case LogLevel::DEBUG:    return spdlog::level::debug;
        case LogLevel::INFO:     return spdlog::level::info;
        case LogLevel::WARNING:  return spdlog::level::warn;
        case LogLevel::ERROR:    return spdlog::level::err;
        case LogLevel::CRITICAL: return spdlog::level::critical;
        case LogLevel::STRATEGY: return spdlog::level::info;   // Same as INFO, but formatted separately
    }
    return spdlog::level::info;
}

/* Initialize logger */
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

/* Set log level at runtime */
inline void set_log_level(LogLevel lvl) {
    if (!logger) return;
    auto spd_lvl = to_spd(lvl);
    logger->set_level(spd_lvl);
    spdlog::set_level(spd_lvl);
}

/* Macro helpers */
#define LOG_DEBUG(msg)     deribit::logger->debug(msg)
#define LOG_INFO(msg)      deribit::logger->info(msg)
#define LOG_WARN(msg)      deribit::logger->warn(msg)
#define LOG_ERROR(msg)     deribit::logger->error(msg)
#define LOG_STRATEGY(msg)  deribit::logger->info("[STRATEGY] {}", msg)
#define LOG_TIMER(msg)     deribit::logger->info("[TIMER] {}", msg)

#define SET_LOG_LEVEL(lvl) deribit::set_log_level(lvl)

} // namespace deribit

#endif // HFT_DERIBIT_LOGGING_H
