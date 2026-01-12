#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <thread>
#include <vector>

#include <audioclient.h>
#include <wrl/client.h>

class AudioWin {
public:
  enum class Source { Loopback, Microphone };
  using SampleHandler = std::function<void(const std::vector<float> &)>;
  bool start(size_t sample_rate, Source source, SampleHandler handler);
  void stop();
  bool running() const;

private:
  void run_loop();

  SampleHandler handler_;
  size_t sample_rate_ = 0;
  Source source_ = Source::Loopback;
  std::atomic<bool> running_{false};
  std::thread worker_;
  Microsoft::WRL::ComPtr<IAudioClient> client_;
  HANDLE capture_event_ = nullptr;
};
