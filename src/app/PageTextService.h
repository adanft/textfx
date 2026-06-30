#pragma once

#include "core/DocumentModel.h"

#include <QStringList>

#include <string>
#include <unordered_map>
#include <vector>

namespace textfx {

using PageTextMap = std::unordered_map<std::string, std::vector<std::string>>;
using PageTextPositionMap = std::unordered_map<std::string, int>;

enum class PageTextApplyStatus {
    Applied,
    MissingText,
    NoSelectedBox,
};

class PageTextService {
public:
    static QStringList textsForPage(const PageTextMap& pageTexts, const std::string& pageKey);
    static int nextTextIndex(PageTextPositionMap& positions, const std::string& pageKey);
    static PageTextApplyStatus applyText(
        std::vector<TextBox>& boxes,
        int selectedIndex,
        const PageTextMap& pageTexts,
        PageTextPositionMap& positions,
        const std::string& pageKey,
        int textIndex);
};

} // namespace textfx
