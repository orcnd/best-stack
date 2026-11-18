package core

import (
	"bytes"
	"crypto/rand"
	"encoding/hex"
	"encoding/json"
	"fmt"
	"html/template"
	"strconv"
	"strings"
	"unicode/utf8"
)

type Handler func(*Req) Resp
type Route struct {
	Method, Pattern string
	Handler         Handler
}

// Routes is the single route table (same as the C++ and Rust cores).
func Routes() []Route {
	return []Route{
		{"GET", "/healthz", healthz},
		{"GET", "/api/catalog/products", apiProducts},
		{"GET", "/api/catalog/products/{id}", apiProduct},
		{"GET", "/api/catalog/cart", apiCartGet},
		{"POST", "/api/catalog/cart", apiCartAdd},
		{"PATCH", "/api/catalog/cart/{item_id}", apiCartPatch},
		{"DELETE", "/api/catalog/cart/{item_id}", apiCartDelete},
		{"GET", "/api/kanban/boards", apiBoards},
		{"GET", "/api/kanban/boards/{id}", apiBoard},
		{"POST", "/api/kanban/boards/{id}/cards", apiCardCreate},
		{"PATCH", "/api/kanban/cards/{id}", apiCardPatch},
		{"DELETE", "/api/kanban/cards/{id}", apiCardDelete},
		{"GET", "/api/dashboard", apiDashboard},
		{"GET", "/{fe}/catalog", catalogPage},
		{"GET", "/{fe}/catalog/p/{id}", productPage},
		{"GET", "/{fe}/catalog/cart", cartPage},
		{"POST", "/{fe}/catalog/cart", cartAddForm},
		{"POST", "/{fe}/catalog/cart/{item_id}", cartItemForm},
		{"GET", "/{fe}/kanban/{board_id}", kanbanPage},
		{"POST", "/{fe}/kanban/{board_id}/cards", cardCreateForm},
		{"POST", "/{fe}/kanban/cards/{id}", cardUpdateForm},
		{"GET", "/{fe}/dashboard", dashboardPage},
	}
}

type M = map[string]any

// ---------- helpers ----------
func html(body string) Resp { return Resp{Status: 200, ContentType: "text/html; charset=utf-8", Body: []byte(body)} }
func htmlStatus(s int, body string) Resp {
	return Resp{Status: s, ContentType: "text/html; charset=utf-8", Body: []byte(body)}
}
func jsonResp(s int, v any) Resp {
	b, _ := json.Marshal(v)
	return Resp{Status: s, ContentType: "application/json", Body: b}
}
func notFoundHTML() Resp { return htmlStatus(404, "<!doctype html><title>404</title><h1>Bulunamadı</h1>") }
func notFoundJSON() Resp {
	return Resp{Status: 404, ContentType: "application/json", Body: []byte(`{"error":"not_found"}`)}
}
func badJSON() Resp {
	return Resp{Status: 400, ContentType: "application/json", Body: []byte(`{"error":"bad_request"}`)}
}
func redirect(loc string) Resp {
	return Resp{Status: 303, ContentType: "text/plain", Headers: [][2]string{{"Location", loc}}}
}

func validFE(fe string) bool { return fe == "htmx" || fe == "alpine" || fe == "vanilla" }
func isHTMX(r *Req) bool    { return r.Header("hx-request") == "true" }
func toInt(s string) (int64, bool) {
	if s == "" || len(s) > 9 {
		return 0, false
	}
	for _, c := range s {
		if c < '0' || c > '9' {
			return 0, false
		}
	}
	v, err := strconv.ParseInt(s, 10, 64)
	return v, err == nil
}

func validSID(s string) bool {
	if len(s) != 16 {
		return false
	}
	for _, c := range s {
		if !(c >= '0' && c <= '9' || c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z') {
			return false
		}
	}
	return true
}
func existingSession(r *Req) string {
	if c := r.Cookies["bs_sid"]; validSID(c) {
		return c
	}
	if h := r.Header("x-session-id"); validSID(h) {
		return h
	}
	return ""
}
func sessionID(r *Req, hdrs *[][2]string) string {
	if s := existingSession(r); s != "" {
		return s
	}
	b := make([]byte, 8)
	_, _ = rand.Read(b)
	id := hex.EncodeToString(b)
	*hdrs = append(*hdrs, [2]string{"Set-Cookie", "bs_sid=" + id + "; Path=/; SameSite=Lax; HttpOnly"})
	return id
}

func validTitle(raw string) (string, bool) {
	t := strings.Trim(raw, " \t\r\n")
	n := utf8.RuneCountInString(t)
	return t, n >= 1 && n <= MaxTitle
}

func safeReferer(r *Req, fallback string) string {
	ref := r.Header("referer")
	if strings.HasPrefix(ref, "http://") || strings.HasPrefix(ref, "https://") {
		after := ref[strings.Index(ref, "//")+2:]
		if i := strings.IndexByte(after, '/'); i >= 0 {
			ref = after[i:]
		} else {
			ref = "/"
		}
	}
	if ref == "" || ref[0] != '/' || strings.HasPrefix(ref, "//") {
		return fallback
	}
	return ref
}

func view(fe, name string, data M) string {
	data["fe"] = fe
	data["base"] = "/" + fe
	var b bytes.Buffer
	if err := tmpl.ExecuteTemplate(&b, name, data); err != nil {
		panic(err)
	}
	return b.String()
}

func page(r *Req, nav, title, description string) M {
	count := int64(0)
	if sid := existingSession(r); sid != "" {
		count = GetCart(sid).Count
	}
	return M{"nav": nav, "title": title, "description": description, "cart_count": count, "fragment": false}
}

func productVM(p Product) M {
	return M{"id": p.ID, "name": p.Name, "category": p.Category, "image": p.Image, "description": p.Description,
		"price": FmtMoney(p.PriceCents), "rating": FmtRating(p.Rating), "stock": p.Stock}
}

func cartVM(c Cart) M {
	items := []M{}
	for _, it := range c.Items {
		items = append(items, M{"id": it.ID, "product_id": it.Product.ID, "name": it.Product.Name, "price": FmtMoney(it.Product.PriceCents),
			"qty": it.Qty, "line": FmtMoney(it.Qty * it.Product.PriceCents)})
	}
	return M{"items": items, "total": FmtMoney(c.TotalCents), "empty": len(c.Items) == 0, "cart_count": c.Count}
}

var columnLabels = map[string]string{"backlog": "Backlog", "todo": "Yapılacak", "doing": "Yapılıyor", "done": "Bitti"}

func boardVM(b Board, all []BoardSummary) M {
	tabs := []M{}
	for _, s := range all {
		tabs = append(tabs, M{"id": s.ID, "name": s.Name, "active": s.ID == b.ID})
	}
	cols := []M{}
	for i, k := range Columns {
		cards := []M{}
		for _, c := range b.Columns[k] {
			cards = append(cards, M{"id": c.ID, "title": c.Title, "labels": strings.Join(c.Labels, ", "), "assignee": c.Assignee,
				"due": c.DueDate, "first": i == 0, "last": i == len(Columns)-1})
		}
		cols = append(cols, M{"key": k, "label": columnLabels[k], "count": len(cards), "cards": cards})
	}
	return M{"board": M{"id": b.ID, "name": b.Name}, "boards": tabs, "columns": cols}
}

// ---------- catalog ----------
func parseQuery(r *Req) ProductQuery {
	q := ProductQuery{Q: r.Query["q"], Cat: r.Query["cat"], Page: 1}
	if s := r.Query["sort"]; s == "price_asc" || s == "price_desc" {
		q.Sort = s
	}
	if p, ok := toInt(r.Query["page"]); ok {
		q.Page = p
	}
	return q
}

func href(base string, q ProductQuery, page int64) string {
	parts := []string{}
	add := func(k, v string) {
		if v != "" {
			parts = append(parts, k+"="+cppEscape(v))
		}
	}
	add("q", q.Q)
	add("cat", q.Cat)
	add("sort", q.Sort)
	if page > 1 {
		parts = append(parts, fmt.Sprintf("page=%d", page))
	}
	if len(parts) == 0 {
		return base + "/catalog"
	}
	return base + "/catalog?" + strings.Join(parts, "&")
}

// cppEscape matches the C++ url_encode (alnum and -_.~ literal, everything else %XX).
func cppEscape(s string) string {
	var b strings.Builder
	for i := 0; i < len(s); i++ {
		c := s[i]
		if c >= 'a' && c <= 'z' || c >= 'A' && c <= 'Z' || c >= '0' && c <= '9' || c == '-' || c == '_' || c == '.' || c == '~' {
			b.WriteByte(c)
		} else {
			fmt.Fprintf(&b, "%%%02X", c)
		}
	}
	return b.String()
}

func catalogPage(r *Req) Resp {
	fe := r.Params["fe"]
	if !validFE(fe) {
		return notFoundHTML()
	}
	q := parseQuery(r)
	p := Products(q)
	vm := page(r, "catalog", "Ürün Kataloğu", "Best Stack demo ürün kataloğu")
	cats := []M{}
	for _, c := range Categories() {
		cats = append(cats, M{"slug": c.Slug, "name": c.Name, "selected": c.Slug == q.Cat})
	}
	items := []M{}
	for _, it := range p.Items {
		items = append(items, productVM(it))
	}
	base := "/" + fe
	vm["q"], vm["sort"], vm["categories"], vm["items"], vm["has_items"] = q.Q, q.Sort, cats, items, len(items) > 0
	vm["total"], vm["total_fmt"], vm["page"], vm["pages"] = p.Total, FmtInt(p.Total), p.Page, p.Pages
	vm["prev_href"], vm["next_href"] = "", ""
	if p.Page > 1 {
		vm["prev_href"] = href(base, q, p.Page-1)
	}
	if p.Page < p.Pages {
		vm["next_href"] = href(base, q, p.Page+1)
	}
	if isHTMX(r) {
		vm["fragment"] = true
		return html(view(fe, "catalog_results", vm))
	}
	return html(view(fe, "catalog", vm))
}

func productPage(r *Req) Resp {
	fe := r.Params["fe"]
	id, ok := toInt(r.Params["id"])
	if !validFE(fe) || !ok {
		return notFoundHTML()
	}
	p, ok := ProductByID(id)
	if !ok {
		return notFoundHTML()
	}
	vm := page(r, "catalog", p.Name, p.Description)
	vm["product"] = productVM(p)
	return html(view(fe, "product", vm))
}

func cartPage(r *Req) Resp {
	fe := r.Params["fe"]
	if !validFE(fe) {
		return notFoundHTML()
	}
	c := Cart{}
	if sid := existingSession(r); sid != "" {
		c = GetCart(sid)
	}
	vm := page(r, "catalog", "Sepet", "Alışveriş sepeti")
	vm["cart"] = cartVM(c)
	return html(view(fe, "cart", vm))
}

func cartAddForm(r *Req) Resp {
	fe := r.Params["fe"]
	if !validFE(fe) {
		return notFoundHTML()
	}
	pid, ok := toInt(r.Form["product_id"])
	qty := int64(1)
	if q, ok2 := toInt(r.Form["qty"]); ok2 {
		qty = q
	}
	if !ok || qty < 1 || qty > 99 {
		return htmlStatus(400, "Geçersiz istek")
	}
	if _, ok := ProductByID(pid); !ok {
		return notFoundHTML()
	}
	var hdrs [][2]string
	sid := sessionID(r, &hdrs)
	CartAdd(sid, pid, qty)
	var res Resp
	if isHTMX(r) {
		res = html(view(fe, "badge", M{"cart_count": GetCart(sid).Count, "fragment": true}))
	} else {
		res = redirect(safeReferer(r, "/"+fe+"/catalog"))
	}
	res.Headers = append(res.Headers, hdrs...)
	return res
}

func cartItemForm(r *Req) Resp {
	fe := r.Params["fe"]
	item, ok := toInt(r.Params["item_id"])
	if !validFE(fe) || !ok {
		return notFoundHTML()
	}
	qty, ok := toInt(r.Form["qty"])
	if !ok || qty > 99 {
		return htmlStatus(400, "Geçersiz adet")
	}
	sid := existingSession(r)
	if sid == "" || !CartSetQty(sid, item, qty) {
		return notFoundHTML()
	}
	if isHTMX(r) {
		return html(view(fe, "cart_body", M{"cart": cartVM(GetCart(sid)), "fragment": true}))
	}
	return redirect("/" + fe + "/catalog/cart")
}

func apiProducts(r *Req) Resp {
	p := Products(parseQuery(r))
	return jsonResp(200, struct {
		ProductPage
		Categories []Category `json:"categories"`
	}{p, Categories()})
}

func apiProduct(r *Req) Resp {
	id, ok := toInt(r.Params["id"])
	if !ok {
		return notFoundJSON()
	}
	p, ok := ProductByID(id)
	if !ok {
		return notFoundJSON()
	}
	return jsonResp(200, p)
}

func apiCartGet(r *Req) Resp {
	c := Cart{Items: []CartItem{}}
	if sid := existingSession(r); sid != "" {
		c = GetCart(sid)
	}
	return jsonResp(200, c)
}

// bodyJSON decodes an object body; numbers stay json.Number so non-integers can be rejected.
func bodyJSON(r *Req) (map[string]any, bool) {
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

func apiCartAdd(r *Req) Resp {
	b, ok := bodyJSON(r)
	if !ok {
		return badJSON()
	}
	pid, ok := intField(b, "product_id")
	if !ok {
		return badJSON()
	}
	qty := int64(1)
	if _, has := b["qty"]; has {
		if qty, ok = intField(b, "qty"); !ok {
			return badJSON()
		}
	}
	if qty < 1 || qty > 99 {
		return badJSON()
	}
	if _, ok := ProductByID(pid); !ok {
		return notFoundJSON()
	}
	var hdrs [][2]string
	sid := sessionID(r, &hdrs)
	CartAdd(sid, pid, qty)
	res := jsonResp(201, GetCart(sid))
	res.Headers = hdrs
	return res
}

func apiCartPatch(r *Req) Resp {
	item, ok := toInt(r.Params["item_id"])
	if !ok {
		return notFoundJSON()
	}
	b, ok := bodyJSON(r)
	if !ok {
		return badJSON()
	}
	qty, ok := intField(b, "qty")
	if !ok || qty < 0 || qty > 99 {
		return badJSON()
	}
	sid := existingSession(r)
	if sid == "" || !CartSetQty(sid, item, qty) {
		return notFoundJSON()
	}
	return jsonResp(200, GetCart(sid))
}

func apiCartDelete(r *Req) Resp {
	item, ok := toInt(r.Params["item_id"])
	sid := existingSession(r)
	if !ok || sid == "" || !CartSetQty(sid, item, 0) {
		return notFoundJSON()
	}
	return jsonResp(200, GetCart(sid))
}

// ---------- kanban ----------
func boardFragment(fe string, boardID int64) Resp {
	b, ok := GetBoard(boardID)
	if !ok {
		return notFoundHTML()
	}
	vm := boardVM(b, Boards())
	vm["fragment"] = true
	return html(view(fe, "board", vm))
}

func kanbanPage(r *Req) Resp {
	fe := r.Params["fe"]
	id, ok := toInt(r.Params["board_id"])
	if !validFE(fe) || !ok {
		return notFoundHTML()
	}
	b, ok := GetBoard(id)
	if !ok {
		return notFoundHTML()
	}
	vm := page(r, "kanban", b.Name+" · Kanban", "Kanban panosu")
	for k, v := range boardVM(b, Boards()) {
		vm[k] = v
	}
	return html(view(fe, "kanban", vm))
}

func cardCreateForm(r *Req) Resp {
	fe := r.Params["fe"]
	id, ok := toInt(r.Params["board_id"])
	if !validFE(fe) || !ok {
		return notFoundHTML()
	}
	title, ok := validTitle(r.Form["title"])
	if !ok {
		return htmlStatus(422, "Geçersiz başlık")
	}
	col := r.Form["column"]
	if col == "" {
		col = "backlog"
	}
	if _, ok := GetBoard(id); !ok {
		return notFoundHTML()
	}
	if _, ok := CardCreate(id, title, col); !ok {
		return htmlStatus(422, "Geçersiz sütun")
	}
	if isHTMX(r) {
		return boardFragment(fe, id)
	}
	return redirect(fmt.Sprintf("/%s/kanban/%d", fe, id))
}

func cardUpdateForm(r *Req) Resp {
	fe := r.Params["fe"]
	id, ok := toInt(r.Params["id"])
	if !validFE(fe) || !ok {
		return notFoundHTML()
	}
	card, ok := CardUpdate(id, nil, "")
	if !ok {
		return notFoundHTML()
	}
	if r.Form["delete"] == "1" {
		CardDelete(id)
	} else {
		var title *string
		if raw, has := r.Form["title"]; has {
			t, ok := validTitle(raw)
			if !ok {
				return htmlStatus(422, "Geçersiz başlık")
			}
			title = &t
		}
		CardUpdate(id, title, r.Form["move"])
	}
	if isHTMX(r) {
		return boardFragment(fe, card.BoardID)
	}
	return redirect(fmt.Sprintf("/%s/kanban/%d", fe, card.BoardID))
}

func apiBoards(*Req) Resp { return jsonResp(200, Boards()) }

func apiBoard(r *Req) Resp {
	id, ok := toInt(r.Params["id"])
	if !ok {
		return notFoundJSON()
	}
	b, ok := GetBoard(id)
	if !ok {
		return notFoundJSON()
	}
	return jsonResp(200, b)
}

func unprocessable() Resp {
	return Resp{Status: 422, ContentType: "application/json", Body: []byte(`{"error":"invalid_title"}`)}
}

func apiCardCreate(r *Req) Resp {
	id, ok := toInt(r.Params["id"])
	if !ok {
		return notFoundJSON()
	}
	b, ok := bodyJSON(r)
	if !ok {
		return badJSON()
	}
	raw, isStr := b["title"].(string)
	title, ok := validTitle(raw)
	if !isStr || !ok {
		return unprocessable()
	}
	col := "backlog"
	if c, isStr := b["column"].(string); isStr {
		col = c
	}
	if !validColumn(col) {
		return Resp{Status: 422, ContentType: "application/json", Body: []byte(`{"error":"invalid_column"}`)}
	}
	c, ok := CardCreate(id, title, col)
	if !ok {
		return notFoundJSON()
	}
	return jsonResp(201, c)
}

func apiCardPatch(r *Req) Resp {
	id, ok := toInt(r.Params["id"])
	if !ok {
		return notFoundJSON()
	}
	b, ok := bodyJSON(r)
	if !ok {
		return badJSON()
	}
	var title *string
	if v, has := b["title"]; has {
		s, isStr := v.(string)
		t, ok := validTitle(s)
		if !isStr || !ok {
			return unprocessable()
		}
		title = &t
	}
	move := ""
	if v, has := b["move"]; has {
		s, isStr := v.(string)
		if !isStr || (s != "left" && s != "right") {
			return badJSON()
		}
		move = s
	}
	c, ok := CardUpdate(id, title, move)
	if !ok {
		return notFoundJSON()
	}
	return jsonResp(200, c)
}

func apiCardDelete(r *Req) Resp {
	id, ok := toInt(r.Params["id"])
	if !ok || !CardDelete(id) {
		return notFoundJSON()
	}
	return Resp{Status: 204, ContentType: "application/json"}
}

// ---------- dashboard ----------
func parseRange(r *Req) (int64, bool) {
	switch v, has := r.Query["range"]; {
	case !has || v == "" || v == "30":
		return 30, true
	case v == "7":
		return 7, true
	case v == "90":
		return 90, true
	}
	return 0, false
}

func dashboardPage(r *Req) Resp {
	fe := r.Params["fe"]
	if !validFE(fe) {
		return notFoundHTML()
	}
	rng, ok := parseRange(r)
	if !ok {
		return htmlStatus(400, "Geçersiz aralık")
	}
	d := GetDashboard(rng)
	vm := page(r, "dashboard", "Analitik", "Analitik dashboard")
	ranges := []M{}
	for _, x := range []struct {
		v int64
		l string
	}{{7, "Son 7 gün"}, {30, "Son 30 gün"}, {90, "Son 90 gün"}} {
		ranges = append(ranges, M{"value": x.v, "label": x.l, "selected": x.v == rng})
	}
	countries := []M{}
	for _, c := range d.Countries {
		countries = append(countries, M{"country": c.Country, "events": FmtInt(c.Events)})
	}
	items := []M{}
	for _, it := range d.TopItems {
		items = append(items, M{"item": it.Item, "purchases": FmtInt(it.Purchases), "revenue": FmtMoney(it.RevenueCents)})
	}
	vm["ranges"], vm["countries"], vm["top_items"] = ranges, countries, items
	vm["kpis"] = M{"events": FmtInt(d.Kpis.Events), "users": FmtInt(d.Kpis.Users), "revenue": FmtMoney(d.Kpis.RevenueCents), "conversion": FmtPct(d.Kpis.Conversion)}
	vm["chart"] = template.HTML(DailyChartSVG(d.Daily)) // trusted, generated markup
	if isHTMX(r) {
		vm["fragment"] = true
		return html(view(fe, "dashboard_body", vm))
	}
	return html(view(fe, "dashboard", vm))
}

func apiDashboard(r *Req) Resp {
	rng, ok := parseRange(r)
	if !ok {
		return Resp{Status: 400, ContentType: "application/json", Body: []byte(`{"error":"invalid_range"}`)}
	}
	return jsonResp(200, GetDashboard(rng))
}

func healthz(*Req) Resp { return Resp{Status: 200, ContentType: "text/plain", Body: []byte("ok")} }
