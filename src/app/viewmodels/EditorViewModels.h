#pragma once

#include "domain/document/DocumentModel.h"

#include <QVariantList>
#include <QVariantMap>

#include <vector>

namespace textfx::EditorViewModels {

QVariantList pointList(const std::vector<Point> &points);
QVariantMap textBoxMap(const TextBox &box, int index);
QVariantList textBoxList(const std::vector<TextBox> &boxes);
QVariantList layerList(const std::vector<TextBox> &boxes);
QVariantMap presetMap(const TextPreset &preset);
QVariantList presetList(const std::vector<TextPreset> &presets);

} // namespace textfx::EditorViewModels
