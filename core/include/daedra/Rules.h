#pragma once

#include "daedra/Facts.h"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace daedra
{
	enum class Severity
	{
		Info,
		Warn,
		Error,
	};

	struct Diagnostic
	{
		Severity    severity{ Severity::Warn };
		std::string text;  // already contains file / rule context
	};

	using Diagnostics = std::vector<Diagnostic>;

	// What a matching rule writes onto the row. Unset members leave the row alone.
	struct Assign
	{
		std::optional<std::string> iconLabel;
		std::optional<std::string> iconSource;
		std::optional<std::string> sourceText;
		std::optional<std::string> nameText;
		std::optional<std::string> extraText;
		std::optional<std::string> sortGroup;
		std::optional<std::string> color;
		std::optional<bool>        hide;
		std::optional<bool>        iconFromWorn;  // show the icon of the worn item that carries the effect
		std::optional<bool>        iconFromSource;  // show the icon PNGII gives the source spell / enchantment
		std::optional<FormKey>     iconAsForm;      // show the icon PNGII gives this form

		[[nodiscard]] bool Any() const noexcept
		{
			return iconLabel || iconSource || sourceText || nameText || extraText || sortGroup || color || hide || iconFromWorn || iconFromSource || iconAsForm;
		}
	};

	using Predicate = std::function<bool(const Facts&)>;

	struct Rule
	{
		std::string id;
		std::string file;
		int         priority{ 0 };
		std::size_t order{ 0 };  // global load order, later wins on ties
		Predicate   match;
		Assign      assign;
	};

	struct ParseOptions
	{
		bool strict{ false };  // unknown keys become errors instead of warnings
	};

	// Parses one rule file's text. Bad rules are skipped and reported; good rules are
	// appended to a_out. Never throws.
	void ParseRuleFile(
		const std::string&  a_fileName,
		const std::string&  a_text,
		const ParseOptions& a_options,
		std::vector<Rule>&  a_out,
		Diagnostics&        a_diag);
}
