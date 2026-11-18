//! Handlers — same routes, status codes and view logic as shared/cpp/src/handlers_*.cpp.
const std = @import("std");
const db = @import("db.zig");
const fmt = @import("format.zig");
const http = @import("http.zig");
const render = @import("render.zig");
const Req = http.Req;
const Resp = http.Resp;
const Fe = render.Fe;
const A = std.mem.Allocator;

pub const Handler = *const fn (*Req) anyerror!Resp;
pub const Route = struct { method: []const u8, pattern: []const u8, handler: Handler };

pub const routes = [_]Route{
    .{ .method = "GET", .pattern = "/healthz", .handler = healthz },
    .{ .method = "GET", .pattern = "/api/catalog/products", .handler = apiProducts },
    .{ .method = "GET", .pattern = "/api/catalog/products/{id}", .handler = apiProduct },
    .{ .method = "GET", .pattern = "/api/catalog/cart", .handler = apiCartGet },
    .{ .method = "POST", .pattern = "/api/catalog/cart", .handler = apiCartAdd },
    .{ .method = "PATCH", .pattern = "/api/catalog/cart/{item_id}", .handler = apiCartPatch },
    .{ .method = "DELETE", .pattern = "/api/catalog/cart/{item_id}", .handler = apiCartDelete },
    .{ .method = "GET", .pattern = "/api/kanban/boards", .handler = apiBoards },
    .{ .method = "GET", .pattern = "/api/kanban/boards/{id}", .handler = apiBoard },
    .{ .method = "POST", .pattern = "/api/kanban/boards/{id}/cards", .handler = apiCardCreate },
    .{ .method = "PATCH", .pattern = "/api/kanban/cards/{id}", .handler = apiCardPatch },
    .{ .method = "DELETE", .pattern = "/api/kanban/cards/{id}", .handler = apiCardDelete },
    .{ .method = "GET", .pattern = "/api/dashboard", .handler = apiDashboard },
    .{ .method = "GET", .pattern = "/{fe}/catalog", .handler = catalogPage },
    .{ .method = "GET", .pattern = "/{fe}/catalog/p/{id}", .handler = productPage },
    .{ .method = "GET", .pattern = "/{fe}/catalog/cart", .handler = cartPage },
    .{ .method = "POST", .pattern = "/{fe}/catalog/cart", .handler = cartAddForm },
    .{ .method = "POST", .pattern = "/{fe}/catalog/cart/{item_id}", .handler = cartItemForm },
    .{ .method = "GET", .pattern = "/{fe}/kanban/{board_id}", .handler = kanbanPage },
    .{ .method = "POST", .pattern = "/{fe}/kanban/{board_id}/cards", .handler = cardCreateForm },
    .{ .method = "POST", .pattern = "/{fe}/kanban/cards/{id}", .handler = cardUpdateForm },
    .{ .method = "GET", .pattern = "/{fe}/dashboard", .handler = dashboardPage },
};

/// Dispatch using the shared matcher (for servers without their own router). Static files included.
pub fn dispatch(r: *Req) !Resp {
    if (std.mem.eql(u8, r.method, "GET") and std.mem.startsWith(u8, r.path, "/static/")) {
        return http.serveStatic(try http.urlDecode(r.a, r.path[8..])) orelse text(404, "not found");
    }
    for (routes) |rt| {
        if (!std.mem.eql(u8, rt.method, r.method)) continue;
        r.params = .empty;
        if (try http.match(r.a, rt.pattern, r.path, &r.params)) return rt.handler(r);
    }
    return text(404, "not found");
}

// ---------- helpers ----------
fn text(status: u16, body: []const u8) Resp {
    return .{ .status = status, .content_type = "text/plain", .body = body };
}
fn html(body: []const u8) Resp {
    return .{ .body = body };
}
fn htmlStatus(status: u16, body: []const u8) Resp {
    return .{ .status = status, .body = body };
}
fn notFoundHtml() Resp {
    return htmlStatus(404, "<!doctype html><title>404</title><h1>Bulunamadı</h1>");
}
fn json(status: u16, body: []const u8) Resp {
    return .{ .status = status, .content_type = "application/json", .body = body };
}
fn notFoundJson() Resp {
    return json(404, "{\"error\":\"not_found\"}");
}
fn badJson() Resp {
    return json(400, "{\"error\":\"bad_request\"}");
}
fn jsonValue(a: A, status: u16, v: anytype) !Resp {
    return json(status, try std.json.Stringify.valueAlloc(a, v, .{}));
}
fn redirect(a: A, loc: []const u8, extra: []const http.Header) !Resp {
    const hs = try a.alloc(http.Header, extra.len + 1);
    hs[0] = .{ .name = "Location", .value = loc };
    @memcpy(hs[1..], extra);
    return .{ .status = 303, .content_type = "text/plain", .headers = hs };
}

fn toInt(s: []const u8) ?i64 {
    if (s.len == 0 or s.len > 9) return null;
    for (s) |ch| if (ch < '0' or ch > '9') return null;
    return std.fmt.parseInt(i64, s, 10) catch null;
}
fn isHtmx(r: *const Req) bool {
    return std.mem.eql(u8, r.header("hx-request"), "true");
}
fn validSid(s: []const u8) bool {
    if (s.len != 16) return false;
    for (s) |ch| if (!std.ascii.isAlphanumeric(ch)) return false;
    return true;
}
fn existingSession(r: *const Req) []const u8 {
    const ck = http.get(&r.cookies, "bs_sid");
    if (validSid(ck)) return ck;
    const h = r.header("x-session-id");
    return if (validSid(h)) h else "";
}
/// Existing session or a new one (adds the Set-Cookie header to `hdrs`).
fn sessionId(r: *Req, hdrs: *std.ArrayList(http.Header)) ![]const u8 {
    const s = existingSession(r);
    if (s.len > 0) return s;
    const id = try r.a.alloc(u8, 16);
    http.randomHex16(id[0..16]);
    try hdrs.append(r.a, .{ .name = "Set-Cookie", .value = try std.fmt.allocPrint(r.a, "bs_sid={s}; Path=/; SameSite=Lax; HttpOnly", .{id}) });
    return id;
}
fn validTitle(raw: []const u8) ?[]const u8 {
    const t = std.mem.trim(u8, raw, " \t\r\n");
    const n = std.unicode.utf8CountCodepoints(t) catch return null;
    return if (n >= 1 and n <= 200) t else null;
}
fn safeReferer(r: *const Req, fallback: []const u8) []const u8 {
    var ref = r.header("referer");
    if (std.mem.startsWith(u8, ref, "http://") or std.mem.startsWith(u8, ref, "https://")) {
        const after = ref[std.mem.indexOf(u8, ref, "//").? + 2 ..];
        ref = if (std.mem.indexOfScalar(u8, after, '/')) |i| after[i..] else "/";
    }
    if (ref.len == 0 or ref[0] != '/' or std.mem.startsWith(u8, ref, "//")) return fallback;
    return ref;
}
fn page(r: *Req, nav: []const u8, title: []const u8, description: []const u8) !render.Page {
    const sid = existingSession(r);
    const count = if (sid.len > 0) (try db.cart(r.a, sid)).count else 0;
    return .{ .nav = nav, .title = title, .description = description, .cart_count = count };
}
fn feOf(r: *const Req) ?Fe {
    return render.parseFe(r.param("fe"));
}

// ---------- catalog ----------
fn parseQuery(r: *const Req) db.ProductQuery {
    const s = http.get(&r.query, "sort");
    const sort = if (std.mem.eql(u8, s, "price_asc") or std.mem.eql(u8, s, "price_desc")) s else "";
    return .{ .q = http.get(&r.query, "q"), .cat = http.get(&r.query, "cat"), .sort = sort, .page = toInt(http.get(&r.query, "page")) orelse 1 };
}

fn cppEscape(a: A, s: []const u8) ![]const u8 {
    var out: std.ArrayList(u8) = .empty;
    for (s) |ch| {
        if (std.ascii.isAlphanumeric(ch) or ch == '-' or ch == '_' or ch == '.' or ch == '~') {
            try out.append(a, ch);
        } else try out.appendSlice(a, try std.fmt.allocPrint(a, "%{X:0>2}", .{ch}));
    }
    return out.toOwnedSlice(a);
}

fn href(a: A, fe: []const u8, q: db.ProductQuery, pg: i64) ![]const u8 {
    var out: std.ArrayList(u8) = .empty;
    try out.appendSlice(a, try std.fmt.allocPrint(a, "/{s}/catalog", .{fe}));
    var first = true;
    const parts = [_][2][]const u8{ .{ "q", q.q }, .{ "cat", q.cat }, .{ "sort", q.sort } };
    for (parts) |p| if (p[1].len > 0) {
        try out.appendSlice(a, if (first) "?" else "&");
        first = false;
        try out.appendSlice(a, try std.fmt.allocPrint(a, "{s}={s}", .{ p[0], try cppEscape(a, p[1]) }));
    };
    if (pg > 1) try out.appendSlice(a, try std.fmt.allocPrint(a, "{s}page={d}", .{ if (first) "?" else "&", pg }));
    return out.toOwnedSlice(a);
}

fn catalogPage(r: *Req) !Resp {
    const fe = feOf(r) orelse return notFoundHtml();
    const q = parseQuery(r);
    const pg = try db.products(r.a, q);
    const p = try page(r, "catalog", "Ürün Kataloğu", "Best Stack demo ürün kataloğu");
    const ctx: render.CatalogCtx = .{
        .q = q.q, .sort = q.sort, .cat = q.cat, .categories = try db.categories(r.a), .page = pg,
        .prev_href = if (pg.page > 1) try href(r.a, r.param("fe"), q, pg.page - 1) else "",
        .next_href = if (pg.page < pg.pages) try href(r.a, r.param("fe"), q, pg.page + 1) else "",
    };
    var o: render.Out = .{ .a = r.a };
    if (isHtmx(r)) try render.catalogResults(&o, fe, ctx) else try render.catalogPage(&o, fe, p, ctx);
    return html(o.buf.items);
}

fn productPage(r: *Req) !Resp {
    const fe = feOf(r) orelse return notFoundHtml();
    const id = toInt(r.param("id")) orelse return notFoundHtml();
    const pr = (try db.productById(r.a, id)) orelse return notFoundHtml();
    var o: render.Out = .{ .a = r.a };
    try render.productPage(&o, fe, try page(r, "catalog", pr.name, pr.description), pr);
    return html(o.buf.items);
}

fn cartPage(r: *Req) !Resp {
    const fe = feOf(r) orelse return notFoundHtml();
    const sid = existingSession(r);
    const c: db.Cart = if (sid.len > 0) try db.cart(r.a, sid) else .{};
    var o: render.Out = .{ .a = r.a };
    try render.cartPage(&o, fe, try page(r, "catalog", "Sepet", "Alışveriş sepeti"), c);
    return html(o.buf.items);
}

fn cartAddForm(r: *Req) !Resp {
    const fe = feOf(r) orelse return notFoundHtml();
    const pid = toInt(http.get(&r.form, "product_id"));
    const qty = toInt(http.get(&r.form, "qty")) orelse 1;
    if (pid == null or qty < 1 or qty > 99) return htmlStatus(400, "Geçersiz istek");
    if ((try db.productById(r.a, pid.?)) == null) return notFoundHtml();
    var hdrs: std.ArrayList(http.Header) = .empty;
    const sid = try sessionId(r, &hdrs);
    try db.cartAdd(r.a, sid, pid.?, qty);
    if (isHtmx(r)) {
        var o: render.Out = .{ .a = r.a };
        try render.badge(&o, fe, (try db.cart(r.a, sid)).count);
        return .{ .body = o.buf.items, .headers = hdrs.items };
    }
    return redirect(r.a, safeReferer(r, try std.fmt.allocPrint(r.a, "/{s}/catalog", .{r.param("fe")})), hdrs.items);
}

fn cartItemForm(r: *Req) !Resp {
    const fe = feOf(r) orelse return notFoundHtml();
    const item = toInt(r.param("item_id")) orelse return notFoundHtml();
    const qty = toInt(http.get(&r.form, "qty")) orelse return htmlStatus(400, "Geçersiz adet");
    if (qty > 99) return htmlStatus(400, "Geçersiz adet");
    const sid = existingSession(r);
    if (sid.len == 0 or !try db.cartSetQty(sid, item, qty)) return notFoundHtml();
    if (isHtmx(r)) {
        var o: render.Out = .{ .a = r.a };
        try render.cartBody(&o, fe, try db.cart(r.a, sid), true);
        return html(o.buf.items);
    }
    return redirect(r.a, try std.fmt.allocPrint(r.a, "/{s}/catalog/cart", .{r.param("fe")}), &.{});
}

fn apiProducts(r: *Req) !Resp {
    const p = try db.products(r.a, parseQuery(r));
    return jsonValue(r.a, 200, .{ .items = p.items, .total = p.total, .page = p.page, .pages = p.pages, .categories = try db.categories(r.a) });
}

fn apiProduct(r: *Req) !Resp {
    const id = toInt(r.param("id")) orelse return notFoundJson();
    const p = (try db.productById(r.a, id)) orelse return notFoundJson();
    return jsonValue(r.a, 200, p);
}

fn apiCartGet(r: *Req) !Resp {
    const sid = existingSession(r);
    return jsonValue(r.a, 200, if (sid.len > 0) try db.cart(r.a, sid) else db.Cart{});
}

fn bodyObject(r: *Req) ?std.json.ObjectMap {
    const v = std.json.parseFromSliceLeaky(std.json.Value, r.a, r.body, .{}) catch return null;
    return switch (v) {
        .object => |o| o,
        else => null,
    };
}
fn intField(o: std.json.ObjectMap, k: []const u8) ?i64 {
    return switch (o.get(k) orelse return null) {
        .integer => |i| i,
        else => null,
    };
}

fn apiCartAdd(r: *Req) !Resp {
    const b = bodyObject(r) orelse return badJson();
    const pid = intField(b, "product_id") orelse return badJson();
    var qty: i64 = 1;
    if (b.get("qty") != null) qty = intField(b, "qty") orelse return badJson();
    if (qty < 1 or qty > 99) return badJson();
    if ((try db.productById(r.a, pid)) == null) return notFoundJson();
    var hdrs: std.ArrayList(http.Header) = .empty;
    const sid = try sessionId(r, &hdrs);
    try db.cartAdd(r.a, sid, pid, qty);
    var res = try jsonValue(r.a, 201, try db.cart(r.a, sid));
    res.headers = hdrs.items;
    return res;
}

fn apiCartPatch(r: *Req) !Resp {
    const item = toInt(r.param("item_id")) orelse return notFoundJson();
    const b = bodyObject(r) orelse return badJson();
    const qty = intField(b, "qty") orelse return badJson();
    if (qty < 0 or qty > 99) return badJson();
    const sid = existingSession(r);
    if (sid.len == 0 or !try db.cartSetQty(sid, item, qty)) return notFoundJson();
    return jsonValue(r.a, 200, try db.cart(r.a, sid));
}

fn apiCartDelete(r: *Req) !Resp {
    const item = toInt(r.param("item_id")) orelse return notFoundJson();
    const sid = existingSession(r);
    if (sid.len == 0 or !try db.cartSetQty(sid, item, 0)) return notFoundJson();
    return jsonValue(r.a, 200, try db.cart(r.a, sid));
}

// ---------- kanban ----------
fn boardFragment(r: *Req, fe: Fe, board_id: i64) !Resp {
    const bd = (try db.board(r.a, board_id)) orelse return notFoundHtml();
    var o: render.Out = .{ .a = r.a };
    try render.board(&o, fe, bd);
    return html(o.buf.items);
}

fn kanbanPage(r: *Req) !Resp {
    const fe = feOf(r) orelse return notFoundHtml();
    const id = toInt(r.param("board_id")) orelse return notFoundHtml();
    const bd = (try db.board(r.a, id)) orelse return notFoundHtml();
    var o: render.Out = .{ .a = r.a };
    try render.kanbanPage(&o, fe, try page(r, "kanban", try std.fmt.allocPrint(r.a, "{s} · Kanban", .{bd.name}), "Kanban panosu"), bd, try db.boards(r.a));
    return html(o.buf.items);
}

fn cardCreateForm(r: *Req) !Resp {
    const fe = feOf(r) orelse return notFoundHtml();
    const id = toInt(r.param("board_id")) orelse return notFoundHtml();
    const title = validTitle(http.get(&r.form, "title")) orelse return htmlStatus(422, "Geçersiz başlık");
    const col_raw = http.get(&r.form, "column");
    const col = if (col_raw.len == 0) "backlog" else col_raw;
    if ((try db.board(r.a, id)) == null) return notFoundHtml();
    if ((try db.cardCreate(r.a, id, title, col)) == null) return htmlStatus(422, "Geçersiz sütun");
    if (isHtmx(r)) return boardFragment(r, fe, id);
    return redirect(r.a, try std.fmt.allocPrint(r.a, "/{s}/kanban/{d}", .{ r.param("fe"), id }), &.{});
}

fn cardUpdateForm(r: *Req) !Resp {
    const fe = feOf(r) orelse return notFoundHtml();
    const id = toInt(r.param("id")) orelse return notFoundHtml();
    const card = (try db.cardUpdate(r.a, id, null, null)) orelse return notFoundHtml();
    if (std.mem.eql(u8, http.get(&r.form, "delete"), "1")) {
        _ = try db.cardDelete(id);
    } else {
        var title: ?[]const u8 = null;
        if (r.form.get("title")) |raw| title = validTitle(raw) orelse return htmlStatus(422, "Geçersiz başlık");
        const mv = http.get(&r.form, "move");
        const move: ?[]const u8 = if (std.mem.eql(u8, mv, "left") or std.mem.eql(u8, mv, "right")) mv else null;
        _ = try db.cardUpdate(r.a, id, title, move);
    }
    if (isHtmx(r)) return boardFragment(r, fe, card.board_id);
    return redirect(r.a, try std.fmt.allocPrint(r.a, "/{s}/kanban/{d}", .{ r.param("fe"), card.board_id }), &.{});
}

fn apiBoards(r: *Req) !Resp {
    return jsonValue(r.a, 200, try db.boards(r.a));
}

fn apiBoard(r: *Req) !Resp {
    const id = toInt(r.param("id")) orelse return notFoundJson();
    const bd = (try db.board(r.a, id)) orelse return notFoundJson();
    return jsonValue(r.a, 200, bd);
}

fn apiCardCreate(r: *Req) !Resp {
    const id = toInt(r.param("id")) orelse return notFoundJson();
    const b = bodyObject(r) orelse return badJson();
    const raw = switch (b.get("title") orelse return json(422, "{\"error\":\"invalid_title\"}")) {
        .string => |s| s,
        else => return json(422, "{\"error\":\"invalid_title\"}"),
    };
    const title = validTitle(raw) orelse return json(422, "{\"error\":\"invalid_title\"}");
    const col = if (b.get("column")) |v| switch (v) {
        .string => |s| s,
        else => "backlog",
    } else "backlog";
    if (!db.validColumn(col)) return json(422, "{\"error\":\"invalid_column\"}");
    const c = (try db.cardCreate(r.a, id, title, col)) orelse return notFoundJson();
    return jsonValue(r.a, 201, c);
}

fn apiCardPatch(r: *Req) !Resp {
    const id = toInt(r.param("id")) orelse return notFoundJson();
    const b = bodyObject(r) orelse return badJson();
    var title: ?[]const u8 = null;
    if (b.get("title")) |v| switch (v) {
        .string => |s| title = validTitle(s) orelse return json(422, "{\"error\":\"invalid_title\"}"),
        else => return json(422, "{\"error\":\"invalid_title\"}"),
    };
    var move: ?[]const u8 = null;
    if (b.get("move")) |v| switch (v) {
        .string => |s| {
            if (!std.mem.eql(u8, s, "left") and !std.mem.eql(u8, s, "right")) return badJson();
            move = s;
        },
        else => return badJson(),
    };
    const c = (try db.cardUpdate(r.a, id, title, move)) orelse return notFoundJson();
    return jsonValue(r.a, 200, c);
}

fn apiCardDelete(r: *Req) !Resp {
    const id = toInt(r.param("id")) orelse return notFoundJson();
    if (!try db.cardDelete(id)) return notFoundJson();
    return .{ .status = 204, .content_type = "application/json" };
}

// ---------- dashboard ----------
fn parseRange(r: *const Req) ?i64 {
    const v = r.query.get("range") orelse return 30;
    if (v.len == 0 or std.mem.eql(u8, v, "30")) return 30;
    if (std.mem.eql(u8, v, "7")) return 7;
    if (std.mem.eql(u8, v, "90")) return 90;
    return null;
}

fn dashboardPage(r: *Req) !Resp {
    const fe = feOf(r) orelse return notFoundHtml();
    const range = parseRange(r) orelse return htmlStatus(400, "Geçersiz aralık");
    const d = try db.dashboard(r.a, range);
    var o: render.Out = .{ .a = r.a };
    if (isHtmx(r)) try render.dashboardBody(&o, fe, d) else try render.dashboardPage(&o, fe, try page(r, "dashboard", "Analitik", "Analitik dashboard"), d);
    return html(o.buf.items);
}

fn apiDashboard(r: *Req) !Resp {
    const range = parseRange(r) orelse return json(400, "{\"error\":\"invalid_range\"}");
    return jsonValue(r.a, 200, try db.dashboard(r.a, range));
}

fn healthz(_: *Req) !Resp {
    return text(200, "ok");
}
