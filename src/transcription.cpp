#include "transcription.h"

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace {
std::filesystem::path documents_root() {
  if (const char *home = std::getenv("USERPROFILE")) {
    return std::filesystem::path(home) / "Documents";
  }
  if (const char *home = std::getenv("HOME")) {
    return std::filesystem::path(home) / "Documents";
  }
  return std::filesystem::current_path();
}

std::string timestamp() {
  auto now = std::chrono::system_clock::now();
  auto tt = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  localtime_s(&tm, &tt);
#else
  localtime_r(&tt, &tm);
#endif
  std::ostringstream oss;
  oss << std::put_time(&tm, "%Y%m%d-%H%M%S");
  return oss.str();
}

std::string format_elapsed(std::chrono::steady_clock::duration d) {
  using namespace std::chrono;
  auto secs = duration_cast<seconds>(d).count();
  auto h = secs / 3600;
  auto m = (secs % 3600) / 60;
  auto s = secs % 60;
  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(2) << h << ':' << std::setw(2) << m << ':' << std::setw(2) << s;
  return oss.str();
}
}  // namespace

TranscriptionWriter::TranscriptionWriter() {
  auto root = documents_root() / "Cool Live Caption";
  std::filesystem::create_directories(root);
  file_path_ = root / ("transcript-" + timestamp() + ".txt");
  stream_.open(file_path_);
  start_ = std::chrono::steady_clock::now();
  if (stream_.is_open()) {
    auto now = std::chrono::system_clock::now();
    auto tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &tt);
#else
    localtime_r(&tt, &tm);
#endif
    std::ostringstream header;
    header << "Cool Live Caption\n"
           << "Date: " << std::put_time(&tm, "%Y-%m-%d") << "\n\n"
           << "Time: " << std::put_time(&tm, "%H:%M:%S") << "\n\n"
           << "----\n\n";
    stream_ << header.str();
  }
}

void TranscriptionWriter::write_line(std::string_view line) {
  if (stream_.is_open()) {
    auto elapsed = std::chrono::steady_clock::now() - start_;
    stream_ << format_elapsed(elapsed) << " " << line << "\n";
    stream_.flush();
  }
}

const std::filesystem::path &TranscriptionWriter::path() const {
  return file_path_;
}
