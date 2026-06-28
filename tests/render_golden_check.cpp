#include "render/RenderGraph.h"

#include <filesystem>
#include <fstream>
#include <iostream>

#include <QColor>
#include <QGuiApplication>
#include <QImage>
#include <QRect>

using namespace textfx;

namespace {
bool hasPngMagic(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    char magic[8]{};
    input.read(magic, 8);
    return input.gcount() == 8 && std::string(magic, 8) == std::string("\x89PNG\r\n\x1a\n", 8);
}

QImage exportedImage(const RenderGraph& graph, const DocumentModel& document, const std::filesystem::path& pagePath, const std::filesystem::path& outPath)
{
    std::string error;
    if (!graph.exportPagePng(document, pagePath, outPath, &error)) {
        std::cerr << "PNG export failed: " << error << '\n';
        return {};
    }
    return QImage(QString::fromStdString(outPath.string()));
}

bool imagesDiffer(const QImage& a, const QImage& b)
{
    if (a.size() != b.size()) return true;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            if (a.pixelColor(x, y) != b.pixelColor(x, y)) return true;
        }
    }
    return false;
}

int firstNonBackgroundY(const QImage& image, const QColor& background)
{
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y) != background) return y;
        }
    }
    return -1;
}

QRect nonBackgroundBounds(const QImage& image, const QColor& background)
{
    QRect bounds;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            if (image.pixelColor(x, y) != background) bounds = bounds.isNull() ? QRect(x, y, 1, 1) : bounds.united(QRect(x, y, 1, 1));
        }
    }
    return bounds;
}

int countPixels(const QImage& image, const QColor& background, const auto& predicate)
{
    int count = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const auto color = image.pixelColor(x, y);
            if (color != background && predicate(color)) ++count;
        }
    }
    return count;
}
} // namespace

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);

    const RenderGraph graph;
    QImage page(240, 160, QImage::Format_RGBA8888);
    const QColor background(230, 230, 230, 255);
    page.fill(background);
    const auto pagePath = std::filesystem::temp_directory_path() / "textfx-export-page.png";
    const auto outPath = std::filesystem::temp_directory_path() / "textfx-export.png";
    if (!page.save(QString::fromStdString(pagePath.string()), "PNG")) return 1;

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
    if (firstTextY < 30 || firstTextY > 50) {
        std::cerr << "Export text does not match editor top-aligned document layout: y=" << firstTextY << '\n';
        return 1;
    }
    const auto textBounds = nonBackgroundBounds(composed, background);
    if (textBounds.left() < 15 || textBounds.left() > 28 || textBounds.width() < 130 || textBounds.height() < 24) {
        std::cerr << "Export text is too small or shifted: bounds=" << textBounds.x() << ',' << textBounds.y() << ' '
                  << textBounds.width() << 'x' << textBounds.height() << '\n';
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

    const auto overlapImage = exportedImage(graph, overlappedGlyphs, pagePath, std::filesystem::temp_directory_path() / "textfx-export-overlap.png");
    const int darkOverlapPixels = countPixels(overlapImage, background, [](const QColor& color) { return color.red() < 40 && color.green() < 40 && color.blue() < 40; });
    if (overlapImage.isNull() || darkOverlapPixels < 100) {
        std::cerr << "Overlapped glyphs cut out instead of filling: darkPixels=" << darkOverlapPixels << '\n';
        return 1;
    }

    DocumentModel allEffects;
    TextBox fx;
    fx.text = "AllEffects";
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

    if (!graph.exportPagePng(allEffects, pagePath, outPath, &error) || !hasPngMagic(outPath) || std::filesystem::file_size(outPath) <= 8) {
        std::cerr << "All-effects PNG export failed: " << error << '\n';
        return 1;
    }
    const QImage fxImage(QString::fromStdString(outPath.string()));
    const auto fxBounds = nonBackgroundBounds(fxImage, background);
    if (fxBounds.top() < 50 || fxBounds.height() < 20) {
        std::cerr << "Path/perspective effects were dropped: bounds=" << fxBounds.x() << ',' << fxBounds.y() << ' '
                  << fxBounds.width() << 'x' << fxBounds.height() << '\n';
        return 1;
    }
    const int redPixels = countPixels(fxImage, background, [](const QColor& color) { return color.red() > color.blue() + 40; });
    const int bluePixels = countPixels(fxImage, background, [](const QColor& color) { return color.blue() > color.red() + 40; });
    if (redPixels == 0 || bluePixels == 0) {
        std::cerr << "Gradient effect was dropped: red=" << redPixels << " blue=" << bluePixels << '\n';
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

    const auto straightImage = exportedImage(graph, straight, pagePath, std::filesystem::temp_directory_path() / "textfx-export-path-straight.png");
    const auto smoothImage = exportedImage(graph, smooth, pagePath, std::filesystem::temp_directory_path() / "textfx-export-path-smooth.png");
    if (straightImage.isNull() || smoothImage.isNull() || !imagesDiffer(straightImage, smoothImage)) {
        std::cerr << "Path mode did not affect PNG export\n";
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

    const auto unwarpedImage = exportedImage(graph, unwarped, pagePath, std::filesystem::temp_directory_path() / "textfx-export-perspective-off.png");
    const auto warpedImage = exportedImage(graph, warped, pagePath, std::filesystem::temp_directory_path() / "textfx-export-perspective-on.png");
    if (unwarpedImage.isNull() || warpedImage.isNull() || !imagesDiffer(unwarpedImage, warpedImage)) {
        std::cerr << "Perspective offsets did not affect PNG export\n";
        return 1;
    }

    return 0;
}
