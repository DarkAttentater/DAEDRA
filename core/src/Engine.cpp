#include "daedra/Engine.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace daedra
{
	namespace
	{
		bool ReadFile(const std::filesystem::path& a_path, std::string& a_out)
		{
			std::ifstream in(a_path, std::ios::binary);
			if (!in) {
				return false;
			}
			std::ostringstream ss;
			ss << in.rdbuf();
			a_out = ss.str();
			// strip UTF-8 BOM
			if (a_out.size() >= 3 && static_cast<unsigned char>(a_out[0]) == 0xEF && static_cast<unsigned char>(a_out[1]) == 0xBB && static_cast<unsigned char>(a_out[2]) == 0xBF) {
				a_out.erase(0, 3);
			}
			return true;
		}
	}

	Diagnostics Engine::LoadFiles(const std::vector<std::pair<std::string, std::string>>& a_files, const ParseOptions& a_options)
	{
		Diagnostics       diag;
		std::vector<Rule> rules;
		for (const auto& [name, text] : a_files) {
			ParseRuleFile(name, text, a_options, rules, diag);
		}
		{
			std::lock_guard l(lock_);
			rules_ = std::move(rules);
			cache_.clear();
			lastMatched_ = nullptr;
			Sort();
		}
		return diag;
	}

	Diagnostics Engine::LoadDirectory(const std::filesystem::path& a_dir, const ParseOptions& a_options)
	{
		Diagnostics diag;
		std::vector<std::pair<std::string, std::string>> files;

		std::error_code ec;
		if (std::filesystem::is_directory(a_dir, ec)) {
			std::vector<std::filesystem::path> paths;
			for (std::filesystem::directory_iterator it(a_dir, ec), end; !ec && it != end; it.increment(ec)) {
				if (it->is_regular_file(ec) && it->path().filename().string().rfind("_", 0) != 0 && ToLower(it->path().extension().string()) == ".json") {
					paths.push_back(it->path());
				}
			}
			std::sort(paths.begin(), paths.end(), [](const auto& x, const auto& y) {
				return ToLower(x.filename().string()) < ToLower(y.filename().string());
			});
			for (const auto& p : paths) {
				std::string text;
				if (ReadFile(p, text)) {
					files.emplace_back(p.filename().string(), std::move(text));
				} else {
					diag.push_back({ Severity::Error, p.filename().string() + ": could not be read" });
				}
			}
		} else {
			diag.push_back({ Severity::Info, "rule folder not found: " + a_dir.string() });
		}

		auto more = LoadFiles(files, a_options);
		diag.insert(diag.end(), more.begin(), more.end());
		return diag;
	}

	Diagnostics Engine::AddJson(const std::string& a_name, const std::string& a_text, const ParseOptions& a_options)
	{
		Diagnostics diag;
		std::lock_guard l(lock_);
		// Rules point into rules_, so rebuild the cache after growing the vector.
		ParseRuleFile(a_name, a_text, a_options, rules_, diag);
		cache_.clear();
		lastMatched_ = nullptr;
		Sort();
		return diag;
	}

	void Engine::Sort()
	{
		std::stable_sort(rules_.begin(), rules_.end(), [](const Rule& x, const Rule& y) {
			if (x.priority != y.priority) {
				return x.priority > y.priority;
			}
			return x.order > y.order;
		});
	}

	std::string Engine::CacheKey(const Facts& a_f)
	{
		std::string key;
		key.reserve(96);
		key += ToString(a_f.effect.form);
		key += '/';
		key += ToString(a_f.source.form);
		key += '/';
		key += ToString(a_f.source.kind);
		key += '/';
		key += ToString(a_f.caster.kind);
		key += '/';
		key += ToString(a_f.caster.form);
		key += '/';
		key += a_f.worn.present ? ToString(a_f.worn.form) : std::string("-");
		// Rows with no source form can differ only by name; include it so they do not collide.
		if (a_f.source.form.Empty()) {
			key += '/';
			key += a_f.source.name;
		}
		return key;
	}

	std::optional<Assign> Engine::Resolve(const Facts& a_facts)
	{
		std::lock_guard l(lock_);
		try {
			const auto key = CacheKey(a_facts);
			if (const auto it = cache_.find(key); it != cache_.end()) {
				lastMatched_ = it->second;
				return it->second ? std::optional<Assign>(it->second->assign) : std::nullopt;
			}
			const Rule* hit = nullptr;
			for (const auto& rule : rules_) {
				bool matched = false;
				try {
					matched = rule.match(a_facts);
				} catch (...) {
					matched = false;  // a bad rule must not affect the others
				}
				if (matched) {
					hit = &rule;
					break;
				}
			}
			cache_.emplace(key, hit);
			lastMatched_ = hit;
			return hit ? std::optional<Assign>(hit->assign) : std::nullopt;
		} catch (...) {
			return std::nullopt;
		}
	}

	std::size_t Engine::RuleCount() const
	{
		std::lock_guard l(lock_);
		return rules_.size();
	}

	void Engine::ClearCache()
	{
		std::lock_guard l(lock_);
		cache_.clear();
		lastMatched_ = nullptr;
	}
}
