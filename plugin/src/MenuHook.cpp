#include "Plugin.h"

namespace daedra::plugin
{
	namespace
	{
		class MenuSink final : public RE::BSTEventSink<RE::MenuOpenCloseEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::MenuOpenCloseEvent* a_event, RE::BSTEventSource<RE::MenuOpenCloseEvent>*) override
			{
				try {
					if (a_event && a_event->opening && a_event->menuName == RE::MagicMenu::MENU_NAME && GetConfig().dumpActiveEffects) {
						DumpActiveEffects("Magic menu opened");
					}
					if (a_event && a_event->opening && a_event->menuName == RE::MagicMenu::MENU_NAME) {
						RequestRowReport();
						ScheduleApply(240);
					}
					if (a_event && a_event->opening && a_event->menuName == RE::InventoryMenu::MENU_NAME) {
						SKSE::log::info("Inventory menu opened: reading item icon labels");
						ScheduleLearnLabels(240);
					}
				} catch (...) {
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		class InputSink final : public RE::BSTEventSink<RE::InputEvent*>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(RE::InputEvent* const* a_event, RE::BSTEventSource<RE::InputEvent*>*) override
			{
				try {
					const auto& cfg = GetConfig();
					if (!a_event || (!cfg.reloadKey && !cfg.dumpKey)) {
						return RE::BSEventNotifyControl::kContinue;
					}
					if (cfg.applyToMenu && RE::UI::GetSingleton() && RE::UI::GetSingleton()->IsMenuOpen(RE::MagicMenu::MENU_NAME)) {
						ScheduleApply(2);  // tab switches and filters rebuild the list after input
					}
					if (cfg.applyToMenu && RE::UI::GetSingleton() && RE::UI::GetSingleton()->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
						ScheduleLearnLabels(2);  // rows get their labels as tabs are viewed
					}
					for (auto* e = *a_event; e; e = e->next) {
						const auto button = e->AsButtonEvent();
						if (!button || !button->IsDown() || button->GetDevice() != RE::INPUT_DEVICE::kKeyboard) {
							continue;
						}
						const auto code = button->GetIDCode();
						if (cfg.reloadKey && code == cfg.reloadKey) {
							ReloadRules();
							RE::SendHUDMessage::ShowHUDMessage(std::format("DAEDRA r{}: rules reloaded ({} loaded)", DAEDRA_REVISION, GetEngine().RuleCount()).c_str());
						} else if (cfg.dumpKey && code == cfg.dumpKey) {
							DumpActiveEffects("hotkey");
							DumpMenuStructure();
							RE::SendHUDMessage::ShowHUDMessage(std::format("DAEDRA r{}: dump written to DAEDRA.log", DAEDRA_REVISION).c_str());
						}
					}
				} catch (...) {
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};

		MenuSink  g_menuSink;
		InputSink g_inputSink;
	}

	void InstallSinks()
	{
		if (const auto ui = RE::UI::GetSingleton()) {
			ui->AddEventSink<RE::MenuOpenCloseEvent>(&g_menuSink);
		} else {
			SKSE::log::warn("UI singleton unavailable, Magic menu events are disabled");
		}
		if (const auto input = RE::BSInputDeviceManager::GetSingleton()) {
			input->AddEventSink(&g_inputSink);
		} else {
			SKSE::log::warn("Input manager unavailable, hotkeys are disabled");
		}
		if (GetConfig().applyToMenu) {
			SKSE::log::info("applyToMenu is on: Active Effects rows will be relabelled by matching rules");
		}
	}
}
