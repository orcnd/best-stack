//! tr-TR formatting, byte-identical to shared/cpp/src/render.cpp.
fn group(v: i64) -> String {
    let digits = v.unsigned_abs().to_string();
    let mut out = String::new();
    for (i, ch) in digits.chars().enumerate() {
        if i > 0 && (digits.len() - i) % 3 == 0 { out.push('.'); }
        out.push(ch);
    }
    if v < 0 { format!("-{out}") } else { out }
}
pub fn int(v: i64) -> String { group(v) }
pub fn money(cents: i64) -> String { format!("₺{},{:02}", group(cents / 100), (cents % 100).abs()) }
pub fn pct(ratio: f64) -> String { format!("%{:.1}", (ratio * 1000.0).round() / 10.0).replace('.', ",") }
pub fn rating(r: f64) -> String { format!("★ {:.1}", r).replace('.', ",") }

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn formats_like_cpp() {
        assert_eq!(money(123450), "₺1.234,50");
        assert_eq!(money(5), "₺0,05");
        assert_eq!(money(123456789), "₺1.234.567,89");
        assert_eq!(int(12345), "12.345");
        assert_eq!(int(999), "999");
        assert_eq!(pct(0.1234), "%12,3");
        assert_eq!(rating(4.5), "★ 4,5");
    }
}
