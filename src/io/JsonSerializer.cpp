#include "io/JsonSerializer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <fstream>

namespace textfx {
namespace {
constexpr int MaxShadowBlurSize = 36;

void setError(std::string *error, std::string message) {
  if (error != nullptr)
    *error = std::move(message);
}

std::string readFile(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

bool writeFile(const std::filesystem::path &path, const QByteArray &bytes,
               std::string *error) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    setError(error, "Could not open save file.");
    return false;
  }
  output.write(bytes.constData(), bytes.size());
  return true;
}

QJsonArray pointToJson(Point point) { return {point.x, point.y}; }

Point pointFromJson(const QJsonValue &value) {
  const auto array = value.toArray();
  if (array.size() < 2)
    return {};
  return {array.at(0).toDouble(), array.at(1).toDouble()};
}

QJsonObject perspectiveToJson(const TextEffects &effects) {
  return {{"nw", pointToJson(effects.perspectiveNw)},
          {"ne", pointToJson(effects.perspectiveNe)},
          {"se", pointToJson(effects.perspectiveSe)},
          {"sw", pointToJson(effects.perspectiveSw)}};
}

QJsonArray pathPointsToJson(const std::vector<Point> &points) {
  QJsonArray result;
  for (const auto &point : points)
    result.append(pointToJson(point));
  if (!points.empty()) {
    while (result.size() < 3)
      result.append(
          pointToJson(result.size() == 1 ? Point{0.5, 0.5} : Point{1.0, 0.5}));
  }
  return result;
}

std::vector<Point> pathPointsFromJson(const QJsonValue &value) {
  std::vector<Point> result;
  for (const auto &item : value.toArray())
    result.push_back(pointFromJson(item));
  return result;
}

TextStyle styleFromJson(const QJsonObject &object) {
  TextStyle style;
  style.fontFamily = object.value("font_family")
                         .toString(QString::fromStdString(style.fontFamily))
                         .toStdString();
  style.fontSize = object.value("font_size").toInt(style.fontSize);
  style.textColor = object.value("text_color")
                        .toString(QString::fromStdString(style.textColor))
                        .toStdString();
  style.lineSpacing = object.value("line_spacing").toInt(style.lineSpacing);
  style.letterSpacing =
      object.value("letter_spacing").toInt(style.letterSpacing);
  style.bold = object.value("bold").toBool(style.bold);
  style.italic = object.value("italic").toBool(style.italic);
  style.uppercase = object.value("uppercase").toBool(style.uppercase);
  style.lowercase = object.value("lowercase").toBool(style.lowercase);
  if (style.uppercase)
    style.lowercase = false;
  style.alignment = static_cast<TextAlignment>(
      object.value("alignment").toInt(static_cast<int>(style.alignment)));
  return style;
}

QJsonObject styleToJson(const TextStyle &style) {
  return {{"font_family", QString::fromStdString(style.fontFamily)},
          {"font_size", style.fontSize},
          {"text_color", QString::fromStdString(style.textColor)},
          {"line_spacing", style.lineSpacing},
          {"letter_spacing", style.letterSpacing},
          {"bold", style.bold},
          {"italic", style.italic},
          {"uppercase", style.uppercase},
          {"lowercase", style.lowercase && !style.uppercase},
          {"alignment", static_cast<int>(style.alignment)}};
}

TextBox boxFromJson(const QJsonObject &object) {
  TextBox box;
  box.text = object.value("text").toString().toStdString();
  box.bounds.x = object.value("x").toDouble();
  box.bounds.y = object.value("y").toDouble();
  box.bounds.w = object.value("w").toDouble();
  box.bounds.h = object.value("h").toDouble();
  box.rotationDegrees = object.value("rotation_degrees").toDouble();
  box.style = styleFromJson(object);

  auto &effects = box.effects;
  effects.outlineEnabled =
      object.value("outline_enabled").toBool(effects.outlineEnabled);
  effects.outlineColor =
      object.value("outline_color")
          .toString(QString::fromStdString(effects.outlineColor))
          .toStdString();
  effects.outlineSize = object.value("outline_size").toInt(effects.outlineSize);
  effects.blurEnabled =
      object.value("blur_enabled").toBool(effects.blurEnabled);
  effects.blurSize = object.value("blur_size").toInt(effects.blurSize);
  effects.shadowEnabled =
      object.value("shadow_enabled").toBool(effects.shadowEnabled);
  effects.shadowColor =
      object.value("shadow_color")
          .toString(QString::fromStdString(effects.shadowColor))
          .toStdString();
  effects.shadowOffsetX =
      object.value("shadow_offset_x").toInt(effects.shadowOffsetX);
  effects.shadowOffsetY =
      object.value("shadow_offset_y").toInt(effects.shadowOffsetY);
  effects.shadowBlurSize =
      std::clamp(object.value("shadow_blur_size").toInt(effects.shadowBlurSize),
                 0, MaxShadowBlurSize);
  effects.gradientEnabled =
      object.value("gradient_enabled").toBool(effects.gradientEnabled);
  effects.gradientDirection = std::clamp(
      object.value("gradient_direction").toInt(effects.gradientDirection), 0,
      1);
  effects.gradientColorA =
      object.value("gradient_color_a")
          .toString(QString::fromStdString(effects.gradientColorA))
          .toStdString();
  effects.gradientColorB =
      object.value("gradient_color_b")
          .toString(QString::fromStdString(effects.gradientColorB))
          .toStdString();
  effects.pathEnabled =
      object.value("path_enabled").toBool(effects.pathEnabled);
  effects.pathMode =
      std::clamp(object.value("path_mode").toInt(effects.pathMode), 0, 1);
  effects.pathPoints = pathPointsFromJson(object.value("path_points"));
  while (effects.pathPoints.size() > 0 && effects.pathPoints.size() < 3)
    effects.pathPoints.push_back(
        {effects.pathPoints.size() == 1 ? 0.5 : 1.0, 0.5});
  effects.perspectiveEnabled =
      object.value("perspective_enabled").toBool(effects.perspectiveEnabled);
  const auto perspective = object.value("perspective_offsets").toObject();
  effects.perspectiveNw = pointFromJson(perspective.value("nw"));
  effects.perspectiveNe = pointFromJson(perspective.value("ne"));
  effects.perspectiveSe = pointFromJson(perspective.value("se"));
  effects.perspectiveSw = pointFromJson(perspective.value("sw"));
  return box;
}

QJsonObject boxToJson(const TextBox &box) {
  auto object = styleToJson(box.style);
  object.insert("text", QString::fromStdString(box.text));
  object.insert("x", box.bounds.x);
  object.insert("y", box.bounds.y);
  object.insert("w", box.bounds.w);
  object.insert("h", box.bounds.h);
  object.insert("rotation_degrees", box.rotationDegrees);
  object.insert("outline_enabled", box.effects.outlineEnabled);
  object.insert("outline_color",
                QString::fromStdString(box.effects.outlineColor));
  object.insert("outline_size", box.effects.outlineSize);
  object.insert("blur_enabled", box.effects.blurEnabled);
  object.insert("blur_size", box.effects.blurSize);
  object.insert("shadow_enabled", box.effects.shadowEnabled);
  object.insert("shadow_color",
                QString::fromStdString(box.effects.shadowColor));
  object.insert("shadow_offset_x", box.effects.shadowOffsetX);
  object.insert("shadow_offset_y", box.effects.shadowOffsetY);
  object.insert("shadow_blur_size",
                std::clamp(box.effects.shadowBlurSize, 0, MaxShadowBlurSize));
  object.insert("gradient_enabled", box.effects.gradientEnabled);
  object.insert("gradient_direction",
                std::clamp(box.effects.gradientDirection, 0, 1));
  object.insert("gradient_color_a",
                QString::fromStdString(box.effects.gradientColorA));
  object.insert("gradient_color_b",
                QString::fromStdString(box.effects.gradientColorB));
  object.insert("path_enabled", box.effects.pathEnabled);
  object.insert("path_mode", std::clamp(box.effects.pathMode, 0, 1));
  object.insert("path_points", pathPointsToJson(box.effects.pathPoints));
  object.insert("perspective_enabled", box.effects.perspectiveEnabled);
  object.insert("perspective_offsets", perspectiveToJson(box.effects));
  return object;
}
} // namespace

bool JsonSerializer::loadPage(const std::filesystem::path &path,
                              DocumentModel &document, std::string *error) {
  const auto json =
      QJsonDocument::fromJson(QByteArray::fromStdString(readFile(path)));
  if (!json.isObject()) {
    setError(error, "Save file is not valid JSON.");
    return false;
  }
  const auto root = json.object();
  if (root.value("format").toString() != PageFormat ||
      !root.value("boxes").isArray()) {
    setError(error, "Unsupported page save format.");
    return false;
  }
  document.clear();
  for (const auto &item : root.value("boxes").toArray()) {
    if (item.isObject())
      document.textBoxes().push_back(boxFromJson(item.toObject()));
  }
  document.markSaved();
  return true;
}

bool JsonSerializer::savePage(const std::filesystem::path &path,
                              const std::string &pageName,
                              const DocumentModel &document,
                              std::string *error) {
  QJsonArray boxes;
  for (const auto &box : document.textBoxes())
    boxes.append(boxToJson(box));
  const QJsonObject root{{"format", PageFormat},
                         {"page", QString::fromStdString(pageName)},
                         {"boxes", boxes}};
  return writeFile(path, QJsonDocument(root).toJson(QJsonDocument::Indented),
                   error);
}

bool JsonSerializer::loadPresets(const std::filesystem::path &path,
                                 std::vector<TextPreset> &presets,
                                 std::string *error) {
  const auto json =
      QJsonDocument::fromJson(QByteArray::fromStdString(readFile(path)));
  if (!json.isObject() ||
      json.object().value("format").toString() != PresetsFormat ||
      !json.object().value("presets").isArray()) {
    setError(error, "Unsupported presets format.");
    return false;
  }
  presets.clear();
  for (const auto &item : json.object().value("presets").toArray()) {
    const auto object = item.toObject();
    const auto properties = object.value("properties").toObject();
    const auto name = object.value("name").toString().trimmed();
    if (!name.isEmpty() && !properties.isEmpty())
      presets.push_back({name.toStdString(), styleFromJson(properties)});
  }
  return true;
}

bool JsonSerializer::savePresets(const std::filesystem::path &path,
                                 const std::vector<TextPreset> &presets,
                                 std::string *error) {
  QJsonArray items;
  for (const auto &preset : presets) {
    if (!preset.name.empty())
      items.append(QJsonObject{{"name", QString::fromStdString(preset.name)},
                               {"properties", styleToJson(preset.style)}});
  }
  return writeFile(
      path,
      QJsonDocument(QJsonObject{{"format", PresetsFormat}, {"presets", items}})
          .toJson(QJsonDocument::Indented),
      error);
}

} // namespace textfx
