#include "bs/json.hpp"

namespace bs {

void to_json(json& j, const Category& v) { j = {{"id", v.id}, {"name", v.name}, {"slug", v.slug}}; }

void to_json(json& j, const Product& v) {
  j = {{"id", v.id},          {"category_id", v.category_id}, {"category", v.category},
       {"name", v.name},      {"description", v.description}, {"image", v.image},
       {"price_cents", v.price_cents}, {"stock", v.stock},    {"rating", v.rating}};
}

void to_json(json& j, const ProductPage& v) {
  j = {{"items", v.items}, {"total", v.total}, {"page", v.page}, {"pages", v.pages}};
}

void to_json(json& j, const CartItem& v) { j = {{"id", v.id}, {"qty", v.qty}, {"product", v.product}}; }

void to_json(json& j, const Cart& v) {
  j = {{"items", v.items}, {"count", v.count}, {"total_cents", v.total_cents}};
}

void to_json(json& j, const Card& v) {
  j = {{"id", v.id},           {"board_id", v.board_id},   {"position", v.position},
       {"column", v.column},   {"title", v.title},         {"description", v.description},
       {"assignee", v.assignee}, {"due_date", v.due_date}, {"labels", v.labels}};
}

void to_json(json& j, const BoardSummary& v) {
  j = {{"id", v.id}, {"name", v.name}, {"card_count", v.card_count}};
}

void to_json(json& j, const Board& v) {
  json cols = json::object();
  for (auto& c : kColumns()) {
    auto it = v.columns.find(c);
    cols[c] = it == v.columns.end() ? json::array() : json(it->second);
  }
  j = {{"id", v.id}, {"name", v.name}, {"columns", cols}};
}

void to_json(json& j, const DayPoint& v) {
  j = {{"day", v.day}, {"events", v.events}, {"revenue_cents", v.revenue_cents}};
}

void to_json(json& j, const Dashboard& v) {
  json countries = json::array();
  for (auto& [c, n] : v.countries) countries.push_back({{"country", c}, {"events", n}});
  json items = json::array();
  for (auto& it : v.top_items)
    items.push_back({{"item", it.item}, {"revenue_cents", it.revenue_cents}, {"purchases", it.purchases}});
  j = {{"range_days", v.range_days},
       {"kpis", {{"events", v.events}, {"users", v.users}, {"revenue_cents", v.revenue_cents}, {"conversion", v.conversion}}},
       {"daily", v.daily},
       {"countries", countries},
       {"top_items", items}};
}

}  // namespace bs
