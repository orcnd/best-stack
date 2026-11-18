#pragma once
#include <string>
#include <vector>

#include "bs/model.hpp"

namespace bs {
/// Deterministic bar chart; algorithm specified in shared/markup-spec.md (ported 1:1 to TS).
std::string daily_chart_svg(const std::vector<DayPoint>& days, int width = 960, int height = 240);
}  // namespace bs
