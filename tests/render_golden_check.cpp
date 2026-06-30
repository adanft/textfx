#include "render/RenderGraph.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>

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

int imageDifferenceCount(const QImage& a, const QImage& b)
{
    if (a.size() != b.size()) return std::numeric_limits<int>::max();
    int count = 0;
    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            if (a.pixelColor(x, y) != b.pixelColor(x, y)) ++count;
        }
    }
    return count;
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
    if (firstTextY < 50 || firstTextY > 70) {
        std::cerr << "Export text is not vertically centered in its box: y=" << firstTextY << '\n';
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

    if (!graph.exportPagePng(allEffects, pagePath, outPath, &error) || !hasPngMagic(outPath) || std::filesystem::file_size(outPath) <= 8) {
        std::cerr << "All-effects PNG export failed: " << error << '\n';
        return 1;
    }
    const QImage fxImage(QString::fromStdString(outPath.string()));
    const auto fxBounds = nonBackgroundBounds(fxImage, background);
    if (fxBounds.top() < 35 || fxBounds.height() < 20) {
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

    const auto defaultFlatOffImage = exportedImage(graph, defaultFlatOff, pagePath, std::filesystem::temp_directory_path() / "textfx-export-default-path-off.png");
    const auto defaultFlatOnImage = exportedImage(graph, defaultFlatOn, pagePath, std::filesystem::temp_directory_path() / "textfx-export-default-path-on.png");
    const auto defaultFlatOffBounds = nonBackgroundBounds(defaultFlatOffImage, background);
    const auto defaultFlatOnBounds = nonBackgroundBounds(defaultFlatOnImage, background);
    const int defaultFlatDiff = imageDifferenceCount(defaultFlatOffImage, defaultFlatOnImage);
    if (defaultFlatOffImage.isNull() || defaultFlatOnImage.isNull() || defaultFlatOffBounds.isEmpty() || defaultFlatOnBounds.isEmpty()
        || defaultFlatDiff != 0 || std::abs(defaultFlatOffBounds.left() - defaultFlatOnBounds.left()) > 3 || std::abs(defaultFlatOffBounds.top() - defaultFlatOnBounds.top()) > 3
        || std::abs(defaultFlatOffBounds.width() - defaultFlatOnBounds.width()) > 6 || std::abs(defaultFlatOffBounds.height() - defaultFlatOnBounds.height()) > 6) {
        std::cerr << "Default flat path changed rendered pixels: diff=" << defaultFlatDiff << " off=" << defaultFlatOffBounds.x() << ',' << defaultFlatOffBounds.y() << ' '
                  << defaultFlatOffBounds.width() << 'x' << defaultFlatOffBounds.height() << " on=" << defaultFlatOnBounds.x() << ','
                  << defaultFlatOnBounds.y() << ' ' << defaultFlatOnBounds.width() << 'x' << defaultFlatOnBounds.height() << '\n';
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
    const auto shortPathBaselineImage = exportedImage(graph, shortPathBaseline, pagePath, std::filesystem::temp_directory_path() / "textfx-export-short-path-baseline.png");
    const auto tallPathBaselineImage = exportedImage(graph, tallPathBaseline, pagePath, std::filesystem::temp_directory_path() / "textfx-export-tall-path-baseline.png");
    const auto shortPathBaselineBounds = nonBackgroundBounds(shortPathBaselineImage, background);
    const auto tallPathBaselineBounds = nonBackgroundBounds(tallPathBaselineImage, background);
    const int shortPathBaselineY = static_cast<int>(pathBaselineBox.bounds.y + 80.0 * 0.75);
    if (shortPathBaselineImage.isNull() || tallPathBaselineImage.isNull() || shortPathBaselineBounds.isEmpty() || tallPathBaselineBounds.isEmpty()
        || tallPathBaselineBounds.top() - shortPathBaselineBounds.top() < 24) {
        std::cerr << "Straight path text did not follow absolute path y: short=" << shortPathBaselineBounds.x() << ',' << shortPathBaselineBounds.y() << ' '
                  << shortPathBaselineBounds.width() << 'x' << shortPathBaselineBounds.height() << " tall=" << tallPathBaselineBounds.x() << ','
                  << tallPathBaselineBounds.y() << ' ' << tallPathBaselineBounds.width() << 'x' << tallPathBaselineBounds.height() << '\n';
        return 1;
    }
    if (std::abs(shortPathBaselineBounds.bottom() - shortPathBaselineY) > 4) {
        std::cerr << "Straight path text is not on the guide baseline: guideY=" << shortPathBaselineY << " bounds=" << shortPathBaselineBounds.x() << ','
                  << shortPathBaselineBounds.y() << ' ' << shortPathBaselineBounds.width() << 'x' << shortPathBaselineBounds.height() << '\n';
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
    const auto topPathBaselineImage = exportedImage(graph, topPathBaseline, pagePath, std::filesystem::temp_directory_path() / "textfx-export-top-path-baseline.png");
    const auto topPathBaselineBounds = nonBackgroundBounds(topPathBaselineImage, background);
    const int expectedPathY = static_cast<int>(topPathBox.bounds.y + topPathBox.bounds.h * 0.05);
    if (topPathBaselineImage.isNull() || topPathBaselineBounds.isEmpty() || topPathBaselineBounds.bottom() > expectedPathY + 6) {
        std::cerr << "Straight path text detached below guide: guideY=" << expectedPathY << " bounds=" << topPathBaselineBounds.x() << ','
                  << topPathBaselineBounds.y() << ' ' << topPathBaselineBounds.width() << 'x' << topPathBaselineBounds.height() << '\n';
        return 1;
    }

    DocumentModel exportFlatPath;
    TextBox flatPathBox;
    flatPathBox.text = "HELLO\nWEAVER!";
    flatPathBox.bounds = {0.0, 20.0, 240.0, 120.0};
    flatPathBox.style.fontSize = 34;
    flatPathBox.style.letterSpacing = 1.5;
    flatPathBox.style.textColor = "000000ff";
    flatPathBox.effects.pathEnabled = true;
    flatPathBox.effects.pathPoints = {{0.0, 0.65}, {1.0, 0.65}};
    exportFlatPath.addTextBox(flatPathBox);
    const auto flatPathImage = exportedImage(graph, exportFlatPath, pagePath, std::filesystem::temp_directory_path() / "textfx-export-flat-path-advance.png");
    const auto flatPathBounds = nonBackgroundBounds(flatPathImage, background);
    if (flatPathImage.isNull() || flatPathBounds.height() <= 50 || flatPathBounds.width() < 120 || flatPathBounds.width() >= 240) {
        std::cerr << "Path text export collapsed multiline layout: bounds=" << flatPathBounds.x() << ',' << flatPathBounds.y() << ' '
                  << flatPathBounds.width() << 'x' << flatPathBounds.height() << '\n';
        return 1;
    }

    flatPathBox.text = "HELLO WEAVER!";
    flatPathBox.bounds = {0.0, 20.0, 150.0, 120.0};
    DocumentModel exportWrappedFlatPath;
    exportWrappedFlatPath.addTextBox(flatPathBox);
    const auto wrappedFlatPathImage = exportedImage(graph, exportWrappedFlatPath, pagePath, std::filesystem::temp_directory_path() / "textfx-export-wrapped-flat-path.png");
    const auto wrappedFlatPathBounds = nonBackgroundBounds(wrappedFlatPathImage, background);
    if (wrappedFlatPathImage.isNull() || wrappedFlatPathBounds.height() <= 50 || wrappedFlatPathBounds.width() >= 150) {
        std::cerr << "Path text export ignored visual wrapping: bounds=" << wrappedFlatPathBounds.x() << ',' << wrappedFlatPathBounds.y() << ' '
                  << wrappedFlatPathBounds.width() << 'x' << wrappedFlatPathBounds.height() << '\n';
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
    const auto evenTwoPointImage = exportedImage(graph, exportEvenTwoPointPath, pagePath, std::filesystem::temp_directory_path() / "textfx-export-even-two-point-path.png");
    const auto unevenFlatImage = exportedImage(graph, exportUnevenFlatPath, pagePath, std::filesystem::temp_directory_path() / "textfx-export-uneven-flat-path.png");
    const int unevenFlatDiff = imageDifferenceCount(evenTwoPointImage, unevenFlatImage);
    if (evenTwoPointImage.isNull() || unevenFlatImage.isNull() || unevenFlatDiff > 100) {
        std::cerr << "Uneven collinear path changed glyph spacing: diffPixels=" << unevenFlatDiff << '\n';
        return 1;
    }

    flatPathBox.effects.pathPoints = {{0.0, 0.65}, {1.0, 0.65}};
    DocumentModel exportFlatSpacingPath;
    exportFlatSpacingPath.addTextBox(flatPathBox);
    flatPathBox.effects.pathPoints = {{0.0, 0.65}, {0.1, 0.20}, {1.0, 0.65}};
    DocumentModel exportCurvedSpacingPath;
    exportCurvedSpacingPath.addTextBox(flatPathBox);
    const auto flatSpacingImage = exportedImage(graph, exportFlatSpacingPath, pagePath, std::filesystem::temp_directory_path() / "textfx-export-flat-spacing-path.png");
    const auto curvedSpacingImage = exportedImage(graph, exportCurvedSpacingPath, pagePath, std::filesystem::temp_directory_path() / "textfx-export-curved-spacing-path.png");
    const auto flatSpacingBounds = nonBackgroundBounds(flatSpacingImage, background);
    const auto curvedSpacingBounds = nonBackgroundBounds(curvedSpacingImage, background);
    if (flatSpacingImage.isNull() || curvedSpacingImage.isNull() || flatSpacingBounds.isEmpty() || curvedSpacingBounds.isEmpty()) {
        std::cerr << "Curved path changed horizontal glyph spacing: flat=" << flatSpacingBounds.x() << ',' << flatSpacingBounds.y() << ' '
                  << flatSpacingBounds.width() << 'x' << flatSpacingBounds.height() << " curved=" << curvedSpacingBounds.x() << ','
                  << curvedSpacingBounds.y() << ' ' << curvedSpacingBounds.width() << 'x' << curvedSpacingBounds.height() << '\n';
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
    const auto layoutAngledImage = exportedImage(graph, exportLayoutAngledPath, pagePath, std::filesystem::temp_directory_path() / "textfx-export-layout-angled-path.png");
    const auto layoutAngledBounds = nonBackgroundBounds(layoutAngledImage, background);
    if (layoutAngledImage.isNull() || layoutAngledBounds.width() <= layoutAngledBounds.height()) {
        std::cerr << "Path text export did not use layout-scaled angle: bounds=" << layoutAngledBounds.width() << 'x' << layoutAngledBounds.height() << '\n';
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
