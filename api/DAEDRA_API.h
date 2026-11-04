#pragma once
// DAEDRA public API, interface version 2.
//
// Pure C++ interface with no CommonLibSSE or STL types in the ABI, so any plugin can
// include this header regardless of the toolchain it was built with.
//
// DAEDRA broadcasts kMsgInterfaceReady to every plugin during SKSE's kPostPostLoad. To
// receive it, listen to messages from "DAEDRA" in your own kPostLoad handler:
//
//   skse->GetMessagingInterface()->RegisterListener("DAEDRA", [](SKSE::MessagingInterface::Message* m) {
//       if (m->type == DAEDRA_API::kMsgInterfaceReady && m->data) {
//           auto* reply = static_cast<DAEDRA_API::RequestReply*>(m->data);
//           if (reply->interface2) {
//               reply->interface2->RegisterRulesJson("MyMod", R"({ "version": 1, "rules": [ ... ] })");
//           }
//       }
//   });
//
// Rules registered this way use exactly the same JSON schema as rule files. They rank
// after file rules of the same priority (so they win ties) and are re-applied on hot reload.

#include <cstdint>

namespace DAEDRA_API
{
	inline constexpr std::uint32_t kInterfaceVersion2 = 2;
	inline constexpr std::uint32_t kMsgInterfaceReady = 'DAER';

	struct Interface2
	{
		virtual ~Interface2() = default;

		// Plugin version packed as (major << 16) | (minor << 8) | patch.
		virtual std::uint32_t GetPluginVersion() const noexcept = 0;

		// Parses a rule document (same schema as the JSON files). Returns the number of rules
		// accepted, or -1 if the document could not be read at all. Problems go to the DAEDRA log,
		// prefixed with a_ownerName. Call from the main thread.
		virtual std::int32_t RegisterRulesJson(const char* a_ownerName, const char* a_json) noexcept = 0;
	};

	struct RequestReply
	{
		std::uint32_t version{ 0 };  // highest interface version the plugin supports
		Interface2*   interface2{ nullptr };
	};
}
