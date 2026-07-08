#pragma once

#include <QString>

#include <filesystem>
#include <vector>

namespace textfx {

class DocumentModel;
class IPageExportRenderer;
class ProjectSession;

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

struct SaveCurrentProjectRequest {
  ProjectSession *session;
  const IPageExportRenderer &renderer;
  DocumentModel &document;
  const std::filesystem::path &currentPage;
};

struct SaveAllProjectRequest {
  ProjectSession *session;
  const IPageExportRenderer &renderer;
  DocumentModel &document;
  const std::filesystem::path &currentPage;
  const std::vector<std::filesystem::path> &pagePaths;
};

class ProjectSaveExportWorkflow {
public:
  static SaveCurrentProjectResult
  saveCurrent(const SaveCurrentProjectRequest &request);

  static SaveAllProjectResult saveAll(const SaveAllProjectRequest &request);
};

} // namespace textfx
