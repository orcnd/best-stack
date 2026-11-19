// Output equivalence between two backends: same requests, same fresh DB, compare every response.
//   node bench/parity/golden.ts <reference-backend> <candidate-backend> [--port 18200]
// HTML must match after whitespace normalization (exact matches are reported too); JSON must be
// deep-equal (key order and number spelling may differ). Exits non-zero on any mismatch.
import { type ChildProcess, spawn } from "node:child_process";
import { copyFileSync, rmSync } from "node:fs";
import { isDeepStrictEqual } from "node:util";
import { join } from "node:path";

const ROOT = join(import.meta.dirname, "..", "..");
const argv = process.argv.slice(2);
const [refName, candName] = argv.filter((a, i) => !a.startsWith("--") && !argv[i - 1]?.startsWith("--"));
const basePort = Number(argv[argv.indexOf("--port") + 1] ?? 0) || 18200;
if (!refName || !candName) { console.error("usage: golden.ts <reference> <candidate> [--port N]"); process.exit(2); }

const FES = ["htmx", "alpine", "vanilla"];
const PAGE_PATHS = [
  "/catalog", "/catalog?page=2", "/catalog?page=9999", "/catalog?q=lamp", "/catalog?q=lamp&sort=price_asc", "/catalog?category=mobilya",
  "/catalog?sort=price_desc&page=3", "/catalog?q=%3Cscript%3Ex%3C%2Fscript%3E", "/catalog/p/1", "/catalog/p/17", "/catalog/p/999999", "/catalog/p/abc",
  "/catalog/cart", "/kanban/1", "/kanban/2", "/kanban/4", "/kanban/999", "/dashboard", "/dashboard?range=7", "/dashboard?range=90", "/dashboard?range=13",
];
const API_PATHS = [
  "/api/catalog/products", "/api/catalog/products?page=3", "/api/catalog/products?q=lamp&sort=price_asc", "/api/catalog/products?category=mobilya",
  "/api/catalog/products/1", "/api/catalog/products/abc", "/api/catalog/products/999999", "/api/catalog/cart", "/api/kanban/boards",
  "/api/kanban/boards/1", "/api/kanban/boards/2", "/api/kanban/boards/999", "/api/dashboard?range=7", "/api/dashboard?range=30", "/api/dashboard?range=90", "/api/dashboard?range=13",
];

interface Req { method: string; path: string; headers?: Record<string, string>; body?: string }
const requests: Req[] = [
  ...FES.flatMap((fe) => PAGE_PATHS.flatMap((p) => [{ method: "GET", path: `/${fe}${p}` }, { method: "GET", path: `/${fe}${p}`, headers: { "hx-request": "true" } }])),
  ...API_PATHS.map((path) => ({ method: "GET", path })),
  // Mutations run in the same order on both servers; the follow-up GETs compare the resulting state.
  { method: "POST", path: "/api/catalog/cart", headers: { "content-type": "application/json" }, body: JSON.stringify({ product_id: 3, qty: 2 }) },
  { method: "GET", path: "/api/catalog/cart" },
  { method: "POST", path: "/htmx/catalog/cart", headers: { "content-type": "application/x-www-form-urlencoded", "hx-request": "true" }, body: "product_id=5&qty=1" },
  { method: "GET", path: "/htmx/catalog/cart" },
  { method: "GET", path: "/vanilla/catalog/cart", headers: { "hx-request": "true" } },
  { method: "POST", path: "/api/kanban/boards/1/cards", headers: { "content-type": "application/json" }, body: JSON.stringify({ title: "Golden <b>&</b> kart", column: "todo" }) },
  { method: "POST", path: "/htmx/kanban/1/cards", headers: { "content-type": "application/x-www-form-urlencoded", "hx-request": "true" }, body: "title=Form+kart+%C3%A7%C4%9F&column=doing" },
  { method: "GET", path: "/htmx/kanban/1" },
  { method: "GET", path: "/alpine/kanban/1", headers: { "hx-request": "true" } },
  { method: "GET", path: "/api/kanban/boards/1" },
];

async function start(name: string, port: number): Promise<ChildProcess> {
  const db = join(ROOT, "data", `golden-${port}.sqlite`);
  for (const s of ["", "-wal", "-shm"]) rmSync(db + s, { force: true });
  copyFileSync(join(ROOT, "data", "pristine.sqlite"), db);
  const bin = join(ROOT, "build", "backends", name, `bs-${name}`);
  const proc = spawn(bin, ["--port", String(port), "--db", db, "--root", ROOT, "--threads", "2"], { stdio: "ignore" });
  for (let i = 0; i < 100; i++) {
    try { if ((await fetch(`http://127.0.0.1:${port}/healthz`)).ok) return proc; } catch { /* not up yet */ }
    await new Promise((r) => setTimeout(r, 100));
  }
  proc.kill();
  throw new Error(`${name} did not start on ${port}`);
}

interface Res { status: number; type: string; location: string; body: string }
async function send(port: number, r: Req, cookies: Map<string, string>): Promise<Res> {
  const headers: Record<string, string> = { ...r.headers };
  if (cookies.size) headers.cookie = [...cookies].map(([k, v]) => `${k}=${v}`).join("; ");
  const res = await fetch(`http://127.0.0.1:${port}${r.path}`, { method: r.method, headers, body: r.body, redirect: "manual" });
  for (const c of res.headers.getSetCookie()) { const [kv] = c.split(";"); const i = kv.indexOf("="); cookies.set(kv.slice(0, i), kv.slice(i + 1)); }
  return { status: res.status, type: (res.headers.get("content-type") ?? "").split(";")[0].trim(), location: res.headers.get("location") ?? "", body: await res.text() };
}

const norm = (s: string) => s.replace(/>\s+</g, "><").replace(/\s+/g, " ").trim();
/** Session ids differ between servers by design; hide cookie values that appear in bodies. */
const scrub = (s: string, cookies: Map<string, string>) => [...cookies.values()].reduce((a, v) => (v.length > 6 ? a.split(v).join("<cookie>") : a), s);

function firstDiff(a: string, b: string): string {
  let i = 0;
  while (i < a.length && a[i] === b[i]) i++;
  return `@${i}\n    ref:  ${JSON.stringify(a.slice(Math.max(0, i - 60), i + 80))}\n    cand: ${JSON.stringify(b.slice(Math.max(0, i - 60), i + 80))}`;
}

async function main() {
  const [pa, pb] = [basePort, basePort + 1];
  const procs = [await start(refName, pa), await start(candName, pb)];
  const ca = new Map<string, string>(), cb = new Map<string, string>();
  let fail = 0, exact = 0;
  try {
    for (const r of requests) {
      const [a, b] = [await send(pa, r, ca), await send(pb, r, cb)];
      const label = `${r.method} ${r.path}${r.headers?.["hx-request"] ? " [hx]" : ""}`;
      const problems: string[] = [];
      if (a.status !== b.status) problems.push(`status ${a.status} ≠ ${b.status}`);
      if (a.type !== b.type) problems.push(`content-type ${a.type} ≠ ${b.type}`);
      if (a.location !== b.location) problems.push(`location ${a.location} ≠ ${b.location}`);
      const [ba, bb] = [scrub(a.body, ca), scrub(b.body, cb)];
      if (a.type === "application/json") {
        let ja: unknown, jb: unknown;
        try { ja = JSON.parse(ba); jb = JSON.parse(bb); } catch { problems.push("invalid JSON"); }
        if (!problems.length && !isDeepStrictEqual(ja, jb)) problems.push(`JSON differs ${firstDiff(JSON.stringify(ja), JSON.stringify(jb))}`);
      } else if (ba === bb) exact++;
      else if (norm(ba) !== norm(bb)) problems.push(`body differs ${firstDiff(norm(ba), norm(bb))}`);
      if (problems.length) { fail++; console.log(`✗ ${label}\n  ${problems.join("\n  ")}`); }
    }
  } finally {
    for (const p of procs) p.kill();
  }
  const html = requests.length - API_PATHS.length;
  console.log(`golden ${refName} → ${candName}: ${requests.length - fail}/${requests.length} equal (${exact} byte-identical non-JSON of ~${html})`);
  process.exit(fail ? 1 : 0);
}

await main();
