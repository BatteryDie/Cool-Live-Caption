#include "app_update.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <shellapi.h>
#include <windows.h>
#endif

namespace app_update {
namespace {

constexpr const char *kGithubRepo = GITHUB_REPO;

struct CommandResult {
  int exit_code = -1;
  std::string output;
};

CommandResult run_command(const std::string &cmd) {
  CommandResult res{};
#if defined(_WIN32)
  FILE *pipe = _popen(cmd.c_str(), "r");
#else
  FILE *pipe = popen(cmd.c_str(), "r");
#endif
  if (!pipe) {
    res.exit_code = -1;
    return res;
  }
  std::array<char, 4096> buffer{};
  while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) {
    res.output.append(buffer.data());
  }
#if defined(_WIN32)
  res.exit_code = _pclose(pipe);
#else
  res.exit_code = pclose(pipe);
#endif
  return res;
}

std::string extract_json_field(const std::string &json, std::string_view key) {
  auto pos = json.find(key);
  if (pos == std::string::npos) {
    return {};
  }
  pos = json.find('"', json.find(':', pos));
  if (pos == std::string::npos) {
    return {};
  }
  auto end = json.find('"', pos + 1);
  if (end == std::string::npos || end <= pos + 1) {
    return {};
  }
  return json.substr(pos + 1, end - pos - 1);
}

UpdateResult fetch_latest_release() {
  UpdateResult r{};
  std::string api = std::string("https://api.github.com/repos/") + kGithubRepo + "/releases/latest";
  auto res = run_command(std::string("curl -fsSL ") + api);
  if (res.exit_code != 0 || res.output.empty()) {
    r.error = "Unable to reach GitHub releases.";
    return r;
  }
  r.latest_tag = extract_json_field(res.output, "\"tag_name\"");
  r.latest_url = extract_json_field(res.output, "\"html_url\"");
  if (r.latest_tag.empty() || r.latest_url.empty()) {
    r.error = "Unable to parse release response.";
    return r;
  }
  r.success = true;
  return r;
}

}  // namespace

int compare_versions(const std::string &a, const std::string &b) {
  auto trim_v = [](std::string s) {
    if (!s.empty() && (s[0] == 'v' || s[0] == 'V')) {
      s.erase(s.begin());
    }
    return s;
  };
  auto sa = trim_v(a);
  auto sb = trim_v(b);
  auto split = [](const std::string &s) {
    std::vector<int> parts;
    std::string token;
    for (char c : s) {
      if (c == '.') {
        if (!token.empty()) {
          parts.push_back(std::stoi(token));
          token.clear();
        }
      } else if (std::isdigit(static_cast<unsigned char>(c))) {
        token.push_back(c);
      }
    }
    if (!token.empty()) {
      parts.push_back(std::stoi(token));
    }
    return parts;
  };
  auto va = split(sa);
  auto vb = split(sb);
  size_t count = std::max(va.size(), vb.size());
  va.resize(count, 0);
  vb.resize(count, 0);
  for (size_t i = 0; i < count; ++i) {
    if (va[i] < vb[i]) return -1;
    if (va[i] > vb[i]) return 1;
  }
  return 0;
}

bool open_url(const std::string &url) {
#if defined(_WIN32)
  auto wurl = std::wstring(url.begin(), url.end());
  return reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr, L"open", wurl.c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
#elif defined(__APPLE__)
  auto res = run_command(std::string("open \"") + url + "\"");
  return res.exit_code == 0;
#else
  auto res = run_command(std::string("xdg-open \"") + url + "\" 2>/dev/null");
  return res.exit_code == 0;
#endif
}

void start_update_check(UpdateState &state, bool show_modal) {
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    if (state.checking) {
      if (show_modal) {
        state.show_modal = true;
      }
      return;
    }
    if (state.worker.joinable()) {
      state.worker.join();
    }
    state.show_modal = show_modal;
    state.checking = true;
    state.has_result = false;
    state.result = UpdateResult{};
  }

  state.worker = std::thread([&state]() {
    UpdateResult r = fetch_latest_release();
    std::lock_guard<std::mutex> lock(state.mutex);
    state.result = std::move(r);
    state.has_result = true;
    state.checking = false;
  });
}

void finalize_update_thread(UpdateState &state) {
  std::thread joiner;
  {
    std::lock_guard<std::mutex> lock(state.mutex);
    if (!state.checking && state.worker.joinable()) {
      joiner = std::move(state.worker);
    }
  }
  if (joiner.joinable()) {
    joiner.join();
  }
}

}  // namespace app_update
