#include "Plugin.h"

#include <sstream>

namespace daedra::plugin
{
	namespace
	{
		std::string Join(const std::vector<std::string>& a_list)
		{
			std::string out;
			for (const auto& s : a_list) {
				out += out.empty() ? s : ", " + s;
			}
			return out;
		}

		std::string Describe(const Assign& a)
		{
			std::ostringstream os;
			auto               put = [&](const char* k, const std::optional<std::string>& v) {
                if (v) {
                    os << k << "=\"" << *v << "\" ";
                }
			};
			put("iconLabel", a.iconLabel);
			put("iconSource", a.iconSource);
			put("sourceText", a.sourceText);
			put("nameText", a.nameText);
			put("extraText", a.extraText);
			put("sortGroup", a.sortGroup);
			put("color", a.color);
			if (a.hide) {
				os << "hide=" << (*a.hide ? "true" : "false");
			}
			return os.str();
		}

		const char* TypeName(RE::GFxValue::ValueType a_type)
		{
			using T = RE::GFxValue::ValueType;
			switch (static_cast<T>(std::to_underlying(a_type) & 0xF)) {
			case T::kUndefined:
				return "undefined";
			case T::kNull:
				return "null";
			case T::kBoolean:
				return "bool";
			case T::kNumber:
				return "number";
			case T::kString:
			case T::kStringW:
				return "string";
			case T::kObject:
				return "object";
			case T::kArray:
				return "array";
			case T::kDisplayObject:
				return "displayObject";
			default:
				return "?";
			}
		}

		struct Walker
		{
			int lines{ 0 };
			int maxLines{ 700 };
			int maxDepth{ 5 };
			static constexpr int kMaxLines = 1800;

			// TextField properties: pure noise when looking for list data
			static bool Noise(const std::string& k)
			{
				static const char* const kSkip[] = { "scroll", "bottomScroll", "tabIndex", "html", "minFontSize", "maxChars", "antiAliasType", "textWidth", "type", "enableShrinkToFit", "htmlText", "textColor", "maxHeightExpand", "wordWrap", "autoSize", "condenseWhite", "restrict", "length", "maxscroll", "styleSheet", "multiline", "variable", "hscroll", "selectable", "textHeight", "border", "background", "embedFonts", "mouseWheelEnabled", "borderColor", "maxhscroll", "backgroundColor", "overflowMode", "password", "PlayReverse", "PlayForward", "Lock", "SetText" };
				for (const char* s : kSkip) {
					if (k == s) {
						return true;
					}
				}
				return false;
			}

			void Put(int a_depth, const std::string& a_name, const RE::GFxValue& a_val)
			{
				if (lines >= maxLines || Noise(a_name)) {
					return;
				}
				++lines;
				std::string text(static_cast<std::size_t>(a_depth) * 2, ' ');
				text += a_name + " : " + TypeName(a_val.GetType());
				if (a_val.IsBool()) {
					text += a_val.GetBool() ? " = true" : " = false";
				} else if (a_val.IsNumber()) {
					text += " = " + std::to_string(a_val.GetNumber());
				} else if (a_val.IsString()) {
					std::string s = a_val.GetString() ? a_val.GetString() : "";
					if (s.size() > 80) {
						s = s.substr(0, 80) + "...";
					}
					text += " = \"" + s + "\"";
				} else if (a_val.IsArray()) {
					text += " [" + std::to_string(a_val.GetArraySize()) + "]";
				}
				SKSE::log::info("    {}", text);

				if (a_depth >= maxDepth) {
					return;
				}
				if (a_val.IsArray()) {
					const auto n = std::min<std::uint32_t>(a_val.GetArraySize(), 4);  // first rows are enough to learn the shape
					for (std::uint32_t i = 0; i < n; ++i) {
						RE::GFxValue el;
						if (a_val.GetElement(i, &el)) {
							Put(a_depth + 1, "[" + std::to_string(i) + "]", el);
						}
					}
				} else if (a_val.IsObject() || a_val.IsDisplayObject()) {
					a_val.VisitMembers([&](const char* a_key, const RE::GFxValue& a_member) {
						if (a_key) {
							Put(a_depth + 1, a_key, a_member);
						}
					});
				}
			}
		};
	}

	void DumpActiveEffects(const char* a_reason)
	{
		try {
			const auto player = RE::PlayerCharacter::GetSingleton();
			const auto target = player ? player->AsMagicTarget() : nullptr;
			const auto list = target ? target->GetActiveEffectList() : nullptr;
			if (!list) {
				SKSE::log::warn("ActiveEffects dump ({}): player has no active effect list", a_reason);
				return;
			}

			SKSE::log::info("==== Active effects dump ({}) ====", a_reason);
			int index = 0;
			for (auto* ae : *list) {
				if (!ae) {
					continue;
				}
				Facts facts;
				if (!BuildFacts(ae, facts)) {
					SKSE::log::warn("  [{}] could not read effect", index++);
					continue;
				}
				const auto mgef = ae->effect ? ae->effect->baseEffect : ae->GetBaseObject();
				SKSE::log::info("  [{}] effect \"{}\" {} edid=\"{}\" archetype={} av={} detrimental={}",
					index, mgef && mgef->GetName() ? mgef->GetName() : "?", ToString(facts.effect.form),
					facts.effect.edid, facts.effect.archetype, facts.effect.actorValue, facts.effect.detrimental);
				SKSE::log::info("        keywords: [{}]", Join(facts.effect.keywords));
				SKSE::log::info("        magnitude={:.2f} duration={:.1f} elapsed={:.1f} castingSource={} flags=0x{:X}",
					ae->magnitude, ae->duration, ae->elapsedSeconds, static_cast<int>(ae->castingSource), ae->flags.underlying());
				SKSE::log::info("        source: kind={} spellType={} name=\"{}\" {} edid=\"{}\"",
					ToString(facts.source.kind), facts.source.spellType, facts.source.name,
					ToString(facts.source.form), facts.source.edid);
				SKSE::log::info("        source keywords: [{}]", Join(facts.source.keywords));
				SKSE::log::info("        caster: {} {}", ToString(facts.caster.kind), ToString(facts.caster.form));
				if (facts.worn.present) {
					SKSE::log::info("        item (ActiveEffect::source): \"{}\" {} keywords: [{}]",
						ae->source && ae->source->GetName() ? ae->source->GetName() : "", ToString(facts.worn.form), Join(facts.worn.keywords));
				}
				if (ae->displacementSpell) {
					SKSE::log::info("        displacementSpell: \"{}\" {:08X}", ae->displacementSpell->GetName(), ae->displacementSpell->GetFormID());
				}

				if (GetEngine().RuleCount() > 0) {
					const auto assign = GetEngine().Resolve(facts);
					if (assign) {
						const auto* rule = GetEngine().LastMatchedRule();
						SKSE::log::info("        rule: {} ({}) -> {}", rule ? rule->id : "?", rule ? rule->file : "?", Describe(*assign));
					} else {
						SKSE::log::info("        rule: none");
					}
				}
				++index;
			}
			SKSE::log::info("==== end dump ({} effects) ====", index);
		} catch (const std::exception& e) {
			SKSE::log::error("ActiveEffects dump failed: {}", e.what());
		} catch (...) {
			SKSE::log::error("ActiveEffects dump failed");
		}
	}

	// Milestone 0 tool: logs the Scaleform tree of the open Magic menu so we can see where
	// the Active Effects rows live and which fields each row carries.
	void DumpMenuStructure()
	{
		try {
			const auto ui = RE::UI::GetSingleton();
			if (!ui || !ui->IsMenuOpen(RE::MagicMenu::MENU_NAME)) {
				SKSE::log::info("Menu structure dump skipped: Magic menu is not open");
				return;
			}
			const auto menu = ui->GetMenu(RE::MagicMenu::MENU_NAME);
			if (!menu || !menu->uiMovie) {
				SKSE::log::warn("Menu structure dump: no movie");
				return;
			}
			RE::GFxValue root;
			if (!menu->uiMovie->GetVariable(&root, "_root")) {
				SKSE::log::warn("Menu structure dump: _root not found");
				return;
			}
			// 1. The list that holds the rows (SkyUI ItemList), deep, with row data.
			for (const char* path : { "_root.Menu_mc.inventoryLists.itemList._entryList", "_root.Menu_mc.inventoryLists.itemList.entryList", "_root.Menu_mc.inventoryLists.itemList" }) {
				RE::GFxValue node;
				if (menu->uiMovie->GetVariable(&node, path)) {
					SKSE::log::info("==== {} ====", path);
					Walker w;
					w.maxLines = 500;
					w.maxDepth = 6;
					w.Put(0, path, node);
				} else {
					SKSE::log::info("==== {} not found ====", path);
				}
			}
			// 2. Everything else, shallow, for orientation.
			SKSE::log::info("==== MagicMenu Scaleform structure (_root, depth<=3, text-field noise removed) ====");
			Walker walker;
			walker.maxLines = Walker::kMaxLines;
			walker.maxDepth = 3;
			walker.Put(0, "_root", root);
			SKSE::log::info("==== end structure ({} lines{}) ====", walker.lines, walker.lines >= walker.maxLines ? ", truncated" : "");
		} catch (const std::exception& e) {
			SKSE::log::error("Menu structure dump failed: {}", e.what());
		} catch (...) {
			SKSE::log::error("Menu structure dump failed");
		}
	}
}

namespace daedra::plugin
{
	void DumpValue(const char* a_label, const RE::GFxValue& a_value, int a_maxDepth, int a_maxLines)
	{
		Walker w;
		w.maxLines = a_maxLines;
		w.maxDepth = a_maxDepth;
		w.Put(0, a_label, a_value);
	}
}
