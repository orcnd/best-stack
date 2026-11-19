package core

import (
	"database/sql"
	"fmt"
	"strings"
	"time"

	sqlite3 "github.com/mattn/go-sqlite3"
)

// Same queries and pragmas as shared/cpp/src/db.cpp.
var db *sql.DB

func init() {
	sql.Register("sqlite3_bs", &sqlite3.SQLiteDriver{ConnectHook: func(c *sqlite3.SQLiteConn) error {
		_, err := c.Exec("PRAGMA journal_mode = WAL; PRAGMA synchronous = NORMAL; PRAGMA temp_store = MEMORY; PRAGMA mmap_size = 268435456; PRAGMA busy_timeout = 5000;", nil)
		return err
	}})
}

func openDB(path string, pool int) {
	var err error
	db, err = sql.Open("sqlite3_bs", "file:"+path)
	if err != nil {
		panic(err)
	}
	db.SetMaxOpenConns(pool)
	db.SetMaxIdleConns(pool)
	db.SetConnMaxLifetime(time.Hour)
}

const productCols = "p.id, p.category_id, c.name, p.name, p.description, p.image, p.price_cents, p.stock, p.rating"
const cardCols = "id, board_id, position, col, title, description, assignee, due_date, labels"

type scanner interface{ Scan(...any) error }

func scanProduct(s scanner, extra ...any) (Product, error) {
	var p Product
	dst := append(extra, &p.ID, &p.CategoryID, &p.Category, &p.Name, &p.Description, &p.Image, &p.PriceCents, &p.Stock, &p.Rating)
	err := s.Scan(dst...)
	return p, err
}

func scanCard(s scanner) (Card, error) {
	var c Card
	var labels string
	err := s.Scan(&c.ID, &c.BoardID, &c.Position, &c.Column, &c.Title, &c.Description, &c.Assignee, &c.DueDate, &labels)
	c.Labels = []string{}
	for _, l := range strings.Split(labels, ",") {
		if l != "" {
			c.Labels = append(c.Labels, l)
		}
	}
	return c, err
}

func likePattern(q string) string {
	var b strings.Builder
	b.WriteByte('%')
	for _, r := range q {
		if r == '%' || r == '_' || r == '\\' {
			b.WriteByte('\\')
		}
		b.WriteRune(r)
	}
	b.WriteByte('%')
	return b.String()
}

func Categories() []Category {
	rows, err := db.Query("SELECT id, name, slug FROM categories ORDER BY id")
	must(err)
	defer rows.Close()
	out := []Category{}
	for rows.Next() {
		var c Category
		must(rows.Scan(&c.ID, &c.Name, &c.Slug))
		out = append(out, c)
	}
	return out
}

func Products(q ProductQuery) ProductPage {
	where := " WHERE 1=1"
	args := []any{}
	if q.Q != "" {
		where += ` AND p.name LIKE ? ESCAPE '\'`
		args = append(args, likePattern(q.Q))
	}
	if q.Cat != "" {
		where += " AND c.slug = ?"
		args = append(args, q.Cat)
	}
	from := " FROM products p JOIN categories c ON c.id = p.category_id"
	var total int64
	must(db.QueryRow("SELECT count(*)"+from+where, args...).Scan(&total))
	pages := max((total+PerPage-1)/PerPage, 1)
	page := min(max(q.Page, 1), pages)
	order := " ORDER BY p.id"
	switch q.Sort {
	case "price_asc":
		order = " ORDER BY p.price_cents ASC, p.id"
	case "price_desc":
		order = " ORDER BY p.price_cents DESC, p.id"
	}
	rows, err := db.Query(fmt.Sprintf("SELECT %s%s%s%s LIMIT %d OFFSET %d", productCols, from, where, order, PerPage, (page-1)*PerPage), args...)
	must(err)
	defer rows.Close()
	items := []Product{}
	for rows.Next() {
		p, err := scanProduct(rows)
		must(err)
		items = append(items, p)
	}
	return ProductPage{Items: items, Total: total, Page: page, Pages: pages}
}

func ProductByID(id int64) (Product, bool) {
	p, err := scanProduct(db.QueryRow("SELECT "+productCols+" FROM products p JOIN categories c ON c.id = p.category_id WHERE p.id = ?", id))
	if err == sql.ErrNoRows {
		return p, false
	}
	must(err)
	return p, true
}

func GetCart(sid string) Cart {
	rows, err := db.Query("SELECT ci.id, ci.qty, "+productCols+" FROM cart_items ci JOIN products p ON p.id = ci.product_id JOIN categories c ON c.id = p.category_id WHERE ci.session_id = ? ORDER BY ci.id", sid)
	must(err)
	defer rows.Close()
	c := Cart{Items: []CartItem{}}
	for rows.Next() {
		var it CartItem
		p, err := scanProduct(rows, &it.ID, &it.Qty)
		must(err)
		it.Product = p
		c.Count += it.Qty
		c.TotalCents += it.Qty * p.PriceCents
		c.Items = append(c.Items, it)
	}
	return c
}

func CartAdd(sid string, productID, qty int64) {
	if _, ok := ProductByID(productID); qty <= 0 || !ok {
		return
	}
	_, err := db.Exec("INSERT INTO cart_items (session_id, product_id, qty) VALUES (?, ?, ?) ON CONFLICT (session_id, product_id) DO UPDATE SET qty = qty + excluded.qty", sid, productID, qty)
	must(err)
}

func CartSetQty(sid string, itemID, qty int64) bool {
	var res sql.Result
	var err error
	if qty <= 0 {
		res, err = db.Exec("DELETE FROM cart_items WHERE id = ? AND session_id = ?", itemID, sid)
	} else {
		res, err = db.Exec("UPDATE cart_items SET qty = ? WHERE id = ? AND session_id = ?", qty, itemID, sid)
	}
	must(err)
	n, _ := res.RowsAffected()
	return n > 0
}

func Boards() []BoardSummary {
	rows, err := db.Query("SELECT b.id, b.name, count(c.id) FROM boards b LEFT JOIN cards c ON c.board_id = b.id GROUP BY b.id ORDER BY b.id")
	must(err)
	defer rows.Close()
	out := []BoardSummary{}
	for rows.Next() {
		var b BoardSummary
		must(rows.Scan(&b.ID, &b.Name, &b.CardCount))
		out = append(out, b)
	}
	return out
}

func GetBoard(id int64) (Board, bool) {
	b := Board{ID: id, Columns: map[string][]Card{}}
	if err := db.QueryRow("SELECT name FROM boards WHERE id = ?", id).Scan(&b.Name); err == sql.ErrNoRows {
		return b, false
	} else {
		must(err)
	}
	for _, k := range Columns {
		b.Columns[k] = []Card{}
	}
	rows, err := db.Query("SELECT "+cardCols+" FROM cards WHERE board_id = ? ORDER BY position, id", id)
	must(err)
	defer rows.Close()
	for rows.Next() {
		c, err := scanCard(rows)
		must(err)
		b.Columns[c.Column] = append(b.Columns[c.Column], c)
	}
	return b, true
}

func cardByID(id int64) (Card, bool) {
	c, err := scanCard(db.QueryRow("SELECT "+cardCols+" FROM cards WHERE id = ?", id))
	if err == sql.ErrNoRows {
		return c, false
	}
	must(err)
	return c, true
}

func nextPosition(boardID int64, col string) int64 {
	var p int64
	must(db.QueryRow("SELECT COALESCE(MAX(position) + 1, 0) FROM cards WHERE board_id = ? AND col = ?", boardID, col).Scan(&p))
	return p
}

func validColumn(c string) bool {
	for _, k := range Columns {
		if k == c {
			return true
		}
	}
	return false
}

func CardCreate(boardID int64, title, column string) (Card, bool) {
	if !validColumn(column) {
		return Card{}, false
	}
	var one int
	if err := db.QueryRow("SELECT 1 FROM boards WHERE id = ?", boardID).Scan(&one); err != nil {
		return Card{}, false
	}
	res, err := db.Exec("INSERT INTO cards (board_id, col, position, title, description, labels, assignee, due_date) VALUES (?, ?, ?, ?, '', '', '', '')", boardID, column, nextPosition(boardID, column), title)
	must(err)
	id, _ := res.LastInsertId()
	return cardByID(id)
}

func CardUpdate(id int64, title *string, move string) (Card, bool) {
	cur, ok := cardByID(id)
	if !ok {
		return cur, false
	}
	if title != nil {
		_, err := db.Exec("UPDATE cards SET title = ? WHERE id = ?", *title, id)
		must(err)
	}
	if move == "left" || move == "right" {
		idx := 0
		for i, k := range Columns {
			if k == cur.Column {
				idx = i
			}
		}
		next := idx - 1
		if move == "right" {
			next = idx + 1
		}
		next = min(max(next, 0), len(Columns)-1)
		if next != idx {
			col := Columns[next]
			_, err := db.Exec("UPDATE cards SET col = ?, position = ? WHERE id = ?", col, nextPosition(cur.BoardID, col), id)
			must(err)
		}
	}
	return cardByID(id)
}

func CardDelete(id int64) bool {
	res, err := db.Exec("DELETE FROM cards WHERE id = ?", id)
	must(err)
	n, _ := res.RowsAffected()
	return n > 0
}

func isoDay(ts int64) string { return time.Unix(ts, 0).UTC().Format("2006-01-02") }

func GetDashboard(rangeDays int64) Dashboard {
	to := int64(AnchorTS)
	from := to - rangeDays*86400
	d := Dashboard{RangeDays: rangeDays, Daily: make([]DayPoint, rangeDays), Countries: []CountryRow{}, TopItems: []TopItem{}}
	var buyers int64
	must(db.QueryRow("WITH r(lo, hi) AS (SELECT ? / 86400, ? / 86400) SELECT (SELECT COALESCE(sum(events), 0) FROM ev_day WHERE day >= lo AND day < hi), (SELECT count(*) FROM ev_user WHERE last_day >= lo), (SELECT COALESCE(sum(revenue_cents), 0) FROM ev_day WHERE day >= lo AND day < hi), (SELECT count(*) FROM ev_user WHERE last_purchase_day >= lo) FROM r", from, to).
		Scan(&d.Kpis.Events, &d.Kpis.Users, &d.Kpis.RevenueCents, &buyers))
	if d.Kpis.Users > 0 {
		d.Kpis.Conversion = float64(buyers) / float64(d.Kpis.Users)
	}
	for i := range d.Daily {
		d.Daily[i].Day = isoDay(from + int64(i)*86400)
	}
	rows, err := db.Query("SELECT day - ? / 86400 AS di, events, revenue_cents FROM ev_day WHERE day >= ? / 86400 AND day < ? / 86400", from, from, to)
	must(err)
	for rows.Next() {
		var di, n, rev int64
		must(rows.Scan(&di, &n, &rev))
		if di >= 0 && di < rangeDays {
			d.Daily[di].Events, d.Daily[di].RevenueCents = n, rev
		}
	}
	rows.Close()
	rows, err = db.Query("SELECT country, sum(events) n FROM ev_country_day WHERE day >= ? / 86400 AND day < ? / 86400 GROUP BY country ORDER BY n DESC, country LIMIT 8", from, to)
	must(err)
	for rows.Next() {
		var c CountryRow
		must(rows.Scan(&c.Country, &c.Events))
		d.Countries = append(d.Countries, c)
	}
	rows.Close()
	rows, err = db.Query("SELECT item, sum(revenue_cents) r, sum(purchases) FROM ev_item_day WHERE day >= ? / 86400 AND day < ? / 86400 GROUP BY item ORDER BY r DESC, item LIMIT 10", from, to)
	must(err)
	for rows.Next() {
		var it TopItem
		must(rows.Scan(&it.Item, &it.RevenueCents, &it.Purchases))
		d.TopItems = append(d.TopItems, it)
	}
	rows.Close()
	return d
}

func must(err error) {
	if err != nil {
		panic(err)
	}
}
