#include "view.hpp"

#include <cstdio>

namespace bs::view {

std::string html_escape(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    switch (c) {
      case '&': out += "&amp;"; break;
      case '<': out += "&lt;"; break;
      case '>': out += "&gt;"; break;
      case '"': out += "&quot;"; break;
      case '\'': out += "&#39;"; break;
      default: out += c;
    }
  }
  return out;
}

std::string url_encode(const std::string& s) {
  std::string out;
  char buf[4];
  for (unsigned char c : s) {
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      out += (char)c;
    } else {
      std::snprintf(buf, sizeof buf, "%%%02X", c);
      out += buf;
    }
  }
  return out;
}

void escape_all(json& j) {
  if (j.is_string()) {
    j = html_escape(j.get<std::string>());
  } else if (j.is_array()) {
    for (auto& v : j) escape_all(v);
  } else if (j.is_object()) {
    for (auto it = j.begin(); it != j.end(); ++it)
      if (it.key().rfind("raw_", 0) != 0) escape_all(it.value());
  }
}

json page(const Req& r, const std::string& nav, const std::string& title, const std::string& description) {
  auto sid = detail::existing_session(r);
  int count = sid.empty() ? 0 : thread_db().cart(sid).count;
  return {{"nav", nav}, {"title", title}, {"description", description}, {"cart_count", count}, {"fragment", false}};
}

std::optional<std::string> valid_title(const std::string& raw) {
  auto b = raw.find_first_not_of(" \t\r\n");
  if (b == std::string::npos) return std::nullopt;
  auto e = raw.find_last_not_of(" \t\r\n");
  auto t = raw.substr(b, e - b + 1);
  size_t cps = 0;
  for (unsigned char c : t)
    if ((c & 0xC0) != 0x80) cps++;
  if (cps < 1 || cps > (size_t)kMaxTitle) return std::nullopt;
  return t;
}

std::string safe_referer(const Req& r, const std::string& fallback) {
  auto ref = detail::get(r.headers, "referer");
  if (ref.rfind("http://", 0) == 0 || ref.rfind("https://", 0) == 0) {
    auto slash = ref.find('/', ref.find("//") + 2);
    ref = slash == std::string::npos ? "/" : ref.substr(slash);
  }
  if (ref.empty() || ref[0] != '/' || ref.rfind("//", 0) == 0) return fallback;
  return ref;
}

json cart_vm(const Cart& c) {
  json items = json::array();
  for (auto& it : c.items)
    items.push_back({{"id", it.id},
                     {"product_id", it.product.id},
                     {"name", it.product.name},
                     {"price", fmt_money(it.product.price_cents)},
                     {"qty", it.qty},
                     {"line", fmt_money((long)it.qty * it.product.price_cents)}});
  return {{"items", items}, {"total", fmt_money(c.total_cents)}, {"empty", c.items.empty()}, {"cart_count", c.count}};
}

json board_vm(const Board& b, const std::vector<BoardSummary>& all) {
  static const std::map<std::string, std::string> labels{
      {"backlog", "Backlog"}, {"todo", "Yapılacak"}, {"doing", "Yapılıyor"}, {"done", "Bitti"}};
  json tabs = json::array();
  for (auto& s : all) tabs.push_back({{"id", s.id}, {"name", s.name}, {"active", s.id == b.id}});
  json cols = json::array();
  const auto& keys = kColumns();
  for (size_t i = 0; i < keys.size(); i++) {
    json cards = json::array();
    auto it = b.columns.find(keys[i]);
    if (it != b.columns.end()) {
      for (auto& c : it->second) {
        std::string lbl;
        for (size_t k = 0; k < c.labels.size(); k++) lbl += (k ? ", " : "") + c.labels[k];
        cards.push_back({{"id", c.id}, {"title", c.title}, {"labels", lbl}, {"assignee", c.assignee},
                         {"due", c.due_date}, {"first", i == 0}, {"last", i + 1 == keys.size()}});
      }
    }
    cols.push_back({{"key", keys[i]}, {"label", labels.at(keys[i])}, {"count", cards.size()}, {"cards", cards}});
  }
  return {{"board", {{"id", b.id}, {"name", b.name}}}, {"boards", tabs}, {"columns", cols}};
}

}  // namespace bs::view
