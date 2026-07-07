#pragma once

#include <QString>
#include <QStringList>

#include <filesystem>
#include <string>
#include <vector>

namespace textfx {

class IProjectPageSource;

struct ProjectPages {
  std::vector<std::filesystem::path> paths;
  QStringList names;
};

class ProjectSessionService {
public:
  static ProjectPages discoverPages(const IProjectPageSource &pageSource);
  static QString pageName(const QStringList &pages, int index);
  static QStringList pageLabels(const QStringList &pages);
  static int normalizePageIndex(int index, std::size_t pageCount);
  static std::string pageKey(const std::filesystem::path &pagePath);
};

} // namespace textfx
