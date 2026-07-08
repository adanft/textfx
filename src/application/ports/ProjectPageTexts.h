#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace textfx {

using PageTextMap = std::unordered_map<std::string, std::vector<std::string>>;
using PageTextPositionMap = std::unordered_map<std::string, int>;

} // namespace textfx
