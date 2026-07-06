#pragma once

#include "domain/document/DocumentModel.h"

#include <vector>

namespace textfx {

class TextBoxSelectionService {
public:
  static int normalizedIndex(const std::vector<TextBox> &boxes, int index);
  static bool duplicateSelected(std::vector<TextBox> &boxes,
                                int &selectedIndex);
  static bool deleteSelected(std::vector<TextBox> &boxes, int &selectedIndex);
  static bool moveLayer(std::vector<TextBox> &boxes, int &selectedIndex,
                        int to);
};

} // namespace textfx
