#include "infrastructure/persistence/ProjectStore.h"

#include "infrastructure/persistence/JsonSerializer.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <optional>

namespace textfx {
namespace {
const std::vector<std::string> legacyCleanedFolders{"cleaned", "clean", "pages",
                                                    "images"};

#ifdef TEXTFX_ENABLE_TEST_HOOKS
std::optional<std::filesystem::path> pageTextsOpenFailurePath;

std::filesystem::path comparablePath(const std::filesystem::path &path) {
  std::error_code error;
  const auto canonical = std::filesystem::weakly_canonical(path, error);
  return error ? path.lexically_normal() : canonical;
}
#endif

int compareNumbers(std::string_view a, std::string_view b) {
  while (a.size() > 1 && a.front() == '0')
    a.remove_prefix(1);
  while (b.size() > 1 && b.front() == '0')
    b.remove_prefix(1);
  if (a.size() != b.size())
    return a.size() < b.size() ? -1 : 1;
  if (a == b)
    return 0;
  return a < b ? -1 : 1;
}

int naturalCompare(std::string a, std::string b) {
  a = ProjectStore::lower(std::move(a));
  b = ProjectStore::lower(std::move(b));
  std::size_t ai = 0;
  std::size_t bi = 0;
  while (ai < a.size() && bi < b.size()) {
    if (std::isdigit(static_cast<unsigned char>(a[ai])) &&
        std::isdigit(static_cast<unsigned char>(b[bi]))) {
      const auto as = ai;
      const auto bs = bi;
      while (ai < a.size() && std::isdigit(static_cast<unsigned char>(a[ai])))
        ++ai;
      while (bi < b.size() && std::isdigit(static_cast<unsigned char>(b[bi])))
        ++bi;
      const auto cmp = compareNumbers(std::string_view(a).substr(as, ai - as),
                                      std::string_view(b).substr(bs, bi - bs));
      if (cmp != 0)
        return cmp;
      continue;
    }
    if (a[ai] != b[bi])
      return a[ai] < b[bi] ? -1 : 1;
    ++ai;
    ++bi;
  }
  if (a.size() == b.size())
    return 0;
  return a.size() < b.size() ? -1 : 1;
}

std::string trim(const std::string &value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos)
    return {};
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}
} // namespace

ProjectStore::ProjectStore(std::filesystem::path folder)
    : folder_(std::move(folder)) {}

std::vector<std::filesystem::path> ProjectStore::listPagePaths() const {
  auto pages = imagePathsIn(folder_ / CleanedFolder);
  if (!pages.empty())
    return pages;
  pages = imagePathsIn(folder_);
  if (!pages.empty())
    return pages;
  for (const auto &name : legacyCleanedFolders) {
    pages = imagePathsIn(folder_ / name);
    if (!pages.empty())
      return pages;
  }
  return {};
}

std::filesystem::path
ProjectStore::rawPagePathFor(const std::filesystem::path &cleanPagePath) const {
  std::vector<std::filesystem::path> candidates{folder_ / RawFolder,
                                                folder_ / "raw"};
  const auto folderName = lower(folder_.filename().string());
  if (folderName == lower(CleanedFolder) ||
      std::ranges::find(legacyCleanedFolders, folderName) !=
          legacyCleanedFolders.end()) {
    candidates.push_back(folder_.parent_path() / RawFolder);
    candidates.push_back(folder_.parent_path() / "raw");
  }
  const auto cleanFolder = cleanPagePath.parent_path();
  const auto cleanFolderName = lower(cleanFolder.filename().string());
  if (cleanFolderName == lower(CleanedFolder) ||
      std::ranges::find(legacyCleanedFolders, cleanFolderName) !=
          legacyCleanedFolders.end()) {
    candidates.push_back(cleanFolder.parent_path() / RawFolder);
    candidates.push_back(cleanFolder.parent_path() / "raw");
  }

  const auto base = lower(cleanPagePath.stem().string());
  for (const auto &candidate : candidates) {
    for (const auto &raw : imagePathsIn(candidate)) {
      if (lower(raw.stem().string()) == base)
        return raw;
    }
  }
  return {};
}

std::filesystem::path
ProjectStore::pageSavePathFor(const std::filesystem::path &pagePath) const {
  return folder_ / SaveFolder / (pagePath.stem().string() + ".json");
}

std::filesystem::path
ProjectStore::pageExportPathFor(const std::filesystem::path &pagePath) const {
  return folder_ / ExportFolder / (pagePath.stem().string() + ".png");
}

std::filesystem::path ProjectStore::presetsPath() const {
  return folder_ / SaveFolder / "presets.json";
}

std::filesystem::path ProjectStore::pageTextsPath() const {
  return folder_ / PageTextsFile;
}

#ifdef TEXTFX_ENABLE_TEST_HOOKS
namespace test_hooks {
void failPageTextsOpen(std::filesystem::path path) {
  pageTextsOpenFailurePath = comparablePath(path);
}

void clearPageTextsOpenFailure() { pageTextsOpenFailurePath.reset(); }
} // namespace test_hooks
#endif

PageTextMap ProjectStore::loadPageTexts(std::string *error) const {
  PageTextMap result;
  const auto canonicalPath = pageTextsPath();
  auto path = canonicalPath;
  if (!std::filesystem::exists(path))
    path = folder_ / "texts.txt";
  if (!std::filesystem::exists(path))
    return result;

  if (!std::filesystem::is_regular_file(path)) {
    if (error != nullptr)
      *error = "Could not open Texts.txt.";
    return result;
  }

#ifdef TEXTFX_ENABLE_TEST_HOOKS
  if (pageTextsOpenFailurePath &&
      comparablePath(path) == *pageTextsOpenFailurePath) {
    if (error != nullptr)
      *error = "Could not open Texts.txt.";
    return result;
  }
#endif

  std::ifstream input(path);
  if (!input) {
    if (error != nullptr)
      *error = "Could not open Texts.txt.";
    return result;
  }

  std::string currentPage;
  std::string line;
  while (std::getline(input, line)) {
    line = trim(line);
    if (line.empty())
      continue;
    if (line.front() == '[' && line.back() == ']') {
      currentPage = trim(line.substr(1, line.size() - 2));
      if (!currentPage.empty())
        result.try_emplace(currentPage);
    } else if (!currentPage.empty()) {
      result[currentPage].push_back(line);
    }
  }
  return result;
}

bool ProjectStore::loadPresets(DocumentModel &document,
                               std::vector<TextPreset> &projectPresets,
                               std::string *error) const {
  projectPresets.clear();
  const auto path = presetsPath();
  if (std::filesystem::exists(path) &&
      !JsonSerializer::loadPresets(path, projectPresets, error))
    return false;

  document.presets() = projectPresets;
  return true;
}

bool ProjectStore::savePresets(const std::vector<TextPreset> &projectPresets,
                               std::string *error) const {
  return JsonSerializer::savePresets(presetsPath(), projectPresets, error);
}

bool ProjectStore::loadPage(const std::filesystem::path &pagePath,
                            DocumentModel &document, std::string *error) const {
  const auto savePath = pageSavePathFor(pagePath);
  if (!std::filesystem::exists(savePath)) {
    document.clear();
    return true;
  }
  return JsonSerializer::loadPage(savePath, document, error);
}

bool ProjectStore::savePage(const std::filesystem::path &pagePath,
                            const DocumentModel &document,
                            std::string *error) const {
  try {
    std::filesystem::create_directories(folder_ / SaveFolder);
  } catch (const std::filesystem::filesystem_error &ex) {
    if (error)
      *error = ex.what();
    return false;
  }
  return JsonSerializer::savePage(
      pageSavePathFor(pagePath), pagePath.filename().string(), document, error);
}

bool ProjectStore::autosave(const std::filesystem::path &pagePath,
                            DocumentModel &document, std::string *error) const {
  if (!document.dirty())
    return true;
  if (!savePage(pagePath, document, error))
    return false;
  document.markSaved();
  return true;
}

bool ProjectStore::saveAll(
    const std::vector<std::pair<std::filesystem::path, DocumentModel *>> &pages,
    std::string *error) const {
  for (const auto &[path, document] : pages) {
    if (document != nullptr && !autosave(path, *document, error))
      return false;
  }
  return true;
}

bool ProjectStore::naturalLess(const std::filesystem::path &a,
                               const std::filesystem::path &b) {
  return naturalCompare(a.filename().string(), b.filename().string()) < 0;
}

bool ProjectStore::isImagePath(const std::filesystem::path &path) {
  const auto ext = lower(path.extension().string());
  return ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".jfif" ||
         ext == ".webp";
}

std::vector<std::filesystem::path>
ProjectStore::imagePathsIn(const std::filesystem::path &folder) {
  std::vector<std::filesystem::path> paths;
  if (!std::filesystem::is_directory(folder))
    return paths;
  for (const auto &entry : std::filesystem::directory_iterator(folder)) {
    if (entry.is_regular_file() && isImagePath(entry.path()))
      paths.push_back(entry.path());
  }
  std::ranges::sort(paths, naturalLess);
  return paths;
}

std::string ProjectStore::lower(std::string value) {
  std::ranges::transform(value, value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

} // namespace textfx
