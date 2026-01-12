#include "audio_win.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <memory>
#include <vector>

#include <audioclient.h>
#include <combaseapi.h>
#include <ksmedia.h>
#include <mmdeviceapi.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

namespace {
void log_hr(const char *stage, HRESULT hr) {
  std::fprintf(stderr, "[error] WASAPI %s failed: 0x%08lx\n", stage, static_cast<unsigned long>(hr));
}
}

bool AudioWin::start(size_t sample_rate, Source source, SampleHandler handler) {
  if (sample_rate == 0) {
    return false;
  }
  if (running_) {
    return true;
  }
  sample_rate_ = sample_rate;
  source_ = source;
  handler_ = std::move(handler);
  running_ = true;
  worker_ = std::thread(&AudioWin::run_loop, this);
  return true;
}

void AudioWin::stop() {
  running_ = false;
  if (client_) {
    client_->Stop();
  }
  if (capture_event_) {
    SetEvent(capture_event_);
  }
  if (worker_.joinable()) {
    worker_.join();
  }
  client_.Reset();
  if (capture_event_) {
    CloseHandle(capture_event_);
    capture_event_ = nullptr;
  }
}

bool AudioWin::running() const {
  return running_.load();
}

void AudioWin::run_loop() {
  HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  if (FAILED(hr)) {
    log_hr("CoInitializeEx", hr);
    running_ = false;
    return;
  }
  auto co_scope = std::unique_ptr<void, void (*)(void *)>(nullptr, [](void *) { CoUninitialize(); });

  ComPtr<IMMDeviceEnumerator> enumerator;
  hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL, IID_PPV_ARGS(&enumerator));
  if (FAILED(hr)) {
    log_hr("CoCreateInstance(MMDeviceEnumerator)", hr);
    running_ = false;
    return;
  }

  EDataFlow flow = source_ == Source::Loopback ? eRender : eCapture;
  const char *source_label = source_ == Source::Loopback ? "loopback" : "microphone";
  ComPtr<IMMDevice> device;
  const ERole roles[3] = {eConsole, eMultimedia, eCommunications};
  bool got_device = false;
  for (ERole role : roles) {
    if (SUCCEEDED(enumerator->GetDefaultAudioEndpoint(flow, role, &device))) {
      got_device = true;
      break;
    }
  }
  if (!got_device) {
    std::fprintf(stderr, "[error] WASAPI no default %s device found\n", source_label);
    running_ = false;
    return;
  }

  ComPtr<IAudioClient> client;
  hr = device->Activate(__uuidof(IAudioClient), CLSCTX_ALL, nullptr, reinterpret_cast<void **>(client.GetAddressOf()));
  if (FAILED(hr)) {
    log_hr("Activate(IAudioClient)", hr);
    running_ = false;
    return;
  }
  client_ = client;

  WAVEFORMATEX *mix_raw = nullptr;
  hr = client->GetMixFormat(&mix_raw);
  if (FAILED(hr) || !mix_raw) {
    log_hr("GetMixFormat", hr);
    running_ = false;
    return;
  }
  std::unique_ptr<WAVEFORMATEX, decltype(&CoTaskMemFree)> mix(mix_raw, &CoTaskMemFree);

  const bool is_float = mix->wFormatTag == WAVE_FORMAT_IEEE_FLOAT ||
                        (mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                         reinterpret_cast<WAVEFORMATEXTENSIBLE *>(mix.get())->SubFormat == KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
  const bool is_pcm16 = mix->wFormatTag == WAVE_FORMAT_PCM && mix->wBitsPerSample == 16;
  const bool is_ext16 = mix->wFormatTag == WAVE_FORMAT_EXTENSIBLE &&
                        reinterpret_cast<WAVEFORMATEXTENSIBLE *>(mix.get())->SubFormat == KSDATAFORMAT_SUBTYPE_PCM &&
                        mix->wBitsPerSample == 16;
  const WORD channels = mix->nChannels;
  const UINT32 mix_rate = mix->nSamplesPerSec;
  if (channels == 0) {
    running_ = false;
    return;
  }

  const REFERENCE_TIME hns_buffer = 100 * 10000; // 100 ms
  DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK |
                AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM |
                AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
  if (source_ == Source::Loopback) {
    flags |= AUDCLNT_STREAMFLAGS_LOOPBACK;
  }
  hr = client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, hns_buffer, 0, mix.get(), nullptr);
  if (FAILED(hr)) {
    log_hr("Initialize", hr);
    running_ = false;
    return;
  }

  ComPtr<IAudioCaptureClient> capture;
  hr = client->GetService(IID_PPV_ARGS(&capture));
  if (FAILED(hr)) {
    log_hr("GetService(IAudioCaptureClient)", hr);
    running_ = false;
    return;
  }

  HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
  if (!event) {
    std::fprintf(stderr, "[error] WASAPI CreateEvent failed\n");
    running_ = false;
    return;
  }
  hr = client->SetEventHandle(event);
  if (FAILED(hr)) {
    log_hr("SetEventHandle", hr);
    CloseHandle(event);
    running_ = false;
    return;
  }
  capture_event_ = event;

  hr = client->Start();
  if (FAILED(hr)) {
    log_hr("Start", hr);
    running_ = false;
    return;
  }

  std::fprintf(stdout, "[info] WASAPI started (%s, mix %u Hz, %u ch -> %zu Hz)\n", source_label,
               mix_rate, static_cast<unsigned int>(channels), sample_rate_);

  const double step = static_cast<double>(mix_rate) / static_cast<double>(sample_rate_);

  std::vector<float> buffer;
  std::vector<float> mono;
  bool logged_first_packet = false;

  while (running_) {
    DWORD wait = WaitForSingleObject(capture_event_, 200);
    if (wait != WAIT_OBJECT_0) {
      continue;
    }

    UINT32 packet = 0;
    if (FAILED(capture->GetNextPacketSize(&packet))) {
      break;
    }
    if (packet == 0) {
      continue;
    }

    BYTE *data = nullptr;
    UINT32 frames = 0;
    DWORD capture_flags = 0;
    hr = capture->GetBuffer(&data, &frames, &capture_flags, nullptr, nullptr);
    if (FAILED(hr)) {
      break;
    }

    const size_t samples = static_cast<size_t>(frames) * channels;
    buffer.resize(samples);

    const bool silent = (capture_flags & AUDCLNT_BUFFERFLAGS_SILENT) || data == nullptr;
    if (silent) {
      std::fill(buffer.begin(), buffer.end(), 0.0f);
    } else if (is_float) {
      const float *src = reinterpret_cast<const float *>(data);
      std::copy(src, src + samples, buffer.begin());
    } else if (is_pcm16 || is_ext16) {
      const int16_t *src = reinterpret_cast<const int16_t *>(data);
      constexpr float scale = 1.0f / 32768.0f;
      for (size_t i = 0; i < samples; ++i) {
        buffer[i] = static_cast<float>(src[i]) * scale;
      }
    } else {
      std::fill(buffer.begin(), buffer.end(), 0.0f);
    }

    mono.resize(frames);
    for (UINT32 i = 0; i < frames; ++i) {
      float sum = 0.0f;
      for (UINT32 ch = 0; ch < channels; ++ch) {
        sum += buffer[i * channels + ch];
      }
      mono[i] = sum / static_cast<float>(channels);
    }

    std::vector<float> output;
    if (sample_rate_ == mix_rate) {
      output = mono;
    } else {
      const size_t out_frames = std::max<size_t>(1, static_cast<size_t>(std::round(mono.size() / step)));
      output.resize(out_frames);
      for (size_t i = 0; i < out_frames; ++i) {
        double src_pos = static_cast<double>(i) * step;
        size_t idx = static_cast<size_t>(src_pos);
        double frac = src_pos - static_cast<double>(idx);
        float s0 = mono[std::min(idx, mono.size() - 1)];
        float s1 = mono[std::min(idx + 1, mono.size() - 1)];
        output[i] = s0 + static_cast<float>(frac) * (s1 - s0);
      }
    }

    if (!silent && !logged_first_packet) {
      std::fprintf(stdout, "[info] WASAPI captured first packet (%u frames)\n", frames);
      logged_first_packet = true;
    }

    if (handler_ && !output.empty()) {
      handler_(output);
    }

    capture->ReleaseBuffer(frames);
  }

  client->Stop();
  client_.Reset();
  if (capture_event_) {
    CloseHandle(capture_event_);
    capture_event_ = nullptr;
  }
}
