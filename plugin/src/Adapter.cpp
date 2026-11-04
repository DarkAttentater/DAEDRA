#include "Plugin.h"

namespace daedra::plugin
{
	namespace
	{
		FormKey KeyOf(const RE::TESForm* a_form)
		{
			if (!a_form) {
				return {};
			}
			const auto file = a_form->GetFile(0);  // the plugin that first defines the record
			if (!file) {
				return {};  // runtime-created form
			}
			const auto mask = file->IsLight() ? 0xFFFu : 0xFFFFFFu;
			return { ToLower(file->GetFilename()), a_form->GetFormID() & mask };
		}

		void Keywords(const RE::TESForm* a_form, std::vector<std::string>& a_out)
		{
			const auto kwForm = a_form ? a_form->As<RE::BGSKeywordForm>() : nullptr;
			if (!kwForm) {
				return;
			}
			kwForm->ForEachKeyword([&](RE::BGSKeyword* a_kw) {
				if (a_kw) {
					if (const char* edid = a_kw->GetFormEditorID(); edid && *edid) {
						a_out.emplace_back(edid);
					}
				}
				return RE::BSContainer::ForEachResult::kContinue;
			});
		}

		const char* SpellTypeName(RE::MagicSystem::SpellType a_type)
		{
			using T = RE::MagicSystem::SpellType;
			switch (a_type) {
			case T::kSpell:
				return "spell";
			case T::kDisease:
				return "disease";
			case T::kPower:
				return "power";
			case T::kLesserPower:
				return "lesserPower";
			case T::kAbility:
				return "ability";
			case T::kPoison:
				return "poison";
			case T::kEnchantment:
				return "enchantment";
			case T::kPotion:
				return "potion";
			case T::kWortCraft:
				return "ingredient";
			case T::kLeveledSpell:
				return "leveledSpell";
			case T::kAddiction:
				return "addiction";
			case T::kVoicePower:
				return "voicePower";
			case T::kStaffEnchantment:
				return "staffEnchantment";
			case T::kScroll:
				return "scroll";
			default:
				return "unknown";
			}
		}

		SourceKind KindOf(const RE::MagicItem* a_item)
		{
			using T = RE::MagicSystem::SpellType;
			switch (a_item->GetFormType()) {
			case RE::FormType::Enchantment:
				return SourceKind::Enchantment;
			case RE::FormType::AlchemyItem:
				return SourceKind::Potion;
			case RE::FormType::Ingredient:
				return SourceKind::Ingredient;
			case RE::FormType::Scroll:
				return SourceKind::Scroll;
			case RE::FormType::Spell:
				break;
			default:
				return SourceKind::Unknown;
			}
			switch (a_item->GetSpellType()) {
			case T::kSpell:
			case T::kLeveledSpell:
				return SourceKind::Spell;
			case T::kDisease:
				return SourceKind::Disease;
			case T::kPower:
			case T::kVoicePower:
				return SourceKind::Power;
			case T::kLesserPower:
				return SourceKind::LesserPower;
			case T::kAbility:
				return SourceKind::Ability;
			case T::kEnchantment:
				return SourceKind::Enchantment;
			case T::kStaffEnchantment:
				return SourceKind::Staff;
			case T::kScroll:
				return SourceKind::Scroll;
			default:
				return SourceKind::Spell;
			}
		}

		const char* ArchetypeName(std::uint32_t a_value)
		{
			static constexpr const char* kNames[] = {
				"ValueModifier", "Script", "Dispel", "CureDisease", "Absorb", "DualValueModifier", "Calm",
				"Demoralize", "Frenzy", "Disarm", "CommandSummoned", "Invisibility", "Light", "Darkness",
				"NightEye", "Lock", "Open", "BoundWeapon", "SummonCreature", "DetectLife", "Telekinesis",
				"Paralysis", "Reanimate", "SoulTrap", "TurnUndead", "Guide", "WerewolfFeed", "CureParalysis",
				"CureAddiction", "CurePoison", "Concussion", "ValueAndParts", "AccumulateMagnitude", "Stagger",
				"PeakValueModifier", "Cloak", "Werewolf", "SlowTime", "Rally", "EnhanceWeapon", "SpawnHazard",
				"Etherealize", "Banish", "SpawnScriptedRef", "Disguise", "GrabActor", "VampireLord"
			};
			return a_value < std::size(kNames) ? kNames[a_value] : "Unknown";
		}
	}

	bool BuildFacts(RE::ActiveEffect* a_effect, Facts& a_facts)
	{
		if (!a_effect) {
			return false;
		}
		try {
			a_facts = Facts{};

			const auto mgef = a_effect->effect ? a_effect->effect->baseEffect : a_effect->GetBaseObject();
			if (mgef) {
				auto& e = a_facts.effect;
				e.form = KeyOf(mgef);
				if (const char* edid = mgef->GetFormEditorID(); edid) {
					e.edid = edid;
				}
				Keywords(mgef, e.keywords);
				e.archetype = ArchetypeName(static_cast<std::uint32_t>(mgef->data.archetype));
				if (const auto avName = RE::ActorValueList::GetActorValueName(mgef->data.primaryAV)) {
					e.actorValue = avName;
				}
				e.detrimental = mgef->IsDetrimental();
			}

			if (const auto spell = a_effect->spell) {
				auto& s = a_facts.source;
				s.kind = KindOf(spell);
				s.form = KeyOf(spell);
				if (const char* edid = spell->GetFormEditorID(); edid) {
					s.edid = edid;
				}
				if (const char* name = spell->GetName(); name) {
					s.name = name;
				}
				Keywords(spell, s.keywords);
				if (spell->GetFormType() == RE::FormType::Spell) {
					s.spellType = SpellTypeName(spell->GetSpellType());
				}
			}

			// Caster
			if (const auto caster = a_effect->GetCasterActor(); caster) {
				auto& c = a_facts.caster;
				c.form = KeyOf(caster->GetBaseObject());
				if (caster->IsPlayerRef()) {
					c.kind = CasterKind::Player;
				} else if (caster->IsPlayerTeammate()) {
					c.kind = CasterKind::Follower;
				} else {
					c.kind = CasterKind::Other;
				}
			}

			// Hypothesis, unverified in game: for enchantments `source` is the worn item.
			if (const auto item = a_effect->source; item) {
				a_facts.worn.present = true;
				a_facts.worn.form = KeyOf(item);
				Keywords(item, a_facts.worn.keywords);
			}
			return true;
		} catch (...) {
			return false;
		}
	}
}

namespace daedra::plugin
{
	FormKey MakeFormKey(const RE::TESForm* a_form)
	{
		return KeyOf(a_form);
	}
}
