// Fiber v2 backend (fasthttp). GOMAXPROCS = --threads; routes registered natively with :params.
package main

import (
	"fmt"
	"runtime"
	"strings"

	"bestack/go/core"
	"github.com/gofiber/fiber/v2"
	"github.com/gofiber/fiber/v2/middleware/recover"
)

func toFiber(pattern string) (string, []string) {
	segs := strings.Split(pattern, "/")
	names := []string{}
	for i, s := range segs {
		if strings.HasPrefix(s, "{") {
			n := strings.Trim(s, "{}")
			names = append(names, n)
			segs[i] = ":" + n
		}
	}
	return strings.Join(segs, "/"), names
}

func toReq(c *fiber.Ctx, names []string) *core.Req {
	// fasthttp reuses buffers: copy everything that outlives the handler.
	q := &core.Req{Method: strings.Clone(c.Method()), Path: strings.Clone(c.Path()), Params: map[string]string{}, Headers: map[string]string{},
		Body: append([]byte(nil), c.Body()...)}
	for _, n := range names {
		q.Params[n] = strings.Clone(c.Params(n))
	}
	c.Request().Header.VisitAll(func(k, v []byte) { q.Headers[strings.ToLower(string(k))] = string(v) })
	q.Finish(string(c.Request().URI().QueryString()))
	return q
}

func write(c *fiber.Ctx, res core.Resp) error {
	c.Status(res.Status)
	c.Set("Content-Type", res.ContentType)
	for _, kv := range res.Headers {
		c.Response().Header.Add(kv[0], kv[1])
	}
	return c.Send(res.Body)
}

func newApp() *fiber.App {
	app := fiber.New(fiber.Config{DisableStartupMessage: true})
	app.Use(recover.New()) // a core panic (e.g. DB error) becomes a 500 instead of killing the process
	for _, rt := range core.Routes() {
		fn := rt.Handler
		path, names := toFiber(rt.Pattern)
		app.Add(rt.Method, path, func(c *fiber.Ctx) error { return write(c, fn(toReq(c, names))) })
	}
	app.Get("/static/*", func(c *fiber.Ctx) error {
		if res, ok := core.ServeStatic(strings.Clone(c.Params("*"))); ok {
			return write(c, res)
		}
		return write(c, core.Resp{Status: 404, ContentType: "text/plain", Body: []byte("not found")})
	})
	app.Use(func(c *fiber.Ctx) error {
		return write(c, core.Resp{Status: 404, ContentType: "text/plain", Body: []byte("not found")})
	})
	return app
}

func main() {
	cli := core.ParseCli()
	runtime.GOMAXPROCS(cli.Threads)
	core.Init(cli)
	app := newApp()
	fmt.Printf("bs-fiber listening on :%d (GOMAXPROCS=%d)\n", cli.Port, cli.Threads)
	if err := app.Listen(fmt.Sprintf("0.0.0.0:%d", cli.Port)); err != nil {
		panic(err)
	}
}
