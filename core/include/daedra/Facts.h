#pragma once
// Plain-data description of one active effect row. The game layer fills these in;
// the rule engine never sees CommonLib types.

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace daedra
{
	// A form identified by the plugin that first defines it plus its local id
	// (light-plugin aware: the adapter supplies the local id, not the load-order id).
	// Plugin names are stored lower-case.
	struct FormKey
	{
		std::string   plugin;
		std::uint32_t id{ 0 };

		[[nodiscard]] bool Empty() const noexcept { return plugin.empty(); }
		bool               operator==(const FormKey&) const = default;
	};

	enum class SourceKind
	{
		Unknown,
		Spell,
		Enchantment,
		Potion,
		Ingredient,
		Ability,
		Power,
		LesserPower,
		Disease,
		Scroll,
		Staff,
	};

	enum class CasterKind
	{
		Unknown,
		Player,
		Follower,
		Other,
	};

	struct EffectFacts
	{
		FormKey                  form;
		std::string              edid;
		std::vector<std::string> keywords;  // keyword editor ids
		std::string              archetype;
		std::string              actorValue;
		bool                     detrimental{ false };
	};

	struct SourceFacts
	{
		SourceKind               kind{ SourceKind::Unknown };
		FormKey                  form;
		std::string              edid;
		std::string              name;
		std::vector<std::string> keywords;
		std::string              spellType;
	};

	struct CasterFacts
	{
		CasterKind kind{ CasterKind::Unknown };
		FormKey    form;
	};

	struct WornFacts
	{
		bool                     present{ false };
		FormKey                  form;
		std::vector<std::string> keywords;
	};

	struct Facts
	{
		EffectFacts effect;
		SourceFacts source;
		CasterFacts caster;
		WornFacts   worn;
	};

	[[nodiscard]] const char* ToString(SourceKind a_kind) noexcept;
	[[nodiscard]] const char* ToString(CasterKind a_kind) noexcept;
	[[nodiscard]] std::string ToString(const FormKey& a_key);

	// Case-insensitive ASCII helpers shared by parser and matcher.
	[[nodiscard]] std::string ToLower(std::string_view a_text);
	[[nodiscard]] bool        EqualsNoCase(std::string_view a_lhs, std::string_view a_rhs) noexcept;
	[[nodiscard]] bool        ContainsNoCase(std::string_view a_haystack, std::string_view a_needle);
}
