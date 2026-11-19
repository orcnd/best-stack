//! Axum backend: tokio multi-thread runtime (worker threads = --threads); handlers run on the
//! blocking pool (spawn_blocking) because SQLite calls block. Routes come from bs_core::routes().
//! Also built as bs-axum-opt, where `bs_core` is bs-core-fast (see ../axum-opt/Cargo.toml).
use axum::body::{Body, Bytes};
use axum::extract::{RawPathParams, RawQuery};
use axum::http::{HeaderMap, HeaderName, HeaderValue, Method, StatusCode, Uri};
use axum::response::Response;
use axum::routing::{get, MethodFilter, MethodRouter};
use axum::Router;
use bs_core::{Req, Resp};
use std::collections::BTreeMap;

fn to_response(r: Resp) -> Response {
    let mut res = Response::new(Body::from(r.body));
    *res.status_mut() = StatusCode::from_u16(r.status).unwrap();
    let h = res.headers_mut();
    h.insert("content-type", HeaderValue::from_str(&r.content_type).unwrap());
    for (k, v) in r.headers {
        h.append(HeaderName::from_bytes(k.as_bytes()).unwrap(), HeaderValue::from_str(&v).unwrap());
    }
    res
}

fn to_req(method: Method, uri: &Uri, headers: &HeaderMap, params: Option<&RawPathParams>, query: Option<String>, body: Bytes) -> Req {
    let mut r = Req { method: method.to_string(), path: uri.path().to_string(), body: body.to_vec(), ..Default::default() };
    for (k, v) in headers { r.headers.insert(k.as_str().to_string(), v.to_str().unwrap_or("").to_string()); }
    if let Some(p) = params { for (k, v) in p.iter() { r.params.insert(k.to_string(), v.to_string()); } }
    r.finish(query.as_deref().unwrap_or(""));
    r
}

fn main() {
    let cli = bs_core::parse_cli();
    bs_core::init(&cli, (cli.threads * 2) as u32);
    let rt = tokio::runtime::Builder::new_multi_thread().worker_threads(cli.threads).enable_all().build().unwrap();
    rt.block_on(async move {
        let mut grouped: BTreeMap<&'static str, MethodRouter> = BTreeMap::new();
        for rt in bs_core::routes() {
            let h = rt.handler;
            let filter = match rt.method { "POST" => MethodFilter::POST, "PATCH" => MethodFilter::PATCH, "DELETE" => MethodFilter::DELETE, _ => MethodFilter::GET };
            let handler = move |method: Method, uri: Uri, headers: HeaderMap, params: RawPathParams, RawQuery(q): RawQuery, body: Bytes| async move {
                let req = to_req(method, &uri, &headers, Some(&params), q, body);
                to_response(tokio::task::spawn_blocking(move || h(&req)).await.unwrap())
            };
            let entry = grouped.remove(rt.pattern).unwrap_or_default();
            grouped.insert(rt.pattern, entry.on(filter, handler));
        }
        let mut app = Router::new();
        for (pattern, mr) in grouped { app = app.route(pattern, mr); }
        app = app.route("/static/{*rel}", get(|uri: Uri| async move {
            let rel = bs_core::http::url_decode(uri.path().trim_start_matches("/static/"));
            to_response(bs_core::serve_static(&rel).unwrap_or_else(|| Resp::new(404, "text/plain", "not found")))
        }));
        app = app.fallback(|| async { to_response(Resp::new(404, "text/plain", "not found")) });
        let listener = tokio::net::TcpListener::bind(("0.0.0.0", cli.port)).await.unwrap();
        println!("{} listening on :{} (workers={})", env!("CARGO_BIN_NAME"), cli.port, cli.threads);
        axum::serve(listener, app).await.unwrap();
    });
}
