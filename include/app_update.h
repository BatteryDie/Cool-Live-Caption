#pragma once

#include <mutex>
#include <string>
#include <thread>

namespace app_update {

struct UpdateResult {
  bool success = false;
  std::string latest_tag;
  std::string latest_url;
  std::string error;
};

struct UpdateState {
  bool show_modal = false;
  bool checking = false;
  bool has_result = false;
  UpdateResult result;
  std::thread worker;
  std::mutex mutex;
};

void start_update_check(UpdateState &state, bool show_modal);
void finalize_update_thread(UpdateState &state);
int compare_versions(const std::string &a, const std::string &b);
bool open_url(const std::string &url);

}  // namespace app_update
