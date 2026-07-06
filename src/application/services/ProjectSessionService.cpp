#include "application/services/ProjectSessionService.h"

#include "core/ProjectStore.h"

namespace textfx {

ProjectPages ProjectSessionService::discoverPages(const ProjectStore &store) {
  ProjectPages pages;
  for (const auto &page : store.listPagePaths()) {
    pages.paths.push_back(page);
    pages.names.push_back(QString::fromStdString(page.filename().string()));
  }
  return pages;
}

QString ProjectSessionService::pageName(const QStringList &pages, int index) {
  return normalizePageIndex(index, static_cast<std::size_t>(pages.size())) < 0
             ? QString{}
             : pages.at(index);
}

QStringList ProjectSessionService::pageLabels(const QStringList &pages) {
  QStringList labels;
  for (int i = 0; i < pages.size(); ++i) {
    labels.push_back(QStringLiteral("%1 - %2").arg(i + 1).arg(pages.at(i)));
  }
  return labels;
}

int ProjectSessionService::normalizePageIndex(int index,
                                              std::size_t pageCount) {
  return index >= 0 && index < static_cast<int>(pageCount) ? index : -1;
}

std::string
ProjectSessionService::pageKey(const std::filesystem::path &pagePath) {
  return pagePath.empty() ? std::string{} : pagePath.filename().string();
}

} // namespace textfx
