// Fast-core handlers: same routes, validation, SQL calls (and call order) as shared/cpp/src/handlers_*.cpp,
// but pages are written straight into one reserved std::string and API bodies are serialized by glaze.
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <string>

#include "bs/db.hpp"
#include "bs/handlers.hpp"
#include "bs/render.hpp"
#include "bsf/alloc.hpp"
#include "bsf/json.hpp"
#include "pages.hpp"

namespace bs {
// Templates are compiled into pages.cpp; bs::init (http.cpp) still calls this at startup, which is
// where the allocator check runs (BS_ALLOC_CHECK=1 prints the report).
void load_templates(const std::string&) { bsf::mimalloc_active(std::getenv("BS_ALLOC_CHECK") != nullptr); }
}  // namespace bs

namespace bs::h {
using namespace bs::detail;
using bsf::pages::Fe;
using bsf::pages::Layout;

namespace {

// Initial capacities: roughly the size of a typical response, so the body is not regrown while writing.
constexpr size_t kPageCap = 32 * 1024, kBoardCap = 192 * 1024, kSmallCap = 4 * 1024;

Resp html_out(std::string body, int status = 200) { return Resp{status, "text/html; charset=utf-8", std::move(body), {}}; }
Resp json_out(std::string body, int status = 200) { return Resp{status, "application/json", std::move(body), {}}; }

template <class T>
Resp json_of(const T& v, int status = 200) {
  std::string out;
  bsf::json::write(out, v);
  return json_out(std::move(out), status);
}

int cart_count(const Req& r) {
  auto sid = existing_session(r);
  return sid.empty() ? 0 : thread_db().cart(sid).count;
}

// ---- copied from shared/cpp/src/view.cpp (validation helpers, not rendering)

// Well-formed UTF-8 (no overlongs, surrogates or code points > U+10FFFF).
bool valid_utf8(const std::string& s) {
  size_t i = 0;
  while (i < s.size()) {
    unsigned char c = s[i];
    int n = c < 0x80 ? 0 : (c >> 5) == 0x6 ? 1 : (c >> 4) == 0xE ? 2 : (c >> 3) == 0x1E ? 3 : -1;
    if (n < 0 || i + n >= s.size()) return false;
    uint32_t cp = n == 0 ? c : c & (0x3F >> n);
    for (int k = 1; k <= n; k++) {
      unsigned char cc = s[i + k];
      if ((cc & 0xC0) != 0x80) return false;
      cp = (cp << 6) | (cc & 0x3F);
    }
    if ((n == 1 && cp < 0x80) || (n == 2 && cp < 0x800) || (n == 3 && (cp < 0x10000 || cp > 0x10FFFF)) ||
        (cp >= 0xD800 && cp <= 0xDFFF))
      return false;
    i += n + 1;
  }
  return true;
}

std::optional<std::string> valid_title(const std::string& raw) {
  if (!valid_utf8(raw)) return std::nullopt;
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
  auto ref = get(r.headers, "referer");
  if (ref.rfind("http://", 0) == 0 || ref.rfind("https://", 0) == 0) {
    auto slash = ref.find('/', ref.find("//") + 2);
    ref = slash == std::string::npos ? "/" : ref.substr(slash);
  }
  if (ref.empty() || ref[0] != '/' || ref.rfind("//", 0) == 0) return fallback;
  return ref;
}

// ---- catalog

ProductQuery parse_query(const Req& r) {
  ProductQuery q;
  q.q = get(r.query, "q");
  q.cat = get(r.query, "cat");
  auto s = get(r.query, "sort");
  if (s == "price_asc" || s == "price_desc") q.sort = s;
  q.page = to_int(get(r.query, "page")).value_or(1);
  return q;
}

Resp bad_request_json() { return json_out(R"({"error":"bad_request"})", 400); }

// ---- kanban

/// Board JSON always lists every column, empty ones included.
void fill_columns(Board& b) {
  for (auto& c : kColumns()) b.columns[c];
}

Resp board_fragment(const std::string& fe, int board_id) {
  auto& db = thread_db();
  auto b = db.board(board_id);
  if (!b) return not_found_html();
  auto all = db.boards();
  std::string o;
  o.reserve(kBoardCap);
  bsf::pages::board(o, Fe(fe), {*b, all});
  return html_out(std::move(o));
}

Resp unprocessable() { return json_out(R"({"error":"invalid_title"})", 422); }

/// 7|30|90; absent → 30; anything else → nullopt (400).
std::optional<int> parse_range(const Req& r) {
  auto it = r.query.find("range");
  if (it == r.query.end() || it->second.empty()) return 30;
  if (it->second == "7") return 7;
  if (it->second == "30") return 30;
  if (it->second == "90") return 90;
  return std::nullopt;
}

}  // namespace

// ================================================================ catalog

Resp catalog_page(const Req& r) {
  auto fe = get(r.params, "fe");
  if (!valid_fe(fe)) return not_found_html();
  auto& db = thread_db();
  auto q = parse_query(r);
  auto page = db.products(q);
  const int count = cart_count(r);
  auto cats = db.categories();
  std::string o;
  o.reserve(kPageCap);
  const bsf::pages::CatalogView v{q, cats, page};
  if (is_htmx(r))
    bsf::pages::catalog_results(o, Fe(fe), v);
  else
    bsf::pages::catalog(o, Fe(fe), {"catalog", "Ürün Kataloğu", "Best Stack demo ürün kataloğu", count}, v);
  return html_out(std::move(o));
}

Resp product_page(const Req& r) {
  auto fe = get(r.params, "fe");
  auto id = to_int(get(r.params, "id"));
  if (!valid_fe(fe) || !id) return not_found_html();
  auto p = thread_db().product(*id);
  if (!p) return not_found_html();
  const int count = cart_count(r);
  std::string o;
  o.reserve(kSmallCap * 2);
  bsf::pages::product(o, Fe(fe), {"catalog", p->name, p->description, count}, *p);
  return html_out(std::move(o));
}

Resp cart_page(const Req& r) {
  auto fe = get(r.params, "fe");
  if (!valid_fe(fe)) return not_found_html();
  auto sid = existing_session(r);
  const int count = cart_count(r);
  auto c = sid.empty() ? Cart{} : thread_db().cart(sid);
  std::string o;
  o.reserve(kPageCap / 2);
  bsf::pages::cart(o, Fe(fe), {"catalog", "Sepet", "Alışveriş sepeti", count}, c);
  return html_out(std::move(o));
}

Resp cart_add_form(const Req& r) {
  auto fe = get(r.params, "fe");
  if (!valid_fe(fe)) return not_found_html();
  auto pid = to_int(get(r.form, "product_id"));
  auto qty = to_int(get(r.form, "qty")).value_or(1);
  if (!pid || qty < 1 || qty > 99) return html_out("Geçersiz istek", 400);
  auto& db = thread_db();
  if (!db.product(*pid)) return not_found_html();
  Resp out;
  auto sid = session_id(r, out);
  db.cart_add(sid, *pid, qty);
  if (is_htmx(r)) {
    out.body.reserve(256);
    bsf::pages::badge(out.body, Fe(fe), db.cart(sid).count);
    return out;
  }
  auto res = redirect(safe_referer(r, "/" + fe + "/catalog"));
  for (auto& hd : out.headers) res.headers.push_back(hd);
  return res;
}

Resp cart_item_form(const Req& r) {
  auto fe = get(r.params, "fe");
  auto item = to_int(get(r.params, "item_id"));
  auto qty = to_int(get(r.form, "qty"));
  if (!valid_fe(fe) || !item) return not_found_html();
  if (!qty || *qty > 99) return html_out("Geçersiz adet", 400);
  auto sid = existing_session(r);
  auto& db = thread_db();
  if (sid.empty() || !db.cart_set_qty(sid, *item, *qty)) return not_found_html();
  if (is_htmx(r)) {
    auto c = db.cart(sid);
    std::string o;
    o.reserve(kPageCap / 2);
    bsf::pages::cart_body(o, Fe(fe), c, true);
    return html_out(std::move(o));
  }
  return redirect("/" + fe + "/catalog/cart");
}

Resp api_products(const Req& r) {
  auto& db = thread_db();
  auto page = db.products(parse_query(r));
  bsf::json::ProductsResp out{std::move(page.items), page.total, page.page, page.pages, db.categories()};
  return json_of(out);
}

Resp api_product(const Req& r) {
  auto id = to_int(get(r.params, "id"));
  if (!id) return not_found_json();
  auto p = thread_db().product(*id);
  return p ? json_of(*p) : not_found_json();
}

Resp api_cart_get(const Req& r) {
  auto sid = existing_session(r);
  return json_of(sid.empty() ? Cart{} : thread_db().cart(sid));
}

Resp api_cart_add(const Req& r) {
  auto body = bsf::json::Body::parse(r.body);
  if (!body || !body->contains("product_id")) return bad_request_json();
  auto pid64 = body->integer("product_id");  // int64: a 32-bit read would silently wrap 2^32+1 to 1
  if (!pid64) return bad_request_json();
  int64_t qty64 = 1;
  if (body->contains("qty")) {
    auto q = body->integer("qty");
    if (!q) return bad_request_json();
    qty64 = *q;
  }
  if (qty64 < 1 || qty64 > 99) return bad_request_json();
  if (*pid64 < 1 || *pid64 > 999999999) return not_found_json();
  const int pid = static_cast<int>(*pid64), qty = static_cast<int>(qty64);
  auto& db = thread_db();
  if (!db.product(pid)) return not_found_json();
  Resp out;
  auto sid = session_id(r, out);
  db.cart_add(sid, pid, qty);
  auto res = json_of(db.cart(sid), 201);
  res.headers = out.headers;
  return res;
}

Resp api_cart_patch(const Req& r) {
  auto item = to_int(get(r.params, "item_id"));
  auto body = bsf::json::Body::parse(r.body);
  if (!item) return not_found_json();
  if (!body) return bad_request_json();
  auto qty64 = body->integer("qty");
  if (!qty64 || *qty64 < 0 || *qty64 > 99) return bad_request_json();
  const int qty = static_cast<int>(*qty64);
  auto sid = existing_session(r);
  auto& db = thread_db();
  if (sid.empty() || !db.cart_set_qty(sid, *item, qty)) return not_found_json();
  return json_of(db.cart(sid));
}

Resp api_cart_delete(const Req& r) {
  auto item = to_int(get(r.params, "item_id"));
  auto sid = existing_session(r);
  auto& db = thread_db();
  if (!item || sid.empty() || !db.cart_set_qty(sid, *item, 0)) return not_found_json();
  return json_of(db.cart(sid));
}

// ================================================================ kanban

Resp kanban_page(const Req& r) {
  auto fe = get(r.params, "fe");
  auto id = to_int(get(r.params, "board_id"));
  if (!valid_fe(fe) || !id) return not_found_html();
  auto& db = thread_db();
  auto b = db.board(*id);
  if (!b) return not_found_html();
  const int count = cart_count(r);
  auto all = db.boards();
  const std::string title = b->name + " · Kanban";
  std::string o;
  o.reserve(kBoardCap);
  bsf::pages::kanban(o, Fe(fe), {"kanban", title, "Kanban panosu", count}, {*b, all});
  return html_out(std::move(o));
}

Resp card_create_form(const Req& r) {
  auto fe = get(r.params, "fe");
  auto id = to_int(get(r.params, "board_id"));
  if (!valid_fe(fe) || !id) return not_found_html();
  auto title = valid_title(get(r.form, "title"));
  if (!title) return html_out("Geçersiz başlık", 422);
  auto col = get(r.form, "column");
  if (col.empty()) col = "backlog";
  auto& db = thread_db();
  if (!db.board(*id)) return not_found_html();
  if (!db.card_create(*id, *title, col)) return html_out("Geçersiz sütun", 422);
  if (is_htmx(r)) return board_fragment(fe, *id);
  return redirect("/" + fe + "/kanban/" + std::to_string(*id));
}

Resp card_update_form(const Req& r) {
  auto fe = get(r.params, "fe");
  auto id = to_int(get(r.params, "id"));
  if (!valid_fe(fe) || !id) return not_found_html();
  auto& db = thread_db();
  auto card = db.card_update(*id, std::nullopt, std::nullopt);
  if (!card) return not_found_html();
  if (get(r.form, "delete") == "1") {
    db.card_delete(*id);
  } else {
    std::optional<std::string> title, move;
    if (r.form.count("title")) {
      title = valid_title(get(r.form, "title"));
      if (!title) return html_out("Geçersiz başlık", 422);
    }
    auto mv = get(r.form, "move");
    if (mv == "left" || mv == "right") move = mv;
    db.card_update(*id, title, move);
  }
  if (is_htmx(r)) return board_fragment(fe, card->board_id);
  return redirect("/" + fe + "/kanban/" + std::to_string(card->board_id));
}

Resp api_boards(const Req&) { return json_of(thread_db().boards()); }

Resp api_board(const Req& r) {
  auto id = to_int(get(r.params, "id"));
  if (!id) return not_found_json();
  auto b = thread_db().board(*id);
  if (!b) return not_found_json();
  fill_columns(*b);
  return json_of(*b);
}

Resp api_card_create(const Req& r) {
  auto id = to_int(get(r.params, "id"));
  if (!id) return not_found_json();
  auto body = bsf::json::Body::parse(r.body);
  if (!body) return bad_request_json();
  auto raw_title = body->string("title");
  if (!raw_title) return unprocessable();
  auto title = valid_title(*raw_title);
  if (!title) return unprocessable();
  std::string col = body->string("column").value_or("backlog");
  const auto& cols = kColumns();
  if (std::find(cols.begin(), cols.end(), col) == cols.end()) return json_out(R"({"error":"invalid_column"})", 422);
  auto c = thread_db().card_create(*id, *title, col);
  return c ? json_of(*c, 201) : not_found_json();
}

Resp api_card_patch(const Req& r) {
  auto id = to_int(get(r.params, "id"));
  if (!id) return not_found_json();
  auto body = bsf::json::Body::parse(r.body);
  if (!body) return bad_request_json();
  std::optional<std::string> title, move;
  if (body->contains("title")) {
    auto raw = body->string("title");
    if (!raw) return unprocessable();
    title = valid_title(*raw);
    if (!title) return unprocessable();
  }
  if (body->contains("move")) {
    auto m = body->string("move");
    if (!m || (*m != "left" && *m != "right")) return bad_request_json();
    move = m;
  }
  auto c = thread_db().card_update(*id, title, move);
  return c ? json_of(*c) : not_found_json();
}

Resp api_card_delete(const Req& r) {
  auto id = to_int(get(r.params, "id"));
  if (!id || !thread_db().card_delete(*id)) return not_found_json();
  return Resp{204, "application/json", "", {}};
}

// ================================================================ dashboard

Resp dashboard_page(const Req& r) {
  auto fe = get(r.params, "fe");
  if (!valid_fe(fe)) return not_found_html();
  auto range = parse_range(r);
  if (!range) return html_out("Geçersiz aralık", 400);
  auto d = thread_db().dashboard(*range);
  const int count = cart_count(r);
  std::string o;
  o.reserve(kPageCap / 2);
  const bsf::pages::DashboardView v{*range, d};
  if (is_htmx(r))
    bsf::pages::dashboard_body(o, Fe(fe), v);
  else
    bsf::pages::dashboard(o, Fe(fe), {"dashboard", "Analitik", "Analitik dashboard", count}, v);
  return html_out(std::move(o));
}

Resp api_dashboard(const Req& r) {
  auto range = parse_range(r);
  if (!range) return json_out(R"({"error":"invalid_range"})", 400);
  return json_of(thread_db().dashboard(*range));
}

}  // namespace bs::h
