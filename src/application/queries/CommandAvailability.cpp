#include "application/queries/CommandAvailability.h"

namespace textfx {

bool CommandAvailability::isEnabled(const QString &command,
                                    CommandAvailabilityState state) {
  if (command == QStringLiteral("new") || command == QStringLiteral("open")) {
    return true;
  }
  if (command == QStringLiteral("paste")) {
    return state.hasProject;
  }
  if (command == QStringLiteral("copy") || command == QStringLiteral("delete") ||
      command == QStringLiteral("duplicate")) {
    return state.hasSelectedBox;
  }
  return state.hasProject;
}

} // namespace textfx
