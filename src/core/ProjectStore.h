#pragma once

#include "core/DocumentModel.h"

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

namespace textfx {

class ProjectStore {
public:
  static constexpr auto SaveFolder = ".textfx";
  static constexpr auto ExportFolder = "exported";

  explicit ProjectStore(std::filesystem::path folder);

  const std::filesystem::path &folder() const { return folder_; }
  std::vector<std::filesystem::path> listPagePaths() const;
  std::filesystem::path
  rawPagePathFor(const std::filesystem::path &cleanPagePath) const;
  std::filesystem::path
  pageSavePathFor(const std::filesystem::path &pagePath) const;
  std::filesystem::path
  pageExportPathFor(const std::filesystem::path &pagePath) const;
  std::filesystem::path presetsPath() const;
  std::filesystem::path pageTextsPath() const;
  std::unordered_map<std::string, std::vector<std::string>>
  loadPageTexts(std::string *error = nullptr) const;
  bool loadPresets(DocumentModel &document,
                   std::vector<TextPreset> &projectPresets,
                   std::string *error = nullptr) const;
  bool savePresets(const std::vector<TextPreset> &projectPresets,
                   std::string *error = nullptr) const;

  bool loadPage(const std::filesystem::path &pagePath, DocumentModel &document,
                std::string *error = nullptr) const;
  bool savePage(const std::filesystem::path &pagePath,
                const DocumentModel &document,
                std::string *error = nullptr) const;
  bool autosave(const std::filesystem::path &pagePath, DocumentModel &document,
                std::string *error = nullptr) const;
  bool
  saveAll(const std::vector<std::pair<std::filesystem::path, DocumentModel *>>
              &pages,
          std::string *error = nullptr) const;

  static bool naturalLess(const std::filesystem::path &a,
                          const std::filesystem::path &b);
  static std::string lower(std::string value);
  static std::vector<TextPreset> defaultTextPresets();
  static bool isDefaultTextPresetName(const std::string &name);

private:
  static bool isImagePath(const std::filesystem::path &path);
  static std::vector<std::filesystem::path>
  imagePathsIn(const std::filesystem::path &folder);

  std::filesystem::path folder_;
};

} // namespace textfx
