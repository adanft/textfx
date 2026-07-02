#include "app/TextPresetService.h"

#include <algorithm>
#include <utility>

namespace textfx {
namespace {
std::string cleanPresetName(const QString &name) {
  return name.trimmed().toStdString();
}

bool isValidPresetIndex(const std::vector<TextPreset> &presets, int index) {
  return index >= 0 && index < static_cast<int>(presets.size());
}

auto findPresetByName(std::vector<TextPreset> &presets,
                      const std::string &name) {
  return std::ranges::find_if(
      presets, [&](const TextPreset &item) { return item.name == name; });
}

void upsertPreset(std::vector<TextPreset> &projectPresets, TextPreset preset) {
  auto found = findPresetByName(projectPresets, preset.name);
  if (found == projectPresets.end()) {
    projectPresets.push_back(std::move(preset));
  } else {
    *found = std::move(preset);
  }
}
} // namespace

bool TextPresetService::applySelectedPreset(
    TextBox &box, const std::vector<TextPreset> &presets,
    int selectedPresetIndex) {
  if (!isValidPresetIndex(presets, selectedPresetIndex))
    return false;
  box.style = presets.at(static_cast<std::size_t>(selectedPresetIndex)).style;
  return true;
}

bool TextPresetService::addPreset(std::vector<TextPreset> &projectPresets,
                                  const TextBox &sourceBox, const QString &name,
                                  std::string &preferredName) {
  const auto cleanName = cleanPresetName(name);
  if (cleanName.empty())
    return false;

  upsertPreset(projectPresets, TextPreset{cleanName, sourceBox.style});
  preferredName = cleanName;
  return true;
}

bool TextPresetService::updateSelectedPreset(
    std::vector<TextPreset> &projectPresets,
    const std::vector<TextPreset> &presets, int selectedPresetIndex,
    const TextBox &sourceBox, std::string &preferredName) {
  if (!isValidPresetIndex(presets, selectedPresetIndex))
    return false;

  const auto name =
      presets.at(static_cast<std::size_t>(selectedPresetIndex)).name;
  upsertPreset(projectPresets, TextPreset{name, sourceBox.style});
  preferredName = name;
  return true;
}

bool TextPresetService::renameSelectedPreset(
    std::vector<TextPreset> &projectPresets,
    const std::vector<TextPreset> &presets, int selectedPresetIndex,
    const QString &name, std::string &preferredName) {
  if (!isValidPresetIndex(presets, selectedPresetIndex))
    return false;

  const auto oldName =
      presets.at(static_cast<std::size_t>(selectedPresetIndex)).name;
  const auto newName = cleanPresetName(name);
  if (newName.empty())
    return false;
  if (std::ranges::any_of(presets, [&](const TextPreset &item) {
        return item.name == newName;
      }))
    return false;

  auto found = findPresetByName(projectPresets, oldName);
  if (found == projectPresets.end())
    return false;

  found->name = newName;
  preferredName = newName;
  return true;
}

bool TextPresetService::deleteSelectedPreset(
    std::vector<TextPreset> &projectPresets,
    const std::vector<TextPreset> &presets, int selectedPresetIndex) {
  if (!isValidPresetIndex(presets, selectedPresetIndex))
    return false;

  const auto name =
      presets.at(static_cast<std::size_t>(selectedPresetIndex)).name;

  const auto oldSize = projectPresets.size();
  std::erase_if(projectPresets,
                [&](const TextPreset &item) { return item.name == name; });
  return projectPresets.size() != oldSize;
}

} // namespace textfx
