#include "bs/render.hpp"

#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <unordered_map>

#include <inja/inja.hpp>

namespace fs = std::filesystem;

namespace bs {
namespace {
std::unique_ptr<inja::Environment> g_env;
std::unordered_map<std::string, inja::Template> g_templates;

std::string group_thousands(long v) {
  std::string digits = std::to_string(v < 0 ? -v : v);
  std::string out;
  int n = 0;
  for (auto it = digits.rbegin(); it != digits.rend(); ++it) {
    if (n && n % 3 == 0) out.insert(out.begin(), '.');
    out.insert(out.begin(), *it);
    n++;
  }
  return (v < 0 ? "-" : "") + out;
}
}  // namespace

std::string fmt_int(long v) { return group_thousands(v); }

std::string fmt_money(long cents) {
  long whole = cents / 100, frac = std::labs(cents % 100);
  char buf[4];
  std::snprintf(buf, sizeof buf, "%02ld", frac);
  return "₺" + group_thousands(whole) + "," + buf;
}

std::string fmt_pct(double ratio) {
  char buf[32];
  std::snprintf(buf, sizeof buf, "%.1f", std::round(ratio * 1000.0) / 10.0);
  std::string s = buf;
  for (auto& c : s)
    if (c == '.') c = ',';
  return "%" + s;
}

std::string fmt_rating(double r) {
  char buf[16];
  std::snprintf(buf, sizeof buf, "%.1f", r);
  std::string s = buf;
  for (auto& c : s)
    if (c == '.') c = ',';
  return "★ " + s;
}

void load_templates(const std::string& root) {
  auto dir = fs::path(root) / "shared" / "cpp" / "templates";
  g_env = std::make_unique<inja::Environment>(dir.string() + "/");
  g_env->set_trim_blocks(true);
  g_env->set_lstrip_blocks(true);
  g_env->set_line_statement("%%%%");  // effectively disable line statements
  g_templates.clear();
  for (auto& e : fs::directory_iterator(dir)) {
    if (e.path().extension() != ".html") continue;
    auto name = e.path().stem().string();
    g_templates.emplace(name, g_env->parse_template(e.path().filename().string()));
  }
}

std::string render(std::string_view fe, std::string_view tmpl, const nlohmann::json& data) {
  auto it = g_templates.find(std::string(tmpl));
  if (it == g_templates.end()) throw std::runtime_error("unknown template: " + std::string(tmpl));
  nlohmann::json d = data;
  d["fe"] = std::string(fe);
  d["base"] = "/" + std::string(fe);
  return g_env->render(it->second, d);
}

}  // namespace bs
