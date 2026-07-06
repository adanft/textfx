#include "application/services/TextWorkflowService.h"

namespace textfx {
namespace {
PresetWorkflowResult toWorkflowResult(const TextPresetResult &result) {
  return {.precondition = WorkflowPrecondition::None,
          .serviceStatus = result.status,
          .preferredName = result.preferredName};
}

PresetWorkflowResult missingSelectedBox() {
  return {.precondition = WorkflowPrecondition::MissingSelectedBox};
}
} // namespace

PageTextWorkflowResult TextWorkflowService::applyPageText(
    PageTextWorkflowContext &ctx, int textIndex) {
  return {PageTextService::applyText(ctx.boxes, ctx.selectedIndex, ctx.pageTexts,
                                     ctx.positions, ctx.pageKey, textIndex)};
}

int TextWorkflowService::nextPageTextIndex(PageTextWorkflowContext &ctx) {
  return PageTextService::nextTextIndex(ctx.positions, ctx.pageKey);
}

PresetWorkflowResult
TextWorkflowService::applySelectedPreset(PresetWorkflowContext &ctx) {
  if (!ctx.selectedBox)
    return missingSelectedBox();
  return toWorkflowResult(TextPresetService::applySelectedPresetResult(
      *ctx.selectedBox, {.availablePresets = ctx.presets,
                         .selectedPresetIndex = ctx.selectedPresetIndex}));
}

PresetWorkflowResult TextWorkflowService::addPreset(PresetWorkflowContext &ctx,
                                                    const QString &name) {
  if (!ctx.selectedBox)
    return missingSelectedBox();
  return toWorkflowResult(TextPresetService::addPresetResult(
      {.projectPresets = ctx.projectPresets, .sourceBox = *ctx.selectedBox},
      name));
}

PresetWorkflowResult
TextWorkflowService::updateSelectedPreset(PresetWorkflowContext &ctx) {
  if (!ctx.selectedBox)
    return missingSelectedBox();
  return toWorkflowResult(TextPresetService::updateSelectedPresetResult(
      {.projectPresets = ctx.projectPresets,
       .availablePresets = ctx.presets,
       .selectedPresetIndex = ctx.selectedPresetIndex},
      *ctx.selectedBox));
}

PresetWorkflowResult
TextWorkflowService::renameSelectedPreset(PresetWorkflowContext &ctx,
                                          const QString &name) {
  return toWorkflowResult(TextPresetService::renameSelectedPresetResult(
      {.projectPresets = ctx.projectPresets,
       .availablePresets = ctx.presets,
       .selectedPresetIndex = ctx.selectedPresetIndex},
      name));
}

PresetWorkflowResult
TextWorkflowService::deleteSelectedPreset(PresetWorkflowContext &ctx) {
  return toWorkflowResult(TextPresetService::deleteSelectedPresetResult(
      {.projectPresets = ctx.projectPresets,
       .availablePresets = ctx.presets,
       .selectedPresetIndex = ctx.selectedPresetIndex}));
}

} // namespace textfx
