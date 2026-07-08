#pragma once

#include "application/ports/ProjectPageTexts.h"
#include "domain/document/DocumentModel.h"

#include <QStringList>

#include <string>
#include <vector>

namespace textfx {

enum class PageTextApplyStatus {
  Applied,
  MissingText,
  NoSelectedBox,
};

class PageTextService {
public:
  static QStringList textsForPage(const PageTextMap &pageTexts,
                                  const std::string &pageKey);
  static int nextTextIndex(PageTextPositionMap &positions,
                           const std::string &pageKey);
  static PageTextApplyStatus
  applyText(std::vector<TextBox> &boxes, int selectedIndex,
            const PageTextMap &pageTexts, PageTextPositionMap &positions,
            const std::string &pageKey, int textIndex);
};

} // namespace textfx
