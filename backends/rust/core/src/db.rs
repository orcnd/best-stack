//! SQLite data layer (same queries and pragmas as shared/cpp/src/db.cpp).
use crate::model::*;
use r2d2::{Pool, PooledConnection};
use r2d2_sqlite::SqliteConnectionManager;
use rusqlite::{params, Connection, OptionalExtension, Row};
use std::collections::BTreeMap;
use std::sync::OnceLock;

pub type Conn = PooledConnection<SqliteConnectionManager>;
static POOL: OnceLock<Pool<SqliteConnectionManager>> = OnceLock::new();

pub fn init(path: &str, size: u32) {
    let mgr = SqliteConnectionManager::file(path).with_init(|c| {
        c.busy_timeout(std::time::Duration::from_millis(5000))?;
        c.execute_batch("PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL; PRAGMA temp_store = MEMORY; PRAGMA mmap_size = 268435456;")
    });
    let pool = Pool::builder().max_size(size).build(mgr).expect("sqlite pool");
    let _ = POOL.set(pool);
}

pub fn conn() -> Conn { POOL.get().expect("db::init not called").get().expect("sqlite connection") }

const PRODUCT_COLS: &str = "p.id, p.category_id, c.name, p.name, p.description, p.image, p.price_cents, p.stock, p.rating";
const CARD_COLS: &str = "id, board_id, position, col, title, description, assignee, due_date, labels";

fn product(r: &Row, o: usize) -> rusqlite::Result<Product> {
    Ok(Product {
        id: r.get(o)?, category_id: r.get(o + 1)?, category: r.get(o + 2)?, name: r.get(o + 3)?, description: r.get(o + 4)?,
        image: r.get(o + 5)?, price_cents: r.get(o + 6)?, stock: r.get(o + 7)?, rating: r.get(o + 8)?,
    })
}

fn card(r: &Row) -> rusqlite::Result<Card> {
    let labels: String = r.get(8)?;
    Ok(Card {
        id: r.get(0)?, board_id: r.get(1)?, position: r.get(2)?, column: r.get(3)?, title: r.get(4)?,
        description: r.get(5)?, assignee: r.get(6)?, due_date: r.get(7)?,
        labels: labels.split(',').filter(|s| !s.is_empty()).map(String::from).collect(),
    })
}

fn like_pattern(q: &str) -> String {
    let mut out = String::from("%");
    for ch in q.chars() {
        if matches!(ch, '%' | '_' | '\\') { out.push('\\'); }
        out.push(ch);
    }
    out.push('%');
    out
}

pub fn categories(c: &Connection) -> Vec<Category> {
    let mut st = c.prepare_cached("SELECT id, name, slug FROM categories ORDER BY id").unwrap();
    st.query_map([], |r| Ok(Category { id: r.get(0)?, name: r.get(1)?, slug: r.get(2)? })).unwrap().filter_map(Result::ok).collect()
}

pub fn products(c: &Connection, q: &ProductQuery) -> ProductPage {
    let mut wh = String::from(" WHERE 1=1");
    let mut args: Vec<String> = vec![];
    if !q.q.is_empty() { wh += " AND p.name LIKE ? ESCAPE '\\'"; args.push(like_pattern(&q.q)); }
    if !q.cat.is_empty() { wh += " AND c.slug = ?"; args.push(q.cat.clone()); }
    let from = " FROM products p JOIN categories c ON c.id = p.category_id";
    let total: i64 = c.prepare_cached(&format!("SELECT count(*){from}{wh}")).unwrap()
        .query_row(rusqlite::params_from_iter(args.iter()), |r| r.get(0)).unwrap();
    let pages = ((total + PER_PAGE - 1) / PER_PAGE).max(1);
    let page = q.page.clamp(1, pages);
    let order = match q.sort.as_str() { "price_asc" => " ORDER BY p.price_cents ASC, p.id", "price_desc" => " ORDER BY p.price_cents DESC, p.id", _ => " ORDER BY p.id" };
    let sql = format!("SELECT {PRODUCT_COLS}{from}{wh}{order} LIMIT {PER_PAGE} OFFSET {}", (page - 1) * PER_PAGE);
    let mut st = c.prepare_cached(&sql).unwrap();
    let items = st.query_map(rusqlite::params_from_iter(args.iter()), |r| product(r, 0)).unwrap().filter_map(Result::ok).collect();
    ProductPage { items, total, page, pages }
}

pub fn product_by_id(c: &Connection, id: i64) -> Option<Product> {
    c.prepare_cached(&format!("SELECT {PRODUCT_COLS} FROM products p JOIN categories c ON c.id = p.category_id WHERE p.id = ?")).unwrap()
        .query_row([id], |r| product(r, 0)).optional().unwrap()
}

pub fn cart(c: &Connection, sid: &str) -> Cart {
    let mut st = c.prepare_cached(&format!("SELECT ci.id, ci.qty, {PRODUCT_COLS} FROM cart_items ci JOIN products p ON p.id = ci.product_id JOIN categories c ON c.id = p.category_id WHERE ci.session_id = ? ORDER BY ci.id")).unwrap();
    let items: Vec<CartItem> = st.query_map([sid], |r| Ok(CartItem { id: r.get(0)?, qty: r.get(1)?, product: product(r, 2)? })).unwrap().filter_map(Result::ok).collect();
    let count = items.iter().map(|i| i.qty).sum();
    let total_cents = items.iter().map(|i| i.qty * i.product.price_cents).sum();
    Cart { items, count, total_cents }
}

pub fn cart_add(c: &Connection, sid: &str, product_id: i64, qty: i64) {
    if qty <= 0 || product_by_id(c, product_id).is_none() { return; }
    c.prepare_cached("INSERT INTO cart_items (session_id, product_id, qty) VALUES (?, ?, ?) ON CONFLICT (session_id, product_id) DO UPDATE SET qty = qty + excluded.qty").unwrap()
        .execute(params![sid, product_id, qty]).unwrap();
}

pub fn cart_set_qty(c: &Connection, sid: &str, item_id: i64, qty: i64) -> bool {
    let n = if qty <= 0 {
        c.prepare_cached("DELETE FROM cart_items WHERE id = ? AND session_id = ?").unwrap().execute(params![item_id, sid])
    } else {
        c.prepare_cached("UPDATE cart_items SET qty = ? WHERE id = ? AND session_id = ?").unwrap().execute(params![qty, item_id, sid])
    };
    n.unwrap() > 0
}

pub fn boards(c: &Connection) -> Vec<BoardSummary> {
    let mut st = c.prepare_cached("SELECT b.id, b.name, count(c.id) FROM boards b LEFT JOIN cards c ON c.board_id = b.id GROUP BY b.id ORDER BY b.id").unwrap();
    st.query_map([], |r| Ok(BoardSummary { id: r.get(0)?, name: r.get(1)?, card_count: r.get(2)? })).unwrap().filter_map(Result::ok).collect()
}

pub fn board(c: &Connection, id: i64) -> Option<Board> {
    let name: String = c.prepare_cached("SELECT name FROM boards WHERE id = ?").unwrap().query_row([id], |r| r.get(0)).optional().unwrap()?;
    let mut columns: BTreeMap<String, Vec<Card>> = COLUMNS.iter().map(|k| (k.to_string(), vec![])).collect();
    let mut st = c.prepare_cached(&format!("SELECT {CARD_COLS} FROM cards WHERE board_id = ? ORDER BY position, id")).unwrap();
    for cd in st.query_map([id], card).unwrap().filter_map(Result::ok) {
        columns.entry(cd.column.clone()).or_default().push(cd);
    }
    Some(Board { id, name, columns })
}

fn card_by_id(c: &Connection, id: i64) -> Option<Card> {
    c.prepare_cached(&format!("SELECT {CARD_COLS} FROM cards WHERE id = ?")).unwrap().query_row([id], card).optional().unwrap()
}

fn next_position(c: &Connection, board_id: i64, col: &str) -> i64 {
    c.prepare_cached("SELECT COALESCE(MAX(position) + 1, 0) FROM cards WHERE board_id = ? AND col = ?").unwrap()
        .query_row(params![board_id, col], |r| r.get(0)).unwrap()
}

pub fn card_create(c: &Connection, board_id: i64, title: &str, column: &str) -> Option<Card> {
    if !COLUMNS.contains(&column) { return None; }
    c.prepare_cached("SELECT 1 FROM boards WHERE id = ?").unwrap().query_row([board_id], |_| Ok(())).optional().unwrap()?;
    let pos = next_position(c, board_id, column);
    c.prepare_cached("INSERT INTO cards (board_id, col, position, title, description, labels, assignee, due_date) VALUES (?, ?, ?, ?, '', '', '', '')").unwrap()
        .execute(params![board_id, column, pos, title]).unwrap();
    card_by_id(c, c.last_insert_rowid())
}

pub fn card_update(c: &Connection, id: i64, title: Option<&str>, mv: Option<&str>) -> Option<Card> {
    let cur = card_by_id(c, id)?;
    if let Some(t) = title {
        c.prepare_cached("UPDATE cards SET title = ? WHERE id = ?").unwrap().execute(params![t, id]).unwrap();
    }
    if let Some(m) = mv {
        let idx = COLUMNS.iter().position(|k| *k == cur.column).unwrap_or(0) as i64;
        let next = (idx + if m == "right" { 1 } else { -1 }).clamp(0, 3);
        if next != idx {
            let col = COLUMNS[next as usize];
            let pos = next_position(c, cur.board_id, col);
            c.prepare_cached("UPDATE cards SET col = ?, position = ? WHERE id = ?").unwrap().execute(params![col, pos, id]).unwrap();
        }
    }
    card_by_id(c, id)
}

pub fn card_delete(c: &Connection, id: i64) -> bool {
    c.prepare_cached("DELETE FROM cards WHERE id = ?").unwrap().execute([id]).unwrap() > 0
}

fn iso_day(ts: i64) -> String {
    // civil-from-days (Howard Hinnant), UTC
    let z = ts.div_euclid(86400) + 719_468;
    let era = z.div_euclid(146_097);
    let doe = z - era * 146_097;
    let yoe = (doe - doe / 1460 + doe / 36524 - doe / 146_096) / 365;
    let y = yoe + era * 400;
    let doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    let mp = (5 * doy + 2) / 153;
    let d = doy - (153 * mp + 2) / 5 + 1;
    let m = if mp < 10 { mp + 3 } else { mp - 9 };
    format!("{:04}-{:02}-{:02}", if m <= 2 { y + 1 } else { y }, m, d)
}

pub fn dashboard(c: &Connection, range_days: i64) -> Dashboard {
    let to = ANCHOR_TS;
    let from = to - range_days * 86400;
    let (events, users, revenue, buyers): (i64, i64, i64, i64) = c.prepare_cached(
        "SELECT count(*), count(DISTINCT user_id), COALESCE(sum(revenue_cents), 0), count(DISTINCT CASE WHEN type = 'purchase' THEN user_id END) FROM events WHERE ts >= ? AND ts < ?").unwrap()
        .query_row(params![from, to], |r| Ok((r.get(0)?, r.get(1)?, r.get(2)?, r.get(3)?))).unwrap();
    let mut daily: Vec<DayPoint> = (0..range_days).map(|i| DayPoint { day: iso_day(from + i * 86400), events: 0, revenue_cents: 0 }).collect();
    let mut st = c.prepare_cached("SELECT (ts - ?) / 86400 AS di, count(*), COALESCE(sum(revenue_cents), 0) FROM events WHERE ts >= ? AND ts < ? GROUP BY di").unwrap();
    for (di, n, rev) in st.query_map(params![from, from, to], |r| Ok((r.get::<_, i64>(0)?, r.get(1)?, r.get(2)?))).unwrap().filter_map(Result::ok) {
        if (0..range_days).contains(&di) { daily[di as usize].events = n; daily[di as usize].revenue_cents = rev; }
    }
    let countries = c.prepare_cached("SELECT country, count(*) n FROM events WHERE ts >= ? AND ts < ? GROUP BY country ORDER BY n DESC, country LIMIT 8").unwrap()
        .query_map(params![from, to], |r| Ok(CountryRow { country: r.get(0)?, events: r.get(1)? })).unwrap().filter_map(Result::ok).collect();
    let top_items = c.prepare_cached("SELECT item, sum(revenue_cents) r, count(*) FROM events WHERE type = 'purchase' AND ts >= ? AND ts < ? GROUP BY item ORDER BY r DESC, item LIMIT 10").unwrap()
        .query_map(params![from, to], |r| Ok(TopItem { item: r.get(0)?, revenue_cents: r.get(1)?, purchases: r.get(2)? })).unwrap().filter_map(Result::ok).collect();
    Dashboard {
        range_days,
        kpis: Kpis { events, users, revenue_cents: revenue, conversion: if users > 0 { buyers as f64 / users as f64 } else { 0.0 } },
        daily, countries, top_items,
    }
}

#[cfg(test)]
mod tests {
    #[test]
    fn iso_day_matches_anchor() {
        assert_eq!(super::iso_day(super::ANCHOR_TS), "2026-11-01");
        assert_eq!(super::iso_day(super::ANCHOR_TS - 86400), "2026-10-31");
    }
}
