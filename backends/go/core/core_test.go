package core

import (
	"strings"
	"testing"
)

func TestFormatsLikeCpp(t *testing.T) {
	cases := map[string]string{
		FmtMoney(123450): "₺1.234,50", FmtMoney(5): "₺0,05", FmtMoney(123456789): "₺1.234.567,89",
		FmtInt(12345): "12.345", FmtInt(999): "999", FmtPct(0.1234): "%12,3", FmtRating(4.5): "★ 4,5",
	}
	for got, want := range cases {
		if got != want {
			t.Errorf("got %q want %q", got, want)
		}
	}
}

func TestChartDeterministic(t *testing.T) {
	got := DailyChartSVG([]DayPoint{{Events: 10}, {Events: 20}, {Events: 5}})
	want := `<svg class="chart-svg" viewBox="0 0 960 240" width="960" height="240" role="img" aria-label="Günlük olaylar"><rect x="1" y="130" width="318" height="110" fill="#4f6bed"/><rect x="321" y="20" width="318" height="220" fill="#4f6bed"/><rect x="641" y="185" width="318" height="55" fill="#4f6bed"/></svg>`
	if got != want {
		t.Fatalf("got\n%s\nwant\n%s", got, want)
	}
	if !strings.Contains(DailyChartSVG(make([]DayPoint, 7)), `<rect x="138.14" y="240" width="135.14" height="0"`) {
		t.Fatal("fractional widths")
	}
}

func TestIsoDay(t *testing.T) {
	if isoDay(AnchorTS) != "2026-11-01" {
		t.Fatal(isoDay(AnchorTS))
	}
}
