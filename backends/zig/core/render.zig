//! HTML for the three C++-style frontends, written directly (Zig has no mature template engine).
//! Mirrors shared/cpp/templates/*.html; visible markup must stay identical (parity tests check pixels).
const std = @import("std");
const db = @import("db.zig");
const fmt = @import("format.zig");
const A = std.mem.Allocator;

pub const Fe = enum { htmx, alpine, vanilla };

pub fn parseFe(s: []const u8) ?Fe {
    return std.meta.stringToEnum(Fe, s);
}

pub const Out = struct {
    a: A,
    buf: std.ArrayList(u8) = .empty,

    pub fn raw(o: *Out, s: []const u8) !void {
        try o.buf.appendSlice(o.a, s);
    }
    pub fn text(o: *Out, s: []const u8) !void {
        for (s) |ch| switch (ch) {
            '&' => try o.raw("&amp;"),
            '<' => try o.raw("&lt;"),
            '>' => try o.raw("&gt;"),
            '"' => try o.raw("&quot;"),
            '\'' => try o.raw("&#39;"),
            else => try o.buf.append(o.a, ch),
        };
    }
    pub fn print(o: *Out, comptime f: []const u8, args: anytype) !void {
        try o.raw(try std.fmt.allocPrint(o.a, f, args));
    }
    /// raw string only for frontend `want`
    pub fn when(o: *Out, fe: Fe, want: Fe, s: []const u8) !void {
        if (fe == want) try o.raw(s);
    }
};

pub const Page = struct { nav: []const u8, title: []const u8, description: []const u8, cart_count: i64 };

fn base(fe: Fe) []const u8 {
    return switch (fe) {
        .htmx => "/htmx",
        .alpine => "/alpine",
        .vanilla => "/vanilla",
    };
}

pub fn layoutTop(o: *Out, fe: Fe, p: Page) !void {
    const b = base(fe);
    try o.raw("<!doctype html>\n<html lang=\"tr\">\n<head>\n<meta charset=\"utf-8\">\n<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">\n<title>");
    try o.text(p.title);
    try o.raw(" · Best Stack</title>\n<meta name=\"description\" content=\"");
    try o.text(p.description);
    try o.raw("\">\n<link rel=\"stylesheet\" href=\"/static/app.css\">\n");
    switch (fe) {
        .htmx => try o.raw("<script src=\"/static/vendor/htmx.min.js\" defer></script>\n<script src=\"/static/js/htmx/htmx-config.js\" defer></script>\n"),
        .alpine => try o.print("<script src=\"/static/js/alpine/{s}.js\" defer></script>\n<script src=\"/static/vendor/alpine.min.js\" defer></script>\n", .{p.nav}),
        .vanilla => try o.print("<script type=\"module\" src=\"/static/js/vanilla/{s}.js\"></script>\n", .{p.nav}),
    }
    try o.print("</head>\n<body>\n<header class=\"site-header\" data-testid=\"site-header\">\n  <a class=\"brand\" href=\"{s}/catalog\">Best Stack</a>\n  <nav class=\"main-nav\">\n", .{b});
    const navs = [_][3][]const u8{ .{ "catalog", "/catalog", "Katalog" }, .{ "kanban", "/kanban/1", "Kanban" }, .{ "dashboard", "/dashboard", "Dashboard" } };
    for (navs) |n| {
        try o.print("    <a data-testid=\"nav-{s}\" href=\"{s}{s}\"", .{ n[0], b, n[1] });
        if (std.mem.eql(u8, p.nav, n[0])) try o.raw(" class=\"active\"");
        try o.print(">{s}</a>\n", .{n[2]});
    }
    try o.print("  </nav>\n  <a class=\"cart-link\" href=\"{s}/catalog/cart\">Sepet ", .{b});
    try badge(o, fe, p.cart_count);
    try o.raw("</a>\n</header>\n<main class=\"container\">\n");
}

pub fn layoutBottom(o: *Out) !void {
    try o.raw("</main>\n</body>\n</html>\n");
}

pub fn badge(o: *Out, fe: Fe, count: i64) !void {
    try o.raw("<span class=\"badge\" id=\"cart-badge\" data-testid=\"cart-badge\"");
    try o.when(fe, .alpine, " x-data x-text=\"$store.cart.count\"");
    try o.print(">{d}</span>", .{count});
}

// ---------- catalog ----------
pub const CatalogCtx = struct {
    q: []const u8,
    sort: []const u8,
    cat: []const u8,
    categories: []const db.Category,
    page: db.ProductPage,
    prev_href: []const u8,
    next_href: []const u8,
};

pub fn catalogPage(o: *Out, fe: Fe, p: Page, c: CatalogCtx) !void {
    const b = base(fe);
    try layoutTop(o, fe, p);
    try o.raw("<div");
    try o.when(fe, .alpine, " x-data=\"shop\"");
    try o.print(">\n<h1>Ürün Kataloğu</h1>\n<form class=\"toolbar\" data-testid=\"search-form\" method=\"get\" action=\"{s}/catalog\"", .{b});
    if (fe == .htmx) try o.print(" hx-get=\"{s}/catalog\" hx-target=\"#product-results\" hx-swap=\"outerHTML\" hx-push-url=\"true\" hx-sync=\"this:replace\"", .{b});
    try o.when(fe, .alpine, " @submit.prevent=\"search()\"");
    try o.raw(">\n  <input type=\"search\" name=\"q\" value=\"");
    try o.text(c.q);
    try o.raw("\" placeholder=\"Ürün ara…\" data-testid=\"search-input\"");
    if (fe == .htmx) try o.print(" hx-get=\"{s}/catalog\" hx-trigger=\"input changed delay:250ms, search\" hx-include=\"closest form\"", .{b});
    try o.when(fe, .alpine, " x-model=\"q\" @input.debounce.250ms=\"search()\"");
    try o.raw(">\n  <select name=\"cat\" data-testid=\"category-filter\"");
    if (fe == .htmx) try o.print(" hx-get=\"{s}/catalog\" hx-trigger=\"change\" hx-include=\"closest form\"", .{b});
    try o.when(fe, .alpine, " x-model=\"cat\" @change=\"search()\"");
    try o.raw(">\n    <option value=\"\">Tüm kategoriler</option>\n");
    for (c.categories) |cat| {
        try o.raw("    <option value=\"");
        try o.text(cat.slug);
        try o.raw("\"");
        if (std.mem.eql(u8, cat.slug, c.cat)) try o.raw(" selected");
        try o.raw(">");
        try o.text(cat.name);
        try o.raw("</option>\n");
    }
    try o.raw("  </select>\n  <select name=\"sort\" data-testid=\"sort-select\"");
    if (fe == .htmx) try o.print(" hx-get=\"{s}/catalog\" hx-trigger=\"change\" hx-include=\"closest form\"", .{b});
    try o.when(fe, .alpine, " x-model=\"sort\" @change=\"search()\"");
    try o.raw(">\n");
    const sorts = [_][2][]const u8{ .{ "", "Varsayılan sıralama" }, .{ "price_asc", "Fiyat (artan)" }, .{ "price_desc", "Fiyat (azalan)" } };
    for (sorts) |s| {
        try o.print("    <option value=\"{s}\"", .{s[0]});
        if (std.mem.eql(u8, s[0], c.sort)) try o.raw(" selected");
        try o.print(">{s}</option>\n", .{s[1]});
    }
    try o.raw("  </select>\n  <button type=\"submit\">Ara</button>\n</form>\n");
    try catalogResults(o, fe, c);
    try o.raw("\n</div>\n");
    try layoutBottom(o);
}

pub fn catalogResults(o: *Out, fe: Fe, c: CatalogCtx) !void {
    const b = base(fe);
    const pg = c.page;
    try o.raw("<section id=\"product-results\"");
    try o.when(fe, .htmx, " hx-target=\"#product-results\" hx-swap=\"outerHTML\" hx-push-url=\"true\"");
    try o.raw(">\n");
    try o.when(fe, .alpine, "<div x-show=\"!ready\">\n");
    try o.print("  <p class=\"result-count\">{s} ürün</p>\n", .{try fmt.int(o.a, pg.total)});
    if (pg.items.len > 0) {
        try o.raw("  <div class=\"grid\" data-testid=\"product-grid\">\n");
        for (pg.items) |it| {
            try o.print("    <article class=\"product-card\" data-testid=\"product-card\">\n      <a class=\"thumb\" href=\"{s}/catalog/p/{d}\"><img src=\"", .{ b, it.id });
            try o.text(it.image);
            try o.raw("\" alt=\"");
            try o.text(it.name);
            try o.print("\" width=\"320\" height=\"240\"></a>\n      <h2 class=\"product-name\" data-testid=\"product-name\"><a href=\"{s}/catalog/p/{d}\">", .{ b, it.id });
            try o.text(it.name);
            try o.raw("</a></h2>\n      <p class=\"product-category\">");
            try o.text(it.category);
            try o.print("</p>\n      <p class=\"product-price\" data-testid=\"product-price\">{s}</p>\n", .{try fmt.money(o.a, it.price_cents)});
            try addForm(o, fe, it.id, true);
            try o.raw("    </article>\n");
        }
        try o.raw("  </div>\n");
    } else {
        try o.raw("  <p class=\"empty\" data-testid=\"empty-state\">Sonuç bulunamadı.</p>\n");
    }
    try o.raw("  <nav class=\"pagination\" data-testid=\"pagination\">\n");
    try pageLink(o, fe, "page-prev", c.prev_href, "‹ Önceki", "go({d} - 1)", pg.page);
    try o.print("    <span class=\"page-info\">Sayfa {d} / {d}</span>\n", .{ pg.page, pg.pages });
    try pageLink(o, fe, "page-next", c.next_href, "Sonraki ›", "go({d} + 1)", pg.page);
    try o.raw("  </nav>\n");
    if (fe == .alpine) {
        try o.raw("</div>\n");
        try o.raw(@embedFile("alpine/alpine_catalog_client.html"));
        try o.raw("\n");
    }
    try o.raw("</section>\n");
}

fn pageLink(o: *Out, fe: Fe, testid: []const u8, href: []const u8, label: []const u8, comptime alpine_click: []const u8, page: i64) !void {
    if (href.len == 0) return o.print("    <span class=\"disabled\">{s}</span>\n", .{label});
    try o.print("    <a data-testid=\"{s}\" href=\"", .{testid});
    try o.text(href);
    try o.raw("\"");
    if (fe == .htmx) {
        try o.raw(" hx-get=\"");
        try o.text(href);
        try o.raw("\"");
    }
    if (fe == .alpine) try o.print(" @click.prevent=\"" ++ alpine_click ++ "\"", .{page});
    try o.print(">{s}</a>\n", .{label});
}

fn addForm(o: *Out, fe: Fe, id: i64, in_results: bool) !void {
    const b = base(fe);
    try o.print("      <form class=\"add-form\" method=\"post\" action=\"{s}/catalog/cart\"", .{b});
    if (fe == .htmx) {
        try o.print(" hx-post=\"{s}/catalog/cart\" hx-target=\"#cart-badge\" hx-swap=\"outerHTML\"", .{b});
        if (in_results) try o.raw(" hx-push-url=\"false\"");
    }
    if (fe == .alpine) try o.print(" @submit.prevent=\"add({d})\"", .{id});
    try o.print(">\n        <input type=\"hidden\" name=\"product_id\" value=\"{d}\">\n        <input type=\"hidden\" name=\"qty\" value=\"1\">\n        <button type=\"submit\" data-testid=\"add-to-cart\">Sepete ekle</button>\n      </form>\n", .{id});
}

pub fn productPage(o: *Out, fe: Fe, p: Page, pr: db.Product) !void {
    try layoutTop(o, fe, p);
    try o.raw("<div");
    try o.when(fe, .alpine, " x-data=\"shop\"");
    try o.raw(">\n<article class=\"product-detail\" data-testid=\"product-detail\">\n  <img src=\"");
    try o.text(pr.image);
    try o.raw("\" alt=\"");
    try o.text(pr.name);
    try o.raw("\" width=\"320\" height=\"240\">\n  <div class=\"info\">\n    <h1>");
    try o.text(pr.name);
    try o.raw("</h1>\n    <p class=\"product-category\">");
    try o.text(pr.category);
    try o.print("</p>\n    <p class=\"product-price\" data-testid=\"product-price\">{s}</p>\n    <p class=\"rating\">{s}</p>\n    <p class=\"stock\">Stok: {d}</p>\n    <p class=\"description\">", .{ try fmt.money(o.a, pr.price_cents), try fmt.rating(o.a, pr.rating), pr.stock });
    try o.text(pr.description);
    try o.raw("</p>\n");
    try addForm(o, fe, pr.id, false);
    try o.print("  </div>\n</article>\n<a class=\"back\" href=\"{s}/catalog\">‹ Kataloğa dön</a>\n</div>\n", .{base(fe)});
    try layoutBottom(o);
}

pub fn cartPage(o: *Out, fe: Fe, p: Page, c: db.Cart) !void {
    try layoutTop(o, fe, p);
    try o.raw("<div");
    try o.when(fe, .alpine, " x-data=\"shop\"");
    try o.raw(">\n<h1>Sepet</h1>\n");
    try cartBody(o, fe, c, false);
    try o.raw("</div>\n");
    try layoutBottom(o);
}

pub fn cartBody(o: *Out, fe: Fe, c: db.Cart, fragment: bool) !void {
    const b = base(fe);
    try o.raw("<section id=\"cart\"");
    try o.when(fe, .htmx, " hx-target=\"#cart\" hx-swap=\"outerHTML\"");
    try o.raw(">\n");
    try o.when(fe, .alpine, "<div x-show=\"!ready\">\n");
    if (c.items.len > 0) {
        try o.raw("  <table class=\"cart-table\" data-testid=\"cart-table\">\n    <thead><tr><th>Ürün</th><th>Fiyat</th><th>Adet</th><th>Tutar</th><th></th></tr></thead>\n    <tbody>\n");
        for (c.items) |it| {
            try o.print("      <tr data-testid=\"cart-row\">\n        <td><a href=\"{s}/catalog/p/{d}\">", .{ b, it.product.id });
            try o.text(it.product.name);
            try o.print("</a></td>\n        <td>{s}</td>\n        <td><form class=\"qty-form\" method=\"post\" action=\"{s}/catalog/cart/{d}\"", .{ try fmt.money(o.a, it.product.price_cents), b, it.id });
            if (fe == .htmx) try o.print(" hx-post=\"{s}/catalog/cart/{d}\"", .{ b, it.id });
            if (fe == .alpine) try o.print(" @submit.prevent=\"setQty({d}, $event.target.elements.qty.value)\"", .{it.id});
            try o.print(">\n              <input type=\"number\" name=\"qty\" min=\"0\" max=\"99\" value=\"{d}\" data-testid=\"cart-qty\">\n              <button type=\"submit\" data-testid=\"cart-update\">Güncelle</button></form></td>\n        <td>{s}</td>\n        <td><form method=\"post\" action=\"{s}/catalog/cart/{d}\"", .{ it.qty, try fmt.money(o.a, it.qty * it.product.price_cents), b, it.id });
            if (fe == .htmx) try o.print(" hx-post=\"{s}/catalog/cart/{d}\"", .{ b, it.id });
            if (fe == .alpine) try o.print(" @submit.prevent=\"setQty({d}, 0)\"", .{it.id});
            try o.raw(">\n              <input type=\"hidden\" name=\"qty\" value=\"0\">\n              <button type=\"submit\" data-testid=\"cart-remove\">Kaldır</button></form></td>\n      </tr>\n");
        }
        try o.print("    </tbody>\n  </table>\n  <p class=\"cart-total\">Toplam: <strong data-testid=\"cart-total\">{s}</strong></p>\n", .{try fmt.money(o.a, c.total_cents)});
    } else {
        try o.raw("  <p class=\"empty\" data-testid=\"empty-state\">Sepetiniz boş.</p>\n");
    }
    if (fe == .alpine) {
        try o.raw("</div>\n");
        try o.raw(@embedFile("alpine/alpine_cart_client.html"));
        try o.raw("\n");
    }
    try o.raw("</section>\n");
    if (fragment and fe == .htmx) try o.print("<span class=\"badge\" id=\"cart-badge\" data-testid=\"cart-badge\" hx-swap-oob=\"true\">{d}</span>\n", .{c.count});
}

// ---------- kanban ----------
const column_labels = [_][]const u8{ "Backlog", "Yapılacak", "Yapılıyor", "Bitti" };

pub fn kanbanPage(o: *Out, fe: Fe, p: Page, bd: db.Board, all: []const db.BoardSummary) !void {
    const b = base(fe);
    try layoutTop(o, fe, p);
    try o.raw("<div");
    if (fe == .alpine) try o.print(" x-data=\"kanban({d})\"", .{bd.id});
    try o.raw(">\n<div class=\"board-head\">\n  <h1>");
    try o.text(bd.name);
    try o.raw("</h1>\n  <nav class=\"board-tabs\" data-testid=\"board-select\">\n");
    for (all) |t| {
        try o.print("    <a href=\"{s}/kanban/{d}\"", .{ b, t.id });
        if (t.id == bd.id) try o.raw(" class=\"active\"");
        try o.raw(">");
        try o.text(t.name);
        try o.raw("</a>\n");
    }
    try o.print("  </nav>\n</div>\n<form class=\"new-card\" method=\"post\" action=\"{s}/kanban/{d}/cards\"", .{ b, bd.id });
    if (fe == .htmx) try o.print(" hx-post=\"{s}/kanban/{d}/cards\" hx-target=\"#board\" hx-swap=\"outerHTML\" hx-on::after-request=\"if (event.detail.successful) this.reset()\"", .{ b, bd.id });
    try o.when(fe, .alpine, " @submit.prevent=\"create($event.target)\"");
    try o.raw(">\n  <input type=\"text\" name=\"title\" maxlength=\"200\" required placeholder=\"Yeni kart başlığı\" data-testid=\"new-card-input\">\n  <input type=\"hidden\" name=\"column\" value=\"backlog\">\n  <button type=\"submit\" data-testid=\"new-card-submit\">Kart ekle</button>\n</form>\n");
    try board(o, fe, bd);
    try o.raw("</div>\n");
    try layoutBottom(o);
}

pub fn board(o: *Out, fe: Fe, bd: db.Board) !void {
    const b = base(fe);
    try o.raw("<div class=\"board\" id=\"board\"");
    try o.when(fe, .htmx, " hx-target=\"#board\" hx-swap=\"outerHTML\"");
    try o.when(fe, .alpine, " x-show=\"!ready\"");
    try o.raw(">\n");
    for (db.columns, 0..) |key, i| {
        const cards = db.column(&bd, key);
        try o.print("  <section class=\"column\" data-testid=\"column-{s}\">\n    <h2>{s} <span class=\"count\">{d}</span></h2>\n", .{ key, column_labels[i], cards.len });
        if (cards.len > 0) {
            try o.raw("    <ul class=\"cards\">\n");
            for (cards) |c| {
                try o.print("      <li class=\"card\" data-testid=\"card\" data-card-id=\"{d}\">\n        <form class=\"title-form\" method=\"post\" action=\"{s}/kanban/cards/{d}\"", .{ c.id, b, c.id });
                try cardAttrs(o, fe, c.id, "update({d}, {{ title: $event.target.elements.title.value }})");
                try o.raw(">\n          <input class=\"card-title\" type=\"text\" name=\"title\" value=\"");
                try o.text(c.title);
                try o.raw("\" maxlength=\"200\" data-testid=\"card-title\">\n          <button type=\"submit\" class=\"small\" data-testid=\"card-edit\">Kaydet</button>\n        </form>\n        <p class=\"card-meta\"><span class=\"labels\">");
                for (c.labels, 0..) |l, li| {
                    if (li > 0) try o.raw(", ");
                    try o.text(l);
                }
                try o.raw("</span> <span class=\"assignee\">");
                try o.text(c.assignee);
                try o.raw("</span> <span class=\"due\">");
                try o.text(c.due_date);
                try o.raw("</span></p>\n        <div class=\"card-actions\">\n");
                try o.print("          <form method=\"post\" action=\"{s}/kanban/cards/{d}\"", .{ b, c.id });
                try cardAttrs(o, fe, c.id, "update({d}, {{ move: 'left' }})");
                try o.print("><input type=\"hidden\" name=\"move\" value=\"left\"><button type=\"submit\" class=\"small\" data-testid=\"card-move-left\"{s}>←</button></form>\n", .{if (i == 0) " disabled" else ""});
                try o.print("          <form method=\"post\" action=\"{s}/kanban/cards/{d}\"", .{ b, c.id });
                try cardAttrs(o, fe, c.id, "update({d}, {{ move: 'right' }})");
                try o.print("><input type=\"hidden\" name=\"move\" value=\"right\"><button type=\"submit\" class=\"small\" data-testid=\"card-move-right\"{s}>→</button></form>\n", .{if (i == 3) " disabled" else ""});
                try o.print("          <form method=\"post\" action=\"{s}/kanban/cards/{d}\"", .{ b, c.id });
                try cardAttrs(o, fe, c.id, "remove({d})");
                try o.raw("><input type=\"hidden\" name=\"delete\" value=\"1\"><button type=\"submit\" class=\"small danger\" data-testid=\"card-delete\">Sil</button></form>\n        </div>\n      </li>\n");
            }
            try o.raw("    </ul>\n");
        } else {
            try o.raw("    <p class=\"empty\" data-testid=\"empty-state\">Kart yok.</p>\n");
        }
        try o.raw("  </section>\n");
    }
    try o.raw("</div>\n");
    if (fe == .alpine) {
        try o.raw(@embedFile("alpine/alpine_board_client.html"));
        try o.raw("\n");
    }
}

fn cardAttrs(o: *Out, fe: Fe, id: i64, comptime alpine: []const u8) !void {
    if (fe == .htmx) try o.print(" hx-post=\"{s}/kanban/cards/{d}\"", .{ base(fe), id });
    if (fe == .alpine) try o.print(" @submit.prevent=\"" ++ alpine ++ "\"", .{id});
}

// ---------- dashboard ----------
pub fn dashboardPage(o: *Out, fe: Fe, p: Page, d: db.Dashboard) !void {
    const b = base(fe);
    try layoutTop(o, fe, p);
    try o.raw("<div");
    try o.when(fe, .alpine, " x-data=\"dashboard\"");
    try o.print(">\n<div class=\"dash-head\">\n  <h1>Analitik</h1>\n  <form class=\"range-form\" method=\"get\" action=\"{s}/dashboard\"", .{b});
    if (fe == .htmx) try o.print(" hx-get=\"{s}/dashboard\" hx-target=\"#dashboard-body\" hx-swap=\"outerHTML\" hx-push-url=\"true\" hx-trigger=\"submit, change\"", .{b});
    try o.when(fe, .alpine, " @submit.prevent=\"load()\"");
    try o.raw(">\n    <select name=\"range\" data-testid=\"range-select\"");
    try o.when(fe, .alpine, " x-model=\"range\" @change=\"load()\"");
    try o.raw(">\n");
    const ranges = [_]struct { v: i64, l: []const u8 }{ .{ .v = 7, .l = "Son 7 gün" }, .{ .v = 30, .l = "Son 30 gün" }, .{ .v = 90, .l = "Son 90 gün" } };
    for (ranges) |r| try o.print("      <option value=\"{d}\"{s}>{s}</option>\n", .{ r.v, if (r.v == d.range_days) " selected" else "", r.l });
    try o.raw("    </select>\n    <button type=\"submit\">Uygula</button>\n  </form>\n</div>\n");
    try dashboardBody(o, fe, d);
    try o.raw("</div>\n");
    try layoutBottom(o);
}

pub fn dashboardBody(o: *Out, fe: Fe, d: db.Dashboard) !void {
    const a = o.a;
    try o.raw("<section id=\"dashboard-body\">\n");
    try o.when(fe, .alpine, "<div x-show=\"!ready\">\n");
    try o.print(
        \\  <div class="kpis">
        \\    <div class="kpi"><span class="kpi-label">Olay</span><strong data-testid="kpi-events">{s}</strong></div>
        \\    <div class="kpi"><span class="kpi-label">Tekil kullanıcı</span><strong data-testid="kpi-users">{s}</strong></div>
        \\    <div class="kpi"><span class="kpi-label">Gelir</span><strong data-testid="kpi-revenue">{s}</strong></div>
        \\    <div class="kpi"><span class="kpi-label">Dönüşüm</span><strong data-testid="kpi-conversion">{s}</strong></div>
        \\  </div>
        \\  <div class="panel"><h2>Günlük olaylar</h2><div class="chart" data-testid="chart-daily">{s}</div></div>
        \\  <div class="two-col">
        \\    <div class="panel"><h2>Ülkeler</h2>
        \\      <table data-testid="country-table"><thead><tr><th>Ülke</th><th>Olay</th></tr></thead>
        \\        <tbody>
        \\
    , .{ try fmt.int(a, d.kpis.events), try fmt.int(a, d.kpis.users), try fmt.money(a, d.kpis.revenue_cents), try fmt.pct(a, d.kpis.conversion), try fmt.dailyChartSvg(a, d.daily) });
    for (d.countries) |c| {
        try o.raw("          <tr><td>");
        try o.text(c.country);
        try o.print("</td><td>{s}</td></tr>\n", .{try fmt.int(a, c.events)});
    }
    try o.raw("        </tbody></table></div>\n    <div class=\"panel\"><h2>En çok gelir getirenler</h2>\n      <table data-testid=\"top-items-table\"><thead><tr><th>Ürün</th><th>Satış</th><th>Gelir</th></tr></thead>\n        <tbody>\n");
    for (d.top_items) |it| {
        try o.raw("          <tr><td>");
        try o.text(it.item);
        try o.print("</td><td>{s}</td><td>{s}</td></tr>\n", .{ try fmt.int(a, it.purchases), try fmt.money(a, it.revenue_cents) });
    }
    try o.raw("        </tbody></table></div>\n  </div>\n");
    if (fe == .alpine) {
        try o.raw("</div>\n");
        try o.raw(@embedFile("alpine/alpine_dashboard_client.html"));
        try o.raw("\n");
    }
    try o.raw("</section>\n");
}
