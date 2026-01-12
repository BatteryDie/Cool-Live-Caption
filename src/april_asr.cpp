#include "april_asr.h"

#include <algorithm>
#include <cmath>
#include <mutex>

namespace {
std::once_flag g_once;
}

bool AprilAsrEngine::load_model(const std::filesystem::path &model_path) {
  stop();

  std::call_once(g_once, [] { aam_api_init(APRIL_VERSION); });

  model_path_ = model_path;
  if (!std::filesystem::exists(model_path_)) {
    return false;
  }

  model_ = aam_create_model(model_path_.string().c_str());
  if (!model_) {
    return false;
  }

  sample_rate_ = aam_get_sample_rate(model_);
  return true;
}

bool AprilAsrEngine::start() {
  if (!model_) {
    return false;
  }

  AprilConfig cfg{};
  cfg.handler = &AprilAsrEngine::handler_trampoline;
  cfg.userdata = this;
  cfg.flags = APRIL_CONFIG_FLAG_ASYNC_RT_BIT;

  session_ = aas_create_session(model_, cfg);
  return session_ != nullptr;
}

void AprilAsrEngine::stop() {
  if (session_) {
    aas_flush(session_);
    aas_free(session_);
    session_ = nullptr;
  }
  if (model_) {
    aam_free(model_);
    model_ = nullptr;
  }

  std::scoped_lock lock(mutex_);
  pending_ = std::queue<std::string>();
  partial_.reset();
}

void AprilAsrEngine::push_audio(const std::vector<float> &samples) {
  if (!session_ || samples.empty()) {
    return;
  }

  pcm16_buffer_.resize(samples.size());
  for (std::size_t i = 0; i < samples.size(); ++i) {
    float clamped = std::clamp(samples[i], -1.0f, 1.0f);
    pcm16_buffer_[i] = static_cast<short>(std::lrintf(clamped * 32767.0f));
  }

  aas_feed_pcm16(session_, pcm16_buffer_.data(), pcm16_buffer_.size());
}

std::optional<std::string> AprilAsrEngine::poll_text() {
  std::scoped_lock lock(mutex_);
  if (pending_.empty()) {
    return std::nullopt;
  }
  auto out = pending_.front();
  pending_.pop();
  return out;
}

std::optional<std::string> AprilAsrEngine::peek_partial() {
  std::scoped_lock lock(mutex_);
  if (!partial_) {
    return std::nullopt;
  }
  return *partial_;
}

size_t AprilAsrEngine::sample_rate() const {
  return sample_rate_;
}

void AprilAsrEngine::handler_trampoline(void *userdata, AprilResultType result, size_t count,
                                        const AprilToken *tokens) {
  auto *self = static_cast<AprilAsrEngine *>(userdata);
  if (!self) {
    return;
  }
  self->handle_result(result, count, tokens);
}

void AprilAsrEngine::handle_result(AprilResultType result, size_t count, const AprilToken *tokens) {
  if (result == APRIL_RESULT_RECOGNITION_PARTIAL) {
    std::string text;
    for (size_t i = 0; i < count; ++i) {
      text.append(tokens[i].token ? tokens[i].token : "");
    }
    std::scoped_lock lock(mutex_);
    partial_ = std::move(text);
    return;
  }

  if (result == APRIL_RESULT_RECOGNITION_FINAL) {
    if (count == 0 || !tokens) {
      return;
    }

    std::string text;
    for (size_t i = 0; i < count; ++i) {
      text.append(tokens[i].token ? tokens[i].token : "");
    }

    if (!text.empty()) {
      std::scoped_lock lock(mutex_);
      pending_.push(std::move(text));
      partial_.reset();
    }
    return;
  }
}
