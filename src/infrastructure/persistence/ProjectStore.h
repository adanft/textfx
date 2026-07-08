#pragma once

#include "application/ports/IProjectDocumentStore.h"
#include "application/ports/IProjectExportStore.h"
#include "application/ports/IProjectPageSource.h"
#include "application/ports/IProjectPresetStore.h"
#include "domain/document/DocumentModel.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace textfx {

class ProjectStore : public IProjectPageSource,
                     public IProjectDocumentStore,
                     public IProjectExportStore,
                     public IProjectPresetStore {
public:
  static constexpr auto SaveFolder = ".textfx";
  static constexpr auto CleanedFolder = "Cleaned";
  static constexpr auto RawFolder = "Raw";
  static constexpr auto ExportFolder = "Typeset";
  static constexpr auto PageTextsFile = "Texts.txt";

  explicit ProjectStore(std::filesystem::path folder);

  const std::filesystem::path &folder() const { return folder_; }
  std::vector<std::filesystem::path> listPagePaths() const override;
  std::filesystem::path
  rawPagePathFor(const std::filesystem::path &cleanPagePath) const;
  std::filesystem::path
  pageSavePathFor(const std::filesystem::path &pagePath) const;
  std::filesystem::path
  pageExportPathFor(const std::filesystem::path &pagePath) const override;
  std::filesystem::path presetsPath() const;
  std::filesystem::path pageTextsPath() const;
  std::unordered_map<std::string, std::vector<std::string>>
  loadPageTexts(std::string *error = nullptr) const;
  bool loadPresets(DocumentModel &document,
                   std::vector<TextPreset> &projectPresets,
                   std::string *error = nullptr) const override;
  bool savePresets(const std::vector<TextPreset> &projectPresets,
                   std::string *error = nullptr) const override;

  bool loadPage(const std::filesystem::path &pagePath, DocumentModel &document,
                std::string *error = nullptr) const override;
  bool savePage(const std::filesystem::path &pagePath,
                const DocumentModel &document,
                std::string *error = nullptr) const override;
  bool autosave(const std::filesystem::path &pagePath, DocumentModel &document,
                std::string *error = nullptr) const override;
  bool
  saveAll(const std::vector<std::pair<std::filesystem::path, DocumentModel *>>
              &pages,
          std::string *error = nullptr) const;

  static bool naturalLess(const std::filesystem::path &a,
                          const std::filesystem::path &b);
  static std::string lower(std::string value);

private:
  static bool isImagePath(const std::filesystem::path &path);
  static std::vector<std::filesystem::path>
  imagePathsIn(const std::filesystem::path &folder);

  std::filesystem::path folder_;
};

} // namespace textfx
