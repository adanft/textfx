#include "app/EditorViewModels.h"

#include "core/ProjectStore.h"
#include "fonts/FontResolver.h"

#include <QFont>

#include <algorithm>

namespace textfx::EditorViewModels {
namespace {
QString toQString(const std::string& value) { return QString::fromStdString(value); }

QString resolvedFontFamily(const TextStyle& style)
{
    QFont font(toQString(style.fontFamily));
    font.setPixelSize(std::max(1, style.fontSize));
    font.setBold(style.bold);
    font.setItalic(style.italic);
    font.setLetterSpacing(QFont::AbsoluteSpacing, style.letterSpacing);
    return resolveFont(font).font.family();
}
} // namespace

QVariantList pointList(const std::vector<Point>& points)
{
    QVariantList result;
    for (const auto& point : points) result.push_back(QVariantList{point.x, point.y});
    return result;
}

QVariantMap textBoxMap(const TextBox& box, int index)
{
    return {
        {"index", index},
        {"text", toQString(box.text)},
        {"x", box.bounds.x},
        {"y", box.bounds.y},
        {"w", box.bounds.w},
        {"h", box.bounds.h},
        {"rotation", box.rotationDegrees},
        {"fontFamily", toQString(box.style.fontFamily)},
        {"resolvedFontFamily", resolvedFontFamily(box.style)},
        {"fontSize", box.style.fontSize},
        {"color", toQString(box.style.textColor)},
        {"lineSpacing", box.style.lineSpacing},
        {"letterSpacing", box.style.letterSpacing},
        {"bold", box.style.bold},
        {"italic", box.style.italic},
        {"uppercase", box.style.uppercase},
        {"alignment", static_cast<int>(box.style.alignment)},
        {"outline", box.effects.outlineEnabled},
        {"outlineColor", toQString(box.effects.outlineColor)},
        {"outlineSize", box.effects.outlineSize},
        {"blur", box.effects.blurEnabled},
        {"blurSize", box.effects.blurSize},
        {"shadow", box.effects.shadowEnabled},
        {"shadowColor", toQString(box.effects.shadowColor)},
        {"shadowOffsetX", box.effects.shadowOffsetX},
        {"shadowOffsetY", box.effects.shadowOffsetY},
        {"shadowBlurSize", box.effects.shadowBlurSize},
        {"gradient", box.effects.gradientEnabled},
        {"gradientDirection", box.effects.gradientDirection},
        {"gradientColorA", toQString(box.effects.gradientColorA)},
        {"gradientColorB", toQString(box.effects.gradientColorB)},
        {"perspective", box.effects.perspectiveEnabled},
        {"perspectiveNw", QVariantList{box.effects.perspectiveNw.x, box.effects.perspectiveNw.y}},
        {"perspectiveNe", QVariantList{box.effects.perspectiveNe.x, box.effects.perspectiveNe.y}},
        {"perspectiveSe", QVariantList{box.effects.perspectiveSe.x, box.effects.perspectiveSe.y}},
        {"perspectiveSw", QVariantList{box.effects.perspectiveSw.x, box.effects.perspectiveSw.y}},
        {"path", box.effects.pathEnabled},
        {"pathMode", box.effects.pathMode},
        {"pathPoints", pointList(box.effects.pathPoints)},
    };
}

QVariantList textBoxList(const std::vector<TextBox>& boxes)
{
    QVariantList list;
    int index = 0;
    for (const auto& box : boxes) list.push_back(textBoxMap(box, index++));
    return list;
}

QVariantList layerList(const std::vector<TextBox>& boxes)
{
    QVariantList list;
    for (int index = static_cast<int>(boxes.size()) - 1; index >= 0; --index) {
        list.push_back(textBoxMap(boxes[static_cast<std::size_t>(index)], index));
    }
    return list;
}

QVariantMap presetMap(const TextPreset& preset)
{
    return {{"name", toQString(preset.name)}, {"fontFamily", toQString(preset.style.fontFamily)}, {"fontSize", preset.style.fontSize}, {"isDefault", ProjectStore::isDefaultTextPresetName(preset.name)}};
}

QVariantList presetList(const std::vector<TextPreset>& presets)
{
    QVariantList list;
    for (const auto& preset : presets) list.push_back(presetMap(preset));
    return list;
}

} // namespace textfx::EditorViewModels
