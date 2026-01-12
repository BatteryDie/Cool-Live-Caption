#pragma once

#include <filesystem>
#include <vector>

class ModelManager {
public:
  explicit ModelManager(std::filesystem::path base_dir);

  void refresh();
  const std::vector<std::filesystem::path> &models() const { return models_; }

  const std::filesystem::path &user_dir() const { return user_dir_; }

  bool open_models_folder() const;

private:
  static bool is_supported(const std::filesystem::path &path);
  static std::filesystem::path detect_user_dir();
  static void ensure_dir(const std::filesystem::path &dir);

  std::vector<std::filesystem::path> models_;
  std::filesystem::path base_dir_;
  std::filesystem::path user_dir_;
};