#pragma once
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace bs {
/// Load and parse all templates under <root>/shared/cpp/templates (call once).
void load_templates(const std::string& root);
/// Render template "<fe>/<tmpl>.html" with data. Thread-safe after load_templates().
std::string render(std::string_view fe, std::string_view tmpl, const nlohmann::json& data);

/// tr-TR formatting helpers (also exposed to templates as callbacks).
std::string fmt_money(long cents);       // ₺1.234,50
std::string fmt_int(long v);             // 12.345
std::string fmt_pct(double ratio);       // %12,3
std::string fmt_rating(double r);        // ★ 4,5
}  // namespace bs
