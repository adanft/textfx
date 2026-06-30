#include "app/TextBoxClipboardService.h"

#include "app/EditorViewModels.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include <algorithm>

namespace textfx {
namespace {
double numberFromJson(const QJsonObject& object, const char* key, double fallback)
{
    const auto value = object.value(QLatin1String(key));
    return value.isDouble() ? value.toDouble() : fallback;
}

int intFromJson(const QJsonObject& object, const char* key, int fallback)
{
    const auto value = object.value(QLatin1String(key));
    return value.isDouble() ? value.toInt() : fallback;
}

bool boolFromJson(const QJsonObject& object, const char* key, bool fallback)
{
    const auto value = object.value(QLatin1String(key));
    return value.isBool() ? value.toBool() : fallback;
}

std::string stringFromJson(const QJsonObject& object, const char* key, const std::string& fallback)
{
    const auto value = object.value(QLatin1String(key));
    return value.isString() ? value.toString().toStdString() : fallback;
}

Point pointFromJson(const QJsonValue& value, Point fallback = {})
{
    const auto array = value.toArray();
    return array.size() >= 2 ? Point{array.at(0).toDouble(), array.at(1).toDouble()} : fallback;
}

std::vector<Point> pointsFromJson(const QJsonValue& value)
{
    std::vector<Point> points;
    for (const auto& item : value.toArray()) points.push_back(pointFromJson(item));
    return points;
}

bool isTextBoxPayload(const QJsonObject& object)
{
    return object.contains("text") && object.contains("x") && object.contains("y");
}

TextBox boxFromClipboardJson(const QJsonObject& object)
{
    TextBox box;
    box.text = stringFromJson(object, "text", box.text);
    box.bounds.x = numberFromJson(object, "x", box.bounds.x) + 16.0;
    box.bounds.y = numberFromJson(object, "y", box.bounds.y) + 16.0;
    box.bounds.w = std::max(20.0, numberFromJson(object, "w", box.bounds.w));
    box.bounds.h = std::max(20.0, numberFromJson(object, "h", box.bounds.h));
    box.rotationDegrees = numberFromJson(object, "rotation", box.rotationDegrees);

    auto& style = box.style;
    style.fontFamily = stringFromJson(object, "fontFamily", style.fontFamily);
    style.fontSize = intFromJson(object, "fontSize", style.fontSize);
    style.textColor = stringFromJson(object, "color", style.textColor);
    style.lineSpacing = intFromJson(object, "lineSpacing", style.lineSpacing);
    style.letterSpacing = intFromJson(object, "letterSpacing", style.letterSpacing);
    style.bold = boolFromJson(object, "bold", style.bold);
    style.italic = boolFromJson(object, "italic", style.italic);
    style.uppercase = boolFromJson(object, "uppercase", style.uppercase);
    style.alignment = static_cast<TextAlignment>(intFromJson(object, "alignment", static_cast<int>(style.alignment)));

    auto& effects = box.effects;
    effects.outlineEnabled = boolFromJson(object, "outline", effects.outlineEnabled);
    effects.outlineColor = stringFromJson(object, "outlineColor", effects.outlineColor);
    effects.outlineSize = intFromJson(object, "outlineSize", effects.outlineSize);
    effects.blurEnabled = boolFromJson(object, "blur", effects.blurEnabled);
    effects.blurSize = intFromJson(object, "blurSize", effects.blurSize);
    effects.shadowEnabled = boolFromJson(object, "shadow", effects.shadowEnabled);
    effects.shadowColor = stringFromJson(object, "shadowColor", effects.shadowColor);
    effects.shadowOffsetX = intFromJson(object, "shadowOffsetX", effects.shadowOffsetX);
    effects.shadowOffsetY = intFromJson(object, "shadowOffsetY", effects.shadowOffsetY);
    effects.shadowBlurSize = intFromJson(object, "shadowBlurSize", effects.shadowBlurSize);
    effects.gradientEnabled = boolFromJson(object, "gradient", effects.gradientEnabled);
    effects.gradientDirection = intFromJson(object, "gradientDirection", effects.gradientDirection);
    effects.gradientColorA = stringFromJson(object, "gradientColorA", effects.gradientColorA);
    effects.gradientColorB = stringFromJson(object, "gradientColorB", effects.gradientColorB);
    effects.perspectiveEnabled = boolFromJson(object, "perspective", effects.perspectiveEnabled);
    effects.pathEnabled = boolFromJson(object, "path", effects.pathEnabled);
    effects.pathMode = std::clamp(intFromJson(object, "pathMode", effects.pathMode), 0, 1);
    effects.perspectiveNw = pointFromJson(object.value("perspectiveNw"), effects.perspectiveNw);
    effects.perspectiveNe = pointFromJson(object.value("perspectiveNe"), effects.perspectiveNe);
    effects.perspectiveSe = pointFromJson(object.value("perspectiveSe"), effects.perspectiveSe);
    effects.perspectiveSw = pointFromJson(object.value("perspectiveSw"), effects.perspectiveSw);
    effects.pathPoints = pointsFromJson(object.value("pathPoints"));
    return box;
}
} // namespace

QString TextBoxClipboardService::serialize(const TextBox& box, int index)
{
    const auto map = EditorViewModels::textBoxMap(box, index);
    return QString::fromUtf8(QJsonDocument::fromVariant(map).toJson(QJsonDocument::Compact));
}

TextBox TextBoxClipboardService::deserializeOrPlainText(const QString& clipboardText)
{
    const auto json = QJsonDocument::fromJson(clipboardText.toUtf8());
    if (json.isObject() && isTextBoxPayload(json.object())) {
        return boxFromClipboardJson(json.object());
    }

    TextBox box;
    box.text = clipboardText.isEmpty() ? "Text" : clipboardText.toStdString();
    box.bounds = {32.0, 32.0, 220.0, 80.0};
    return box;
}

} // namespace textfx
