#include "profanity.h"

#include <algorithm>
#include <cctype>
#include <fstream>

namespace {
std::string to_lower_trim(std::string s) {
  auto is_space = [](unsigned char c) { return std::isspace(c) != 0; };
  auto first = std::find_if_not(s.begin(), s.end(), is_space);
  auto last = std::find_if_not(s.rbegin(), s.rend(), is_space).base();
  if (first >= last) {
    return {};
  }
  s.assign(first, last);
  std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return s;
}
}  // namespace

bool ProfanityFilter::load(const std::filesystem::path &dir, std::string_view language_code) {
  words_.clear();
  std::string lang(language_code);
  std::transform(lang.begin(), lang.end(), lang.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (lang.empty()) {
    return false;
  }
  auto path = dir / (lang + ".txt");
  if (!std::filesystem::exists(path)) {
    return false;
  }
  std::ifstream in(path);
  if (!in.is_open()) {
    return false;
  }
  std::string line;
  while (std::getline(in, line)) {
    auto cleaned = to_lower_trim(line);
    if (!cleaned.empty()) {
      words_.insert(std::move(cleaned));
    }
  }
  return !words_.empty();
}

std::string ProfanityFilter::filter(std::string_view text) const {
  if (words_.empty()) {
    return std::string(text);
  }
  std::string out;
  out.reserve(text.size());
  std::string token;
  token.reserve(32);

  auto flush = [&]() {
    if (token.empty()) {
      return;
    }
    std::string lower = token;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (words_.find(lower) != words_.end()) {
      out.append(token.size(), '*');
    } else {
      out.append(token);
    }
    token.clear();
  };

  for (char ch : text) {
    unsigned char c = static_cast<unsigned char>(ch);
    if (std::isalnum(c)) {
      token.push_back(ch);
    } else {
      flush();
      out.push_back(ch);
    }
  }
  flush();
  return out;
}
