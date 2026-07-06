#pragma once

#include "core/DocumentModel.h"

#include <QByteArray>
#include <QHash>
#include <QVariant>
#include <QVariantList>

#include <vector>

namespace textfx::SelectionQueryService {

const TextBox *selectedBox(const std::vector<TextBox> &boxes, int selectedIndex);
TextBox *selectedBox(std::vector<TextBox> &boxes, int selectedIndex);
QVariant selectedBoxViewModel(const std::vector<TextBox> &boxes, int selectedIndex);
bool roleAffectsSelectedBoxState(int role);
bool rolesAffectSelectedBoxState(const QVariantList &roles,
                                 const QHash<int, QByteArray> &knownRoles);

} // namespace textfx::SelectionQueryService
