#pragma once

#include <atomic>
#include <cstddef>
#include <functional>
#include <mutex>
#include <vector>

#ifdef HAVE_PIPEWIRE
struct pw_thread_loop;
struct pw_context;
struct pw_stream;
#endif

class AudioLinux {
public:
  using SampleHandler = std::function<void(const std::vector<float> &)>;
  // source: 0 = loopback (sink monitor), 1 = microphone/default source.
  bool start(size_t sample_rate, int source, SampleHandler handler);
  void stop();

private:
  static void ensure_init();
  static void on_process(void *data);

  SampleHandler handler_;
  size_t sample_rate_ = 0;
  int source_ = 0;
  std::atomic<bool> running_{false};

#ifdef HAVE_PIPEWIRE
  pw_thread_loop *loop_ = nullptr;
  pw_context *context_ = nullptr;
  pw_stream *stream_ = nullptr;
#endif

  std::vector<float> mono_;
};
