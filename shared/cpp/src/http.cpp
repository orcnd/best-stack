#include "bs/http.hpp"

#include <array>
#include <filesystem>
#include <fstream>
#include <random>
#include <shared_mutex>
#include <sstream>
#include <unordered_map>

#include "bs/db.hpp"
#include "bs/handlers.hpp"
#include "bs/render.hpp"

namespace fs = std::filesystem;

namespace bs {
namespace {
fs::path g_static_dir;

struct StaticCache {
  std::shared_mutex mu;
  std::unordered_map<std::string, std::shared_ptr<Resp>> files;
} g_static;

std::string mime_for(const fs::path& p) {
  static const std::unordered_map<std::string, std::string> m{
      {".css", "text/css; charset=utf-8"},   {".js", "text/javascript; charset=utf-8"},
      {".mjs", "text/javascript; charset=utf-8"}, {".svg", "image/svg+xml"},
      {".png", "image/png"},                 {".webp", "image/webp"},
      {".wasm", "application/wasm"},         {".json", "application/json"},
      {".html", "text/html; charset=utf-8"}, {".ico", "image/x-icon"}};
  auto it = m.find(p.extension().string());
  return it == m.end() ? "application/octet-stream" : it->second;
}
}  // namespace

std::string url_decode(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size(); i++) {
    char c = s[i];
    if (c == '+') {
      out += ' ';
    } else if (c == '%' && i + 2 < s.size() && std::isxdigit((unsigned char)s[i + 1]) &&
               std::isxdigit((unsigned char)s[i + 2])) {
      out += (char)std::stoi(s.substr(i + 1, 2), nullptr, 16);
      i += 2;
    } else {
      out += c;
    }
  }
  return out;
}

std::map<std::string, std::string> parse_urlencoded(const std::string& s) {
  std::map<std::string, std::string> out;
  size_t start = 0;
  while (start <= s.size()) {
    auto end = s.find('&', start);
    if (end == std::string::npos) end = s.size();
    auto part = s.substr(start, end - start);
    if (!part.empty()) {
      auto eq = part.find('=');
      if (eq == std::string::npos)
        out[url_decode(part)] = "";
      else
        out[url_decode(part.substr(0, eq))] = url_decode(part.substr(eq + 1));
    }
    start = end + 1;
  }
  return out;
}

std::map<std::string, std::string> parse_cookies(const std::string& header) {
  std::map<std::string, std::string> out;
  std::stringstream ss(header);
  std::string part;
  while (std::getline(ss, part, ';')) {
    auto b = part.find_first_not_of(' ');
    if (b == std::string::npos) continue;
    part = part.substr(b);
    auto eq = part.find('=');
    if (eq != std::string::npos) out[part.substr(0, eq)] = part.substr(eq + 1);
  }
  return out;
}

bool match(const std::string& pattern, const std::string& path, std::map<std::string, std::string>& params) {
  size_t pi = 0, si = 0;
  while (pi < pattern.size() && si <= path.size()) {
    if (pattern[pi] == '{') {
      auto close = pattern.find('}', pi);
      auto name = pattern.substr(pi + 1, close - pi - 1);
      auto seg_end = path.find('/', si);
      if (seg_end == std::string::npos) seg_end = path.size();
      if (seg_end == si) return false;
      params[name] = path.substr(si, seg_end - si);
      si = seg_end;
      pi = close + 1;
    } else {
      if (si >= path.size() || pattern[pi] != path[si]) return false;
      pi++;
      si++;
    }
  }
  return pi == pattern.size() && si == path.size();
}

std::optional<Resp> serve_static(const std::string& rel_path) {
  {
    std::shared_lock lk(g_static.mu);
    auto it = g_static.files.find(rel_path);
    if (it != g_static.files.end()) return *it->second;
  }
  if (rel_path.empty() || rel_path.find("..") != std::string::npos || rel_path[0] == '/') return std::nullopt;
  auto full = (g_static_dir / rel_path).lexically_normal();
  auto rel = full.lexically_relative(g_static_dir);
  if (rel.empty() || *rel.begin() == "..") return std::nullopt;
  std::ifstream f(full, std::ios::binary);
  if (!f || fs::is_directory(full)) return std::nullopt;
  auto r = std::make_shared<Resp>();
  r->content_type = mime_for(full);
  r->body.assign(std::istreambuf_iterator<char>(f), {});
  r->headers.emplace_back("Cache-Control", "public, max-age=31536000");
  std::unique_lock lk(g_static.mu);
  g_static.files.emplace(rel_path, r);
  return *r;
}

void init(const Config& cfg) {
  set_db_path(cfg.db_path);
  g_static_dir = fs::path(cfg.root) / "shared" / "static";
  load_templates(cfg.root);
}

const std::vector<Route>& routes() {
  static const std::vector<Route> r{
      {"GET", "/healthz", h::healthz},
      {"GET", "/api/catalog/products", h::api_products},
      {"GET", "/api/catalog/products/{id}", h::api_product},
      {"GET", "/api/catalog/cart", h::api_cart_get},
      {"POST", "/api/catalog/cart", h::api_cart_add},
      {"PATCH", "/api/catalog/cart/{item_id}", h::api_cart_patch},
      {"DELETE", "/api/catalog/cart/{item_id}", h::api_cart_delete},
      {"GET", "/api/kanban/boards", h::api_boards},
      {"GET", "/api/kanban/boards/{id}", h::api_board},
      {"POST", "/api/kanban/boards/{id}/cards", h::api_card_create},
      {"PATCH", "/api/kanban/cards/{id}", h::api_card_patch},
      {"DELETE", "/api/kanban/cards/{id}", h::api_card_delete},
      {"GET", "/api/dashboard", h::api_dashboard},
      {"GET", "/{fe}/catalog", h::catalog_page},
      {"GET", "/{fe}/catalog/p/{id}", h::product_page},
      {"GET", "/{fe}/catalog/cart", h::cart_page},
      {"POST", "/{fe}/catalog/cart", h::cart_add_form},
      {"POST", "/{fe}/catalog/cart/{item_id}", h::cart_item_form},
      {"GET", "/{fe}/kanban/{board_id}", h::kanban_page},
      {"POST", "/{fe}/kanban/{board_id}/cards", h::card_create_form},
      {"POST", "/{fe}/kanban/cards/{id}", h::card_update_form},
      {"GET", "/{fe}/dashboard", h::dashboard_page},
  };
  return r;
}

namespace detail {
bool valid_fe(const std::string& fe) { return fe == "htmx" || fe == "alpine" || fe == "vanilla"; }
bool is_htmx(const Req& r) { return get(r.headers, "hx-request") == "true"; }

std::optional<int> to_int(const std::string& s) {
  if (s.empty() || s.size() > 9) return std::nullopt;
  int v = 0;
  for (char c : s) {
    if (c < '0' || c > '9') return std::nullopt;
    v = v * 10 + (c - '0');
  }
  return v;
}

std::string get(const std::map<std::string, std::string>& m, const std::string& k) {
  auto it = m.find(k);
  return it == m.end() ? std::string() : it->second;
}

static bool valid_sid(const std::string& s) {
  if (s.size() != 16) return false;
  for (char c : s)
    if (!std::isalnum((unsigned char)c)) return false;
  return true;
}

std::string existing_session(const Req& r) {
  auto c = get(r.cookies, "bs_sid");
  if (valid_sid(c)) return c;
  auto h = get(r.headers, "x-session-id");
  if (valid_sid(h)) return h;
  return "";
}

std::string session_id(const Req& r, Resp& out) {
  auto s = existing_session(r);
  if (!s.empty()) return s;
  thread_local std::mt19937_64 rng{std::random_device{}()};
  static const char* hex = "0123456789abcdef";
  std::string id(16, '0');
  for (auto& c : id) c = hex[rng() & 15];
  out.headers.emplace_back("Set-Cookie", "bs_sid=" + id + "; Path=/; SameSite=Lax; HttpOnly");
  return id;
}

Resp html(std::string body, int status) { return Resp{status, "text/html; charset=utf-8", std::move(body), {}}; }
Resp json_resp(const std::string& body, int status) { return Resp{status, "application/json", body, {}}; }
Resp not_found_html() { return html("<!doctype html><title>404</title><h1>Bulunamadı</h1>", 404); }
Resp not_found_json() { return json_resp(R"({"error":"not_found"})", 404); }
Resp redirect(const std::string& location) {
  Resp r{303, "text/plain", "", {}};
  r.headers.emplace_back("Location", location);
  return r;
}
}  // namespace detail

namespace h {
Resp healthz(const Req&) { return Resp{200, "text/plain", "ok", {}}; }
Resp static_file(const Req& r) {
  auto res = serve_static(r.path.size() > 8 ? r.path.substr(8) : "");
  return res ? *res : Resp{404, "text/plain", "not found", {}};
}
}  // namespace h

}  // namespace bs
