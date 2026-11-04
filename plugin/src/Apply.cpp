#include "Plugin.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <functional>
#include <nlohmann/json.hpp>
#include <unordered_map>

namespace daedra::plugin
{
	namespace
	{
		constexpr const char* kListPath = "_root.Menu_mc.inventoryLists.itemList";
		constexpr double      kActiveEffectFormType = 18;  // Active Effects rows are magic-effect entries; spell rows are 22

		std::atomic_bool g_pending{ false };
		std::atomic_bool g_logRows{ false };  // one detailed per-row report after each menu open
		std::atomic<int> g_settle{ 0 };       // extra spaced-out passes still owed after the menu opened

		void Later(int a_frames, std::function<void()> a_fn)
		{
			if (const auto tasks = SKSE::GetTaskInterface()) {
				tasks->AddUITask([a_frames, a_fn]() {
					if (a_frames <= 0) {
						a_fn();
					} else {
						Later(a_frames - 1, a_fn);
					}
				});
			}
		}

		// Item icon labels learned from the Inventory menu, keyed by "plugin|0xLOCALID" (stable across load orders).
		std::unordered_map<std::string, std::string> g_itemLabels;
		std::atomic_bool                              g_learnPending{ false };
		int                                           g_inventoryReports = 0;  // cap the diagnostic lines

		std::filesystem::path LabelCachePath()
		{
			return "Data/SKSE/Plugins/DAEDRA/cache/item-icon-labels.json";
		}

		void SaveLabelCache()
		{
			try {
				nlohmann::json doc;
				for (const auto& [k, v] : g_itemLabels) {
					doc["labels"][k] = v;
				}
				std::error_code ec;
				std::filesystem::create_directories(LabelCachePath().parent_path(), ec);
				std::ofstream(LabelCachePath()) << doc.dump(1);
			} catch (...) {
			}
		}

		// Reads the open Inventory menu's rows and remembers the icon label of every armor and weapon.
		int LearnLabels()
		{
			const auto ui = RE::UI::GetSingleton();
			if (!ui || !ui->IsMenuOpen(RE::InventoryMenu::MENU_NAME)) {
				return -1;
			}
			const auto menu = ui->GetMenu(RE::InventoryMenu::MENU_NAME);
			if (!menu || !menu->uiMovie) {
				return -1;
			}
			RE::GFxValue list, rows;
			if (!menu->uiMovie->GetVariable(&list, kListPath) || !list.GetMember("_entryList", &rows) || !rows.IsArray() || rows.GetArraySize() == 0) {
				return -1;
			}
			int learned = 0;
			int withLabel = 0;
			int armorRows = 0;
			for (std::uint32_t i = 0; i < rows.GetArraySize(); ++i) {
				RE::GFxValue row, typeVal, idVal, labelVal;
				{
					RE::GFxValue probeRow, probeLabel;
					if (rows.GetElement(i, &probeRow) && probeRow.IsObject() && probeRow.GetMember("iconLabel", &probeLabel) && probeLabel.IsString()) {
						++withLabel;
					}
				}
				if (!rows.GetElement(i, &row) || !row.IsObject() || !row.GetMember("formType", &typeVal) || !typeVal.IsNumber() ||
					!row.GetMember("formId", &idVal) || !idVal.IsNumber() || !row.GetMember("iconLabel", &labelVal) || !labelVal.IsString()) {
					continue;
				}
				const auto type = static_cast<std::uint32_t>(typeVal.GetNumber());
				if (type != static_cast<std::uint32_t>(RE::FormType::Armor) && type != static_cast<std::uint32_t>(RE::FormType::Weapon)) {
					continue;
				}
				++armorRows;
				const auto form = RE::TESForm::LookupByID(static_cast<std::uint32_t>(idVal.GetNumber()));
				const auto key = ToString(MakeFormKey(form));
				const std::string label = labelVal.GetString();
				if (!form || label.empty() || key == "<none>") {
					continue;
				}
				auto [it, inserted] = g_itemLabels.try_emplace(key, label);
				if (inserted || it->second != label) {
					it->second = label;
					++learned;
					SKSE::log::info("learned icon label: \"{}\" {} -> \"{}\"", form->GetName() ? form->GetName() : "", key, label);
				}
			}
			if (learned > 0 || g_inventoryReports < 6) {
				++g_inventoryReports;
				SKSE::log::info("inventory: {} rows, {} with an iconLabel, {} armor or weapon rows, {} labels learned", rows.GetArraySize(), withLabel, armorRows, learned);
			}
			if (learned > 0) {
				SaveLabelCache();
			}
			return learned;
		}

		using EffectsByMgef = std::unordered_multimap<std::uint32_t, RE::ActiveEffect*>;

		// An Active Effects row identifies its effect by form id and its source only by the display
		// name in `cardName` (spell / enchantment name, or the worn item's name with a suffix such as
		// "(Legendary)"). Pair the row with an active effect whose names are consistent with that.
		RE::ActiveEffect* FindEffect(const EffectsByMgef& a_byMgef, std::uint32_t a_mgefId, const std::string& a_cardName)
		{
			const auto [first, last] = a_byMgef.equal_range(a_mgefId);
			if (first == last) {
				return nullptr;
			}
			if (std::distance(first, last) == 1) {
				return first->second;
			}
			auto consistent = [&](const char* a_name) {
				return a_name && *a_name && !a_cardName.empty() &&
				       (EqualsNoCase(a_name, a_cardName) || ContainsNoCase(a_cardName, a_name) || ContainsNoCase(a_name, a_cardName));
			};
			for (auto it = first; it != last; ++it) {
				const auto ae = it->second;
				if ((ae->spell && consistent(ae->spell->GetName())) || (ae->source && consistent(ae->source->GetName()))) {
					return ae;
				}
			}
			return nullptr;
		}

		// Returns how many fields changed, or -1 when the list is not ready / not showing Active Effects.
		int ApplyRows()
		{
			const auto ui = RE::UI::GetSingleton();
			if (!ui || !ui->IsMenuOpen(RE::MagicMenu::MENU_NAME)) {
				return -1;
			}
			const auto menu = ui->GetMenu(RE::MagicMenu::MENU_NAME);
			if (!menu || !menu->uiMovie) {
				return -1;
			}
			auto& movie = *menu->uiMovie;

			// Active Effects is the last category tab; other tabs share the same list data.
			{
				RE::GFxValue lists, cat, art;
				if (!movie.GetVariable(&lists, "_root.Menu_mc.inventoryLists") || !lists.GetMember("_currCategoryIndex", &cat) || !cat.IsNumber() ||
					!movie.GetVariable(&art, "_root.Menu_mc._categoryListIconArt") || !art.IsArray() || art.GetArraySize() == 0) {
					return -1;
				}
				if (static_cast<std::uint32_t>(cat.GetNumber()) != art.GetArraySize() - 1) {
					return 0;
				}
			}

			RE::GFxValue list;
			if (!movie.GetVariable(&list, kListPath)) {
				return -1;
			}
			RE::GFxValue rows;
			if (!list.GetMember("_entryList", &rows) || !rows.IsArray()) {
				return -1;
			}
			const auto count = rows.GetArraySize();
			if (count == 0) {
				return -1;
			}

			const auto player = RE::PlayerCharacter::GetSingleton();
			const auto target = player ? player->AsMagicTarget() : nullptr;
			const auto effects = target ? target->GetActiveEffectList() : nullptr;
			if (!effects) {
				return -1;
			}
			EffectsByMgef byMgef;
			for (auto* ae : *effects) {
				if (!ae) {
					continue;
				}
				if (const auto mgef = ae->effect ? ae->effect->baseEffect : ae->GetBaseObject()) {
					byMgef.emplace(mgef->GetFormID(), ae);
				}
			}

			const bool report = g_logRows.load();
			int        changed = 0;
			int        seen = 0;
			int        matched = 0;
			for (std::uint32_t i = 0; i < count; ++i) {
				RE::GFxValue row;
				if (!rows.GetElement(i, &row) || !row.IsObject()) {
					continue;
				}
				RE::GFxValue typeVal, idVal, cardVal, nameVal, origTypeVal;
				if (!row.GetMember("formType", &typeVal) || !typeVal.IsNumber() ||
					(row.GetMember("daedraFormType", &origTypeVal) && origTypeVal.IsNumber() ? origTypeVal.GetNumber() : typeVal.GetNumber()) != kActiveEffectFormType ||
					!row.GetMember("formId", &idVal) || !idVal.IsNumber() ||
					!row.GetMember("effectName", &nameVal) || !nameVal.IsString()) {
					continue;
				}
				row.GetMember("cardName", &cardVal);
				const std::string cardName = cardVal.IsString() ? cardVal.GetString() : "";
				const std::string effectName = nameVal.GetString();
				// After iconFromWorn the row's formId is the item's; the effect id is kept in daedraFormId.
				RE::GFxValue origVal;
				const bool   hasOrig = row.GetMember("daedraFormId", &origVal) && origVal.IsNumber();
				const auto   mgefId = static_cast<std::uint32_t>(hasOrig ? origVal.GetNumber() : idVal.GetNumber());
				++seen;

				const auto ae = FindEffect(byMgef, mgefId, cardName);
				if (report) {
					SKSE::log::info("  row {} effect=\"{}\" cardName=\"{}\" formId={:08X} -> {}", i, effectName, cardName, mgefId,
						ae ? "paired with an active effect" : "no active effect paired");
				}
				if (!ae) {
					continue;
				}
				++matched;
				Facts facts;
				if (!BuildFacts(ae, facts)) {
					continue;
				}
				const auto assign = GetEngine().Resolve(facts);
				if (!assign) {
					continue;
				}

				// Sets a string member only when it differs, so repeated passes are cheap and idempotent.
				auto setString = [&](const char* a_member, const std::string& a_value) {
					RE::GFxValue cur;
					if (row.GetMember(a_member, &cur) && cur.IsString() && a_value == cur.GetString()) {
						return;
					}
					RE::GFxValue value;
					movie.CreateString(&value, a_value.c_str());
					if (row.SetMember(a_member, value)) {
						++changed;
						if (GetConfig().logMatches) {
							const auto* rule = GetEngine().LastMatchedRule();
							SKSE::log::info("apply: row {} \"{}\" [{}] {} -> \"{}\" (rule {})", i, effectName, cardName, a_member, a_value, rule ? rule->id : "?");
						}
					}
				};
				if (assign->iconLabel) {
					setString("iconLabel", *assign->iconLabel);
				}
				if (assign->iconSource) {
					setString("iconSource", *assign->iconSource);
				}
				if (assign->sourceText) {  // Source column
					setString("cardName", *assign->sourceText);
				}
				// Icons come from PNGII, which picks them from the row's form id (and type / keywords). To get the
				// icon of another form, make the row impersonate that form. Originals are kept in daedra* members.
				{
					const RE::TESForm* target = nullptr;
					const char*        via = "";
					if (assign->iconAsForm) {
						const auto dataHandler = RE::TESDataHandler::GetSingleton();
						const auto fid = dataHandler ? dataHandler->LookupFormID(assign->iconAsForm->id, assign->iconAsForm->plugin) : 0;
						target = fid ? RE::TESForm::LookupByID(fid) : nullptr;
						via = "iconAsForm";
					} else if (assign->iconFromWorn.value_or(false) && ae->source) {
						target = ae->source;
						via = "worn item";
					} else if (assign->iconFromSource.value_or(false) && ae->spell) {
						target = ae->spell;
						via = "source";
					}
					if (target) {
						const double targetId = target->GetFormID();
						const double targetType = static_cast<double>(std::to_underlying(target->GetFormType()));
						const bool   mimic = GetConfig().iconMimicType;
						RE::GFxValue curId, curType;
						row.GetMember("formId", &curId);
						row.GetMember("formType", &curType);
						const bool idDone = curId.IsNumber() && curId.GetNumber() == targetId;
						const bool typeDone = !mimic || (curType.IsNumber() && curType.GetNumber() == targetType);
						if (!(idDone && typeDone)) {
							RE::GFxValue probe;
							if (!row.GetMember("daedraFormId", &probe)) {
								row.SetMember("daedraFormId", idVal);
								row.SetMember("daedraFormType", typeVal);
							}
							bool ok = row.SetMember("formId", RE::GFxValue(targetId));
							if (mimic) {
								ok = row.SetMember("formType", RE::GFxValue(targetType)) && ok;
								RE::GFxValue kws;
								movie.CreateObject(&kws);
								if (const auto kwForm = target->As<RE::BGSKeywordForm>()) {
									kwForm->ForEachKeyword([&](RE::BGSKeyword* a_kw) {
										if (a_kw && a_kw->GetFormEditorID() && *a_kw->GetFormEditorID()) {
											kws.SetMember(a_kw->GetFormEditorID(), RE::GFxValue(true));
										}
										return RE::BSContainer::ForEachResult::kContinue;
									});
								}
								row.SetMember("keywords", kws);
							}
							if (ok) {
								++changed;
								if (GetConfig().logMatches) {
									const auto* rule = GetEngine().LastMatchedRule();
									SKSE::log::info("apply: row {} \"{}\" [{}] now shows the icon of {} \"{}\" {:08X} (rule {})", i, effectName, cardName, via,
										target->GetName() ? target->GetName() : "", target->GetFormID(), rule ? rule->id : "?");
								}
							}
						}
						// The item's own label (learned from the Inventory menu) gives armor its generic icon; an explicit rule label wins.
						if (!assign->iconLabel) {
							if (const auto learnt = g_itemLabels.find(ToString(MakeFormKey(target))); learnt != g_itemLabels.end()) {
								setString("iconLabel", learnt->second);
							}
						}
					}
				}
				if (assign->nameText) {  // Name column
					setString("effectName", *assign->nameText);
					setString("text", *assign->nameText);
				}
			}

			if (report && seen > 0) {
				g_logRows = false;
				SKSE::log::info("apply: {} Active Effects rows, {} paired with an effect, {} fields changed", seen, matched, changed);
			}
			if (changed > 0) {
				// A tab switch makes SkyUI rebuild the list and PNGII redraw the icons; do the same refresh here.
				RE::GFxValue lists;
				if (movie.GetVariable(&lists, "_root.Menu_mc.inventoryLists") && lists.HasMember("InvalidateListData")) {
					lists.Invoke("InvalidateListData");
				} else {
					list.Invoke("UpdateList");
				}
			}
			return changed;
		}
	}

	void RequestRowReport()
	{
		g_settle = 3;
		g_logRows = true;
	}

	void ScheduleApply(int a_retries)
	{
		if (!GetConfig().applyToMenu || GetEngine().RuleCount() == 0) {
			return;
		}
		if (g_pending.exchange(true)) {
			return;  // one queued task is enough
		}
		const auto tasks = SKSE::GetTaskInterface();
		if (!tasks) {
			g_pending = false;
			return;
		}
		tasks->AddUITask([a_retries]() {
			g_pending = false;
			try {
				const auto result = ApplyRows();
				// PNGII and SkyUI keep rebuilding rows after the menu opens, so repeat a few times, spaced out, until it settles.
				if (result > 0 && g_settle > 0) {
					--g_settle;
					Later(20, [] { ScheduleApply(2); });
				}
				// List not built yet (menu just opened): look again next frame.
				if (result < 0 && a_retries > 0) {
					ScheduleApply(a_retries - 1);
				}
			} catch (const std::exception& e) {
				SKSE::log::error("apply failed: {}", e.what());
			} catch (...) {
				SKSE::log::error("apply failed");
			}
		});
	}
}

namespace daedra::plugin
{
	void LoadLabelCache()
	{
		try {
			std::ifstream in(LabelCachePath());
			if (!in) {
				return;
			}
			const auto doc = nlohmann::json::parse(in, nullptr, false);
			if (doc.is_object() && doc.contains("labels") && doc["labels"].is_object()) {
				for (auto it = doc["labels"].begin(); it != doc["labels"].end(); ++it) {
					if (it.value().is_string()) {
						g_itemLabels[it.key()] = it.value().get<std::string>();
					}
				}
			}
			SKSE::log::info("Loaded {} cached item icon labels", g_itemLabels.size());
		} catch (...) {
		}
	}

	void ScheduleLearnLabels(int a_retries)
	{
		if (!GetConfig().applyToMenu || g_learnPending.exchange(true)) {
			return;
		}
		const auto tasks = SKSE::GetTaskInterface();
		if (!tasks) {
			g_learnPending = false;
			return;
		}
		tasks->AddUITask([a_retries]() {
			g_learnPending = false;
			try {
				const auto result = LearnLabels();
				if (result < 0 && a_retries > 0) {
					ScheduleLearnLabels(a_retries - 1);
				}
			} catch (...) {
			}
		});
	}
}
