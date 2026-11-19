package corefast

import (
	"context"
	"io"
	"strconv"

	"bestack/go/core"
	"github.com/a-h/templ"
)

// Typed view models: the templ components read core's structs directly, so there is no map[string]any
// tree and no reflection between the DB rows and the HTML.

// Page is what layout_top needs (the html/template "page" map).
type Page struct {
	FE, Base, Nav, Title, Description string
	CartCount                         int64
}

type CatalogVM struct {
	Q, Sort, Cat       string
	Categories         []core.Category
	Items              []core.Product
	TotalFmt           string
	Page, Pages        int64
	PrevHref, NextHref string
}

type BoardVM struct {
	Board  core.Board
	Boards []core.BoardSummary
}

type DashboardVM struct {
	Range int64
	D     core.Dashboard
	Chart string // trusted, generated SVG markup
}

type rangeOpt struct {
	v int64
	l string
}

var ranges = []rangeOpt{{7, "Son 7 gün"}, {30, "Son 30 gün"}, {90, "Son 90 gün"}}

func itoa(v int64) string { return strconv.FormatInt(v, 10) }

// alpineMoveForm writes the Alpine move-left/right form. The reference markup has a literal JS string
// ('left' / 'right') inside the @submit.prevent attribute; templ HTML-escapes every dynamic attribute
// value (' → &#39;), so this one tag is written directly. Inputs are an int and two constants.
func alpineMoveForm(id int64, dir string, disabled bool) templ.Component {
	return templ.ComponentFunc(func(_ context.Context, w io.Writer) error {
		ids, arrow, dis := itoa(id), "←", ""
		if dir == "right" {
			arrow = "→"
		}
		if disabled {
			dis = " disabled"
		}
		_, err := io.WriteString(w, `<form method="post" action="/alpine/kanban/cards/`+ids+`" @submit.prevent="update(`+ids+`, { move: '`+dir+
			`' })"><input type="hidden" name="move" value="`+dir+`"><button type="submit" class="small" data-testid="card-move-`+dir+`"`+dis+`>`+arrow+`</button></form>`)
		return err
	})
}
