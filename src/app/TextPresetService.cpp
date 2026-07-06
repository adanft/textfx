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

bool succeeded(const TextPresetResult &result) {
  return result.status == TextPresetStatus::Succeeded;
}
} // namespace

TextPresetResult TextPresetService::applySelectedPresetResult(
    TextBox &box, const std::vector<TextPreset> &presets,
    int selectedPresetIndex) {
  if (!isValidPresetIndex(presets, selectedPresetIndex))
    return {TextPresetStatus::InvalidPresetIndex};
  box.style = presets.at(static_cast<std::size_t>(selectedPresetIndex)).style;
  return {TextPresetStatus::Succeeded};
}

TextPresetResult
TextPresetService::addPresetResult(std::vector<TextPreset> &projectPresets,
                                   const TextBox &sourceBox,
                                   const QString &name) {
  const auto cleanName = cleanPresetName(name);
  if (cleanName.empty())
    return {TextPresetStatus::InvalidName};
  upsertPreset(projectPresets, TextPreset{cleanName, sourceBox.style});
  return {TextPresetStatus::Succeeded, cleanName};
}

TextPresetResult TextPresetService::updateSelectedPresetResult(
    std::vector<TextPreset> &projectPresets,
    const std::vector<TextPreset> &presets, int selectedPresetIndex,
    const TextBox &sourceBox) {
  if (!isValidPresetIndex(presets, selectedPresetIndex))
    return {TextPresetStatus::InvalidPresetIndex};

  const auto name = presets.at(static_cast<std::size_t>(selectedPresetIndex)).name;
  upsertPreset(projectPresets, TextPreset{name, sourceBox.style});
  return {TextPresetStatus::Succeeded, name};
}

TextPresetResult TextPresetService::renameSelectedPresetResult(
    std::vector<TextPreset> &projectPresets,
    const std::vector<TextPreset> &presets, int selectedPresetIndex,
    const QString &name) {
  if (!isValidPresetIndex(presets, selectedPresetIndex))
    return {TextPresetStatus::InvalidPresetIndex};

  const auto oldName =
      presets.at(static_cast<std::size_t>(selectedPresetIndex)).name;
  const auto newName = cleanPresetName(name);
  if (newName.empty())
    return {TextPresetStatus::InvalidName};
  if (std::ranges::any_of(presets, [&](const TextPreset &item) {
        return item.name == newName;
      }))
    return {TextPresetStatus::DuplicateName};

  auto found = findPresetByName(projectPresets, oldName);
  if (found == projectPresets.end())
    return {TextPresetStatus::MissingProjectPreset};

  found->name = newName;
  return {TextPresetStatus::Succeeded, newName};
}

TextPresetResult TextPresetService::deleteSelectedPresetResult(
    std::vector<TextPreset> &projectPresets,
    const std::vector<TextPreset> &presets, int selectedPresetIndex) {
  if (!isValidPresetIndex(presets, selectedPresetIndex))
    return {TextPresetStatus::InvalidPresetIndex};

  const auto name = presets.at(static_cast<std::size_t>(selectedPresetIndex)).name;

  const auto oldSize = projectPresets.size();
  std::erase_if(projectPresets,
                [&](const TextPreset &item) { return item.name == name; });
  if (projectPresets.size() == oldSize)
    return {TextPresetStatus::MissingProjectPreset};
  return {TextPresetStatus::Succeeded};
}

bool TextPresetService::applySelectedPreset(
    TextBox &box, const std::vector<TextPreset> &presets,
    int selectedPresetIndex) {
  return succeeded(
      applySelectedPresetResult(box, presets, selectedPresetIndex));
}

bool TextPresetService::addPreset(std::vector<TextPreset> &projectPresets,
                                  const TextBox &sourceBox, const QString &name,
                                  std::string &preferredName) {
  const auto result = addPresetResult(projectPresets, sourceBox, name);
  if (succeeded(result))
    preferredName = result.preferredName;
  return succeeded(result);
}

bool TextPresetService::updateSelectedPreset(
    std::vector<TextPreset> &projectPresets,
    const std::vector<TextPreset> &presets, int selectedPresetIndex,
    const TextBox &sourceBox, std::string &preferredName) {
  const auto result = updateSelectedPresetResult(
      projectPresets, presets, selectedPresetIndex, sourceBox);
  if (succeeded(result))
    preferredName = result.preferredName;
  return succeeded(result);
}

bool TextPresetService::renameSelectedPreset(
    std::vector<TextPreset> &projectPresets,
    const std::vector<TextPreset> &presets, int selectedPresetIndex,
    const QString &name, std::string &preferredName) {
  const auto result = renameSelectedPresetResult(projectPresets, presets,
                                                 selectedPresetIndex, name);
  if (succeeded(result))
    preferredName = result.preferredName;
  return succeeded(result);
}

bool TextPresetService::deleteSelectedPreset(
    std::vector<TextPreset> &projectPresets,
    const std::vector<TextPreset> &presets, int selectedPresetIndex) {
  return succeeded(deleteSelectedPresetResult(projectPresets, presets,
                                              selectedPresetIndex));
}

} // namespace textfx
