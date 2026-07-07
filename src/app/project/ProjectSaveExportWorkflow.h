#pragma once

#include <QString>

#include <filesystem>
#include <vector>

namespace textfx {

class DocumentModel;
class ProjectStore;

enum class SaveNotificationOrder {
  BeforeStateChanged,
  AfterStateChanged,
};

struct ProjectSaveExportResult {
  QString notification;
  bool stateChanged = false;
  SaveNotificationOrder notificationOrder =
      SaveNotificationOrder::BeforeStateChanged;
};

using SaveCurrentProjectResult = ProjectSaveExportResult;
using SaveAllProjectResult = ProjectSaveExportResult;

class ProjectSaveExportWorkflow {
public:
  static SaveCurrentProjectResult saveCurrent(
      ProjectStore *store, DocumentModel &document,
      const std::filesystem::path &currentPage);

  static SaveAllProjectResult saveAll(
      ProjectStore *store, DocumentModel &document,
      const std::filesystem::path &currentPage,
      const std::vector<std::filesystem::path> &pagePaths);
};

} // namespace textfx
