#include "app/BoxRenderState.h"

#include "fonts/FontResolver.h"

#include <QFont>

#include <algorithm>

namespace textfx {
namespace {
QString toQString(const std::string &value) {
  return QString::fromStdString(value);
}

QString resolvedFontFamily(const TextStyle &style) {
  QFont font(toQString(style.fontFamily));
  font.setPixelSize(std::max(1, style.fontSize));
  font.setBold(style.bold);
  font.setItalic(style.italic);
  font.setLetterSpacing(QFont::AbsoluteSpacing, style.letterSpacing);
  return resolveFont(font).font.family();
}
} // namespace

QVariantList pointValue(const Point &point) { return {point.x, point.y}; }

QVariantList pointList(const std::vector<Point> &points) {
  QVariantList result;
  for (const auto &point : points)
    result.push_back(pointValue(point));
  return result;
}

namespace {
QVariantMap effectsMap(const TextEffects &effects) {
  return {{"outline",
           QVariantMap{{"enabled", effects.outlineEnabled},
                       {"color", toQString(effects.outlineColor)},
                       {"size", effects.outlineSize}}},
          {"blur",
           QVariantMap{{"enabled", effects.blurEnabled},
                       {"size", effects.blurSize}}},
          {"shadow",
           QVariantMap{{"enabled", effects.shadowEnabled},
                       {"color", toQString(effects.shadowColor)},
                       {"offsetX", effects.shadowOffsetX},
                       {"offsetY", effects.shadowOffsetY},
                       {"blurSize", effects.shadowBlurSize}}},
          {"gradient",
           QVariantMap{{"enabled", effects.gradientEnabled},
                       {"direction", effects.gradientDirection},
                       {"colorA", toQString(effects.gradientColorA)},
                       {"colorB", toQString(effects.gradientColorB)}}},
          {"path",
           QVariantMap{{"enabled", effects.pathEnabled},
                       {"mode", effects.pathMode},
                       {"points", pointList(effects.pathPoints)}}},
          {"perspective",
           QVariantMap{{"enabled", effects.perspectiveEnabled},
                       {"nw", pointValue(effects.perspectiveNw)},
                       {"ne", pointValue(effects.perspectiveNe)},
                       {"se", pointValue(effects.perspectiveSe)},
                       {"sw", pointValue(effects.perspectiveSw)}}}};
}
} // namespace

BoxRenderState mapBoxRenderState(const TextBox &box, int index) {
  return BoxRenderState{
      .index = index,
      .text = toQString(box.text),
      .x = box.bounds.x,
      .y = box.bounds.y,
      .width = box.bounds.w,
      .height = box.bounds.h,
      .rotation = box.rotationDegrees,
      .fontFamily = toQString(box.style.fontFamily),
      .resolvedFontFamily = resolvedFontFamily(box.style),
      .fontSize = box.style.fontSize,
      .color = toQString(box.style.textColor),
      .lineSpacing = box.style.lineSpacing,
      .letterSpacing = box.style.letterSpacing,
      .bold = box.style.bold,
      .italic = box.style.italic,
      .uppercase = box.style.uppercase,
      .lowercase = box.style.lowercase && !box.style.uppercase,
      .alignment = static_cast<int>(box.style.alignment),
      .outline = box.effects.outlineEnabled,
      .outlineColor = toQString(box.effects.outlineColor),
      .outlineSize = box.effects.outlineSize,
      .blur = box.effects.blurEnabled,
      .blurSize = box.effects.blurSize,
      .shadow = box.effects.shadowEnabled,
      .shadowColor = toQString(box.effects.shadowColor),
      .shadowOffsetX = box.effects.shadowOffsetX,
      .shadowOffsetY = box.effects.shadowOffsetY,
      .shadowBlurSize = box.effects.shadowBlurSize,
      .gradient = box.effects.gradientEnabled,
      .gradientDirection = box.effects.gradientDirection,
      .gradientColorA = toQString(box.effects.gradientColorA),
      .gradientColorB = toQString(box.effects.gradientColorB),
      .perspective = box.effects.perspectiveEnabled,
      .perspectiveNw = pointValue(box.effects.perspectiveNw),
      .perspectiveNe = pointValue(box.effects.perspectiveNe),
      .perspectiveSe = pointValue(box.effects.perspectiveSe),
      .perspectiveSw = pointValue(box.effects.perspectiveSw),
      .path = box.effects.pathEnabled,
      .pathMode = box.effects.pathMode,
      .pathPoints = pointList(box.effects.pathPoints),
      .effects = effectsMap(box.effects),
  };
}

} // namespace textfx
