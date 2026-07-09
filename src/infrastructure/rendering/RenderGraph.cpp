#include "infrastructure/rendering/RenderGraph.h"

#include "domain/AuthoringLimits.h"
#include "infrastructure/rendering/GaussianBlur.h"
#include "infrastructure/fonts/FontResolver.h"
#include "infrastructure/rendering/TextComposition.h"
#include "infrastructure/rendering/RenderTextLayout.h"

#include <QColor>
#include <QFont>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QImageWriter>
#include <QPainterPathStroker>
#include <QPen>
#include <QSaveFile>
#include <QString>
#include <QVector>

#include <algorithm>
#include <chrono>
#include <expected>
#include <filesystem>
#include <optional>
#include <system_error>

namespace textfx {
namespace {
#ifdef TEXTFX_ENABLE_TEST_HOOKS
std::optional<std::filesystem::path> pngCommitFailurePath;

std::filesystem::path comparablePath(const std::filesystem::path &path) {
  std::error_code error;
  const auto canonical = std::filesystem::weakly_canonical(path, error);
  return error ? path.lexically_normal() : canonical;
}
#endif

QColor toQColor(const std::string &color) {
  const auto hexValue = [](char ch) -> int {
    if (ch >= '0' && ch <= '9')
      return ch - '0';
    if (ch >= 'a' && ch <= 'f')
      return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
      return ch - 'A' + 10;
    return -1;
  };
  if (color.size() != 8)
    return QColor(0, 0, 0, 255);
  const auto byte = [&](std::size_t i) -> int {
    const int high = hexValue(color[i]);
    const int low = hexValue(color[i + 1]);
    if (high < 0 || low < 0)
      return -1;
    return high * 16 + low;
  };
  const int red = byte(0);
  const int green = byte(2);
  const int blue = byte(4);
  const int alpha = byte(6);
  if (red < 0 || green < 0 || blue < 0 || alpha < 0)
    return QColor(0, 0, 0, 255);
  return QColor(red, green, blue, alpha);
}

QFont qFontFor(const TextBox &box) {
  QFont font(QString::fromStdString(box.style.fontFamily));
  font.setPixelSize(std::max(1, box.style.fontSize));
  font.setBold(box.style.bold);
  font.setItalic(box.style.italic);
  font.setLetterSpacing(QFont::AbsoluteSpacing, box.style.letterSpacing);
  return resolveFont(font).font;
}

int qtAlignmentFor(TextAlignment alignment) {
  if (alignment == TextAlignment::Center)
    return Qt::AlignHCenter;
  if (alignment == TextAlignment::Right)
    return Qt::AlignRight;
  return Qt::AlignLeft;
}

QVector<QPointF> normalizedPathPoints(const std::vector<Point> &points) {
  QVector<QPointF> result;
  result.reserve(static_cast<qsizetype>(points.size()));
  for (const auto &point : points)
    result.append({point.x, point.y});
  return result;
}

QBrush fillBrushFor(const TextBox &box) {
  if (!box.effects.gradientEnabled)
    return QBrush(toQColor(box.style.textColor));
  QLinearGradient gradient(
      0, 0, box.effects.gradientDirection == 1 ? box.bounds.w : 0,
      box.effects.gradientDirection == 1 ? 0 : box.bounds.h);
  gradient.setColorAt(0.0, toQColor(box.effects.gradientColorA));
  gradient.setColorAt(1.0, toQColor(box.effects.gradientColorB));
  return QBrush(gradient);
}

std::vector<OutlineLayer> effectiveOutlineLayers(const TextEffects &effects) {
  if (effects.outlineLayersSet || !effects.outlineLayers.empty())
    return effects.outlineLayers;
  if (!effects.outlineEnabled)
    return {};
  return {{effects.outlineEnabled, effects.outlineColor, effects.outlineSize}};
}

std::vector<OutlineLayer> paintOutlineLayers(const TextEffects &effects) {
  return effectiveOutlineLayers(effects);
}

void drawPaintStrokes(QPainter &painter, const std::vector<PaintStroke> &strokes) {
  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  for (const auto &stroke : strokes) {
    if (stroke.points.empty() || stroke.size <= 0.0 || stroke.opacity <= 0.0)
      continue;
    QColor color = toQColor(stroke.color);
    color.setAlphaF(std::clamp(stroke.opacity, 0.0, 1.0) * color.alphaF());
    QPen pen(color, stroke.size, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    painter.setPen(pen);
    if (stroke.points.size() == 1) {
      const auto &point = stroke.points.front();
      painter.drawPoint(QPointF(point.x, point.y));
      continue;
    }
    QPainterPath path(QPointF(stroke.points.front().x, stroke.points.front().y));
    for (std::size_t i = 1; i < stroke.points.size(); ++i)
      path.lineTo(stroke.points[i].x, stroke.points[i].y);
    painter.drawPath(path);
  }
  painter.restore();
}

QVector<TextCompositionOutlineLayer>
compositionOutlineLayers(const std::vector<OutlineLayer> &layers) {
  QVector<TextCompositionOutlineLayer> result;
  result.reserve(static_cast<qsizetype>(layers.size()));
  for (const OutlineLayer &layer : layers)
    result.append({layer.enabled, static_cast<qreal>(layer.size)});
  return result;
}

void fillShadow(QPainter &painter, const QPainterPath &path,
                const QColor &color, int blurSize) {
  const int radius = cappedBlurKernelRadius(blurSize);
  if (radius <= 0) {
    painter.fillPath(path, color);
    return;
  }
  const QRect sourceRect = path.boundingRect()
                               .adjusted(-radius, -radius, radius, radius)
                               .toAlignedRect();
  if (sourceRect.isEmpty())
    return;
  QImage layer(sourceRect.size(), QImage::Format_ARGB32_Premultiplied);
  layer.fill(Qt::transparent);
  QPainter layerPainter(&layer);
  layerPainter.setRenderHint(QPainter::Antialiasing, true);
  layerPainter.translate(-sourceRect.topLeft());
  layerPainter.fillPath(path, color);
  layerPainter.end();
  painter.drawImage(sourceRect.topLeft(), gaussianBlurred(layer, radius));
}

TextLayoutOptions layoutOptionsFor(const TextBox &box, qreal inset) {
  const QString text = QString::fromStdString(box.text);
  return {box.style.uppercase ? text.toUpper()
                              : box.style.lowercase ? text.toLower() : text,
          box.bounds.w,
          box.bounds.h,
          inset,
          static_cast<qreal>(box.style.lineSpacing),
          qtAlignmentFor(box.style.alignment)};
}

void drawTextBox(QPainter &painter, const TextBox &box) {
  if (box.text.empty())
    return;
  const auto font = qFontFor(box);
  const auto outlineLayers = paintOutlineLayers(box.effects);
  const QVector<TextCompositionOutlineLayer> compositionLayers =
      compositionOutlineLayers(outlineLayers);
  const bool hasExplicitOutlineLayers =
      box.effects.outlineLayersSet || !box.effects.outlineLayers.empty();
  const QVector<qreal> outlineStrokeWidths =
      !hasExplicitOutlineLayers
          ? QVector<qreal>{static_cast<qreal>(box.effects.outlineSize)}
          : stackedOutlineStrokeWidths(compositionLayers);
  const qreal outline = !hasExplicitOutlineLayers
          ? (box.effects.outlineEnabled ? box.effects.outlineSize
                                                             : 0)
                               : maximumOutlineStrokeWidth(compositionLayers);
  const qreal inset = outlineInset(outline);
  const QVector<QPointF> normalizedPoints =
      normalizedPathPoints(box.effects.pathPoints);
  const TextLayoutOptions layoutOptions = layoutOptionsFor(box, inset);
  const bool usingPathText = usesPathText(box.effects.pathEnabled, normalizedPoints);
  const TextLayoutPathPolicy pathPolicy{
      .enabled = box.effects.pathEnabled,
      .normalizedPoints = normalizedPoints,
      .smooth = box.effects.pathMode == 1,
      .lineSpacing = static_cast<qreal>(box.style.fontSize +
                                        box.style.lineSpacing)};
  auto path = composeTextLayoutPath(layoutOptions, font, pathPolicy);
  QRectF paintedBounds = paintedTextBounds(path, outline);
  const QPointF translation = translationToConstrainTextBounds(
      paintedBounds, box.bounds.w, box.bounds.h, usingPathText);
  if (!qFuzzyIsNull(translation.x()) || !qFuzzyIsNull(translation.y())) {
    path.translate(translation);
    paintedBounds.translate(translation);
  }
  QRectF effectBounds = paintedBounds;
  const int shadowRadius =
      box.effects.shadowEnabled
          ? cappedBlurKernelRadius(box.effects.shadowBlurSize)
          : 0;
  if (box.effects.shadowEnabled) {
    effectBounds = effectBoundsForShadow(
        paintedBounds, path,
        {static_cast<qreal>(box.effects.shadowOffsetX),
         static_cast<qreal>(box.effects.shadowOffsetY)}, shadowRadius);
  }

  painter.save();
  painter.setRenderHint(QPainter::Antialiasing, true);
  painter.translate(box.bounds.x + box.bounds.w / 2.0,
                    box.bounds.y + box.bounds.h / 2.0);
  painter.rotate(box.rotationDegrees);
  painter.translate(-box.bounds.w / 2.0, -box.bounds.h / 2.0);
  if (box.effects.perspectiveEnabled) {
    QPolygonF source({{0, 0},
                      {box.bounds.w, 0},
                      {box.bounds.w, box.bounds.h},
                      {0, box.bounds.h}});
    QPolygonF target(
        {{box.effects.perspectiveNw.x, box.effects.perspectiveNw.y},
         {box.bounds.w + box.effects.perspectiveNe.x,
          box.effects.perspectiveNe.y},
         {box.bounds.w + box.effects.perspectiveSe.x,
          box.bounds.h + box.effects.perspectiveSe.y},
         {box.effects.perspectiveSw.x,
          box.bounds.h + box.effects.perspectiveSw.y}});
    QTransform transform;
    if (QTransform::quadToQuad(source, target, transform))
      painter.setTransform(transform, true);
  }
  auto paintText = [&](QPainter &target, bool clipShadow) {
    if (box.effects.shadowEnabled) {
      if (clipShadow) {
        target.save();
        target.setClipRect(QRectF(0, 0, box.bounds.w, box.bounds.h));
        fillShadow(target,
                   path.translated(box.effects.shadowOffsetX,
                                   box.effects.shadowOffsetY),
                   toQColor(box.effects.shadowColor),
                   box.effects.shadowBlurSize);
        target.restore();
      } else {
        fillShadow(target,
                   path.translated(box.effects.shadowOffsetX,
                                   box.effects.shadowOffsetY),
                   toQColor(box.effects.shadowColor),
                   box.effects.shadowBlurSize);
      }
    }
    for (auto i = outlineLayers.size(); i > 0; --i) {
      const auto &outlineLayer = outlineLayers[i - 1];
      const qreal strokeWidth = outlineStrokeWidths[i - 1];
      if (!outlineLayer.enabled || strokeWidth <= 0)
        continue;
      QPainterPathStroker stroker;
      stroker.setWidth(strokeWidth);
      stroker.setJoinStyle(Qt::RoundJoin);
      QPainterPath stroke = stroker.createStroke(path);
      stroke.setFillRule(Qt::WindingFill);
      target.fillPath(stroke, toQColor(outlineLayer.color));
    }
  };
  auto paintFill = [&](QPainter &target) {
    target.fillPath(path, fillBrushFor(box));
  };
  if (box.effects.blurEnabled && box.effects.blurSize > 0) {
    const int pad = std::max(1, cappedBlurKernelRadius(box.effects.blurSize));
    const QRect sourceRect =
        effectBounds.adjusted(-pad, -pad, pad, pad)
            .toAlignedRect()
            .intersected(QRectF(0, 0, box.bounds.w, box.bounds.h)
                             .adjusted(-pad, -pad, pad, pad)
                             .toAlignedRect());
    if (sourceRect.isEmpty()) {
      painter.restore();
      return;
    }
    QImage layer(sourceRect.size(), QImage::Format_ARGB32_Premultiplied);
    layer.fill(Qt::transparent);
    QPainter layerPainter(&layer);
    layerPainter.setRenderHint(QPainter::Antialiasing, true);
    layerPainter.translate(-sourceRect.topLeft());
    paintText(layerPainter, false);
    paintFill(layerPainter);
    layerPainter.end();
    painter.save();
    painter.setClipRect(QRectF(0, 0, box.bounds.w, box.bounds.h));
    painter.drawImage(sourceRect.topLeft(), gaussianBlurred(layer, pad));
    painter.restore();
  } else {
    paintText(painter, true);
    paintFill(painter);
  }
  painter.restore();
}

} // namespace

#ifdef TEXTFX_ENABLE_TEST_HOOKS
namespace test_hooks {
void failPngCommit(std::filesystem::path path) {
  pngCommitFailurePath = comparablePath(path);
}

void clearPngCommitFailure() { pngCommitFailurePath.reset(); }
} // namespace test_hooks
#endif

std::expected<RenderGraph::ExportResult, std::string>
RenderGraph::exportPagePngTimed(const DocumentModel &document,
                                const std::filesystem::path &pageImagePath,
                                const std::filesystem::path &exportPath) const {
  const auto startTime = std::chrono::steady_clock::now();
  const auto exportResult = [&startTime] {
    return ExportResult{
        ExportTiming{std::chrono::steady_clock::now() - startTime}};
  };

  QImage source(QString::fromStdString(pageImagePath.string()));
  if (source.isNull()) {
    return std::unexpected("Could not load page image: " +
                           pageImagePath.string());
  }
  const bool sourceHasAlpha = source.hasAlphaChannel();
  source = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);

  QPainter painter(&source);
  drawPaintStrokes(painter, document.paint().behindText);
  for (const auto &box : document.textBoxes())
    drawTextBox(painter, box);
  drawPaintStrokes(painter, document.paint().aboveText);
  painter.end();

  std::error_code filesystemError;
  const auto exportDirectory = exportPath.parent_path();
  if (!exportDirectory.empty())
    std::filesystem::create_directories(exportDirectory, filesystemError);
  if (filesystemError)
    return std::unexpected("Could not write PNG output: " +
                           exportPath.string());

  const QImage exportImage = sourceHasAlpha
                                 ? source
                                 : source.convertToFormat(QImage::Format_RGB888);
  QSaveFile exportFile(QString::fromStdString(exportPath.string()));
  if (!exportFile.open(QIODevice::WriteOnly))
    return std::unexpected("Could not write PNG output: " +
                           exportPath.string());

  QImageWriter writer(&exportFile, "PNG");
  if (!writer.write(exportImage))
    return std::unexpected("Could not write PNG output: " +
                           exportPath.string());

#ifdef TEXTFX_ENABLE_TEST_HOOKS
  if (pngCommitFailurePath && comparablePath(exportPath) == *pngCommitFailurePath)
    return std::unexpected("Could not write PNG output: " +
                           exportPath.string());
#endif

  if (!exportFile.commit())
    return std::unexpected("Could not write PNG output: " +
                           exportPath.string());

  return exportResult();
}

std::expected<void, std::string> RenderGraph::exportPagePngResult(
    const DocumentModel &document, const std::filesystem::path &pageImagePath,
    const std::filesystem::path &exportPath) const {
  const auto result = exportPagePngTimed(document, pageImagePath, exportPath);
  if (!result)
    return std::unexpected(result.error());
  return {};
}

bool RenderGraph::exportPagePng(const DocumentModel &document,
                                const std::filesystem::path &pageImagePath,
                                const std::filesystem::path &exportPath,
                                std::string *error) const {
  const auto result = exportPagePngResult(document, pageImagePath, exportPath);
  if (!result) {
    if (error)
      *error = result.error();
    return false;
  }
  return true;
}

std::vector<std::string>
RenderGraph::warnings(const DocumentModel &document) const {
  std::vector<std::string> result;
  for (const auto &box : document.textBoxes()) {
    if (box.effects.perspectiveEnabled)
      result.push_back("Perspective is approximated with an affine transform "
                       "in MVP export/preview");
    if (box.effects.pathEnabled)
      result.push_back(box.effects.pathMode == 1
                           ? "Smooth path text is approximated with "
                             "TypeX-style smoothed segments"
                           : "Path text is approximated on straight segments "
                             "between path handles");
  }
  std::ranges::sort(result);
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

} // namespace textfx
