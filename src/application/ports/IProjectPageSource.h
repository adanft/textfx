#pragma once

#include <filesystem>
#include <vector>

namespace textfx {

class IProjectPageSource {
public:
  virtual ~IProjectPageSource() = default;

  // Returns the project's clean page paths in the order the application should
  // present them. Implementations are responsible for deterministic ordering.
  virtual std::vector<std::filesystem::path> listPagePaths() const = 0;
};

} // namespace textfx
