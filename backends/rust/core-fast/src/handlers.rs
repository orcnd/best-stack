//! Handlers — same routes, status codes, SQL and validation as bs_core::handlers (copied from there);
//! only the rendering differs: typed askama views instead of serde_json::Value + minijinja, and JSON
//! serialized straight from typed structs.
use crate::view::{self, html};
use bs_core::chart::daily_chart_svg;
use bs_core::db::{self, Conn};
use bs_core::handlers::{Handler, Route};
use bs_core::http::{Req, Resp};
use bs_core::model::*;
use serde::Serialize;
use serde_json::Value;

pub fn routes() -> Vec<Route> {
    let r = |method, pattern, handler| Route { method, pattern, handler };
    vec![
        r("GET", "/healthz", healthz as Handler),
        r("GET", "/api/catalog/products", api_products),
        r("GET", "/api/catalog/products/{id}", api_product),
        r("GET", "/api/catalog/cart", api_cart_get),
        r("POST", "/api/catalog/cart", api_cart_add),
        r("PATCH", "/api/catalog/cart/{item_id}", api_cart_patch),
        r("DELETE", "/api/catalog/cart/{item_id}", api_cart_delete),
        r("GET", "/api/kanban/boards", api_boards),
        r("GET", "/api/kanban/boards/{id}", api_board),
        r("POST", "/api/kanban/boards/{id}/cards", api_card_create),
        r("PATCH", "/api/kanban/cards/{id}", api_card_patch),
        r("DELETE", "/api/kanban/cards/{id}", api_card_delete),
        r("GET", "/api/dashboard", api_dashboard),
        r("GET", "/{fe}/catalog", catalog_page),
        r("GET", "/{fe}/catalog/p/{id}", product_page),
        r("GET", "/{fe}/catalog/cart", cart_page),
        r("POST", "/{fe}/catalog/cart", cart_add_form),
        r("POST", "/{fe}/catalog/cart/{item_id}", cart_item_form),
        r("GET", "/{fe}/kanban/{board_id}", kanban_page),
        r("POST", "/{fe}/kanban/{board_id}/cards", card_create_form),
        r("POST", "/{fe}/kanban/cards/{id}", card_update_form),
        r("GET", "/{fe}/dashboard", dashboard_page),
    ]
}

// ---------- helpers ----------
fn valid_fe(fe: &str) -> bool { matches!(fe, "htmx" | "alpine" | "vanilla") }
fn is_htmx(r: &Req) -> bool { r.header("hx-request") == "true" }
fn to_int(s: &str) -> Option<i64> { if s.is_empty() || s.len() > 9 || !s.bytes().all(|b| b.is_ascii_digit()) { None } else { s.parse().ok() } }
fn not_found_html() -> Resp { Resp::new(404, "text/html; charset=utf-8", "<!doctype html><title>404</title><h1>Bulunamadı</h1>") }
fn not_found_json() -> Resp { Resp::json(404, r#"{"error":"not_found"}"#.into()) }
fn bad_json() -> Resp { Resp::json(400, r#"{"error":"bad_request"}"#.into()) }
fn redirect(loc: &str) -> Resp { let mut r = Resp::new(303, "text/plain", ""); r.headers.push(("Location".into(), loc.into())); r }
fn html_status(status: u16, body: &str) -> Resp { Resp::new(status, "text/html; charset=utf-8", body.to_string()) }
fn get<'a>(m: &'a std::collections::HashMap<String, String>, k: &str) -> &'a str { m.get(k).map(String::as_str).unwrap_or("") }

fn valid_sid(s: &str) -> bool { s.len() == 16 && s.bytes().all(|b| b.is_ascii_alphanumeric()) }
fn existing_session(r: &Req) -> String {
    let c = get(&r.cookies, "bs_sid");
    if valid_sid(c) { return c.into(); }
    let h = r.header("x-session-id");
    if valid_sid(h) { h.into() } else { String::new() }
}
fn session_id(r: &Req, out: &mut Vec<(String, String)>) -> String {
    let s = existing_session(r);
    if !s.is_empty() { return s; }
    let mut b = [0u8; 8];
    getrandom::fill(&mut b).unwrap();
    let id: String = b.iter().map(|x| format!("{x:02x}")).collect();
    out.push(("Set-Cookie".into(), format!("bs_sid={id}; Path=/; SameSite=Lax; HttpOnly")));
    id
}

fn valid_title(raw: &str) -> Option<String> {
    let t = raw.trim_matches(|c| c == ' ' || c == '\t' || c == '\r' || c == '\n');
    let n = t.chars().count();
    if n < 1 || n > MAX_TITLE { None } else { Some(t.to_string()) }
}

fn safe_referer(r: &Req, fallback: &str) -> String {
    let mut s = r.header("referer").to_string();
    if s.starts_with("http://") || s.starts_with("https://") {
        let after = &s[s.find("//").unwrap() + 2..];
        s = after.find('/').map(|i| after[i..].to_string()).unwrap_or_else(|| "/".into());
    }
    if s.is_empty() || !s.starts_with('/') || s.starts_with("//") { fallback.into() } else { s }
}

/// Cart badge count for the layout (same query as bs_core's page()).
fn cart_count(c: &Conn, r: &Req) -> i64 {
    let sid = existing_session(r);
    if sid.is_empty() { 0 } else { db::cart(c, &sid).count }
}

// ---------- catalog ----------
fn parse_query(r: &Req) -> ProductQuery {
    let s = get(&r.query, "sort");
    ProductQuery { q: get(&r.query, "q").into(), cat: get(&r.query, "cat").into(),
        sort: if s == "price_asc" || s == "price_desc" { s.into() } else { String::new() }, page: to_int(get(&r.query, "page")).unwrap_or(1) }
}

/// Same unreserved set as the C++ url_encode: alnum and -_.~ stay literal.
const QUERY: &percent_encoding::AsciiSet = &percent_encoding::NON_ALPHANUMERIC.remove(b'-').remove(b'_').remove(b'.').remove(b'~');

fn href(base: &str, q: &ProductQuery, page: i64) -> String {
    let enc = |v: &str| percent_encoding::utf8_percent_encode(v, QUERY).to_string();
    let mut parts: Vec<String> = vec![];
    if !q.q.is_empty() { parts.push(format!("q={}", enc(&q.q))); }
    if !q.cat.is_empty() { parts.push(format!("cat={}", enc(&q.cat))); }
    if !q.sort.is_empty() { parts.push(format!("sort={}", enc(&q.sort))); }
    if page > 1 { parts.push(format!("page={page}")); }
    if parts.is_empty() { format!("{base}/catalog") } else { format!("{base}/catalog?{}", parts.join("&")) }
}

fn catalog_page(r: &Req) -> Resp {
    let fe = r.param("fe");
    if !valid_fe(fe) { return not_found_html(); }
    let c = db::conn();
    let q = parse_query(r);
    let p = db::products(&c, &q);
    let cart_count = cart_count(&c, r);
    let cats = db::categories(&c);
    let base = format!("/{fe}");
    let res = view::Results {
        items: &p.items, total: p.total, page: p.page, pages: p.pages,
        prev_href: if p.page > 1 { href(&base, &q, p.page - 1) } else { String::new() },
        next_href: if p.page < p.pages { href(&base, &q, p.page + 1) } else { String::new() },
    };
    if is_htmx(r) { return Resp::html(html(&view::CatalogResults { fe, r: res })); }
    Resp::html(html(&view::CatalogPage { fe, nav: "catalog", title: "Ürün Kataloğu", description: "Best Stack demo ürün kataloğu", cart_count,
        q: &q.q, cat: &q.cat, sort: &q.sort, categories: &cats, r: res }))
}

fn product_page(r: &Req) -> Resp {
    let fe = r.param("fe");
    let Some(id) = to_int(r.param("id")) else { return not_found_html() };
    if !valid_fe(fe) { return not_found_html(); }
    let c = db::conn();
    let Some(p) = db::product_by_id(&c, id) else { return not_found_html() };
    let cart_count = cart_count(&c, r);
    Resp::html(html(&view::ProductPage { fe, nav: "catalog", title: &p.name, description: &p.description, cart_count, product: &p }))
}

fn cart_page(r: &Req) -> Resp {
    let fe = r.param("fe");
    if !valid_fe(fe) { return not_found_html(); }
    let c = db::conn();
    let sid = existing_session(r);
    let cart_count = cart_count(&c, r);
    let cart = if sid.is_empty() { Cart::default() } else { db::cart(&c, &sid) };
    Resp::html(html(&view::CartPage { fe, nav: "catalog", title: "Sepet", description: "Alışveriş sepeti", cart_count, cart: &cart, fragment: false }))
}

fn cart_add_form(r: &Req) -> Resp {
    let fe = r.param("fe");
    if !valid_fe(fe) { return not_found_html(); }
    let pid = to_int(get(&r.form, "product_id"));
    let qty = to_int(get(&r.form, "qty")).unwrap_or(1);
    let Some(pid) = pid.filter(|_| (1..=99).contains(&qty)) else { return html_status(400, "Geçersiz istek") };
    let c = db::conn();
    if db::product_by_id(&c, pid).is_none() { return not_found_html(); }
    let mut hdrs = vec![];
    let sid = session_id(r, &mut hdrs);
    db::cart_add(&c, &sid, pid, qty);
    let mut res = if is_htmx(r) {
        Resp::html(html(&view::Badge { fe, cart_count: db::cart(&c, &sid).count }))
    } else {
        redirect(&safe_referer(r, &format!("/{fe}/catalog")))
    };
    res.headers.extend(hdrs);
    res
}

fn cart_item_form(r: &Req) -> Resp {
    let fe = r.param("fe");
    let item = to_int(r.param("item_id"));
    if !valid_fe(fe) || item.is_none() { return not_found_html(); }
    let Some(qty) = to_int(get(&r.form, "qty")).filter(|q| *q <= 99) else { return html_status(400, "Geçersiz adet") };
    let sid = existing_session(r);
    let c = db::conn();
    if sid.is_empty() || !db::cart_set_qty(&c, &sid, item.unwrap(), qty) { return not_found_html(); }
    if is_htmx(r) { return Resp::html(html(&view::CartBody { fe, cart: &db::cart(&c, &sid), fragment: true })); }
    redirect(&format!("/{fe}/catalog/cart"))
}

fn api_products(r: &Req) -> Resp {
    let c = db::conn();
    #[derive(Serialize)]
    struct Body<'a> { items: &'a [Product], total: i64, page: i64, pages: i64, categories: &'a [Category] }
    let p = db::products(&c, &parse_query(r));
    let cats = db::categories(&c);
    Resp::json(200, serde_json::to_string(&Body { items: &p.items, total: p.total, page: p.page, pages: p.pages, categories: &cats }).unwrap())
}

fn api_product(r: &Req) -> Resp {
    let Some(id) = to_int(r.param("id")) else { return not_found_json() };
    match db::product_by_id(&db::conn(), id) { Some(p) => Resp::json(200, serde_json::to_string(&p).unwrap()), None => not_found_json() }
}

fn api_cart_get(r: &Req) -> Resp {
    let sid = existing_session(r);
    let cart = if sid.is_empty() { Cart::default() } else { db::cart(&db::conn(), &sid) };
    Resp::json(200, serde_json::to_string(&cart).unwrap())
}

fn body_json(r: &Req) -> Option<serde_json::Map<String, Value>> {
    match serde_json::from_slice::<Value>(&r.body) { Ok(Value::Object(m)) => Some(m), _ => None }
}

fn api_cart_add(r: &Req) -> Resp {
    let Some(b) = body_json(r) else { return bad_json() };
    let Some(pid) = b.get("product_id").and_then(Value::as_i64).filter(|_| b["product_id"].is_i64() || b["product_id"].is_u64()) else { return bad_json() };
    if b.contains_key("qty") && !b["qty"].is_i64() { return bad_json(); }
    let qty = b.get("qty").and_then(Value::as_i64).unwrap_or(1);
    if !(1..=99).contains(&qty) { return bad_json(); }
    let c = db::conn();
    if db::product_by_id(&c, pid).is_none() { return not_found_json(); }
    let mut hdrs = vec![];
    let sid = session_id(r, &mut hdrs);
    db::cart_add(&c, &sid, pid, qty);
    let mut res = Resp::json(201, serde_json::to_string(&db::cart(&c, &sid)).unwrap());
    res.headers = hdrs;
    res
}

fn api_cart_patch(r: &Req) -> Resp {
    let Some(item) = to_int(r.param("item_id")) else { return not_found_json() };
    let Some(b) = body_json(r) else { return bad_json() };
    let Some(qty) = b.get("qty").filter(|v| v.is_i64()).and_then(Value::as_i64) else { return bad_json() };
    if !(0..=99).contains(&qty) { return bad_json(); }
    let sid = existing_session(r);
    let c = db::conn();
    if sid.is_empty() || !db::cart_set_qty(&c, &sid, item, qty) { return not_found_json(); }
    Resp::json(200, serde_json::to_string(&db::cart(&c, &sid)).unwrap())
}

fn api_cart_delete(r: &Req) -> Resp {
    let item = to_int(r.param("item_id"));
    let sid = existing_session(r);
    let c = db::conn();
    match item { Some(i) if !sid.is_empty() && db::cart_set_qty(&c, &sid, i, 0) => Resp::json(200, serde_json::to_string(&db::cart(&c, &sid)).unwrap()), _ => not_found_json() }
}

// ---------- kanban ----------
fn board_fragment(fe: &str, c: &Conn, board_id: i64) -> Resp {
    let Some(b) = db::board(c, board_id) else { return not_found_html() };
    let _ = db::boards(c); // bs_core queries the board list for the fragment too; keep the same SQL per request.
    Resp::html(html(&view::BoardFragment { fe, columns: view::columns(&b) }))
}

fn kanban_page(r: &Req) -> Resp {
    let fe = r.param("fe");
    let Some(id) = to_int(r.param("board_id")) else { return not_found_html() };
    if !valid_fe(fe) { return not_found_html(); }
    let c = db::conn();
    let Some(b) = db::board(&c, id) else { return not_found_html() };
    let cart_count = cart_count(&c, r);
    let title = format!("{} · Kanban", b.name);
    let boards = db::boards(&c);
    Resp::html(html(&view::KanbanPage { fe, nav: "kanban", title: &title, description: "Kanban panosu", cart_count, board: &b, boards: &boards, columns: view::columns(&b) }))
}

fn card_create_form(r: &Req) -> Resp {
    let fe = r.param("fe");
    let Some(id) = to_int(r.param("board_id")) else { return not_found_html() };
    if !valid_fe(fe) { return not_found_html(); }
    let Some(title) = valid_title(get(&r.form, "title")) else { return html_status(422, "Geçersiz başlık") };
    let col = match get(&r.form, "column") { "" => "backlog", c => c };
    let c = db::conn();
    if db::board(&c, id).is_none() { return not_found_html(); }
    if db::card_create(&c, id, &title, col).is_none() { return html_status(422, "Geçersiz sütun"); }
    if is_htmx(r) { return board_fragment(fe, &c, id); }
    redirect(&format!("/{fe}/kanban/{id}"))
}

fn card_update_form(r: &Req) -> Resp {
    let fe = r.param("fe");
    let Some(id) = to_int(r.param("id")) else { return not_found_html() };
    if !valid_fe(fe) { return not_found_html(); }
    let c = db::conn();
    let Some(card) = db::card_update(&c, id, None, None) else { return not_found_html() };
    if get(&r.form, "delete") == "1" {
        db::card_delete(&c, id);
    } else {
        let title = if r.form.contains_key("title") {
            match valid_title(get(&r.form, "title")) { Some(t) => Some(t), None => return html_status(422, "Geçersiz başlık") }
        } else { None };
        let mv = match get(&r.form, "move") { m @ ("left" | "right") => Some(m), _ => None };
        db::card_update(&c, id, title.as_deref(), mv);
    }
    if is_htmx(r) { return board_fragment(fe, &c, card.board_id); }
    redirect(&format!("/{fe}/kanban/{}", card.board_id))
}

fn api_boards(_: &Req) -> Resp { Resp::json(200, serde_json::to_string(&db::boards(&db::conn())).unwrap()) }

fn api_board(r: &Req) -> Resp {
    let Some(id) = to_int(r.param("id")) else { return not_found_json() };
    match db::board(&db::conn(), id) { Some(b) => Resp::json(200, serde_json::to_string(&b).unwrap()), None => not_found_json() }
}

fn unprocessable() -> Resp { Resp::json(422, r#"{"error":"invalid_title"}"#.into()) }

fn api_card_create(r: &Req) -> Resp {
    let Some(id) = to_int(r.param("id")) else { return not_found_json() };
    let Some(b) = body_json(r) else { return bad_json() };
    let Some(title) = b.get("title").and_then(Value::as_str).and_then(valid_title) else { return unprocessable() };
    let col = b.get("column").and_then(Value::as_str).unwrap_or("backlog");
    if !COLUMNS.contains(&col) { return Resp::json(422, r#"{"error":"invalid_column"}"#.into()); }
    match db::card_create(&db::conn(), id, &title, col) { Some(c) => Resp::json(201, serde_json::to_string(&c).unwrap()), None => not_found_json() }
}

fn api_card_patch(r: &Req) -> Resp {
    let Some(id) = to_int(r.param("id")) else { return not_found_json() };
    let Some(b) = body_json(r) else { return bad_json() };
    let title = match b.get("title") {
        None => None,
        Some(Value::String(s)) => match valid_title(s) { Some(t) => Some(t), None => return unprocessable() },
        Some(_) => return unprocessable(),
    };
    let mv = match b.get("move") {
        None => None,
        Some(Value::String(m)) if m == "left" || m == "right" => Some(m.as_str()),
        Some(_) => return bad_json(),
    };
    match db::card_update(&db::conn(), id, title.as_deref(), mv) { Some(c) => Resp::json(200, serde_json::to_string(&c).unwrap()), None => not_found_json() }
}

fn api_card_delete(r: &Req) -> Resp {
    match to_int(r.param("id")) { Some(id) if db::card_delete(&db::conn(), id) => Resp::new(204, "application/json", ""), _ => not_found_json() }
}

// ---------- dashboard ----------
fn parse_range(r: &Req) -> Option<i64> {
    match r.query.get("range").map(String::as_str) { None | Some("") => Some(30), Some("7") => Some(7), Some("30") => Some(30), Some("90") => Some(90), _ => None }
}

fn dashboard_page(r: &Req) -> Resp {
    let fe = r.param("fe");
    if !valid_fe(fe) { return not_found_html(); }
    let Some(range) = parse_range(r) else { return html_status(400, "Geçersiz aralık") };
    let c = db::conn();
    let d = db::dashboard(&c, range);
    let cart_count = cart_count(&c, r);
    let chart = daily_chart_svg(&d.daily);
    if is_htmx(r) { return Resp::html(html(&view::DashboardBody { fe, d: &d, chart })); }
    Resp::html(html(&view::DashboardPage { fe, nav: "dashboard", title: "Analitik", description: "Analitik dashboard", cart_count, ranges: view::ranges(range), d: &d, chart }))
}

fn api_dashboard(r: &Req) -> Resp {
    match parse_range(r) { Some(range) => Resp::json(200, serde_json::to_string(&db::dashboard(&db::conn(), range)).unwrap()), None => Resp::json(400, r#"{"error":"invalid_range"}"#.into()) }
}

fn healthz(_: &Req) -> Resp { Resp::new(200, "text/plain", "ok") }
