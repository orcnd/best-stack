package core

import (
	"embed"
	"flag"
	"html/template"
	"io"
	"mime"
	"net/url"
	"os"
	"path"
	"path/filepath"
	"strings"
	"sync"
)

// Req/Resp are framework-agnostic; net/http and Fiber adapters convert to/from them.
type Req struct {
	Method, Path string
	Params, Query, Form, Headers, Cookies map[string]string // Headers: lower-case keys
	Body                                  []byte
}

func (r *Req) Header(k string) string { return r.Headers[k] }

// Finish fills Query, Cookies and Form from raw parts (adapters set Method, Path, Params, Headers, Body first).
func (r *Req) Finish(rawQuery string) {
	r.Query = flatten(rawQuery)
	r.Cookies = map[string]string{}
	for _, part := range strings.Split(r.Header("cookie"), ";") {
		if k, v, ok := strings.Cut(strings.TrimSpace(part), "="); ok {
			r.Cookies[k] = v
		}
	}
	r.Form = map[string]string{}
	if strings.HasPrefix(r.Header("content-type"), "application/x-www-form-urlencoded") {
		r.Form = flatten(string(r.Body))
	}
}

func flatten(raw string) map[string]string {
	out := map[string]string{}
	v, _ := url.ParseQuery(raw)
	for k, xs := range v {
		if len(xs) > 0 {
			out[k] = xs[len(xs)-1]
		}
	}
	return out
}

type Resp struct {
	Status      int
	ContentType string
	Body        []byte
	Headers     [][2]string
}

type Cli struct {
	Port, Threads int
	DB, Root      string
}

func ParseCli() Cli {
	var c Cli
	flag.IntVar(&c.Port, "port", 18080, "")
	flag.IntVar(&c.Threads, "threads", 8, "")
	flag.StringVar(&c.DB, "db", "data/app.sqlite", "")
	flag.StringVar(&c.Root, "root", ".", "")
	flag.Parse()
	return c
}

//go:embed templates/*.tmpl
var templateFS embed.FS
var tmpl *template.Template
var staticDir string
var staticCache sync.Map // rel path → Resp

// Init must be called once before serving.
func Init(c Cli) {
	openDB(c.DB, c.Threads*2)
	tmpl = template.Must(template.ParseFS(templateFS, "templates/*.tmpl"))
	root, _ := filepath.Abs(c.Root)
	staticDir = filepath.Join(root, "shared", "static")
}

// ServeStatic answers GET /static/<rel>; ok=false when missing or escaping the static dir.
func ServeStatic(rel string) (Resp, bool) {
	if v, ok := staticCache.Load(rel); ok {
		return v.(Resp), true
	}
	if rel == "" || strings.Contains(rel, "..") || strings.HasPrefix(rel, "/") || path.Clean(rel) != rel {
		return Resp{}, false
	}
	f, err := os.Open(filepath.Join(staticDir, filepath.FromSlash(rel)))
	if err != nil {
		return Resp{}, false
	}
	defer f.Close()
	if st, _ := f.Stat(); st == nil || st.IsDir() {
		return Resp{}, false
	}
	body, _ := io.ReadAll(f)
	ct := mime.TypeByExtension(filepath.Ext(rel))
	switch filepath.Ext(rel) { // same MIME table as the C++ core
	case ".css":
		ct = "text/css; charset=utf-8"
	case ".js", ".mjs":
		ct = "text/javascript; charset=utf-8"
	case ".svg":
		ct = "image/svg+xml"
	case ".wasm":
		ct = "application/wasm"
	}
	if ct == "" {
		ct = "application/octet-stream"
	}
	r := Resp{Status: 200, ContentType: ct, Body: body, Headers: [][2]string{{"Cache-Control", "public, max-age=31536000"}}}
	staticCache.Store(rel, r)
	return r, true
}
