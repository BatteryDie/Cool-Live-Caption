#include "audio_mac.h"

bool AudioMac::start(size_t sample_rate, int /*source*/, SampleHandler handler) {
  (void)sample_rate;
  (void)handler;
  return false;
}

void AudioMac::stop() {}
