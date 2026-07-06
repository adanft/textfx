#pragma once

#include <QString>

namespace textfx {

struct CommandAvailabilityState {
  bool hasProject = false;
  bool hasSelectedBox = false;
};

class CommandAvailability final {
public:
  static bool isEnabled(const QString &command,
                        CommandAvailabilityState state);
};

} // namespace textfx
