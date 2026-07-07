#pragma once

#include "application/ports/IProjectExportStore.h"
#include "application/services/PageTextService.h"
#include "application/services/ProjectSessionService.h"
#include "domain/document/DocumentModel.h"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace textfx {

class ProjectStore;

class ProjectSession {
public:
  explicit ProjectSession(std::filesystem::path folder);
  ~ProjectSession();

  ProjectSession(ProjectSession &&) noexcept;
  ProjectSession &operator=(ProjectSession &&) noexcept;

  ProjectSession(const ProjectSession &) = delete;
  ProjectSession &operator=(const ProjectSession &) = delete;

  PageTextMap loadPageTexts(std::string *error = nullptr) const;
  ProjectPages discoverPages() const;
  std::filesystem::path
  rawPagePathFor(const std::filesystem::path &cleanPagePath) const;
  bool loadPage(const std::filesystem::path &pagePath, DocumentModel &document,
                std::string *error = nullptr) const;
  bool autosave(const std::filesystem::path &pagePath, DocumentModel &document,
                std::string *error = nullptr) const;
  bool savePage(const std::filesystem::path &pagePath,
                const DocumentModel &document,
                std::string *error = nullptr) const;
  bool savePresets(const std::vector<TextPreset> &projectPresets,
                   std::string *error = nullptr) const;
  bool loadPresets(DocumentModel &document,
                   std::vector<TextPreset> &projectPresets,
                   std::string *error = nullptr) const;
  std::filesystem::path
  pageExportPathFor(const std::filesystem::path &pagePath) const;
  const IProjectExportStore &exportStore() const;

private:
  std::unique_ptr<ProjectStore> store_;
};

} // namespace textfx
