#include "app/TextWorkflowService.h"

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
      *ctx.selectedBox, ctx.presets, ctx.selectedPresetIndex));
}

PresetWorkflowResult TextWorkflowService::addPreset(PresetWorkflowContext &ctx,
                                                    const QString &name) {
  if (!ctx.selectedBox)
    return missingSelectedBox();
  return toWorkflowResult(TextPresetService::addPresetResult(
      ctx.projectPresets, *ctx.selectedBox, name));
}

PresetWorkflowResult
TextWorkflowService::updateSelectedPreset(PresetWorkflowContext &ctx) {
  if (!ctx.selectedBox)
    return missingSelectedBox();
  return toWorkflowResult(TextPresetService::updateSelectedPresetResult(
      ctx.projectPresets, ctx.presets, ctx.selectedPresetIndex, *ctx.selectedBox));
}

PresetWorkflowResult
TextWorkflowService::renameSelectedPreset(PresetWorkflowContext &ctx,
                                          const QString &name) {
  return toWorkflowResult(TextPresetService::renameSelectedPresetResult(
      ctx.projectPresets, ctx.presets, ctx.selectedPresetIndex, name));
}

PresetWorkflowResult
TextWorkflowService::deleteSelectedPreset(PresetWorkflowContext &ctx) {
  return toWorkflowResult(TextPresetService::deleteSelectedPresetResult(
      ctx.projectPresets, ctx.presets, ctx.selectedPresetIndex));
}

} // namespace textfx
