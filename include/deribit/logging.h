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

	/* Initialize the logger for Deribit module */
	inline void init_logging(const std::string& filename = "deribit.log") {
		// Create a vector of sinks
		std::vector<spdlog::sink_ptr> sinks;

		// Console sink
		auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		console_sink->set_pattern("[%H:%M:%S.%e] [T%t] [%l] %v");
		sinks.push_back(console_sink);
		// File sink
		auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(filename, true);
		file_sink->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [T%t] [%l] %v");
		sinks.push_back(file_sink);

		// Create the logger with both sinks
		logger = std::make_shared<spdlog::logger>("deribit", begin(sinks), end(sinks));
		logger->set_level(spdlog::level::debug);
		spdlog::set_default_logger(logger);
	}

	// Macro helpers
	#define LOG_DEBUG(msg)     deribit::logger->debug(msg)
	#define LOG_INFO(msg)      deribit::logger->info(msg)
	#define LOG_WARN(msg)      deribit::logger->warn(msg)
	#define LOG_ERROR(msg)     deribit::logger->error(msg)
	#define LOG_STRATEGY(msg)  deribit::logger->info("[STRATEGY] {}", msg)
	#define LOG_TIMER(msg)     deribit::logger->info("[TIMER] {}", msg)

} // namespace deribit
#endif // HFT_DERIBIT_LOGGING_H
