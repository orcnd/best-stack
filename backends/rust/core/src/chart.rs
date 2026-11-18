//! Port of shared/cpp/src/chart.cpp; output must be byte-identical.
use crate::model::DayPoint;
use std::fmt::Write;

fn fmt(v: f64) -> String {
    let s = format!("{:.2}", (v * 100.0).round() / 100.0);
    let s = s.trim_end_matches('0').trim_end_matches('.');
    if s == "-0" { "0".into() } else { s.to_string() }
}

pub fn daily_chart_svg(days: &[DayPoint]) -> String {
    let (w, h) = (960.0_f64, 240_i64);
    let max = days.iter().map(|d| d.events).max().unwrap_or(0).max(1);
    let bar_w = w / (days.len().max(1) as f64);
    let mut out = String::from("<svg class=\"chart-svg\" viewBox=\"0 0 960 240\" width=\"960\" height=\"240\" role=\"img\" aria-label=\"Günlük olaylar\">");
    for (i, d) in days.iter().enumerate() {
        let bh = ((d.events as f64 / max as f64) * (h - 20) as f64).round() as i64;
        let _ = write!(out, "<rect x=\"{}\" y=\"{}\" width=\"{}\" height=\"{}\" fill=\"#4f6bed\"/>", fmt(i as f64 * bar_w + 1.0), h - bh, fmt(bar_w - 2.0), bh);
    }
    out + "</svg>"
}

#[cfg(test)]
mod tests {
    use super::*;
    fn p(e: i64) -> DayPoint { DayPoint { day: String::new(), events: e, revenue_cents: 0 } }
    #[test]
    fn chart_deterministic() {
        assert_eq!(daily_chart_svg(&[p(10), p(20), p(5)]),
            "<svg class=\"chart-svg\" viewBox=\"0 0 960 240\" width=\"960\" height=\"240\" role=\"img\" aria-label=\"Günlük olaylar\"><rect x=\"1\" y=\"130\" width=\"318\" height=\"110\" fill=\"#4f6bed\"/><rect x=\"321\" y=\"20\" width=\"318\" height=\"220\" fill=\"#4f6bed\"/><rect x=\"641\" y=\"185\" width=\"318\" height=\"55\" fill=\"#4f6bed\"/></svg>");
    }
    #[test]
    fn fractional() {
        let days: Vec<_> = (0..7).map(|_| p(0)).collect();
        assert!(daily_chart_svg(&days).contains("<rect x=\"138.14\" y=\"240\" width=\"135.14\" height=\"0\""));
    }
}
