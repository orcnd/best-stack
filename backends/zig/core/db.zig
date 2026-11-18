//! Data layer: same queries as shared/cpp/src/db.cpp. Results are allocated in the request arena.
const std = @import("std");
const sql = @import("sqlite.zig");
const A = std.mem.Allocator;

pub const anchor_ts: i64 = 1793491200; // 2026-11-01T00:00:00Z
pub const per_page: i64 = 24;
pub const columns = [_][]const u8{ "backlog", "todo", "doing", "done" };

pub const Category = struct { id: i64, name: []const u8, slug: []const u8 };
pub const Product = struct {
    id: i64,
    category_id: i64,
    category: []const u8,
    name: []const u8,
    description: []const u8,
    image: []const u8,
    price_cents: i64,
    stock: i64,
    rating: f64,
};
pub const ProductQuery = struct { q: []const u8 = "", cat: []const u8 = "", sort: []const u8 = "", page: i64 = 1 };
pub const ProductPage = struct { items: []Product, total: i64, page: i64, pages: i64 };
pub const CartItem = struct { id: i64, qty: i64, product: Product };
pub const Cart = struct { items: []CartItem = &.{}, count: i64 = 0, total_cents: i64 = 0 };
pub const Card = struct {
    id: i64,
    board_id: i64,
    position: i64,
    column: []const u8,
    title: []const u8,
    description: []const u8,
    assignee: []const u8,
    due_date: []const u8,
    labels: [][]const u8,
};
pub const BoardSummary = struct { id: i64, name: []const u8, card_count: i64 };
pub const Columns = struct { backlog: []Card, todo: []Card, doing: []Card, done: []Card };
pub const Board = struct { id: i64, name: []const u8, columns: Columns };
pub const DayPoint = struct { day: []const u8, events: i64, revenue_cents: i64 };
pub const Kpis = struct { events: i64, users: i64, revenue_cents: i64, conversion: f64 };
pub const CountryRow = struct { country: []const u8, events: i64 };
pub const TopItem = struct { item: []const u8, revenue_cents: i64, purchases: i64 };
pub const Dashboard = struct { range_days: i64, kpis: Kpis, daily: []DayPoint, countries: []CountryRow, top_items: []TopItem };

pub fn column(b: *const Board, key: []const u8) []Card {
    if (std.mem.eql(u8, key, "backlog")) return b.columns.backlog;
    if (std.mem.eql(u8, key, "todo")) return b.columns.todo;
    if (std.mem.eql(u8, key, "doing")) return b.columns.doing;
    return b.columns.done;
}

const product_cols = "p.id, p.category_id, c.name, p.name, p.description, p.image, p.price_cents, p.stock, p.rating";
const card_cols = "id, board_id, position, col, title, description, assignee, due_date, labels";

fn readProduct(a: A, s: sql.Stmt, o: c_int) !Product {
    return .{
        .id = s.int(o),
        .category_id = s.int(o + 1),
        .category = try s.text(a, o + 2),
        .name = try s.text(a, o + 3),
        .description = try s.text(a, o + 4),
        .image = try s.text(a, o + 5),
        .price_cents = s.int(o + 6),
        .stock = s.int(o + 7),
        .rating = s.float(o + 8),
    };
}

fn readCard(a: A, s: sql.Stmt) !Card {
    var labels: std.ArrayList([]const u8) = .empty;
    var it = std.mem.splitScalar(u8, try s.text(a, 8), ',');
    while (it.next()) |l| if (l.len > 0) try labels.append(a, l);
    return .{
        .id = s.int(0),
        .board_id = s.int(1),
        .position = s.int(2),
        .column = try s.text(a, 3),
        .title = try s.text(a, 4),
        .description = try s.text(a, 5),
        .assignee = try s.text(a, 6),
        .due_date = try s.text(a, 7),
        .labels = try labels.toOwnedSlice(a),
    };
}

fn likePattern(a: A, q: []const u8) ![]const u8 {
    var out: std.ArrayList(u8) = .empty;
    try out.append(a, '%');
    for (q) |ch| {
        if (ch == '%' or ch == '_' or ch == '\\') try out.append(a, '\\');
        try out.append(a, ch);
    }
    try out.append(a, '%');
    return out.toOwnedSlice(a);
}

pub fn categories(a: A) ![]Category {
    const s = try (try sql.conn()).prepare("SELECT id, name, slug FROM categories ORDER BY id");
    defer s.done();
    var out: std.ArrayList(Category) = .empty;
    while (try s.step()) try out.append(a, .{ .id = s.int(0), .name = try s.text(a, 1), .slug = try s.text(a, 2) });
    return out.toOwnedSlice(a);
}

pub fn products(a: A, q: ProductQuery) !ProductPage {
    const cn = try sql.conn();
    // four fixed statement variants (search on/off × category on/off) keep the statement cache small
    const where = if (q.q.len > 0 and q.cat.len > 0) " WHERE p.name LIKE ?1 ESCAPE '\\' AND c.slug = ?2" else if (q.q.len > 0) " WHERE p.name LIKE ?1 ESCAPE '\\'" else if (q.cat.len > 0) " WHERE c.slug = ?2" else "";
    const from = " FROM products p JOIN categories c ON c.id = p.category_id";
    const pat = if (q.q.len > 0) try likePattern(a, q.q) else "";
    var total: i64 = 0;
    {
        const s = try cn.prepare(try std.fmt.allocPrint(a, "SELECT count(*){s}{s}", .{ from, where }));
        defer s.done();
        if (q.q.len > 0) s.bindText(1, pat);
        if (q.cat.len > 0) s.bindText(2, q.cat);
        if (try s.step()) total = s.int(0);
    }
    const pages = @max(@divTrunc(total + per_page - 1, per_page), 1);
    const page = std.math.clamp(q.page, 1, pages);
    const order = if (std.mem.eql(u8, q.sort, "price_asc")) " ORDER BY p.price_cents ASC, p.id" else if (std.mem.eql(u8, q.sort, "price_desc")) " ORDER BY p.price_cents DESC, p.id" else " ORDER BY p.id";
    const s = try cn.prepare(try std.fmt.allocPrint(a, "SELECT {s}{s}{s}{s} LIMIT ?3 OFFSET ?4", .{ product_cols, from, where, order }));
    defer s.done();
    if (q.q.len > 0) s.bindText(1, pat);
    if (q.cat.len > 0) s.bindText(2, q.cat);
    s.bindInt(3, per_page);
    s.bindInt(4, (page - 1) * per_page);
    var items: std.ArrayList(Product) = .empty;
    while (try s.step()) try items.append(a, try readProduct(a, s, 0));
    return .{ .items = try items.toOwnedSlice(a), .total = total, .page = page, .pages = pages };
}

pub fn productById(a: A, id: i64) !?Product {
    const s = try (try sql.conn()).prepare("SELECT " ++ product_cols ++ " FROM products p JOIN categories c ON c.id = p.category_id WHERE p.id = ?");
    defer s.done();
    s.bindInt(1, id);
    return if (try s.step()) try readProduct(a, s, 0) else null;
}

pub fn cart(a: A, sid: []const u8) !Cart {
    const s = try (try sql.conn()).prepare("SELECT ci.id, ci.qty, " ++ product_cols ++ " FROM cart_items ci JOIN products p ON p.id = ci.product_id JOIN categories c ON c.id = p.category_id WHERE ci.session_id = ? ORDER BY ci.id");
    defer s.done();
    s.bindText(1, sid);
    var items: std.ArrayList(CartItem) = .empty;
    var out: Cart = .{};
    while (try s.step()) {
        const it: CartItem = .{ .id = s.int(0), .qty = s.int(1), .product = try readProduct(a, s, 2) };
        out.count += it.qty;
        out.total_cents += it.qty * it.product.price_cents;
        try items.append(a, it);
    }
    out.items = try items.toOwnedSlice(a);
    return out;
}

pub fn cartAdd(a: A, sid: []const u8, product_id: i64, qty: i64) !void {
    if (qty <= 0 or (try productById(a, product_id)) == null) return;
    const s = try (try sql.conn()).prepare("INSERT INTO cart_items (session_id, product_id, qty) VALUES (?, ?, ?) ON CONFLICT (session_id, product_id) DO UPDATE SET qty = qty + excluded.qty");
    defer s.done();
    s.bindText(1, sid);
    s.bindInt(2, product_id);
    s.bindInt(3, qty);
    _ = try s.step();
}

pub fn cartSetQty(sid: []const u8, item_id: i64, qty: i64) !bool {
    const cn = try sql.conn();
    if (qty <= 0) {
        const s = try cn.prepare("DELETE FROM cart_items WHERE id = ? AND session_id = ?");
        defer s.done();
        s.bindInt(1, item_id);
        s.bindText(2, sid);
        _ = try s.step();
    } else {
        const s = try cn.prepare("UPDATE cart_items SET qty = ? WHERE id = ? AND session_id = ?");
        defer s.done();
        s.bindInt(1, qty);
        s.bindInt(2, item_id);
        s.bindText(3, sid);
        _ = try s.step();
    }
    return cn.changes() > 0;
}

pub fn boards(a: A) ![]BoardSummary {
    const s = try (try sql.conn()).prepare("SELECT b.id, b.name, count(c.id) FROM boards b LEFT JOIN cards c ON c.board_id = b.id GROUP BY b.id ORDER BY b.id");
    defer s.done();
    var out: std.ArrayList(BoardSummary) = .empty;
    while (try s.step()) try out.append(a, .{ .id = s.int(0), .name = try s.text(a, 1), .card_count = s.int(2) });
    return out.toOwnedSlice(a);
}

pub fn board(a: A, id: i64) !?Board {
    const cn = try sql.conn();
    var name: []const u8 = undefined;
    {
        const s = try cn.prepare("SELECT name FROM boards WHERE id = ?");
        defer s.done();
        s.bindInt(1, id);
        if (!try s.step()) return null;
        name = try s.text(a, 0);
    }
    var lists = [_]std.ArrayList(Card){ .empty, .empty, .empty, .empty };
    const s = try cn.prepare("SELECT " ++ card_cols ++ " FROM cards WHERE board_id = ? ORDER BY position, id");
    defer s.done();
    s.bindInt(1, id);
    while (try s.step()) {
        const cd = try readCard(a, s);
        for (columns, 0..) |k, i| if (std.mem.eql(u8, k, cd.column)) try lists[i].append(a, cd);
    }
    return .{ .id = id, .name = name, .columns = .{
        .backlog = try lists[0].toOwnedSlice(a),
        .todo = try lists[1].toOwnedSlice(a),
        .doing = try lists[2].toOwnedSlice(a),
        .done = try lists[3].toOwnedSlice(a),
    } };
}

fn cardById(a: A, id: i64) !?Card {
    const s = try (try sql.conn()).prepare("SELECT " ++ card_cols ++ " FROM cards WHERE id = ?");
    defer s.done();
    s.bindInt(1, id);
    return if (try s.step()) try readCard(a, s) else null;
}

fn nextPosition(board_id: i64, col: []const u8) !i64 {
    const s = try (try sql.conn()).prepare("SELECT COALESCE(MAX(position) + 1, 0) FROM cards WHERE board_id = ? AND col = ?");
    defer s.done();
    s.bindInt(1, board_id);
    s.bindText(2, col);
    _ = try s.step();
    return s.int(0);
}

pub fn validColumn(col: []const u8) bool {
    for (columns) |k| if (std.mem.eql(u8, k, col)) return true;
    return false;
}

pub fn cardCreate(a: A, board_id: i64, title: []const u8, col: []const u8) !?Card {
    if (!validColumn(col)) return null;
    const cn = try sql.conn();
    {
        const s = try cn.prepare("SELECT 1 FROM boards WHERE id = ?");
        defer s.done();
        s.bindInt(1, board_id);
        if (!try s.step()) return null;
    }
    const pos = try nextPosition(board_id, col);
    const s = try cn.prepare("INSERT INTO cards (board_id, col, position, title, description, labels, assignee, due_date) VALUES (?, ?, ?, ?, '', '', '', '')");
    defer s.done();
    s.bindInt(1, board_id);
    s.bindText(2, col);
    s.bindInt(3, pos);
    s.bindText(4, title);
    _ = try s.step();
    return cardById(a, cn.lastInsertRowid());
}

pub fn cardUpdate(a: A, id: i64, title: ?[]const u8, move: ?[]const u8) !?Card {
    const cur = (try cardById(a, id)) orelse return null;
    const cn = try sql.conn();
    if (title) |t| {
        const s = try cn.prepare("UPDATE cards SET title = ? WHERE id = ?");
        defer s.done();
        s.bindText(1, t);
        s.bindInt(2, id);
        _ = try s.step();
    }
    if (move) |m| {
        var idx: i64 = 0;
        for (columns, 0..) |k, i| if (std.mem.eql(u8, k, cur.column)) {
            idx = @intCast(i);
        };
        const next = std.math.clamp(idx + @as(i64, if (std.mem.eql(u8, m, "right")) 1 else -1), 0, 3);
        if (next != idx) {
            const col = columns[@intCast(next)];
            const pos = try nextPosition(cur.board_id, col);
            const s = try cn.prepare("UPDATE cards SET col = ?, position = ? WHERE id = ?");
            defer s.done();
            s.bindText(1, col);
            s.bindInt(2, pos);
            s.bindInt(3, id);
            _ = try s.step();
        }
    }
    return cardById(a, id);
}

pub fn cardDelete(id: i64) !bool {
    const cn = try sql.conn();
    const s = try cn.prepare("DELETE FROM cards WHERE id = ?");
    defer s.done();
    s.bindInt(1, id);
    _ = try s.step();
    return cn.changes() > 0;
}

/// YYYY-MM-DD (UTC) for a unix timestamp — civil-from-days.
pub fn isoDay(a: A, ts: i64) ![]const u8 {
    const z = @divFloor(ts, 86400) + 719468;
    const era = @divFloor(z, 146097);
    const doe = z - era * 146097;
    const yoe = @divFloor(doe - @divFloor(doe, 1460) + @divFloor(doe, 36524) - @divFloor(doe, 146096), 365);
    const doy = doe - (365 * yoe + @divFloor(yoe, 4) - @divFloor(yoe, 100));
    const mp = @divFloor(5 * doy + 2, 153);
    const d = doy - @divFloor(153 * mp + 2, 5) + 1;
    const m = if (mp < 10) mp + 3 else mp - 9;
    const y = yoe + era * 400 + @as(i64, if (m <= 2) 1 else 0);
    return std.fmt.allocPrint(a, "{d:0>4}-{d:0>2}-{d:0>2}", .{ @as(u64, @intCast(y)), @as(u64, @intCast(m)), @as(u64, @intCast(d)) });
}

pub fn dashboard(a: A, range_days: i64) !Dashboard {
    const cn = try sql.conn();
    const to = anchor_ts;
    const from = to - range_days * 86400;
    var k: Kpis = undefined;
    {
        const s = try cn.prepare("SELECT count(*), count(DISTINCT user_id), COALESCE(sum(revenue_cents), 0), count(DISTINCT CASE WHEN type = 'purchase' THEN user_id END) FROM events WHERE ts >= ? AND ts < ?");
        defer s.done();
        s.bindInt(1, from);
        s.bindInt(2, to);
        _ = try s.step();
        k = .{ .events = s.int(0), .users = s.int(1), .revenue_cents = s.int(2), .conversion = 0 };
        if (k.users > 0) k.conversion = @as(f64, @floatFromInt(s.int(3))) / @as(f64, @floatFromInt(k.users));
    }
    const daily = try a.alloc(DayPoint, @intCast(range_days));
    for (daily, 0..) |*d, i| d.* = .{ .day = try isoDay(a, from + @as(i64, @intCast(i)) * 86400), .events = 0, .revenue_cents = 0 };
    {
        const s = try cn.prepare("SELECT (ts - ?) / 86400 AS di, count(*), COALESCE(sum(revenue_cents), 0) FROM events WHERE ts >= ? AND ts < ? GROUP BY di");
        defer s.done();
        s.bindInt(1, from);
        s.bindInt(2, from);
        s.bindInt(3, to);
        while (try s.step()) {
            const di = s.int(0);
            if (di >= 0 and di < range_days) {
                daily[@intCast(di)].events = s.int(1);
                daily[@intCast(di)].revenue_cents = s.int(2);
            }
        }
    }
    var countries: std.ArrayList(CountryRow) = .empty;
    {
        const s = try cn.prepare("SELECT country, count(*) n FROM events WHERE ts >= ? AND ts < ? GROUP BY country ORDER BY n DESC, country LIMIT 8");
        defer s.done();
        s.bindInt(1, from);
        s.bindInt(2, to);
        while (try s.step()) try countries.append(a, .{ .country = try s.text(a, 0), .events = s.int(1) });
    }
    var items: std.ArrayList(TopItem) = .empty;
    {
        const s = try cn.prepare("SELECT item, sum(revenue_cents) r, count(*) FROM events WHERE type = 'purchase' AND ts >= ? AND ts < ? GROUP BY item ORDER BY r DESC, item LIMIT 10");
        defer s.done();
        s.bindInt(1, from);
        s.bindInt(2, to);
        while (try s.step()) try items.append(a, .{ .item = try s.text(a, 0), .revenue_cents = s.int(1), .purchases = s.int(2) });
    }
    return .{ .range_days = range_days, .kpis = k, .daily = daily, .countries = try countries.toOwnedSlice(a), .top_items = try items.toOwnedSlice(a) };
}
