#pragma once

#include <filesystem>

namespace textfx {

class IProjectRawPageSource {
public:
  virtual ~IProjectRawPageSource() = default;
  virtual std::filesystem::path
  rawPagePathFor(const std::filesystem::path &cleanPagePath) const = 0;
};

} // namespace textfx
