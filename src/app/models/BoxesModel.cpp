#include "app/models/BoxesModel.h"

#include "app/viewmodels/BoxRenderState.h"
#include "app/EffectMetadata.h"

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
  switch (role) {
  case IndexRole: return index.row();
  case TextRole: return QString::fromStdString(box.text);
  case XRole: return box.bounds.x;
  case YRole: return box.bounds.y;
  case WidthRole: return box.bounds.w;
  case HeightRole: return box.bounds.h;
  case RotationRole: return box.rotationDegrees;
  case FontFamilyRole: return QString::fromStdString(box.style.fontFamily);
  case ResolvedFontFamilyRole: return resolvedFontFamily(box.style);
  case FontSizeRole: return box.style.fontSize;
  case ColorRole: return QString::fromStdString(box.style.textColor);
  case LineSpacingRole: return box.style.lineSpacing;
  case LetterSpacingRole: return box.style.letterSpacing;
  case BoldRole: return box.style.bold;
  case ItalicRole: return box.style.italic;
  case UppercaseRole: return box.style.uppercase;
  case LowercaseRole: return box.style.lowercase && !box.style.uppercase;
  case AlignmentRole: return static_cast<int>(box.style.alignment);
  case OutlineRole: {
    const bool hasExplicitLayers =
        box.effects.outlineLayersSet || !box.effects.outlineLayers.empty();
    if (!box.effects.outlineLayers.empty())
      return box.effects.outlineLayers.front().enabled;
    return !hasExplicitLayers && box.effects.outlineEnabled;
  }
  case OutlineColorRole:
    return QString::fromStdString(box.effects.outlineLayers.empty()
                                      ? box.effects.outlineColor
                                      : box.effects.outlineLayers.front().color);
  case OutlineSizeRole: {
    const bool hasExplicitLayers =
        box.effects.outlineLayersSet || !box.effects.outlineLayers.empty();
    if (!box.effects.outlineLayers.empty())
      return box.effects.outlineLayers.front().size;
    return hasExplicitLayers ? 0 : box.effects.outlineSize;
  }
  case BlurRole: return box.effects.blurEnabled;
  case BlurSizeRole: return box.effects.blurSize;
  case ShadowRole: return box.effects.shadowEnabled;
  case ShadowColorRole: return QString::fromStdString(box.effects.shadowColor);
  case ShadowOffsetXRole: return box.effects.shadowOffsetX;
  case ShadowOffsetYRole: return box.effects.shadowOffsetY;
  case ShadowBlurSizeRole: return box.effects.shadowBlurSize;
  case GradientRole: return box.effects.gradientEnabled;
  case GradientDirectionRole: return box.effects.gradientDirection;
  case GradientColorARole: return QString::fromStdString(box.effects.gradientColorA);
  case GradientColorBRole: return QString::fromStdString(box.effects.gradientColorB);
  case PerspectiveRole: return box.effects.perspectiveEnabled;
  case PerspectiveNwRole: return pointValue(box.effects.perspectiveNw);
  case PerspectiveNeRole: return pointValue(box.effects.perspectiveNe);
  case PerspectiveSeRole: return pointValue(box.effects.perspectiveSe);
  case PerspectiveSwRole: return pointValue(box.effects.perspectiveSw);
  case PathRole: return box.effects.pathEnabled;
  case PathModeRole: return box.effects.pathMode;
  case PathPointsRole: return pointList(box.effects.pathPoints);
  case EffectsRole: return effectsValue(box.effects);
  default: return {};
  }
}

QHash<int, QByteArray> BoxesModel::roleNames() const {
  return boxRoleNames();
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
