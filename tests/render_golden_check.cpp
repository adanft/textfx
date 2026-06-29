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

    const auto unblurredImage = exportedImage(graph, unblurred, pagePath, std::filesystem::temp_directory_path() / "textfx-export-blur-off.png");
    const auto blurredImage = exportedImage(graph, blurred, pagePath, std::filesystem::temp_directory_path() / "textfx-export-blur-on.png");
    const auto unblurredBounds = nonBackgroundBounds(unblurredImage, background);
    const auto blurredBounds = nonBackgroundBounds(blurredImage, background);
    const int hardUnblurredPixels = countPixels(unblurredImage, background, [](const QColor& color) { return color.red() < 40 && color.green() < 40 && color.blue() < 40; });
    const int hardBlurredPixels = countPixels(blurredImage, background, [](const QColor& color) { return color.red() < 40 && color.green() < 40 && color.blue() < 40; });
    const int softBlurredPixels = countPixels(blurredImage, background, [](const QColor& color) { return color.red() > 40 && color.red() < 220 && color.green() > 40 && color.green() < 220 && color.blue() > 40 && color.blue() < 220; });
    if (blurredImage.isNull() || !imagesDiffer(unblurredImage, blurredImage) || blurredBounds.width() <= unblurredBounds.width() || blurredBounds.height() <= unblurredBounds.height() || hardBlurredPixels >= hardUnblurredPixels || softBlurredPixels < 200) {
        std::cerr << "Blur is not a soft post-process filter: hard=" << hardBlurredPixels << '/' << hardUnblurredPixels
                  << " soft=" << softBlurredPixels << " bounds=" << blurredBounds.width() << 'x' << blurredBounds.height()
                  << " vs " << unblurredBounds.width() << 'x' << unblurredBounds.height() << '\n';
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

    const auto shadowSharpImage = exportedImage(graph, shadowSharp, pagePath, std::filesystem::temp_directory_path() / "textfx-export-shadow-sharp.png");
    const auto shadowBlurredImage = exportedImage(graph, shadowBlurred, pagePath, std::filesystem::temp_directory_path() / "textfx-export-shadow-blur.png");
    const auto shadowSharpBounds = nonBackgroundBounds(shadowSharpImage, background);
    const auto shadowBlurredBounds = nonBackgroundBounds(shadowBlurredImage, background);
    const int hardShadowSharpPixels = countPixels(shadowSharpImage, background, [](const QColor& color) { return color.red() < 40 && color.green() < 40 && color.blue() < 40; });
    const int hardShadowBlurredPixels = countPixels(shadowBlurredImage, background, [](const QColor& color) { return color.red() < 40 && color.green() < 40 && color.blue() < 40; });
    const int softShadowBlurredPixels = countPixels(shadowBlurredImage, background, [](const QColor& color) { return color.red() > 40 && color.red() < 220 && color.green() > 40 && color.green() < 220 && color.blue() > 40 && color.blue() < 220; });
    if (shadowBlurredImage.isNull() || !imagesDiffer(shadowSharpImage, shadowBlurredImage) || shadowBlurredBounds.width() <= shadowSharpBounds.width()
        || shadowBlurredBounds.height() <= shadowSharpBounds.height() || hardShadowBlurredPixels >= hardShadowSharpPixels || softShadowBlurredPixels < 200) {
        std::cerr << "Shadow blur is not a soft Gaussian filter: hard=" << hardShadowBlurredPixels << '/' << hardShadowSharpPixels
                  << " soft=" << softShadowBlurredPixels << " bounds=" << shadowBlurredBounds.width() << 'x' << shadowBlurredBounds.height()
                  << " vs " << shadowSharpBounds.width() << 'x' << shadowSharpBounds.height() << '\n';
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
    const auto edgeBlurImage = exportedImage(graph, edgeBlur, pagePath, std::filesystem::temp_directory_path() / "textfx-export-blur-clipped.png");
    const QRect edgeRect(static_cast<int>(edgeBox.bounds.x), static_cast<int>(edgeBox.bounds.y), static_cast<int>(edgeBox.bounds.w), static_cast<int>(edgeBox.bounds.h));
    int edgeBleedPixels = 0;
    for (int y = 0; y < edgeBlurImage.height(); ++y) {
        for (int x = 0; x < edgeBlurImage.width(); ++x) {
            if (!edgeRect.contains(x, y) && edgeBlurImage.pixelColor(x, y) != background) ++edgeBleedPixels;
        }
    }
    if (edgeBlurImage.isNull() || edgeBleedPixels != 0) {
        std::cerr << "Blur export bled outside the box: pixels=" << edgeBleedPixels << '\n';
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
    const auto edgeShadowImage = exportedImage(graph, edgeShadow, pagePath, std::filesystem::temp_directory_path() / "textfx-export-shadow-clipped.png");
    const QRect edgeShadowRect(static_cast<int>(edgeShadowBox.bounds.x), static_cast<int>(edgeShadowBox.bounds.y), static_cast<int>(edgeShadowBox.bounds.w), static_cast<int>(edgeShadowBox.bounds.h));
    int edgeShadowBleedPixels = 0;
    int edgeShadowInsidePixels = 0;
    for (int y = 0; y < edgeShadowImage.height(); ++y) {
        for (int x = 0; x < edgeShadowImage.width(); ++x) {
            if (edgeShadowImage.pixelColor(x, y) == background) continue;
            if (edgeShadowRect.contains(x, y)) ++edgeShadowInsidePixels;
            else ++edgeShadowBleedPixels;
        }
    }
    if (edgeShadowImage.isNull() || edgeShadowBleedPixels != 0 || edgeShadowInsidePixels < 50) {
        std::cerr << "Shadow export bled outside the box or disappeared: bleed=" << edgeShadowBleedPixels << " inside=" << edgeShadowInsidePixels << '\n';
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

    const auto rotatedBlurImage = exportedImage(graph, rotatedBlur, pagePath, std::filesystem::temp_directory_path() / "textfx-export-rotated-blur.png");
    const auto rotatedBlurBounds = nonBackgroundBounds(rotatedBlurImage, background);
    const int rotatedSoftPixels = countPixels(rotatedBlurImage, background, [](const QColor& color) {
        return color.red() > 40 && color.red() < 220 && color.green() > 40 && color.green() < 220 && color.blue() > 40 && color.blue() < 220;
    });
    if (rotatedBlurImage.isNull() || !imagesDiffer(page, rotatedBlurImage) || rotatedBlurBounds.isEmpty()
        || rotatedBlurBounds.left() < 20 || rotatedBlurBounds.top() < 25 || rotatedBlurBounds.right() > 205 || rotatedBlurBounds.bottom() > 130
        || rotatedBlurBounds.width() < 85 || rotatedBlurBounds.height() < 35 || rotatedSoftPixels < 150) {
        std::cerr << "Rotated combined blur export looks cropped or over-bleeding: bounds=" << rotatedBlurBounds.x() << ',' << rotatedBlurBounds.y() << ' '
                  << rotatedBlurBounds.width() << 'x' << rotatedBlurBounds.height() << " soft=" << rotatedSoftPixels << '\n';
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
