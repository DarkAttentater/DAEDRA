#include "Plugin.h"

namespace daedra::plugin
{
	void LogDiagnostics(const Diagnostics&);
	void ReapplyRegisteredRules();
	void BroadcastApi();

	Engine& GetEngine()
	{
		static Engine engine;
		return engine;
	}

	void ReloadRules()
	{
		try {
			const auto diag = GetEngine().LoadDirectory("Data/SKSE/Plugins/DAEDRA", ParseOptions{ GetConfig().strict });
			LogDiagnostics(diag);
			ReapplyRegisteredRules();
			SKSE::log::info("Rules loaded: {}", GetEngine().RuleCount());
		} catch (const std::exception& e) {
			SKSE::log::error("Rule reload failed: {}", e.what());
		} catch (...) {
			SKSE::log::error("Rule reload failed");
		}
	}

	namespace
	{
		void OnMessage(SKSE::MessagingInterface::Message* a_msg)
		{
			try {
				switch (a_msg->type) {
				case SKSE::MessagingInterface::kPostPostLoad:
					BroadcastApi();
					break;
				case SKSE::MessagingInterface::kDataLoaded:
					InstallSinks();
					LoadLabelCache();
					ReloadRules();
					break;
				case SKSE::MessagingInterface::kPostLoadGame:
				case SKSE::MessagingInterface::kNewGame:
					GetEngine().ClearCache();
					break;
				default:
					break;
				}
			} catch (...) {
			}
		}
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
	SKSE::Init(a_skse);
	daedra::plugin::InitLog();
	SKSE::log::info("DAEDRA 0.1.0 r{} starting (runtime {})", DAEDRA_REVISION, a_skse->RuntimeVersion().string());
	daedra::plugin::LoadConfig();

	if (const auto messaging = SKSE::GetMessagingInterface()) {
		messaging->RegisterListener(daedra::plugin::OnMessage);
	} else {
		SKSE::log::error("Messaging interface unavailable; plugin stays idle");
	}
	return true;
}

// Address Library (no hard-coded offsets), post-1.6.629 struct layouts as provided by CommonLibSSE-NG.
SKSEPluginInfo(
	.Version = REL::Version{ 0, 1, 0, 0 },
	.Name = "DAEDRA"sv,
	.Author = "DarkAttentater"sv,
	.StructCompatibility = SKSE::StructCompatibility::Dependent,
	.RuntimeCompatibility = SKSE::PluginDeclaration::RuntimeCompatibility(SKSE::VersionIndependence::AddressLibrary, true),
	.MinimumSKSEVersion = REL::Version{ 0, 0, 0, 0 })
