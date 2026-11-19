// net/http backend, run-3 fast path: identical server setup to cmd/gohttp, handlers from corefast (templ + goccy/go-json).
// one goroutine per connection, SQLite through database/sql's pool.
package main

import (
	"fmt"
	"io"
	"net/http"
	"runtime"
	"strings"

	"bestack/go/core"
	"bestack/go/corefast"
)

func names(pattern string) []string {
	out := []string{}
	for _, seg := range strings.Split(pattern, "/") {
		if strings.HasPrefix(seg, "{") {
			out = append(out, strings.Trim(seg, "{}"))
		}
	}
	return out
}

func toReq(r *http.Request, params []string) *core.Req {
	body, _ := io.ReadAll(r.Body)
	q := &core.Req{Method: r.Method, Path: r.URL.Path, Params: map[string]string{}, Headers: map[string]string{}, Body: body}
	for _, n := range params {
		q.Params[n] = r.PathValue(n)
	}
	for k, v := range r.Header {
		q.Headers[strings.ToLower(k)] = strings.Join(v, ", ")
	}
	q.Finish(r.URL.RawQuery)
	return q
}

func write(w http.ResponseWriter, res core.Resp) {
	h := w.Header()
	h.Set("Content-Type", res.ContentType)
	for _, kv := range res.Headers {
		h.Add(kv[0], kv[1])
	}
	w.WriteHeader(res.Status)
	_, _ = w.Write(res.Body)
}

func handle(fn core.Handler, params []string) http.HandlerFunc {
	return func(w http.ResponseWriter, r *http.Request) { write(w, fn(toReq(r, params))) }
}

func main() {
	cli := core.ParseCli()
	runtime.GOMAXPROCS(cli.Threads)
	core.InitData(cli)
	mux := http.NewServeMux()
	byPattern := map[string]core.Handler{}
	for _, rt := range corefast.Routes() {
		byPattern[rt.Method+" "+rt.Pattern] = rt.Handler
	}
	// ServeMux rejects these two as conflicting ({fe}/kanban/cards/{id} vs {fe}/kanban/{board_id}/cards),
	// so they share one pattern and are told apart by the literal segment.
	update, create := byPattern["POST /{fe}/kanban/cards/{id}"], byPattern["POST /{fe}/kanban/{board_id}/cards"]
	delete(byPattern, "POST /{fe}/kanban/cards/{id}")
	delete(byPattern, "POST /{fe}/kanban/{board_id}/cards")
	mux.HandleFunc("POST /{fe}/kanban/{a}/{b}", func(w http.ResponseWriter, r *http.Request) {
		q := toReq(r, []string{"fe"})
		a, b := r.PathValue("a"), r.PathValue("b")
		switch {
		case a == "cards":
			q.Params["id"] = b
			write(w, update(q))
		case b == "cards":
			q.Params["board_id"] = a
			write(w, create(q))
		default:
			http.NotFound(w, r)
		}
	})
	for key, fn := range byPattern {
		mux.HandleFunc(key, handle(fn, names(strings.SplitN(key, " ", 2)[1])))
	}
	// /static/ is dispatched before the mux: ServeMux also rejects "GET /static/{rel...}" next to "GET /{fe}/catalog".
	root := http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		if rel, ok := strings.CutPrefix(r.URL.Path, "/static/"); ok && r.Method == http.MethodGet {
			if res, ok := core.ServeStatic(rel); ok {
				write(w, res)
			} else {
				http.NotFound(w, r)
			}
			return
		}
		mux.ServeHTTP(w, r)
	})
	fmt.Printf("bs-gohttp-opt listening on :%d (GOMAXPROCS=%d)\n", cli.Port, cli.Threads)
	if err := http.ListenAndServe(fmt.Sprintf("0.0.0.0:%d", cli.Port), root); err != nil {
		panic(err)
	}
}
