#include "app/project/ProjectCreationWorkflow.h"

#include "infrastructure/persistence/ProjectStore.h"

#include <fstream>
#include <optional>

namespace textfx {
namespace {
#ifdef TEXTFX_ENABLE_TEST_HOOKS
std::optional<std::filesystem::path> projectFileReadFailurePath;

std::filesystem::path comparablePath(const std::filesystem::path &path) {
  std::error_code error;
  const auto canonical = std::filesystem::weakly_canonical(path, error);
  return error ? path.lexically_normal() : canonical;
}
#endif

QString toQString(const std::string &value) {
  return QString::fromStdString(value);
}

bool ensureRegularFile(const std::filesystem::path &path, std::string *error) {
  if (std::filesystem::exists(path) &&
      !std::filesystem::is_regular_file(path)) {
    if (error)
      *error = path.filename().string() + " must be a regular file";
    return false;
  }

  std::ofstream file(path, std::ios::app);
  if (!file.is_open()) {
    if (error)
      *error = "could not open " + path.filename().string();
    return false;
  }
  file.close();

  if (!std::filesystem::is_regular_file(path)) {
    if (error)
      *error = path.filename().string() + " must be a regular file";
    return false;
  }

#ifdef TEXTFX_ENABLE_TEST_HOOKS
  if (projectFileReadFailurePath &&
      comparablePath(path) == *projectFileReadFailurePath) {
    if (error)
      *error = "could not read " + path.filename().string();
    return false;
  }
#endif

  std::ifstream readable(path);
  if (!readable.is_open()) {
    if (error)
      *error = "could not read " + path.filename().string();
    return false;
  }

  return true;
}
} // namespace

#ifdef TEXTFX_ENABLE_TEST_HOOKS
namespace test_hooks {
void failProjectFileRead(std::filesystem::path path) {
  projectFileReadFailurePath = comparablePath(path);
}

void clearProjectFileReadFailure() { projectFileReadFailurePath.reset(); }
} // namespace test_hooks
#endif

ProjectCreationResult
ProjectCreationWorkflow::createStructure(const std::filesystem::path &root) {
  try {
    std::filesystem::create_directories(root / ProjectStore::CleanedFolder);
    std::filesystem::create_directories(root / ProjectStore::RawFolder);
    std::filesystem::create_directories(root / ProjectStore::ExportFolder);
    std::string error;
    if (!ensureRegularFile(root / ProjectStore::PageTextsFile, &error)) {
      return {.success = false, .error = toQString(error)};
    }
  } catch (const std::filesystem::filesystem_error &ex) {
    return {.success = false, .error = toQString(ex.what())};
  }

  return {.success = true};
}

} // namespace textfx
