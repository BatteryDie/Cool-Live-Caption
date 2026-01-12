#pragma once

#include <filesystem>
#include <fstream>
#include <optional>
#include <string_view>
#include <chrono>

class TranscriptionWriter {
public:
  TranscriptionWriter();
  void write_line(std::string_view line);
  const std::filesystem::path &path() const;

private:
  std::filesystem::path file_path_;
  std::ofstream stream_;
  std::chrono::steady_clock::time_point start_;
};
