//! Typed view structs, one per rendered template. They borrow the bs_core model rows directly; tr-TR
//! number formatting and escaping happen while askama writes the output (see fmt.rs, escape.rs).
#![allow(clippy::needless_lifetimes)]
use crate::fmt::{Int, Join, Money, Pct, Rating};
use askama::Template;
use bs_core::model::{Board, BoardSummary, Card, Cart, Category, Dashboard, Product};

/// Fields read by layout_top.html / badge.html are repeated on every full page (askama includes
/// share the including template's scope).
#[derive(Template)]
#[template(path = "badge.html")]
pub struct Badge<'a> { pub fe: &'a str, pub cart_count: i64 }

pub struct Results<'a> { pub items: &'a [Product], pub total: i64, pub page: i64, pub pages: i64, pub prev_href: String, pub next_href: String }

#[derive(Template)]
#[template(path = "catalog.html")]
pub struct CatalogPage<'a> {
    pub fe: &'a str, pub nav: &'a str, pub title: &'a str, pub description: &'a str, pub cart_count: i64,
    pub q: &'a str, pub cat: &'a str, pub sort: &'a str, pub categories: &'a [Category], pub r: Results<'a>,
}

#[derive(Template)]
#[template(path = "catalog_results.html")]
pub struct CatalogResults<'a> { pub fe: &'a str, pub r: Results<'a> }

#[derive(Template)]
#[template(path = "product.html")]
pub struct ProductPage<'a> {
    pub fe: &'a str, pub nav: &'a str, pub title: &'a str, pub description: &'a str, pub cart_count: i64,
    pub product: &'a Product,
}

#[derive(Template)]
#[template(path = "cart.html")]
pub struct CartPage<'a> {
    pub fe: &'a str, pub nav: &'a str, pub title: &'a str, pub description: &'a str, pub cart_count: i64,
    pub cart: &'a Cart, pub fragment: bool,
}

#[derive(Template)]
#[template(path = "cart_body.html")]
pub struct CartBody<'a> { pub fe: &'a str, pub cart: &'a Cart, pub fragment: bool }

pub struct Col<'a> { pub key: &'static str, pub label: &'static str, pub cards: &'a [Card], pub first: bool, pub last: bool }

pub fn columns(b: &Board) -> Vec<Col<'_>> {
    let label = |k: &str| match k { "backlog" => "Backlog", "todo" => "Yapılacak", "doing" => "Yapılıyor", _ => "Bitti" };
    let n = bs_core::model::COLUMNS.len();
    bs_core::model::COLUMNS.iter().enumerate().map(|(i, k)| Col {
        key: k, label: label(k), cards: b.columns.get(*k).map(Vec::as_slice).unwrap_or(&[]), first: i == 0, last: i + 1 == n,
    }).collect()
}

#[derive(Template)]
#[template(path = "kanban.html")]
pub struct KanbanPage<'a> {
    pub fe: &'a str, pub nav: &'a str, pub title: &'a str, pub description: &'a str, pub cart_count: i64,
    pub board: &'a Board, pub boards: &'a [BoardSummary], pub columns: Vec<Col<'a>>,
}

#[derive(Template)]
#[template(path = "board.html")]
pub struct BoardFragment<'a> { pub fe: &'a str, pub columns: Vec<Col<'a>> }

pub struct RangeOpt { pub value: i64, pub label: &'static str, pub selected: bool }

pub fn ranges(range: i64) -> [RangeOpt; 3] {
    [(7, "Son 7 gün"), (30, "Son 30 gün"), (90, "Son 90 gün")].map(|(value, label)| RangeOpt { value, label, selected: value == range })
}

#[derive(Template)]
#[template(path = "dashboard.html")]
pub struct DashboardPage<'a> {
    pub fe: &'a str, pub nav: &'a str, pub title: &'a str, pub description: &'a str, pub cart_count: i64,
    pub ranges: [RangeOpt; 3], pub d: &'a Dashboard, pub chart: String,
}

#[derive(Template)]
#[template(path = "dashboard_body.html")]
pub struct DashboardBody<'a> { pub fe: &'a str, pub d: &'a Dashboard, pub chart: String }

/// Render into a String sized from askama's size hint (no intermediate value tree).
pub fn html<T: Template>(t: &T) -> String { t.render().expect("render") }

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;
    use std::collections::BTreeMap;
    use std::path::Path;

    /// Reference output: the original minijinja templates via bs_core::render.
    fn reference(tmpl: &str, fe: &str, vm: serde_json::Value) -> String {
        bs_core::render::load(&Path::new(env!("CARGO_MANIFEST_DIR")).join("../../..").canonicalize().unwrap());
        bs_core::render::view(fe, tmpl, vm)
    }

    fn card(id: i64, title: &str, column: &str, labels: &[&str]) -> Card {
        Card { id, board_id: 1, position: 0, column: column.into(), title: title.into(), description: String::new(),
               assignee: "Ayşe <a&b>".into(), due_date: "2026-11-20".into(), labels: labels.iter().map(|s| s.to_string()).collect() }
    }

    #[test]
    fn badge_matches_minijinja() {
        for fe in ["htmx", "alpine", "vanilla"] {
            assert_eq!(html(&Badge { fe, cart_count: 7 }), reference("badge", fe, json!({ "cart_count": 7, "fragment": true })));
        }
    }

    #[test]
    fn board_fragment_matches_minijinja_byte_for_byte() {
        let mut columns = BTreeMap::new();
        columns.insert("todo".to_string(), vec![card(5, "Golden <b>&</b> 'kart' \"x\"", "todo", &["ui", "<bug>"]), card(6, "İkinci", "todo", &[])]);
        columns.insert("done".to_string(), vec![card(9, "Bitti", "done", &["a"])]);
        let b = Board { id: 1, name: "Pano".into(), columns };
        for fe in ["htmx", "alpine", "vanilla"] {
            let cols: Vec<serde_json::Value> = bs_core::model::COLUMNS.iter().enumerate().map(|(i, k)| {
                let label = match *k { "backlog" => "Backlog", "todo" => "Yapılacak", "doing" => "Yapılıyor", _ => "Bitti" };
                let cards: Vec<_> = b.columns.get(*k).map(|v| v.iter().map(|c| json!({ "id": c.id, "title": c.title, "labels": c.labels.join(", "),
                    "assignee": c.assignee, "due": c.due_date, "first": i == 0, "last": i == 3 })).collect()).unwrap_or_default();
                json!({ "key": k, "label": label, "count": cards.len(), "cards": cards })
            }).collect();
            let want = reference("board", fe, json!({ "columns": cols, "fragment": true }));
            let got = html(&BoardFragment { fe, columns: columns_of(&b) });
            assert!(got.contains("Golden &lt;b&gt;&amp;&lt;/b&gt; &#39;kart&#39; &quot;x&quot;"));
            assert_eq!(got, want, "fe={fe}");
        }
    }

    fn columns_of(b: &Board) -> Vec<Col<'_>> { columns(b) }
}
