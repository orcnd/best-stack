// Writers of the fast core: every helper appends into the caller's buffer.
#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "bsf/html.hpp"

namespace {
template <class F>
std::string w(F f) {
  std::string out = "pre:";
  f(out);
  return out;
}
}  // namespace

TEST_CASE("fast: esc escapes the five HTML specials while appending") {
  CHECK(w([](auto& o) { bsf::esc(o, "a<b>&\"c'd"); }) == "pre:a&lt;b&gt;&amp;&quot;c&#39;d");
  CHECK(w([](auto& o) { bsf::esc(o, ""); }) == "pre:");
  CHECK(w([](auto& o) { bsf::esc(o, "Çalışma ğüş"); }) == "pre:Çalışma ğüş");
  CHECK(w([](auto& o) { bsf::esc(o, "<<<"); }) == "pre:&lt;&lt;&lt;");
}

TEST_CASE("fast: url_enc matches the original href encoding") {
  CHECK(w([](auto& o) { bsf::url_enc(o, "a b&c=d/é~._-"); }) == "pre:a%20b%26c%3Dd%2F%C3%A9~._-");
}

TEST_CASE("fast: integers and tr-TR formatting") {
  CHECK(w([](auto& o) { bsf::integer(o, 0); }) == "pre:0");
  CHECK(w([](auto& o) { bsf::integer(o, -42); }) == "pre:-42");
  CHECK(w([](auto& o) { bsf::money(o, 123450); }) == "pre:₺1.234,50");
  CHECK(w([](auto& o) { bsf::money(o, 5); }) == "pre:₺0,05");
  CHECK(w([](auto& o) { bsf::money(o, 123456789); }) == "pre:₺1.234.567,89");
  CHECK(w([](auto& o) { bsf::grouped(o, 12345); }) == "pre:12.345");
  CHECK(w([](auto& o) { bsf::grouped(o, 999); }) == "pre:999");
  CHECK(w([](auto& o) { bsf::grouped(o, 1000); }) == "pre:1.000");
  CHECK(w([](auto& o) { bsf::grouped(o, -1234567); }) == "pre:-1.234.567");
  CHECK(w([](auto& o) { bsf::pct(o, 0.1234); }) == "pre:%12,3");
  CHECK(w([](auto& o) { bsf::pct(o, 0.0); }) == "pre:%0,0");
  CHECK(w([](auto& o) { bsf::rating(o, 4.5); }) == "pre:★ 4,5");
  CHECK(w([](auto& o) { bsf::rating(o, 3.0); }) == "pre:★ 3,0");
}

TEST_CASE("fast: chart svg is identical to the original algorithm") {
  std::vector<bs::DayPoint> d{{"2026-10-29", 10, 0}, {"2026-10-30", 20, 0}, {"2026-10-31", 5, 0}};
  CHECK(w([&](auto& o) { bsf::chart_svg(o, d); }) ==
        "pre:<svg class=\"chart-svg\" viewBox=\"0 0 960 240\" width=\"960\" height=\"240\" role=\"img\" "
        "aria-label=\"Günlük olaylar\">"
        "<rect x=\"1\" y=\"130\" width=\"318\" height=\"110\" fill=\"#4f6bed\"/>"
        "<rect x=\"321\" y=\"20\" width=\"318\" height=\"220\" fill=\"#4f6bed\"/>"
        "<rect x=\"641\" y=\"185\" width=\"318\" height=\"55\" fill=\"#4f6bed\"/>"
        "</svg>");
  std::vector<bs::DayPoint> e(7);
  auto svg = w([&](auto& o) { bsf::chart_svg(o, e); });
  CHECK(svg.find("<rect x=\"138.14\" y=\"240\" width=\"135.14\" height=\"0\"") != std::string::npos);
}
