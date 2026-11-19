#pragma once
// Fast-core writers: everything appends into the response's single std::string (no temporaries).
#include <string>
#include <string_view>
#include <vector>

#include "bs/model.hpp"

namespace bsf {

/// Appends s with & < > " ' escaped (same entities as the original bs::view::html_escape).
void esc(std::string& out, std::string_view s);
/// Appends s percent-encoded (unreserved = alnum - _ . ~), as in the original catalog hrefs.
void url_enc(std::string& out, std::string_view s);
/// Plain decimal integer.
void integer(std::string& out, long long v);

/// tr-TR formatting, byte-identical to bs::fmt_int / fmt_money / fmt_pct / fmt_rating.
void grouped(std::string& out, long v);    // 12.345
void money(std::string& out, long cents);  // ₺1.234,50
void pct(std::string& out, double ratio);  // %12,3
void rating(std::string& out, double r);   // ★ 4,5

/// Deterministic daily bar chart, byte-identical to bs::daily_chart_svg.
void chart_svg(std::string& out, const std::vector<bs::DayPoint>& days, int width = 960, int height = 240);

}  // namespace bsf
