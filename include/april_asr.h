#pragma once

#include <filesystem>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <vector>

#include "april_api.h"

class AprilAsrEngine {
public:
  bool load_model(const std::filesystem::path &model_path);
  bool start();
  void stop();
  void push_audio(const std::vector<float> &samples);
  std::optional<std::string> poll_text();
  std::optional<std::string> peek_partial();
  size_t sample_rate() const;

private:
  static void handler_trampoline(void *userdata, AprilResultType result, size_t count,
                                 const AprilToken *tokens);
  void handle_result(AprilResultType result, size_t count, const AprilToken *tokens);

  std::filesystem::path model_path_;
  AprilASRModel model_{nullptr};
  AprilASRSession session_{nullptr};
  size_t sample_rate_{16000};
  std::queue<std::string> pending_;
  std::optional<std::string> partial_;
  std::vector<short> pcm16_buffer_;
  std::mutex mutex_;
};
