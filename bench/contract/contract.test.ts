// Contract every C++ backend must satisfy. Run against a live server:
//   BASE=http://localhost:18080 bun test bench/contract
// The DB must be a fresh copy of data/pristine.sqlite.
import { describe, expect, test } from "bun:test";

const BASE = process.env.BASE ?? "http://localhost:18080";
const FES = ["htmx", "alpine", "vanilla"] as const;

const get = (p: string, init?: RequestInit) => fetch(BASE + p, { redirect: "manual", ...init });
const post = (p: string, body: string, headers: Record<string, string> = {}) =>
  fetch(BASE + p, {
    method: "POST",
    redirect: "manual",
    body,
    headers: { "content-type": "application/x-www-form-urlencoded", ...headers },
  });
const jsonReq = (method: string, p: string, body?: unknown, headers: Record<string, string> = {}) =>
  fetch(BASE + p, {
    method,
    body: body === undefined ? undefined : JSON.stringify(body),
    headers: { "content-type": "application/json", ...headers },
  });
const count = (s: string, needle: string) => s.split(needle).length - 1;
const ssr = (s: string) => {
  const i = s.indexOf('<template x-if="ready">');
  return i < 0 ? s : s.slice(0, i);
};

describe("health and static", () => {
  test("healthz", async () => {
    const r = await get("/healthz");
    expect(r.status).toBe(200);
    expect(await r.text()).toBe("ok");
  });
  test("static mime types", async () => {
    expect((await get("/static/app.css")).headers.get("content-type")).toStartWith("text/css");
    expect((await get("/static/vendor/htmx.min.js")).headers.get("content-type")).toStartWith("text/javascript");
    expect((await get("/static/img/cat-1.svg")).headers.get("content-type")).toStartWith("image/svg+xml");
    expect((await get("/static/nope.css")).status).toBe(404);
  });
  test("static traversal blocked", async () => {
    expect((await get("/static/..%2f..%2fshared%2fschema.sql")).status).toBe(404);
  });
});

describe("html pages", () => {
  for (const fe of FES) {
    test(`${fe}: catalog`, async () => {
      const r = await get(`/${fe}/catalog`);
      expect(r.status).toBe(200);
      expect(r.headers.get("content-type")).toStartWith("text/html");
      const html = ssr(await r.text());
      expect(count(html, 'data-testid="product-card"')).toBe(24);
      expect(html).toContain("<title>Ürün Kataloğu · Best Stack</title>");
    });
    test(`${fe}: detail, cart, kanban, dashboard`, async () => {
      expect(await (await get(`/${fe}/catalog/p/1`)).text()).toContain('data-testid="product-detail"');
      expect((await get(`/${fe}/catalog/cart`)).status).toBe(200);
      const k = ssr(await (await get(`/${fe}/kanban/1`)).text());
      expect(count(k, 'data-testid="card"')).toBe(100);
      const d = await (await get(`/${fe}/dashboard?range=7`)).text();
      expect(d).toContain('<svg class="chart-svg"');
    });
  }
  test("htmx fragment on hx-request", async () => {
    const r = await get("/htmx/catalog?q=lamp", { headers: { "hx-request": "true" } });
    const html = await r.text();
    expect(html).not.toContain("<html");
    expect(html).toContain('data-testid="product-grid"');
  });
});

describe("forms", () => {
  test("cart form: 303 + cookie, then badge", async () => {
    const r = await post("/vanilla/catalog/cart", "product_id=3&qty=1", { referer: `${BASE}/vanilla/catalog?page=2` });
    expect(r.status).toBe(303);
    expect(r.headers.get("location")).toBe("/vanilla/catalog?page=2");
    const cookie = r.headers.get("set-cookie") ?? "";
    expect(cookie).toStartWith("bs_sid=");
    const sid = cookie.slice(0, 23);
    const cart = await (await get("/vanilla/catalog/cart", { headers: { cookie: sid } })).text();
    expect(count(cart, 'data-testid="cart-row"')).toBe(1);
  });
  test("kanban form flow", async () => {
    const r = await post("/htmx/kanban/2/cards", "title=Contract+card&column=backlog");
    expect(r.status).toBe(303);
    expect(r.headers.get("location")).toBe("/htmx/kanban/2");
    const b = await (await get("/api/kanban/boards/2")).json();
    const id = b.columns.backlog.at(-1).id;
    expect((await post(`/htmx/kanban/cards/${id}`, "delete=1")).status).toBe(303);
  });
});

describe("json api", () => {
  test("products", async () => {
    const r = await get("/api/catalog/products?q=lamp&sort=price_asc");
    expect(r.headers.get("content-type")).toStartWith("application/json");
    const j = await r.json();
    for (const k of ["items", "total", "page", "pages", "categories"]) expect(j).toHaveProperty(k);
    expect(j.categories.length).toBe(12);
    expect((await (await get("/api/catalog/products/1")).json()).id).toBe(1);
  });
  test("cart api with x-session-id", async () => {
    const h = { "x-session-id": "contractsession1" };
    const add = await jsonReq("POST", "/api/catalog/cart", { product_id: 7, qty: 2 }, h);
    expect(add.status).toBe(201);
    const cart = await add.json();
    expect(cart.count).toBe(2);
    const item = cart.items[0].id;
    expect((await (await jsonReq("PATCH", `/api/catalog/cart/${item}`, { qty: 5 }, h)).json()).count).toBe(5);
    expect((await (await jsonReq("DELETE", `/api/catalog/cart/${item}`, undefined, h)).json()).count).toBe(0);
  });
  test("kanban api crud", async () => {
    expect((await (await get("/api/kanban/boards")).json()).length).toBe(5);
    const c = await jsonReq("POST", "/api/kanban/boards/3/cards", { title: "api", column: "todo" });
    expect(c.status).toBe(201);
    const card = await c.json();
    const moved = await (await jsonReq("PATCH", `/api/kanban/cards/${card.id}`, { move: "right" })).json();
    expect(moved.column).toBe("doing");
    expect((await jsonReq("DELETE", `/api/kanban/cards/${card.id}`)).status).toBe(204);
  });
  test("dashboard", async () => {
    const d = await (await get("/api/dashboard?range=30")).json();
    expect(d.daily.length).toBe(30);
    expect(d.top_items.length).toBe(10);
  });
});

describe("invalid params", () => {
  test("404s and 400s, never 500", async () => {
    expect((await get("/vanilla/catalog/p/abc")).status).toBe(404);
    expect((await get("/vanilla/catalog/p/999999")).status).toBe(404);
    expect((await get("/xyz/catalog")).status).toBe(404);
    expect((await get("/api/dashboard?range=13")).status).toBe(400);
    expect((await get("/api/catalog/products/abc")).status).toBe(404);
    expect((await jsonReq("POST", "/api/catalog/cart", "nope")).status).toBe(400);
    const p = await (await get("/vanilla/catalog?page=9999")).text();
    expect(p).toContain("Sayfa 84 / 84");
  });
  test("title validation and escaping", async () => {
    const long = "a".repeat(201);
    expect((await jsonReq("POST", "/api/kanban/boards/1/cards", { title: long })).status).toBe(422);
    expect((await post("/vanilla/kanban/1/cards", `title=${long}&column=backlog`)).status).toBe(422);
    const q = await (await get("/vanilla/catalog?q=%3Cscript%3Ex%3C%2Fscript%3E")).text();
    expect(q).toContain("&lt;script&gt;");
    expect(q).not.toContain("<script>x");
  });
});

describe("no app-level compression (run-2 fairness rule)", () => {
  test("pages, api and static are sent uncompressed even when the client accepts compression", async () => {
    const h = { "accept-encoding": "gzip, deflate, br, zstd" };
    for (const p of ["/vanilla/kanban/1", "/htmx/catalog", "/api/catalog/products", "/static/app.css", "/static/vendor/htmx.min.js"]) {
      const r = await get(p, { headers: h });
      expect(`${p}: ${r.headers.get("content-encoding")}`).toBe(`${p}: null`);
    }
  });
});

describe("concurrency smoke", () => {
  test("64 parallel GETs all 200", async () => {
    const rs = await Promise.all(Array.from({ length: 64 }, () => get("/api/catalog/products?page=3")));
    expect(rs.every((r) => r.status === 200)).toBe(true);
  });
});
