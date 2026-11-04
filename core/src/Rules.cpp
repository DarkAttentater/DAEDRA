#include "daedra/Rules.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <charconv>
#include <initializer_list>
#include <memory>
#include <regex>

namespace daedra
{
	namespace
	{
		using json = nlohmann::json;

		constexpr int kMaxDepth = 8;

		struct Ctx
		{
			const ParseOptions& options;
			Diagnostics&        diag;
			std::string         where;
			bool                failed{ false };

			void Error(const std::string& a_path, const std::string& a_msg)
			{
				diag.push_back({ Severity::Error, where + a_path + ": " + a_msg });
				failed = true;
			}

			void Unknown(const std::string& a_path, const std::string& a_key)
			{
				if (options.strict) {
					Error(a_path, "unknown key '" + a_key + "'");
				} else {
					diag.push_back({ Severity::Warn, where + a_path + ": unknown key '" + a_key + "' ignored" });
				}
			}

			void CheckKeys(const json& a_obj, const std::string& a_path, std::initializer_list<const char*> a_allowed)
			{
				for (auto it = a_obj.begin(); it != a_obj.end(); ++it) {
					const auto found = std::find_if(a_allowed.begin(), a_allowed.end(),
						[&](const char* k) { return it.key() == k; });
					if (found == a_allowed.end()) {
						Unknown(a_path, it.key());
					}
				}
			}
		};

		// "Plugin.esp|0x0012AB" (0x optional)
		std::optional<FormKey> ParseForm(const std::string& a_text)
		{
			const auto bar = a_text.find('|');
			if (bar == std::string::npos) {
				return std::nullopt;
			}
			auto trim = [](std::string s) {
				const auto b = s.find_first_not_of(" \t");
				const auto e = s.find_last_not_of(" \t");
				return b == std::string::npos ? std::string{} : s.substr(b, e - b + 1);
			};
			auto plugin = trim(a_text.substr(0, bar));
			auto hex = trim(a_text.substr(bar + 1));
			if (hex.size() > 2 && hex[0] == '0' && (hex[1] == 'x' || hex[1] == 'X')) {
				hex = hex.substr(2);
			}
			if (plugin.empty() || hex.empty()) {
				return std::nullopt;
			}
			std::uint32_t id = 0;
			const auto [ptr, ec] = std::from_chars(hex.data(), hex.data() + hex.size(), id, 16);
			if (ec != std::errc{} || ptr != hex.data() + hex.size() || id > 0xFFFFFF) {
				return std::nullopt;
			}
			return FormKey{ ToLower(plugin), id };
		}

		// string or array of strings
		std::optional<std::vector<std::string>> ReadStrings(const json& a_val, Ctx& a_ctx, const std::string& a_path)
		{
			std::vector<std::string> out;
			if (a_val.is_string()) {
				out.push_back(a_val.get<std::string>());
			} else if (a_val.is_array()) {
				for (const auto& v : a_val) {
					if (!v.is_string()) {
						a_ctx.Error(a_path, "expected strings");
						return std::nullopt;
					}
					out.push_back(v.get<std::string>());
				}
			} else {
				a_ctx.Error(a_path, "expected a string or an array of strings");
				return std::nullopt;
			}
			if (out.empty()) {
				a_ctx.Error(a_path, "list is empty");
				return std::nullopt;
			}
			return out;
		}

		std::optional<FormKey> ReadForm(const json& a_val, Ctx& a_ctx, const std::string& a_path)
		{
			if (!a_val.is_string()) {
				a_ctx.Error(a_path, "expected \"Plugin.esp|0xHEX\"");
				return std::nullopt;
			}
			auto key = ParseForm(a_val.get<std::string>());
			if (!key) {
				a_ctx.Error(a_path, "bad form '" + a_val.get<std::string>() + "', expected \"Plugin.esp|0xHEX\"");
			}
			return key;
		}

		bool HasKeywordAll(const std::vector<std::string>& a_have, const std::vector<std::string>& a_want)
		{
			return std::all_of(a_want.begin(), a_want.end(), [&](const std::string& w) {
				return std::any_of(a_have.begin(), a_have.end(), [&](const std::string& h) { return EqualsNoCase(h, w); });
			});
		}

		bool HasKeywordAny(const std::vector<std::string>& a_have, const std::vector<std::string>& a_want)
		{
			return std::any_of(a_want.begin(), a_want.end(), [&](const std::string& w) {
				return std::any_of(a_have.begin(), a_have.end(), [&](const std::string& h) { return EqualsNoCase(h, w); });
			});
		}

		std::optional<SourceKind> ParseKind(const std::string& a_text)
		{
			static constexpr SourceKind kAll[] = {
				SourceKind::Spell, SourceKind::Enchantment, SourceKind::Potion, SourceKind::Ingredient,
				SourceKind::Ability, SourceKind::Power, SourceKind::LesserPower, SourceKind::Disease,
				SourceKind::Scroll, SourceKind::Staff, SourceKind::Unknown
			};
			for (auto k : kAll) {
				if (EqualsNoCase(ToString(k), a_text)) {
					return k;
				}
			}
			return std::nullopt;
		}

		using Pred = Predicate;

		Pred AllOf(std::vector<Pred> a_preds)
		{
			return [preds = std::move(a_preds)](const Facts& f) {
				return std::all_of(preds.begin(), preds.end(), [&](const Pred& p) { return p(f); });
			};
		}

		std::optional<Pred> ParseMatch(const json& a_obj, Ctx& a_ctx, const std::string& a_path, int a_depth);

		std::optional<Pred> ParseEffect(const json& a_obj, Ctx& a_ctx, const std::string& a_path)
		{
			if (!a_obj.is_object()) {
				a_ctx.Error(a_path, "expected an object");
				return std::nullopt;
			}
			a_ctx.CheckKeys(a_obj, a_path, { "form", "edid", "keywords", "keywordsAny", "archetype", "actorValue", "detrimental", "plugin" });
			std::vector<Pred> preds;
			for (auto it = a_obj.begin(); it != a_obj.end(); ++it) {
				const auto& k = it.key();
				const auto& v = it.value();
				const auto  p = a_path + "." + k;
				if (k == "form") {
					if (auto key = ReadForm(v, a_ctx, p)) {
						preds.push_back([key = *key](const Facts& f) { return f.effect.form == key; });
					}
				} else if (k == "edid") {
					if (auto s = ReadStrings(v, a_ctx, p)) {
						preds.push_back([s = *s](const Facts& f) {
							return std::any_of(s.begin(), s.end(), [&](const std::string& e) { return EqualsNoCase(f.effect.edid, e); });
						});
					}
				} else if (k == "keywords" || k == "keywordsAny") {
					if (auto s = ReadStrings(v, a_ctx, p)) {
						if (k == "keywords") {
							preds.push_back([s = *s](const Facts& f) { return HasKeywordAll(f.effect.keywords, s); });
						} else {
							preds.push_back([s = *s](const Facts& f) { return HasKeywordAny(f.effect.keywords, s); });
						}
					}
				} else if (k == "archetype") {
					if (auto s = ReadStrings(v, a_ctx, p)) {
						preds.push_back([s = *s](const Facts& f) {
							return std::any_of(s.begin(), s.end(), [&](const std::string& e) { return EqualsNoCase(f.effect.archetype, e); });
						});
					}
				} else if (k == "actorValue") {
					if (auto s = ReadStrings(v, a_ctx, p)) {
						preds.push_back([s = *s](const Facts& f) {
							return std::any_of(s.begin(), s.end(), [&](const std::string& e) { return EqualsNoCase(f.effect.actorValue, e); });
						});
					}
				} else if (k == "detrimental") {
					if (!v.is_boolean()) {
						a_ctx.Error(p, "expected true or false");
					} else {
						preds.push_back([b = v.get<bool>()](const Facts& f) { return f.effect.detrimental == b; });
					}
				} else if (k == "plugin") {
					if (v.is_string()) {
						preds.push_back([s = ToLower(v.get<std::string>())](const Facts& f) { return f.effect.form.plugin == s; });
					} else {
						a_ctx.Error(p, "expected a plugin file name");
					}
				}
			}
			if (preds.empty() && !a_ctx.failed) {
				a_ctx.Error(a_path, "no usable keys");
			}
			return AllOf(std::move(preds));
		}

		std::optional<Pred> ParseName(const json& a_val, Ctx& a_ctx, const std::string& a_path)
		{
			if (a_val.is_string()) {
				return Pred{ [s = a_val.get<std::string>()](const Facts& f) { return EqualsNoCase(f.source.name, s); } };
			}
			if (!a_val.is_object()) {
				a_ctx.Error(a_path, "expected a string or {exact|contains|regex}");
				return std::nullopt;
			}
			a_ctx.CheckKeys(a_val, a_path, { "exact", "contains", "regex" });
			std::vector<Pred> preds;
			if (a_val.contains("exact") && a_val["exact"].is_string()) {
				preds.push_back([s = a_val["exact"].get<std::string>()](const Facts& f) { return EqualsNoCase(f.source.name, s); });
			}
			if (a_val.contains("contains") && a_val["contains"].is_string()) {
				preds.push_back([s = a_val["contains"].get<std::string>()](const Facts& f) { return ContainsNoCase(f.source.name, s); });
			}
			if (a_val.contains("regex")) {
				if (!a_val["regex"].is_string()) {
					a_ctx.Error(a_path + ".regex", "expected a string");
				} else {
					try {
						auto re = std::make_shared<std::regex>(a_val["regex"].get<std::string>(), std::regex::ECMAScript | std::regex::icase);
						preds.push_back([re](const Facts& f) { return std::regex_search(f.source.name, *re); });
					} catch (const std::regex_error& e) {
						a_ctx.Error(a_path + ".regex", std::string("invalid regex: ") + e.what());
					}
				}
			}
			if (preds.empty()) {
				a_ctx.Error(a_path, "no usable keys");
				return std::nullopt;
			}
			return AllOf(std::move(preds));
		}

		std::optional<Pred> ParseSource(const json& a_obj, Ctx& a_ctx, const std::string& a_path)
		{
			if (!a_obj.is_object()) {
				a_ctx.Error(a_path, "expected an object");
				return std::nullopt;
			}
			a_ctx.CheckKeys(a_obj, a_path, { "form", "edid", "kind", "keywords", "keywordsAny", "spellType", "name", "plugin" });
			std::vector<Pred> preds;
			for (auto it = a_obj.begin(); it != a_obj.end(); ++it) {
				const auto& k = it.key();
				const auto& v = it.value();
				const auto  p = a_path + "." + k;
				if (k == "form") {
					if (auto key = ReadForm(v, a_ctx, p)) {
						preds.push_back([key = *key](const Facts& f) { return f.source.form == key; });
					}
				} else if (k == "edid") {
					if (auto s = ReadStrings(v, a_ctx, p)) {
						preds.push_back([s = *s](const Facts& f) {
							return std::any_of(s.begin(), s.end(), [&](const std::string& e) { return EqualsNoCase(f.source.edid, e); });
						});
					}
				} else if (k == "kind") {
					if (auto s = ReadStrings(v, a_ctx, p)) {
						std::vector<SourceKind> kinds;
						for (const auto& text : *s) {
							if (auto kind = ParseKind(text)) {
								kinds.push_back(*kind);
							} else {
								a_ctx.Error(p, "unknown kind '" + text + "'");
							}
						}
						if (!kinds.empty()) {
							preds.push_back([kinds](const Facts& f) {
								return std::find(kinds.begin(), kinds.end(), f.source.kind) != kinds.end();
							});
						}
					}
				} else if (k == "keywords" || k == "keywordsAny") {
					if (auto s = ReadStrings(v, a_ctx, p)) {
						if (k == "keywords") {
							preds.push_back([s = *s](const Facts& f) { return HasKeywordAll(f.source.keywords, s); });
						} else {
							preds.push_back([s = *s](const Facts& f) { return HasKeywordAny(f.source.keywords, s); });
						}
					}
				} else if (k == "spellType") {
					if (auto s = ReadStrings(v, a_ctx, p)) {
						preds.push_back([s = *s](const Facts& f) {
							return std::any_of(s.begin(), s.end(), [&](const std::string& e) { return EqualsNoCase(f.source.spellType, e); });
						});
					}
				} else if (k == "name") {
					if (auto pr = ParseName(v, a_ctx, p)) {
						preds.push_back(std::move(*pr));
					}
				} else if (k == "plugin") {
					if (v.is_string()) {
						preds.push_back([s = ToLower(v.get<std::string>())](const Facts& f) { return f.source.form.plugin == s; });
					} else {
						a_ctx.Error(p, "expected a plugin file name");
					}
				}
			}
			if (preds.empty() && !a_ctx.failed) {
				a_ctx.Error(a_path, "no usable keys");
			}
			return AllOf(std::move(preds));
		}

		std::optional<Pred> ParseCaster(const json& a_val, Ctx& a_ctx, const std::string& a_path)
		{
			if (a_val.is_string()) {
				const auto text = ToLower(a_val.get<std::string>());
				CasterKind kind;
				if (text == "player") {
					kind = CasterKind::Player;
				} else if (text == "follower") {
					kind = CasterKind::Follower;
				} else if (text == "other") {
					kind = CasterKind::Other;
				} else {
					a_ctx.Error(a_path, "expected player, follower, other or {form}");
					return std::nullopt;
				}
				return Pred{ [kind](const Facts& f) { return f.caster.kind == kind; } };
			}
			if (a_val.is_object()) {
				a_ctx.CheckKeys(a_val, a_path, { "form" });
				if (a_val.contains("form")) {
					if (auto key = ReadForm(a_val["form"], a_ctx, a_path + ".form")) {
						return Pred{ [key = *key](const Facts& f) { return f.caster.form == key; } };
					}
					return std::nullopt;
				}
			}
			a_ctx.Error(a_path, "expected player, follower, other or {form}");
			return std::nullopt;
		}

		std::optional<Pred> ParseWorn(const json& a_obj, Ctx& a_ctx, const std::string& a_path)
		{
			if (!a_obj.is_object()) {
				a_ctx.Error(a_path, "expected an object");
				return std::nullopt;
			}
			a_ctx.CheckKeys(a_obj, a_path, { "form", "keywords" });
			std::vector<Pred> preds;
			preds.push_back([](const Facts& f) { return f.worn.present; });
			if (a_obj.contains("form")) {
				if (auto key = ReadForm(a_obj["form"], a_ctx, a_path + ".form")) {
					preds.push_back([key = *key](const Facts& f) { return f.worn.form == key; });
				}
			}
			if (a_obj.contains("keywords")) {
				if (auto s = ReadStrings(a_obj["keywords"], a_ctx, a_path + ".keywords")) {
					preds.push_back([s = *s](const Facts& f) { return HasKeywordAll(f.worn.keywords, s); });
				}
			}
			return AllOf(std::move(preds));
		}

		std::optional<Pred> ParseMatchList(const json& a_val, Ctx& a_ctx, const std::string& a_path, int a_depth, bool a_any)
		{
			if (!a_val.is_array() || a_val.empty()) {
				a_ctx.Error(a_path, "expected a non-empty array of match objects");
				return std::nullopt;
			}
			std::vector<Pred> preds;
			std::size_t       i = 0;
			for (const auto& item : a_val) {
				if (auto p = ParseMatch(item, a_ctx, a_path + "[" + std::to_string(i) + "]", a_depth + 1)) {
					preds.push_back(std::move(*p));
				}
				++i;
			}
			if (a_any) {
				return Pred{ [preds = std::move(preds)](const Facts& f) {
					return std::any_of(preds.begin(), preds.end(), [&](const Pred& p) { return p(f); });
				} };
			}
			return AllOf(std::move(preds));
		}

		std::optional<Pred> ParseMatch(const json& a_obj, Ctx& a_ctx, const std::string& a_path, int a_depth)
		{
			if (a_depth > kMaxDepth) {
				a_ctx.Error(a_path, "match nesting is too deep");
				return std::nullopt;
			}
			if (!a_obj.is_object()) {
				a_ctx.Error(a_path, "expected an object");
				return std::nullopt;
			}
			a_ctx.CheckKeys(a_obj, a_path, { "effect", "source", "plugin", "caster", "worn", "anyOf", "allOf", "not" });
			std::vector<Pred> preds;
			for (auto it = a_obj.begin(); it != a_obj.end(); ++it) {
				const auto& k = it.key();
				const auto& v = it.value();
				const auto  p = a_path + "." + k;
				std::optional<Pred> parsed;
				if (k == "effect") {
					parsed = ParseEffect(v, a_ctx, p);
				} else if (k == "source") {
					parsed = ParseSource(v, a_ctx, p);
				} else if (k == "plugin") {
					if (v.is_string()) {
						parsed = Pred{ [s = ToLower(v.get<std::string>())](const Facts& f) { return f.source.form.plugin == s; } };
					} else {
						a_ctx.Error(p, "expected a plugin file name");
					}
				} else if (k == "caster") {
					parsed = ParseCaster(v, a_ctx, p);
				} else if (k == "worn") {
					parsed = ParseWorn(v, a_ctx, p);
				} else if (k == "anyOf") {
					parsed = ParseMatchList(v, a_ctx, p, a_depth, true);
				} else if (k == "allOf") {
					parsed = ParseMatchList(v, a_ctx, p, a_depth, false);
				} else if (k == "not") {
					if (auto inner = ParseMatch(v, a_ctx, p, a_depth + 1)) {
						parsed = Pred{ [inner = std::move(*inner)](const Facts& f) { return !inner(f); } };
					}
				}
				if (parsed) {
					preds.push_back(std::move(*parsed));
				}
			}
			if (preds.empty() && !a_ctx.failed) {
				a_ctx.Error(a_path, "match is empty (an empty match would hit every row)");
			}
			return AllOf(std::move(preds));
		}

		bool ReadAssign(const json& a_obj, Ctx& a_ctx, const std::string& a_path, Assign& a_out)
		{
			if (!a_obj.is_object()) {
				a_ctx.Error(a_path, "expected an object");
				return false;
			}
			a_ctx.CheckKeys(a_obj, a_path, { "iconLabel", "iconSource", "sourceText", "nameText", "extraText", "sortGroup", "color", "hide", "iconFromWorn", "iconFromSource", "iconAsForm" });
			auto str = [&](const char* a_key, std::optional<std::string>& a_dst) {
				if (!a_obj.contains(a_key)) {
					return;
				}
				if (a_obj[a_key].is_string()) {
					a_dst = a_obj[a_key].get<std::string>();
				} else {
					a_ctx.Error(a_path + "." + a_key, "expected a string");
				}
			};
			str("iconLabel", a_out.iconLabel);
			str("iconSource", a_out.iconSource);
			str("sourceText", a_out.sourceText);
			str("nameText", a_out.nameText);
			str("extraText", a_out.extraText);
			str("sortGroup", a_out.sortGroup);
			str("color", a_out.color);
			if (a_obj.contains("iconFromSource")) {
				if (a_obj["iconFromSource"].is_boolean()) {
					a_out.iconFromSource = a_obj["iconFromSource"].get<bool>();
				} else {
					a_ctx.Error(a_path + ".iconFromSource", "expected true or false");
				}
			}
			if (a_obj.contains("iconAsForm")) {
				if (auto key = ReadForm(a_obj["iconAsForm"], a_ctx, a_path + ".iconAsForm")) {
					a_out.iconAsForm = *key;
				}
			}
			if (a_obj.contains("iconFromWorn")) {
				if (a_obj["iconFromWorn"].is_boolean()) {
					a_out.iconFromWorn = a_obj["iconFromWorn"].get<bool>();
				} else {
					a_ctx.Error(a_path + ".iconFromWorn", "expected true or false");
				}
			}
			if (a_obj.contains("hide")) {
				if (a_obj["hide"].is_boolean()) {
					a_out.hide = a_obj["hide"].get<bool>();
				} else {
					a_ctx.Error(a_path + ".hide", "expected true or false");
				}
			}
			if (!a_out.Any() && !a_ctx.failed) {
				a_ctx.Error(a_path, "assigns nothing");
			}
			return !a_ctx.failed;
		}
	}

	void ParseRuleFile(
		const std::string&  a_fileName,
		const std::string&  a_text,
		const ParseOptions& a_options,
		std::vector<Rule>&  a_out,
		Diagnostics&        a_diag)
	{
		try {
			json doc;
			try {
				doc = json::parse(a_text, nullptr, true, true);  // allow comments
			} catch (const json::parse_error& e) {
				a_diag.push_back({ Severity::Error, a_fileName + ": " + e.what() });
				return;
			}

			Ctx top{ a_options, a_diag, a_fileName, false };
			if (!doc.is_object()) {
				top.Error("", "top level must be an object");
				return;
			}
			top.CheckKeys(doc, "", { "$schema", "version", "rules" });
			if (!doc.contains("version") || !doc["version"].is_number_integer()) {
				top.Error("", "missing integer 'version'");
				return;
			}
			if (doc["version"].get<int>() != 1) {
				top.Error("", "unsupported version " + std::to_string(doc["version"].get<int>()) + " (this build reads version 1)");
				return;
			}
			if (!doc.contains("rules") || !doc["rules"].is_array()) {
				top.Error("", "missing 'rules' array");
				return;
			}

			std::size_t index = 0;
			for (const auto& entry : doc["rules"]) {
				std::string id = a_fileName + "#" + std::to_string(index);
				if (entry.is_object() && entry.contains("id") && entry["id"].is_string()) {
					id = entry["id"].get<std::string>();
				}
				Ctx ctx{ a_options, a_diag, a_fileName + ": rule " + std::to_string(index) + " [" + id + "]", false };
				++index;

				if (!entry.is_object()) {
					ctx.Error("", "rule must be an object");
					continue;
				}
				ctx.CheckKeys(entry, "", { "id", "priority", "match", "assign", "comment" });

				Rule rule;
				rule.id = id;
				rule.file = a_fileName;
				if (entry.contains("priority")) {
					if (entry["priority"].is_number_integer()) {
						rule.priority = entry["priority"].get<int>();
					} else {
						ctx.Error(".priority", "expected an integer");
					}
				}
				if (!entry.contains("match")) {
					ctx.Error("", "missing 'match'");
				}
				if (!entry.contains("assign")) {
					ctx.Error("", "missing 'assign'");
				}
				if (ctx.failed) {
					continue;
				}
				auto pred = ParseMatch(entry["match"], ctx, ".match", 0);
				ReadAssign(entry["assign"], ctx, ".assign", rule.assign);
				if (ctx.failed || !pred) {
					a_diag.push_back({ Severity::Warn, ctx.where + ": rule skipped" });
					continue;
				}
				rule.match = std::move(*pred);
				rule.order = a_out.size();
				a_out.push_back(std::move(rule));
			}
		} catch (const std::exception& e) {
			a_diag.push_back({ Severity::Error, a_fileName + ": unexpected error: " + e.what() });
		} catch (...) {
			a_diag.push_back({ Severity::Error, a_fileName + ": unexpected error" });
		}
	}
}
