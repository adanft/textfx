#pragma once

#include "application/ports/IPageExportRenderer.h"
#include "domain/document/DocumentModel.h"

#include <chrono>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace textfx {

class RenderGraph : public IPageExportRenderer {
public:
  using ExportTiming = textfx::ExportTiming;
  using ExportResult = textfx::ExportResult;

  std::expected<ExportResult, std::string>
  exportPagePngTimed(const DocumentModel &document,
                     const std::filesystem::path &pageImagePath,
                     const std::filesystem::path &exportPath) const override;
  std::expected<void, std::string>
  exportPagePngResult(const DocumentModel &document,
                      const std::filesystem::path &pageImagePath,
                      const std::filesystem::path &exportPath) const;
  bool exportPagePng(const DocumentModel &document,
                     const std::filesystem::path &pageImagePath,
                     const std::filesystem::path &exportPath,
                     std::string *error = nullptr) const;
  std::vector<std::string> warnings(const DocumentModel &document) const;
};

} // namespace textfx
