#include "Plugin.h"

#include <spdlog/sinks/basic_file_sink.h>

namespace daedra::plugin
{
	void InitLog()
	{
		auto dir = SKSE::log::log_directory();
		if (!dir) {
			return;  // no log is better than a crash
		}
		try {
			auto path = *dir / (std::string(kPluginName) + ".log");
			auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path.string(), true);
			auto log = std::make_shared<spdlog::logger>("daedra", std::move(sink));
			log->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
			log->set_level(spdlog::level::trace);
			log->flush_on(spdlog::level::info);
			spdlog::set_default_logger(std::move(log));
		} catch (...) {
		}
	}

	void ApplyLogLevel()
	{
		const auto& lvl = GetConfig().logLevel;
		spdlog::level::level_enum level = spdlog::level::info;
		if (lvl == "trace") {
			level = spdlog::level::trace;
		} else if (lvl == "debug") {
			level = spdlog::level::debug;
		} else if (lvl == "warn") {
			level = spdlog::level::warn;
		} else if (lvl == "error") {
			level = spdlog::level::err;
		}
		spdlog::set_level(level);
	}
}
