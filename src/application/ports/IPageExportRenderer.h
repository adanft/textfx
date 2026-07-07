#pragma once

#include "domain/document/DocumentModel.h"

#include <chrono>
#include <expected>
#include <filesystem>
#include <string>

namespace textfx {

struct ExportTiming {
  using Duration = std::chrono::steady_clock::duration;

  Duration elapsed{};
};

struct ExportResult {
  ExportTiming timing{};
};

class IPageExportRenderer {
public:
  virtual ~IPageExportRenderer() = default;

  virtual std::expected<ExportResult, std::string>
  exportPagePngTimed(const DocumentModel &document,
                     const std::filesystem::path &pageImagePath,
                     const std::filesystem::path &exportPath) const = 0;
};

} // namespace textfx
