#include "bs/chart.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace bs {
namespace {
// Up to 2 decimals, trailing zeros and dot stripped (matches the TS port).
std::string fmt(double v) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.2f", std::round(v * 100.0) / 100.0);
  std::string s = buf;
  while (!s.empty() && s.back() == '0') s.pop_back();
  if (!s.empty() && s.back() == '.') s.pop_back();
  return s == "-0" ? "0" : s;
}
}  // namespace

std::string daily_chart_svg(const std::vector<DayPoint>& days, int width, int height) {
  long max = 0;
  for (auto& d : days) max = std::max(max, d.events);
  if (max == 0) max = 1;
  const double n = days.empty() ? 1.0 : (double)days.size();
  const double bar_w = width / n;
  std::string out = "<svg class=\"chart-svg\" viewBox=\"0 0 " + std::to_string(width) + " " + std::to_string(height) +
                    "\" width=\"" + std::to_string(width) + "\" height=\"" + std::to_string(height) +
                    "\" role=\"img\" aria-label=\"Günlük olaylar\">";
  for (size_t i = 0; i < days.size(); i++) {
    long h = std::lround((double)days[i].events / (double)max * (height - 20));
    out += "<rect x=\"" + fmt(i * bar_w + 1) + "\" y=\"" + std::to_string(height - h) + "\" width=\"" +
           fmt(bar_w - 2) + "\" height=\"" + std::to_string(h) + "\" fill=\"#4f6bed\"/>";
  }
  return out + "</svg>";
}
}  // namespace bs
