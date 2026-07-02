#include "app/EffectMetadata.h"

#include <algorithm>

namespace textfx {
namespace {
using Role = BoxesModel::Role;

constexpr QStringView outlineName() { return u"outline"; }
constexpr QStringView blurName() { return u"blur"; }
constexpr QStringView shadowName() { return u"shadow"; }
constexpr QStringView gradientName() { return u"gradient"; }
constexpr QStringView pathName() { return u"path"; }
constexpr QStringView perspectiveName() { return u"perspective"; }
} // namespace

QList<RoleMetadata> boxRoleMetadata() {
  return {{Role::IndexRole, "boxIndex"},
          {Role::TextRole, "boxText"},
          {Role::XRole, "boxX"},
          {Role::YRole, "boxY"},
          {Role::WidthRole, "boxWidth"},
          {Role::HeightRole, "boxHeight"},
          {Role::RotationRole, "boxRotation"},
          {Role::FontFamilyRole, "boxFontFamily"},
          {Role::ResolvedFontFamilyRole, "boxResolvedFontFamily"},
          {Role::FontSizeRole, "boxFontSize"},
          {Role::ColorRole, "boxColor"},
          {Role::LineSpacingRole, "boxLineSpacing"},
          {Role::LetterSpacingRole, "boxLetterSpacing"},
          {Role::BoldRole, "boxBold"},
          {Role::ItalicRole, "boxItalic"},
          {Role::UppercaseRole, "boxUppercase"},
          {Role::LowercaseRole, "boxLowercase"},
          {Role::AlignmentRole, "boxAlignment"},
          {Role::OutlineRole, "boxOutline"},
          {Role::OutlineColorRole, "boxOutlineColor"},
          {Role::OutlineSizeRole, "boxOutlineSize"},
          {Role::BlurRole, "boxBlur"},
          {Role::BlurSizeRole, "boxBlurSize"},
          {Role::ShadowRole, "boxShadow"},
          {Role::ShadowColorRole, "boxShadowColor"},
          {Role::ShadowOffsetXRole, "boxShadowOffsetX"},
          {Role::ShadowOffsetYRole, "boxShadowOffsetY"},
          {Role::ShadowBlurSizeRole, "boxShadowBlurSize"},
          {Role::GradientRole, "boxGradient"},
          {Role::GradientDirectionRole, "boxGradientDirection"},
          {Role::GradientColorARole, "boxGradientColorA"},
          {Role::GradientColorBRole, "boxGradientColorB"},
          {Role::PerspectiveRole, "boxPerspective"},
          {Role::PerspectiveNwRole, "boxPerspectiveNw"},
          {Role::PerspectiveNeRole, "boxPerspectiveNe"},
          {Role::PerspectiveSeRole, "boxPerspectiveSe"},
          {Role::PerspectiveSwRole, "boxPerspectiveSw"},
          {Role::PathRole, "boxPath"},
          {Role::PathModeRole, "boxPathMode"},
          {Role::PathPointsRole, "boxPathPoints"},
          {Role::EffectsRole, "boxEffects"}};
}

QHash<int, QByteArray> boxRoleNames() {
  QHash<int, QByteArray> names;
  for (const auto &metadata : boxRoleMetadata())
    names.insert(metadata.role, metadata.name);
  return names;
}

QList<int> allBoxRoles() {
  QList<int> roles;
  for (const auto &metadata : boxRoleMetadata())
    roles.push_back(metadata.role);
  return roles;
}

QList<EffectDescriptor> effectDescriptors() {
  return {{EffectId::Outline,
           outlineName(),
           Role::OutlineRole,
           {Role::OutlineRole, Role::OutlineColorRole, Role::OutlineSizeRole,
            Role::EffectsRole}},
          {EffectId::Blur,
           blurName(),
           Role::BlurRole,
           {Role::BlurRole, Role::BlurSizeRole, Role::EffectsRole}},
          {EffectId::Shadow,
           shadowName(),
           Role::ShadowRole,
           {Role::ShadowRole, Role::ShadowColorRole, Role::ShadowOffsetXRole,
            Role::ShadowOffsetYRole, Role::ShadowBlurSizeRole,
            Role::EffectsRole}},
          {EffectId::Gradient,
           gradientName(),
           Role::GradientRole,
           {Role::GradientRole, Role::GradientDirectionRole,
            Role::GradientColorARole, Role::GradientColorBRole,
            Role::EffectsRole}},
          {EffectId::Path,
           pathName(),
           Role::PathRole,
           {Role::PathRole, Role::PathModeRole, Role::PathPointsRole,
            Role::EffectsRole}},
          {EffectId::Perspective,
           perspectiveName(),
           Role::PerspectiveRole,
           {Role::PerspectiveRole, Role::PerspectiveNwRole,
            Role::PerspectiveNeRole, Role::PerspectiveSeRole,
            Role::PerspectiveSwRole, Role::EffectsRole}}};
}

QList<int> effectRoles(EffectId effect) {
  const auto descriptors = effectDescriptors();
  const auto it = std::ranges::find_if(descriptors, [effect](const auto &item) {
    return item.id == effect;
  });
  return it == descriptors.cend() ? QList<int>{} : it->roles;
}

QList<int> allEffectRoles() {
  QList<int> roles;
  for (const auto &descriptor : effectDescriptors()) {
    for (const int role : descriptor.roles) {
      if (!roles.contains(role))
        roles.push_back(role);
    }
  }
  return roles;
}

} // namespace textfx
