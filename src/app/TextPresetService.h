#pragma once

#include "core/DocumentModel.h"

#include <QString>

#include <string>
#include <vector>

namespace textfx {

class TextPresetService {
public:
    static bool applySelectedPreset(TextBox& box, const std::vector<TextPreset>& presets, int selectedPresetIndex);
    static bool addPreset(std::vector<TextPreset>& projectPresets, const TextBox& sourceBox, const QString& name, std::string& preferredName);
    static bool updateSelectedPreset(std::vector<TextPreset>& projectPresets,
                                     const std::vector<TextPreset>& presets,
                                     int selectedPresetIndex,
                                     const TextBox& sourceBox,
                                     std::string& preferredName);
    static bool renameSelectedPreset(std::vector<TextPreset>& projectPresets,
                                     const std::vector<TextPreset>& presets,
                                     int selectedPresetIndex,
                                     const QString& name,
                                     std::string& preferredName);
    static bool deleteSelectedPreset(std::vector<TextPreset>& projectPresets, const std::vector<TextPreset>& presets, int selectedPresetIndex);
};

} // namespace textfx
