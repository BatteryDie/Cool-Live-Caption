#include <filesystem>
#include <cstdlib>
#include <optional>
#include <algorithm>
#include <cstdio>
#include <system_error>
#include <string>
#include <string_view>
#include <vector>
#include <fstream>
#include <cctype>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#if defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

#include "april_asr.h"
#include "caption.h"
#include "transcription.h"
#include "model.h"
#include "profanity.h"
#include "app_update.h"

#if defined(_WIN32)
#include "audio_win.h"
using AudioBackend = AudioWin;
#elif defined(__APPLE__)
#include "audio_mac.h"
using AudioBackend = AudioMac;
#else
#include "audio_linux.h"
using AudioBackend = AudioLinux;
#endif

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

namespace {

void log_info(const std::string &msg) {
  std::fprintf(stdout, "[info] %s\n", msg.c_str());
}
constexpr const char *kAppVersion = APP_VERSION_STRING;
constexpr const char *kAppVersionTag = APP_VERSION_TAG;

void log_error(const std::string &msg) {
  std::fprintf(stderr, "[error] %s\n", msg.c_str());
}

std::filesystem::path user_config_dir(const std::filesystem::path &fallback) {
#if defined(_WIN32)
  if (const char *local = std::getenv("LOCALAPPDATA")) {
    return std::filesystem::path(local) / "CoolLiveCaptions";
  }
#endif
  if (const char *xdg = std::getenv("XDG_CONFIG_HOME")) {
    return std::filesystem::path(xdg) / "CoolLiveCaptions";
  }
  if (const char *home = std::getenv("HOME")) {
    return std::filesystem::path(home) / ".config" / "CoolLiveCaptions";
  }
  return fallback;
}

struct AppSettings {
  bool always_on_top = true;
  float font_size_px = 26.0f;
  bool auto_scroll = true;
  bool break_lines = true;
  bool profanity_filter = false;
  bool lower_case = true;
  bool auto_check_updates = true;
};

std::string detect_language_from_model(const std::filesystem::path &model_path) {
  auto name = model_path.filename().string();
  auto dot = name.find_last_of('.');
  if (dot == std::string::npos) {
    dot = name.size();
  }
  auto underscore = name.find_last_of('_', dot);
  std::string code;
  if (underscore != std::string::npos && underscore + 1 < dot) {
    code = name.substr(underscore + 1, dot - underscore - 1);
  }
  std::transform(code.begin(), code.end(), code.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  if (code.size() == 2 || code.size() == 3) {
    return code;
  }
  return "en";
}

std::string apply_lower_case(std::string_view text) {
  std::string out;
  out.reserve(text.size());
  bool cap_next = true;
  for (char ch : text) {
    unsigned char c = static_cast<unsigned char>(ch);
    bool is_alpha = std::isalpha(c) != 0;
    char lowered = static_cast<char>(std::tolower(c));
    char emit = lowered;
    if (is_alpha && cap_next) {
      emit = static_cast<char>(std::toupper(c));
      cap_next = false;
    }
    out.push_back(emit);
    if (ch == '.' || ch == '!' || ch == '?' || ch == '\n') {
      cap_next = true;
    } else if (is_alpha) {
      cap_next = false;
    }
  }
  return out;
}

std::filesystem::path configure_imgui_ini(ImGuiIO &io, const std::filesystem::path &exe_path) {
  static std::string ini_path;
  auto window_dir = user_config_dir(exe_path);
  std::error_code ec;
  std::filesystem::create_directories(window_dir, ec);
  auto target = window_dir / "window.ini";
  ini_path = target.string();
  io.IniFilename = ini_path.c_str();
  return window_dir;
}

std::filesystem::path settings_file(const std::filesystem::path &config_dir) {
  std::error_code ec;
  std::filesystem::create_directories(config_dir, ec);
  return config_dir / "settings.ini";
}

void load_settings(const std::filesystem::path &path, AppSettings &settings) {
  if (!std::filesystem::exists(path)) {
    return;
  }
  std::ifstream in(path);
  std::string line;
  while (std::getline(in, line)) {
    if (line.rfind("always_on_top=", 0) == 0) {
      settings.always_on_top = line.find("=1") != std::string::npos;
    } else if (line.rfind("font_size=", 0) == 0) {
      try {
        settings.font_size_px = std::stof(line.substr(std::string("font_size=").size()));
      } catch (...) {
      }
    } else if (line.rfind("auto_scroll=", 0) == 0) {
      settings.auto_scroll = line.find("=1") != std::string::npos;
    } else if (line.rfind("break_lines=", 0) == 0) {
      settings.break_lines = line.find("=1") != std::string::npos;
    } else if (line.rfind("profanity_filter=", 0) == 0) {
      settings.profanity_filter = line.find("=1") != std::string::npos;
    } else if (line.rfind("lower_case=", 0) == 0) {
      settings.lower_case = line.find("=1") != std::string::npos;
    } else if (line.rfind("auto_check_updates=", 0) == 0) {
      settings.auto_check_updates = line.find("=1") != std::string::npos;
    }
  }
}

void save_settings(const std::filesystem::path &path, const AppSettings &settings) {
  std::vector<std::string> lines;
  if (std::filesystem::exists(path)) {
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
      if (line.rfind("always_on_top=", 0) == 0 || line.rfind("font_size=", 0) == 0 ||
          line.rfind("auto_scroll=", 0) == 0 || line.rfind("break_lines=", 0) == 0 ||
          line.rfind("profanity_filter=", 0) == 0 || line.rfind("lower_case=", 0) == 0 ||
          line.rfind("auto_check_updates=", 0) == 0) {
        continue;
      }
      lines.push_back(line);
    }
  }
  lines.push_back(std::string("always_on_top=") + (settings.always_on_top ? "1" : "0"));
  lines.push_back(std::string("font_size=") + std::to_string(settings.font_size_px));
  lines.push_back(std::string("auto_scroll=") + (settings.auto_scroll ? "1" : "0"));
  lines.push_back(std::string("break_lines=") + (settings.break_lines ? "1" : "0"));
  lines.push_back(std::string("profanity_filter=") + (settings.profanity_filter ? "1" : "0"));
  lines.push_back(std::string("lower_case=") + (settings.lower_case ? "1" : "0"));
  lines.push_back(std::string("auto_check_updates=") + (settings.auto_check_updates ? "1" : "0"));
  std::ofstream out(path, std::ios::trunc);
  for (const auto &l : lines) {
    out << l << '\n';
  }
}

void configure_style() {
  ImGui::StyleColorsDark();
  auto &style = ImGui::GetStyle();
  style.WindowRounding = 2.0f;
}

void configure_fonts(const std::filesystem::path &base, float size) {
  ImGuiIO &io = ImGui::GetIO();
  (void)base;
  ImFontConfig cfg;
  cfg.SizePixels = size;
#if defined(_WIN32)
  const char *segoe_path = "C:/Windows/Fonts/segoeui.ttf";
  if (std::filesystem::exists(segoe_path)) {
    io.Fonts->AddFontFromFileTTF(segoe_path, size, &cfg);
    return;
  }
#elif defined(__APPLE__)
  const char *sf_paths[] = {
    "/System/Library/Fonts/SFNS.ttf",
    "/System/Library/Fonts/SFNSText.ttf",
    "/System/Library/Fonts/SF-Pro-Text-Regular.otf"
  };
  for (const char *path : sf_paths) {
    if (std::filesystem::exists(path)) {
      io.Fonts->AddFontFromFileTTF(path, size, &cfg);
      return;
    }
  }
#else
  const char *linux_paths[] = {
    "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/dejavu/DejaVuSans.ttf",
    "/usr/share/fonts/liberation/LiberationSans-Regular.ttf"
  };
  for (const char *path : linux_paths) {
    if (std::filesystem::exists(path)) {
      io.Fonts->AddFontFromFileTTF(path, size, &cfg);
      return;
    }
  }
#endif
  io.Fonts->AddFontDefault(&cfg);
}

bool open_folder(const std::filesystem::path &path) {
#if defined(_WIN32)
  std::wstring wpath = path.wstring();
  auto hinst = ShellExecuteW(nullptr, L"open", wpath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
  return reinterpret_cast<INT_PTR>(hinst) > 32;
#elif defined(__APPLE__)
  std::string cmd = "open \"" + path.string() + "\"";
  return std::system(cmd.c_str()) == 0;
#else
  std::string cmd = "xdg-open \"" + path.string() + "\"";
  return std::system(cmd.c_str()) == 0;
#endif
}
} // namespace
enum class AudioSourceKind { Desktop, Microphone };

int run_app(int argc, char **argv) {
  (void)argc;

  if (!glfwInit()) {
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
  glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
#if defined(__linux__) && !defined(__APPLE__)
  glfwWindowHintString(GLFW_WAYLAND_APP_ID, "coollivecaptions");
  glfwWindowHintString(GLFW_X11_CLASS_NAME, "CoolLiveCaptions");
  glfwWindowHintString(GLFW_X11_INSTANCE_NAME, "CoolLiveCaptions");
#endif

  GLFWwindow *window = glfwCreateWindow(1280, 720, "Cool Live Captions", nullptr, nullptr);
  if (!window) {
    glfwTerminate();
    return 1;
  }
  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  bool refresh_models = false;
  glfwSetWindowUserPointer(window, &refresh_models);
  glfwSetWindowFocusCallback(window, [](GLFWwindow *win, int focused) {
    if (focused != 0) {
      if (auto flag = static_cast<bool *>(glfwGetWindowUserPointer(win))) {
        *flag = true;
      }
    }
  });

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    glfwDestroyWindow(window);
    glfwTerminate();
    return 1;
  }

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui_ImplGlfw_InitForOpenGL(window, true);
  ImGui_ImplOpenGL3_Init("#version 130");

  auto exe_path = std::filesystem::absolute(argv[0]).parent_path();
  auto window_dir = configure_imgui_ini(io, exe_path);
  auto settings_path = settings_file(window_dir);
  AppSettings settings{};
  load_settings(settings_path, settings);
  ModelManager model_manager(exe_path);
  model_manager.refresh();
  configure_fonts(exe_path, settings.font_size_px);
  configure_style();
  glfwSetWindowAttrib(window, GLFW_FLOATING, settings.always_on_top ? GLFW_TRUE : GLFW_FALSE);

  auto profanity_dir = exe_path / "profanity";

  CaptionView caption;
  TranscriptionWriter writer;
  AprilAsrEngine engine;
  AudioBackend audio;
  AudioSourceKind audio_source = AudioSourceKind::Desktop;
  ProfanityFilter profanity;
  app_update::UpdateState update_state;
  if (settings.auto_check_updates) {
    app_update::start_update_check(update_state, false);
  }

  auto models = model_manager.models();
  std::optional<std::filesystem::path> active_model;
  bool engine_ready = false;
  if (!models.empty()) {
    active_model = models.front();
    caption.set_active_model(active_model->filename().string());
    engine_ready = engine.load_model(*active_model) && engine.start();
    if (engine_ready) {
      log_info("Loaded model: " + active_model->filename().string());
    } else {
      log_error("Failed to load model: " + active_model->filename().string());
    }
    if (!profanity.load(profanity_dir, detect_language_from_model(active_model->filename()))) {
      log_error("Profanity list not found for model language: " + active_model->filename().string());
    }
  }
  if (!active_model) {
    log_error("No caption models found. Add .april/.onnx/.ort files to models/.");
  }

  auto start_audio = [&]() {
    if (!engine_ready) {
      return;
    }
#if defined(_WIN32)
    auto src = audio_source == AudioSourceKind::Desktop ? AudioBackend::Source::Loopback : AudioBackend::Source::Microphone;
#else
    int src = 0;
#endif
    log_info(std::string("Starting audio: ") + (audio_source == AudioSourceKind::Desktop ? "Desktop" : "Microphone") +
             ", model rate " + std::to_string(engine.sample_rate()));
    audio.start(engine.sample_rate(), src, [&](const std::vector<float> &samples) { engine.push_audio(samples); });
  };

  if (engine_ready) {
    start_audio();
  }

  bool rebuild_fonts = false;
  float pending_font_size = settings.font_size_px;
  bool auto_scroll_enabled = settings.auto_scroll;
  bool profanity_filter_enabled = settings.profanity_filter;
  bool lower_case_enabled = settings.lower_case;

  while (!glfwWindowShouldClose(window)) {
    app_update::finalize_update_thread(update_state);
    glfwPollEvents();

    if (refresh_models) {
      refresh_models = false;
      model_manager.refresh();
      auto updated = model_manager.models();
      if (!updated.empty()) {
        if (!active_model || std::find(updated.begin(), updated.end(), *active_model) == updated.end()) {
          active_model = updated.front();
          caption.clear();
          caption.set_active_model(active_model->filename().string());
          audio.stop();
          engine.stop();
          engine_ready = engine.load_model(*active_model) && engine.start();
          if (engine_ready) {
            log_info("Loaded model: " + active_model->filename().string());
            if (!profanity.load(profanity_dir, detect_language_from_model(active_model->filename()))) {
              log_error("Profanity list not found for model language: " + active_model->filename().string());
            }
            start_audio();
          } else {
            log_error("Failed to load model: " + active_model->filename().string());
          }
        }
      } else {
        active_model.reset();
        caption.clear();
        engine.stop();
        engine_ready = false;
        audio.stop();
        log_error("No caption models found. Add .april/.onnx/.ort files to models/.");
      }
      models = std::move(updated);
    }

    if (auto text = engine.poll_text()) {
      auto normalized = lower_case_enabled ? apply_lower_case(*text) : *text;
      auto filtered = profanity_filter_enabled ? profanity.filter(normalized) : normalized;
      if (settings.break_lines && !caption.buffer().empty()) {
        caption.append("\n");
      }
      caption.append(filtered);
      writer.write_line(filtered);
    }
    auto partial_raw = engine.peek_partial();
    std::optional<std::string> partial_filtered;
    if (partial_raw && !partial_raw->empty()) {
      auto normalized_partial = lower_case_enabled ? apply_lower_case(*partial_raw) : *partial_raw;
      partial_filtered = profanity_filter_enabled ? profanity.filter(normalized_partial) : normalized_partial;
    }

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("Audio Sources")) {
        if (ImGui::MenuItem("Desktop Audio", nullptr, audio_source == AudioSourceKind::Desktop)) {
          audio_source = AudioSourceKind::Desktop;
          audio.stop();
          start_audio();
        }
        if (ImGui::MenuItem("Microphone", nullptr, audio_source == AudioSourceKind::Microphone)) {
          audio_source = AudioSourceKind::Microphone;
          audio.stop();
          start_audio();
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Caption Models")) {
        if (ImGui::MenuItem("Open Models Folder")) {
          model_manager.open_models_folder();
        }
        ImGui::Separator();
        for (std::size_t i = 0; i < models.size(); ++i) {
          bool selected = active_model && *active_model == models[i];
          if (ImGui::MenuItem(models[i].filename().string().c_str(), nullptr, selected)) {
            active_model = models[i];
            caption.clear();
            caption.set_active_model(models[i].filename().string());
            audio.stop();
            engine.stop();
            engine_ready = engine.load_model(*active_model) && engine.start();
            if (engine_ready) {
              log_info("Loaded model: " + active_model->filename().string());
              if (!profanity.load(profanity_dir, detect_language_from_model(active_model->filename()))) {
                log_error("Profanity list not found for model language: " + active_model->filename().string());
              }
              start_audio();
            } else {
              log_error("Failed to load model: " + active_model->filename().string());
            }
          }
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Settings")) {
        ImGui::TextDisabled("System");
        bool auto_update_menu = settings.auto_check_updates;
        if (ImGui::MenuItem("Automatically Check for Updates", nullptr, auto_update_menu)) {
          settings.auto_check_updates = !auto_update_menu;
          save_settings(settings_path, settings);
          if (settings.auto_check_updates) {
            app_update::start_update_check(update_state, false);
          }
        }
        if (ImGui::MenuItem("Check for Updates now...")) {
          app_update::start_update_check(update_state, true);
        }
        ImGui::Separator();

        ImGui::TextDisabled("Windows");
        bool atop = settings.always_on_top;
        if (ImGui::MenuItem("Always On Top", nullptr, atop)) {
          settings.always_on_top = !atop;
          glfwSetWindowAttrib(window, GLFW_FLOATING, settings.always_on_top ? GLFW_TRUE : GLFW_FALSE);
          save_settings(settings_path, settings);
        }
        ImGui::Separator();

        ImGui::TextDisabled("Captions");
        bool auto_scroll_menu = auto_scroll_enabled;
        if (ImGui::MenuItem("Auto Scroll", nullptr, auto_scroll_menu)) {
          auto_scroll_enabled = !auto_scroll_menu;
          settings.auto_scroll = auto_scroll_enabled;
          save_settings(settings_path, settings);
        }
        bool break_lines_menu = settings.break_lines;
        if (ImGui::MenuItem("Break Lines", nullptr, break_lines_menu)) {
          settings.break_lines = !break_lines_menu;
          save_settings(settings_path, settings);
        }
        bool lower_case_menu = lower_case_enabled;
        if (ImGui::MenuItem("Lower Case Text", nullptr, lower_case_menu)) {
          lower_case_enabled = !lower_case_menu;
          settings.lower_case = lower_case_enabled;
          save_settings(settings_path, settings);
        }
        if (ImGui::BeginMenu("Text Size")) {
          const struct { const char *label; float px; } sizes[] = {
              {"Normal", 26.0f},
              {"Large", 30.0f},
              {"Extra Large", 34.0f},
              {"Extra Extra Large", 38.0f},
          };
          for (const auto &opt : sizes) {
            bool selected = std::abs(settings.font_size_px - opt.px) < 0.5f;
            if (ImGui::MenuItem(opt.label, nullptr, selected)) {
              pending_font_size = opt.px;
              rebuild_fonts = true;
            }
          }
          ImGui::EndMenu();
        }
        ImGui::Separator();

        ImGui::TextDisabled("Extras");
        bool profanity_menu = profanity_filter_enabled;
        if (ImGui::MenuItem("Profanity Filter", nullptr, profanity_menu)) {
          profanity_filter_enabled = !profanity_menu;
          settings.profanity_filter = profanity_filter_enabled;
          save_settings(settings_path, settings);
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }

    bool open_update_popup = false;
    {
      std::lock_guard<std::mutex> lock(update_state.mutex);
      if (update_state.show_modal) {
        open_update_popup = true;
        update_state.show_modal = false;
      }
    }
    if (open_update_popup) {
      ImGui::OpenPopup("Check for Update");
    }

    if (ImGui::BeginPopupModal("Check for Update", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      bool checking = false;
      bool has_result = false;
      app_update::UpdateResult snapshot;
      {
        std::lock_guard<std::mutex> lock(update_state.mutex);
        checking = update_state.checking;
        has_result = update_state.has_result;
        snapshot = update_state.result;
      }

      if (checking || !has_result) {
        ImGui::TextUnformatted("Please wait. Checking for update.");
        if (ImGui::Button("Close")) {
          ImGui::CloseCurrentPopup();
        }
      } else if (!snapshot.success) {
        ImGui::TextWrapped("Update check failed: %s", snapshot.error.c_str());
        if (ImGui::Button("Close")) {
          ImGui::CloseCurrentPopup();
        }
      } else {
        int cmp = app_update::compare_versions(kAppVersionTag, snapshot.latest_tag);
        if (cmp < 0) {
          ImGui::TextWrapped("%s is available (current %s). Update now?", snapshot.latest_tag.c_str(), kAppVersionTag);
#if defined(__linux__)
          ImGui::Spacing();
          ImGui::TextWrapped("If you installed this app through a package manager, please update it there to get the latest version.");
#endif
          if (ImGui::Button("Yes")) {
            app_update::open_url(snapshot.latest_url);
            ImGui::CloseCurrentPopup();
          }
          ImGui::SameLine();
          if (ImGui::Button("No")) {
            ImGui::CloseCurrentPopup();
          }
        } else {
          ImGui::TextWrapped("You are up to date. Current version %s.", kAppVersionTag);
          if (ImGui::Button("OK")) {
            ImGui::CloseCurrentPopup();
          }
        }
      }

      ImGui::EndPopup();
    }

    float menu_height = ImGui::GetFrameHeight();
    ImGui::SetNextWindowPos(ImVec2(0.0f, menu_height), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2((float)io.DisplaySize.x, (float)io.DisplaySize.y - menu_height), ImGuiCond_Always);
    ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                             ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
    ImGui::Begin("Caption", nullptr, flags);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 255));
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));

    bool window_hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
    const ImGuiIO &local_io = ImGui::GetIO();
    if (auto_scroll_enabled && window_hovered && (std::abs(local_io.MouseWheel) > 0.0f || ImGui::IsMouseDragging(ImGuiMouseButton_Left))) {
      auto_scroll_enabled = false;
    }

    std::string composed = caption.buffer();
    if (partial_filtered && !partial_filtered->empty()) {
      composed += *partial_filtered;
    }
    float wrap_width = ImGui::GetContentRegionAvail().x;
    float line_spacing = ImGui::GetTextLineHeight() * 0.5f; // 1.5x line spacing for captions
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, line_spacing));
    ImGui::PushTextWrapPos(ImGui::GetCursorPos().x + wrap_width);

    const char *start = composed.c_str();
    const char *end = start + composed.size();
    while (start < end) {
      const char *newline = static_cast<const char *>(memchr(start, '\n', end - start));
      if (!newline) {
        ImGui::TextUnformatted(start, end);
        break;
      }
      ImGui::TextUnformatted(start, newline);
      start = newline + 1;
    }

    float max_scroll = ImGui::GetScrollMaxY();
    float scroll_y = ImGui::GetScrollY();
    if (auto_scroll_enabled) {
      ImGui::SetScrollY(max_scroll);
    } else if (max_scroll > 0.0f && (max_scroll - scroll_y) < 2.0f) {
      auto_scroll_enabled = true; // user reached bottom, resume auto-scroll
    }
    ImGui::PopTextWrapPos();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);
    ImGui::End();
    ImGui::PopStyleVar();

    ImGui::Render();
    int display_w = 0;
    int display_h = 0;
    glfwGetFramebufferSize(window, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    glfwSwapBuffers(window);

    if (rebuild_fonts) {
      rebuild_fonts = false;
      settings.font_size_px = pending_font_size;
      io.Fonts->Clear();
      configure_fonts(exe_path, settings.font_size_px);
      ImGui_ImplOpenGL3_DestroyFontsTexture();
      ImGui_ImplOpenGL3_CreateFontsTexture();
      save_settings(settings_path, settings);
    }
  }

  audio.stop();
  engine.stop();
  app_update::finalize_update_thread(update_state);

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();

  glfwDestroyWindow(window);
  glfwTerminate();
  save_settings(settings_path, settings);
  return 0;
}

  #if defined(_WIN32)
  int APIENTRY wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int) {
    int wargc = 0;
    LPWSTR *wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    if (!wargv) {
      return run_app(0, nullptr);
    }

    std::vector<std::string> args;
    args.reserve(static_cast<size_t>(wargc));
    auto wide_to_utf8 = [](const std::wstring &w) {
      if (w.empty()) {
        return std::string();
      }
      int size = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), nullptr, 0, nullptr, nullptr);
      std::string out(static_cast<size_t>(size), '\0');
      WideCharToMultiByte(CP_UTF8, 0, w.c_str(), static_cast<int>(w.size()), out.data(), size, nullptr, nullptr);
      return out;
    };

    bool want_console = false;
    for (int i = 0; i < wargc; ++i) {
      auto utf8 = wide_to_utf8(wargv[i]);
      if (utf8 == "-console" || utf8 == "--console") {
        want_console = true;
      }
      args.push_back(std::move(utf8));
    }

    LocalFree(wargv);

    if (want_console) {
      AllocConsole();
      FILE *out = nullptr;
      freopen_s(&out, "CONOUT$", "w", stdout);
      freopen_s(&out, "CONOUT$", "w", stderr);
    }

    std::vector<char *> argv;
    argv.reserve(args.size());
    for (auto &s : args) {
      argv.push_back(s.empty() ? const_cast<char *>("") : s.data());
    }

    return run_app(static_cast<int>(argv.size()), argv.data());
  }

  int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR, int nShow) {
    return wWinMain(hInst, hPrev, GetCommandLineW(), nShow);
  }
  #else
  int main(int argc, char **argv) { return run_app(argc, argv); }
  #endif
