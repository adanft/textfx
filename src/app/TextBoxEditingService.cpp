#include "app/TextBoxEditingService.h"

#include "core/AuthoringLimits.h"

#include <algorithm>
#include <cctype>

namespace textfx {
namespace {
std::string toStdString(const QString &value) { return value.toStdString(); }

std::string normalizeHexColor(QString color, const std::string &fallback) {
  color = color.trimmed();
  if (color.startsWith(QLatin1Char('#')))
    color.remove(0, 1);
  if (color.size() == 6)
    color += QStringLiteral("ff");
  if (color.size() != 8)
    return fallback;
  for (const auto ch : color) {
    if (!std::isxdigit(static_cast<unsigned char>(ch.toLatin1())))
      return fallback;
  }
  return color.toLower().toStdString();
}

Point clampedUnitPoint(double x, double y) {
  return {std::clamp(x, 0.0, 1.0), std::clamp(y, 0.0, 1.0)};
}

double distanceSquared(const Point &a, const Point &b) {
  const double dx = a.x - b.x;
  const double dy = a.y - b.y;
  return dx * dx + dy * dy;
}

Point midpoint(const Point &a, const Point &b) {
  return {(a.x + b.x) * 0.5, (a.y + b.y) * 0.5};
}

bool pathPointsEqual(const std::vector<Point> &lhs,
                     const std::vector<Point> &rhs) {
  if (lhs.size() != rhs.size())
    return false;
  for (std::size_t i = 0; i < lhs.size(); ++i) {
    if (lhs[i].x != rhs[i].x || lhs[i].y != rhs[i].y)
      return false;
  }
  return true;
}

void ensureDefaultPath(TextBox &box) {
  if (box.effects.pathPoints.size() < 3) {
    box.effects.pathPoints = {{0.0, 0.5}, {0.5, 0.5}, {1.0, 0.5}};
  }
}

void syncLegacyOutlineFields(TextBox &box) {
  if (box.effects.outlineLayers.empty()) {
    box.effects.outlineEnabled = false;
    return;
  }
  const auto &first = box.effects.outlineLayers.front();
  box.effects.outlineEnabled = first.enabled;
  box.effects.outlineColor = first.color;
  box.effects.outlineSize = first.size;
}

void ensureOutlineLayer(TextBox &box) {
  box.effects.outlineLayersSet = true;
  if (!box.effects.outlineLayers.empty())
    return;
  box.effects.outlineLayers.push_back({box.effects.outlineEnabled,
                                       box.effects.outlineColor,
                                       clampedEffectSize(box.effects.outlineSize)});
}

int defaultAddedOutlineLayerSize(const TextBox &box) {
  int maxSize = clampedEffectSize(box.effects.outlineSize);
  for (const auto &layer : box.effects.outlineLayers)
    maxSize = std::max(maxSize, clampedEffectSize(layer.size));
  return clampedEffectSize(maxSize + AddedOutlineLayerSizeStep);
}
} // namespace

void TextBoxEditingService::setFontFamily(TextBox &box, const QString &family) {
  const auto value = family.trimmed();
  if (!value.isEmpty())
    box.style.fontFamily = toStdString(value);
}

void TextBoxEditingService::setFontSize(TextBox &box, int size) {
  box.style.fontSize = std::clamp(size, MinFontSize, MaxFontSize);
}
void TextBoxEditingService::setTextColor(TextBox &box, const QString &color) {
  box.style.textColor = normalizeHexColor(color, box.style.textColor);
}
void TextBoxEditingService::setBold(TextBox &box, bool enabled) {
  box.style.bold = enabled;
}
void TextBoxEditingService::setItalic(TextBox &box, bool enabled) {
  box.style.italic = enabled;
}
void TextBoxEditingService::setUppercase(TextBox &box, bool enabled) {
  box.style.uppercase = enabled;
  if (enabled)
    box.style.lowercase = false;
}
void TextBoxEditingService::setLowercase(TextBox &box, bool enabled) {
  box.style.lowercase = enabled;
  if (enabled)
    box.style.uppercase = false;
}
void TextBoxEditingService::setAlignment(TextBox &box, int alignment) {
  box.style.alignment = static_cast<TextAlignment>(std::clamp(alignment, 0, 2));
}
void TextBoxEditingService::setLineSpacing(TextBox &box, int spacing) {
  box.style.lineSpacing = std::clamp(spacing, MinTextSpacing, MaxTextSpacing);
}
void TextBoxEditingService::setLetterSpacing(TextBox &box, int spacing) {
  box.style.letterSpacing = std::clamp(spacing, MinTextSpacing, MaxTextSpacing);
}

void TextBoxEditingService::move(TextBox &box, double dx, double dy) {
  box.bounds.x += dx;
  box.bounds.y += dy;
}

void TextBoxEditingService::resize(TextBox &box, double dw, double dh) {
  box.bounds.w = std::max(MinBoxSize, box.bounds.w + dw);
  box.bounds.h = std::max(MinBoxSize, box.bounds.h + dh);
}

void TextBoxEditingService::setBounds(TextBox &box, double x, double y,
                                      double w, double h) {
  box.bounds.x = x;
  box.bounds.y = y;
  box.bounds.w = std::max(MinBoxSize, w);
  box.bounds.h = std::max(MinBoxSize, h);
}

void TextBoxEditingService::rotate(TextBox &box, double degrees) {
  box.rotationDegrees += degrees;
}
void TextBoxEditingService::setRotation(TextBox &box, double degrees) {
  box.rotationDegrees = degrees;
}

void TextBoxEditingService::setOutlineEnabled(TextBox &box, bool enabled) {
  ensureOutlineLayer(box);
  box.effects.outlineLayers.front().enabled = enabled;
  syncLegacyOutlineFields(box);
}
void TextBoxEditingService::setOutlineColor(TextBox &box,
                                            const QString &color) {
  ensureOutlineLayer(box);
  box.effects.outlineLayers.front().color =
      normalizeHexColor(color, box.effects.outlineLayers.front().color);
  syncLegacyOutlineFields(box);
}
void TextBoxEditingService::setOutlineSize(TextBox &box, int size) {
  ensureOutlineLayer(box);
  box.effects.outlineLayers.front().size = clampedEffectSize(size);
  syncLegacyOutlineFields(box);
}

void TextBoxEditingService::addOutlineLayer(TextBox &box) {
  box.effects.outlineLayersSet = true;
  box.effects.outlineLayers.push_back(
      {true, box.effects.outlineColor, defaultAddedOutlineLayerSize(box)});
  syncLegacyOutlineFields(box);
}

bool TextBoxEditingService::removeOutlineLayer(TextBox &box, int index) {
  if (index < 0 || index >= static_cast<int>(box.effects.outlineLayers.size()))
    return false;
  box.effects.outlineLayersSet = true;
  box.effects.outlineLayers.erase(box.effects.outlineLayers.begin() + index);
  syncLegacyOutlineFields(box);
  return true;
}

bool TextBoxEditingService::setOutlineLayerEnabled(TextBox &box, int index,
                                                   bool enabled) {
  if (index < 0 || index >= static_cast<int>(box.effects.outlineLayers.size()))
    return false;
  box.effects.outlineLayersSet = true;
  box.effects.outlineLayers[static_cast<std::size_t>(index)].enabled = enabled;
  syncLegacyOutlineFields(box);
  return true;
}

bool TextBoxEditingService::setOutlineLayerColor(TextBox &box, int index,
                                                 const QString &color) {
  if (index < 0 || index >= static_cast<int>(box.effects.outlineLayers.size()))
    return false;
  box.effects.outlineLayersSet = true;
  auto &layer = box.effects.outlineLayers[static_cast<std::size_t>(index)];
  layer.color = normalizeHexColor(color, layer.color);
  syncLegacyOutlineFields(box);
  return true;
}

bool TextBoxEditingService::setOutlineLayerSize(TextBox &box, int index,
                                                int size) {
  if (index < 0 || index >= static_cast<int>(box.effects.outlineLayers.size()))
    return false;
  box.effects.outlineLayersSet = true;
  box.effects.outlineLayers[static_cast<std::size_t>(index)].size =
      clampedEffectSize(size);
  syncLegacyOutlineFields(box);
  return true;
}
void TextBoxEditingService::setBlurEnabled(TextBox &box, bool enabled) {
  box.effects.blurEnabled = enabled;
}
void TextBoxEditingService::setBlurSize(TextBox &box, int size) {
  box.effects.blurSize = clampedEffectSize(size);
}
void TextBoxEditingService::setShadowEnabled(TextBox &box, bool enabled) {
  box.effects.shadowEnabled = enabled;
}
void TextBoxEditingService::setShadowColor(TextBox &box, const QString &color) {
  box.effects.shadowColor = normalizeHexColor(color, box.effects.shadowColor);
}
void TextBoxEditingService::setShadowOffsetX(TextBox &box, int offset) {
  box.effects.shadowOffsetX = clampedShadowOffset(offset);
}
void TextBoxEditingService::setShadowOffsetY(TextBox &box, int offset) {
  box.effects.shadowOffsetY = clampedShadowOffset(offset);
}
void TextBoxEditingService::setShadowBlurSize(TextBox &box, int size) {
  box.effects.shadowBlurSize = clampedEffectSize(size);
}
void TextBoxEditingService::setGradientEnabled(TextBox &box, bool enabled) {
  box.effects.gradientEnabled = enabled;
}
void TextBoxEditingService::setGradientDirection(TextBox &box, int direction) {
  box.effects.gradientDirection = std::clamp(direction, 0, 1);
}
void TextBoxEditingService::setGradientColorA(TextBox &box,
                                              const QString &color) {
  box.effects.gradientColorA =
      normalizeHexColor(color, box.effects.gradientColorA);
}
void TextBoxEditingService::setGradientColorB(TextBox &box,
                                              const QString &color) {
  box.effects.gradientColorB =
      normalizeHexColor(color, box.effects.gradientColorB);
}

void TextBoxEditingService::setPerspectiveEnabled(TextBox &box, bool enabled) {
  box.effects.perspectiveEnabled = enabled;
}

void TextBoxEditingService::resetPerspective(TextBox &box) {
  box.effects.perspectiveNw = {};
  box.effects.perspectiveNe = {};
  box.effects.perspectiveSe = {};
  box.effects.perspectiveSw = {};
}

void TextBoxEditingService::setPerspectiveHandle(TextBox &box,
                                                 const QString &corner,
                                                 double x, double y) {
  box.effects.perspectiveEnabled = true;
  const auto point = Point{x, y};
  if (corner == QStringLiteral("ne"))
    box.effects.perspectiveNe = point;
  else if (corner == QStringLiteral("se"))
    box.effects.perspectiveSe = point;
  else if (corner == QStringLiteral("sw"))
    box.effects.perspectiveSw = point;
  else if (corner == QStringLiteral("n")) {
    box.effects.perspectiveNw.y = y;
    box.effects.perspectiveNe.y = y;
  } else if (corner == QStringLiteral("e")) {
    box.effects.perspectiveNe.x = x;
    box.effects.perspectiveSe.x = x;
  } else if (corner == QStringLiteral("s")) {
    box.effects.perspectiveSw.y = y;
    box.effects.perspectiveSe.y = y;
  } else if (corner == QStringLiteral("w")) {
    box.effects.perspectiveNw.x = x;
    box.effects.perspectiveSw.x = x;
  } else
    box.effects.perspectiveNw = point;
}

void TextBoxEditingService::setPathEnabled(TextBox &box, bool enabled) {
  box.effects.pathEnabled = enabled;
  if (enabled)
    ensureDefaultPath(box);
}

void TextBoxEditingService::setPathMode(TextBox &box, int mode) {
  box.effects.pathMode = std::clamp(mode, 0, 1);
}

void TextBoxEditingService::resetPath(TextBox &box) {
  box.effects.pathPoints = {{0.0, 0.5}, {0.5, 0.5}, {1.0, 0.5}};
}

void TextBoxEditingService::addPathPoint(TextBox &box) {
  box.effects.pathEnabled = true;
  ensureDefaultPath(box);
  std::size_t insertIndex = 1;
  double longest = -1.0;
  for (std::size_t i = 0; i + 1 < box.effects.pathPoints.size(); ++i) {
    const double length = distanceSquared(box.effects.pathPoints[i],
                                          box.effects.pathPoints[i + 1]);
    if (length > longest) {
      longest = length;
      insertIndex = i + 1;
    }
  }
  box.effects.pathPoints.insert(
      box.effects.pathPoints.begin() + static_cast<std::ptrdiff_t>(insertIndex),
      midpoint(box.effects.pathPoints[insertIndex - 1],
               box.effects.pathPoints[insertIndex]));
}

bool TextBoxEditingService::setPathHandle(TextBox &box, int index, double x,
                                          double y) {
  const bool wasPathEnabled = box.effects.pathEnabled;
  const auto previousPoints = box.effects.pathPoints;
  box.effects.pathEnabled = true;
  ensureDefaultPath(box);
  if (index < 0 || index >= static_cast<int>(box.effects.pathPoints.size()))
    return box.effects.pathEnabled != wasPathEnabled ||
           !pathPointsEqual(box.effects.pathPoints, previousPoints);
  box.effects.pathPoints[static_cast<std::size_t>(index)] =
      clampedUnitPoint(x, y);
  return true;
}

} // namespace textfx
