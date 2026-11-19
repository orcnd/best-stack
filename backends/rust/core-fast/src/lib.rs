//! best-stack Rust fast core (run 3): askama compile-time templates over typed view structs +
//! mimalloc. Reuses bs-core's SQLite pool/SQL, HTTP parsing, chart math and formatting; exposes the
//! same API as bs-core (routes, init, parse_cli, serve_static, Req, Resp) so the axum/actix adapter
//! sources are shared unchanged.
pub mod escape;
pub mod fmt;
pub mod view;

#[global_allocator]
static GLOBAL: mimalloc::MiMalloc = mimalloc::MiMalloc;
pub mod handlers;

// Same surface as bs_core, so ../axum/src/main.rs and ../actix/src/main.rs compile against either.
pub use bs_core::{http, init, parse_cli, serve_static, Cli, Req, Resp, Route};
pub use handlers::routes;
