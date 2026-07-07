#pragma once

#include "domain/document/DocumentModel.h"

#include <QString>

namespace textfx {

class TextBoxClipboardService {
public:
  static QString serialize(const TextBox &box, int index);
  static TextBox deserializeOrPlainText(const QString &clipboardText);
};

} // namespace textfx
