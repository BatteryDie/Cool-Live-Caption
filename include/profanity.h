#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_set>

class ProfanityFilter {
public:
  bool load(const std::filesystem::path &dir, std::string_view language_code);
  std::string filter(std::string_view text) const;
  bool has_entries() const { return !words_.empty(); }

private:
  std::unordered_set<std::string> words_;
};
