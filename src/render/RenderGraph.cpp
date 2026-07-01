#include "render/RenderGraph.h"

#include "core/EffectLimits.h"
#include "core/GaussianBlur.h"
#include "fonts/FontResolver.h"
#include "render/RenderTextLayout.h"

#include <QColor>
#include <QFont>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPainterPathStroker>
#include <QString>
#include <QVector>

#include <algorithm>
#include <chrono>
#include <expected>
#include <filesystem>
#include <system_error>

namespace textfx {
namespace {
QColor toQColor(const std::string &color) {
  if (color.size() != 8)
    return QColor(0, 0, 0, 255);
  const auto byte = [&](std::size_t i) {
    return std::stoi(color.substr(i, 2), nullptr, 16);
  };
  return QColor(byte(0), byte(2), byte(4), byte(6));
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
  return {box.style.uppercase ? QString::fromStdString(box.text).toUpper()
                              : QString::fromStdString(box.text),
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
  const qreal outline =
      box.effects.outlineEnabled && box.effects.outlineSize > 0
          ? box.effects.outlineSize
          : 0;
  const qreal inset = outline > 0 ? outline / 2.0 : 0.0;
  const QVector<QPointF> normalizedPoints =
      normalizedPathPoints(box.effects.pathPoints);
  const TextLayoutOptions layoutOptions = layoutOptionsFor(box, inset);
  const bool usingPathText = box.effects.pathEnabled &&
                             normalizedPoints.size() > 1 &&
                             !isNeutralFlatPath(normalizedPoints);
  auto path =
      usingPathText
          ? pathTextLayoutPath(layoutOptions, font, normalizedPoints,
                               box.effects.pathMode == 1,
                               box.style.fontSize + box.style.lineSpacing)
          : textLayoutPath(layoutOptions, font);
  QRectF paintedBounds = path.boundingRect();
  if (outline > 0) {
    QPainterPathStroker stroker;
    stroker.setWidth(outline);
    stroker.setJoinStyle(Qt::RoundJoin);
    paintedBounds =
        paintedBounds.united(stroker.createStroke(path).boundingRect());
  }
  qreal dx = 0.0;
  qreal dy = 0.0;
  if (paintedBounds.left() < 0.0)
    dx = -paintedBounds.left();
  else if (paintedBounds.width() <= box.bounds.w &&
           paintedBounds.right() > box.bounds.w)
    dx = box.bounds.w - paintedBounds.right();
  if (!usingPathText) {
    if (paintedBounds.top() < 0.0)
      dy = -paintedBounds.top();
    else if (paintedBounds.height() <= box.bounds.h &&
             paintedBounds.bottom() > box.bounds.h)
      dy = box.bounds.h - paintedBounds.bottom();
  }
  if (!qFuzzyIsNull(dx) || !qFuzzyIsNull(dy)) {
    path.translate(dx, dy);
    paintedBounds.translate(dx, dy);
  }
  QRectF effectBounds = paintedBounds;
  const int shadowRadius =
      box.effects.shadowEnabled
          ? cappedBlurKernelRadius(box.effects.shadowBlurSize)
          : 0;
  if (box.effects.shadowEnabled) {
    effectBounds = effectBounds.united(
        path.boundingRect()
            .translated(box.effects.shadowOffsetX, box.effects.shadowOffsetY)
            .adjusted(-shadowRadius, -shadowRadius, shadowRadius,
                      shadowRadius));
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
    if (outline <= 0)
      return;
    QPainterPathStroker stroker;
    stroker.setWidth(outline);
    stroker.setJoinStyle(Qt::RoundJoin);
    QPainterPath stroke = stroker.createStroke(path);
    stroke.setFillRule(Qt::WindingFill);
    target.fillPath(stroke, toQColor(box.effects.outlineColor));
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
  source = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);

  QPainter painter(&source);
  for (const auto &box : document.textBoxes())
    drawTextBox(painter, box);
  painter.end();

  std::error_code filesystemError;
  const auto exportDirectory = exportPath.parent_path();
  if (!exportDirectory.empty())
    std::filesystem::create_directories(exportDirectory, filesystemError);
  if (filesystemError)
    return std::unexpected("Could not write PNG output: " +
                           exportPath.string());

  const auto tempPath = exportPath.string() + ".tmp";
  if (!source.save(QString::fromStdString(tempPath), "PNG")) {
    return std::unexpected("Could not write PNG output: " +
                           exportPath.string());
  }
  std::filesystem::remove(exportPath, filesystemError);
  if (filesystemError)
    return std::unexpected("Could not write PNG output: " +
                           exportPath.string());

  std::filesystem::rename(tempPath, exportPath, filesystemError);
  if (filesystemError)
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
