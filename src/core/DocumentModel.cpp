#include "core/DocumentModel.h"

#include <utility>

namespace textfx {

void DocumentModel::addTextBox(TextBox box) {
  boxes_.push_back(std::move(box));
  dirty_ = true;
}

void DocumentModel::moveLayer(std::size_t from, std::size_t to) {
  if (from >= boxes_.size() || to >= boxes_.size() || from == to) {
    return;
  }
  auto box = std::move(boxes_[from]);
  boxes_.erase(boxes_.begin() + static_cast<std::ptrdiff_t>(from));
  boxes_.insert(boxes_.begin() + static_cast<std::ptrdiff_t>(to),
                std::move(box));
  dirty_ = true;
}

void DocumentModel::clear() {
  boxes_.clear();
  presets_.clear();
  dirty_ = false;
}

} // namespace textfx
