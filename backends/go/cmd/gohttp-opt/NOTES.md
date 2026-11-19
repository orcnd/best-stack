# gohttp-opt (run 3)

`cmd/gohttp` with only the rendering layer replaced. Server setup (ServeMux patterns, the shared
`POST /{fe}/kanban/{a}/{b}` route, static dispatch, GOMAXPROCS = --threads, DB pool = 2 × threads) is a
copy of `cmd/gohttp/main.go`; the handlers come from `corefast` instead of `core`.

- HTML: [templ](https://github.com/a-h/templ) v0.3.1118 components (`corefast/*.templ`) over typed
  structs (core's DB structs + small view models) instead of `html/template` over `map[string]any`.
- JSON: [goccy/go-json](https://github.com/goccy/go-json) v0.12.0 for responses and request bodies.
- SQL, validation, sessions, formatting and the chart SVG are `core`'s code (exported aliases in
  `core/export.go`); every request still queries SQLite.

Regenerate after editing a `.templ` file (generated `*_templ.go` files are committed):
`cd backends/go/corefast && go run github.com/a-h/templ/cmd/templ@v0.3.1118 generate`.

Output differs from `gohttp` only in whitespace between tags (templ drops source indentation), and
`+` / NUL in text are not entity-encoded (html/template writes `&#43;`); the parity data has neither.
