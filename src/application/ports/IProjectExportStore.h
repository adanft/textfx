#pragma once

#include "domain/document/DocumentModel.h"

#include <filesystem>
#include <string>

namespace textfx {

class IProjectExportStore {
public:
  virtual ~IProjectExportStore() = default;

  virtual std::filesystem::path
  pageExportPathFor(const std::filesystem::path &pagePath) const = 0;

  virtual bool loadPage(const std::filesystem::path &pagePath,
                        DocumentModel &document,
                        std::string *error = nullptr) const = 0;
};

} // namespace textfx
