#include "audio_linux.h"

#ifdef HAVE_PIPEWIRE

#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/audio/raw.h>
#include <spa/param/props.h>

namespace {
const int kChannels = 2;
}

void AudioLinux::ensure_init() {
  static std::once_flag once;
  std::call_once(once, [] { pw_init(nullptr, nullptr); });
}

bool AudioLinux::start(size_t sample_rate, int source, SampleHandler handler) {
  if (sample_rate == 0 || !handler) {
    return false;
  }
  if (running_) {
    return true;
  }

  ensure_init();

  handler_ = std::move(handler);
  sample_rate_ = sample_rate;
  source_ = source;

  loop_ = pw_thread_loop_new("coollivecaption-audio", nullptr);
  if (!loop_) {
    return false;
  }

  context_ = pw_context_new(pw_thread_loop_get_loop(loop_), nullptr, 0);
  if (!context_) {
    pw_thread_loop_destroy(loop_);
    loop_ = nullptr;
    return false;
  }

  struct pw_core *core = pw_context_connect(context_, nullptr, 0);
  if (!core) {
    pw_context_destroy(context_);
    pw_thread_loop_destroy(loop_);
    context_ = nullptr;
    loop_ = nullptr;
    return false;
  }

  const char *capture_sink = source_ == 0 ? "true" : "false";
  pw_properties *props = pw_properties_new(nullptr, nullptr);
  if (!props) {
    pw_core_disconnect(core);
    pw_context_destroy(context_);
    pw_thread_loop_destroy(loop_);
    context_ = nullptr;
    loop_ = nullptr;
    return false;
  }
  pw_properties_set(props, PW_KEY_MEDIA_TYPE, "Audio");
  pw_properties_set(props, PW_KEY_MEDIA_CATEGORY, "Capture");
  pw_properties_set(props, PW_KEY_MEDIA_ROLE, "Communication");
  pw_properties_set(props, PW_KEY_STREAM_CAPTURE_SINK, capture_sink);

  static const pw_stream_events stream_events = {
      .version = PW_VERSION_STREAM_EVENTS,
      .process = &AudioLinux::on_process,
  };

  stream_ = pw_stream_new_simple(pw_thread_loop_get_loop(loop_),
                                 "CoolLiveCaption Capture",
                                 props,
                                 &stream_events,
                                 this);
  if (!stream_) {
    pw_core_disconnect(core);
    pw_context_destroy(context_);
    pw_thread_loop_destroy(loop_);
    pw_properties_free(props);
    context_ = nullptr;
    loop_ = nullptr;
    return false;
  }

  pw_core_disconnect(core);

  spa_audio_info_raw info{};
  info.format = SPA_AUDIO_FORMAT_F32;
  info.rate = static_cast<uint32_t>(sample_rate_);
  info.channels = kChannels;
  info.position[0] = SPA_AUDIO_CHANNEL_FL;
  info.position[1] = SPA_AUDIO_CHANNEL_FR;

  uint8_t buffer[256];
  spa_pod_builder b = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
  const spa_pod *params[] = {spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &info)};

  running_ = true;
  pw_thread_loop_lock(loop_);
  pw_thread_loop_start(loop_);
  int res = pw_stream_connect(stream_, PW_DIRECTION_INPUT, PW_ID_ANY,
                              static_cast<pw_stream_flags>(PW_STREAM_FLAG_AUTOCONNECT | PW_STREAM_FLAG_MAP_BUFFERS | PW_STREAM_FLAG_RT_PROCESS),
                              params, 1);
  pw_thread_loop_unlock(loop_);

  if (res < 0) {
    stop();
    return false;
  }

  return true;
}

void AudioLinux::stop() {
  running_ = false;

  if (loop_) {
    pw_thread_loop_lock(loop_);
    if (stream_) {
      pw_stream_disconnect(stream_);
    }
    pw_thread_loop_unlock(loop_);
    pw_thread_loop_stop(loop_);
  }

  if (stream_) {
    pw_stream_destroy(stream_);
    stream_ = nullptr;
  }

  if (context_) {
    pw_context_destroy(context_);
    context_ = nullptr;
  }

  if (loop_) {
    pw_thread_loop_destroy(loop_);
    loop_ = nullptr;
  }
}

void AudioLinux::on_process(void *data) {
  auto *self = static_cast<AudioLinux *>(data);
  if (!self || !self->stream_ || !self->handler_) {
    return;
  }

  pw_buffer *buffer = pw_stream_dequeue_buffer(self->stream_);
  if (!buffer) {
    return;
  }
  spa_buffer *b = buffer->buffer;
  if (!b || b->n_datas == 0) {
    pw_stream_queue_buffer(self->stream_, buffer);
    return;
  }

  spa_data *d = b->datas;
  if (!d->data) {
    pw_stream_queue_buffer(self->stream_, buffer);
    return;
  }

  const spa_chunk *c = d->chunk;
  uint32_t offset = c ? c->offset : 0;
  uint32_t size = c ? c->size : d->maxsize;
  uint32_t stride = c && c->stride ? c->stride : static_cast<uint32_t>(sizeof(float) * kChannels);
  if (stride == 0 || size == 0) {
    pw_stream_queue_buffer(self->stream_, buffer);
    return;
  }

  uint8_t *data_ptr = static_cast<uint8_t *>(d->data) + offset;
  uint32_t frames = size / stride;
  if (frames == 0) {
    pw_stream_queue_buffer(self->stream_, buffer);
    return;
  }

  const float *interleaved = reinterpret_cast<const float *>(data_ptr);
  self->mono_.resize(frames);
  for (uint32_t i = 0; i < frames; ++i) {
    float l = interleaved[i * kChannels + 0];
    float r = interleaved[i * kChannels + 1];
    self->mono_[i] = 0.5f * (l + r);
  }

  if (!self->mono_.empty()) {
    self->handler_(self->mono_);
  }

  pw_stream_queue_buffer(self->stream_, buffer);
}

#else

bool AudioLinux::start(size_t sample_rate, int source, SampleHandler handler) {
  (void)sample_rate;
  (void)source;
  (void)handler;
  return false;
}

void AudioLinux::stop() {}

void AudioLinux::ensure_init() {}
void AudioLinux::on_process(void *) {}

#endif
