#include <doctest/doctest.h>

#include <atomic>
#include <filesystem>
#include <string>
#include <thread>
#include <vector>

#include <sqlite3.h>
#include <unistd.h>

#include "bs/db.hpp"
#include "bs/json.hpp"

namespace fs = std::filesystem;

// Fresh copy of the pristine DB per call.
static std::string temp_db() {
  static std::atomic<int> n{0};
  auto dir = fs::temp_directory_path() / "bs-tests";
  fs::create_directories(dir);
  auto p = dir / ("db-" + std::to_string(::getpid()) + "-" + std::to_string(n++) + ".sqlite");
  for (auto s : {"", "-wal", "-shm"}) fs::remove(p.string() + s);
  fs::copy_file(fs::path(BS_ROOT) / "data/pristine.sqlite", p);
  return p.string();
}

static long scalar(const std::string& path, const std::string& sql) {
  sqlite3* db = nullptr;
  sqlite3_open(path.c_str(), &db);
  sqlite3_stmt* st = nullptr;
  sqlite3_prepare_v2(db, sql.c_str(), -1, &st, nullptr);
  sqlite3_step(st);
  long v = sqlite3_column_int64(st, 0);
  sqlite3_finalize(st);
  sqlite3_close(db);
  return v;
}

TEST_CASE("db: products_default_page") {
  bs::Db db(temp_db());
  auto p = db.products({});
  CHECK(p.items.size() == 24);
  CHECK(p.total == 2000);
  CHECK(p.pages == 84);
  CHECK(p.page == 1);
  CHECK(!p.items[0].category.empty());
}

TEST_CASE("db: products_filter_and_sort") {
  bs::Db db(temp_db());
  auto cats = db.categories();
  REQUIRE(cats.size() == 12);
  bs::ProductQuery q;
  q.cat = cats[0].slug;
  q.sort = "price_asc";
  auto p = db.products(q);
  REQUIRE(!p.items.empty());
  for (auto& it : p.items) CHECK(it.category_id == cats[0].id);
  for (size_t i = 1; i < p.items.size(); i++) CHECK(p.items[i - 1].price_cents <= p.items[i].price_cents);
  q.sort = "price_desc";
  auto d = db.products(q);
  for (size_t i = 1; i < d.items.size(); i++) CHECK(d.items[i - 1].price_cents >= d.items[i].price_cents);
}

TEST_CASE("db: products_search_escapes_wildcards") {
  auto path = temp_db();
  bs::Db db(path);
  bs::ProductQuery q;
  q.q = "%";
  CHECK(db.products(q).total == 0);
  q.q = "_";
  CHECK(db.products(q).total == 0);
  q.q = "lamp";
  CHECK(db.products(q).total == scalar(path, "SELECT count(*) FROM products WHERE name LIKE '%lamp%'"));
  q.q = "' OR 1=1 --";
  CHECK(db.products(q).total == 0);
  q.q = "Doğal";
  CHECK(db.products(q).total > 0);
}

TEST_CASE("db: page_clamp") {
  bs::Db db(temp_db());
  bs::ProductQuery q;
  q.page = 0;
  CHECK(db.products(q).page == 1);
  q.page = 9999;
  CHECK(db.products(q).page == 84);
  q.q = "zzzzzz";
  auto e = db.products(q);
  CHECK(e.total == 0);
  CHECK(e.pages == 1);
  CHECK(e.page == 1);
}

TEST_CASE("db: cart_roundtrip") {
  bs::Db db(temp_db());
  CHECK(db.cart("s1").items.empty());
  db.cart_add("s1", 5, 1);
  db.cart_add("s1", 5, 1);
  auto c = db.cart("s1");
  REQUIRE(c.items.size() == 1);
  CHECK(c.items[0].qty == 2);
  CHECK(c.count == 2);
  CHECK(c.total_cents == 2 * c.items[0].product.price_cents);
  CHECK_FALSE(db.cart_set_qty("other", c.items[0].id, 3));
  CHECK(db.cart_set_qty("s1", c.items[0].id, 0));
  CHECK(db.cart("s1").items.empty());
}

TEST_CASE("db: kanban_crud") {
  bs::Db db(temp_db());
  auto before = db.board(1);
  REQUIRE(before);
  CHECK(before->columns.size() == 4);
  auto c = db.card_create(1, "x", "backlog");
  REQUIRE(c);
  auto b = *db.board(1);
  CHECK(b.columns["backlog"].back().id == c->id);
  auto m = db.card_update(c->id, std::nullopt, std::string("right"));
  REQUIRE(m);
  CHECK(m->column == "todo");
  auto done = db.board(1)->columns["done"].front();
  auto stay = db.card_update(done.id, std::nullopt, std::string("right"));
  CHECK(stay->column == "done");
  auto t = db.card_update(c->id, std::string("renamed"), std::nullopt);
  CHECK(t->title == "renamed");
  CHECK(db.card_delete(c->id));
  CHECK_FALSE(db.card_delete(c->id));
  int total = 0;
  auto after = *db.board(1);
  for (auto& [k, v] : after.columns) total += (int)v.size();
  CHECK(total == 100);
  CHECK_FALSE(db.card_create(99, "x", "backlog"));
  CHECK_FALSE(db.card_create(1, "x", "nope"));
  CHECK_FALSE(db.board(99));
}

TEST_CASE("db: dashboard_ranges") {
  auto path = temp_db();
  bs::Db db(path);
  for (int r : {7, 30, 90}) {
    auto d = db.dashboard(r);
    CHECK(d.range_days == r);
    CHECK((int)d.daily.size() == r);
    long from = bs::kAnchorTs - r * 86400L;
    CHECK(d.events == scalar(path, "SELECT count(*) FROM events WHERE ts >= " + std::to_string(from) +
                                       " AND ts < " + std::to_string(bs::kAnchorTs)));
    long sum = 0;
    for (auto& p : d.daily) sum += p.events;
    CHECK(sum == d.events);
    CHECK(d.countries.size() <= 8);
    CHECK(d.top_items.size() == 10);
    CHECK(d.conversion >= 0);
    CHECK(d.conversion <= 1);
  }
}

TEST_CASE("db: concurrent_cart_writes") {
  auto path = temp_db();
  std::vector<std::thread> ts;
  std::atomic<int> errors{0};
  for (int t = 0; t < 8; t++) {
    ts.emplace_back([&, t] {
      try {
        bs::Db db(path);
        for (int i = 0; i < 200; i++) db.cart_add("conc", 1 + (i % 10), 1);
      } catch (...) {
        errors++;
      }
    });
  }
  for (auto& t : ts) t.join();
  CHECK(errors == 0);
  bs::Db db(path);
  CHECK(db.cart("conc").count == 1600);
}

TEST_CASE("db: json_shapes") {
  bs::Db db(temp_db());
  bs::json d = db.dashboard(7);
  for (auto k : {"range_days", "kpis", "daily", "countries", "top_items"}) CHECK(d.contains(k));
  for (auto k : {"events", "users", "revenue_cents", "conversion"}) CHECK(d["kpis"].contains(k));
  bs::json b = *db.board(1);
  CHECK(b["columns"].size() == 4);
  bs::json p = db.products({});
  for (auto k : {"items", "total", "page", "pages"}) CHECK(p.contains(k));
  CHECK(p["items"][0].contains("price_cents"));
  bs::json c = db.board(1)->columns["todo"].front();
  CHECK(c["column"] == "todo");
  CHECK(c["labels"].is_array());
}
