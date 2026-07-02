#pragma once

#include "core/DocumentModel.h"

#include <QAbstractListModel>
#include <QList>

namespace textfx {

class BoxesModel final : public QAbstractListModel {
  Q_OBJECT

public:
  enum Role {
    IndexRole = Qt::UserRole + 1,
    TextRole,
    XRole,
    YRole,
    WidthRole,
    HeightRole,
    RotationRole,
    FontFamilyRole,
    ResolvedFontFamilyRole,
    FontSizeRole,
    ColorRole,
    LineSpacingRole,
    LetterSpacingRole,
    BoldRole,
    ItalicRole,
    UppercaseRole,
    AlignmentRole,
    OutlineRole,
    OutlineColorRole,
    OutlineSizeRole,
    BlurRole,
    BlurSizeRole,
    ShadowRole,
    ShadowColorRole,
    ShadowOffsetXRole,
    ShadowOffsetYRole,
    ShadowBlurSizeRole,
    GradientRole,
    GradientDirectionRole,
    GradientColorARole,
    GradientColorBRole,
    PerspectiveRole,
    PerspectiveNwRole,
    PerspectiveNeRole,
    PerspectiveSeRole,
    PerspectiveSwRole,
    PathRole,
    PathModeRole,
    PathPointsRole,
  };

  explicit BoxesModel(DocumentModel &document, QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = {}) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QHash<int, QByteArray> roleNames() const override;

  void notifyBoxChanged(int row, const QList<int> &roles);
  void beginInsertBox(int row);
  void endInsertBox();
  void beginRemoveBox(int row);
  void endRemoveBox();
  void beginResetBoxes();
  void endResetBoxes();

private:
  DocumentModel &document_;
};

} // namespace textfx
