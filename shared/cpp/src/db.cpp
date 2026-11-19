#include "bs/db.hpp"

#include <sqlite3.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <ctime>
#include <stdexcept>
#include <unordered_map>

namespace bs {
namespace {

class Stmt {
 public:
  explicit Stmt(sqlite3_stmt* s) : s_(s) {}
  ~Stmt() {
    sqlite3_reset(s_);
    sqlite3_clear_bindings(s_);
  }
  Stmt& bind(int i, int v) { sqlite3_bind_int(s_, i, v); return *this; }
  Stmt& bind(int i, long v) { sqlite3_bind_int64(s_, i, v); return *this; }
  Stmt& bind(int i, const std::string& v) { sqlite3_bind_text(s_, i, v.data(), (int)v.size(), SQLITE_TRANSIENT); return *this; }
  bool step() {
    int rc = sqlite3_step(s_);
    if (rc == SQLITE_ROW) return true;
    if (rc == SQLITE_DONE) return false;
    throw std::runtime_error(std::string("sqlite step: ") + sqlite3_errmsg(sqlite3_db_handle(s_)));
  }
  int i(int c) const { return sqlite3_column_int(s_, c); }
  long l(int c) const { return (long)sqlite3_column_int64(s_, c); }
  double d(int c) const { return sqlite3_column_double(s_, c); }
  std::string t(int c) const {
    auto p = sqlite3_column_text(s_, c);
    return p ? std::string(reinterpret_cast<const char*>(p), sqlite3_column_bytes(s_, c)) : std::string();
  }

 private:
  sqlite3_stmt* s_;
};

std::string like_pattern(const std::string& q) {
  std::string out = "%";
  for (char c : q) {
    if (c == '%' || c == '_' || c == '\\') out += '\\';
    out += c;
  }
  return out + "%";
}

std::vector<std::string> split_labels(const std::string& s) {
  std::vector<std::string> out;
  size_t start = 0;
  while (start < s.size()) {
    auto end = s.find(',', start);
    if (end == std::string::npos) end = s.size();
    if (end > start) out.push_back(s.substr(start, end - start));
    start = end + 1;
  }
  return out;
}

std::string iso_day(long ts) {
  std::time_t t = ts;
  std::tm tm{};
  gmtime_r(&t, &tm);
  char buf[11];
  std::strftime(buf, sizeof buf, "%Y-%m-%d", &tm);
  return buf;
}

constexpr const char* kProductCols =
    "p.id, p.category_id, c.name, p.name, p.description, p.image, p.price_cents, p.stock, p.rating";
constexpr const char* kCardCols = "id, board_id, position, col, title, description, assignee, due_date, labels";

Product read_product(const Stmt& s, int o = 0) {
  Product p;
  p.id = s.i(o);
  p.category_id = s.i(o + 1);
  p.category = s.t(o + 2);
  p.name = s.t(o + 3);
  p.description = s.t(o + 4);
  p.image = s.t(o + 5);
  p.price_cents = s.i(o + 6);
  p.stock = s.i(o + 7);
  p.rating = s.d(o + 8);
  return p;
}

Card read_card(const Stmt& s) {
  Card c;
  c.id = s.i(0);
  c.board_id = s.i(1);
  c.position = s.i(2);
  c.column = s.t(3);
  c.title = s.t(4);
  c.description = s.t(5);
  c.assignee = s.t(6);
  c.due_date = s.t(7);
  c.labels = split_labels(s.t(8));
  return c;
}

std::string g_db_path;

}  // namespace

struct Db::Impl {
  sqlite3* db = nullptr;
  std::unordered_map<std::string, sqlite3_stmt*> cache;

  Stmt prep(const std::string& sql) {
    auto it = cache.find(sql);
    if (it != cache.end()) return Stmt(it->second);
    sqlite3_stmt* s = nullptr;
    if (sqlite3_prepare_v3(db, sql.c_str(), -1, SQLITE_PREPARE_PERSISTENT, &s, nullptr) != SQLITE_OK)
      throw std::runtime_error(std::string("sqlite prepare: ") + sqlite3_errmsg(db) + " :: " + sql);
    cache.emplace(sql, s);
    return Stmt(s);
  }
  void exec(const char* sql) {
    char* err = nullptr;
    if (sqlite3_exec(db, sql, nullptr, nullptr, &err) != SQLITE_OK) {
      std::string m = err ? err : "?";
      sqlite3_free(err);
      throw std::runtime_error("sqlite exec: " + m);
    }
  }
  int changes() const { return sqlite3_changes(db); }

  std::optional<Card> card_by_id(int id) {
    auto s = prep(std::string("SELECT ") + kCardCols + " FROM cards WHERE id = ?");
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    return read_card(s);
  }
  int next_position(int board_id, const std::string& col) {
    auto s = prep("SELECT COALESCE(MAX(position) + 1, 0) FROM cards WHERE board_id = ? AND col = ?");
    s.bind(1, board_id).bind(2, col);
    s.step();
    return s.i(0);
  }
};

namespace {
// Process-wide SQLite settings; must run before the first connection is opened.
// MEMSTATUS=0 removes a global mutex taken on every allocation (it serialized concurrent readers).
const bool g_sqlite_configured = [] {
  sqlite3_config(SQLITE_CONFIG_MEMSTATUS, 0);
  return true;
}();
}  // namespace

Db::Db(const std::string& path) : impl_(std::make_unique<Impl>()) {
  (void)g_sqlite_configured;
  if (sqlite3_open_v2(path.c_str(), &impl_->db, SQLITE_OPEN_READWRITE | SQLITE_OPEN_NOMUTEX, nullptr) != SQLITE_OK)
    throw std::runtime_error("cannot open db: " + path);
  sqlite3_busy_timeout(impl_->db, 5000);
  // temp_store=MEMORY: GROUP BY / COUNT(DISTINCT) temp B-trees stay in RAM instead of temp files.
  impl_->exec("PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL; PRAGMA temp_store = MEMORY;"
              // mmap: readers share OS pages instead of pread-copying into each connection's cache.
              " PRAGMA mmap_size = 268435456;");
}

Db::~Db() {
  for (auto& [k, s] : impl_->cache) sqlite3_finalize(s);
  sqlite3_close(impl_->db);
}

std::vector<Category> Db::categories() {
  std::vector<Category> out;
  auto s = impl_->prep("SELECT id, name, slug FROM categories ORDER BY id");
  while (s.step()) out.push_back({s.i(0), s.t(1), s.t(2)});
  return out;
}

ProductPage Db::products(const ProductQuery& q) {
  std::string where = " WHERE 1=1";
  if (!q.q.empty()) where += " AND p.name LIKE ? ESCAPE '\\'";
  if (!q.cat.empty()) where += " AND c.slug = ?";
  auto bind_where = [&](Stmt& s) {
    int i = 1;
    if (!q.q.empty()) s.bind(i++, like_pattern(q.q));
    if (!q.cat.empty()) s.bind(i++, q.cat);
    return i;
  };
  const std::string from = " FROM products p JOIN categories c ON c.id = p.category_id";

  ProductPage page;
  {
    auto s = impl_->prep("SELECT count(*)" + from + where);
    bind_where(s);
    s.step();
    page.total = s.i(0);
  }
  int per = std::max(1, q.per_page);
  page.pages = std::max(1, (page.total + per - 1) / per);
  page.page = std::clamp(q.page, 1, page.pages);

  std::string order = " ORDER BY p.id";
  if (q.sort == "price_asc") order = " ORDER BY p.price_cents ASC, p.id";
  if (q.sort == "price_desc") order = " ORDER BY p.price_cents DESC, p.id";
  auto s = impl_->prep(std::string("SELECT ") + kProductCols + from + where + order + " LIMIT ? OFFSET ?");
  int i = bind_where(s);
  s.bind(i, per).bind(i + 1, (page.page - 1) * per);
  while (s.step()) page.items.push_back(read_product(s));
  return page;
}

std::optional<Product> Db::product(int id) {
  auto s = impl_->prep(std::string("SELECT ") + kProductCols +
                       " FROM products p JOIN categories c ON c.id = p.category_id WHERE p.id = ?");
  s.bind(1, id);
  if (!s.step()) return std::nullopt;
  return read_product(s);
}

Cart Db::cart(const std::string& sid) {
  Cart c;
  auto s = impl_->prep(std::string("SELECT ci.id, ci.qty, ") + kProductCols +
                       " FROM cart_items ci JOIN products p ON p.id = ci.product_id"
                       " JOIN categories c ON c.id = p.category_id WHERE ci.session_id = ? ORDER BY ci.id");
  s.bind(1, sid);
  while (s.step()) {
    CartItem it{s.i(0), read_product(s, 2), s.i(1)};
    c.count += it.qty;
    c.total_cents += it.qty * it.product.price_cents;
    c.items.push_back(std::move(it));
  }
  return c;
}

void Db::cart_add(const std::string& sid, int product_id, int qty) {
  if (qty <= 0 || !product(product_id)) return;
  auto s = impl_->prep(
      "INSERT INTO cart_items (session_id, product_id, qty) VALUES (?, ?, ?) "
      "ON CONFLICT (session_id, product_id) DO UPDATE SET qty = qty + excluded.qty");
  s.bind(1, sid).bind(2, product_id).bind(3, qty);
  s.step();
}

bool Db::cart_set_qty(const std::string& sid, int item_id, int qty) {
  if (qty <= 0) {
    auto s = impl_->prep("DELETE FROM cart_items WHERE id = ? AND session_id = ?");
    s.bind(1, item_id).bind(2, sid);
    s.step();
  } else {
    auto s = impl_->prep("UPDATE cart_items SET qty = ? WHERE id = ? AND session_id = ?");
    s.bind(1, qty).bind(2, item_id).bind(3, sid);
    s.step();
  }
  return impl_->changes() > 0;
}

std::vector<BoardSummary> Db::boards() {
  std::vector<BoardSummary> out;
  auto s = impl_->prep(
      "SELECT b.id, b.name, count(c.id) FROM boards b LEFT JOIN cards c ON c.board_id = b.id "
      "GROUP BY b.id ORDER BY b.id");
  while (s.step()) out.push_back({s.i(0), s.t(1), s.i(2)});
  return out;
}

std::optional<Board> Db::board(int id) {
  Board b;
  {
    auto s = impl_->prep("SELECT id, name FROM boards WHERE id = ?");
    s.bind(1, id);
    if (!s.step()) return std::nullopt;
    b.id = s.i(0);
    b.name = s.t(1);
  }
  for (auto& c : kColumns()) b.columns[c];
  auto s = impl_->prep(std::string("SELECT ") + kCardCols + " FROM cards WHERE board_id = ? ORDER BY position, id");
  s.bind(1, id);
  while (s.step()) {
    auto c = read_card(s);
    b.columns[c.column].push_back(std::move(c));
  }
  return b;
}

std::optional<Card> Db::card_create(int board_id, const std::string& title, const std::string& column) {
  const auto& cols = kColumns();
  if (std::find(cols.begin(), cols.end(), column) == cols.end()) return std::nullopt;
  {
    auto s = impl_->prep("SELECT 1 FROM boards WHERE id = ?");
    s.bind(1, board_id);
    if (!s.step()) return std::nullopt;
  }
  int pos = impl_->next_position(board_id, column);
  {
    auto s = impl_->prep(
        "INSERT INTO cards (board_id, col, position, title, description, labels, assignee, due_date) "
        "VALUES (?, ?, ?, ?, '', '', '', '')");
    s.bind(1, board_id).bind(2, column).bind(3, pos).bind(4, title);
    s.step();
  }
  return impl_->card_by_id((int)sqlite3_last_insert_rowid(impl_->db));
}

std::optional<Card> Db::card_update(int id, std::optional<std::string> title, std::optional<std::string> move) {
  auto card = impl_->card_by_id(id);
  if (!card) return std::nullopt;
  if (title) {
    auto s = impl_->prep("UPDATE cards SET title = ? WHERE id = ?");
    s.bind(1, *title).bind(2, id);
    s.step();
  }
  if (move && (*move == "left" || *move == "right")) {
    const auto& cols = kColumns();
    int idx = (int)(std::find(cols.begin(), cols.end(), card->column) - cols.begin());
    int next = std::clamp(idx + (*move == "right" ? 1 : -1), 0, (int)cols.size() - 1);
    if (next != idx) {
      int pos = impl_->next_position(card->board_id, cols[next]);
      auto s = impl_->prep("UPDATE cards SET col = ?, position = ? WHERE id = ?");
      s.bind(1, cols[next]).bind(2, pos).bind(3, id);
      s.step();
    }
  }
  return impl_->card_by_id(id);
}

bool Db::card_delete(int id) {
  auto s = impl_->prep("DELETE FROM cards WHERE id = ?");
  s.bind(1, id);
  s.step();
  return impl_->changes() > 0;
}

Dashboard Db::dashboard(int range_days) {
  Dashboard d;
  d.range_days = range_days;
  const long to = kAnchorTs;
  const long from = to - range_days * 86400L;
  long buyers = 0;
  {
    auto s = impl_->prep(
        "WITH r(lo, hi) AS (SELECT ? / 86400, ? / 86400) SELECT (SELECT COALESCE(sum(events), 0) FROM ev_day WHERE day >= lo AND day < hi), (SELECT count(*) FROM ev_user WHERE last_day >= lo), (SELECT COALESCE(sum(revenue_cents), 0) FROM ev_day WHERE day >= lo AND day < hi), (SELECT count(*) FROM ev_user WHERE last_purchase_day >= lo) FROM r");
    s.bind(1, from).bind(2, to);
    s.step();
    d.events = s.l(0);
    d.users = s.l(1);
    d.revenue_cents = s.l(2);
    buyers = s.l(3);
  }
  d.conversion = d.users ? (double)buyers / (double)d.users : 0.0;

  d.daily.resize(range_days);
  for (int i = 0; i < range_days; i++) d.daily[i].day = iso_day(from + i * 86400L);
  {
    auto s = impl_->prep(
        "SELECT day - ? / 86400 AS di, events, revenue_cents FROM ev_day WHERE day >= ? / 86400 AND day < ? / 86400");
    s.bind(1, from).bind(2, from).bind(3, to);
    while (s.step()) {
      int di = s.i(0);
      if (di >= 0 && di < range_days) {
        d.daily[di].events = s.l(1);
        d.daily[di].revenue_cents = s.l(2);
      }
    }
  }
  {
    auto s = impl_->prep(
        "SELECT country, sum(events) n FROM ev_country_day WHERE day >= ? / 86400 AND day < ? / 86400 GROUP BY country ORDER BY n DESC, country LIMIT 8");
    s.bind(1, from).bind(2, to);
    while (s.step()) d.countries.emplace_back(s.t(0), s.l(1));
  }
  {
    auto s = impl_->prep(
        "SELECT item, sum(revenue_cents) r, sum(purchases) FROM ev_item_day WHERE day >= ? / 86400 AND day < ? / 86400 GROUP BY item ORDER BY r DESC, item LIMIT 10");
    s.bind(1, from).bind(2, to);
    while (s.step()) d.top_items.push_back({s.t(0), s.l(1), s.l(2)});
  }
  return d;
}

namespace {
std::atomic<size_t> g_opens{0};
std::mutex g_pool_mu;
std::vector<std::unique_ptr<Db>> g_pool;  // connections released by exited threads

/// Owns the thread's connection; on thread exit hands it back to the pool instead of closing it.
struct ThreadConn {
  std::unique_ptr<Db> db;
  ~ThreadConn() {
    if (!db) return;
    std::lock_guard lk(g_pool_mu);
    g_pool.push_back(std::move(db));
  }
};
}  // namespace

void set_db_path(const std::string& path) {
  g_db_path = path;
  std::lock_guard lk(g_pool_mu);
  g_pool.clear();  // pooled connections point at the previous path
}

size_t thread_db_opens() { return g_opens.load(); }

Db& thread_db() {
  thread_local ThreadConn tc;
  if (!tc.db) {
    {
      std::lock_guard lk(g_pool_mu);
      if (!g_pool.empty()) {
        tc.db = std::move(g_pool.back());
        g_pool.pop_back();
      }
    }
    if (!tc.db) {
      tc.db = std::make_unique<Db>(g_db_path);
      g_opens++;
    }
  }
  return *tc.db;
}

}  // namespace bs
