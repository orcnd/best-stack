#pragma once
// HTML writers, one per original inja template (shared/cpp/templates/*.html), producing the same bytes.
// Each appends to `o`; strings from the DB or the request are escaped while being written.
#include <string>
#include <string_view>
#include <vector>

#include "bs/model.hpp"

namespace bsf::pages {

/// Frontend of the request ("htmx" | "alpine" | "vanilla"; already validated).
struct Fe {
  std::string_view name;
  bool htmx, alpine, vanilla;
  explicit Fe(std::string_view n) : name(n), htmx(n == "htmx"), alpine(n == "alpine"), vanilla(n == "vanilla") {}
};

struct Layout {
  std::string_view nav, title, description;
  int cart_count = 0;
};

struct CatalogView {
  const bs::ProductQuery& q;
  const std::vector<bs::Category>& categories;
  const bs::ProductPage& page;
};

struct DashboardView {
  int range;
  const bs::Dashboard& d;
};

struct BoardView {
  const bs::Board& board;
  const std::vector<bs::BoardSummary>& boards;
};

void catalog(std::string& o, const Fe& fe, const Layout& l, const CatalogView& v);
void catalog_results(std::string& o, const Fe& fe, const CatalogView& v);
void product(std::string& o, const Fe& fe, const Layout& l, const bs::Product& p);
void cart(std::string& o, const Fe& fe, const Layout& l, const bs::Cart& c);
void cart_body(std::string& o, const Fe& fe, const bs::Cart& c, bool fragment);
void badge(std::string& o, const Fe& fe, int cart_count);
void kanban(std::string& o, const Fe& fe, const Layout& l, const BoardView& v);
void board(std::string& o, const Fe& fe, const BoardView& v);
void dashboard(std::string& o, const Fe& fe, const Layout& l, const DashboardView& v);
void dashboard_body(std::string& o, const Fe& fe, const DashboardView& v);

}  // namespace bsf::pages
