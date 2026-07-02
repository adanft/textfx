#pragma once

#include "core/DocumentModel.h"

#include <QString>
#include <QVariantList>

namespace textfx {

struct BoxRenderState {
  int index = -1;
  QString text;
  double x = 0.0;
  double y = 0.0;
  double width = 0.0;
  double height = 0.0;
  double rotation = 0.0;
  QString fontFamily;
  QString resolvedFontFamily;
  int fontSize = 0;
  QString color;
  int lineSpacing = 0;
  int letterSpacing = 0;
  bool bold = false;
  bool italic = false;
  bool uppercase = false;
  int alignment = 0;
  bool outline = false;
  QString outlineColor;
  int outlineSize = 0;
  bool blur = false;
  int blurSize = 0;
  bool shadow = false;
  QString shadowColor;
  int shadowOffsetX = 0;
  int shadowOffsetY = 0;
  int shadowBlurSize = 0;
  bool gradient = false;
  int gradientDirection = 0;
  QString gradientColorA;
  QString gradientColorB;
  bool perspective = false;
  QVariantList perspectiveNw;
  QVariantList perspectiveNe;
  QVariantList perspectiveSe;
  QVariantList perspectiveSw;
  bool path = false;
  int pathMode = 0;
  QVariantList pathPoints;
};

BoxRenderState mapBoxRenderState(const TextBox &box, int index);
QVariantList pointList(const std::vector<Point> &points);
QVariantList pointValue(const Point &point);

} // namespace textfx
