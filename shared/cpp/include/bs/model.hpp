#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace bs {

/// Fixed "today" (2026-11-01T00:00:00Z); dashboard ranges end here.
inline constexpr long kAnchorTs = 1793491200;
inline constexpr int kPerPage = 24;
inline constexpr int kMaxTitle = 200;

struct Category { int id = 0; std::string name, slug; };

struct Product {
  int id = 0, category_id = 0;
  std::string category, name, description, image;
  int price_cents = 0, stock = 0;
  double rating = 0;
};

struct ProductQuery {
  std::string q, cat, sort;  // sort: "", "price_asc", "price_desc"
  int page = 1;
  int per_page = kPerPage;
};

struct ProductPage { std::vector<Product> items; int total = 0, page = 1, pages = 1; };

struct CartItem { int id = 0; Product product; int qty = 0; };
struct Cart { std::vector<CartItem> items; int count = 0; int total_cents = 0; };

struct Card {
  int id = 0, board_id = 0, position = 0;
  std::string column, title, description, assignee, due_date;
  std::vector<std::string> labels;
};
struct BoardSummary { int id = 0; std::string name; int card_count = 0; };
struct Board { int id = 0; std::string name; std::map<std::string, std::vector<Card>> columns; };
inline const std::vector<std::string>& kColumns() {
  static const std::vector<std::string> c{"backlog", "todo", "doing", "done"};
  return c;
}

struct DayPoint { std::string day; long events = 0; long revenue_cents = 0; };
struct Dashboard {
  int range_days = 30;
  long events = 0, users = 0, revenue_cents = 0;
  double conversion = 0;
  std::vector<DayPoint> daily;
  std::vector<std::pair<std::string, long>> countries;
  struct Item { std::string item; long revenue_cents = 0; long purchases = 0; };
  std::vector<Item> top_items;
};

}  // namespace bs
