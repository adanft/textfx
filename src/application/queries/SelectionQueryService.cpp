#include "application/queries/SelectionQueryService.h"

#include "app/viewmodels/EditorViewModels.h"
#include "application/queries/EffectMetadata.h"

#include <utility>

namespace textfx::SelectionQueryService {
namespace {
using Role = BoxRole;
}

const TextBox *selectedBox(const std::vector<TextBox> &boxes, int selectedIndex) {
  if (selectedIndex < 0 || selectedIndex >= static_cast<int>(boxes.size()))
    return nullptr;
  return &boxes[static_cast<std::size_t>(selectedIndex)];
}

TextBox *selectedBox(std::vector<TextBox> &boxes, int selectedIndex) {
  return const_cast<TextBox *>(
      selectedBox(std::as_const(boxes), selectedIndex));
}

QVariant selectedBoxViewModel(const std::vector<TextBox> &boxes, int selectedIndex) {
  const auto *box = selectedBox(boxes, selectedIndex);
  if (!box)
    return {};
  return EditorViewModels::textBoxMap(*box, selectedIndex);
}

bool roleAffectsSelectedBoxState(int role) {
  switch (role) {
  case Role::TextRole:
  case Role::RotationRole:
  case Role::FontFamilyRole:
  case Role::FontSizeRole:
  case Role::ColorRole:
  case Role::LineSpacingRole:
  case Role::LetterSpacingRole:
  case Role::BoldRole:
  case Role::ItalicRole:
  case Role::UppercaseRole:
  case Role::LowercaseRole:
  case Role::AlignmentRole:
    return true;
  default:
    break;
  }

  const QList<int> effectRoles = allEffectRoles();
  return effectRoles.contains(role);
}

bool rolesAffectSelectedBoxState(const QVariantList &roles,
                                 const QHash<int, QByteArray> &knownRoles) {
  if (roles.empty())
    return true;

  for (const QVariant &roleValue : roles) {
    bool ok = false;
    const int role = roleValue.toInt(&ok);
    if (!ok || !knownRoles.contains(role))
      return true;
    if (roleAffectsSelectedBoxState(role))
      return true;
  }
  return false;
}

} // namespace textfx::SelectionQueryService
