#pragma once

#include "app/models/BoxesModel.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QStringView>

namespace textfx {

enum class EffectId {
  Outline,
  Blur,
  Shadow,
  Gradient,
  Path,
  Perspective,
};

struct RoleMetadata {
  BoxesModel::Role role;
  QByteArray name;
};

struct EffectDescriptor {
  EffectId id;
  QStringView name;
  BoxesModel::Role enabledRole;
  QList<int> roles;
};

QList<RoleMetadata> boxRoleMetadata();
QHash<int, QByteArray> boxRoleNames();
QList<int> allBoxRoles();

QList<EffectDescriptor> effectDescriptors();
QList<int> effectRoles(EffectId effect);
QList<int> allEffectRoles();

} // namespace textfx
