#pragma once

#include "application/queries/BoxRoles.h"
#include "domain/document/DocumentModel.h"

#include <QAbstractListModel>
#include <QList>

namespace textfx {

class BoxesModel final : public QAbstractListModel {
  Q_OBJECT

public:
  using Role = BoxRole;

  static constexpr Role IndexRole = textfx::IndexRole;
  static constexpr Role TextRole = textfx::TextRole;
  static constexpr Role XRole = textfx::XRole;
  static constexpr Role YRole = textfx::YRole;
  static constexpr Role WidthRole = textfx::WidthRole;
  static constexpr Role HeightRole = textfx::HeightRole;
  static constexpr Role RotationRole = textfx::RotationRole;
  static constexpr Role FontFamilyRole = textfx::FontFamilyRole;
  static constexpr Role ResolvedFontFamilyRole = textfx::ResolvedFontFamilyRole;
  static constexpr Role FontSizeRole = textfx::FontSizeRole;
  static constexpr Role ColorRole = textfx::ColorRole;
  static constexpr Role LineSpacingRole = textfx::LineSpacingRole;
  static constexpr Role LetterSpacingRole = textfx::LetterSpacingRole;
  static constexpr Role BoldRole = textfx::BoldRole;
  static constexpr Role ItalicRole = textfx::ItalicRole;
  static constexpr Role UppercaseRole = textfx::UppercaseRole;
  static constexpr Role LowercaseRole = textfx::LowercaseRole;
  static constexpr Role AlignmentRole = textfx::AlignmentRole;
  static constexpr Role OutlineRole = textfx::OutlineRole;
  static constexpr Role OutlineColorRole = textfx::OutlineColorRole;
  static constexpr Role OutlineSizeRole = textfx::OutlineSizeRole;
  static constexpr Role BlurRole = textfx::BlurRole;
  static constexpr Role BlurSizeRole = textfx::BlurSizeRole;
  static constexpr Role ShadowRole = textfx::ShadowRole;
  static constexpr Role ShadowColorRole = textfx::ShadowColorRole;
  static constexpr Role ShadowOffsetXRole = textfx::ShadowOffsetXRole;
  static constexpr Role ShadowOffsetYRole = textfx::ShadowOffsetYRole;
  static constexpr Role ShadowBlurSizeRole = textfx::ShadowBlurSizeRole;
  static constexpr Role GradientRole = textfx::GradientRole;
  static constexpr Role GradientDirectionRole = textfx::GradientDirectionRole;
  static constexpr Role GradientColorARole = textfx::GradientColorARole;
  static constexpr Role GradientColorBRole = textfx::GradientColorBRole;
  static constexpr Role PerspectiveRole = textfx::PerspectiveRole;
  static constexpr Role PerspectiveNwRole = textfx::PerspectiveNwRole;
  static constexpr Role PerspectiveNeRole = textfx::PerspectiveNeRole;
  static constexpr Role PerspectiveSeRole = textfx::PerspectiveSeRole;
  static constexpr Role PerspectiveSwRole = textfx::PerspectiveSwRole;
  static constexpr Role PathRole = textfx::PathRole;
  static constexpr Role PathModeRole = textfx::PathModeRole;
  static constexpr Role PathPointsRole = textfx::PathPointsRole;
  static constexpr Role EffectsRole = textfx::EffectsRole;

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
