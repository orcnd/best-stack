//! Actix-web backend: --threads workers (one event loop each); handlers run on actix's blocking
//! pool via web::block because SQLite calls block. Routes come from bs_core::routes().
//! Also built as bs-actix-opt, where `bs_core` is bs-core-fast (see ../actix-opt/Cargo.toml).
use actix_web::http::{Method, StatusCode};
use actix_web::{web, App, HttpRequest, HttpResponse, HttpServer};
use bs_core::{Req, Resp};
use std::collections::BTreeMap;

fn to_response(r: Resp) -> HttpResponse {
    let mut b = HttpResponse::build(StatusCode::from_u16(r.status).unwrap());
    b.insert_header(("content-type", r.content_type));
    for (k, v) in r.headers { b.append_header((k, v)); }
    b.body(r.body)
}

fn to_req(h: &HttpRequest, body: web::Bytes) -> Req {
    let mut r = Req { method: h.method().to_string(), path: h.path().to_string(), body: body.to_vec(), ..Default::default() };
    for (k, v) in h.headers() { r.headers.insert(k.as_str().to_string(), v.to_str().unwrap_or("").to_string()); }
    for (k, v) in h.match_info().iter() { r.params.insert(k.to_string(), v.to_string()); }
    r.finish(h.query_string());
    r
}

#[actix_web::main]
async fn main() -> std::io::Result<()> {
    let cli = bs_core::parse_cli();
    bs_core::init(&cli, (cli.threads * 2) as u32);
    println!("{} listening on :{} (workers={})", env!("CARGO_BIN_NAME"), cli.port, cli.threads);
    HttpServer::new(|| {
        // Group by pattern so one resource answers every method of a path.
        let mut grouped: BTreeMap<&'static str, Vec<bs_core::Route>> = BTreeMap::new();
        for rt in bs_core::routes() { grouped.entry(rt.pattern).or_default().push(rt); }
        // Register in bs_core::routes() order (static /api paths before /{fe}/... patterns).
        let order: Vec<&'static str> = bs_core::routes().iter().map(|r| r.pattern).fold(vec![], |mut v, p| { if !v.contains(&p) { v.push(p); } v });
        let mut app = App::new();
        for pattern in order {
            let mut res = web::resource(pattern);
            for rt in grouped.remove(pattern).unwrap() {
                let h = rt.handler;
                let m = Method::from_bytes(rt.method.as_bytes()).unwrap();
                res = res.route(web::route().method(m).to(move |req: HttpRequest, body: web::Bytes| async move {
                    let r = to_req(&req, body);
                    to_response(web::block(move || h(&r)).await.unwrap())
                }));
            }
            app = app.service(res);
        }
        app.service(web::resource("/static/{rel:.*}").route(web::get().to(|req: HttpRequest| async move {
            let rel = bs_core::http::url_decode(req.match_info().get("rel").unwrap_or(""));
            to_response(bs_core::serve_static(&rel).unwrap_or_else(|| Resp::new(404, "text/plain", "not found")))
        })))
        .default_service(web::to(|| async { to_response(Resp::new(404, "text/plain", "not found")) }))
    })
    .workers(cli.threads)
    .bind(("0.0.0.0", cli.port))?
    .run()
    .await
}
