#pragma once

#include "core/DocumentModel.h"

#include <QString>

namespace textfx {

class TextBoxEditingService {
public:
  static void setFontFamily(TextBox &box, const QString &family);
  static void setFontSize(TextBox &box, int size);
  static void setTextColor(TextBox &box, const QString &color);
  static void setBold(TextBox &box, bool enabled);
  static void setItalic(TextBox &box, bool enabled);
  static void setUppercase(TextBox &box, bool enabled);
  static void setAlignment(TextBox &box, int alignment);
  static void setLineSpacing(TextBox &box, int spacing);
  static void setLetterSpacing(TextBox &box, int spacing);

  static void move(TextBox &box, double dx, double dy);
  static void resize(TextBox &box, double dw, double dh);
  static void setBounds(TextBox &box, double x, double y, double w, double h);
  static void rotate(TextBox &box, double degrees);
  static void setRotation(TextBox &box, double degrees);

  static void setOutlineEnabled(TextBox &box, bool enabled);
  static void setOutlineColor(TextBox &box, const QString &color);
  static void setOutlineSize(TextBox &box, int size);
  static void setBlurEnabled(TextBox &box, bool enabled);
  static void setBlurSize(TextBox &box, int size);
  static void setShadowEnabled(TextBox &box, bool enabled);
  static void setShadowColor(TextBox &box, const QString &color);
  static void setShadowOffsetX(TextBox &box, int offset);
  static void setShadowOffsetY(TextBox &box, int offset);
  static void setShadowBlurSize(TextBox &box, int size);
  static void setGradientEnabled(TextBox &box, bool enabled);
  static void setGradientDirection(TextBox &box, int direction);
  static void setGradientColorA(TextBox &box, const QString &color);
  static void setGradientColorB(TextBox &box, const QString &color);

  static void setPerspectiveEnabled(TextBox &box, bool enabled);
  static void resetPerspective(TextBox &box);
  static void setPerspectiveHandle(TextBox &box, const QString &corner,
                                   double x, double y);

  static void setPathEnabled(TextBox &box, bool enabled);
  static void setPathMode(TextBox &box, int mode);
  static void resetPath(TextBox &box);
  static void addPathPoint(TextBox &box);
  static bool setPathHandle(TextBox &box, int index, double x, double y);
};

} // namespace textfx
