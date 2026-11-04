#include "Plugin.h"

#include "DAEDRA_API.h"

namespace daedra::plugin
{
	namespace
	{
		constexpr std::uint32_t kPluginVersion = (0u << 16) | (1u << 8) | 0u;

		std::vector<std::pair<std::string, std::string>>& Registered()
		{
			static std::vector<std::pair<std::string, std::string>> docs;
			return docs;
		}

		void Log(const Diagnostics& a_diag)
		{
			for (const auto& d : a_diag) {
				switch (d.severity) {
				case Severity::Info:
					SKSE::log::info("{}", d.text);
					break;
				case Severity::Warn:
					SKSE::log::warn("{}", d.text);
					break;
				case Severity::Error:
					SKSE::log::error("{}", d.text);
					break;
				}
			}
		}

		class ApiV2 final : public DAEDRA_API::Interface2
		{
		public:
			std::uint32_t GetPluginVersion() const noexcept override { return kPluginVersion; }

			std::int32_t RegisterRulesJson(const char* a_ownerName, const char* a_json) noexcept override
			{
				try {
					if (!a_json) {
						return -1;
					}
					const std::string owner = a_ownerName && *a_ownerName ? a_ownerName : "api";
					const auto        before = GetEngine().RuleCount();
					const auto        diag = GetEngine().AddJson("api:" + owner, a_json, ParseOptions{ GetConfig().strict });
					Log(diag);
					Registered().emplace_back("api:" + owner, a_json);
					const auto after = GetEngine().RuleCount();
					SKSE::log::info("API: '{}' registered {} rule(s)", owner, after - before);
					return static_cast<std::int32_t>(after - before);
				} catch (...) {
					return -1;
				}
			}
		};

		ApiV2 g_apiV2;
	}

	void LogDiagnostics(const Diagnostics& a_diag)
	{
		Log(a_diag);
	}

	void ReapplyRegisteredRules()
	{
		for (const auto& [name, json] : Registered()) {
			Log(GetEngine().AddJson(name, json, ParseOptions{ GetConfig().strict }));
		}
	}

	void BroadcastApi()
	{
		const auto messaging = SKSE::GetMessagingInterface();
		if (!messaging) {
			return;
		}
		static DAEDRA_API::RequestReply reply;
		reply.version = DAEDRA_API::kInterfaceVersion2;
		reply.interface2 = &g_apiV2;
		messaging->Dispatch(DAEDRA_API::kMsgInterfaceReady, &reply, sizeof(reply), nullptr);
		SKSE::log::info("API v{} broadcast to other plugins", reply.version);
	}
}
