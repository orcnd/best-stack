#include <doctest/doctest.h>

#include <filesystem>
#include <regex>
#include <string>
#include <unistd.h>

#include "bs/db.hpp"
#include "bs/http.hpp"
#include "bs/json.hpp"

namespace fs = std::filesystem;

namespace {

void ensure_init() {
  static bool done = false;
  if (done) return;
  auto dir = fs::temp_directory_path() / "bs-tests";
  fs::create_directories(dir);
  auto p = dir / ("handlers-" + std::to_string(::getpid()) + ".sqlite");
  for (auto s : {"", "-wal", "-shm"}) fs::remove(p.string() + s);
  fs::copy_file(fs::path(BS_ROOT) / "data/pristine.sqlite", p);
  bs::init({BS_ROOT, p.string(), 1});
  done = true;
}

struct CallOpts {
  std::string body;
  std::map<std::string, std::string> headers;
  std::map<std::string, std::string> cookies;
};

bs::Resp call(const std::string& method, const std::string& target, CallOpts o = {}) {
  ensure_init();
  bs::Req r;
  r.method = method;
  auto q = target.find('?');
  r.path = target.substr(0, q);
  if (q != std::string::npos) r.query = bs::parse_urlencoded(target.substr(q + 1));
  r.headers = o.headers;
  r.cookies = o.cookies;
  r.body = o.body;
  if (method == "POST" && !o.body.empty() && o.body[0] != '{') r.form = bs::parse_urlencoded(o.body);
  for (auto& rt : bs::routes()) {
    std::map<std::string, std::string> params;
    if (method == rt.method && bs::match(rt.pattern, r.path, params)) {
      r.params = params;
      return rt.fn(r);
    }
  }
  return bs::Resp{404, "text/plain", "no route"};
}

// Server-rendered part only: Alpine's client-side <template x-if="ready"> blocks are not rendered content.
int count(const std::string& body, const std::string& needle) {
  std::string hay = body;
  for (auto p = hay.find("<template x-if=\"ready\">"); p != std::string::npos; p = hay.find("<template x-if=\"ready\">")) {
    auto end = hay.rfind("</template>");
    hay.erase(p, (end == std::string::npos ? hay.size() : end + 11) - p);
  }
  int n = 0;
  for (size_t p = hay.find(needle); p != std::string::npos; p = hay.find(needle, p + 1)) n++;
  return n;
}

std::string header(const bs::Resp& r, const std::string& k) {
  for (auto& [hk, hv] : r.headers)
    if (hk == k) return hv;
  return "";
}

}  // namespace

TEST_CASE("handlers: catalog_page_all_fes") {
  for (auto fe : {"htmx", "alpine", "vanilla"}) {
    auto r = call("GET", std::string("/") + fe + "/catalog");
    CHECK(r.status == 200);
    CHECK(count(r.body, "data-testid=\"product-card\"") == 24);
    CHECK(r.body.find("<title>Ürün Kataloğu · Best Stack</title>") != std::string::npos);
    CHECK(r.body.find("<meta name=\"description\"") != std::string::npos);
    CHECK(r.body.find("/static/app.css") != std::string::npos);
    CHECK(r.body.find(std::string("href=\"/") + fe + "/catalog/p/") != std::string::npos);
  }
}

TEST_CASE("handlers: detail cart kanban dashboard pages render for all fes") {
  for (std::string fe : {"htmx", "alpine", "vanilla"}) {
    CHECK(call("GET", "/" + fe + "/catalog/p/1").body.find("data-testid=\"product-detail\"") != std::string::npos);
    CHECK(call("GET", "/" + fe + "/catalog/cart").status == 200);
    auto k = call("GET", "/" + fe + "/kanban/1");
    CHECK(k.status == 200);
    CHECK(count(k.body, "data-testid=\"card\"") == 100);
    CHECK(k.body.find("data-testid=\"column-doing\"") != std::string::npos);
    auto d = call("GET", "/" + fe + "/dashboard?range=7");
    CHECK(d.status == 200);
    CHECK(d.body.find("<svg class=\"chart-svg\"") != std::string::npos);
    CHECK(d.body.find("data-testid=\"kpi-events\"") != std::string::npos);
  }
}

TEST_CASE("handlers: htmx_fragment") {
  auto r = call("GET", "/htmx/catalog?q=lamp", {.headers = {{"hx-request", "true"}}});
  CHECK(r.status == 200);
  CHECK(r.body.find("<html") == std::string::npos);
  CHECK(r.body.find("data-testid=\"product-grid\"") != std::string::npos);
  auto d = call("GET", "/htmx/dashboard?range=7", {.headers = {{"hx-request", "true"}}});
  CHECK(d.body.find("<html") == std::string::npos);
  CHECK(d.body.find("data-testid=\"kpi-events\"") != std::string::npos);
}

TEST_CASE("handlers: catalog_page_escapes_query") {
  auto r = call("GET", "/vanilla/catalog?q=%3Cscript%3Ex%3C%2Fscript%3E");
  CHECK(r.status == 200);
  CHECK(r.body.find("&lt;script&gt;") != std::string::npos);
  CHECK(r.body.find("<script>x") == std::string::npos);
}

TEST_CASE("handlers: invalid_params") {
  CHECK(call("GET", "/vanilla/catalog/p/abc").status == 404);
  CHECK(call("GET", "/vanilla/catalog/p/999999").status == 404);
  CHECK(call("GET", "/xyz/catalog").status == 404);
  CHECK(call("GET", "/vanilla/kanban/99").status == 404);
  CHECK(call("GET", "/api/dashboard?range=13").status == 400);
  CHECK(call("GET", "/vanilla/dashboard?range=13").status == 400);
  CHECK(call("GET", "/api/catalog/products/abc").status == 404);
  auto p = call("GET", "/vanilla/catalog?page=9999");
  CHECK(p.status == 200);
  CHECK(p.body.find("Sayfa 84 / 84") != std::string::npos);
  CHECK(call("GET", "/vanilla/catalog?page=abc").status == 200);
}

TEST_CASE("handlers: cart_post_redirects") {
  auto r = call("POST", "/vanilla/catalog/cart", {.body = "product_id=3&qty=1", .headers = {{"referer", "/vanilla/catalog?page=2"}}});
  CHECK(r.status == 303);
  CHECK(header(r, "Location") == "/vanilla/catalog?page=2");
  auto cookie = header(r, "Set-Cookie");
  REQUIRE(cookie.rfind("bs_sid=", 0) == 0);
  auto sid = cookie.substr(7, 16);
  auto cart = call("GET", "/vanilla/catalog/cart", {.cookies = {{"bs_sid", sid}}});
  CHECK(count(cart.body, "data-testid=\"cart-row\"") == 1);
  CHECK(cart.body.find("data-testid=\"cart-badge\">1<") != std::string::npos);
  // htmx: badge fragment instead of redirect
  auto h = call("POST", "/htmx/catalog/cart", {.body = "product_id=3&qty=1", .headers = {{"hx-request", "true"}}, .cookies = {{"bs_sid", sid}}});
  CHECK(h.status == 200);
  CHECK(h.body.find("data-testid=\"cart-badge\"") != std::string::npos);
  CHECK(h.body.find(">2<") != std::string::npos);
}

TEST_CASE("handlers: empty_states_render") {
  CHECK(call("GET", "/alpine/catalog/cart", {.cookies = {{"bs_sid", "emptyemptyempty1"}}}).body.find("data-testid=\"empty-state\"") != std::string::npos);
  CHECK(call("GET", "/alpine/catalog?q=zzzz").body.find("data-testid=\"empty-state\"") != std::string::npos);
  // empty a column: move every doing card on board 5 to done via API
  auto b = bs::json::parse(call("GET", "/api/kanban/boards/5").body);
  for (auto& c : b["columns"]["doing"]) call("DELETE", "/api/kanban/cards/" + std::to_string(c["id"].get<int>()));
  auto page = call("GET", "/htmx/kanban/5");
  CHECK(page.body.find("data-testid=\"empty-state\"") != std::string::npos);
}

TEST_CASE("handlers: card_title_validation_and_escape") {
  std::string long_title(201, 'a');
  auto bad = call("POST", "/api/kanban/boards/1/cards", {.body = bs::json{{"title", long_title}, {"column", "backlog"}}.dump()});
  CHECK(bad.status == 422);
  auto empty = call("POST", "/api/kanban/boards/1/cards", {.body = R"({"title":"   ","column":"backlog"})"});
  CHECK(empty.status == 422);
  auto form_bad = call("POST", "/vanilla/kanban/1/cards", {.body = "title=" + long_title + "&column=backlog"});
  CHECK(form_bad.status == 422);
  auto ok = call("POST", "/api/kanban/boards/1/cards", {.body = R"({"title":"<b>x</b>","column":"backlog"})"});
  CHECK(ok.status == 201);
  auto page = call("GET", "/vanilla/kanban/1");
  CHECK(page.body.find("&lt;b&gt;x&lt;/b&gt;") != std::string::npos);
  CHECK(page.body.find("<b>x</b>") == std::string::npos);
  auto id = bs::json::parse(ok.body)["id"].get<int>();
  CHECK(call("DELETE", "/api/kanban/cards/" + std::to_string(id)).status == 204);
}

TEST_CASE("handlers: kanban form flow") {
  auto r = call("POST", "/alpine/kanban/2/cards", {.body = "title=Form+card&column=backlog"});
  CHECK(r.status == 303);
  CHECK(header(r, "Location") == "/alpine/kanban/2");
  auto b = bs::json::parse(call("GET", "/api/kanban/boards/2").body);
  auto id = b["columns"]["backlog"].back()["id"].get<int>();
  auto ids = std::to_string(id);
  CHECK(call("POST", "/alpine/kanban/cards/" + ids, {.body = "move=right"}).status == 303);
  CHECK(bs::json::parse(call("GET", "/api/kanban/boards/2").body)["columns"]["todo"].back()["id"] == id);
  auto hx = call("POST", "/htmx/kanban/cards/" + ids, {.body = "title=Renamed", .headers = {{"hx-request", "true"}}});
  CHECK(hx.status == 200);
  CHECK(hx.body.find("value=\"Renamed\"") != std::string::npos);
  CHECK(call("POST", "/alpine/kanban/cards/" + ids, {.body = "delete=1"}).status == 303);
  CHECK(call("POST", "/alpine/kanban/cards/" + ids, {.body = "delete=1"}).status == 404);
}

TEST_CASE("handlers: api_shapes") {
  auto p = bs::json::parse(call("GET", "/api/catalog/products?q=lamp&sort=price_asc").body);
  for (auto k : {"items", "total", "page", "pages", "categories"}) CHECK(p.contains(k));
  CHECK(p["categories"].size() == 12);
  auto one = call("GET", "/api/catalog/products/1");
  CHECK(one.content_type.rfind("application/json", 0) == 0);
  CHECK(bs::json::parse(one.body)["id"] == 1);
  auto nf = call("GET", "/api/catalog/products/999999");
  CHECK(nf.status == 404);
  CHECK(bs::json::parse(nf.body)["error"] == "not_found");
  CHECK(bs::json::parse(call("GET", "/api/kanban/boards").body).size() == 5);
  CHECK(bs::json::parse(call("GET", "/api/kanban/boards/1").body)["columns"].size() == 4);
  auto d = bs::json::parse(call("GET", "/api/dashboard?range=30").body);
  CHECK(d["daily"].size() == 30);
  CHECK(bs::json::parse(call("GET", "/api/dashboard").body)["range_days"] == 30);
  // cart api with header session
  CallOpts s{.headers = {{"x-session-id", "apisession000001"}}};
  s.body = R"({"product_id":7,"qty":2})";
  auto add = call("POST", "/api/catalog/cart", s);
  CHECK(add.status == 201);
  auto cart = bs::json::parse(add.body);
  CHECK(cart["count"] == 2);
  auto item = std::to_string(cart["items"][0]["id"].get<int>());
  s.body = R"({"qty":5})";
  CHECK(bs::json::parse(call("PATCH", "/api/catalog/cart/" + item, s).body)["count"] == 5);
  s.body = "";
  CHECK(bs::json::parse(call("DELETE", "/api/catalog/cart/" + item, s).body)["count"] == 0);
  s.body = R"({"product_id":"x"})";
  CHECK(call("POST", "/api/catalog/cart", s).status == 400);
  CHECK(call("POST", "/api/catalog/cart", {.body = "not json"}).status == 400);
}

TEST_CASE("handlers: static_mime") {
  ensure_init();
  auto css = bs::serve_static("app.css");
  REQUIRE(css);
  CHECK(css->content_type == "text/css; charset=utf-8");
  CHECK(bs::serve_static("vendor/htmx.min.js")->content_type == "text/javascript; charset=utf-8");
  CHECK(bs::serve_static("img/cat-1.svg")->content_type == "image/svg+xml");
  CHECK_FALSE(bs::serve_static("../app.css"));
  CHECK_FALSE(bs::serve_static("img/../../schema.sql"));
  CHECK_FALSE(bs::serve_static("nope.css"));
  CHECK(call("GET", "/healthz").body == "ok");
}

TEST_CASE("handlers: match") {
  std::map<std::string, std::string> p;
  CHECK(bs::match("/{fe}/catalog/p/{id}", "/htmx/catalog/p/12", p));
  CHECK(p["fe"] == "htmx");
  CHECK(p["id"] == "12");
  p.clear();
  CHECK_FALSE(bs::match("/{fe}/catalog", "/htmx/catalog/x", p));
  CHECK_FALSE(bs::match("/{fe}/catalog", "//catalog", p));
  CHECK(bs::url_decode("a+b%C4%9F") == "a bğ");
}

TEST_CASE("handlers: static works with a relative root") {
  ensure_init();
  auto cwd = fs::current_path();
  fs::current_path(BS_ROOT);
  bs::init({".", (fs::temp_directory_path() / "bs-tests" / ("handlers-" + std::to_string(::getpid()) + ".sqlite")).string(), 1});
  auto r = bs::serve_static("img/cat-2.svg");
  fs::current_path(cwd);
  bs::init({BS_ROOT, (fs::temp_directory_path() / "bs-tests" / ("handlers-" + std::to_string(::getpid()) + ".sqlite")).string(), 1});
  CHECK(r.has_value());
}
