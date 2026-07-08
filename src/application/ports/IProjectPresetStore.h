#pragma once

#include "domain/document/DocumentModel.h"

#include <string>
#include <vector>

namespace textfx {

class IProjectPresetStore {
public:
  virtual ~IProjectPresetStore() = default;

  virtual bool savePresets(const std::vector<TextPreset> &projectPresets,
                           std::string *error = nullptr) const = 0;
  virtual bool loadPresets(DocumentModel &document,
                           std::vector<TextPreset> &projectPresets,
                           std::string *error = nullptr) const = 0;
};

} // namespace textfx
