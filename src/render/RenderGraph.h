#pragma once

#include "core/DocumentModel.h"

#include <chrono>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace textfx {

class RenderGraph {
public:
    struct ExportTiming {
        using Duration = std::chrono::steady_clock::duration;

        Duration elapsed{};
    };

    struct ExportResult {
        ExportTiming timing{};
    };

    std::expected<ExportResult, std::string> exportPagePngTimed(const DocumentModel& document, const std::filesystem::path& pageImagePath, const std::filesystem::path& exportPath) const;
    std::expected<void, std::string> exportPagePngResult(const DocumentModel& document, const std::filesystem::path& pageImagePath, const std::filesystem::path& exportPath) const;
    bool exportPagePng(const DocumentModel& document, const std::filesystem::path& pageImagePath, const std::filesystem::path& exportPath, std::string* error = nullptr) const;
    std::vector<std::string> warnings(const DocumentModel& document) const;
};

} // namespace textfx
