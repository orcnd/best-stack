#include "view.hpp"

namespace bs::h {
using namespace bs::detail;
using namespace bs::view;

namespace {
ProductQuery parse_query(const Req& r) {
  ProductQuery q;
  q.q = get(r.query, "q");
  q.cat = get(r.query, "cat");
  auto s = get(r.query, "sort");
  if (s == "price_asc" || s == "price_desc") q.sort = s;
  q.page = to_int(get(r.query, "page")).value_or(1);
  return q;
}

std::string href(const std::string& base, const ProductQuery& q, int page) {
  std::string s;
  auto add = [&](const char* k, const std::string& v) {
    if (v.empty()) return;
    s += (s.empty() ? "?" : "&") + std::string(k) + "=" + url_encode(v);
  };
  add("q", q.q);
  add("cat", q.cat);
  add("sort", q.sort);
  if (page > 1) add("page", std::to_string(page));
  return base + "/catalog" + s;
}

json product_vm(const Product& p) {
  return {{"id", p.id},
          {"name", p.name},
          {"category", p.category},
          {"image", p.image},
          {"description", p.description},
          {"price", fmt_money(p.price_cents)},
          {"rating", fmt_rating(p.rating)},
          {"stock", p.stock}};
}

Resp cart_json(const Cart& c, int status = 200) { return json_resp(json(c).dump(), status); }
}  // namespace

Resp catalog_page(const Req& r) {
  auto fe = get(r.params, "fe");
  if (!valid_fe(fe)) return not_found_html();
  auto& db = thread_db();
  auto q = parse_query(r);
  auto page = db.products(q);
  auto vm = view::page(r, "catalog", "Ürün Kataloğu", "Best Stack demo ürün kataloğu");
  json cats = json::array();
  for (auto& c : db.categories()) cats.push_back({{"slug", c.slug}, {"name", c.name}, {"selected", c.slug == q.cat}});
  json items = json::array();
  for (auto& p : page.items) items.push_back(product_vm(p));
  const std::string base = "/" + fe;
  vm["q"] = q.q;
  vm["sort"] = q.sort;
  vm["categories"] = cats;
  vm["items"] = items;
  vm["total"] = page.total;
  vm["total_fmt"] = fmt_int(page.total);
  vm["page"] = page.page;
  vm["pages"] = page.pages;
  vm["prev_href"] = page.page > 1 ? href(base, q, page.page - 1) : "";
  vm["next_href"] = page.page < page.pages ? href(base, q, page.page + 1) : "";
  if (is_htmx(r)) {
    vm["fragment"] = true;
    return html(view::view(fe, "catalog_results", vm));
  }
  return html(view::view(fe, "catalog", vm));
}

Resp product_page(const Req& r) {
  auto fe = get(r.params, "fe");
  auto id = to_int(get(r.params, "id"));
  if (!valid_fe(fe) || !id) return not_found_html();
  auto p = thread_db().product(*id);
  if (!p) return not_found_html();
  auto vm = view::page(r, "catalog", p->name, p->description);
  vm["product"] = product_vm(*p);
  return html(view::view(fe, "product", vm));
}

Resp cart_page(const Req& r) {
  auto fe = get(r.params, "fe");
  if (!valid_fe(fe)) return not_found_html();
  auto sid = existing_session(r);
  auto vm = view::page(r, "catalog", "Sepet", "Alışveriş sepeti");
  vm["cart"] = cart_vm(sid.empty() ? Cart{} : thread_db().cart(sid));
  return html(view::view(fe, "cart", vm));
}

Resp cart_add_form(const Req& r) {
  auto fe = get(r.params, "fe");
  if (!valid_fe(fe)) return not_found_html();
  auto pid = to_int(get(r.form, "product_id"));
  auto qty = to_int(get(r.form, "qty")).value_or(1);
  if (!pid || qty < 1 || qty > 99) return html("Geçersiz istek", 400);
  auto& db = thread_db();
  if (!db.product(*pid)) return not_found_html();
  Resp out;
  auto sid = session_id(r, out);
  db.cart_add(sid, *pid, qty);
  if (is_htmx(r)) {
    auto body = view::view(fe, "badge", {{"cart_count", db.cart(sid).count}, {"fragment", true}});
    out.body = std::move(body);
    return out;
  }
  auto res = redirect(safe_referer(r, "/" + fe + "/catalog"));
  for (auto& hd : out.headers) res.headers.push_back(hd);
  return res;
}

Resp cart_item_form(const Req& r) {
  auto fe = get(r.params, "fe");
  auto item = to_int(get(r.params, "item_id"));
  auto qty = to_int(get(r.form, "qty"));
  if (!valid_fe(fe) || !item) return not_found_html();
  if (!qty || *qty > 99) return html("Geçersiz adet", 400);
  auto sid = existing_session(r);
  auto& db = thread_db();
  if (sid.empty() || !db.cart_set_qty(sid, *item, *qty)) return not_found_html();
  if (is_htmx(r)) {
    json vm{{"cart", cart_vm(db.cart(sid))}, {"fragment", true}};
    return html(view::view(fe, "cart_body", vm));
  }
  return redirect("/" + fe + "/catalog/cart");
}

Resp api_products(const Req& r) {
  auto& db = thread_db();
  json j = db.products(parse_query(r));
  j["categories"] = db.categories();
  return json_resp(j.dump());
}

Resp api_product(const Req& r) {
  auto id = to_int(get(r.params, "id"));
  if (!id) return not_found_json();
  auto p = thread_db().product(*id);
  return p ? json_resp(json(*p).dump()) : not_found_json();
}

Resp api_cart_get(const Req& r) {
  auto sid = existing_session(r);
  return cart_json(sid.empty() ? Cart{} : thread_db().cart(sid));
}

Resp api_cart_add(const Req& r) {
  auto body = json::parse(r.body, nullptr, false);
  if (body.is_discarded() || !body.is_object() || !body.contains("product_id") || !body["product_id"].is_number_integer())
    return json_resp(R"({"error":"bad_request"})", 400);
  int pid = body["product_id"].get<int>();
  if (body.contains("qty") && !body["qty"].is_number_integer()) return json_resp(R"({"error":"bad_request"})", 400);
  int qty = body.value("qty", 1);
  if (qty < 1 || qty > 99) return json_resp(R"({"error":"bad_request"})", 400);
  auto& db = thread_db();
  if (!db.product(pid)) return not_found_json();
  Resp out;
  auto sid = session_id(r, out);
  db.cart_add(sid, pid, qty);
  auto res = cart_json(db.cart(sid), 201);
  res.headers = out.headers;
  return res;
}

Resp api_cart_patch(const Req& r) {
  auto item = to_int(get(r.params, "item_id"));
  auto body = json::parse(r.body, nullptr, false);
  if (!item) return not_found_json();
  if (body.is_discarded() || !body.is_object() || !body.contains("qty") || !body["qty"].is_number_integer())
    return json_resp(R"({"error":"bad_request"})", 400);
  int qty = body["qty"].get<int>();
  if (qty < 0 || qty > 99) return json_resp(R"({"error":"bad_request"})", 400);
  auto sid = existing_session(r);
  auto& db = thread_db();
  if (sid.empty() || !db.cart_set_qty(sid, *item, qty)) return not_found_json();
  return cart_json(db.cart(sid));
}

Resp api_cart_delete(const Req& r) {
  auto item = to_int(get(r.params, "item_id"));
  auto sid = existing_session(r);
  auto& db = thread_db();
  if (!item || sid.empty() || !db.cart_set_qty(sid, *item, 0)) return not_found_json();
  return cart_json(db.cart(sid));
}

}  // namespace bs::h
