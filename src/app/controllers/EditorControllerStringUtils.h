#pragma once

#include <QString>

#include <string>

namespace textfx {

inline QString toQString(const std::string &value) {
  return QString::fromStdString(value);
}

inline std::string toStdString(const QString &value) {
  return value.toStdString();
}

} // namespace textfx
