#include "app/EditorViewModels.h"

#include "app/BoxRenderState.h"
namespace textfx::EditorViewModels {
namespace {
QString toQString(const std::string &value) { return QString::fromStdString(value); }
} // namespace

QVariantList pointList(const std::vector<Point> &points) {
  return textfx::pointList(points);
}

QVariantMap textBoxMap(const TextBox &box, int index) {
  const auto state = mapBoxRenderState(box, index);
  return {
      {"index", state.index},
      {"text", state.text},
      {"x", state.x},
      {"y", state.y},
      {"w", state.width},
      {"h", state.height},
      {"rotation", state.rotation},
      {"fontFamily", state.fontFamily},
      {"resolvedFontFamily", state.resolvedFontFamily},
      {"fontSize", state.fontSize},
      {"color", state.color},
      {"lineSpacing", state.lineSpacing},
      {"letterSpacing", state.letterSpacing},
      {"bold", state.bold},
      {"italic", state.italic},
      {"uppercase", state.uppercase},
      {"lowercase", state.lowercase},
      {"alignment", state.alignment},
      {"outline", state.outline},
      {"outlineColor", state.outlineColor},
      {"outlineSize", state.outlineSize},
      {"blur", state.blur},
      {"blurSize", state.blurSize},
      {"shadow", state.shadow},
      {"shadowColor", state.shadowColor},
      {"shadowOffsetX", state.shadowOffsetX},
      {"shadowOffsetY", state.shadowOffsetY},
      {"shadowBlurSize", state.shadowBlurSize},
      {"gradient", state.gradient},
      {"gradientDirection", state.gradientDirection},
      {"gradientColorA", state.gradientColorA},
      {"gradientColorB", state.gradientColorB},
      {"perspective", state.perspective},
      {"perspectiveNw", state.perspectiveNw},
      {"perspectiveNe", state.perspectiveNe},
      {"perspectiveSe", state.perspectiveSe},
      {"perspectiveSw", state.perspectiveSw},
      {"path", state.path},
      {"pathMode", state.pathMode},
      {"pathPoints", state.pathPoints},
      {"effects", state.effects},
  };
}

QVariantList textBoxList(const std::vector<TextBox> &boxes) {
  QVariantList list;
  int index = 0;
  for (const auto &box : boxes)
    list.push_back(textBoxMap(box, index++));
  return list;
}

QVariantList layerList(const std::vector<TextBox> &boxes) {
  QVariantList list;
  for (int index = static_cast<int>(boxes.size()) - 1; index >= 0; --index) {
    list.push_back(textBoxMap(boxes[static_cast<std::size_t>(index)], index));
  }
  return list;
}

QVariantMap presetMap(const TextPreset &preset) {
  return {{"name", toQString(preset.name)},
          {"fontFamily", toQString(preset.style.fontFamily)},
          {"fontSize", preset.style.fontSize}};
}

QVariantList presetList(const std::vector<TextPreset> &presets) {
  QVariantList list;
  for (const auto &preset : presets)
    list.push_back(presetMap(preset));
  return list;
}

} // namespace textfx::EditorViewModels
