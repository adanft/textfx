#include "app/TextBoxSelectionService.h"

#include <algorithm>
#include <utility>

namespace textfx {

int TextBoxSelectionService::normalizedIndex(const std::vector<TextBox>& boxes, int index)
{
    return index >= 0 && index < static_cast<int>(boxes.size()) ? index : -1;
}

bool TextBoxSelectionService::duplicateSelected(std::vector<TextBox>& boxes, int& selectedIndex)
{
    const int sourceIndex = normalizedIndex(boxes, selectedIndex);
    if (sourceIndex < 0) return false;

    auto copy = boxes.at(static_cast<std::size_t>(sourceIndex));
    copy.bounds.x += 16.0;
    copy.bounds.y += 16.0;
    boxes.push_back(std::move(copy));
    selectedIndex = static_cast<int>(boxes.size()) - 1;
    return true;
}

bool TextBoxSelectionService::deleteSelected(std::vector<TextBox>& boxes, int& selectedIndex)
{
    const int deleteIndex = normalizedIndex(boxes, selectedIndex);
    if (deleteIndex < 0) return false;

    boxes.erase(boxes.begin() + deleteIndex);
    selectedIndex = std::min(deleteIndex, static_cast<int>(boxes.size()) - 1);
    return true;
}

bool TextBoxSelectionService::moveLayer(std::vector<TextBox>& boxes, int& selectedIndex, int to)
{
    const int from = normalizedIndex(boxes, selectedIndex);
    if (from < 0) return false;

    to = std::clamp(to, 0, static_cast<int>(boxes.size()) - 1);
    if (from != to) {
        auto box = std::move(boxes[static_cast<std::size_t>(from)]);
        boxes.erase(boxes.begin() + from);
        boxes.insert(boxes.begin() + to, std::move(box));
    }
    selectedIndex = to;
    return true;
}

} // namespace textfx
