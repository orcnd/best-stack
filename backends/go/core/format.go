package core

import (
	"fmt"
	"math"
	"strconv"
	"strings"
)

// tr-TR formatting, byte-identical to shared/cpp/src/render.cpp.
func group(v int64) string {
	neg := v < 0
	if neg {
		v = -v
	}
	d := strconv.FormatInt(v, 10)
	var b strings.Builder
	for i, ch := range d {
		if i > 0 && (len(d)-i)%3 == 0 {
			b.WriteByte('.')
		}
		b.WriteRune(ch)
	}
	if neg {
		return "-" + b.String()
	}
	return b.String()
}

func FmtInt(v int64) string { return group(v) }
func FmtMoney(c int64) string {
	f := c % 100
	if f < 0 {
		f = -f
	}
	return fmt.Sprintf("₺%s,%02d", group(c/100), f)
}
func FmtPct(r float64) string {
	return "%" + strings.Replace(fmt.Sprintf("%.1f", math.Round(r*1000)/10), ".", ",", 1)
}
func FmtRating(r float64) string { return "★ " + strings.Replace(fmt.Sprintf("%.1f", r), ".", ",", 1) }
