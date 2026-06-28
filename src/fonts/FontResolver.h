#pragma once

#include <QFont>
#include <QString>

namespace textfx {

struct ResolvedFont {
    QFont font;
    QString requestedFamily;
    QString resolvedFamily;
    bool requestedAvailable = false;
    bool exactMatch = false;
    bool fellBack = false;
};

ResolvedFont resolveFont(QFont requested);

} // namespace textfx
