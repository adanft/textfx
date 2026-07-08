#pragma once

#include "domain/document/DocumentModel.h"

#include <filesystem>
#include <string>

namespace textfx {

class IProjectDocumentStore {
public:
  virtual ~IProjectDocumentStore() = default;

  virtual bool loadPage(const std::filesystem::path &pagePath,
                        DocumentModel &document,
                        std::string *error = nullptr) const = 0;
  virtual bool autosave(const std::filesystem::path &pagePath,
                        DocumentModel &document,
                        std::string *error = nullptr) const = 0;
  virtual bool savePage(const std::filesystem::path &pagePath,
                        const DocumentModel &document,
                        std::string *error = nullptr) const = 0;
};

} // namespace textfx
