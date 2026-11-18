//! Framework-agnostic request/response, CLI, static files, parsing helpers.
use std::collections::HashMap;
use std::path::{Component, Path, PathBuf};
use std::sync::{OnceLock, RwLock};

#[derive(Default, Debug)]
pub struct Req {
    pub method: String,
    pub path: String,
    pub params: HashMap<String, String>,
    pub query: HashMap<String, String>,
    pub form: HashMap<String, String>,
    pub headers: HashMap<String, String>, // lower-case keys
    pub cookies: HashMap<String, String>,
    pub body: Vec<u8>,
}

impl Req {
    pub fn header(&self, k: &str) -> &str { self.headers.get(k).map(String::as_str).unwrap_or("") }
    pub fn param(&self, k: &str) -> &str { self.params.get(k).map(String::as_str).unwrap_or("") }
    /// Fill query/cookies/form from raw parts (adapters call this after setting method, path, headers, body).
    pub fn finish(&mut self, raw_query: &str) {
        self.query = parse_urlencoded(raw_query);
        self.cookies = parse_cookies(self.header("cookie"));
        if self.header("content-type").starts_with("application/x-www-form-urlencoded") {
            self.form = parse_urlencoded(&String::from_utf8_lossy(&self.body));
        }
    }
}

#[derive(Debug, Clone)]
pub struct Resp {
    pub status: u16,
    pub content_type: String,
    pub body: Vec<u8>,
    pub headers: Vec<(String, String)>,
}

impl Resp {
    pub fn new(status: u16, content_type: &str, body: impl Into<Vec<u8>>) -> Self {
        Resp { status, content_type: content_type.into(), body: body.into(), headers: vec![] }
    }
    pub fn html(body: String) -> Self { Self::new(200, "text/html; charset=utf-8", body) }
    pub fn json(status: u16, body: String) -> Self { Self::new(status, "application/json", body) }
}

pub fn parse_urlencoded(s: &str) -> HashMap<String, String> {
    form_urlencoded::parse(s.as_bytes()).into_owned().collect()
}

pub fn parse_cookies(h: &str) -> HashMap<String, String> {
    h.split(';').filter_map(|p| { let p = p.trim(); p.split_once('=').map(|(k, v)| (k.to_string(), v.to_string())) }).collect()
}

pub fn url_decode(s: &str) -> String { percent_encoding::percent_decode_str(s).decode_utf8_lossy().into_owned() }

pub struct Cli { pub port: u16, pub db: String, pub root: String, pub threads: usize }

pub fn parse_cli() -> Cli {
    let a: Vec<String> = std::env::args().collect();
    let get = |k: &str, d: &str| a.iter().position(|x| x == k).and_then(|i| a.get(i + 1)).cloned().unwrap_or_else(|| d.to_string());
    Cli { port: get("--port", "18080").parse().unwrap(), db: get("--db", "data/app.sqlite"), root: get("--root", "."), threads: get("--threads", "8").parse().unwrap() }
}

static STATIC_DIR: OnceLock<PathBuf> = OnceLock::new();
static STATIC_CACHE: OnceLock<RwLock<HashMap<String, Resp>>> = OnceLock::new();

/// Must be called once before serving.
pub fn init(cli: &Cli, pool_size: u32) {
    let root = std::fs::canonicalize(&cli.root).expect("root");
    crate::db::init(&cli.db, pool_size);
    crate::render::load(&root);
    let _ = STATIC_DIR.set(root.join("shared").join("static"));
    let _ = STATIC_CACHE.set(RwLock::new(HashMap::new()));
}

fn mime(p: &Path) -> &'static str {
    match p.extension().and_then(|e| e.to_str()).unwrap_or("") {
        "css" => "text/css; charset=utf-8", "js" | "mjs" => "text/javascript; charset=utf-8", "svg" => "image/svg+xml",
        "png" => "image/png", "webp" => "image/webp", "wasm" => "application/wasm", "json" => "application/json",
        "html" => "text/html; charset=utf-8", "ico" => "image/x-icon", _ => "application/octet-stream",
    }
}

/// GET /static/<rel>; None when missing or escaping the static dir.
pub fn serve_static(rel: &str) -> Option<Resp> {
    let cache = STATIC_CACHE.get()?;
    if let Some(r) = cache.read().unwrap().get(rel) { return Some(r.clone()); }
    let relp = Path::new(rel);
    if rel.is_empty() || relp.components().any(|c| !matches!(c, Component::Normal(_))) { return None; }
    let full = STATIC_DIR.get()?.join(relp);
    let body = std::fs::read(&full).ok()?;
    let mut r = Resp::new(200, mime(&full), body);
    r.headers.push(("Cache-Control".into(), "public, max-age=31536000".into()));
    cache.write().unwrap().insert(rel.to_string(), r.clone());
    Some(r)
}
