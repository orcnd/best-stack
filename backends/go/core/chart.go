package core

import (
	"fmt"
	"math"
	"strings"
)

func chartFmt(v float64) string {
	s := fmt.Sprintf("%.2f", math.Round(v*100)/100)
	s = strings.TrimRight(s, "0")
	s = strings.TrimSuffix(s, ".")
	if s == "-0" {
		return "0"
	}
	return s
}

// DailyChartSVG is a port of shared/cpp/src/chart.cpp; output must be byte-identical.
func DailyChartSVG(days []DayPoint) string {
	const w, h = 960.0, 240
	var max int64 = 0
	for _, d := range days {
		if d.Events > max {
			max = d.Events
		}
	}
	if max == 0 {
		max = 1
	}
	n := len(days)
	if n == 0 {
		n = 1
	}
	barW := w / float64(n)
	var b strings.Builder
	b.WriteString(`<svg class="chart-svg" viewBox="0 0 960 240" width="960" height="240" role="img" aria-label="Günlük olaylar">`)
	for i, d := range days {
		bh := int64(math.Round(float64(d.Events) / float64(max) * (h - 20)))
		fmt.Fprintf(&b, `<rect x="%s" y="%d" width="%s" height="%d" fill="#4f6bed"/>`, chartFmt(float64(i)*barW+1), h-bh, chartFmt(barW-2), bh)
	}
	b.WriteString("</svg>")
	return b.String()
}
