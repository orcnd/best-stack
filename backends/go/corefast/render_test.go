package corefast

import (
	"html/template"
	"regexp"
	"strings"
	"testing"

	"bestack/go/core"
)

// norm mirrors bench/parity/golden.ts: whitespace between tags is insignificant.
var (
	reTagGap = regexp.MustCompile(`>\s+<`)
	reSpace  = regexp.MustCompile(`\s+`)
)

func norm(s string) string {
	return strings.TrimSpace(reSpace.ReplaceAllString(reTagGap.ReplaceAllString(s, "><"), " "))
}

func same(t *testing.T, name, want, got string) {
	t.Helper()
	if norm(want) != norm(got) {
		w, g := norm(want), norm(got)
		i := 0
		for i < len(w) && i < len(g) && w[i] == g[i] {
			i++
		}
		t.Fatalf("%s differs at %d\nwant: %q\ngot:  %q", name, i, w[max(0, i-80):min(len(w), i+80)], g[max(0, i-80):min(len(g), i+80)])
	}
}

const nasty = `<b>"x" & 'y'</b>`

var product = core.Product{ID: 7, CategoryID: 2, Category: "Ev & Yaşam", Name: nasty, Description: "Açıklama <i>", Image: "/static/img/cat-2.svg", PriceCents: 123456, Stock: 3, Rating: 4.5}

func page(fe, nav string) (Page, core.M) {
	p := Page{FE: fe, Base: "/" + fe, Nav: nav, Title: nasty, Description: nasty, CartCount: 12}
	return p, core.M{"nav": nav, "title": nasty, "description": nasty, "cart_count": int64(12), "fragment": false}
}

func TestEscapingMatchesHTMLTemplate(t *testing.T) {
	got := string(Render(badge("htmx", 3)))
	if got != `<span class="badge" id="cart-badge" data-testid="cart-badge">3</span>` {
		t.Fatalf("badge: %s", got)
	}
	p, _ := page("htmx", "catalog")
	out := string(Render(productPage(p, product)))
	for _, want := range []string{
		`<title>&lt;b&gt;&#34;x&#34; &amp; &#39;y&#39;&lt;/b&gt; · Best Stack</title>`,
		`content="&lt;b&gt;&#34;x&#34; &amp; &#39;y&#39;&lt;/b&gt;"`,
		`<h1>&lt;b&gt;&#34;x&#34; &amp; &#39;y&#39;&lt;/b&gt;</h1>`,
		`<p class="description">Açıklama &lt;i&gt;</p>`,
	} {
		if !strings.Contains(out, want) {
			t.Errorf("missing %s", want)
		}
	}
	if strings.Contains(out, "<b>") {
		t.Error("unescaped markup in output")
	}
}

func TestProductPageMatchesReference(t *testing.T) {
	for _, fe := range []string{"htmx", "alpine", "vanilla"} {
		p, m := page(fe, "catalog")
		m["product"] = core.ProductVMMap(product)
		same(t, fe+" product", core.View(fe, "product", m), string(Render(productPage(p, product))))
	}
}

func TestCatalogMatchesReference(t *testing.T) {
	cats := []core.Category{{ID: 1, Name: "Mobilya", Slug: "mobilya"}, {ID: 2, Name: "Ev & Yaşam", Slug: "ev"}}
	for _, fe := range []string{"htmx", "alpine", "vanilla"} {
		for _, items := range [][]core.Product{{product, product}, {}} {
			vm := CatalogVM{Q: nasty, Sort: "price_asc", Cat: "ev", Categories: cats, Items: items, TotalFmt: "1.234",
				Page: 2, Pages: 3, PrevHref: "/" + fe + "/catalog?q=%3Cb%3E&page=1", NextHref: "/" + fe + "/catalog?q=%3Cb%3E&page=3"}
			p, m := page(fe, "catalog")
			mc := []core.M{}
			for _, c := range cats {
				mc = append(mc, core.M{"slug": c.Slug, "name": c.Name, "selected": c.Slug == vm.Cat})
			}
			mi := []core.M{}
			for _, it := range items {
				mi = append(mi, core.ProductVMMap(it))
			}
			m["q"], m["sort"], m["categories"], m["items"], m["has_items"] = vm.Q, vm.Sort, mc, mi, len(mi) > 0
			m["total_fmt"], m["page"], m["pages"], m["prev_href"], m["next_href"] = vm.TotalFmt, vm.Page, vm.Pages, vm.PrevHref, vm.NextHref
			same(t, fe+" catalog", core.View(fe, "catalog", m), string(Render(catalogPage(p, vm))))
			m["fragment"] = true
			same(t, fe+" catalog_results", core.View(fe, "catalog_results", m), string(Render(catalogResults(fe, "/"+fe, vm))))
		}
	}
}

func TestCartMatchesReference(t *testing.T) {
	carts := []core.Cart{{Items: []core.CartItem{{ID: 4, Qty: 2, Product: product}}, Count: 2, TotalCents: 246912}, {Items: []core.CartItem{}}}
	for _, fe := range []string{"htmx", "alpine", "vanilla"} {
		for _, c := range carts {
			p, m := page(fe, "catalog")
			m["cart"] = core.CartVMMap(c)
			same(t, fe+" cart", core.View(fe, "cart", m), string(Render(cartPage(p, c))))
			frag := core.M{"cart": core.CartVMMap(c), "fragment": true}
			same(t, fe+" cart_body", core.View(fe, "cart_body", frag), string(Render(cartBody(fe, "/"+fe, c, true))))
		}
	}
}

func TestKanbanMatchesReference(t *testing.T) {
	card := core.Card{ID: 9, BoardID: 1, Column: "todo", Title: nasty, Assignee: "Ayşe & Co", DueDate: "2026-11-30", Labels: []string{"a<b", "c"}}
	b := core.Board{ID: 1, Name: nasty, Columns: map[string][]core.Card{"backlog": {card}, "todo": {card, card}, "doing": {}, "done": {card}}}
	all := []core.BoardSummary{{ID: 1, Name: nasty}, {ID: 2, Name: "İkinci"}}
	for _, fe := range []string{"htmx", "alpine", "vanilla"} {
		p, m := page(fe, "kanban")
		for k, v := range core.BoardVMMap(b, all) {
			m[k] = v
		}
		same(t, fe+" kanban", core.View(fe, "kanban", m), string(Render(kanbanPage(p, BoardVM{Board: b, Boards: all}))))
		frag := core.BoardVMMap(b, all)
		frag["fragment"] = true
		same(t, fe+" board", core.View(fe, "board", frag), string(Render(board(fe, "/"+fe, b))))
	}
}

func TestDashboardMatchesReference(t *testing.T) {
	d := core.Dashboard{RangeDays: 7, Kpis: core.Kpis{Events: 12345, Users: 678, RevenueCents: 9876543, Conversion: 0.1234},
		Daily:     []core.DayPoint{{Day: "2026-11-01", Events: 5}, {Day: "2026-11-02", Events: 9}},
		Countries: []core.CountryRow{{Country: "TR & <x>", Events: 1000}}, TopItems: []core.TopItem{{Item: nasty, RevenueCents: 1999, Purchases: 3}}}
	vm := DashboardVM{Range: 7, D: d, Chart: core.DailyChartSVG(d.Daily)}
	for _, fe := range []string{"htmx", "alpine", "vanilla"} {
		p, m := page(fe, "dashboard")
		m["ranges"] = []core.M{{"value": int64(7), "label": "Son 7 gün", "selected": true}, {"value": int64(30), "label": "Son 30 gün", "selected": false}, {"value": int64(90), "label": "Son 90 gün", "selected": false}}
		m["countries"] = []core.M{{"country": "TR & <x>", "events": core.FmtInt(1000)}}
		m["top_items"] = []core.M{{"item": nasty, "purchases": core.FmtInt(3), "revenue": core.FmtMoney(1999)}}
		m["kpis"] = core.M{"events": core.FmtInt(12345), "users": core.FmtInt(678), "revenue": core.FmtMoney(9876543), "conversion": core.FmtPct(0.1234)}
		m["chart"] = template.HTML(vm.Chart)
		same(t, fe+" dashboard", core.View(fe, "dashboard", m), string(Render(dashboardPage(p, vm))))
		same(t, fe+" dashboard_body", core.View(fe, "dashboard_body", m), string(Render(dashboardBody(fe, vm))))
	}
}

func TestMain(m *testing.M) {
	core.LoadTemplates()
	m.Run()
}
