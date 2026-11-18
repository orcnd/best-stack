#include <doctest/doctest.h>

#include "bs/chart.hpp"
#include "bs/render.hpp"

TEST_CASE("chart: chart_deterministic") {
  std::vector<bs::DayPoint> d{{"2026-10-29", 10, 0}, {"2026-10-30", 20, 0}, {"2026-10-31", 5, 0}};
  CHECK(bs::daily_chart_svg(d) ==
        "<svg class=\"chart-svg\" viewBox=\"0 0 960 240\" width=\"960\" height=\"240\" role=\"img\" "
        "aria-label=\"Günlük olaylar\">"
        "<rect x=\"1\" y=\"130\" width=\"318\" height=\"110\" fill=\"#4f6bed\"/>"
        "<rect x=\"321\" y=\"20\" width=\"318\" height=\"220\" fill=\"#4f6bed\"/>"
        "<rect x=\"641\" y=\"185\" width=\"318\" height=\"55\" fill=\"#4f6bed\"/>"
        "</svg>");
}

TEST_CASE("chart: fractional widths and empty data") {
  std::vector<bs::DayPoint> d(7);
  auto svg = bs::daily_chart_svg(d);
  CHECK(svg.find("<rect x=\"138.14\" y=\"240\" width=\"135.14\" height=\"0\"") != std::string::npos);
}

TEST_CASE("format: tr-TR helpers") {
  CHECK(bs::fmt_money(123450) == "₺1.234,50");
  CHECK(bs::fmt_money(5) == "₺0,05");
  CHECK(bs::fmt_money(123456789) == "₺1.234.567,89");
  CHECK(bs::fmt_int(12345) == "12.345");
  CHECK(bs::fmt_int(999) == "999");
  CHECK(bs::fmt_pct(0.1234) == "%12,3");
  CHECK(bs::fmt_rating(4.5) == "★ 4,5");
}
