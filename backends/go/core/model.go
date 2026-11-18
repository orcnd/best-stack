package core

const (
	AnchorTS = 1793491200 // 2026-11-01T00:00:00Z
	PerPage  = 24
	MaxTitle = 200
)

var Columns = []string{"backlog", "todo", "doing", "done"}

type Category struct {
	ID   int64  `json:"id"`
	Name string `json:"name"`
	Slug string `json:"slug"`
}

type Product struct {
	ID          int64   `json:"id"`
	CategoryID  int64   `json:"category_id"`
	Category    string  `json:"category"`
	Name        string  `json:"name"`
	Description string  `json:"description"`
	Image       string  `json:"image"`
	PriceCents  int64   `json:"price_cents"`
	Stock       int64   `json:"stock"`
	Rating      float64 `json:"rating"`
}

type ProductQuery struct {
	Q, Cat, Sort string
	Page         int64
}

type ProductPage struct {
	Items []Product `json:"items"`
	Total int64     `json:"total"`
	Page  int64     `json:"page"`
	Pages int64     `json:"pages"`
}

type CartItem struct {
	ID      int64   `json:"id"`
	Qty     int64   `json:"qty"`
	Product Product `json:"product"`
}

type Cart struct {
	Items      []CartItem `json:"items"`
	Count      int64      `json:"count"`
	TotalCents int64      `json:"total_cents"`
}

type Card struct {
	ID          int64    `json:"id"`
	BoardID     int64    `json:"board_id"`
	Position    int64    `json:"position"`
	Column      string   `json:"column"`
	Title       string   `json:"title"`
	Description string   `json:"description"`
	Assignee    string   `json:"assignee"`
	DueDate     string   `json:"due_date"`
	Labels      []string `json:"labels"`
}

type BoardSummary struct {
	ID        int64  `json:"id"`
	Name      string `json:"name"`
	CardCount int64  `json:"card_count"`
}

type Board struct {
	ID      int64             `json:"id"`
	Name    string            `json:"name"`
	Columns map[string][]Card `json:"columns"`
}

type DayPoint struct {
	Day          string `json:"day"`
	Events       int64  `json:"events"`
	RevenueCents int64  `json:"revenue_cents"`
}

type Kpis struct {
	Events       int64   `json:"events"`
	Users        int64   `json:"users"`
	RevenueCents int64   `json:"revenue_cents"`
	Conversion   float64 `json:"conversion"`
}

type CountryRow struct {
	Country string `json:"country"`
	Events  int64  `json:"events"`
}

type TopItem struct {
	Item         string `json:"item"`
	RevenueCents int64  `json:"revenue_cents"`
	Purchases    int64  `json:"purchases"`
}

type Dashboard struct {
	RangeDays int64        `json:"range_days"`
	Kpis      Kpis         `json:"kpis"`
	Daily     []DayPoint   `json:"daily"`
	Countries []CountryRow `json:"countries"`
	TopItems  []TopItem    `json:"top_items"`
}
