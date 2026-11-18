use serde::Serialize;
use std::collections::BTreeMap;

pub const ANCHOR_TS: i64 = 1_793_491_200; // 2026-11-01T00:00:00Z
pub const PER_PAGE: i64 = 24;
pub const MAX_TITLE: usize = 200;
pub const COLUMNS: [&str; 4] = ["backlog", "todo", "doing", "done"];

#[derive(Serialize, Clone)]
pub struct Category { pub id: i64, pub name: String, pub slug: String }

#[derive(Serialize, Clone)]
pub struct Product {
    pub id: i64, pub category_id: i64, pub category: String, pub name: String, pub description: String,
    pub image: String, pub price_cents: i64, pub stock: i64, pub rating: f64,
}

#[derive(Default)]
pub struct ProductQuery { pub q: String, pub cat: String, pub sort: String, pub page: i64 }

#[derive(Serialize)]
pub struct ProductPage { pub items: Vec<Product>, pub total: i64, pub page: i64, pub pages: i64 }

#[derive(Serialize)]
pub struct CartItem { pub id: i64, pub qty: i64, pub product: Product }

#[derive(Serialize, Default)]
pub struct Cart { pub items: Vec<CartItem>, pub count: i64, pub total_cents: i64 }

#[derive(Serialize, Clone)]
pub struct Card {
    pub id: i64, pub board_id: i64, pub position: i64, pub column: String, pub title: String,
    pub description: String, pub assignee: String, pub due_date: String, pub labels: Vec<String>,
}

#[derive(Serialize)]
pub struct BoardSummary { pub id: i64, pub name: String, pub card_count: i64 }

#[derive(Serialize)]
pub struct Board { pub id: i64, pub name: String, pub columns: BTreeMap<String, Vec<Card>> }

#[derive(Serialize)]
pub struct DayPoint { pub day: String, pub events: i64, pub revenue_cents: i64 }

#[derive(Serialize)]
pub struct Kpis { pub events: i64, pub users: i64, pub revenue_cents: i64, pub conversion: f64 }

#[derive(Serialize)]
pub struct CountryRow { pub country: String, pub events: i64 }

#[derive(Serialize)]
pub struct TopItem { pub item: String, pub revenue_cents: i64, pub purchases: i64 }

#[derive(Serialize)]
pub struct Dashboard {
    pub range_days: i64, pub kpis: Kpis, pub daily: Vec<DayPoint>, pub countries: Vec<CountryRow>, pub top_items: Vec<TopItem>,
}
