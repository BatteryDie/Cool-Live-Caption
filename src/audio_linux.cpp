#include "audio_linux.h"

bool AudioLinux::start(size_t sample_rate, int /*source*/, SampleHandler handler) {
  (void)sample_rate;
  (void)handler;
  return false;
}

void AudioLinux::stop() {}
