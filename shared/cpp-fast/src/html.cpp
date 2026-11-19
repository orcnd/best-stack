#include "bsf/html.hpp"

#include <charconv>
#include <cmath>
#include <cstdlib>
#include <format>
#include <iterator>

namespace bsf {

void esc(std::string& out, std::string_view s) {
  size_t run = 0;  // start of the pending run of characters that need no escaping
  for (size_t i = 0; i < s.size(); i++) {
    const char* rep;
    switch (s[i]) {
      case '&': rep = "&amp;"; break;
      case '<': rep = "&lt;"; break;
      case '>': rep = "&gt;"; break;
      case '"': rep = "&quot;"; break;
      case '\'': rep = "&#39;"; break;
      default: continue;
    }
    out.append(s.data() + run, i - run);
    out.append(rep);
    run = i + 1;
  }
  out.append(s.data() + run, s.size() - run);
}

void url_enc(std::string& out, std::string_view s) {
  static constexpr char kHex[] = "0123456789ABCDEF";
  for (unsigned char c : s) {
    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_' ||
        c == '.' || c == '~') {
      out += (char)c;
    } else {
      out += '%';
      out += kHex[c >> 4];
      out += kHex[c & 15];
    }
  }
}

void integer(std::string& out, long long v) {
  char buf[24];
  auto r = std::to_chars(buf, buf + sizeof buf, v);
  out.append(buf, r.ptr);
}

void grouped(std::string& out, long v) {
  char buf[24];
  unsigned long long u = v < 0 ? 0ULL - (unsigned long long)v : (unsigned long long)v;
  auto r = std::to_chars(buf, buf + sizeof buf, u);
  const int n = (int)(r.ptr - buf);
  if (v < 0) out += '-';
  for (int i = 0; i < n; i++) {
    if (i && (n - i) % 3 == 0) out += '.';
    out += buf[i];
  }
}

void money(std::string& out, long cents) {
  const long whole = cents / 100, frac = std::labs(cents % 100);
  out += "₺";
  grouped(out, whole);
  out += ',';
  out += (char)('0' + frac / 10);
  out += (char)('0' + frac % 10);
}

namespace {
/// "%.1f" with the decimal point written as a comma.
void one_decimal_comma(std::string& out, double v) {
  const size_t start = out.size();
  std::format_to(std::back_inserter(out), "{:.1f}", v);
  for (size_t i = start; i < out.size(); i++)
    if (out[i] == '.') out[i] = ',';
}

/// Up to 2 decimals, trailing zeros and dot stripped (as in chart.cpp).
void chart_num(std::string& out, double v) {
  char buf[48];
  auto end = std::format_to(buf, "{:.2f}", std::round(v * 100.0) / 100.0);
  while (end > buf && end[-1] == '0') --end;
  if (end > buf && end[-1] == '.') --end;
  std::string_view s(buf, end - buf);
  out.append(s == "-0" ? std::string_view("0") : s);
}
}  // namespace

void pct(std::string& out, double ratio) {
  out += '%';
  one_decimal_comma(out, std::round(ratio * 1000.0) / 10.0);
}

void rating(std::string& out, double r) {
  out += "★ ";
  one_decimal_comma(out, r);
}

void chart_svg(std::string& out, const std::vector<bs::DayPoint>& days, int width, int height) {
  long max = 0;
  for (auto& d : days) max = std::max(max, d.events);
  if (max == 0) max = 1;
  const double n = days.empty() ? 1.0 : (double)days.size();
  const double bar_w = width / n;
  std::format_to(std::back_inserter(out),
                 "<svg class=\"chart-svg\" viewBox=\"0 0 {0} {1}\" width=\"{0}\" height=\"{1}\" role=\"img\" "
                 "aria-label=\"Günlük olaylar\">",
                 width, height);
  for (size_t i = 0; i < days.size(); i++) {
    const long h = std::lround((double)days[i].events / (double)max * (height - 20));
    out += "<rect x=\"";
    chart_num(out, i * bar_w + 1);
    out += "\" y=\"";
    integer(out, height - h);
    out += "\" width=\"";
    chart_num(out, bar_w - 2);
    out += "\" height=\"";
    integer(out, h);
    out += "\" fill=\"#4f6bed\"/>";
  }
  out += "</svg>";
}

}  // namespace bsf
