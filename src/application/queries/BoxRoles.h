#pragma once

#include <Qt>

namespace textfx {

enum BoxRole {
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
  LowercaseRole,
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
  EffectsRole,
};

} // namespace textfx
