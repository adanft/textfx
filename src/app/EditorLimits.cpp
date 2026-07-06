#include "app/EditorLimits.h"

#include "core/AuthoringLimits.h"

namespace textfx {

EditorLimits::EditorLimits(QObject *parent) : QObject(parent) {}

double EditorLimits::minimumBoxSize() const { return MinBoxSize; }
int EditorLimits::minimumFontSize() const { return MinFontSize; }
int EditorLimits::maximumFontSize() const { return MaxFontSize; }
int EditorLimits::minimumTextSpacing() const { return MinTextSpacing; }
int EditorLimits::maximumTextSpacing() const { return MaxTextSpacing; }
int EditorLimits::minimumEffectSize() const { return MinEffectSize; }
int EditorLimits::maximumEffectSize() const { return MaxEffectSize; }
int EditorLimits::minimumShadowOffset() const { return MinShadowOffset; }
int EditorLimits::maximumShadowOffset() const { return MaxShadowOffset; }

} // namespace textfx
