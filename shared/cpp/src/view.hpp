#pragma once
// Internal helpers shared by handler files (view models, escaping, validation).
#include <string>

#include "bs/db.hpp"
#include "bs/handlers.hpp"
#include "bs/json.hpp"
#include "bs/render.hpp"

namespace bs::view {

std::string html_escape(const std::string& s);
std::string url_encode(const std::string& s);
/// Escape every string in place except values under keys starting with "raw_".
void escape_all(json& j);

/// Common page fields: title, description, nav, cart_count.
json page(const Req& r, const std::string& nav, const std::string& title, const std::string& description);

/// Render with escaping applied; fe must already be validated.
inline std::string view(const std::string& fe, const std::string& tmpl, json data) {
  escape_all(data);
  return render(fe, tmpl, data);
}

/// Trimmed title if 1..kMaxTitle code points, else nullopt.
std::optional<std::string> valid_title(const std::string& raw);

/// Path part of a Referer header if same-site-ish ("/..."), else fallback.
std::string safe_referer(const Req& r, const std::string& fallback);

json cart_vm(const Cart& c);
json board_vm(const Board& b, const std::vector<BoardSummary>& all);

}  // namespace bs::view
