#include "io/JsonSerializer.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

using namespace textfx;

namespace {
std::filesystem::path fixtures()
{
    return std::filesystem::path(TEXTFX_FIXTURE_DIR);
}

std::string readText(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}
} // namespace

int main()
{
    const auto pageFixture = fixtures() / "typex-compatible-page.json";
    const auto presetFixture = fixtures() / "typex-compatible-presets.json";

    DocumentModel document;
    std::string error;
    if (!JsonSerializer::loadPage(pageFixture, document, &error)) {
        std::cerr << "Page fixture failed to load: " << error << '\n';
        return 1;
    }
    if (document.textBoxes().size() != 2 || document.textBoxes().front().text != "Hello TextFX") {
        std::cerr << "Page fixture did not preserve expected text boxes\n";
        return 1;
    }

    std::vector<TextPreset> presets;
    if (!JsonSerializer::loadPresets(presetFixture, presets, &error) || presets.empty()) {
        std::cerr << "Preset fixture failed to load: " << error << '\n';
        return 1;
    }

    const auto output = std::filesystem::temp_directory_path() / "textfx-serialization-check.json";
    if (!JsonSerializer::savePage(output, "fixture.png", document, &error)) {
        std::cerr << "Page fixture failed to save: " << error << '\n';
        return 1;
    }

    const auto saved = readText(output);
    std::filesystem::remove(output);
    if (saved.find("typebubblex_only") != std::string::npos || saved.find("mask_brush") != std::string::npos) {
        std::cerr << "Unsupported fields leaked into saved page JSON\n";
        return 1;
    }

    return 0;
}
