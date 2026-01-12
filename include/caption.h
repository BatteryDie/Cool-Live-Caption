#pragma once

#include <string>
#include <string_view>
#include <vector>

class CaptionView {
public:
  void append(std::string_view text);
  void clear();
  const std::string &buffer() const;
  void set_active_model(std::string model_name);
  const std::string &active_model() const;

private:
  std::string data_;
  std::string model_;
};
