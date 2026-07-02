#include "app/BoxesModel.h"

#include "app/BoxRenderState.h"

namespace textfx {

BoxesModel::BoxesModel(DocumentModel &document, QObject *parent)
    : QAbstractListModel(parent), document_(document) {}

int BoxesModel::rowCount(const QModelIndex &parent) const {
  if (parent.isValid())
    return 0;
  return static_cast<int>(document_.textBoxes().size());
}

QVariant BoxesModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= static_cast<int>(document_.textBoxes().size()))
    return {};

  const auto &box = document_.textBoxes().at(static_cast<std::size_t>(index.row()));
  const auto state = mapBoxRenderState(box, index.row());
  switch (role) {
  case IndexRole: return state.index;
  case TextRole: return state.text;
  case XRole: return state.x;
  case YRole: return state.y;
  case WidthRole: return state.width;
  case HeightRole: return state.height;
  case RotationRole: return state.rotation;
  case FontFamilyRole: return state.fontFamily;
  case ResolvedFontFamilyRole: return state.resolvedFontFamily;
  case FontSizeRole: return state.fontSize;
  case ColorRole: return state.color;
  case LineSpacingRole: return state.lineSpacing;
  case LetterSpacingRole: return state.letterSpacing;
  case BoldRole: return state.bold;
  case ItalicRole: return state.italic;
  case UppercaseRole: return state.uppercase;
  case AlignmentRole: return state.alignment;
  case OutlineRole: return state.outline;
  case OutlineColorRole: return state.outlineColor;
  case OutlineSizeRole: return state.outlineSize;
  case BlurRole: return state.blur;
  case BlurSizeRole: return state.blurSize;
  case ShadowRole: return state.shadow;
  case ShadowColorRole: return state.shadowColor;
  case ShadowOffsetXRole: return state.shadowOffsetX;
  case ShadowOffsetYRole: return state.shadowOffsetY;
  case ShadowBlurSizeRole: return state.shadowBlurSize;
  case GradientRole: return state.gradient;
  case GradientDirectionRole: return state.gradientDirection;
  case GradientColorARole: return state.gradientColorA;
  case GradientColorBRole: return state.gradientColorB;
  case PerspectiveRole: return state.perspective;
  case PerspectiveNwRole: return state.perspectiveNw;
  case PerspectiveNeRole: return state.perspectiveNe;
  case PerspectiveSeRole: return state.perspectiveSe;
  case PerspectiveSwRole: return state.perspectiveSw;
  case PathRole: return state.path;
  case PathModeRole: return state.pathMode;
  case PathPointsRole: return state.pathPoints;
  default: return {};
  }
}

QHash<int, QByteArray> BoxesModel::roleNames() const {
  return {{IndexRole, "boxIndex"},
          {TextRole, "boxText"},
          {XRole, "boxX"},
          {YRole, "boxY"},
          {WidthRole, "boxWidth"},
          {HeightRole, "boxHeight"},
          {RotationRole, "boxRotation"},
          {FontFamilyRole, "boxFontFamily"},
          {ResolvedFontFamilyRole, "boxResolvedFontFamily"},
          {FontSizeRole, "boxFontSize"},
          {ColorRole, "boxColor"},
          {LineSpacingRole, "boxLineSpacing"},
          {LetterSpacingRole, "boxLetterSpacing"},
          {BoldRole, "boxBold"},
          {ItalicRole, "boxItalic"},
          {UppercaseRole, "boxUppercase"},
          {AlignmentRole, "boxAlignment"},
          {OutlineRole, "boxOutline"},
          {OutlineColorRole, "boxOutlineColor"},
          {OutlineSizeRole, "boxOutlineSize"},
          {BlurRole, "boxBlur"},
          {BlurSizeRole, "boxBlurSize"},
          {ShadowRole, "boxShadow"},
          {ShadowColorRole, "boxShadowColor"},
          {ShadowOffsetXRole, "boxShadowOffsetX"},
          {ShadowOffsetYRole, "boxShadowOffsetY"},
          {ShadowBlurSizeRole, "boxShadowBlurSize"},
          {GradientRole, "boxGradient"},
          {GradientDirectionRole, "boxGradientDirection"},
          {GradientColorARole, "boxGradientColorA"},
          {GradientColorBRole, "boxGradientColorB"},
          {PerspectiveRole, "boxPerspective"},
          {PerspectiveNwRole, "boxPerspectiveNw"},
          {PerspectiveNeRole, "boxPerspectiveNe"},
          {PerspectiveSeRole, "boxPerspectiveSe"},
          {PerspectiveSwRole, "boxPerspectiveSw"},
          {PathRole, "boxPath"},
          {PathModeRole, "boxPathMode"},
          {PathPointsRole, "boxPathPoints"}};
}

void BoxesModel::notifyBoxChanged(int row, const QList<int> &roles) {
  if (row < 0 || row >= rowCount())
    return;
  const QModelIndex changed = index(row, 0);
  emit dataChanged(changed, changed, roles);
}

void BoxesModel::beginInsertBox(int row) { beginInsertRows({}, row, row); }
void BoxesModel::endInsertBox() { endInsertRows(); }
void BoxesModel::beginRemoveBox(int row) { beginRemoveRows({}, row, row); }
void BoxesModel::endRemoveBox() { endRemoveRows(); }
void BoxesModel::beginResetBoxes() { beginResetModel(); }
void BoxesModel::endResetBoxes() { endResetModel(); }

} // namespace textfx
