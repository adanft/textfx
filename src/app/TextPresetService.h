#pragma once

#include "core/DocumentModel.h"

#include <QString>

#include <string>
#include <vector>

namespace textfx {

enum class TextPresetStatus {
  Succeeded,
  InvalidPresetIndex,
  InvalidName,
  DuplicateName,
  MissingProjectPreset,
};

struct TextPresetResult {
  TextPresetStatus status = TextPresetStatus::InvalidPresetIndex;
  std::string preferredName;
};

struct SelectedTextPresetContext {
  const std::vector<TextPreset> &availablePresets;
  int selectedPresetIndex;
};

struct ProjectTextPresetContext {
  std::vector<TextPreset> &projectPresets;
  const std::vector<TextPreset> &availablePresets;
  int selectedPresetIndex;
};

struct TextPresetSourceContext {
  std::vector<TextPreset> &projectPresets;
  const TextBox &sourceBox;
};

class TextPresetService {
public:
  static TextPresetResult
  applySelectedPresetResult(TextBox &box, const SelectedTextPresetContext &ctx);
  static TextPresetResult addPresetResult(const TextPresetSourceContext &ctx,
                                          const QString &name);
  static TextPresetResult updateSelectedPresetResult(
      const ProjectTextPresetContext &ctx, const TextBox &sourceBox);
  static TextPresetResult renameSelectedPresetResult(
      const ProjectTextPresetContext &ctx, const QString &name);
  static TextPresetResult
  deleteSelectedPresetResult(const ProjectTextPresetContext &ctx);

  static bool applySelectedPreset(TextBox &box,
                                  const SelectedTextPresetContext &ctx);
  static bool addPreset(const TextPresetSourceContext &ctx, const QString &name,
                        std::string &preferredName);
  static bool updateSelectedPreset(const ProjectTextPresetContext &ctx,
                                   const TextBox &sourceBox,
                                   std::string &preferredName);
  static bool renameSelectedPreset(const ProjectTextPresetContext &ctx,
                                   const QString &name,
                                   std::string &preferredName);
  static bool deleteSelectedPreset(const ProjectTextPresetContext &ctx);
};

} // namespace textfx
