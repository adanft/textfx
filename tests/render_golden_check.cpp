#include "app/viewmodels/BoxRenderState.h"
#include "application/services/ProjectExportService.h"
#include "app/viewmodels/EditorViewModels.h"
#include "infrastructure/persistence/ProjectStore.h"
#include "render/RenderGraph.h"

#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QPoint>
#include <QRect>
#include <QTemporaryDir>

using namespace textfx;

#ifdef TEXTFX_ENABLE_TEST_HOOKS
namespace textfx::test_hooks {
void failPngCommit(std::filesystem::path path);
void clearPngCommitFailure();
} // namespace textfx::test_hooks
#endif

namespace {
struct PngCommitFailureGuard {
  explicit PngCommitFailureGuard(const std::filesystem::path &path) {
    textfx::test_hooks::failPngCommit(path);
  }

  ~PngCommitFailureGuard() { textfx::test_hooks::clearPngCommitFailure(); }
};

std::string readBytes(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(input),
          std::istreambuf_iterator<char>()};
}

bool hasPngMagic(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  char magic[8]{};
  input.read(magic, 8);
  return input.gcount() == 8 &&
         std::string(magic, 8) == std::string("\x89PNG\r\n\x1a\n", 8);
}

int pngColorType(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::array<unsigned char, 26> header{};
  input.read(reinterpret_cast<char *>(header.data()), header.size());
  if (input.gcount() != static_cast<std::streamsize>(header.size()) ||
      std::string(reinterpret_cast<const char *>(header.data()), 8) !=
          std::string("\x89PNG\r\n\x1a\n", 8) ||
      std::string(reinterpret_cast<const char *>(header.data() + 12), 4) !=
          "IHDR")
    return -1;
  return header[25];
}

QImage exportedImage(const RenderGraph &graph, const DocumentModel &document,
                     const std::filesystem::path &pagePath,
                     const std::filesystem::path &outPath) {
  std::string error;
  if (!graph.exportPagePng(document, pagePath, outPath, &error)) {
    std::cerr << "PNG export failed: " << error << '\n';
    return {};
  }
  return QImage(QString::fromStdString(outPath.string()));
}

bool imagesDiffer(const QImage &a, const QImage &b) {
  if (a.size() != b.size())
    return true;
  for (int y = 0; y < a.height(); ++y) {
    for (int x = 0; x < a.width(); ++x) {
      if (a.pixelColor(x, y) != b.pixelColor(x, y))
        return true;
    }
  }
  return false;
}

int imageDifferenceCount(const QImage &a, const QImage &b) {
  if (a.size() != b.size())
    return std::numeric_limits<int>::max();
  int count = 0;
  for (int y = 0; y < a.height(); ++y) {
    for (int x = 0; x < a.width(); ++x) {
      if (a.pixelColor(x, y) != b.pixelColor(x, y))
        ++count;
    }
  }
  return count;
}

int firstNonBackgroundY(const QImage &image, const QColor &background) {
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (image.pixelColor(x, y) != background)
        return y;
    }
  }
  return -1;
}

QRect nonBackgroundBounds(const QImage &image, const QColor &background) {
  QRect bounds;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      if (image.pixelColor(x, y) != background)
        bounds = bounds.isNull() ? QRect(x, y, 1, 1)
                                 : bounds.united(QRect(x, y, 1, 1));
    }
  }
  return bounds;
}

int countPixels(const QImage &image, const QColor &background,
                const auto &predicate) {
  int count = 0;
  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const auto color = image.pixelColor(x, y);
      if (color != background && predicate(color))
        ++count;
    }
  }
  return count;
}

QPoint firstDarkPixelInRect(const QImage &image, const QRect &bounds) {
  const QRect clipped = bounds.intersected(image.rect());
  for (int y = clipped.top(); y <= clipped.bottom(); ++y) {
    for (int x = clipped.left(); x <= clipped.right(); ++x) {
      const QColor color = image.pixelColor(x, y);
      if (color.alpha() > 200 && color.red() < 40 && color.green() < 40 &&
          color.blue() < 40)
        return QPoint(x, y);
    }
  }
  return QPoint(-1, -1);
}
} // namespace

int main(int argc, char **argv) {
  QGuiApplication app(argc, argv);

  const RenderGraph graph;
  QTemporaryDir tempDir;
  if (!tempDir.isValid()) {
    std::cerr
        << "Failed to create temporary directory for render golden check\n";
    return 1;
  }
  const auto tempPath = std::filesystem::path(tempDir.path().toStdString());

  TextBox mappedBox;
  mappedBox.text = "Mapped";
  mappedBox.bounds = {12.0, 34.0, 156.0, 78.0};
  mappedBox.rotationDegrees = 17.5;
  mappedBox.style.fontFamily = "TextFX Missing Render State Font";
  mappedBox.style.fontSize = 29;
  mappedBox.style.textColor = "abcdef99";
  mappedBox.style.lineSpacing = 6;
  mappedBox.style.letterSpacing = 2;
  mappedBox.style.bold = true;
  mappedBox.style.italic = true;
  mappedBox.style.uppercase = true;
  mappedBox.style.lowercase = true;
  mappedBox.style.alignment = TextAlignment::Right;
  mappedBox.effects.outlineEnabled = true;
  mappedBox.effects.outlineColor = "111111ff";
  mappedBox.effects.outlineSize = 4;
  mappedBox.effects.outlineLayers = {{true, "111111ff", 12},
                                     {true, "333333ff", 4}};
  mappedBox.effects.blurEnabled = true;
  mappedBox.effects.blurSize = 5;
  mappedBox.effects.shadowEnabled = true;
  mappedBox.effects.shadowColor = "222222ff";
  mappedBox.effects.shadowOffsetX = -3;
  mappedBox.effects.shadowOffsetY = 8;
  mappedBox.effects.shadowBlurSize = 9;
  mappedBox.effects.gradientEnabled = true;
  mappedBox.effects.gradientDirection = 1;
  mappedBox.effects.gradientColorA = "123456ff";
  mappedBox.effects.gradientColorB = "654321ff";
  mappedBox.effects.pathEnabled = true;
  mappedBox.effects.pathMode = 1;
  mappedBox.effects.pathPoints = {{0.1, 0.2}, {0.4, 0.5}, {0.8, 0.9}};
  mappedBox.effects.perspectiveEnabled = true;
  mappedBox.effects.perspectiveNw = {1.0, 2.0};
  mappedBox.effects.perspectiveNe = {3.0, 4.0};
  mappedBox.effects.perspectiveSe = {5.0, 6.0};
  mappedBox.effects.perspectiveSw = {7.0, 8.0};

  const BoxRenderState renderState = mapBoxRenderState(mappedBox, 4);
  const QVariantMap groupedEffects = renderState.effects;
  const QVariantMap groupedOutline = groupedEffects.value(QStringLiteral("outline")).toMap();
  const QVariantMap groupedBlur = groupedEffects.value(QStringLiteral("blur")).toMap();
  const QVariantMap groupedShadow = groupedEffects.value(QStringLiteral("shadow")).toMap();
  const QVariantMap groupedGradient = groupedEffects.value(QStringLiteral("gradient")).toMap();
  const QVariantMap groupedPath = groupedEffects.value(QStringLiteral("path")).toMap();
  const QVariantMap groupedPerspective = groupedEffects.value(QStringLiteral("perspective")).toMap();
  if (renderState.index != 4 || renderState.text != QStringLiteral("Mapped") ||
      renderState.x != 12.0 || renderState.y != 34.0 ||
      renderState.width != 156.0 || renderState.height != 78.0 ||
      renderState.rotation != 17.5 || renderState.fontSize != 29 ||
      renderState.color != QStringLiteral("abcdef99") ||
      renderState.lineSpacing != 6 || renderState.letterSpacing != 2 ||
      !renderState.bold || !renderState.italic || !renderState.uppercase ||
      renderState.lowercase || renderState.alignment != 2 || !renderState.outline ||
      renderState.outlineColor != QStringLiteral("111111ff") ||
      renderState.outlineSize != 12 || renderState.outlineLayers.size() != 2 ||
      !renderState.blur ||
      renderState.blurSize != 5 || !renderState.shadow ||
      renderState.shadowColor != QStringLiteral("222222ff") ||
      renderState.shadowOffsetX != -3 || renderState.shadowOffsetY != 8 ||
      renderState.shadowBlurSize != 9 || !renderState.gradient ||
      renderState.gradientDirection != 1 ||
      renderState.gradientColorA != QStringLiteral("123456ff") ||
      renderState.gradientColorB != QStringLiteral("654321ff") ||
      !renderState.path || renderState.pathMode != 1 ||
      renderState.pathPoints.size() != 3 || !renderState.perspective ||
      renderState.perspectiveNe.at(0).toDouble() != 3.0 ||
      !groupedOutline.value(QStringLiteral("enabled")).toBool() ||
      groupedOutline.value(QStringLiteral("color")).toString() != QStringLiteral("111111ff") ||
      groupedOutline.value(QStringLiteral("size")).toInt() != 12 ||
      groupedOutline.value(QStringLiteral("layers")).toList().size() != 2 ||
      !groupedBlur.value(QStringLiteral("enabled")).toBool() ||
      groupedBlur.value(QStringLiteral("size")).toInt() != 5 ||
      !groupedShadow.value(QStringLiteral("enabled")).toBool() ||
      groupedShadow.value(QStringLiteral("color")).toString() != QStringLiteral("222222ff") ||
      groupedShadow.value(QStringLiteral("offsetX")).toDouble() != -3.0 ||
      groupedShadow.value(QStringLiteral("offsetY")).toDouble() != 8.0 ||
      groupedShadow.value(QStringLiteral("blurSize")).toInt() != 9 ||
      !groupedGradient.value(QStringLiteral("enabled")).toBool() ||
      groupedGradient.value(QStringLiteral("direction")).toInt() != 1 ||
      groupedGradient.value(QStringLiteral("colorA")).toString() != QStringLiteral("123456ff") ||
      groupedGradient.value(QStringLiteral("colorB")).toString() != QStringLiteral("654321ff") ||
      !groupedPath.value(QStringLiteral("enabled")).toBool() ||
      groupedPath.value(QStringLiteral("mode")).toInt() != 1 ||
      groupedPath.value(QStringLiteral("points")).toList().size() != 3 ||
      !groupedPerspective.value(QStringLiteral("enabled")).toBool() ||
      groupedPerspective.value(QStringLiteral("ne")).toList().at(0).toDouble() != 3.0 ||
      groupedPerspective.value(QStringLiteral("sw")).toList().at(1).toDouble() != 8.0) {
    std::cerr << "Expected BoxRenderState to preserve styled/path/perspective box semantics\n";
    return 1;
  }

  const QVariantMap viewModel = EditorViewModels::textBoxMap(mappedBox, 4);
  if (viewModel.value(QStringLiteral("text")).toString() != renderState.text ||
      viewModel.value(QStringLiteral("w")).toDouble() != renderState.width ||
      viewModel.value(QStringLiteral("fontFamily")).toString() !=
          renderState.fontFamily ||
      viewModel.value(QStringLiteral("resolvedFontFamily")).toString() !=
          renderState.resolvedFontFamily ||
      viewModel.value(QStringLiteral("pathPoints")).toList() !=
          renderState.pathPoints ||
      viewModel.value(QStringLiteral("perspectiveSw")).toList() !=
          renderState.perspectiveSw ||
      viewModel.value(QStringLiteral("effects")).toMap() != renderState.effects) {
    std::cerr << "Expected legacy box map to use BoxRenderState-equivalent values\n";
    return 1;
  }

  const auto missingPageResult =
      graph.exportPagePngResult(DocumentModel{}, tempPath / "missing-page.png",
                                tempPath / "missing-export.png");
  if (missingPageResult || missingPageResult.error() !=
                               "Could not load page image: " +
                                   (tempPath / "missing-page.png").string()) {
    std::cerr << "Expected missing page export to return an equivalent "
                 "std::expected error\n";
    return 1;
  }

  QImage page(240, 160, QImage::Format_RGBA8888);
  const QColor background(230, 230, 230, 255);
  page.fill(background);
  const auto pagePath = tempPath / "export-page.png";
  const auto outPath = tempPath / "export.png";
  if (!page.save(QString::fromStdString(pagePath.string()), "PNG"))
    return 1;

  DocumentModel paintOrderDocument;
  TextBox paintOrderText;
  paintOrderText.text = "MMMM";
  paintOrderText.bounds = {36.0, 4.0, 180.0, 72.0};
  paintOrderText.style.fontSize = 48;
  paintOrderText.style.textColor = "000000ff";
  paintOrderDocument.addTextBox(paintOrderText);
  paintOrderDocument.paint().behindText.push_back(
      {"ff0000ff", 30.0, 1.0, {{20.0, 40.0}, {220.0, 40.0}}});
  paintOrderDocument.paint().aboveText.push_back(
      {"0000ffff", 30.0, 1.0, {{20.0, 40.0}, {220.0, 40.0}}});

  DocumentModel textOnlyPaintOrderDocument;
  textOnlyPaintOrderDocument.addTextBox(paintOrderText);
  const auto textOnlyPaintOrderImage = exportedImage(
      graph, textOnlyPaintOrderDocument, pagePath,
      tempPath / "paint-order-text-only.png");
  const QPoint overlappedTextPixel =
      firstDarkPixelInRect(textOnlyPaintOrderImage, QRect(40, 26, 150, 28));

  DocumentModel behindTextPaintOrderDocument;
  behindTextPaintOrderDocument.addTextBox(paintOrderText);
  behindTextPaintOrderDocument.paint().behindText.push_back(
      {"ff0000ff", 30.0, 1.0, {{20.0, 40.0}, {220.0, 40.0}}});
  const auto behindTextPaintOrderImage = exportedImage(
      graph, behindTextPaintOrderDocument, pagePath,
      tempPath / "paint-order-behind-text.png");

  const auto paintOrderImage = exportedImage(graph, paintOrderDocument, pagePath,
                                             tempPath / "paint-order.png");
  if (paintOrderImage.isNull() || textOnlyPaintOrderImage.isNull() ||
      behindTextPaintOrderImage.isNull() || overlappedTextPixel.x() < 0 ||
      behindTextPaintOrderImage.pixelColor(overlappedTextPixel) !=
          textOnlyPaintOrderImage.pixelColor(overlappedTextPixel) ||
      paintOrderImage.pixelColor(overlappedTextPixel) !=
          QColor(0, 0, 255, 255)) {
    std::cerr << "Expected paint export to draw behind-text paint below text "
                 "and above-text paint above text\n";
    return 1;
  }

  DocumentModel invalidPaintColorDocument;
  invalidPaintColorDocument.paint().aboveText.push_back(
      {"zzzzzzzz", 10.0, 1.0, {{20.0, 40.0}, {120.0, 40.0}}});
  const auto invalidPaintColorImage = exportedImage(
      graph, invalidPaintColorDocument, pagePath,
      tempPath / "invalid-paint-color.png");
  if (invalidPaintColorImage.isNull() ||
      invalidPaintColorImage.pixelColor(70, 40) != QColor(0, 0, 0, 255)) {
    std::cerr << "Expected invalid paint colors to export with a safe fallback\n";
    return 1;
  }

  const auto timedOutPath = tempPath / "timed-export.png";
  const auto timedResult =
      graph.exportPagePngTimed(DocumentModel{}, pagePath, timedOutPath);
  if (!timedResult ||
      timedResult->timing.elapsed <
          RenderGraph::ExportTiming::Duration::zero() ||
      !hasPngMagic(timedOutPath)) {
    std::cerr << "Expected timed export to return non-negative timing data and "
                 "write a PNG\n";
    return 1;
  }

  const auto preservedOutPath = tempPath / "preserved-export.png";
  {
    std::ofstream existing(preservedOutPath, std::ios::binary);
    existing << "previous export bytes";
  }
  const std::string previousExport = readBytes(preservedOutPath);
  {
    const PngCommitFailureGuard failCommit(preservedOutPath);
    const auto failedReplace = graph.exportPagePngResult(
        DocumentModel{}, pagePath, preservedOutPath);
    if (failedReplace ||
        failedReplace.error() !=
            "Could not write PNG output: " + preservedOutPath.string()) {
      std::cerr << "Expected forced PNG commit failure to return the write "
                   "error contract\n";
      return 1;
    }
  }
  if (readBytes(preservedOutPath) != previousExport) {
    std::cerr << "Expected failed PNG replacement to preserve the previous "
                 "export bytes\n";
    return 1;
  }

  QImage rgbPage(64, 48, QImage::Format_RGB888);
  rgbPage.fill(QColor(12, 34, 56));
  const auto rgbPagePath = tempPath / "rgb-source.png";
  const auto rgbOutPath = tempPath / "rgb-export.png";
  if (!rgbPage.save(QString::fromStdString(rgbPagePath.string()), "PNG"))
    return 1;
  const auto rgbResult = graph.exportPagePngTimed(DocumentModel{}, rgbPagePath,
                                                  rgbOutPath);
  const QImage rgbExport(QString::fromStdString(rgbOutPath.string()));
  if (!rgbResult || rgbExport.isNull() || rgbExport.hasAlphaChannel() ||
      pngColorType(rgbOutPath) != 2 || rgbExport.size() != rgbPage.size() ||
      rgbExport.pixelColor(0, 0) != rgbPage.pixelColor(0, 0)) {
    std::cerr << "Expected RGB source export to remain RGB/no-alpha\n";
    return 1;
  }

  QImage alphaPage(64, 48, QImage::Format_RGBA8888);
  alphaPage.fill(QColor(12, 34, 56, 255));
  alphaPage.setPixelColor(0, 0, QColor(12, 34, 56, 96));
  const auto alphaPagePath = tempPath / "alpha-source.png";
  const auto alphaOutPath = tempPath / "alpha-export.png";
  if (!alphaPage.save(QString::fromStdString(alphaPagePath.string()), "PNG"))
    return 1;
  const auto alphaResult = graph.exportPagePngTimed(DocumentModel{},
                                                    alphaPagePath, alphaOutPath);
  const QImage alphaExport(QString::fromStdString(alphaOutPath.string()));
  if (!alphaResult || alphaExport.isNull() || !alphaExport.hasAlphaChannel() ||
      pngColorType(alphaOutPath) != 6 ||
      alphaExport.pixelColor(0, 0).alpha() != 96 ||
      alphaExport.size() != alphaPage.size()) {
    std::cerr << "Expected alpha source export to preserve alpha\n";
    return 1;
  }

  DocumentModel serviceDocument;
  ProjectStore exportStore(tempPath);
  const auto serviceResult =
      ProjectExportService(exportStore)
          .exportPages(ExportJob{
              .pagePaths = {pagePath, tempPath / "missing-service-page.png"},
              .currentPage = pagePath,
              .currentDocument = &serviceDocument,
          });
  if (serviceResult.total != 2 || serviceResult.completed != 1 ||
      serviceResult.failed != 1 || serviceResult.pages.size() != 2 ||
      serviceResult.elapsed < std::chrono::steady_clock::duration::zero()) {
    std::cerr << "Expected project export service to aggregate completed and "
                 "failed page counts\n";
    return 1;
  }
  if (!serviceResult.pages[0].completed ||
      serviceResult.pages[0].timing.elapsed <
          RenderGraph::ExportTiming::Duration::zero() ||
      !hasPngMagic(exportStore.pageExportPathFor(pagePath))) {
    std::cerr << "Expected project export service to retain successful page "
                 "timing and output path\n";
    return 1;
  }
  if (serviceResult.pages[1].completed ||
      serviceResult.pages[1].error !=
          "Could not load page image: " +
              (tempPath / "missing-service-page.png").string()) {
    std::cerr << "Expected project export service to retain failed page error "
                 "details\n";
    return 1;
  }

  const auto corruptSavedPagePath = tempPath / "corrupt-saved-page.png";
  if (!page.save(QString::fromStdString(corruptSavedPagePath.string()), "PNG"))
    return 1;
  std::filesystem::create_directories(tempPath / ProjectStore::SaveFolder);
  {
    std::ofstream corruptSave(
        exportStore.pageSavePathFor(corruptSavedPagePath));
    corruptSave << "not-json";
  }
  const auto corruptSaveResult =
      ProjectExportService(exportStore)
          .exportPages(ExportJob{
              .pagePaths = {pagePath, corruptSavedPagePath},
              .currentPage = pagePath,
              .currentDocument = &serviceDocument,
          });
  if (corruptSaveResult.total != 2 || corruptSaveResult.completed != 1 ||
      corruptSaveResult.failed != 1 || corruptSaveResult.pages.size() != 2) {
    std::cerr << "Expected corrupt saved page JSON to count as one failed "
                 "export page\n";
    return 1;
  }
  if (!corruptSaveResult.pages[0].completed ||
      corruptSaveResult.pages[0].error != "") {
    std::cerr << "Expected current page to export successfully before corrupt "
                 "saved page load failure\n";
    return 1;
  }
  if (corruptSaveResult.pages[1].completed ||
      corruptSaveResult.pages[1].pagePath != corruptSavedPagePath ||
      corruptSaveResult.pages[1].error != "Save file is not valid JSON.") {
    std::cerr << "Expected corrupt saved page load failure to preserve the "
                 "per-page error\n";
    return 1;
  }

  const auto parentConflict = tempPath / "parent-conflict";
  {
    std::ofstream conflict(parentConflict);
    conflict << "not a directory";
  }
  const auto parentConflictOut = parentConflict / "export.png";
  try {
    const auto parentConflictResult =
        graph.exportPagePngResult(DocumentModel{}, pagePath, parentConflictOut);
    if (parentConflictResult ||
        parentConflictResult.error() !=
            "Could not write PNG output: " + parentConflictOut.string()) {
      std::cerr << "Expected parent path conflict export to return a write "
                   "std::expected error\n";
      return 1;
    }
  } catch (const std::exception &ex) {
    std::cerr << "Parent path conflict export threw unexpectedly: " << ex.what()
              << '\n';
    return 1;
  }

  std::string wrapperError;
  try {
    if (graph.exportPagePng(DocumentModel{}, pagePath, parentConflictOut,
                            &wrapperError) ||
        wrapperError !=
            "Could not write PNG output: " + parentConflictOut.string()) {
      std::cerr << "Expected wrapper export to return false with the write "
                   "error string\n";
      return 1;
    }
  } catch (const std::exception &ex) {
    std::cerr << "Wrapper parent path conflict export threw unexpectedly: "
              << ex.what() << '\n';
    return 1;
  }

  const auto cleanupConflictOut = tempPath / "cleanup-conflict-export.png";
  std::filesystem::create_directory(cleanupConflictOut);
  {
    std::ofstream conflict(cleanupConflictOut / "child.txt");
    conflict << "keeps directory non-empty";
  }
  try {
    const auto cleanupConflictResult = graph.exportPagePngResult(
        DocumentModel{}, pagePath, cleanupConflictOut);
    if (cleanupConflictResult ||
        cleanupConflictResult.error() !=
            "Could not write PNG output: " + cleanupConflictOut.string()) {
      std::cerr << "Expected existing output cleanup conflict export to return "
                   "a write std::expected error\n";
      return 1;
    }
  } catch (const std::exception &ex) {
    std::cerr << "Existing output cleanup conflict export threw unexpectedly: "
              << ex.what() << '\n';
    return 1;
  }

  DocumentModel editorLayout;
  TextBox plain;
  plain.text = "Editor sized";
  plain.bounds = {20.0, 30.0, 180.0, 70.0};
  plain.style.fontSize = 32;
  plain.style.textColor = "ff0000ff";
  editorLayout.addTextBox(plain);

  std::string error;
  if (!graph.exportPagePng(editorLayout, pagePath, outPath, &error)) {
    std::cerr << "Editor-layout export failed: " << error << '\n';
    return 1;
  }
  const QImage composed(QString::fromStdString(outPath.string()));
  if (composed.width() != page.width() || composed.height() != page.height()) {
    std::cerr << "Export changed source page dimensions\n";
    return 1;
  }
  const auto firstTextY = firstNonBackgroundY(composed, background);
  if (firstTextY < 50 || firstTextY > 70) {
    std::cerr << "Export text is not vertically centered in its box: y="
              << firstTextY << '\n';
    return 1;
  }
  const auto textBounds = nonBackgroundBounds(composed, background);
  if (textBounds.left() < 15 || textBounds.left() > 28 ||
      textBounds.width() < 130 || textBounds.height() < 24) {
    std::cerr << "Export text is too small or shifted: bounds="
              << textBounds.x() << ',' << textBounds.y() << ' '
              << textBounds.width() << 'x' << textBounds.height() << '\n';
    return 1;
  }

  TextBox lowercaseBox;
  lowercaseBox.text = "MIXED CASE";
  lowercaseBox.bounds = {20.0, 30.0, 180.0, 70.0};
  lowercaseBox.style.fontSize = 32;
  lowercaseBox.style.textColor = "000000ff";
  lowercaseBox.style.lowercase = true;
  DocumentModel lowercaseDocument;
  lowercaseDocument.addTextBox(lowercaseBox);

  TextBox explicitLowercaseBox = lowercaseBox;
  explicitLowercaseBox.text = "mixed case";
  explicitLowercaseBox.style.lowercase = false;
  DocumentModel explicitLowercaseDocument;
  explicitLowercaseDocument.addTextBox(explicitLowercaseBox);

  const auto lowercasePath = tempPath / "lowercase-export.png";
  const auto explicitLowercasePath = tempPath / "explicit-lowercase-export.png";
  const QImage lowercaseExport =
      exportedImage(graph, lowercaseDocument, pagePath, lowercasePath);
  const QImage explicitLowercaseExport = exportedImage(
      graph, explicitLowercaseDocument, pagePath, explicitLowercasePath);
  if (lowercaseExport.isNull() || explicitLowercaseExport.isNull() ||
      imagesDiffer(lowercaseExport, explicitLowercaseExport)) {
    std::cerr << "Expected lowercase export to render the same pixels as "
                 "explicit lowercase text\n";
    return 1;
  }

  DocumentModel overlappedGlyphs;
  TextBox overlap;
  overlap.text = "XX";
  overlap.bounds = {40.0, 35.0, 140.0, 90.0};
  overlap.style.fontSize = 72;
  overlap.style.textColor = "000000ff";
  overlap.effects.pathEnabled = true;
  overlap.effects.pathPoints = {{0.5, 0.7}, {0.5, 0.7}};
  overlappedGlyphs.addTextBox(overlap);

  const auto overlapImage = exportedImage(graph, overlappedGlyphs, pagePath,
                                          tempPath / "export-overlap.png");
  const int darkOverlapPixels =
      countPixels(overlapImage, background, [](const QColor &color) {
        return color.red() < 40 && color.green() < 40 && color.blue() < 40;
      });
  if (overlapImage.isNull() || darkOverlapPixels < 100) {
    std::cerr << "Overlapped glyphs cut out instead of filling: darkPixels="
              << darkOverlapPixels << '\n';
    return 1;
  }

  DocumentModel equalOutlineLayers;
  TextBox equalLayerBox;
  equalLayerBox.text = "Stack";
  equalLayerBox.bounds = {20.0, 35.0, 200.0, 90.0};
  equalLayerBox.style.fontSize = 46;
  equalLayerBox.style.textColor = "000000ff";
  equalLayerBox.effects.outlineLayers = {{true, "ff0000ff", 7},
                                         {true, "ff0000ff", 7}};
  equalLayerBox.effects.outlineLayersSet = true;
  equalOutlineLayers.addTextBox(equalLayerBox);

  DocumentModel equivalentSingleOutline;
  TextBox equivalentSingleBox = equalLayerBox;
  equivalentSingleBox.effects.outlineLayers.clear();
  equivalentSingleBox.effects.outlineLayersSet = false;
  equivalentSingleBox.effects.outlineEnabled = true;
  equivalentSingleBox.effects.outlineColor = "ff0000ff";
  equivalentSingleBox.effects.outlineSize = 28;
  equivalentSingleOutline.addTextBox(equivalentSingleBox);

  const auto equalLayerImage = exportedImage(
      graph, equalOutlineLayers, pagePath, tempPath / "export-equal-outline-layers.png");
  const auto equivalentSingleImage = exportedImage(
      graph, equivalentSingleOutline, pagePath,
      tempPath / "export-equal-outline-single.png");
  if (equalLayerImage.isNull() || equivalentSingleImage.isNull() ||
      imagesDiffer(equalLayerImage, equivalentSingleImage)) {
    std::cerr << "Equal 7px outline layers did not match a legacy 28px "
                 "centered stroke\n";
    return 1;
  }

  DocumentModel legacyEightOutline;
  TextBox legacyEightBox = equalLayerBox;
  legacyEightBox.effects.outlineLayers.clear();
  legacyEightBox.effects.outlineLayersSet = false;
  legacyEightBox.effects.outlineEnabled = true;
  legacyEightBox.effects.outlineColor = "ff0000ff";
  legacyEightBox.effects.outlineSize = 8;
  legacyEightOutline.addTextBox(legacyEightBox);

  DocumentModel explicitFourOutline;
  TextBox explicitFourBox = legacyEightBox;
  explicitFourBox.effects.outlineLayers = {{true, "ff0000ff", 4}};
  explicitFourBox.effects.outlineLayersSet = true;
  explicitFourOutline.addTextBox(explicitFourBox);

  DocumentModel explicitEightOutline;
  TextBox explicitEightBox = legacyEightBox;
  explicitEightBox.effects.outlineLayers = {{true, "ff0000ff", 8}};
  explicitEightBox.effects.outlineLayersSet = true;
  explicitEightOutline.addTextBox(explicitEightBox);

  const auto legacyEightImage = exportedImage(
      graph, legacyEightOutline, pagePath, tempPath / "export-legacy-outline-8.png");
  const auto explicitFourImage = exportedImage(
      graph, explicitFourOutline, pagePath, tempPath / "export-explicit-outline-4.png");
  const auto explicitEightImage = exportedImage(
      graph, explicitEightOutline, pagePath, tempPath / "export-explicit-outline-8.png");
  if (legacyEightImage.isNull() || explicitFourImage.isNull() ||
      explicitEightImage.isNull() || imagesDiffer(legacyEightImage, explicitFourImage) ||
      !imagesDiffer(legacyEightImage, explicitEightImage)) {
    std::cerr << "Legacy outline_size 8 did not keep old 8px stroke semantics "
                 "distinct from explicit outline layer size 8\n";
    return 1;
  }

  DocumentModel legacyOddOutline;
  TextBox legacyOddBox = legacyEightBox;
  legacyOddBox.effects.outlineSize = 7;
  legacyOddOutline.addTextBox(legacyOddBox);

  const auto legacyOddImage = exportedImage(
      graph, legacyOddOutline, pagePath, tempPath / "export-legacy-outline-7.png");
  if (legacyOddImage.isNull() || !imagesDiffer(legacyOddImage, legacyEightImage) ||
      !imagesDiffer(legacyOddImage, explicitFourImage)) {
    std::cerr << "Legacy outline_size 7 did not keep old 7px centered stroke "
                  "semantics distinct from rounded explicit layer size 4\n";
    return 1;
  }

  DocumentModel stackedOutlines;
  TextBox stackedOutlineBox;
  stackedOutlineBox.text = "Stack";
  stackedOutlineBox.bounds = {20.0, 35.0, 200.0, 90.0};
  stackedOutlineBox.style.fontSize = 46;
  stackedOutlineBox.style.textColor = "000000ff";
  stackedOutlineBox.effects.outlineLayers = {{true, "ffffffff", 11},
                                             {true, "ff0000ff", 9}};
  stackedOutlineBox.effects.outlineLayersSet = true;
  stackedOutlines.addTextBox(stackedOutlineBox);
  const auto stackedOutlineImage = exportedImage(
      graph, stackedOutlines, pagePath, tempPath / "export-stacked-outline.png");
  const int stackedRedPixels =
      countPixels(stackedOutlineImage, background, [](const QColor &color) {
        return color.red() > 120 && color.green() < 80 && color.blue() < 80;
      });
  const int stackedWhitePixels =
      countPixels(stackedOutlineImage, background, [](const QColor &color) {
        return color.red() > 180 && color.green() > 180 && color.blue() > 180;
      });
  if (stackedOutlineImage.isNull() || stackedRedPixels < 300 ||
      stackedWhitePixels == 0) {
    std::cerr << "Stacked outline export did not preserve reversed-size layer order: red="
              << stackedRedPixels << " white=" << stackedWhitePixels << '\n';
    return 1;
  }

  DocumentModel unblurred;
  TextBox blurBox;
  blurBox.text = "Blur";
  blurBox.bounds = {45.0, 45.0, 140.0, 70.0};
  blurBox.style.fontSize = 42;
  blurBox.style.textColor = "000000ff";
  unblurred.addTextBox(blurBox);

  DocumentModel blurred;
  blurBox.effects.blurEnabled = true;
  blurBox.effects.blurSize = 6;
  blurred.addTextBox(blurBox);

  const auto unblurredImage = exportedImage(graph, unblurred, pagePath,
                                            tempPath / "export-blur-off.png");
  const auto blurredImage =
      exportedImage(graph, blurred, pagePath, tempPath / "export-blur-on.png");
  const auto unblurredBounds = nonBackgroundBounds(unblurredImage, background);
  const auto blurredBounds = nonBackgroundBounds(blurredImage, background);
  const int hardUnblurredPixels =
      countPixels(unblurredImage, background, [](const QColor &color) {
        return color.red() < 40 && color.green() < 40 && color.blue() < 40;
      });
  const int hardBlurredPixels =
      countPixels(blurredImage, background, [](const QColor &color) {
        return color.red() < 40 && color.green() < 40 && color.blue() < 40;
      });
  const int softBlurredPixels =
      countPixels(blurredImage, background, [](const QColor &color) {
        return color.red() > 40 && color.red() < 220 && color.green() > 40 &&
               color.green() < 220 && color.blue() > 40 && color.blue() < 220;
      });
  if (blurredImage.isNull() || !imagesDiffer(unblurredImage, blurredImage) ||
      blurredBounds.width() <= unblurredBounds.width() ||
      blurredBounds.height() <= unblurredBounds.height() ||
      hardBlurredPixels >= hardUnblurredPixels || softBlurredPixels < 200) {
    std::cerr << "Blur is not a soft post-process filter: hard="
              << hardBlurredPixels << '/' << hardUnblurredPixels
              << " soft=" << softBlurredPixels
              << " bounds=" << blurredBounds.width() << 'x'
              << blurredBounds.height() << " vs " << unblurredBounds.width()
              << 'x' << unblurredBounds.height() << '\n';
    return 1;
  }

  DocumentModel shadowSharp;
  TextBox shadowBox;
  shadowBox.text = "Shadow";
  shadowBox.bounds = {35.0, 40.0, 170.0, 75.0};
  shadowBox.style.fontSize = 40;
  shadowBox.style.textColor = "00000000";
  shadowBox.effects.shadowEnabled = true;
  shadowBox.effects.shadowColor = "000000ff";
  shadowBox.effects.shadowOffsetX = 6;
  shadowBox.effects.shadowOffsetY = 5;
  shadowSharp.addTextBox(shadowBox);

  DocumentModel shadowBlurred;
  shadowBox.effects.shadowBlurSize = 6;
  shadowBlurred.addTextBox(shadowBox);

  const auto shadowSharpImage = exportedImage(
      graph, shadowSharp, pagePath, tempPath / "export-shadow-sharp.png");
  const auto shadowBlurredImage = exportedImage(
      graph, shadowBlurred, pagePath, tempPath / "export-shadow-blur.png");
  const auto shadowSharpBounds =
      nonBackgroundBounds(shadowSharpImage, background);
  const auto shadowBlurredBounds =
      nonBackgroundBounds(shadowBlurredImage, background);
  const int hardShadowSharpPixels =
      countPixels(shadowSharpImage, background, [](const QColor &color) {
        return color.red() < 40 && color.green() < 40 && color.blue() < 40;
      });
  const int hardShadowBlurredPixels =
      countPixels(shadowBlurredImage, background, [](const QColor &color) {
        return color.red() < 40 && color.green() < 40 && color.blue() < 40;
      });
  const int softShadowBlurredPixels =
      countPixels(shadowBlurredImage, background, [](const QColor &color) {
        return color.red() > 40 && color.red() < 220 && color.green() > 40 &&
               color.green() < 220 && color.blue() > 40 && color.blue() < 220;
      });
  if (shadowBlurredImage.isNull() ||
      !imagesDiffer(shadowSharpImage, shadowBlurredImage) ||
      shadowBlurredBounds.width() <= shadowSharpBounds.width() ||
      shadowBlurredBounds.height() <= shadowSharpBounds.height() ||
      hardShadowBlurredPixels >= hardShadowSharpPixels ||
      softShadowBlurredPixels < 200) {
    std::cerr << "Shadow blur is not a soft Gaussian filter: hard="
              << hardShadowBlurredPixels << '/' << hardShadowSharpPixels
              << " soft=" << softShadowBlurredPixels
              << " bounds=" << shadowBlurredBounds.width() << 'x'
              << shadowBlurredBounds.height() << " vs "
              << shadowSharpBounds.width() << 'x' << shadowSharpBounds.height()
              << '\n';
    return 1;
  }

  DocumentModel edgeBlur;
  TextBox edgeBox;
  edgeBox.text = "W";
  edgeBox.bounds = {80.0, 60.0, 48.0, 48.0};
  edgeBox.style.fontSize = 60;
  edgeBox.style.textColor = "000000ff";
  edgeBox.effects.blurEnabled = true;
  edgeBox.effects.blurSize = 8;
  edgeBlur.addTextBox(edgeBox);
  const auto edgeBlurImage = exportedImage(
      graph, edgeBlur, pagePath, tempPath / "export-blur-clipped.png");
  const QRect edgeRect(
      static_cast<int>(edgeBox.bounds.x), static_cast<int>(edgeBox.bounds.y),
      static_cast<int>(edgeBox.bounds.w), static_cast<int>(edgeBox.bounds.h));
  int edgeBleedPixels = 0;
  for (int y = 0; y < edgeBlurImage.height(); ++y) {
    for (int x = 0; x < edgeBlurImage.width(); ++x) {
      if (!edgeRect.contains(x, y) &&
          edgeBlurImage.pixelColor(x, y) != background)
        ++edgeBleedPixels;
    }
  }
  if (edgeBlurImage.isNull() || edgeBleedPixels != 0) {
    std::cerr << "Blur export bled outside the box: pixels=" << edgeBleedPixels
              << '\n';
    return 1;
  }

  DocumentModel edgeShadow;
  TextBox edgeShadowBox;
  edgeShadowBox.text = "W";
  edgeShadowBox.bounds = {80.0, 60.0, 48.0, 48.0};
  edgeShadowBox.style.fontSize = 60;
  edgeShadowBox.style.textColor = "00000000";
  edgeShadowBox.effects.shadowEnabled = true;
  edgeShadowBox.effects.shadowColor = "000000ff";
  edgeShadowBox.effects.shadowOffsetX = 12;
  edgeShadowBox.effects.shadowOffsetY = 8;
  edgeShadowBox.effects.shadowBlurSize = 8;
  edgeShadow.addTextBox(edgeShadowBox);
  const auto edgeShadowImage = exportedImage(
      graph, edgeShadow, pagePath, tempPath / "export-shadow-clipped.png");
  const QRect edgeShadowRect(static_cast<int>(edgeShadowBox.bounds.x),
                             static_cast<int>(edgeShadowBox.bounds.y),
                             static_cast<int>(edgeShadowBox.bounds.w),
                             static_cast<int>(edgeShadowBox.bounds.h));
  int edgeShadowBleedPixels = 0;
  int edgeShadowInsidePixels = 0;
  for (int y = 0; y < edgeShadowImage.height(); ++y) {
    for (int x = 0; x < edgeShadowImage.width(); ++x) {
      if (edgeShadowImage.pixelColor(x, y) == background)
        continue;
      if (edgeShadowRect.contains(x, y))
        ++edgeShadowInsidePixels;
      else
        ++edgeShadowBleedPixels;
    }
  }
  if (edgeShadowImage.isNull() || edgeShadowBleedPixels != 0 ||
      edgeShadowInsidePixels < 50) {
    std::cerr << "Shadow export bled outside the box or disappeared: bleed="
              << edgeShadowBleedPixels << " inside=" << edgeShadowInsidePixels
              << '\n';
    return 1;
  }

  DocumentModel rotatedBlur;
  TextBox rotatedFx;
  rotatedFx.text = "Risky Blur";
  rotatedFx.bounds = {35.0, 45.0, 150.0, 55.0};
  rotatedFx.rotationDegrees = 12.0;
  rotatedFx.style.fontSize = 28;
  rotatedFx.style.textColor = "101010ff";
  rotatedFx.effects.outlineEnabled = true;
  rotatedFx.effects.outlineColor = "ffffffff";
  rotatedFx.effects.outlineSize = 2;
  rotatedFx.effects.blurEnabled = true;
  rotatedFx.effects.blurSize = 5;
  rotatedFx.effects.shadowEnabled = true;
  rotatedFx.effects.shadowColor = "000000aa";
  rotatedFx.effects.shadowOffsetX = 6;
  rotatedFx.effects.shadowOffsetY = 5;
  rotatedFx.effects.shadowBlurSize = 3;
  rotatedBlur.addTextBox(rotatedFx);

  const auto rotatedBlurImage = exportedImage(
      graph, rotatedBlur, pagePath, tempPath / "export-rotated-blur.png");
  const auto rotatedBlurBounds =
      nonBackgroundBounds(rotatedBlurImage, background);
  const int rotatedSoftPixels =
      countPixels(rotatedBlurImage, background, [](const QColor &color) {
        return color.red() > 40 && color.red() < 220 && color.green() > 40 &&
               color.green() < 220 && color.blue() > 40 && color.blue() < 220;
      });
  if (rotatedBlurImage.isNull() || !imagesDiffer(page, rotatedBlurImage) ||
      rotatedBlurBounds.isEmpty() || rotatedBlurBounds.left() < 20 ||
      rotatedBlurBounds.top() < 25 || rotatedBlurBounds.right() > 205 ||
      rotatedBlurBounds.bottom() > 130 || rotatedBlurBounds.width() < 85 ||
      rotatedBlurBounds.height() < 35 || rotatedSoftPixels < 150) {
    std::cerr << "Rotated combined blur export looks cropped or over-bleeding: "
                 "bounds="
              << rotatedBlurBounds.x() << ',' << rotatedBlurBounds.y() << ' '
              << rotatedBlurBounds.width() << 'x' << rotatedBlurBounds.height()
              << " soft=" << rotatedSoftPixels << '\n';
    return 1;
  }

  DocumentModel allEffects;
  TextBox fx;
  fx.text = "AllEffectsAllEffects";
  fx.bounds = {20.0, 30.0, 180.0, 70.0};
  fx.style.fontSize = 24;
  fx.style.textColor = "2030ffff";
  fx.effects.outlineEnabled = true;
  fx.effects.outlineColor = "ffffffff";
  fx.effects.outlineSize = 2;
  fx.effects.blurEnabled = true;
  fx.effects.blurSize = 1;
  fx.effects.shadowEnabled = true;
  fx.effects.shadowColor = "000000aa";
  fx.effects.shadowOffsetX = 5;
  fx.effects.shadowOffsetY = 6;
  fx.effects.shadowBlurSize = 3;
  fx.effects.gradientEnabled = true;
  fx.effects.gradientDirection = 1;
  fx.effects.gradientColorA = "ff0000ff";
  fx.effects.gradientColorB = "0000ffff";
  fx.effects.perspectiveEnabled = true;
  fx.effects.perspectiveNw = {0.0, 0.0};
  fx.effects.perspectiveNe = {0.15, 0.0};
  fx.effects.perspectiveSe = {0.0, 0.12};
  fx.effects.perspectiveSw = {-0.10, 0.0};
  fx.effects.pathEnabled = true;
  fx.effects.pathPoints = {{0.0, 0.90}, {1.0, 0.80}};
  allEffects.addTextBox(fx);

  if (!graph.exportPagePng(allEffects, pagePath, outPath, &error) ||
      !hasPngMagic(outPath) || std::filesystem::file_size(outPath) <= 8) {
    std::cerr << "All-effects PNG export failed: " << error << '\n';
    return 1;
  }
  const QImage fxImage(QString::fromStdString(outPath.string()));
  const auto fxBounds = nonBackgroundBounds(fxImage, background);
  if (fxBounds.top() < 35 || fxBounds.height() < 20) {
    std::cerr << "Path/perspective effects were dropped: bounds="
              << fxBounds.x() << ',' << fxBounds.y() << ' ' << fxBounds.width()
              << 'x' << fxBounds.height() << '\n';
    return 1;
  }
  const int redPixels =
      countPixels(fxImage, background, [](const QColor &color) {
        return color.red() > color.blue() + 40;
      });
  const int bluePixels =
      countPixels(fxImage, background, [](const QColor &color) {
        return color.blue() > color.red() + 40;
      });
  if (redPixels == 0 || bluePixels == 0) {
    std::cerr << "Gradient effect was dropped: red=" << redPixels
              << " blue=" << bluePixels << '\n';
    return 1;
  }
  const auto warnings = graph.warnings(allEffects);
  if (warnings.size() < 2) {
    std::cerr << "All-effects approximation warnings were not reported\n";
    return 1;
  }

  DocumentModel straight;
  TextBox pathBox;
  pathBox.text = "PathModeAffectsOutput";
  pathBox.bounds = {20.0, 30.0, 180.0, 70.0};
  pathBox.style.fontSize = 16;
  pathBox.effects.pathEnabled = true;
  pathBox.effects.pathMode = 0;
  pathBox.effects.pathPoints = {{0.0, 0.9}, {0.5, 0.1}, {1.0, 0.9}};
  straight.addTextBox(pathBox);

  DocumentModel smooth;
  pathBox.effects.pathMode = 1;
  smooth.addTextBox(pathBox);

  const auto straightImage = exportedImage(
      graph, straight, pagePath, tempPath / "export-path-straight.png");
  const auto smoothImage = exportedImage(graph, smooth, pagePath,
                                         tempPath / "export-path-smooth.png");
  if (straightImage.isNull() || smoothImage.isNull() ||
      !imagesDiffer(straightImage, smoothImage)) {
    std::cerr << "Path mode did not affect PNG export\n";
    return 1;
  }

  DocumentModel defaultFlatOff;
  TextBox defaultFlatBox;
  defaultFlatBox.text = "HELLO WEAVER!";
  defaultFlatBox.bounds = {0.0, 20.0, 150.0, 120.0};
  defaultFlatBox.style.fontSize = 34;
  defaultFlatBox.style.letterSpacing = 1;
  defaultFlatBox.style.alignment = TextAlignment::Center;
  defaultFlatBox.style.textColor = "000000ff";
  defaultFlatOff.addTextBox(defaultFlatBox);

  DocumentModel defaultFlatOn;
  defaultFlatBox.effects.pathEnabled = true;
  defaultFlatBox.effects.pathPoints = {{0.0, 0.5}, {0.5, 0.5}, {1.0, 0.5}};
  defaultFlatOn.addTextBox(defaultFlatBox);

  const auto defaultFlatOffImage =
      exportedImage(graph, defaultFlatOff, pagePath,
                    tempPath / "export-default-path-off.png");
  const auto defaultFlatOnImage = exportedImage(
      graph, defaultFlatOn, pagePath, tempPath / "export-default-path-on.png");
  const auto defaultFlatOffBounds =
      nonBackgroundBounds(defaultFlatOffImage, background);
  const auto defaultFlatOnBounds =
      nonBackgroundBounds(defaultFlatOnImage, background);
  const int defaultFlatDiff =
      imageDifferenceCount(defaultFlatOffImage, defaultFlatOnImage);
  if (defaultFlatOffImage.isNull() || defaultFlatOnImage.isNull() ||
      defaultFlatOffBounds.isEmpty() || defaultFlatOnBounds.isEmpty() ||
      defaultFlatDiff != 0 ||
      std::abs(defaultFlatOffBounds.left() - defaultFlatOnBounds.left()) > 3 ||
      std::abs(defaultFlatOffBounds.top() - defaultFlatOnBounds.top()) > 3 ||
      std::abs(defaultFlatOffBounds.width() - defaultFlatOnBounds.width()) >
          6 ||
      std::abs(defaultFlatOffBounds.height() - defaultFlatOnBounds.height()) >
          6) {
    std::cerr << "Default flat path changed rendered pixels: diff="
              << defaultFlatDiff << " off=" << defaultFlatOffBounds.x() << ','
              << defaultFlatOffBounds.y() << ' ' << defaultFlatOffBounds.width()
              << 'x' << defaultFlatOffBounds.height()
              << " on=" << defaultFlatOnBounds.x() << ','
              << defaultFlatOnBounds.y() << ' ' << defaultFlatOnBounds.width()
              << 'x' << defaultFlatOnBounds.height() << '\n';
    return 1;
  }

  DocumentModel shortPathBaseline;
  TextBox pathBaselineBox;
  pathBaselineBox.text = "BASELINE";
  pathBaselineBox.bounds = {0.0, 20.0, 240.0, 80.0};
  pathBaselineBox.style.fontSize = 30;
  pathBaselineBox.style.textColor = "000000ff";
  pathBaselineBox.effects.pathEnabled = true;
  pathBaselineBox.effects.pathPoints = {{0.0, 0.75}, {1.0, 0.75}};
  shortPathBaseline.addTextBox(pathBaselineBox);
  DocumentModel tallPathBaseline;
  pathBaselineBox.bounds.h = 120.0;
  tallPathBaseline.addTextBox(pathBaselineBox);
  const auto shortPathBaselineImage =
      exportedImage(graph, shortPathBaseline, pagePath,
                    tempPath / "export-short-path-baseline.png");
  const auto tallPathBaselineImage =
      exportedImage(graph, tallPathBaseline, pagePath,
                    tempPath / "export-tall-path-baseline.png");
  const auto shortPathBaselineBounds =
      nonBackgroundBounds(shortPathBaselineImage, background);
  const auto tallPathBaselineBounds =
      nonBackgroundBounds(tallPathBaselineImage, background);
  const int shortPathBaselineY =
      static_cast<int>(pathBaselineBox.bounds.y + 80.0 * 0.75);
  if (shortPathBaselineImage.isNull() || tallPathBaselineImage.isNull() ||
      shortPathBaselineBounds.isEmpty() || tallPathBaselineBounds.isEmpty() ||
      tallPathBaselineBounds.top() - shortPathBaselineBounds.top() < 24) {
    std::cerr << "Straight path text did not follow absolute path y: short="
              << shortPathBaselineBounds.x() << ','
              << shortPathBaselineBounds.y() << ' '
              << shortPathBaselineBounds.width() << 'x'
              << shortPathBaselineBounds.height()
              << " tall=" << tallPathBaselineBounds.x() << ','
              << tallPathBaselineBounds.y() << ' '
              << tallPathBaselineBounds.width() << 'x'
              << tallPathBaselineBounds.height() << '\n';
    return 1;
  }
  if (std::abs(shortPathBaselineBounds.bottom() - shortPathBaselineY) > 4) {
    std::cerr << "Straight path text is not on the guide baseline: guideY="
              << shortPathBaselineY << " bounds=" << shortPathBaselineBounds.x()
              << ',' << shortPathBaselineBounds.y() << ' '
              << shortPathBaselineBounds.width() << 'x'
              << shortPathBaselineBounds.height() << '\n';
    return 1;
  }

  DocumentModel topPathBaseline;
  TextBox topPathBox;
  topPathBox.text = "HELLO";
  topPathBox.bounds = {0.0, 20.0, 240.0, 80.0};
  topPathBox.style.fontSize = 30;
  topPathBox.style.textColor = "000000ff";
  topPathBox.effects.pathEnabled = true;
  topPathBox.effects.pathPoints = {{0.0, 0.05}, {1.0, 0.05}};
  topPathBaseline.addTextBox(topPathBox);
  const auto topPathBaselineImage =
      exportedImage(graph, topPathBaseline, pagePath,
                    tempPath / "export-top-path-baseline.png");
  const auto topPathBaselineBounds =
      nonBackgroundBounds(topPathBaselineImage, background);
  const int expectedPathY =
      static_cast<int>(topPathBox.bounds.y + topPathBox.bounds.h * 0.05);
  if (topPathBaselineImage.isNull() || topPathBaselineBounds.isEmpty() ||
      topPathBaselineBounds.bottom() > expectedPathY + 6) {
    std::cerr << "Straight path text detached below guide: guideY="
              << expectedPathY << " bounds=" << topPathBaselineBounds.x() << ','
              << topPathBaselineBounds.y() << ' '
              << topPathBaselineBounds.width() << 'x'
              << topPathBaselineBounds.height() << '\n';
    return 1;
  }

  DocumentModel exportFlatPath;
  TextBox flatPathBox;
  flatPathBox.text = "HELLO\nWEAVER!";
  flatPathBox.bounds = {0.0, 20.0, 240.0, 120.0};
  flatPathBox.style.fontSize = 34;
  flatPathBox.style.letterSpacing = 1;
  flatPathBox.style.textColor = "000000ff";
  flatPathBox.effects.pathEnabled = true;
  flatPathBox.effects.pathPoints = {{0.0, 0.65}, {1.0, 0.65}};
  exportFlatPath.addTextBox(flatPathBox);
  const auto flatPathImage =
      exportedImage(graph, exportFlatPath, pagePath,
                    tempPath / "export-flat-path-advance.png");
  const auto flatPathBounds = nonBackgroundBounds(flatPathImage, background);
  if (flatPathImage.isNull() || flatPathBounds.height() <= 50 ||
      flatPathBounds.width() < 120 || flatPathBounds.width() >= 240) {
    std::cerr << "Path text export collapsed multiline layout: bounds="
              << flatPathBounds.x() << ',' << flatPathBounds.y() << ' '
              << flatPathBounds.width() << 'x' << flatPathBounds.height()
              << '\n';
    return 1;
  }

  flatPathBox.text = "HELLO WEAVER!";
  flatPathBox.bounds = {0.0, 20.0, 150.0, 120.0};
  DocumentModel exportWrappedFlatPath;
  exportWrappedFlatPath.addTextBox(flatPathBox);
  const auto wrappedFlatPathImage =
      exportedImage(graph, exportWrappedFlatPath, pagePath,
                    tempPath / "export-wrapped-flat-path.png");
  const auto wrappedFlatPathBounds =
      nonBackgroundBounds(wrappedFlatPathImage, background);
  if (wrappedFlatPathImage.isNull() || wrappedFlatPathBounds.height() <= 50 ||
      wrappedFlatPathBounds.width() >= 150) {
    std::cerr << "Path text export ignored visual wrapping: bounds="
              << wrappedFlatPathBounds.x() << ',' << wrappedFlatPathBounds.y()
              << ' ' << wrappedFlatPathBounds.width() << 'x'
              << wrappedFlatPathBounds.height() << '\n';
    return 1;
  }

  DocumentModel exportUnevenFlatPath;
  flatPathBox.text = "EVEN SPACING";
  flatPathBox.bounds = {0.0, 20.0, 240.0, 120.0};
  flatPathBox.style.fontSize = 30;
  flatPathBox.style.letterSpacing = 0.0;
  flatPathBox.effects.pathPoints = {{0.0, 0.65}, {1.0, 0.65}};
  DocumentModel exportEvenTwoPointPath;
  exportEvenTwoPointPath.addTextBox(flatPathBox);
  flatPathBox.effects.pathPoints = {{0.0, 0.65}, {0.1, 0.65}, {1.0, 0.65}};
  exportUnevenFlatPath.addTextBox(flatPathBox);
  const auto evenTwoPointImage =
      exportedImage(graph, exportEvenTwoPointPath, pagePath,
                    tempPath / "export-even-two-point-path.png");
  const auto unevenFlatImage =
      exportedImage(graph, exportUnevenFlatPath, pagePath,
                    tempPath / "export-uneven-flat-path.png");
  const int unevenFlatDiff =
      imageDifferenceCount(evenTwoPointImage, unevenFlatImage);
  if (evenTwoPointImage.isNull() || unevenFlatImage.isNull() ||
      unevenFlatDiff > 100) {
    std::cerr << "Uneven collinear path changed glyph spacing: diffPixels="
              << unevenFlatDiff << '\n';
    return 1;
  }

  flatPathBox.effects.pathPoints = {{0.0, 0.65}, {1.0, 0.65}};
  DocumentModel exportFlatSpacingPath;
  exportFlatSpacingPath.addTextBox(flatPathBox);
  flatPathBox.effects.pathPoints = {{0.0, 0.65}, {0.1, 0.20}, {1.0, 0.65}};
  DocumentModel exportCurvedSpacingPath;
  exportCurvedSpacingPath.addTextBox(flatPathBox);
  const auto flatSpacingImage =
      exportedImage(graph, exportFlatSpacingPath, pagePath,
                    tempPath / "export-flat-spacing-path.png");
  const auto curvedSpacingImage =
      exportedImage(graph, exportCurvedSpacingPath, pagePath,
                    tempPath / "export-curved-spacing-path.png");
  const auto flatSpacingBounds =
      nonBackgroundBounds(flatSpacingImage, background);
  const auto curvedSpacingBounds =
      nonBackgroundBounds(curvedSpacingImage, background);
  if (flatSpacingImage.isNull() || curvedSpacingImage.isNull() ||
      flatSpacingBounds.isEmpty() || curvedSpacingBounds.isEmpty()) {
    std::cerr << "Curved path changed horizontal glyph spacing: flat="
              << flatSpacingBounds.x() << ',' << flatSpacingBounds.y() << ' '
              << flatSpacingBounds.width() << 'x' << flatSpacingBounds.height()
              << " curved=" << curvedSpacingBounds.x() << ','
              << curvedSpacingBounds.y() << ' ' << curvedSpacingBounds.width()
              << 'x' << curvedSpacingBounds.height() << '\n';
    return 1;
  }

  DocumentModel exportLayoutAngledPath;
  TextBox angledPathBox;
  angledPathBox.text = "ANGLE";
  angledPathBox.bounds = {0.0, 0.0, 240.0, 120.0};
  angledPathBox.style.fontSize = 34;
  angledPathBox.style.textColor = "000000ff";
  angledPathBox.effects.pathEnabled = true;
  angledPathBox.effects.pathPoints = {{0.0, 0.0}, {1.0, 1.0}};
  exportLayoutAngledPath.addTextBox(angledPathBox);
  const auto layoutAngledImage =
      exportedImage(graph, exportLayoutAngledPath, pagePath,
                    tempPath / "export-layout-angled-path.png");
  const auto layoutAngledBounds =
      nonBackgroundBounds(layoutAngledImage, background);
  if (layoutAngledImage.isNull() ||
      layoutAngledBounds.width() <= layoutAngledBounds.height()) {
    std::cerr << "Path text export did not use layout-scaled angle: bounds="
              << layoutAngledBounds.width() << 'x'
              << layoutAngledBounds.height() << '\n';
    return 1;
  }

  DocumentModel unwarped;
  TextBox perspectiveBox;
  perspectiveBox.text = "Perspective";
  perspectiveBox.bounds = {20.0, 30.0, 180.0, 70.0};
  perspectiveBox.style.fontSize = 24;
  unwarped.addTextBox(perspectiveBox);

  DocumentModel warped;
  perspectiveBox.effects.perspectiveEnabled = true;
  perspectiveBox.effects.perspectiveNw = {20.0, 8.0};
  perspectiveBox.effects.perspectiveNe = {24.0, 0.0};
  perspectiveBox.effects.perspectiveSw = {10.0, -6.0};
  perspectiveBox.effects.perspectiveSe = {12.0, 10.0};
  warped.addTextBox(perspectiveBox);

  const auto unwarpedImage = exportedImage(
      graph, unwarped, pagePath, tempPath / "export-perspective-off.png");
  const auto warpedImage = exportedImage(
      graph, warped, pagePath, tempPath / "export-perspective-on.png");
  if (unwarpedImage.isNull() || warpedImage.isNull() ||
      !imagesDiffer(unwarpedImage, warpedImage)) {
    std::cerr << "Perspective offsets did not affect PNG export\n";
    return 1;
  }

  return 0;
}
