#pragma once

#include "daedra/Engine.h"

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

namespace daedra::plugin
{
	inline constexpr auto kPluginName = "DAEDRA";

	struct Config
	{
		std::string   logLevel{ "info" };  // trace, debug, info, warn, error
		bool          logMatches{ false };
		bool          dumpActiveEffects{ false };  // dump on every Magic menu open
		bool          strict{ false };
		bool          applyToMenu{ false };  // M4, not implemented yet
		bool          iconMimicType{ true };  // iconFrom* also copies the item's form type and keywords onto the row
		std::uint32_t reloadKey{ 0 };        // DirectInput scan code, 0 = off
		std::uint32_t dumpKey{ 0 };          // DirectInput scan code, 0 = off
	};

	[[nodiscard]] Config& GetConfig();
	void                  LoadConfig();

	void InitLog();

	[[nodiscard]] Engine& GetEngine();
	void                  ReloadRules();

	// Adapter: game record -> plain facts. Returns false when the effect is unusable.
	bool BuildFacts(RE::ActiveEffect* a_effect, Facts& a_facts);
	FormKey MakeFormKey(const RE::TESForm* a_form);

	// Diagnostics (milestone 2)
	void DumpActiveEffects(const char* a_reason);
	void DumpMenuStructure();

	// Queues a pass over the open Magic menu's Active Effects rows (retries while the list is still empty).
	void ScheduleApply(int a_retries);
	// Inventory menu: learn the icon label SkyUI / Inventory Injector gave each armor and weapon, for iconFromWorn.
	void ScheduleLearnLabels(int a_retries);
	void LoadLabelCache();
	void DumpValue(const char* a_label, const RE::GFxValue& a_value, int a_maxDepth, int a_maxLines);
	void RequestRowReport();  // ask for one detailed per-row report on the next apply pass

	void InstallSinks();
}
