#pragma once

#include "core/DocumentModel.h"

#include <filesystem>
#include <string>
#include <vector>

namespace textfx {

class RenderGraph {
public:
    bool exportPagePng(const DocumentModel& document, const std::filesystem::path& pageImagePath, const std::filesystem::path& exportPath, std::string* error = nullptr) const;
    std::vector<std::string> warnings(const DocumentModel& document) const;
};

} // namespace textfx
