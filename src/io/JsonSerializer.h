#pragma once

#include "core/DocumentModel.h"

#include <filesystem>
#include <string>

namespace textfx {

class JsonSerializer {
public:
    static constexpr auto PageFormat = "textfx.page-boxes.v1";
    static constexpr auto PresetsFormat = "textfx.text-presets.v1";

    static bool loadPage(const std::filesystem::path& path, DocumentModel& document, std::string* error = nullptr);
    static bool savePage(const std::filesystem::path& path, const std::string& pageName, const DocumentModel& document, std::string* error = nullptr);
    static bool loadPresets(const std::filesystem::path& path, std::vector<TextPreset>& presets, std::string* error = nullptr);
    static bool savePresets(const std::filesystem::path& path, const std::vector<TextPreset>& presets, std::string* error = nullptr);
};

} // namespace textfx
