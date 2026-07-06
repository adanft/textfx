#pragma once

#include "app/PageTextService.h"
#include "app/TextPresetService.h"

#include <QString>

#include <string>
#include <vector>

namespace textfx {

struct PageTextWorkflowContext {
  std::vector<TextBox> &boxes;
  int selectedIndex;
  const PageTextMap &pageTexts;
  PageTextPositionMap &positions;
  std::string pageKey;
};

struct PresetWorkflowContext {
  TextBox *selectedBox;
  std::vector<TextPreset> &projectPresets;
  const std::vector<TextPreset> &presets;
  int selectedPresetIndex;
};

enum class WorkflowPrecondition {
  None,
  MissingSelectedBox,
};

struct PageTextWorkflowResult {
  PageTextApplyStatus status = PageTextApplyStatus::MissingText;
};

struct PresetWorkflowResult {
  WorkflowPrecondition precondition = WorkflowPrecondition::None;
  TextPresetStatus serviceStatus = TextPresetStatus::InvalidPresetIndex;
  std::string preferredName;

  [[nodiscard]] bool succeeded() const {
    return precondition == WorkflowPrecondition::None &&
           serviceStatus == TextPresetStatus::Succeeded;
  }
};

class TextWorkflowService {
public:
  static PageTextWorkflowResult applyPageText(PageTextWorkflowContext &ctx,
                                              int textIndex);
  static int nextPageTextIndex(PageTextWorkflowContext &ctx);

  static PresetWorkflowResult applySelectedPreset(PresetWorkflowContext &ctx);
  static PresetWorkflowResult addPreset(PresetWorkflowContext &ctx,
                                        const QString &name);
  static PresetWorkflowResult updateSelectedPreset(PresetWorkflowContext &ctx);
  static PresetWorkflowResult renameSelectedPreset(PresetWorkflowContext &ctx,
                                                   const QString &name);
  static PresetWorkflowResult deleteSelectedPreset(PresetWorkflowContext &ctx);
};

} // namespace textfx
