//! best-stack Rust core: SQLite data layer, view models, templates (minijinja over the shared
//! Jinja files), framework-agnostic handlers. Axum and Actix only add the HTTP layer.
pub mod chart;
pub mod db;
pub mod format;
pub mod handlers;
pub mod http;
pub mod model;
pub mod render;

pub use handlers::{routes, Route};
pub use http::{init, parse_cli, serve_static, Cli, Req, Resp};
