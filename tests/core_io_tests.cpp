#include "application/services/PageTextService.h"
#include "application/services/ProjectSessionService.h"
#include "application/services/TextPresetService.h"
#include "application/services/TextWorkflowService.h"
#include "core/ProjectStore.h"
#include "infrastructure/persistence/JsonSerializer.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>

using namespace textfx;

namespace {
std::filesystem::path makeTempDir(const std::string &name) {
  auto path = std::filesystem::temp_directory_path() /
              (name + std::to_string(std::random_device{}()));
  std::filesystem::create_directories(path);
  return path;
}

void touch(const std::filesystem::path &path) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream(path) << "x";
}

std::string readText(const std::filesystem::path &path) {
  std::ifstream input(path);
  std::ostringstream output;
  output << input.rdbuf();
  return output.str();
}

void writeText(const std::filesystem::path &path, const std::string &text) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream(path) << text;
}
} // namespace

TEST_CASE("Project pages are naturally sorted") {
  const auto folder = makeTempDir("textfx-natural-sort-");
  for (const auto &name : {"10.webp", "2.PNG", "notes.txt", "1.jpg", "3.jfif",
                           "page-10.png", "page-2.jpeg"}) {
    touch(folder / name);
  }

  const auto pages = ProjectStore(folder).listPagePaths();
  std::vector<std::string> names;
  std::ranges::transform(
      pages, std::back_inserter(names),
      [](const auto &path) { return path.filename().string(); });

  CHECK(names == std::vector<std::string>{"1.jpg", "2.PNG", "3.jfif", "10.webp",
                                          "page-2.jpeg", "page-10.png"});
  std::filesystem::remove_all(folder);
}

TEST_CASE("Project store uses canonical project folders") {
  const auto folder = makeTempDir("textfx-canonical-folders-");
  const auto cleanPage = folder / ProjectStore::CleanedFolder / "page2.png";
  touch(folder / "page1.png");
  touch(cleanPage);
  touch(folder / ProjectStore::RawFolder / "page2.png");

  const ProjectStore store(folder);

  const auto pages = store.listPagePaths();
  REQUIRE(pages.size() == 1);
  CHECK(pages.front() == cleanPage);
  CHECK(store.rawPagePathFor(cleanPage) ==
        folder / ProjectStore::RawFolder / "page2.png");
  CHECK(store.pageExportPathFor(cleanPage) ==
        folder / ProjectStore::ExportFolder / "page2.png");
  CHECK(store.pageTextsPath() == folder / ProjectStore::PageTextsFile);
  std::filesystem::remove_all(folder);
}

TEST_CASE("Project session service exposes page names labels and keys") {
  const auto folder = makeTempDir("textfx-project-session-pages-");
  for (const auto &name : {"page10.png", "page2.png"}) {
    touch(folder / name);
  }

  const auto pages = ProjectSessionService::discoverPages(ProjectStore(folder));

  REQUIRE(pages.paths.size() == 2);
  CHECK(pages.names == QStringList({QStringLiteral("page2.png"),
                                    QStringLiteral("page10.png")}));
  CHECK(ProjectSessionService::pageName(pages.names, 0) ==
        QStringLiteral("page2.png"));
  CHECK(ProjectSessionService::pageName(pages.names, 2).isEmpty());
  CHECK(ProjectSessionService::pageLabels(pages.names) ==
        QStringList({QStringLiteral("1 - page2.png"),
                     QStringLiteral("2 - page10.png")}));
  CHECK(ProjectSessionService::normalizePageIndex(1, pages.paths.size()) == 1);
  CHECK(ProjectSessionService::normalizePageIndex(-1, pages.paths.size()) ==
        -1);
  CHECK(ProjectSessionService::pageKey(pages.paths.front()) == "page2.png");
  CHECK(ProjectSessionService::pageKey({}).empty());
  std::filesystem::remove_all(folder);
}

TEST_CASE("Missing TextFX page data opens an empty clean document") {
  const auto folder = makeTempDir("textfx-missing-page-data-");
  const auto page = folder / "001.png";
  touch(page);

  DocumentModel document;
  document.addTextBox({});
  REQUIRE(ProjectStore(folder).loadPage(page, document));

  CHECK(document.textBoxes().empty());
  CHECK_FALSE(document.dirty());
  std::filesystem::remove_all(folder);
}

TEST_CASE("Compatible page save preserves MVP editable fields") {
  const auto folder = makeTempDir("textfx-compatible-save-");
  const auto page = folder / "001.png";
  touch(page);

  TextBox box;
  box.text = "Hello";
  box.bounds = {1.0, 2.0, 30.0, 40.0};
  box.rotationDegrees = 15.5;
  box.style = {.fontFamily = "TextFX Custom Missing Family",
               .fontSize = 24,
               .textColor = "ff0000ff",
               .lineSpacing = 8,
               .letterSpacing = 3,
               .bold = true,
               .italic = true,
               .uppercase = true,
               .alignment = TextAlignment::Center};
  box.effects.outlineEnabled = true;
  box.effects.outlineColor = "ffffffff";
  box.effects.outlineSize = 4;
  box.effects.blurEnabled = true;
  box.effects.blurSize = 6;
  box.effects.shadowEnabled = true;
  box.effects.shadowColor = "000000ff";
  box.effects.shadowOffsetX = 5;
  box.effects.shadowOffsetY = 6;
  box.effects.shadowBlurSize = 7;
  box.effects.gradientEnabled = true;
  box.effects.gradientDirection = 1;
  box.effects.gradientColorA = "ff0000ff";
  box.effects.gradientColorB = "0000ffff";
  box.effects.pathEnabled = true;
  box.effects.pathPoints = {{0.0, 0.5}, {1.0, 0.5}};
  box.effects.perspectiveEnabled = true;
  box.effects.perspectiveNw = {1.0, 2.0};

  DocumentModel saved;
  saved.addTextBox(box);
  saved.markSaved();
  REQUIRE(ProjectStore(folder).savePage(page, saved));

  DocumentModel loaded;
  REQUIRE(ProjectStore(folder).loadPage(page, loaded));
  REQUIRE(loaded.textBoxes().size() == 1);
  const auto &loadedBox = loaded.textBoxes().front();
  CHECK(loadedBox.text == "Hello");
  CHECK(loadedBox.bounds.w == 30.0);
  CHECK(loadedBox.style.fontFamily == "TextFX Custom Missing Family");
  CHECK(loadedBox.style.alignment == TextAlignment::Center);
  CHECK(loadedBox.style.lowercase == false);
  CHECK(loadedBox.effects.shadowBlurSize == 7);
  CHECK(loadedBox.effects.gradientColorB == "0000ffff");
  CHECK(loadedBox.effects.pathPoints.size() == 3);
  CHECK(loadedBox.effects.perspectiveNw.x == 1.0);
  std::filesystem::remove_all(folder);
}

TEST_CASE("Compatible page load defaults missing lowercase to false") {
  const auto folder = makeTempDir("textfx-compatible-lowercase-default-");
  const auto page = folder / "001.png";
  touch(page);
  std::filesystem::create_directories(folder / ".textfx");
  writeText(folder / ".textfx/001.json",
            R"({"format":"textfx.page-boxes.v1","page":"001.png","boxes":[{"text":"Hello","x":1,"y":2,"w":30,"h":40,"uppercase":false}]})");

  DocumentModel loaded;
  REQUIRE(ProjectStore(folder).loadPage(page, loaded));
  REQUIRE(loaded.textBoxes().size() == 1);
  CHECK_FALSE(loaded.textBoxes().front().style.lowercase);
  std::filesystem::remove_all(folder);
}

TEST_CASE("Compatible page load prefers uppercase when both case transforms are present") {
  const auto folder = makeTempDir("textfx-compatible-case-conflict-");
  const auto page = folder / "001.png";
  touch(page);
  std::filesystem::create_directories(folder / ".textfx");
  writeText(folder / ".textfx/001.json",
            R"({"format":"textfx.page-boxes.v1","page":"001.png","boxes":[{"text":"Hello","x":1,"y":2,"w":30,"h":40,"uppercase":true,"lowercase":true}]})");

  DocumentModel loaded;
  REQUIRE(ProjectStore(folder).loadPage(page, loaded));
  REQUIRE(loaded.textBoxes().size() == 1);
  CHECK(loaded.textBoxes().front().style.uppercase);
  CHECK_FALSE(loaded.textBoxes().front().style.lowercase);
  std::filesystem::remove_all(folder);
}

TEST_CASE("Perspective offsets persist as TextFX pixel offsets") {
  const auto folder = makeTempDir("textfx-perspective-offsets-");
  const auto page = folder / "001.png";
  touch(page);

  TextBox box;
  box.bounds = {10.0, 20.0, 100.0, 80.0};
  box.effects.perspectiveEnabled = true;
  box.effects.perspectiveNw = {5.0, -3.0};
  box.effects.perspectiveNe = {-7.0, 4.0};

  DocumentModel saved;
  saved.addTextBox(box);
  REQUIRE(ProjectStore(folder).savePage(page, saved));

  const auto text = readText(ProjectStore(folder).pageSavePathFor(page));
  CHECK(text.find("[\n                    5") != std::string::npos);

  DocumentModel loaded;
  REQUIRE(ProjectStore(folder).loadPage(page, loaded));
  REQUIRE(loaded.textBoxes().size() == 1);
  CHECK(loaded.textBoxes().front().effects.perspectiveNw.x == 5.0);
  CHECK(loaded.textBoxes().front().effects.perspectiveNw.y == -3.0);
  CHECK(loaded.textBoxes().front().effects.perspectiveNe.x == -7.0);
  CHECK(loaded.textBoxes().front().effects.perspectiveNe.y == 4.0);
  std::filesystem::remove_all(folder);
}

TEST_CASE("Unsupported fields are ignored on import and omitted on save") {
  const auto folder = makeTempDir("textfx-unsupported-import-");
  const auto page = folder / "001.png";
  const auto savePath = ProjectStore(folder).pageSavePathFor(page);
  touch(page);
  std::filesystem::create_directories(savePath.parent_path());
  std::ofstream(savePath) << R"({
        "format": "textfx.page-boxes.v1",
        "page": "001.png",
        "typebubblex_only": true,
        "boxes": [{"text": "Kept", "x": 1, "y": 2, "w": 3, "h": 4, "mask_brush": {"unsupported": true}, "shadow_blur_size": 999}]
    })";

  DocumentModel document;
  REQUIRE(ProjectStore(folder).loadPage(page, document));
  REQUIRE(document.textBoxes().size() == 1);
  CHECK(document.textBoxes().front().text == "Kept");
  CHECK(document.textBoxes().front().effects.shadowBlurSize == 128);
  REQUIRE(ProjectStore(folder).savePage(page, document));

  const std::string text = readText(savePath);
  CHECK(text.find("mask_brush") == std::string::npos);
  CHECK(text.find("typebubblex_only") == std::string::npos);
  CHECK(text.find("Kept") != std::string::npos);
  CHECK(text.find("\"shadow_blur_size\": 128") != std::string::npos);
  std::filesystem::remove_all(folder);
}

TEST_CASE("Texts.txt page texts are parsed by page section") {
  const auto folder = makeTempDir("textfx-page-texts-");
  std::ofstream(folder / ProjectStore::PageTextsFile) << R"(
ignored before section
[page2.png]
 First line 

Second line
[ page10.png ]
Third line
)";

  const auto texts = ProjectStore(folder).loadPageTexts();

  REQUIRE(texts.at("page2.png").size() == 2);
  CHECK(texts.at("page2.png").at(0) == "First line");
  CHECK(texts.at("page2.png").at(1) == "Second line");
  REQUIRE(texts.at("page10.png").size() == 1);
  CHECK(texts.at("page10.png").at(0) == "Third line");
  std::filesystem::remove_all(folder);
}

TEST_CASE("Legacy lowercase texts file remains a read fallback") {
  const auto folder = makeTempDir("textfx-legacy-page-texts-");
  std::ofstream(folder / "texts.txt") << R"(
[page1.png]
Legacy line
)";

  const auto texts = ProjectStore(folder).loadPageTexts();

  REQUIRE(texts.at("page1.png").size() == 1);
  CHECK(texts.at("page1.png").at(0) == "Legacy line");
  std::filesystem::remove_all(folder);
}

TEST_CASE("Page text service applies selected texts and advances per-page "
          "positions") {
  PageTextMap pageTexts{{"page1.png", {"First", "Second"}},
                        {"page2.png", {"Other"}}};
  PageTextPositionMap positions;
  std::vector<TextBox> boxes(1);

  CHECK(PageTextService::textsForPage(pageTexts, "page1.png") ==
        QStringList({QStringLiteral("First"), QStringLiteral("Second")}));
  CHECK(PageTextService::textsForPage(pageTexts, "missing.png").isEmpty());
  CHECK(PageTextService::nextTextIndex(positions, "page1.png") == 0);
  REQUIRE(positions.at("page1.png") == 0);

  CHECK(PageTextService::applyText(boxes, 0, pageTexts, positions, "page1.png",
                                   0) == PageTextApplyStatus::Applied);
  CHECK(boxes.front().text == "First");
  CHECK(positions.at("page1.png") == 1);

  CHECK(PageTextService::applyText(boxes, 0, pageTexts, positions, "page1.png",
                                   1) == PageTextApplyStatus::Applied);
  CHECK(boxes.front().text == "Second");
  CHECK(positions.at("page1.png") == 2);

  CHECK(PageTextService::applyText(boxes, 0, pageTexts, positions, "page1.png",
                                   2) == PageTextApplyStatus::MissingText);
  CHECK(positions.at("page1.png") == 2);
  CHECK(PageTextService::applyText(boxes, -1, pageTexts, positions, "page2.png",
                                   0) == PageTextApplyStatus::NoSelectedBox);
  CHECK_FALSE(positions.contains("page2.png"));
}

TEST_CASE("Text preset service reports structured mutation outcomes") {
  TextBox source;
  source.style.fontFamily = "Inter";
  source.style.fontSize = 24;
  TextBox target;
  std::vector<TextPreset> projectPresets{{"Narration", {.fontSize = 18}}};
  std::vector<TextPreset> presets = projectPresets;

  CHECK(TextPresetService::applySelectedPresetResult(
            target, {.availablePresets = presets, .selectedPresetIndex = -1})
            .status == TextPresetStatus::InvalidPresetIndex);
  CHECK(target.style.fontSize != 18);

  auto addBlank = TextPresetService::addPresetResult(
      {.projectPresets = projectPresets, .sourceBox = source}, "  ");
  CHECK(addBlank.status == TextPresetStatus::InvalidName);

  auto added = TextPresetService::addPresetResult(
      {.projectPresets = projectPresets, .sourceBox = source}, " Caption ");
  CHECK(added.status == TextPresetStatus::Succeeded);
  CHECK(added.preferredName == "Caption");
  REQUIRE(projectPresets.size() == 2);
  CHECK(projectPresets.back().name == "Caption");
  source.style.fontSize = 32;
  auto duplicateAdd = TextPresetService::addPresetResult(
      {.projectPresets = projectPresets, .sourceBox = source}, "Caption");
  CHECK(duplicateAdd.status == TextPresetStatus::Succeeded);
  CHECK(duplicateAdd.preferredName == "Caption");
  REQUIRE(projectPresets.size() == 2);
  CHECK(projectPresets.back().style.fontSize == 32);

  presets = projectPresets;
  ProjectTextPresetContext presetCtx{.projectPresets = projectPresets,
                                     .availablePresets = presets,
                                     .selectedPresetIndex = 0};
  auto duplicateRename =
      TextPresetService::renameSelectedPresetResult(presetCtx, "Caption");
  CHECK(duplicateRename.status == TextPresetStatus::DuplicateName);
  std::string preferredName = "unchanged";
  CHECK_FALSE(TextPresetService::renameSelectedPreset(presetCtx, "Caption",
                                                      preferredName));
  CHECK(preferredName == "unchanged");

  std::vector<TextPreset> documentOnlyPresets{{"DocumentOnly", {.fontSize = 30}}};
  ProjectTextPresetContext documentOnlyCtx{
      .projectPresets = projectPresets,
      .availablePresets = documentOnlyPresets,
      .selectedPresetIndex = 0};
  auto missingUpdate =
      TextPresetService::updateSelectedPresetResult(documentOnlyCtx, source);
  CHECK(missingUpdate.status == TextPresetStatus::Succeeded);
  CHECK(missingUpdate.preferredName == "DocumentOnly");
  REQUIRE(projectPresets.size() == 3);
  CHECK(projectPresets.back().name == "DocumentOnly");
  CHECK(projectPresets.back().style.fontSize == 32);
  std::vector<TextPreset> deleteOnlyPresets{{"DeleteOnly", {.fontSize = 30}}};
  ProjectTextPresetContext deleteOnlyCtx{.projectPresets = projectPresets,
                                         .availablePresets = deleteOnlyPresets,
                                         .selectedPresetIndex = 0};
  auto missingDelete = TextPresetService::deleteSelectedPresetResult(deleteOnlyCtx);
  CHECK(missingDelete.status == TextPresetStatus::MissingProjectPreset);
}

TEST_CASE("Text workflow service delegates page text and preset orchestration") {
  PageTextMap pageTexts{{"page1.png", {"First", "Second"}}};
  PageTextPositionMap positions;
  std::vector<TextBox> boxes(1);
  PageTextWorkflowContext pageCtx{boxes, 0, pageTexts, positions, "page1.png"};

  CHECK(TextWorkflowService::nextPageTextIndex(pageCtx) == 0);
  auto pageResult = TextWorkflowService::applyPageText(pageCtx, 0);
  CHECK(pageResult.status == PageTextApplyStatus::Applied);
  CHECK(boxes.front().text == "First");
  CHECK(positions.at("page1.png") == 1);

  std::vector<TextPreset> projectPresets;
  std::vector<TextPreset> presets{{"Narration", {.fontSize = 18}}};
  PresetWorkflowContext missingBoxCtx{nullptr, projectPresets, presets, 0};
  auto missingBox = TextWorkflowService::applySelectedPreset(missingBoxCtx);
  CHECK(missingBox.precondition == WorkflowPrecondition::MissingSelectedBox);
  CHECK_FALSE(missingBox.succeeded());

  TextBox selectedBox;
  PresetWorkflowContext presetCtx{&selectedBox, projectPresets, presets, 5};
  auto invalidPreset = TextWorkflowService::applySelectedPreset(presetCtx);
  CHECK(invalidPreset.precondition == WorkflowPrecondition::None);
  CHECK(invalidPreset.serviceStatus == TextPresetStatus::InvalidPresetIndex);

  presetCtx.selectedPresetIndex = 0;
  auto appliedPreset = TextWorkflowService::applySelectedPreset(presetCtx);
  CHECK(appliedPreset.succeeded());
  CHECK(selectedBox.style.fontSize == 18);
}

TEST_CASE("Missing project presets returns no presets without writing a file") {
  const auto folder = makeTempDir("textfx-default-presets-");
  DocumentModel document;
  std::vector<TextPreset> projectPresets;

  REQUIRE(ProjectStore(folder).loadPresets(document, projectPresets));

  CHECK_FALSE(std::filesystem::exists(ProjectStore(folder).presetsPath()));
  CHECK(document.presets().empty());
  CHECK(projectPresets.empty());
  std::filesystem::remove_all(folder);
}

TEST_CASE("Project presets persist as TextFX presets file") {
  const auto folder = makeTempDir("textfx-project-presets-");
  ProjectStore store(folder);
  std::vector<TextPreset> saved{{"Narration",
                                 {.fontFamily = "Inter",
                                  .fontSize = 18,
                                  .textColor = "112233ff",
                                  .bold = true}}};

  REQUIRE(store.savePresets(saved));

  DocumentModel document;
  std::vector<TextPreset> projectPresets;
  REQUIRE(store.loadPresets(document, projectPresets));
  REQUIRE(projectPresets.size() == 1);
  CHECK(projectPresets.front().name == "Narration");
  CHECK(projectPresets.front().style.bold);
  const std::string text = readText(store.presetsPath());
  CHECK(text.find("textfx.text-presets.v1") != std::string::npos);
  CHECK(text.find("Narration") != std::string::npos);
  std::filesystem::remove_all(folder);
}

TEST_CASE("Project presets load without built-in merging") {
  const auto folder = makeTempDir("textfx-override-presets-");
  ProjectStore store(folder);
  REQUIRE(store.savePresets(
      {{"Custom Bubble", {.fontFamily = "Custom Bubble", .fontSize = 30}}}));

  DocumentModel document;
  std::vector<TextPreset> projectPresets;
  REQUIRE(store.loadPresets(document, projectPresets));

  REQUIRE(document.presets().size() == 1);
  CHECK(document.presets().front().name == "Custom Bubble");
  CHECK(document.presets().front().style.fontFamily == "Custom Bubble");
  CHECK(document.presets().front().style.fontSize == 30);
  std::filesystem::remove_all(folder);
}
