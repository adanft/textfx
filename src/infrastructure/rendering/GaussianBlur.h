#pragma once

#include "domain/AuthoringLimits.h"

#include <QImage>
#include <QVector>

#include <algorithm>
#include <array>
#include <cmath>

namespace textfx {

inline QVector<qreal> gaussianKernel(int radius) {
  QVector<qreal> kernel(radius * 2 + 1);
  const qreal sigma = std::max<qreal>(0.5, radius / 3.0);
  qreal sum = 0.0;
  for (int i = -radius; i <= radius; ++i) {
    const qreal weight = std::exp(-(i * i) / (2.0 * sigma * sigma));
    kernel[i + radius] = weight;
    sum += weight;
  }
  for (qreal &weight : kernel)
    weight /= sum;
  return kernel;
}

inline const QVector<qreal> &cachedGaussianKernel(int radius) {
  static const auto kernels = [] {
    std::array<QVector<qreal>, MaxBlurKernelRadius + 1> result;
    for (int i = 0; i <= MaxBlurKernelRadius; ++i)
      result[static_cast<std::size_t>(i)] = gaussianKernel(i);
    return result;
  }();
  return kernels[static_cast<std::size_t>(
      std::clamp(radius, 0, MaxBlurKernelRadius))];
}

inline QImage gaussianBlurred(const QImage &source, int radius) {
  if (radius <= 0)
    return source;
  radius = std::min(radius, MaxBlurKernelRadius);
  const QImage input =
      source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
  QImage horizontal(input.size(), QImage::Format_ARGB32_Premultiplied);
  QImage output(input.size(), QImage::Format_ARGB32_Premultiplied);
  const QVector<qreal> &kernel = cachedGaussianKernel(radius);
  auto sample = [](const QImage &image, int x, int y) -> QRgb {
    if (x < 0 || y < 0 || x >= image.width() || y >= image.height())
      return 0;
    return reinterpret_cast<const QRgb *>(image.constScanLine(y))[x];
  };
  auto pass = [&](const QImage &from, QImage &to, bool vertical) {
    for (int y = 0; y < from.height(); ++y) {
      auto *out = reinterpret_cast<QRgb *>(to.scanLine(y));
      for (int x = 0; x < from.width(); ++x) {
        qreal a = 0.0, r = 0.0, g = 0.0, b = 0.0;
        for (int i = -radius; i <= radius; ++i) {
          const QRgb px =
              vertical ? sample(from, x, y + i) : sample(from, x + i, y);
          const qreal weight = kernel[i + radius];
          a += qAlpha(px) * weight;
          r += qRed(px) * weight;
          g += qGreen(px) * weight;
          b += qBlue(px) * weight;
        }
        out[x] =
            qRgba(static_cast<int>(std::clamp<long>(std::lround(r), 0, 255)),
                  static_cast<int>(std::clamp<long>(std::lround(g), 0, 255)),
                  static_cast<int>(std::clamp<long>(std::lround(b), 0, 255)),
                  static_cast<int>(std::clamp<long>(std::lround(a), 0, 255)));
      }
    }
  };
  pass(input, horizontal, false);
  pass(horizontal, output, true);
  return output;
}

} // namespace textfx
