//! tr-TR formatting as Display types that write straight into the template output (no String per
//! value). Output is identical to bs_core::format (checked in tests).
use std::borrow::Borrow;
use std::fmt::{self, Display, Formatter, Write};

/// Digits of |v| with '.' thousands separators, written without allocating.
fn group(f: &mut Formatter<'_>, v: i64) -> fmt::Result {
    if v < 0 { f.write_char('-')?; }
    let mut buf = [0u8; 20];
    let mut n = v.unsigned_abs();
    let mut i = buf.len();
    loop { i -= 1; buf[i] = b'0' + (n % 10) as u8; n /= 10; if n == 0 { break; } }
    let digits = &buf[i..];
    let len = digits.len();
    for (k, &d) in digits.iter().enumerate() {
        if k > 0 && (len - k) % 3 == 0 { f.write_char('.')?; }
        f.write_char(d as char)?;
    }
    Ok(())
}

/// Writes `args` with '.' replaced by ',' (the C++ core's decimal comma).
struct Comma<'a, 'b>(&'a mut Formatter<'b>);
impl Write for Comma<'_, '_> {
    fn write_str(&mut self, s: &str) -> fmt::Result {
        for part in s.split_inclusive('.') {
            match part.strip_suffix('.') { Some(p) => { self.0.write_str(p)?; self.0.write_char(',')?; } None => self.0.write_str(part)? }
        }
        Ok(())
    }
}

pub struct Int<T>(pub T); // T: i64 or &i64 (askama passes call arguments by reference)
impl<T: Borrow<i64>> Display for Int<T> { fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result { group(f, *self.0.borrow()) } }

pub struct Money<T>(pub T); // T: i64 or &i64 (askama passes call arguments by reference)
impl<T: Borrow<i64>> Display for Money<T> {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        let v = *self.0.borrow();
        f.write_char('₺')?;
        // bs_core::format::money groups cents/100 (truncating), so -5 cents prints "₺0,05".
        group(f, v / 100)?;
        write!(f, ",{:02}", (v % 100).abs())
    }
}

pub struct Pct<T>(pub T); // T: f64 or &f64 (askama passes call arguments by reference)
impl<T: Borrow<f64>> Display for Pct<T> {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.write_char('%')?;
        write!(Comma(f), "{:.1}", (*self.0.borrow() * 1000.0).round() / 10.0)
    }
}

pub struct Rating<T>(pub T); // T: f64 or &f64 (askama passes call arguments by reference)
impl<T: Borrow<f64>> Display for Rating<T> {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        f.write_str("★ ")?;
        write!(Comma(f), "{:.1}", self.0.borrow())
    }
}

/// `labels.join(", ")` without the intermediate String.
pub struct Join<'a>(pub &'a [String]);
impl Display for Join<'_> {
    fn fmt(&self, f: &mut Formatter<'_>) -> fmt::Result {
        for (i, s) in self.0.iter().enumerate() { if i > 0 { f.write_str(", ")?; } f.write_str(s)?; }
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use bs_core::format as orig;

    #[test]
    fn matches_bs_core_format() {
        for v in [0, 5, -5, 99, 100, 999, 1000, 12345, 123450, -123450, 123456789, 1_000_000, -1, i64::from(i32::MAX)] {
            assert_eq!(Int(v).to_string(), orig::int(v), "int {v}");
            assert_eq!(Money(v).to_string(), orig::money(v), "money {v}");
        }
        for r in [0.0, 0.1234, 0.05, 0.99999, 1.0, 0.0449, 0.12345] { assert_eq!(Pct(r).to_string(), orig::pct(r), "pct {r}"); }
        for r in [0.0, 4.5, 3.25, 4.95, 5.0, 1.04] { assert_eq!(Rating(r).to_string(), orig::rating(r), "rating {r}"); }
        let l = vec!["a".to_string(), "b<".to_string()];
        assert_eq!(Join(&l).to_string(), l.join(", "));
        assert_eq!(Join(&[]).to_string(), "");
    }
}
