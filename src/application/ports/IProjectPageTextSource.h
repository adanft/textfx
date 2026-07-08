#pragma once

#include "application/ports/ProjectPageTexts.h"

#include <string>

namespace textfx {

class IProjectPageTextSource {
public:
  virtual ~IProjectPageTextSource() = default;
  virtual PageTextMap loadPageTexts(std::string *error = nullptr) const = 0;
};

} // namespace textfx
