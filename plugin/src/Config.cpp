#include "Plugin.h"

#include <fstream>

namespace daedra::plugin
{
	void ApplyLogLevel();  // Log.cpp

	namespace
	{
		std::string Trim(std::string s)
		{
			const auto b = s.find_first_not_of(" \t\r\n");
			const auto e = s.find_last_not_of(" \t\r\n");
			return b == std::string::npos ? std::string{} : s.substr(b, e - b + 1);
		}

		bool ToBool(const std::string& v)
		{
			const auto l = ToLower(v);
			return l == "1" || l == "true" || l == "yes" || l == "on";
		}

		std::uint32_t ToKey(const std::string& v)
		{
			try {
				return static_cast<std::uint32_t>(std::stoul(v, nullptr, 0));  // accepts 0x..
			} catch (...) {
				return 0;
			}
		}
	}

	Config& GetConfig()
	{
		static Config config;
		return config;
	}

	void LoadConfig()
	{
		auto& cfg = GetConfig();
		cfg = Config{};

		std::ifstream in("Data/SKSE/Plugins/DAEDRA.ini");
		if (!in) {
			SKSE::log::info("No DAEDRA.ini found, using defaults");
			ApplyLogLevel();
			return;
		}

		std::string line;
		while (std::getline(in, line)) {
			if (const auto c = line.find_first_of(";#"); c != std::string::npos) {
				line.erase(c);
			}
			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			const auto key = ToLower(Trim(line.substr(0, eq)));
			const auto val = Trim(line.substr(eq + 1));
			if (key == "loglevel") {
				cfg.logLevel = ToLower(val);
			} else if (key == "logmatches") {
				cfg.logMatches = ToBool(val);
			} else if (key == "dumpactiveeffects") {
				cfg.dumpActiveEffects = ToBool(val);
			} else if (key == "strict") {
				cfg.strict = ToBool(val);
			} else if (key == "applytomenu") {
				cfg.applyToMenu = ToBool(val);
			} else if (key == "iconmimictype") {
				cfg.iconMimicType = ToBool(val);
			} else if (key == "reloadkey") {
				cfg.reloadKey = ToKey(val);
			} else if (key == "dumpkey") {
				cfg.dumpKey = ToKey(val);
			}
		}
		ApplyLogLevel();
		SKSE::log::info("Config: logLevel={} logMatches={} dumpActiveEffects={} strict={} applyToMenu={} reloadKey={} dumpKey={}",
			cfg.logLevel, cfg.logMatches, cfg.dumpActiveEffects, cfg.strict, cfg.applyToMenu, cfg.reloadKey, cfg.dumpKey);
	}
}
