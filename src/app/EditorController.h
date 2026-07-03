#pragma once

#include "app/BoxesModel.h"
#include "app/PageTextService.h"
#include "core/DocumentModel.h"
#include "core/ProjectStore.h"

#include <QObject>
#include <QAbstractListModel>
#include <QString>
#include <QStringList>
#include <QUrl>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <filesystem>
#include <functional>
#include <memory>

namespace textfx {

class EditorController final : public QObject {
  Q_OBJECT
  Q_PROPERTY(bool hasProject READ hasProject NOTIFY stateChanged)
  Q_PROPERTY(bool dirty READ dirty NOTIFY stateChanged)
  Q_PROPERTY(
      bool rawVisible READ rawVisible WRITE setRawVisible NOTIFY stateChanged)
  Q_PROPERTY(bool editingText READ editingText NOTIFY stateChanged)
  Q_PROPERTY(int selectedIndex READ selectedIndex NOTIFY selectionChanged)
  Q_PROPERTY(QVariant selectedBox READ selectedBoxViewModel NOTIFY
                 selectedBoxChanged)
  Q_PROPERTY(QString notification READ notification NOTIFY notificationChanged)
  Q_PROPERTY(QStringList pages READ pages NOTIFY stateChanged)
  Q_PROPERTY(QStringList pageLabels READ pageLabels NOTIFY stateChanged)
  Q_PROPERTY(QString currentPageName READ currentPageName NOTIFY stateChanged)
  Q_PROPERTY(int currentPageIndex READ currentPageIndex NOTIFY stateChanged)
  Q_PROPERTY(int pageCount READ pageCount NOTIFY stateChanged)
  Q_PROPERTY(QUrl currentPageUrl READ currentPageUrl NOTIFY stateChanged)
  Q_PROPERTY(QUrl rawPageUrl READ rawPageUrl NOTIFY stateChanged)
  Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY stateChanged)
  Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY stateChanged)
  Q_PROPERTY(QVariantList boxes READ boxes NOTIFY documentChanged)
  Q_PROPERTY(int boxCount READ boxCount NOTIFY documentChanged)
  Q_PROPERTY(QAbstractListModel *boxesModel READ boxesModel CONSTANT)
  Q_PROPERTY(QVariantList layers READ layers NOTIFY documentChanged)
  Q_PROPERTY(QVariantList presets READ presets NOTIFY documentChanged)
  Q_PROPERTY(
      int selectedPresetIndex READ selectedPresetIndex NOTIFY documentChanged)
  Q_PROPERTY(QStringList pageTexts READ pageTexts NOTIFY pageTextsChanged)

public:
  explicit EditorController(QObject *parent = nullptr);

  bool hasProject() const { return static_cast<bool>(store_); }
  bool dirty() const { return document_.dirty(); }
  bool rawVisible() const { return rawVisible_; }
  bool editingText() const { return editingText_; }
  int selectedIndex() const { return selectedIndex_; }
  QString notification() const { return notification_; }
  QStringList pages() const { return pages_; }
  QStringList pageLabels() const;
  QString currentPageName() const;
  int currentPageIndex() const { return currentPageIndex_; }
  int pageCount() const { return static_cast<int>(pagePaths_.size()); }
  QUrl currentPageUrl() const;
  QUrl rawPageUrl() const;
  bool canGoPrevious() const { return currentPageIndex_ > 0; }
  bool canGoNext() const {
    return currentPageIndex_ >= 0 && currentPageIndex_ + 1 < pageCount();
  }
  QVariantList boxes() const;
  int boxCount() const;
  QAbstractListModel *boxesModel();
  QVariant selectedBoxViewModel() const;
  QVariantList layers() const;
  QVariantList presets() const;
  int selectedPresetIndex() const { return selectedPresetIndex_; }
  QStringList pageTexts() const;

  Q_INVOKABLE bool actionEnabled(const QString &command) const;
  Q_INVOKABLE void openProjectUrl(const QUrl &folderUrl);
  Q_INVOKABLE void openProject(const QString &folder);
  Q_INVOKABLE void newProjectUrl(const QUrl &folderUrl);
  Q_INVOKABLE void newProject(const QString &folder);
  Q_INVOKABLE void newDocument();
  Q_INVOKABLE void save();
  Q_INVOKABLE void saveAll();
  Q_INVOKABLE void previousPage();
  Q_INVOKABLE void nextPage();
  Q_INVOKABLE void goToPage(int index);
  Q_INVOKABLE void selectBox(int index);
  Q_INVOKABLE void createTextBox(double x, double y, double w, double h);
  Q_INVOKABLE QVariant boxRole(int row, const QString &roleName) const;
  Q_INVOKABLE void updateSelectedText(const QString &text);
  Q_INVOKABLE void setSelectedFontFamily(const QString &family);
  Q_INVOKABLE void setSelectedFontSize(int size);
  Q_INVOKABLE void setSelectedTextColor(const QString &color);
  Q_INVOKABLE void setSelectedBold(bool enabled);
  Q_INVOKABLE void setSelectedItalic(bool enabled);
  Q_INVOKABLE void setSelectedUppercase(bool enabled);
  Q_INVOKABLE void setSelectedLowercase(bool enabled);
  Q_INVOKABLE void setSelectedAlignment(int alignment);
  Q_INVOKABLE void setSelectedLineSpacing(int spacing);
  Q_INVOKABLE void setSelectedLetterSpacing(int spacing);
  Q_INVOKABLE void moveSelected(double dx, double dy);
  Q_INVOKABLE void resizeSelected(double dw, double dh);
  Q_INVOKABLE void setSelectedBounds(double x, double y, double w, double h);
  Q_INVOKABLE void rotateSelected(double degrees);
  Q_INVOKABLE void setSelectedRotation(double degrees);
  Q_INVOKABLE void setSelectedOutlineEnabled(bool enabled);
  Q_INVOKABLE void setSelectedOutlineColor(const QString &color);
  Q_INVOKABLE void setSelectedOutlineSize(int size);
  Q_INVOKABLE void addSelectedOutlineLayer();
  Q_INVOKABLE void removeSelectedOutlineLayer(int index);
  Q_INVOKABLE void setSelectedOutlineLayerEnabled(int index, bool enabled);
  Q_INVOKABLE void setSelectedOutlineLayerColor(int index, const QString &color);
  Q_INVOKABLE void setSelectedOutlineLayerSize(int index, int size);
  Q_INVOKABLE void setSelectedBlurEnabled(bool enabled);
  Q_INVOKABLE void setSelectedBlurSize(int size);
  Q_INVOKABLE void setSelectedShadowEnabled(bool enabled);
  Q_INVOKABLE void setSelectedShadowColor(const QString &color);
  Q_INVOKABLE void setSelectedShadowOffsetX(int offset);
  Q_INVOKABLE void setSelectedShadowOffsetY(int offset);
  Q_INVOKABLE void setSelectedShadowBlurSize(int size);
  Q_INVOKABLE void setSelectedGradientEnabled(bool enabled);
  Q_INVOKABLE void setSelectedGradientDirection(int direction);
  Q_INVOKABLE void setSelectedGradientColorA(const QString &color);
  Q_INVOKABLE void setSelectedGradientColorB(const QString &color);
  Q_INVOKABLE void setSelectedPerspectiveEnabled(bool enabled);
  Q_INVOKABLE void setSelectedPathEnabled(bool enabled);
  Q_INVOKABLE void setSelectedPathMode(int mode);
  Q_INVOKABLE void resetSelectedPerspective();
  Q_INVOKABLE void resetSelectedPath();
  Q_INVOKABLE void addSelectedPathPoint();
  Q_INVOKABLE void setPerspectiveHandle(const QString &corner, double x,
                                        double y);
  Q_INVOKABLE void setPathHandle(int index, double x, double y);
  Q_INVOKABLE void duplicateSelected();
  Q_INVOKABLE void deleteSelected();
  Q_INVOKABLE void moveLayer(int to);
  Q_INVOKABLE void copySelected();
  Q_INVOKABLE void pasteBox();
  Q_INVOKABLE bool applyPageText(int index);
  Q_INVOKABLE bool applyNextPageText();
  Q_INVOKABLE void selectPreset(int index);
  Q_INVOKABLE bool applySelectedPreset();
  Q_INVOKABLE bool addPreset(const QString &name);
  Q_INVOKABLE bool updateSelectedPreset();
  Q_INVOKABLE bool renameSelectedPreset(const QString &name);
  Q_INVOKABLE bool deleteSelectedPreset();
  Q_INVOKABLE bool leftMouseButtonDown() const;
  Q_INVOKABLE void beginInteraction();
  Q_INVOKABLE void endInteraction();
  Q_INVOKABLE void beginTextEdit();
  Q_INVOKABLE void endTextEdit();
  Q_INVOKABLE void applyTextLineSpacing(QObject *quickTextDocument,
                                        double spacing);
  Q_INVOKABLE void notify(const QString &message);

public slots:
  void setRawVisible(bool visible);

signals:
  void stateChanged();
  void documentChanged();
  void selectionChanged();
  void selectedBoxChanged();
  void notificationChanged();
  void pageTextsChanged();

private:
  TextBox *selectedBox();
  const TextBox *selectedBox() const;
  bool editSelectedBoxIf(const std::function<bool(TextBox &)> &mutation);
  void editSelectedBox(const std::function<void(TextBox &)> &mutation);
  bool editSelectedBoxIf(const std::function<bool(TextBox &)> &mutation,
                         const QList<int> &roles);
  void editSelectedBox(const std::function<void(TextBox &)> &mutation,
                       const QList<int> &roles);
  void markDocumentChanged();
  void markDocumentChanged(const QList<int> &roles);
  bool flushPendingDocumentChanged();
  void setNotification(QString message);
  void clearProjectState();
  bool openProjectInternal(const QString &folder,
                           const QString &successNotification);
  void refreshPages();
  bool loadPageAt(int index);
  bool autosaveCurrent();
  std::string currentPageKey() const;
  bool saveProjectPresets(const std::string &preferredName = {});
  bool reloadPresets(const std::string &preferredName = {});

  DocumentModel document_;
  BoxesModel boxesModel_;
  std::vector<TextPreset> projectPresets_;
  std::unique_ptr<ProjectStore> store_;
  std::filesystem::path currentPage_;
  std::vector<std::filesystem::path> pagePaths_;
  QStringList pages_;
  int currentPageIndex_ = -1;
  int selectedIndex_ = -1;
  int selectedPresetIndex_ = -1;
  bool rawVisible_ = false;
  bool editingText_ = false;
  QString notification_;
  PageTextMap pageTexts_;
  PageTextPositionMap pageTextPositions_;
  int interactionDepth_ = 0;
  bool pendingDocumentChanged_ = false;
};

} // namespace textfx
