#include "model.h"

#include <algorithm>
#include <cstdlib>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

namespace {

std::filesystem::path to_lower_ext(const std::filesystem::path &p) {
  auto ext = p.extension().string();
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return ext;
}

std::vector<std::filesystem::path> enumerate(const std::filesystem::path &dir) {
  std::vector<std::filesystem::path> out;
  std::error_code ec;
  if (!std::filesystem::exists(dir, ec)) {
    return out;
  }
  for (auto &entry : std::filesystem::directory_iterator(dir, ec)) {
    if (ec) {
      break;
    }
    if (!entry.is_regular_file()) {
      continue;
    }
    out.push_back(entry.path());
  }
  return out;
}

} // namespace

ModelManager::ModelManager(std::filesystem::path base_dir)
    : base_dir_(std::move(base_dir)), user_dir_(detect_user_dir()) {
  ensure_dir(user_dir_);
}

void ModelManager::refresh() {
  std::vector<std::filesystem::path> collected;
  for (const auto &p : enumerate(user_dir_)) {
    if (is_supported(p)) {
      collected.push_back(p);
    }
  }
  std::sort(collected.begin(), collected.end());
  collected.erase(std::unique(collected.begin(), collected.end()), collected.end());
  models_.swap(collected);
}

bool ModelManager::open_models_folder() const {
  std::filesystem::path target = user_dir_;

#if defined(_WIN32)
  std::wstring wpath = target.wstring();
  auto hinst = ShellExecuteW(nullptr, L"open", wpath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  return reinterpret_cast<INT_PTR>(hinst) > 32;
#elif defined(__APPLE__)
  std::string cmd = "open \"" + target.string() + "\"";
  return std::system(cmd.c_str()) == 0;
#else
  std::string cmd = "xdg-open \"" + target.string() + "\"";
  return std::system(cmd.c_str()) == 0;
#endif
}

bool ModelManager::is_supported(const std::filesystem::path &path) {
  auto ext = to_lower_ext(path);
  return ext == ".april" || ext == ".onnx" || ext == ".ort";
}

std::filesystem::path ModelManager::detect_user_dir() {
#if defined(_WIN32)
  if (const char *localAppData = std::getenv("LOCALAPPDATA")) {
    return std::filesystem::path(localAppData) / "CoolLiveCaption" / "models";
  }
  if (const char *appData = std::getenv("APPDATA")) {
    return std::filesystem::path(appData) / "CoolLiveCaption" / "models";
  }
  if (const char *userProfile = std::getenv("USERPROFILE")) {
    return std::filesystem::path(userProfile) / "CoolLiveCaption" / "models";
  }
#elif defined(__APPLE__)
  if (const char *home = std::getenv("HOME")) {
    return std::filesystem::path(home) / "Library" / "Application Support" / "com.batterydie.coollivecaption" / "models";
  }
#else
  if (const char *home = std::getenv("HOME")) {
    return std::filesystem::path(home) / ".coollivecaption" / "models";
  }
#endif
  return {};
}

void ModelManager::ensure_dir(const std::filesystem::path &dir) {
  if (dir.empty()) {
    return;
  }
  std::error_code ec;
  std::filesystem::create_directories(dir, ec);
}