// Direct HTML writers. The literal text is copied from shared/cpp/templates/*.html; where the template
// has a `{% ... %}` statement on its own line, the newline that followed it is kept (inja runs with
// trim_blocks off), so the output is byte-identical to the original backends.
#include "pages.hpp"

#include <charconv>

#include "bsf/html.hpp"

namespace bsf::pages {
namespace {

void base(std::string& o, const Fe& fe) {
  o += '/';
  o += fe.name;
}

void layout_top(std::string& o, const Fe& fe, const Layout& l) {
  o += R"~(<!doctype html>
<html lang="tr">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>)~";
  esc(o, l.title);
  o += R"~( · Best Stack</title>
<meta name="description" content=")~";
  esc(o, l.description);
  o += R"~(">
<link rel="stylesheet" href="/static/app.css">
)~";
  if (fe.htmx)
    o += R"~(
<script src="/static/vendor/htmx.min.js" defer></script>
<script src="/static/js/htmx/htmx-config.js" defer></script>
)~";
  o += '\n';
  if (fe.alpine) {
    o += "\n<script src=\"/static/js/alpine/";
    esc(o, l.nav);
    o += R"~(.js" defer></script>
<script src="/static/vendor/alpine.min.js" defer></script>
)~";
  }
  o += '\n';
  if (fe.vanilla) {
    o += "\n<script type=\"module\" src=\"/static/js/vanilla/";
    esc(o, l.nav);
    o += ".js\"></script>\n";
  }
  o += R"~(
</head>
<body>
<header class="site-header" data-testid="site-header">
  <a class="brand" href=")~";
  base(o, fe);
  o += R"~(/catalog">Best Stack</a>
  <nav class="main-nav">
    <a data-testid="nav-catalog" href=")~";
  base(o, fe);
  o += "/catalog\"";
  if (l.nav == "catalog") o += " class=\"active\"";
  o += R"~(>Katalog</a>
    <a data-testid="nav-kanban" href=")~";
  base(o, fe);
  o += "/kanban/1\"";
  if (l.nav == "kanban") o += " class=\"active\"";
  o += R"~(>Kanban</a>
    <a data-testid="nav-dashboard" href=")~";
  base(o, fe);
  o += "/dashboard\"";
  if (l.nav == "dashboard") o += " class=\"active\"";
  o += R"~(>Dashboard</a>
  </nav>
  <a class="cart-link" href=")~";
  base(o, fe);
  o += "/catalog/cart\">Sepet ";
  badge(o, fe, l.cart_count);
  o += R"~(</a>
</header>
<main class="container">
)~";
}

void layout_bottom(std::string& o) { o += "</main>\n</body>\n</html>\n"; }

/// Escaped catalog link (the original builds the href, then html-escapes it: '&' → "&amp;").
void catalog_href(std::string& o, const Fe& fe, const bs::ProductQuery& q, int page) {
  base(o, fe);
  o += "/catalog";
  bool first = true;
  auto add = [&](const char* k, std::string_view v) {
    if (v.empty()) return;
    o += first ? "?" : "&amp;";
    first = false;
    o += k;
    o += '=';
    url_enc(o, v);
  };
  add("q", q.q);
  add("cat", q.cat);
  add("sort", q.sort);
  if (page > 1) {
    char buf[16];
    auto n = std::to_chars(buf, buf + sizeof buf, page).ptr - buf;
    add("page", std::string_view(buf, n));
  }
}

void htmx_get_change(std::string& o, const Fe& fe) {
  o += " hx-get=\"";
  base(o, fe);
  o += R"~(/catalog" hx-trigger="change" hx-include="closest form")~";
}

}  // namespace

void badge(std::string& o, const Fe& fe, int cart_count) {
  o += R"~(<span class="badge" id="cart-badge" data-testid="cart-badge")~";
  if (fe.alpine) o += R"~( x-data x-text="$store.cart.count")~";
  o += '>';
  integer(o, cart_count);
  o += "</span>\n";
}

// ---------------------------------------------------------------- catalog

void catalog(std::string& o, const Fe& fe, const Layout& l, const CatalogView& v) {
  layout_top(o, fe, l);
  o += "\n<div";
  if (fe.alpine) o += R"~( x-data="shop")~";
  o += R"~(>
<h1>Ürün Kataloğu</h1>
<form class="toolbar" data-testid="search-form" method="get" action=")~";
  base(o, fe);
  o += "/catalog\"";
  if (fe.htmx) {
    o += " hx-get=\"";
    base(o, fe);
    o += R"~(/catalog" hx-target="#product-results" hx-swap="outerHTML" hx-push-url="true" hx-sync="this:replace")~";
  }
  if (fe.alpine) o += R"~( @submit.prevent="search()")~";
  o += R"~(>
  <input type="search" name="q" value=")~";
  esc(o, v.q.q);
  o += R"~(" placeholder="Ürün ara…" data-testid="search-input")~";
  if (fe.htmx) {
    o += " hx-get=\"";
    base(o, fe);
    o += R"~(/catalog" hx-trigger="input changed delay:250ms, search" hx-include="closest form")~";
  }
  if (fe.alpine) o += R"~( x-model="q" @input.debounce.250ms="search()")~";
  o += R"~(>
  <select name="cat" data-testid="category-filter")~";
  if (fe.htmx) htmx_get_change(o, fe);
  if (fe.alpine) o += R"~( x-model="cat" @change="search()")~";
  o += R"~(>
    <option value="">Tüm kategoriler</option>
)~";
  for (auto& c : v.categories) {
    o += "\n    <option value=\"";
    esc(o, c.slug);
    o += '"';
    if (c.slug == v.q.cat) o += " selected";
    o += '>';
    esc(o, c.name);
    o += "</option>\n";
  }
  o += R"~(
  </select>
  <select name="sort" data-testid="sort-select")~";
  if (fe.htmx) htmx_get_change(o, fe);
  if (fe.alpine) o += R"~( x-model="sort" @change="search()")~";
  o += ">\n    <option value=\"\"";
  if (v.q.sort.empty()) o += " selected";
  o += ">Varsayılan sıralama</option>\n    <option value=\"price_asc\"";
  if (v.q.sort == "price_asc") o += " selected";
  o += ">Fiyat (artan)</option>\n    <option value=\"price_desc\"";
  if (v.q.sort == "price_desc") o += " selected";
  o += R"~(>Fiyat (azalan)</option>
  </select>
  <button type="submit">Ara</button>
</form>
)~";
  catalog_results(o, fe, v);
  o += "\n</div>\n";
  layout_bottom(o);
  o += '\n';
}

void catalog_results(std::string& o, const Fe& fe, const CatalogView& v) {
  const auto& pg = v.page;
  o += "<section id=\"product-results\"";
  if (fe.htmx) o += R"~( hx-target="#product-results" hx-swap="outerHTML" hx-push-url="true")~";
  o += ">\n";
  if (fe.alpine) o += "\n<div x-show=\"!ready\">\n";
  o += "\n  <p class=\"result-count\">";
  grouped(o, pg.total);
  o += " ürün</p>\n";
  if (!pg.items.empty()) {
    o += "\n  <div class=\"grid\" data-testid=\"product-grid\">\n";
    for (auto& p : pg.items) {
      o += R"~(
    <article class="product-card" data-testid="product-card">
      <a class="thumb" href=")~";
      base(o, fe);
      o += "/catalog/p/";
      integer(o, p.id);
      o += "\"><img src=\"";
      esc(o, p.image);
      o += "\" alt=\"";
      esc(o, p.name);
      o += R"~(" width="320" height="240"></a>
      <h2 class="product-name" data-testid="product-name"><a href=")~";
      base(o, fe);
      o += "/catalog/p/";
      integer(o, p.id);
      o += "\">";
      esc(o, p.name);
      o += "</a></h2>\n      <p class=\"product-category\">";
      esc(o, p.category);
      o += "</p>\n      <p class=\"product-price\" data-testid=\"product-price\">";
      money(o, p.price_cents);
      o += "</p>\n      <form class=\"add-form\" method=\"post\" action=\"";
      base(o, fe);
      o += "/catalog/cart\"";
      if (fe.htmx) {
        o += " hx-post=\"";
        base(o, fe);
        o += R"~(/catalog/cart" hx-target="#cart-badge" hx-swap="outerHTML" hx-push-url="false")~";
      }
      if (fe.alpine) {
        o += " @submit.prevent=\"add(";
        integer(o, p.id);
        o += ")\"";
      }
      o += ">\n        <input type=\"hidden\" name=\"product_id\" value=\"";
      integer(o, p.id);
      o += R"~(">
        <input type="hidden" name="qty" value="1">
        <button type="submit" data-testid="add-to-cart">Sepete ekle</button>
      </form>
    </article>
)~";
    }
    o += "\n  </div>\n";
  } else {
    o += "\n  <p class=\"empty\" data-testid=\"empty-state\">Sonuç bulunamadı.</p>\n";
  }
  o += "\n  <nav class=\"pagination\" data-testid=\"pagination\">\n";
  if (pg.page > 1) {
    o += "\n    <a data-testid=\"page-prev\" href=\"";
    catalog_href(o, fe, v.q, pg.page - 1);
    o += '"';
    if (fe.htmx) {
      o += " hx-get=\"";
      catalog_href(o, fe, v.q, pg.page - 1);
      o += '"';
    }
    if (fe.alpine) {
      o += " @click.prevent=\"go(";
      integer(o, pg.page);
      o += " - 1)\"";
    }
    o += ">‹ Önceki</a>\n";
  } else {
    o += "\n    <span class=\"disabled\">‹ Önceki</span>\n";
  }
  o += "\n    <span class=\"page-info\">Sayfa ";
  integer(o, pg.page);
  o += " / ";
  integer(o, pg.pages);
  o += "</span>\n";
  if (pg.page < pg.pages) {
    o += "\n    <a data-testid=\"page-next\" href=\"";
    catalog_href(o, fe, v.q, pg.page + 1);
    o += '"';
    if (fe.htmx) {
      o += " hx-get=\"";
      catalog_href(o, fe, v.q, pg.page + 1);
      o += '"';
    }
    if (fe.alpine) {
      o += " @click.prevent=\"go(";
      integer(o, pg.page);
      o += " + 1)\"";
    }
    o += ">Sonraki ›</a>\n";
  } else {
    o += "\n    <span class=\"disabled\">Sonraki ›</span>\n";
  }
  o += "\n  </nav>\n";
  if (fe.alpine) {
    o += R"~(
</div>
<template x-if="ready"><div>
  <p class="result-count" x-text="num(total) + ' ürün'"></p>
  <template x-if="items.length > 0">
  <div class="grid" data-testid="product-grid">
    <template x-for="p in items" :key="p.id">
    <article class="product-card" data-testid="product-card">
      <a class="thumb" :href="')~";
    base(o, fe);
    o += R"~(/catalog/p/' + p.id"><img :src="p.image" :alt="p.name" width="320" height="240"></a>
      <h2 class="product-name" data-testid="product-name"><a :href="')~";
    base(o, fe);
    o += R"~(/catalog/p/' + p.id" x-text="p.name"></a></h2>
      <p class="product-category" x-text="p.category"></p>
      <p class="product-price" data-testid="product-price" x-text="money(p.price_cents)"></p>
      <form class="add-form" method="post" action=")~";
    base(o, fe);
    o += R"~(/catalog/cart" @submit.prevent="add(p.id)">
        <input type="hidden" name="product_id" :value="p.id">
        <input type="hidden" name="qty" value="1">
        <button type="submit" data-testid="add-to-cart">Sepete ekle</button>
      </form>
    </article>
    </template>
  </div>
  </template>
  <template x-if="items.length === 0"><p class="empty" data-testid="empty-state">Sonuç bulunamadı.</p></template>
  <nav class="pagination" data-testid="pagination">
    <template x-if="page > 1"><a data-testid="page-prev" :href="hrefFor(page - 1)" @click.prevent="go(page - 1)">‹ Önceki</a></template>
    <template x-if="page <= 1"><span class="disabled">‹ Önceki</span></template>
    <span class="page-info" x-text="'Sayfa ' + page + ' / ' + pages"></span>
    <template x-if="page < pages"><a data-testid="page-next" :href="hrefFor(page + 1)" @click.prevent="go(page + 1)">Sonraki ›</a></template>
    <template x-if="page >= pages"><span class="disabled">Sonraki ›</span></template>
  </nav>
</div></template>
)~";
  }
  o += "\n</section>\n";
}

// ---------------------------------------------------------------- product

void product(std::string& o, const Fe& fe, const Layout& l, const bs::Product& p) {
  layout_top(o, fe, l);
  o += "\n<div";
  if (fe.alpine) o += R"~( x-data="shop")~";
  o += R"~(>
<article class="product-detail" data-testid="product-detail">
  <img src=")~";
  esc(o, p.image);
  o += "\" alt=\"";
  esc(o, p.name);
  o += R"~(" width="320" height="240">
  <div class="info">
    <h1>)~";
  esc(o, p.name);
  o += "</h1>\n    <p class=\"product-category\">";
  esc(o, p.category);
  o += "</p>\n    <p class=\"product-price\" data-testid=\"product-price\">";
  money(o, p.price_cents);
  o += "</p>\n    <p class=\"rating\">";
  rating(o, p.rating);
  o += "</p>\n    <p class=\"stock\">Stok: ";
  integer(o, p.stock);
  o += "</p>\n    <p class=\"description\">";
  esc(o, p.description);
  o += "</p>\n    <form class=\"add-form\" method=\"post\" action=\"";
  base(o, fe);
  o += "/catalog/cart\"";
  if (fe.htmx) {
    o += " hx-post=\"";
    base(o, fe);
    o += R"~(/catalog/cart" hx-target="#cart-badge" hx-swap="outerHTML")~";
  }
  if (fe.alpine) {
    o += " @submit.prevent=\"add(";
    integer(o, p.id);
    o += ")\"";
  }
  o += ">\n      <input type=\"hidden\" name=\"product_id\" value=\"";
  integer(o, p.id);
  o += R"~(">
      <input type="hidden" name="qty" value="1">
      <button type="submit" data-testid="add-to-cart">Sepete ekle</button>
    </form>
  </div>
</article>
<a class="back" href=")~";
  base(o, fe);
  o += "/catalog\">‹ Kataloğa dön</a>\n</div>\n";
  layout_bottom(o);
  o += '\n';
}

// ---------------------------------------------------------------- cart

void cart(std::string& o, const Fe& fe, const Layout& l, const bs::Cart& c) {
  layout_top(o, fe, l);
  o += "\n<div";
  if (fe.alpine) o += R"~( x-data="shop")~";
  o += ">\n<h1>Sepet</h1>\n";
  cart_body(o, fe, c, false);
  o += "\n</div>\n";
  layout_bottom(o);
  o += '\n';
}

void cart_body(std::string& o, const Fe& fe, const bs::Cart& c, bool fragment) {
  o += "<section id=\"cart\"";
  if (fe.htmx) o += R"~( hx-target="#cart" hx-swap="outerHTML")~";
  o += ">\n";
  if (fe.alpine) o += "\n<div x-show=\"!ready\">\n";
  o += '\n';
  if (!c.items.empty()) {
    o += R"~(
  <table class="cart-table" data-testid="cart-table">
    <thead><tr><th>Ürün</th><th>Fiyat</th><th>Adet</th><th>Tutar</th><th></th></tr></thead>
    <tbody>
)~";
    for (auto& it : c.items) {
      auto item_action = [&] {
        base(o, fe);
        o += "/catalog/cart/";
        integer(o, it.id);
        o += '"';
        if (fe.htmx) {
          o += " hx-post=\"";
          base(o, fe);
          o += "/catalog/cart/";
          integer(o, it.id);
          o += '"';
        }
      };
      o += "\n      <tr data-testid=\"cart-row\">\n        <td><a href=\"";
      base(o, fe);
      o += "/catalog/p/";
      integer(o, it.product.id);
      o += "\">";
      esc(o, it.product.name);
      o += "</a></td>\n        <td>";
      money(o, it.product.price_cents);
      o += "</td>\n        <td><form class=\"qty-form\" method=\"post\" action=\"";
      item_action();
      if (fe.alpine) {
        o += " @submit.prevent=\"setQty(";
        integer(o, it.id);
        o += ", $event.target.elements.qty.value)\"";
      }
      o += ">\n              <input type=\"number\" name=\"qty\" min=\"0\" max=\"99\" value=\"";
      integer(o, it.qty);
      o += R"~(" data-testid="cart-qty">
              <button type="submit" data-testid="cart-update">Güncelle</button></form></td>
        <td>)~";
      money(o, (long)it.qty * it.product.price_cents);
      o += "</td>\n        <td><form method=\"post\" action=\"";
      item_action();
      if (fe.alpine) {
        o += " @submit.prevent=\"setQty(";
        integer(o, it.id);
        o += ", 0)\"";
      }
      o += R"~(>
              <input type="hidden" name="qty" value="0">
              <button type="submit" data-testid="cart-remove">Kaldır</button></form></td>
      </tr>
)~";
    }
    o += R"~(
    </tbody>
  </table>
  <p class="cart-total">Toplam: <strong data-testid="cart-total">)~";
    money(o, c.total_cents);
    o += "</strong></p>\n";
  } else {
    o += "\n  <p class=\"empty\" data-testid=\"empty-state\">Sepetiniz boş.</p>\n";
  }
  o += '\n';
  if (fe.alpine) {
    o += R"~(
</div>
<template x-if="ready"><div>
  <template x-if="cart.items.length > 0"><div>
  <table class="cart-table" data-testid="cart-table">
    <thead><tr><th>Ürün</th><th>Fiyat</th><th>Adet</th><th>Tutar</th><th></th></tr></thead>
    <tbody>
      <template x-for="it in cart.items" :key="it.id">
      <tr data-testid="cart-row">
        <td><a :href="')~";
    base(o, fe);
    o += R"~(/catalog/p/' + it.product.id" x-text="it.product.name"></a></td>
        <td x-text="money(it.product.price_cents)"></td>
        <td><form class="qty-form" method="post" :action="')~";
    base(o, fe);
    o += R"~(/catalog/cart/' + it.id" @submit.prevent="setQty(it.id, $event.target.elements.qty.value)">
              <input type="number" name="qty" min="0" max="99" :value="it.qty" data-testid="cart-qty">
              <button type="submit" data-testid="cart-update">Güncelle</button></form></td>
        <td x-text="money(it.qty * it.product.price_cents)"></td>
        <td><form method="post" :action="')~";
    base(o, fe);
    o += R"~(/catalog/cart/' + it.id" @submit.prevent="setQty(it.id, 0)">
              <input type="hidden" name="qty" value="0">
              <button type="submit" data-testid="cart-remove">Kaldır</button></form></td>
      </tr>
      </template>
    </tbody>
  </table>
  <p class="cart-total">Toplam: <strong data-testid="cart-total" x-text="money(cart.total_cents)"></strong></p>
  </div></template>
  <template x-if="cart.items.length === 0"><p class="empty" data-testid="empty-state">Sepetiniz boş.</p></template>
</div></template>
)~";
  }
  o += "\n</section>\n";
  if (fragment && fe.htmx) {
    o += R"~(
<span class="badge" id="cart-badge" data-testid="cart-badge" hx-swap-oob="true">)~";
    integer(o, c.count);
    o += "</span>\n";
  }
  o += '\n';
}

// ---------------------------------------------------------------- kanban

void kanban(std::string& o, const Fe& fe, const Layout& l, const BoardView& v) {
  const auto& b = v.board;
  layout_top(o, fe, l);
  o += "\n<div";
  if (fe.alpine) {
    o += " x-data=\"kanban(";
    integer(o, b.id);
    o += ")\"";
  }
  o += ">\n<div class=\"board-head\">\n  <h1>";
  esc(o, b.name);
  o += "</h1>\n  <nav class=\"board-tabs\" data-testid=\"board-select\">\n";
  for (auto& t : v.boards) {
    o += "\n    <a href=\"";
    base(o, fe);
    o += "/kanban/";
    integer(o, t.id);
    o += '"';
    if (t.id == b.id) o += " class=\"active\"";
    o += '>';
    esc(o, t.name);
    o += "</a>\n";
  }
  o += "\n  </nav>\n</div>\n<form class=\"new-card\" method=\"post\" action=\"";
  base(o, fe);
  o += "/kanban/";
  integer(o, b.id);
  o += "/cards\"";
  if (fe.htmx) {
    o += " hx-post=\"";
    base(o, fe);
    o += "/kanban/";
    integer(o, b.id);
    o += R"~(/cards" hx-target="#board" hx-swap="outerHTML" hx-on::after-request="if (event.detail.successful) this.reset()")~";
  }
  if (fe.alpine) o += R"~( @submit.prevent="create($event.target)")~";
  o += R"~(>
  <input type="text" name="title" maxlength="200" required placeholder="Yeni kart başlığı" data-testid="new-card-input">
  <input type="hidden" name="column" value="backlog">
  <button type="submit" data-testid="new-card-submit">Kart ekle</button>
</form>
)~";
  board(o, fe, v);
  o += "\n</div>\n";
  layout_bottom(o);
  o += '\n';
}

void board(std::string& o, const Fe& fe, const BoardView& v) {
  static constexpr std::string_view kLabels[] = {"Backlog", "Yapılacak", "Yapılıyor", "Bitti"};
  const auto& keys = bs::kColumns();
  o += "<div class=\"board\" id=\"board\"";
  if (fe.htmx) o += R"~( hx-target="#board" hx-swap="outerHTML")~";
  if (fe.alpine) o += R"~( x-show="!ready")~";
  o += ">\n";
  static const std::vector<bs::Card> kNone;
  for (size_t i = 0; i < keys.size(); i++) {
    auto it = v.board.columns.find(keys[i]);
    const auto& cards = it == v.board.columns.end() ? kNone : it->second;
    o += "\n  <section class=\"column\" data-testid=\"column-";
    esc(o, keys[i]);
    o += "\">\n    <h2>";
    esc(o, kLabels[i]);
    o += " <span class=\"count\">";
    integer(o, (long long)cards.size());
    o += "</span></h2>\n";
    if (!cards.empty()) {
      o += "\n    <ul class=\"cards\">\n";
      for (auto& c : cards) {
        auto action = [&] {
          base(o, fe);
          o += "/kanban/cards/";
          integer(o, c.id);
          o += '"';
          if (fe.htmx) {
            o += " hx-post=\"";
            base(o, fe);
            o += "/kanban/cards/";
            integer(o, c.id);
            o += '"';
          }
        };
        auto alpine_call = [&](std::string_view fn, std::string_view rest) {
          if (!fe.alpine) return;
          o += " @submit.prevent=\"";
          o += fn;
          integer(o, c.id);
          o += rest;
        };
        o += "\n      <li class=\"card\" data-testid=\"card\" data-card-id=\"";
        integer(o, c.id);
        o += "\">\n        <form class=\"title-form\" method=\"post\" action=\"";
        action();
        alpine_call("update(", ", { title: $event.target.elements.title.value })\"");
        o += ">\n          <input class=\"card-title\" type=\"text\" name=\"title\" value=\"";
        esc(o, c.title);
        o += R"~(" maxlength="200" data-testid="card-title">
          <button type="submit" class="small" data-testid="card-edit">Kaydet</button>
        </form>
        <p class="card-meta"><span class="labels">)~";
        for (size_t k = 0; k < c.labels.size(); k++) {
          if (k) o += ", ";
          esc(o, c.labels[k]);
        }
        o += "</span> <span class=\"assignee\">";
        esc(o, c.assignee);
        o += "</span> <span class=\"due\">";
        esc(o, c.due_date);
        o += "</span></p>\n        <div class=\"card-actions\">\n          <form method=\"post\" action=\"";
        action();
        alpine_call("update(", ", { move: 'left' })\"");
        o += R"~(><input type="hidden" name="move" value="left"><button type="submit" class="small" data-testid="card-move-left")~";
        if (i == 0) o += " disabled";
        o += ">←</button></form>\n          <form method=\"post\" action=\"";
        action();
        alpine_call("update(", ", { move: 'right' })\"");
        o += R"~(><input type="hidden" name="move" value="right"><button type="submit" class="small" data-testid="card-move-right")~";
        if (i + 1 == keys.size()) o += " disabled";
        o += ">→</button></form>\n          <form method=\"post\" action=\"";
        action();
        alpine_call("remove(", ")\"");
        o += R"~(><input type="hidden" name="delete" value="1"><button type="submit" class="small danger" data-testid="card-delete">Sil</button></form>
        </div>
      </li>
)~";
      }
      o += "\n    </ul>\n";
    } else {
      o += "\n    <p class=\"empty\" data-testid=\"empty-state\">Kart yok.</p>\n";
    }
    o += "\n  </section>\n";
  }
  o += "\n</div>\n";
  if (fe.alpine) {
    o += R"~(
<template x-if="ready">
<div class="board">
  <template x-for="col in columns" :key="col.key">
  <section class="column" :data-testid="'column-' + col.key">
    <h2><span x-text="col.label"></span> <span class="count" x-text="col.cards.length"></span></h2>
    <template x-if="col.cards.length > 0">
    <ul class="cards">
      <template x-for="c in col.cards" :key="c.id">
      <li class="card" data-testid="card" :data-card-id="c.id">
        <form class="title-form" method="post" :action="')~";
    base(o, fe);
    o += R"~(/kanban/cards/' + c.id" @submit.prevent="update(c.id, { title: $event.target.elements.title.value })">
          <input class="card-title" type="text" name="title" :value="c.title" maxlength="200" data-testid="card-title">
          <button type="submit" class="small" data-testid="card-edit">Kaydet</button>
        </form>
        <p class="card-meta"><span class="labels" x-text="c.labels.join(', ')"></span> <span class="assignee" x-text="c.assignee"></span> <span class="due" x-text="c.due_date"></span></p>
        <div class="card-actions">
          <form method="post" :action="')~";
    base(o, fe);
    o += R"~(/kanban/cards/' + c.id" @submit.prevent="update(c.id, { move: 'left' })"><input type="hidden" name="move" value="left"><button type="submit" class="small" data-testid="card-move-left" :disabled="col.key === 'backlog'">←</button></form>
          <form method="post" :action="')~";
    base(o, fe);
    o += R"~(/kanban/cards/' + c.id" @submit.prevent="update(c.id, { move: 'right' })"><input type="hidden" name="move" value="right"><button type="submit" class="small" data-testid="card-move-right" :disabled="col.key === 'done'">→</button></form>
          <form method="post" :action="')~";
    base(o, fe);
    o += R"~(/kanban/cards/' + c.id" @submit.prevent="remove(c.id)"><input type="hidden" name="delete" value="1"><button type="submit" class="small danger" data-testid="card-delete">Sil</button></form>
        </div>
      </li>
      </template>
    </ul>
    </template>
    <template x-if="col.cards.length === 0"><p class="empty" data-testid="empty-state">Kart yok.</p></template>
  </section>
  </template>
</div>
</template>
)~";
  }
  o += '\n';
}

// ---------------------------------------------------------------- dashboard

void dashboard(std::string& o, const Fe& fe, const Layout& l, const DashboardView& v) {
  static constexpr std::pair<int, std::string_view> kRanges[] = {{7, "Son 7 gün"}, {30, "Son 30 gün"}, {90, "Son 90 gün"}};
  layout_top(o, fe, l);
  o += "\n<div";
  if (fe.alpine) o += R"~( x-data="dashboard")~";
  o += R"~(>
<div class="dash-head">
  <h1>Analitik</h1>
  <form class="range-form" method="get" action=")~";
  base(o, fe);
  o += "/dashboard\"";
  if (fe.htmx) {
    o += " hx-get=\"";
    base(o, fe);
    o += R"~(/dashboard" hx-target="#dashboard-body" hx-swap="outerHTML" hx-push-url="true" hx-trigger="submit, change")~";
  }
  if (fe.alpine) o += R"~( @submit.prevent="load()")~";
  o += ">\n    <select name=\"range\" data-testid=\"range-select\"";
  if (fe.alpine) o += R"~( x-model="range" @change="load()")~";
  o += ">\n";
  for (auto& [value, label] : kRanges) {
    o += "\n      <option value=\"";
    integer(o, value);
    o += '"';
    if (value == v.range) o += " selected";
    o += '>';
    esc(o, label);
    o += "</option>\n";
  }
  o += R"~(
    </select>
    <button type="submit">Uygula</button>
  </form>
</div>
)~";
  dashboard_body(o, fe, v);
  o += "\n</div>\n";
  layout_bottom(o);
  o += '\n';
}

void dashboard_body(std::string& o, const Fe& fe, const DashboardView& v) {
  const auto& d = v.d;
  o += "<section id=\"dashboard-body\">\n";
  if (fe.alpine) o += "\n<div x-show=\"!ready\">\n";
  o += R"~(
  <div class="kpis">
    <div class="kpi"><span class="kpi-label">Olay</span><strong data-testid="kpi-events">)~";
  grouped(o, d.events);
  o += R"~(</strong></div>
    <div class="kpi"><span class="kpi-label">Tekil kullanıcı</span><strong data-testid="kpi-users">)~";
  grouped(o, d.users);
  o += R"~(</strong></div>
    <div class="kpi"><span class="kpi-label">Gelir</span><strong data-testid="kpi-revenue">)~";
  money(o, d.revenue_cents);
  o += R"~(</strong></div>
    <div class="kpi"><span class="kpi-label">Dönüşüm</span><strong data-testid="kpi-conversion">)~";
  pct(o, d.conversion);
  o += R"~(</strong></div>
  </div>
  <div class="panel"><h2>Günlük olaylar</h2><div class="chart" data-testid="chart-daily">)~";
  chart_svg(o, d.daily);
  o += R"~(</div></div>
  <div class="two-col">
    <div class="panel"><h2>Ülkeler</h2>
      <table data-testid="country-table"><thead><tr><th>Ülke</th><th>Olay</th></tr></thead>
        <tbody>
)~";
  for (auto& [country, events] : d.countries) {
    o += "\n          <tr><td>";
    esc(o, country);
    o += "</td><td>";
    grouped(o, events);
    o += "</td></tr>\n";
  }
  o += R"~(
        </tbody></table></div>
    <div class="panel"><h2>En çok gelir getirenler</h2>
      <table data-testid="top-items-table"><thead><tr><th>Ürün</th><th>Satış</th><th>Gelir</th></tr></thead>
        <tbody>
)~";
  for (auto& it : d.top_items) {
    o += "\n          <tr><td>";
    esc(o, it.item);
    o += "</td><td>";
    grouped(o, it.purchases);
    o += "</td><td>";
    money(o, it.revenue_cents);
    o += "</td></tr>\n";
  }
  o += "\n        </tbody></table></div>\n  </div>\n";
  if (fe.alpine) {
    o += R"~(
</div>
<template x-if="ready"><div>
  <div class="kpis">
    <div class="kpi"><span class="kpi-label">Olay</span><strong data-testid="kpi-events" x-text="num(d.kpis.events)"></strong></div>
    <div class="kpi"><span class="kpi-label">Tekil kullanıcı</span><strong data-testid="kpi-users" x-text="num(d.kpis.users)"></strong></div>
    <div class="kpi"><span class="kpi-label">Gelir</span><strong data-testid="kpi-revenue" x-text="money(d.kpis.revenue_cents)"></strong></div>
    <div class="kpi"><span class="kpi-label">Dönüşüm</span><strong data-testid="kpi-conversion" x-text="pct(d.kpis.conversion)"></strong></div>
  </div>
  <div class="panel"><h2>Günlük olaylar</h2><div class="chart" data-testid="chart-daily" x-html="chart(d.daily)"></div></div>
  <div class="two-col">
    <div class="panel"><h2>Ülkeler</h2>
      <table data-testid="country-table"><thead><tr><th>Ülke</th><th>Olay</th></tr></thead>
        <tbody><template x-for="c in d.countries" :key="c.country"><tr><td x-text="c.country"></td><td x-text="num(c.events)"></td></tr></template></tbody></table></div>
    <div class="panel"><h2>En çok gelir getirenler</h2>
      <table data-testid="top-items-table"><thead><tr><th>Ürün</th><th>Satış</th><th>Gelir</th></tr></thead>
        <tbody><template x-for="it in d.top_items" :key="it.item"><tr><td x-text="it.item"></td><td x-text="num(it.purchases)"></td><td x-text="money(it.revenue_cents)"></td></tr></template></tbody></table></div>
  </div>
</div></template>
)~";
  }
  o += "\n</section>\n";
}

}  // namespace bsf::pages
