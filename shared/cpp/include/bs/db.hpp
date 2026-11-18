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
Db& thread_db();

}  // namespace bs
