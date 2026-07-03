#include "io/JsonSerializer.h"
#include "app/TextBoxEditingService.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>

#include <QTemporaryDir>

using namespace textfx;

namespace {
std::filesystem::path fixtures() {
  return std::filesystem::path(TEXTFX_FIXTURE_DIR);
}

std::string readText(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::ostringstream output;
  output << input.rdbuf();
  return output.str();
}
} // namespace

int main() {
  const auto pageFixture = fixtures() / "textfx-page.json";
  const auto presetFixture = fixtures() / "textfx-presets.json";

  DocumentModel document;
  std::string error;
  if (!JsonSerializer::loadPage(pageFixture, document, &error)) {
    std::cerr << "Page fixture failed to load: " << error << '\n';
    return 1;
  }
  if (document.textBoxes().size() != 2 ||
      document.textBoxes().front().text != "Hello TextFX") {
    std::cerr << "Page fixture did not preserve expected text boxes\n";
    return 1;
  }
  std::vector<TextPreset> presets;
  if (!JsonSerializer::loadPresets(presetFixture, presets, &error) ||
      presets.empty()) {
    std::cerr << "Preset fixture failed to load: " << error << '\n';
    return 1;
  }

  QTemporaryDir tempDir;
  if (!tempDir.isValid()) {
    std::cerr
        << "Failed to create temporary directory for serialization check\n";
    return 1;
  }
  const auto output = std::filesystem::path(tempDir.path().toStdString()) /
                      "serialization-check.json";
  if (!JsonSerializer::savePage(output, "fixture.png", document, &error)) {
    std::cerr << "Page fixture failed to save: " << error << '\n';
    return 1;
  }

  const auto saved = readText(output);
  if (saved.find("typebubblex_only") != std::string::npos ||
      saved.find("mask_brush") != std::string::npos) {
    std::cerr << "Unsupported fields leaked into saved page JSON\n";
    return 1;
  }
  if (saved.find("path_inactive_points") != std::string::npos) {
    std::cerr
        << "Obsolete TypeX path inactive points leaked into saved page JSON\n";
    return 1;
  }

  DocumentModel layeredDocument;
  TextBox layeredBox;
  layeredBox.text = "Layered outline";
  layeredBox.effects.outlineLayers = {{true, "ff0000ff", 10},
                                      {false, "00ff00ff", 4}};
  layeredBox.effects.outlineEnabled = true;
  layeredBox.effects.outlineColor = "ff0000ff";
  layeredBox.effects.outlineSize = 10;
  layeredBox.effects.outlineLayersSet = true;
  layeredDocument.textBoxes().push_back(layeredBox);
  const auto layeredOutput = std::filesystem::path(tempDir.path().toStdString()) /
                             "layered-outline.json";
  if (!JsonSerializer::savePage(layeredOutput, "layered.png", layeredDocument,
                                &error)) {
    std::cerr << "Layered outline page failed to save: " << error << '\n';
    return 1;
  }
  DocumentModel layeredRoundTrip;
  if (!JsonSerializer::loadPage(layeredOutput, layeredRoundTrip, &error) ||
      layeredRoundTrip.textBoxes().front().effects.outlineLayers.size() != 2 ||
      layeredRoundTrip.textBoxes().front().effects.outlineLayers[0].size != 10 ||
      layeredRoundTrip.textBoxes().front().effects.outlineLayers[1].enabled) {
    std::cerr << "Layered outline JSON did not round-trip\n";
    return 1;
  }

  const auto legacyOutlinePath = std::filesystem::path(tempDir.path().toStdString()) /
                                 "legacy-outline.json";
  {
    std::ofstream legacy(legacyOutlinePath, std::ios::binary);
    legacy << R"({"format":"textfx.page-boxes.v1","page":"legacy.png","boxes":[{"text":"Legacy","x":0,"y":0,"w":100,"h":40,"outline_enabled":true,"outline_color":"112233ff","outline_size":7}]})";
  }
  DocumentModel legacyDocument;
  if (!JsonSerializer::loadPage(legacyOutlinePath, legacyDocument, &error) ||
      legacyDocument.textBoxes().front().effects.outlineLayersSet ||
      legacyDocument.textBoxes().front().effects.outlineLayers.size() != 0 ||
      !legacyDocument.textBoxes().front().effects.outlineEnabled ||
      legacyDocument.textBoxes().front().effects.outlineColor != "112233ff" ||
      legacyDocument.textBoxes().front().effects.outlineSize != 7) {
    std::cerr << "Legacy outline fields did not remain a legacy fallback\n";
    return 1;
  }

  DocumentModel legacyMemoryDocument;
  TextBox legacyMemoryBox;
  legacyMemoryBox.text = "Legacy memory outline";
  legacyMemoryBox.effects.outlineEnabled = true;
  legacyMemoryBox.effects.outlineColor = "445566ff";
  legacyMemoryBox.effects.outlineSize = 8;
  legacyMemoryDocument.textBoxes().push_back(legacyMemoryBox);
  const auto legacyMemoryOutput =
      std::filesystem::path(tempDir.path().toStdString()) /
      "legacy-memory-outline.json";
  if (!JsonSerializer::savePage(legacyMemoryOutput, "legacy-memory.png",
                                legacyMemoryDocument, &error)) {
    std::cerr << "Legacy in-memory outline page failed to save: " << error
              << '\n';
    return 1;
  }
  DocumentModel legacyMemoryRoundTrip;
  if (!JsonSerializer::loadPage(legacyMemoryOutput, legacyMemoryRoundTrip,
                                &error) ||
      legacyMemoryRoundTrip.textBoxes().front().effects.outlineLayersSet ||
      legacyMemoryRoundTrip.textBoxes().front().effects.outlineLayers.size() !=
          0 ||
      !legacyMemoryRoundTrip.textBoxes().front().effects.outlineEnabled ||
      legacyMemoryRoundTrip.textBoxes().front().effects.outlineColor !=
          "445566ff" ||
      legacyMemoryRoundTrip.textBoxes().front().effects.outlineSize != 8) {
    std::cerr << "Legacy in-memory outline was lost after save/load\n";
    return 1;
  }

  const auto emptyOutlineOutput =
      std::filesystem::path(tempDir.path().toStdString()) / "empty-outline.json";
  {
    std::ofstream emptyOutline(emptyOutlineOutput, std::ios::binary);
    emptyOutline << R"({"format":"textfx.page-boxes.v1","page":"empty.png","boxes":[{"text":"No outline layers","x":0,"y":0,"w":100,"h":40,"outline_enabled":true,"outline_color":"112233ff","outline_size":7,"outline_layers":[]}]})";
  }
  DocumentModel emptyOutlineRoundTrip;
  if (!JsonSerializer::loadPage(emptyOutlineOutput, emptyOutlineRoundTrip,
                                &error) ||
      !emptyOutlineRoundTrip.textBoxes().front().effects.outlineLayersSet ||
      emptyOutlineRoundTrip.textBoxes().front().effects.outlineLayers.size() !=
          0 ||
      emptyOutlineRoundTrip.textBoxes().front().effects.outlineEnabled) {
    std::cerr << "Explicit empty outline layers still enabled legacy outline\n";
    return 1;
  }
  const auto emptyOutlineSavedOutput =
      std::filesystem::path(tempDir.path().toStdString()) /
      "empty-outline-saved.json";
  if (!JsonSerializer::savePage(emptyOutlineSavedOutput, "empty.png",
                                emptyOutlineRoundTrip, &error)) {
    std::cerr << "Explicit empty outline page failed to save: " << error
              << '\n';
    return 1;
  }
  const auto emptyOutlineSaved = readText(emptyOutlineSavedOutput);
  if (emptyOutlineSaved.find("\"outline_layers\": [") == std::string::npos &&
      emptyOutlineSaved.find("\"outline_layers\":[]") == std::string::npos) {
    std::cerr << "Explicit empty outline layers did not save back to JSON\n";
    return 1;
  }
  const auto legacyMemorySaved = readText(legacyMemoryOutput);
  if (legacyMemorySaved.find("outline_layers") != std::string::npos) {
    std::cerr << "Legacy in-memory outline unexpectedly saved outline_layers\n";
    return 1;
  }

  DocumentModel editedEmptyDocument;
  TextBox editedEmptyBox;
  TextBoxEditingService::setOutlineEnabled(editedEmptyBox, true);
  if (!TextBoxEditingService::removeOutlineLayer(editedEmptyBox, 0)) {
    std::cerr << "Edited outline layer could not be removed\n";
    return 1;
  }
  editedEmptyDocument.textBoxes().push_back(editedEmptyBox);
  const auto editedEmptyOutput =
      std::filesystem::path(tempDir.path().toStdString()) /
      "edited-empty-outline.json";
  if (!JsonSerializer::savePage(editedEmptyOutput, "edited-empty.png",
                                editedEmptyDocument, &error)) {
    std::cerr << "Edited empty outline page failed to save: " << error << '\n';
    return 1;
  }
  const auto editedEmptySaved = readText(editedEmptyOutput);
  if (editedEmptySaved.find("outline_layers") == std::string::npos) {
    std::cerr << "Edited empty outline layers did not persist as explicit empty\n";
    return 1;
  }

  return 0;
}
