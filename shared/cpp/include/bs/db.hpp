#pragma once
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "bs/model.hpp"

struct sqlite3;

namespace bs {

/// One connection; not thread-safe. Use thread_db() for one per thread.
class Db {
 public:
  explicit Db(const std::string& path);
  ~Db();
  Db(const Db&) = delete;
  Db& operator=(const Db&) = delete;

  std::vector<Category> categories();
  ProductPage products(const ProductQuery& q);
  std::optional<Product> product(int id);

  Cart cart(const std::string& sid);
  void cart_add(const std::string& sid, int product_id, int qty);
  bool cart_set_qty(const std::string& sid, int item_id, int qty);

  std::vector<BoardSummary> boards();
  std::optional<Board> board(int id);
  std::optional<Card> card_create(int board_id, const std::string& title, const std::string& column);
  std::optional<Card> card_update(int id, std::optional<std::string> title, std::optional<std::string> move);
  bool card_delete(int id);

  Dashboard dashboard(int range_days);

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

/// Path used by thread_db(); set once at startup (bs::init).
void set_db_path(const std::string& path);
/// The calling thread's connection. Connections of exited threads go back to a shared pool and are
/// reused, so thread-per-connection servers (Oat++) don't re-open SQLite and re-prepare per request.
Db& thread_db();
/// Number of SQLite connections opened by thread_db() so far.
size_t thread_db_opens();

}  // namespace bs
