// Fast handlers through bs::routes(): rendering, escaping and glaze request parsing.
#include <doctest/doctest.h>

#include <filesystem>
#include <string>
#include <unistd.h>

#include <mimalloc.h>
#include <malloc/malloc.h>

#include "bs/http.hpp"

namespace fs = std::filesystem;

namespace {
void ensure_init() {
  static bool done = false;
  if (done) return;
  auto dir = fs::temp_directory_path() / "bs-tests";
  fs::create_directories(dir);
  auto p = dir / ("fast-handlers-" + std::to_string(::getpid()) + ".sqlite");
  for (auto s : {"", "-wal", "-shm"}) fs::remove(p.string() + s);
  fs::copy_file(fs::path(BS_ROOT) / "data/pristine.sqlite", p);
  bs::init({BS_ROOT, p.string(), 1});
  done = true;
}

bs::Resp call(const std::string& method, const std::string& target, const std::string& body = "",
              std::map<std::string, std::string> headers = {}) {
  ensure_init();
  bs::Req r;
  r.method = method;
  auto q = target.find('?');
  r.path = target.substr(0, q);
  if (q != std::string::npos) r.query = bs::parse_urlencoded(target.substr(q + 1));
  r.headers = headers;
  r.body = body;
  if (method == "POST" && !body.empty() && body[0] != '{') r.form = bs::parse_urlencoded(body);
  for (auto& rt : bs::routes()) {
    std::map<std::string, std::string> params;
    if (method == rt.method && bs::match(rt.pattern, r.path, params)) {
      r.params = params;
      return rt.fn(r);
    }
  }
  return bs::Resp{404, "text/plain", "no route", {}};
}
}  // namespace

TEST_CASE("fast handlers: catalog page renders and escapes the query") {
  auto r = call("GET", "/vanilla/catalog?q=%3Cscript%3Ex%3C%2Fscript%3E");
  CHECK(r.status == 200);
  CHECK(r.body.find("&lt;script&gt;") != std::string::npos);
  CHECK(r.body.find("<script>x") == std::string::npos);
  auto full = call("GET", "/htmx/catalog");
  CHECK(full.body.rfind("<!doctype html>\n<html lang=\"tr\">", 0) == 0);
  CHECK(full.body.find("<title>Ürün Kataloğu · Best Stack</title>") != std::string::npos);
  CHECK(full.body.find("Sayfa 1 / 84") != std::string::npos);
}

TEST_CASE("fast handlers: JSON request bodies parsed with glaze keep the original validation") {
  std::map<std::string, std::string> h{{"x-session-id", "fasttestsession1"}};
  CHECK(call("POST", "/api/catalog/cart", "\"nope\"", h).status == 400);
  CHECK(call("POST", "/api/catalog/cart", "{bad", h).status == 400);
  CHECK(call("POST", "/api/catalog/cart", R"({"product_id":3.0})", h).status == 400);
  CHECK(call("POST", "/api/catalog/cart", R"({"product_id":4294967297,"qty":1})", h).status == 404);
  CHECK(call("POST", "/api/catalog/cart", R"({"product_id":7,"qty":4294967297})", h).status == 400);
  CHECK(call("POST", "/api/catalog/cart", R"({"product_id":7,"qty":null})", h).status == 400);
  auto ok = call("POST", "/api/catalog/cart", R"({"product_id":7,"qty":2,"extra":[1,{"a":2}]})", h);
  CHECK(ok.status == 201);
  CHECK(ok.content_type == "application/json");
  CHECK(ok.body.find("\"count\":2") != std::string::npos);
  CHECK(call("POST", "/api/kanban/boards/1/cards", R"({"title":5})").status == 422);
  CHECK(call("POST", "/api/kanban/boards/1/cards", R"({"title":"x","column":"nope"})").status == 422);
  auto c = call("POST", "/api/kanban/boards/1/cards", R"({"title":"a \"q\" ç"})");
  CHECK(c.status == 201);
  CHECK(c.body.find(R"("title":"a \"q\" ç")") != std::string::npos);
  CHECK(c.body.find(R"("column":"backlog")") != std::string::npos);
}

TEST_CASE("fast handlers: board JSON always has all four columns") {
  auto b = call("GET", "/api/kanban/boards/1");
  for (auto col : {"\"backlog\":", "\"todo\":", "\"doing\":", "\"done\":"}) CHECK(b.body.find(col) != std::string::npos);
}

TEST_CASE("mimalloc overrides malloc and new for the whole process") {
  CHECK(mi_version() >= 30700);
  void* p = std::malloc(64);
  auto* q = new int[16];
  char* s = strdup("allocated inside libSystem");  // malloc called from a system dylib
  CHECK(mi_is_in_heap_region(p));
  CHECK(mi_is_in_heap_region(q));
  CHECK(mi_is_in_heap_region(s));
  std::free(p);
  delete[] q;
  std::free(s);
}
