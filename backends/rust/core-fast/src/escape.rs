//! HTML escaper used by every askama template in this crate (see askama.toml). Escapes while writing:
//! no intermediate escaped String. Same entity spelling as bs_core::render::html_escape.
use std::fmt::{self, Write};

#[derive(Clone, Copy, Default, Debug)]
pub struct Html;

impl askama::filters::Escaper for Html {
    fn write_escaped_str<W: Write>(&self, mut dest: W, s: &str) -> fmt::Result {
        let b = s.as_bytes();
        let mut last = 0;
        for (i, &c) in b.iter().enumerate() {
            let rep = match c { b'&' => "&amp;", b'<' => "&lt;", b'>' => "&gt;", b'"' => "&quot;", b'\'' => "&#39;", _ => continue };
            if last < i { dest.write_str(&s[last..i])?; }
            dest.write_str(rep)?;
            last = i + 1;
        }
        if last < b.len() { dest.write_str(&s[last..])?; }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use askama::filters::Escaper;
    fn esc(s: &str) -> String { let mut o = String::new(); Html.write_escaped_str(&mut o, s).unwrap(); o }

    #[test]
    fn escapes_like_bs_core() {
        for s in ["", "plain", "<script>x</script>", "a & b \"c\" 'd'", "çğıöşü ₺ & <>", "&&", "x'", "<"] {
            assert_eq!(esc(s), bs_core::render::html_escape(s), "{s:?}");
        }
        assert_eq!(esc("Golden <b>&</b> 'k'\""), "Golden &lt;b&gt;&amp;&lt;/b&gt; &#39;k&#39;&quot;");
    }

    #[test]
    fn escapes_chars() {
        let mut o = String::new();
        for c in ['<', 'a', '\'', 'ş'] { Html.write_escaped_char(&mut o, c).unwrap(); }
        assert_eq!(o, "&lt;a&#39;ş");
    }
}
