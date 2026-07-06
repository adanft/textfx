#include "application/services/PageTextService.h"

#include <algorithm>

namespace textfx {

namespace {
QString toQString(const std::string &value) {
  return QString::fromStdString(value);
}
} // namespace

QStringList PageTextService::textsForPage(const PageTextMap &pageTexts,
                                          const std::string &pageKey) {
  QStringList list;
  const auto found = pageTexts.find(pageKey);
  if (found == pageTexts.end())
    return list;

  for (const auto &text : found->second)
    list.push_back(toQString(text));
  return list;
}

int PageTextService::nextTextIndex(PageTextPositionMap &positions,
                                   const std::string &pageKey) {
  return positions[pageKey];
}

PageTextApplyStatus PageTextService::applyText(std::vector<TextBox> &boxes,
                                               int selectedIndex,
                                               const PageTextMap &pageTexts,
                                               PageTextPositionMap &positions,
                                               const std::string &pageKey,
                                               int textIndex) {
  const auto found = pageTexts.find(pageKey);
  if (found == pageTexts.end() || textIndex < 0 ||
      textIndex >= static_cast<int>(found->second.size())) {
    return PageTextApplyStatus::MissingText;
  }
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(boxes.size())) {
    return PageTextApplyStatus::NoSelectedBox;
  }

  boxes[static_cast<std::size_t>(selectedIndex)].text =
      found->second[static_cast<std::size_t>(textIndex)];
  positions[pageKey] =
      std::min(textIndex + 1, static_cast<int>(found->second.size()));
  return PageTextApplyStatus::Applied;
}

} // namespace textfx
