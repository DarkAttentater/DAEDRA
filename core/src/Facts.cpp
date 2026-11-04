#include "daedra/Facts.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace daedra
{
	const char* ToString(SourceKind a_kind) noexcept
	{
		switch (a_kind) {
		case SourceKind::Spell:
			return "spell";
		case SourceKind::Enchantment:
			return "enchantment";
		case SourceKind::Potion:
			return "potion";
		case SourceKind::Ingredient:
			return "ingredient";
		case SourceKind::Ability:
			return "ability";
		case SourceKind::Power:
			return "power";
		case SourceKind::LesserPower:
			return "lesserPower";
		case SourceKind::Disease:
			return "disease";
		case SourceKind::Scroll:
			return "scroll";
		case SourceKind::Staff:
			return "staff";
		default:
			return "unknown";
		}
	}

	const char* ToString(CasterKind a_kind) noexcept
	{
		switch (a_kind) {
		case CasterKind::Player:
			return "player";
		case CasterKind::Follower:
			return "follower";
		case CasterKind::Other:
			return "other";
		default:
			return "unknown";
		}
	}

	std::string ToString(const FormKey& a_key)
	{
		if (a_key.Empty()) {
			return "<none>";
		}
		char hex[16];
		std::snprintf(hex, sizeof(hex), "0x%06X", a_key.id);
		return a_key.plugin + "|" + hex;
	}

	std::string ToLower(std::string_view a_text)
	{
		std::string out(a_text);
		std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		return out;
	}

	bool EqualsNoCase(std::string_view a_lhs, std::string_view a_rhs) noexcept
	{
		return a_lhs.size() == a_rhs.size() &&
		       std::equal(a_lhs.begin(), a_lhs.end(), a_rhs.begin(), [](unsigned char x, unsigned char y) {
			       return std::tolower(x) == std::tolower(y);
		       });
	}

	bool ContainsNoCase(std::string_view a_haystack, std::string_view a_needle)
	{
		if (a_needle.empty()) {
			return true;
		}
		const auto it = std::search(a_haystack.begin(), a_haystack.end(), a_needle.begin(), a_needle.end(),
			[](unsigned char x, unsigned char y) { return std::tolower(x) == std::tolower(y); });
		return it != a_haystack.end();
	}
}
