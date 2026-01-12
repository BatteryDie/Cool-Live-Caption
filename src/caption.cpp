#include "caption.h"

void CaptionView::append(std::string_view text) {
  data_.append(text);
}

void CaptionView::clear() {
  data_.clear();
}

const std::string &CaptionView::buffer() const {
  return data_;
}

void CaptionView::set_active_model(std::string model_name) {
  model_ = std::move(model_name);
}

const std::string &CaptionView::active_model() const {
  return model_;
}
