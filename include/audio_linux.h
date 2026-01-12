#pragma once

#include <cstddef>
#include <functional>
#include <vector>

class AudioLinux {
public:
  using SampleHandler = std::function<void(const std::vector<float> &)>;
  bool start(size_t sample_rate, int /*source*/, SampleHandler handler);
  void stop();
};
