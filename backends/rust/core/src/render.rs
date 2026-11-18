//! minijinja over the shared Jinja templates (shared/cpp/templates). Like the C++ core, strings are
//! HTML-escaped in the view model (keys starting with `raw_` are exempt), so autoescape is off.
use minijinja::{path_loader, AutoEscape, Environment};
use serde_json::Value;
use std::path::Path;
use std::sync::OnceLock;

static ENV: OnceLock<Environment<'static>> = OnceLock::new();

pub fn load(root: &Path) {
    let mut env = Environment::new();
    env.set_loader(path_loader(root.join("shared").join("cpp").join("templates")));
    env.set_auto_escape_callback(|_| AutoEscape::None);
    env.set_keep_trailing_newline(true);
    let _ = ENV.set(env);
}

pub fn html_escape(s: &str) -> String {
    let mut o = String::with_capacity(s.len() + 8);
    for ch in s.chars() {
        match ch { '&' => o.push_str("&amp;"), '<' => o.push_str("&lt;"), '>' => o.push_str("&gt;"), '"' => o.push_str("&quot;"), '\'' => o.push_str("&#39;"), c => o.push(c) }
    }
    o
}

pub fn escape_all(v: &mut Value) {
    match v {
        Value::String(s) => *s = html_escape(s),
        Value::Array(a) => a.iter_mut().for_each(escape_all),
        Value::Object(m) => { for (k, x) in m.iter_mut() { if !k.starts_with("raw_") { escape_all(x); } } }
        _ => {}
    }
}

/// Render `<tmpl>.html` for frontend `fe` (adds `fe` and `base`).
pub fn view(fe: &str, tmpl: &str, mut data: Value) -> String {
    escape_all(&mut data);
    data["fe"] = Value::from(fe);
    data["base"] = Value::from(format!("/{fe}"));
    let env = ENV.get().expect("render::load not called");
    env.get_template(&format!("{tmpl}.html")).expect("template").render(minijinja::Value::from_serialize(&data)).expect("render")
}
