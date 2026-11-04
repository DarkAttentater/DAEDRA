#pragma once

#include "daedra/Rules.h"

#include <filesystem>
#include <mutex>
#include <string>
#include <unordered_map>

namespace daedra
{
	class Engine
	{
	public:
		Engine() = default;
		Engine(Engine&& a_other) noexcept
		{
			std::lock_guard l(a_other.lock_);
			rules_ = std::move(a_other.rules_);
			cache_ = std::move(a_other.cache_);
			lastMatched_ = a_other.lastMatched_;
		}

		// Replaces the rule set from every *.json in a_dir (alphabetical, case-insensitive).
		// A missing folder is not an error. Returns diagnostics; never throws.
		Diagnostics LoadDirectory(const std::filesystem::path& a_dir, const ParseOptions& a_options);

		// Replaces the rule set from in-memory files (used by tests and the runtime API).
		Diagnostics LoadFiles(const std::vector<std::pair<std::string, std::string>>& a_files, const ParseOptions& a_options);

		// Adds rules from one JSON document without dropping the existing set.
		Diagnostics AddJson(const std::string& a_name, const std::string& a_text, const ParseOptions& a_options);

		// Highest priority wins; equal priority, the later rule wins. First hit is returned whole.
		[[nodiscard]] std::optional<Assign> Resolve(const Facts& a_facts);

		[[nodiscard]] std::size_t RuleCount() const;
		[[nodiscard]] const Rule* LastMatchedRule() const noexcept { return lastMatched_; }
		void                      ClearCache();

	private:
		static std::string CacheKey(const Facts& a_facts);
		void               Sort();

		mutable std::mutex                                      lock_;
		std::vector<Rule>                                       rules_;  // sorted: best first
		std::unordered_map<std::string, const Rule*>            cache_;  // value null = no rule matches
		const Rule*                                             lastMatched_{ nullptr };
	};
}
