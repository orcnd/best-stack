#include "bs/chart.hpp"
#include "view.hpp"

namespace bs::h {
using namespace bs::detail;
using namespace bs::view;

namespace {
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

Resp dashboard_page(const Req& r) {
  auto fe = get(r.params, "fe");
  if (!valid_fe(fe)) return not_found_html();
  auto range = parse_range(r);
  if (!range) return html("Geçersiz aralık", 400);
  auto d = thread_db().dashboard(*range);
  auto vm = view::page(r, "dashboard", "Analitik", "Analitik dashboard");
  json ranges = json::array();
  for (auto [v, label] : std::vector<std::pair<int, const char*>>{{7, "Son 7 gün"}, {30, "Son 30 gün"}, {90, "Son 90 gün"}})
    ranges.push_back({{"value", v}, {"label", label}, {"selected", v == *range}});
  json countries = json::array();
  for (auto& [c, n] : d.countries) countries.push_back({{"country", c}, {"events", fmt_int(n)}});
  json items = json::array();
  for (auto& it : d.top_items)
    items.push_back({{"item", it.item}, {"purchases", fmt_int(it.purchases)}, {"revenue", fmt_money(it.revenue_cents)}});
  vm["ranges"] = ranges;
  vm["kpis"] = {{"events", fmt_int(d.events)},
                {"users", fmt_int(d.users)},
                {"revenue", fmt_money(d.revenue_cents)},
                {"conversion", fmt_pct(d.conversion)}};
  vm["raw_chart"] = daily_chart_svg(d.daily);
  vm["countries"] = countries;
  vm["top_items"] = items;
  if (is_htmx(r)) {
    vm["fragment"] = true;
    return html(view::view(fe, "dashboard_body", vm));
  }
  return html(view::view(fe, "dashboard", vm));
}

Resp api_dashboard(const Req& r) {
  auto range = parse_range(r);
  if (!range) return json_resp(R"({"error":"invalid_range"})", 400);
  return json_resp(json(thread_db().dashboard(*range)).dump());
}

}  // namespace bs::h
