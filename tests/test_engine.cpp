#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "daedra/Engine.h"

using namespace daedra;

namespace
{
	Facts MakeFacts(const char* a_effectPlugin, std::uint32_t a_effectId, const char* a_sourcePlugin, std::uint32_t a_sourceId, const char* a_sourceName, SourceKind a_kind = SourceKind::Ability)
	{
		Facts f;
		f.effect.form = { a_effectPlugin, a_effectId };
		f.source.form = { a_sourcePlugin, a_sourceId };
		f.source.name = a_sourceName;
		f.source.kind = a_kind;
		return f;
	}

	bool HasSeverity(const Diagnostics& d, Severity s)
	{
		for (const auto& x : d) {
			if (x.severity == s) {
				return true;
			}
		}
		return false;
	}

	Engine LoadOne(const std::string& a_json, Diagnostics* a_diag = nullptr, ParseOptions a_opt = {})
	{
		Engine e;
		auto   d = e.LoadFiles({ { "test.json", a_json } }, a_opt);
		if (a_diag) {
			*a_diag = d;
		}
		return e;
	}
}

TEST_CASE("same effect, different source gives different icon")
{
	const auto json = R"({
	  "version": 1,
	  "rules": [
	    { "id": "akatosh", "match": { "effect": { "form": "Skyrim.esm|0x0ABCDE" }, "source": { "form": "Skyrim.esm|0x100000" } },
	      "assign": { "iconLabel": "deity_akatosh" } },
	    { "id": "circlet", "match": { "effect": { "form": "Skyrim.esm|0x0ABCDE" }, "source": { "kind": "enchantment" } },
	      "assign": { "iconLabel": "skill_experience" } }
	  ] })";
	Diagnostics d;
	auto        e = LoadOne(json, &d);
	CHECK(e.RuleCount() == 2);
	CHECK_FALSE(HasSeverity(d, Severity::Error));

	auto a = e.Resolve(MakeFacts("skyrim.esm", 0xABCDE, "skyrim.esm", 0x100000, "Blessing of Akatosh"));
	REQUIRE(a);
	CHECK(*a->iconLabel == "deity_akatosh");

	auto b = e.Resolve(MakeFacts("skyrim.esm", 0xABCDE, "skyrim.esm", 0x200000, "Elven Armored Circlet of Experience", SourceKind::Enchantment));
	REQUIRE(b);
	CHECK(*b->iconLabel == "skill_experience");

	CHECK_FALSE(e.Resolve(MakeFacts("skyrim.esm", 0x111, "skyrim.esm", 0x100000, "Other")));
}

TEST_CASE("priority beats order, later wins ties")
{
	const auto json = R"({ "version": 1, "rules": [
	  { "id": "a", "priority": 5, "match": { "plugin": "x.esp" }, "assign": { "iconLabel": "A" } },
	  { "id": "b", "match": { "plugin": "x.esp" }, "assign": { "iconLabel": "B" } },
	  { "id": "c", "match": { "plugin": "x.esp" }, "assign": { "iconLabel": "C" } } ] })";
	auto e = LoadOne(json);
	CHECK(*e.Resolve(MakeFacts("a.esp", 1, "x.esp", 1, "n"))->iconLabel == "A");

	const auto json2 = R"({ "version": 1, "rules": [
	  { "match": { "plugin": "x.esp" }, "assign": { "iconLabel": "B" } },
	  { "match": { "plugin": "x.esp" }, "assign": { "iconLabel": "C" } } ] })";
	auto e2 = LoadOne(json2);
	CHECK(*e2.Resolve(MakeFacts("a.esp", 1, "x.esp", 1, "n"))->iconLabel == "C");
}

TEST_CASE("later file overrides earlier file")
{
	Engine e;
	e.LoadFiles({ { "a.json", R"({ "version": 1, "rules": [ { "match": { "plugin": "x.esp" }, "assign": { "iconLabel": "first" } } ] })" },
					{ "b.json", R"({ "version": 1, "rules": [ { "match": { "plugin": "x.esp" }, "assign": { "iconLabel": "second" } } ] })" } },
		{});
	CHECK(*e.Resolve(MakeFacts("a.esp", 1, "x.esp", 1, "n"))->iconLabel == "second");
}

TEST_CASE("name matching: exact, contains, regex, all case-insensitive")
{
	const auto json = R"({ "version": 1, "rules": [
	  { "id": "exact", "match": { "source": { "name": "Agent of Mara" } }, "assign": { "iconLabel": "mara" } },
	  { "id": "contains", "match": { "source": { "name": { "contains": "beastblood" } } }, "assign": { "iconLabel": "beast" } },
	  { "id": "regex", "match": { "source": { "name": { "regex": "^Follower of (Mephala|Boethiah)$" } } }, "assign": { "iconLabel": "devotee" } } ] })";
	auto e = LoadOne(json);
	CHECK(*e.Resolve(MakeFacts("a", 1, "b", 1, "agent of mara"))->iconLabel == "mara");
	CHECK(*e.Resolve(MakeFacts("a", 1, "b", 2, "Ancient BeastBlood Boon"))->iconLabel == "beast");
	CHECK(*e.Resolve(MakeFacts("a", 1, "b", 3, "Follower of Boethiah"))->iconLabel == "devotee");
	CHECK_FALSE(e.Resolve(MakeFacts("a", 1, "b", 4, "Follower of Azura")));
}

TEST_CASE("keywords, detrimental, caster and worn item")
{
	const auto json = R"({ "version": 1, "rules": [
	  { "id": "k", "match": { "effect": { "keywords": ["MagicDamageFire"], "detrimental": true }, "caster": "player" }, "assign": { "iconLabel": "fire" } },
	  { "id": "w", "match": { "worn": { "form": "Skyrim.esm|0x000010" } }, "assign": { "iconLabel": "worn" } } ] })";
	auto e = LoadOne(json);

	auto f = MakeFacts("a", 1, "b", 1, "n");
	f.effect.keywords = { "magicdamagefire", "Other" };
	f.effect.detrimental = true;
	f.caster.kind = CasterKind::Player;
	CHECK(*e.Resolve(f)->iconLabel == "fire");

	f.caster.kind = CasterKind::Follower;
	f.effect.form.id = 2;  // new cache entry
	CHECK_FALSE(e.Resolve(f));

	auto g = MakeFacts("a", 9, "b", 9, "n");
	g.worn = { true, { "skyrim.esm", 0x10 }, {} };
	CHECK(*e.Resolve(g)->iconLabel == "worn");
}

TEST_CASE("anyOf / allOf / not")
{
	const auto json = R"({ "version": 1, "rules": [
	  { "id": "x", "match": { "anyOf": [ { "plugin": "a.esp" }, { "plugin": "b.esp" } ], "not": { "source": { "kind": "potion" } } },
	    "assign": { "iconLabel": "ok" } } ] })";
	auto e = LoadOne(json);
	CHECK(e.Resolve(MakeFacts("e", 1, "a.esp", 1, "n", SourceKind::Spell)));
	CHECK(e.Resolve(MakeFacts("e", 1, "b.esp", 1, "n", SourceKind::Spell)));
	CHECK_FALSE(e.Resolve(MakeFacts("e", 1, "c.esp", 1, "n", SourceKind::Spell)));
	CHECK_FALSE(e.Resolve(MakeFacts("e", 1, "a.esp", 2, "n", SourceKind::Potion)));
}

TEST_CASE("bad rules are skipped with a clear message, good ones survive")
{
	const auto json = R"({ "version": 1, "rules": [
	  { "id": "badform", "match": { "effect": { "form": "nonsense" } }, "assign": { "iconLabel": "x" } },
	  { "id": "badregex", "match": { "source": { "name": { "regex": "(" } } }, "assign": { "iconLabel": "x" } },
	  { "id": "empty", "match": {}, "assign": { "iconLabel": "x" } },
	  { "id": "noassign", "match": { "plugin": "a.esp" }, "assign": {} },
	  { "id": "badkind", "match": { "source": { "kind": "banana" } }, "assign": { "iconLabel": "x" } },
	  { "id": "good", "match": { "plugin": "a.esp" }, "assign": { "iconLabel": "yes" } } ] })";
	Diagnostics d;
	auto        e = LoadOne(json, &d);
	CHECK(e.RuleCount() == 1);
	CHECK(HasSeverity(d, Severity::Error));
	bool namedRule = false;
	for (const auto& x : d) {
		if (x.text.find("test.json: rule 0 [badform]") != std::string::npos) {
			namedRule = true;
		}
	}
	CHECK(namedRule);
	CHECK(*e.Resolve(MakeFacts("e", 1, "a.esp", 1, "n"))->iconLabel == "yes");
}

TEST_CASE("unknown keys warn by default and fail in strict mode")
{
	const auto json = R"({ "version": 1, "rules": [
	  { "match": { "plugin": "a.esp", "bogus": 1 }, "assign": { "iconLabel": "x" } } ] })";
	Diagnostics d;
	auto        e = LoadOne(json, &d);
	CHECK(e.RuleCount() == 1);
	CHECK(HasSeverity(d, Severity::Warn));
	CHECK_FALSE(HasSeverity(d, Severity::Error));

	auto s = LoadOne(json, &d, ParseOptions{ true });
	CHECK(s.RuleCount() == 0);
	CHECK(HasSeverity(d, Severity::Error));
}

TEST_CASE("file level errors do not throw")
{
	Diagnostics d;
	CHECK(LoadOne("{ not json", &d).RuleCount() == 0);
	CHECK(HasSeverity(d, Severity::Error));
	CHECK(LoadOne(R"({ "version": 2, "rules": [] })", &d).RuleCount() == 0);
	CHECK(HasSeverity(d, Severity::Error));
	CHECK(LoadOne(R"({ "rules": [] })", &d).RuleCount() == 0);
	CHECK(HasSeverity(d, Severity::Error));
	CHECK(LoadOne(R"([])", &d).RuleCount() == 0);
}

TEST_CASE("comments and $schema are accepted")
{
	const auto json = R"({
	  // authors like comments
	  "$schema": "./schema.json", "version": 1,
	  "rules": [ { "match": { "plugin": "a.esp" }, "assign": { "iconLabel": "x", "hide": false } } ] })";
	Diagnostics d;
	auto        e = LoadOne(json, &d);
	CHECK(e.RuleCount() == 1);
	CHECK_FALSE(HasSeverity(d, Severity::Error));
}

TEST_CASE("light plugin local ids compare by plugin and local id")
{
	const auto json = R"({ "version": 1, "rules": [
	  { "match": { "source": { "form": "Light.esl|0x800" } }, "assign": { "iconLabel": "esl" } } ] })";
	auto e = LoadOne(json);
	CHECK(e.Resolve(MakeFacts("e", 1, "light.esl", 0x800, "n")));
	CHECK_FALSE(e.Resolve(MakeFacts("e", 1, "light.esl", 0x801, "n")));
}

TEST_CASE("missing rule folder is only informational")
{
	Engine e;
	auto   d = e.LoadDirectory("Z:/definitely/not/here", {});
	CHECK_FALSE(HasSeverity(d, Severity::Error));
	CHECK(e.RuleCount() == 0);
}

TEST_CASE("iconFromWorn parses, and a low-priority generic gear rule yields to specific ones")
{
	const auto json = R"({ "version": 1, "rules": [
	  { "id": "gear", "priority": -10, "match": { "worn": {} }, "assign": { "iconFromWorn": true } },
	  { "id": "special", "match": { "worn": { "form": "Mod.esp|0x1182B" } }, "assign": { "iconLabel": "circlet", "iconFromWorn": true } },
	  { "id": "bad", "match": { "plugin": "x.esp" }, "assign": { "iconFromWorn": "yes" } } ] })";
	Diagnostics d;
	auto        e = LoadOne(json, &d);
	CHECK(e.RuleCount() == 2);
	CHECK(HasSeverity(d, Severity::Error));  // the bad one only

	auto f = MakeFacts("a", 1, "b", 1, "n");
	CHECK_FALSE(e.Resolve(f));  // no worn item: neither rule
	f.worn = { true, { "other.esp", 5 }, {} };
	auto generic = e.Resolve(f);
	REQUIRE(generic);
	CHECK(generic->iconFromWorn.value_or(false));
	CHECK_FALSE(generic->iconLabel);

	auto g = MakeFacts("a", 2, "b", 2, "n");
	g.worn = { true, { "mod.esp", 0x1182B }, {} };
	auto specific = e.Resolve(g);
	REQUIRE(specific);
	CHECK(*specific->iconLabel == "circlet");
}
