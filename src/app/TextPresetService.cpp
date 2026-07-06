#include "app/TextPresetService.h"

#include <algorithm>
#include <utility>

namespace textfx {
namespace {
std::string cleanPresetName(const QString &name) {
  return name.trimmed().toStdString();
}

bool isValidPresetIndex(const SelectedTextPresetContext &ctx) {
  return ctx.selectedPresetIndex >= 0 &&
         ctx.selectedPresetIndex < static_cast<int>(ctx.availablePresets.size());
}

bool isValidPresetIndex(const ProjectTextPresetContext &ctx) {
  return isValidPresetIndex(SelectedTextPresetContext{ctx.availablePresets,
                                                      ctx.selectedPresetIndex});
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
    TextBox &box, const SelectedTextPresetContext &ctx) {
  if (!isValidPresetIndex(ctx))
    return {TextPresetStatus::InvalidPresetIndex};
  box.style = ctx.availablePresets.at(static_cast<std::size_t>(ctx.selectedPresetIndex)).style;
  return {TextPresetStatus::Succeeded};
}

TextPresetResult
TextPresetService::addPresetResult(const TextPresetSourceContext &ctx,
                                   const QString &name) {
  const auto cleanName = cleanPresetName(name);
  if (cleanName.empty())
    return {TextPresetStatus::InvalidName};
  upsertPreset(ctx.projectPresets, TextPreset{cleanName, ctx.sourceBox.style});
  return {TextPresetStatus::Succeeded, cleanName};
}

TextPresetResult TextPresetService::updateSelectedPresetResult(
    const ProjectTextPresetContext &ctx, const TextBox &sourceBox) {
  if (!isValidPresetIndex(ctx))
    return {TextPresetStatus::InvalidPresetIndex};

  const auto name = ctx.availablePresets.at(static_cast<std::size_t>(ctx.selectedPresetIndex)).name;
  upsertPreset(ctx.projectPresets, TextPreset{name, sourceBox.style});
  return {TextPresetStatus::Succeeded, name};
}

TextPresetResult TextPresetService::renameSelectedPresetResult(
    const ProjectTextPresetContext &ctx, const QString &name) {
  if (!isValidPresetIndex(ctx))
    return {TextPresetStatus::InvalidPresetIndex};

  const auto oldName =
      ctx.availablePresets.at(static_cast<std::size_t>(ctx.selectedPresetIndex)).name;
  const auto newName = cleanPresetName(name);
  if (newName.empty())
    return {TextPresetStatus::InvalidName};
  if (std::ranges::any_of(ctx.availablePresets, [&](const TextPreset &item) {
        return item.name == newName;
      }))
    return {TextPresetStatus::DuplicateName};

  auto found = findPresetByName(ctx.projectPresets, oldName);
  if (found == ctx.projectPresets.end())
    return {TextPresetStatus::MissingProjectPreset};

  found->name = newName;
  return {TextPresetStatus::Succeeded, newName};
}

TextPresetResult TextPresetService::deleteSelectedPresetResult(
    const ProjectTextPresetContext &ctx) {
  if (!isValidPresetIndex(ctx))
    return {TextPresetStatus::InvalidPresetIndex};

  const auto name = ctx.availablePresets.at(static_cast<std::size_t>(ctx.selectedPresetIndex)).name;

  const auto oldSize = ctx.projectPresets.size();
  std::erase_if(ctx.projectPresets,
                [&](const TextPreset &item) { return item.name == name; });
  if (ctx.projectPresets.size() == oldSize)
    return {TextPresetStatus::MissingProjectPreset};
  return {TextPresetStatus::Succeeded};
}

bool TextPresetService::applySelectedPreset(
    TextBox &box, const SelectedTextPresetContext &ctx) {
  return succeeded(applySelectedPresetResult(box, ctx));
}

bool TextPresetService::addPreset(const TextPresetSourceContext &ctx,
                                  const QString &name,
                                  std::string &preferredName) {
  const auto result = addPresetResult(ctx, name);
  if (succeeded(result))
    preferredName = result.preferredName;
  return succeeded(result);
}

bool TextPresetService::updateSelectedPreset(
    const ProjectTextPresetContext &ctx, const TextBox &sourceBox,
    std::string &preferredName) {
  const auto result = updateSelectedPresetResult(ctx, sourceBox);
  if (succeeded(result))
    preferredName = result.preferredName;
  return succeeded(result);
}

bool TextPresetService::renameSelectedPreset(const ProjectTextPresetContext &ctx,
                                             const QString &name,
                                             std::string &preferredName) {
  const auto result = renameSelectedPresetResult(ctx, name);
  if (succeeded(result))
    preferredName = result.preferredName;
  return succeeded(result);
}

bool TextPresetService::deleteSelectedPreset(
    const ProjectTextPresetContext &ctx) {
  return succeeded(deleteSelectedPresetResult(ctx));
}

} // namespace textfx
