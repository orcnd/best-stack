// Package corefast is the run-3 fast path for the Go backends: same routes, SQL, validation and
// sessions as core, but HTML comes from compiled templ components over typed structs and JSON from
// goccy/go-json instead of html/template over map[string]any and encoding/json.
package corefast

//go:generate go run github.com/a-h/templ/cmd/templ@v0.3.1118 generate

import (
	"bytes"
	"context"
	"fmt"

	"bestack/go/core"
	"github.com/a-h/templ"
	json "github.com/goccy/go-json"
)

// Routes is the same table as core.Routes, with the rendering handlers swapped.
func Routes() []core.Route {
	return []core.Route{
		{Method: "GET", Pattern: "/healthz", Handler: core.Healthz},
		{Method: "GET", Pattern: "/api/catalog/products", Handler: apiProducts},
		{Method: "GET", Pattern: "/api/catalog/products/{id}", Handler: apiProduct},
		{Method: "GET", Pattern: "/api/catalog/cart", Handler: apiCartGet},
		{Method: "POST", Pattern: "/api/catalog/cart", Handler: apiCartAdd},
		{Method: "PATCH", Pattern: "/api/catalog/cart/{item_id}", Handler: apiCartPatch},
		{Method: "DELETE", Pattern: "/api/catalog/cart/{item_id}", Handler: apiCartDelete},
		{Method: "GET", Pattern: "/api/kanban/boards", Handler: apiBoards},
		{Method: "GET", Pattern: "/api/kanban/boards/{id}", Handler: apiBoard},
		{Method: "POST", Pattern: "/api/kanban/boards/{id}/cards", Handler: apiCardCreate},
		{Method: "PATCH", Pattern: "/api/kanban/cards/{id}", Handler: apiCardPatch},
		{Method: "DELETE", Pattern: "/api/kanban/cards/{id}", Handler: apiCardDelete},
		{Method: "GET", Pattern: "/api/dashboard", Handler: apiDashboard},
		{Method: "GET", Pattern: "/{fe}/catalog", Handler: catalogHandler},
		{Method: "GET", Pattern: "/{fe}/catalog/p/{id}", Handler: productHandler},
		{Method: "GET", Pattern: "/{fe}/catalog/cart", Handler: cartHandler},
		{Method: "POST", Pattern: "/{fe}/catalog/cart", Handler: cartAddForm},
		{Method: "POST", Pattern: "/{fe}/catalog/cart/{item_id}", Handler: cartItemForm},
		{Method: "GET", Pattern: "/{fe}/kanban/{board_id}", Handler: kanbanHandler},
		{Method: "POST", Pattern: "/{fe}/kanban/{board_id}/cards", Handler: cardCreateForm},
		{Method: "POST", Pattern: "/{fe}/kanban/cards/{id}", Handler: cardUpdateForm},
		{Method: "GET", Pattern: "/{fe}/dashboard", Handler: dashboardHandler},
	}
}

// Render runs a component into a fresh buffer (sized for the largest page, ~20 KB).
func Render(c templ.Component) []byte {
	var b bytes.Buffer
	b.Grow(24 << 10)
	if err := c.Render(context.Background(), &b); err != nil {
		panic(err)
	}
	return b.Bytes()
}

func html(c templ.Component) core.Resp {
	return core.Resp{Status: 200, ContentType: "text/html; charset=utf-8", Body: Render(c)}
}

func jsonResp(s int, v any) core.Resp {
	b, _ := json.Marshal(v)
	return core.Resp{Status: s, ContentType: "application/json", Body: b}
}

func newPage(r *core.Req, fe, nav, title, description string) Page {
	count := int64(0)
	if sid := core.ExistingSession(r); sid != "" {
		count = core.GetCart(sid).Count
	}
	return Page{FE: fe, Base: "/" + fe, Nav: nav, Title: title, Description: description, CartCount: count}
}

// ---------- catalog ----------
func catalogHandler(r *core.Req) core.Resp {
	fe := r.Params["fe"]
	if !core.ValidFE(fe) {
		return core.NotFoundHTML()
	}
	q := core.ParseQuery(r)
	pg := core.Products(q)
	p := newPage(r, fe, "catalog", "Ürün Kataloğu", "Best Stack demo ürün kataloğu")
	vm := CatalogVM{Q: q.Q, Sort: q.Sort, Cat: q.Cat, Categories: core.Categories(), Items: pg.Items,
		TotalFmt: core.FmtInt(pg.Total), Page: pg.Page, Pages: pg.Pages}
	if pg.Page > 1 {
		vm.PrevHref = core.Href(p.Base, q, pg.Page-1)
	}
	if pg.Page < pg.Pages {
		vm.NextHref = core.Href(p.Base, q, pg.Page+1)
	}
	if core.IsHTMX(r) {
		return html(catalogResults(fe, p.Base, vm))
	}
	return html(catalogPage(p, vm))
}

func productHandler(r *core.Req) core.Resp {
	fe := r.Params["fe"]
	id, ok := core.ToInt(r.Params["id"])
	if !core.ValidFE(fe) || !ok {
		return core.NotFoundHTML()
	}
	pr, ok := core.ProductByID(id)
	if !ok {
		return core.NotFoundHTML()
	}
	return html(productPage(newPage(r, fe, "catalog", pr.Name, pr.Description), pr))
}

func cartHandler(r *core.Req) core.Resp {
	fe := r.Params["fe"]
	if !core.ValidFE(fe) {
		return core.NotFoundHTML()
	}
	c := core.Cart{}
	if sid := core.ExistingSession(r); sid != "" {
		c = core.GetCart(sid)
	}
	return html(cartPage(newPage(r, fe, "catalog", "Sepet", "Alışveriş sepeti"), c))
}

func cartAddForm(r *core.Req) core.Resp {
	fe := r.Params["fe"]
	if !core.ValidFE(fe) {
		return core.NotFoundHTML()
	}
	pid, ok := core.ToInt(r.Form["product_id"])
	qty := int64(1)
	if q, ok2 := core.ToInt(r.Form["qty"]); ok2 {
		qty = q
	}
	if !ok || qty < 1 || qty > 99 {
		return core.HTMLStatus(400, "Geçersiz istek")
	}
	if _, ok := core.ProductByID(pid); !ok {
		return core.NotFoundHTML()
	}
	var hdrs [][2]string
	sid := core.SessionID(r, &hdrs)
	core.CartAdd(sid, pid, qty)
	var res core.Resp
	if core.IsHTMX(r) {
		res = html(badge(fe, core.GetCart(sid).Count))
	} else {
		res = core.Redirect(core.SafeReferer(r, "/"+fe+"/catalog"))
	}
	res.Headers = append(res.Headers, hdrs...)
	return res
}

func cartItemForm(r *core.Req) core.Resp {
	fe := r.Params["fe"]
	item, ok := core.ToInt(r.Params["item_id"])
	if !core.ValidFE(fe) || !ok {
		return core.NotFoundHTML()
	}
	qty, ok := core.ToInt(r.Form["qty"])
	if !ok || qty > 99 {
		return core.HTMLStatus(400, "Geçersiz adet")
	}
	sid := core.ExistingSession(r)
	if sid == "" || !core.CartSetQty(sid, item, qty) {
		return core.NotFoundHTML()
	}
	if core.IsHTMX(r) {
		return html(cartBody(fe, "/"+fe, core.GetCart(sid), true))
	}
	return core.Redirect("/" + fe + "/catalog/cart")
}

func apiProducts(r *core.Req) core.Resp {
	p := core.Products(core.ParseQuery(r))
	return jsonResp(200, struct {
		core.ProductPage
		Categories []core.Category `json:"categories"`
	}{p, core.Categories()})
}

func apiProduct(r *core.Req) core.Resp {
	id, ok := core.ToInt(r.Params["id"])
	if !ok {
		return core.NotFoundJSON()
	}
	p, ok := core.ProductByID(id)
	if !ok {
		return core.NotFoundJSON()
	}
	return jsonResp(200, p)
}

func apiCartGet(r *core.Req) core.Resp {
	c := core.Cart{Items: []core.CartItem{}}
	if sid := core.ExistingSession(r); sid != "" {
		c = core.GetCart(sid)
	}
	return jsonResp(200, c)
}

// bodyJSON decodes an object body; numbers stay json.Number so non-integers can be rejected.
func bodyJSON(r *core.Req) (map[string]any, bool) {
	d := json.NewDecoder(bytes.NewReader(r.Body))
	d.UseNumber()
	var m map[string]any
	if err := d.Decode(&m); err != nil || m == nil {
		return nil, false
	}
	return m, true
}

func intField(m map[string]any, k string) (int64, bool) {
	n, ok := m[k].(json.Number)
	if !ok {
		return 0, false
	}
	v, err := n.Int64()
	return v, err == nil
}

func apiCartAdd(r *core.Req) core.Resp {
	b, ok := bodyJSON(r)
	if !ok {
		return core.BadJSON()
	}
	pid, ok := intField(b, "product_id")
	if !ok {
		return core.BadJSON()
	}
	qty := int64(1)
	if _, has := b["qty"]; has {
		if qty, ok = intField(b, "qty"); !ok {
			return core.BadJSON()
		}
	}
	if qty < 1 || qty > 99 {
		return core.BadJSON()
	}
	if _, ok := core.ProductByID(pid); !ok {
		return core.NotFoundJSON()
	}
	var hdrs [][2]string
	sid := core.SessionID(r, &hdrs)
	core.CartAdd(sid, pid, qty)
	res := jsonResp(201, core.GetCart(sid))
	res.Headers = hdrs
	return res
}

func apiCartPatch(r *core.Req) core.Resp {
	item, ok := core.ToInt(r.Params["item_id"])
	if !ok {
		return core.NotFoundJSON()
	}
	b, ok := bodyJSON(r)
	if !ok {
		return core.BadJSON()
	}
	qty, ok := intField(b, "qty")
	if !ok || qty < 0 || qty > 99 {
		return core.BadJSON()
	}
	sid := core.ExistingSession(r)
	if sid == "" || !core.CartSetQty(sid, item, qty) {
		return core.NotFoundJSON()
	}
	return jsonResp(200, core.GetCart(sid))
}

func apiCartDelete(r *core.Req) core.Resp {
	item, ok := core.ToInt(r.Params["item_id"])
	sid := core.ExistingSession(r)
	if !ok || sid == "" || !core.CartSetQty(sid, item, 0) {
		return core.NotFoundJSON()
	}
	return jsonResp(200, core.GetCart(sid))
}

// ---------- kanban ----------
func boardFragment(fe string, boardID int64) core.Resp {
	b, ok := core.GetBoard(boardID)
	if !ok {
		return core.NotFoundHTML()
	}
	_ = core.Boards() // the reference fragment queries the board list too; keep the same DB work
	return html(board(fe, "/"+fe, b))
}

func kanbanHandler(r *core.Req) core.Resp {
	fe := r.Params["fe"]
	id, ok := core.ToInt(r.Params["board_id"])
	if !core.ValidFE(fe) || !ok {
		return core.NotFoundHTML()
	}
	b, ok := core.GetBoard(id)
	if !ok {
		return core.NotFoundHTML()
	}
	p := newPage(r, fe, "kanban", b.Name+" · Kanban", "Kanban panosu")
	return html(kanbanPage(p, BoardVM{Board: b, Boards: core.Boards()}))
}

func cardCreateForm(r *core.Req) core.Resp {
	fe := r.Params["fe"]
	id, ok := core.ToInt(r.Params["board_id"])
	if !core.ValidFE(fe) || !ok {
		return core.NotFoundHTML()
	}
	title, ok := core.ValidTitle(r.Form["title"])
	if !ok {
		return core.HTMLStatus(422, "Geçersiz başlık")
	}
	col := r.Form["column"]
	if col == "" {
		col = "backlog"
	}
	if _, ok := core.GetBoard(id); !ok {
		return core.NotFoundHTML()
	}
	if _, ok := core.CardCreate(id, title, col); !ok {
		return core.HTMLStatus(422, "Geçersiz sütun")
	}
	if core.IsHTMX(r) {
		return boardFragment(fe, id)
	}
	return core.Redirect(fmt.Sprintf("/%s/kanban/%d", fe, id))
}

func cardUpdateForm(r *core.Req) core.Resp {
	fe := r.Params["fe"]
	id, ok := core.ToInt(r.Params["id"])
	if !core.ValidFE(fe) || !ok {
		return core.NotFoundHTML()
	}
	card, ok := core.CardUpdate(id, nil, "")
	if !ok {
		return core.NotFoundHTML()
	}
	if r.Form["delete"] == "1" {
		core.CardDelete(id)
	} else {
		var title *string
		if raw, has := r.Form["title"]; has {
			t, ok := core.ValidTitle(raw)
			if !ok {
				return core.HTMLStatus(422, "Geçersiz başlık")
			}
			title = &t
		}
		core.CardUpdate(id, title, r.Form["move"])
	}
	if core.IsHTMX(r) {
		return boardFragment(fe, card.BoardID)
	}
	return core.Redirect(fmt.Sprintf("/%s/kanban/%d", fe, card.BoardID))
}

func apiBoards(*core.Req) core.Resp { return jsonResp(200, core.Boards()) }

func apiBoard(r *core.Req) core.Resp {
	id, ok := core.ToInt(r.Params["id"])
	if !ok {
		return core.NotFoundJSON()
	}
	b, ok := core.GetBoard(id)
	if !ok {
		return core.NotFoundJSON()
	}
	return jsonResp(200, b)
}

func apiCardCreate(r *core.Req) core.Resp {
	id, ok := core.ToInt(r.Params["id"])
	if !ok {
		return core.NotFoundJSON()
	}
	b, ok := bodyJSON(r)
	if !ok {
		return core.BadJSON()
	}
	raw, isStr := b["title"].(string)
	title, ok := core.ValidTitle(raw)
	if !isStr || !ok {
		return core.Unprocessable()
	}
	col := "backlog"
	if c, isStr := b["column"].(string); isStr {
		col = c
	}
	if !core.ValidColumn(col) {
		return core.Resp{Status: 422, ContentType: "application/json", Body: []byte(`{"error":"invalid_column"}`)}
	}
	c, ok := core.CardCreate(id, title, col)
	if !ok {
		return core.NotFoundJSON()
	}
	return jsonResp(201, c)
}

func apiCardPatch(r *core.Req) core.Resp {
	id, ok := core.ToInt(r.Params["id"])
	if !ok {
		return core.NotFoundJSON()
	}
	b, ok := bodyJSON(r)
	if !ok {
		return core.BadJSON()
	}
	var title *string
	if v, has := b["title"]; has {
		s, isStr := v.(string)
		t, ok := core.ValidTitle(s)
		if !isStr || !ok {
			return core.Unprocessable()
		}
		title = &t
	}
	move := ""
	if v, has := b["move"]; has {
		s, isStr := v.(string)
		if !isStr || (s != "left" && s != "right") {
			return core.BadJSON()
		}
		move = s
	}
	c, ok := core.CardUpdate(id, title, move)
	if !ok {
		return core.NotFoundJSON()
	}
	return jsonResp(200, c)
}

func apiCardDelete(r *core.Req) core.Resp {
	id, ok := core.ToInt(r.Params["id"])
	if !ok || !core.CardDelete(id) {
		return core.NotFoundJSON()
	}
	return core.Resp{Status: 204, ContentType: "application/json"}
}

// ---------- dashboard ----------
func dashboardHandler(r *core.Req) core.Resp {
	fe := r.Params["fe"]
	if !core.ValidFE(fe) {
		return core.NotFoundHTML()
	}
	rng, ok := core.ParseRange(r)
	if !ok {
		return core.HTMLStatus(400, "Geçersiz aralık")
	}
	d := core.GetDashboard(rng)
	vm := DashboardVM{Range: rng, D: d, Chart: core.DailyChartSVG(d.Daily)}
	if core.IsHTMX(r) {
		return html(dashboardBody(fe, vm))
	}
	return html(dashboardPage(newPage(r, fe, "dashboard", "Analitik", "Analitik dashboard"), vm))
}

func apiDashboard(r *core.Req) core.Resp {
	rng, ok := core.ParseRange(r)
	if !ok {
		return core.Resp{Status: 400, ContentType: "application/json", Body: []byte(`{"error":"invalid_range"}`)}
	}
	return jsonResp(200, core.GetDashboard(rng))
}
