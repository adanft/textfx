#include "application/services/TextBoxSelectionService.h"

#include <algorithm>
#include <utility>

namespace textfx {

int TextBoxSelectionService::normalizedIndex(const std::vector<TextBox> &boxes,
                                             int index) {
  return index >= 0 && index < static_cast<int>(boxes.size()) ? index : -1;
}

bool TextBoxSelectionService::duplicateSelected(
    std::vector<TextBox> &boxes, int &selectedIndex,
    std::vector<int> &selectedIndices) {
  const int sourceIndex = normalizedIndex(boxes, selectedIndex);
  if (sourceIndex < 0)
    return false;

  auto copy = boxes.at(static_cast<std::size_t>(sourceIndex));
  copy.bounds.x += 16.0;
  copy.bounds.y += 16.0;
  boxes.push_back(std::move(copy));
  selectedIndex = static_cast<int>(boxes.size()) - 1;
  selectedIndices = {selectedIndex};
  return true;
}

bool TextBoxSelectionService::deleteSelected(
    std::vector<TextBox> &boxes, int &selectedIndex,
    std::vector<int> &selectedIndices) {
  const int deleteIndex = normalizedIndex(boxes, selectedIndex);
  if (deleteIndex < 0)
    return false;

  boxes.erase(boxes.begin() + deleteIndex);
  std::erase(selectedIndices, deleteIndex);
  for (auto &index : selectedIndices) {
    if (index > deleteIndex)
      --index;
  }
  selectedIndex = selectedIndices.empty()
                      ? std::min(deleteIndex, static_cast<int>(boxes.size()) - 1)
                      : selectedIndices.back();
  if (selectedIndex >= 0 && selectedIndices.empty())
    selectedIndices = {selectedIndex};
  return true;
}

bool TextBoxSelectionService::moveLayer(std::vector<TextBox> &boxes,
                                        int &selectedIndex,
                                        std::vector<int> &selectedIndices,
                                        int to) {
  const int from = normalizedIndex(boxes, selectedIndex);
  if (from < 0)
    return false;

  to = std::clamp(to, 0, static_cast<int>(boxes.size()) - 1);
  if (from != to) {
    auto box = std::move(boxes[static_cast<std::size_t>(from)]);
    boxes.erase(boxes.begin() + from);
    boxes.insert(boxes.begin() + to, std::move(box));
    for (auto &index : selectedIndices) {
      if (index == from)
        index = to;
      else if (from < to && index > from && index <= to)
        --index;
      else if (to < from && index >= to && index < from)
        ++index;
    }
  }
  selectedIndex = to;
  return true;
}

} // namespace textfx
