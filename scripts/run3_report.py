#!/usr/bin/env python3
"""Run-3 tables (server-only): python3 scripts/run3_report.py <results.sqlite> > tables.md"""
import sqlite3
import sys
from statistics import mean

LANG = {"crow": "C++", "uwebsockets": "C++", "axum": "Rust", "actix": "Rust", "gohttp": "Go", "fiber": "Go", "httpzig": "Zig", "zap": "Zig"}
NAME = {"crow": "Crow", "uwebsockets": "uWebSockets", "axum": "Axum", "actix": "Actix-web", "gohttp": "net/http", "fiber": "Fiber", "httpzig": "http.zig", "zap": "zap"}
LIBS = {"C++": "glaze + mimalloc", "Rust": "askama + mimalloc", "Go": "templ + go-json", "Zig": "—"}
UCS = ["catalog", "kanban", "dashboard"]
UC_TR = {"catalog": "Katalog", "kanban": "Kanban", "dashboard": "Dashboard"}

db = sqlite3.connect(sys.argv[1])
run = db.execute("select run_id from env order by started_at desc limit 1").fetchone()[0]


def label(b):
    base = b.removesuffix("-opt")
    return NAME[base] + (" (opt)" if b.endswith("-opt") else "")


rows = {}
for cid, b, u in db.execute("select combo_id, backend, usecase from combos"):
    lt = {(t, c): (rps, p50, p95, err, tot) for t, c, rps, p50, p95, err, tot in db.execute(
        "select target, concurrency, rps, p50, p95, errors, total from load_tests where run_id=? and combo_id=?", (run, cid))}
    if not lt:
        continue
    mem = db.execute("select max(footprint_kb) from resources where run_id=? and combo_id=? and process_role='backend' and phase like 'load_test%'", (run, cid)).fetchone()[0]
    cpu = db.execute("select avg(cpu_pct) from resources where run_id=? and combo_id=? and process_role='backend' and phase='load_test_page'", (run, cid)).fetchone()[0]
    par = db.execute("select parity_ok from parity where run_id=? and combo_id=?", (run, cid)).fetchone()
    rows[(b, u)] = dict(lt=lt, mem_mb=(mem or 0) / 1024, cpu=cpu or 0, parity=par[0] if par else None)

backends = sorted({b for b, _ in rows})
g = lambda b, u, t, c, i: rows[(b, u)]["lt"].get((t, c), (None,) * 5)[i] if (b, u) in rows else None
f0 = lambda v: "—" if v is None else f"{v:,.0f}".replace(",", ".")
f1 = lambda v: "—" if v is None else f"{v:.1f}".replace(".", ",")
f2 = lambda v: "—" if v is None else f"{v:.2f}".replace(".", ",")

print(f"Run `{run}`\n")
print("## Sayfa (HTML) req/s, 64 eşzamanlı istemci\n")
print("| Backend | Dil | " + " | ".join(UC_TR[u] for u in UCS) + " | Katalog API | HTML/API |")
print("|---|---|" + "---|" * (len(UCS) + 2))
for b in sorted(backends, key=lambda b: -(g(b, "catalog", "page", 64, 0) or 0)):
    page, api = g(b, "catalog", "page", 64, 0), g(b, "catalog", "api", 64, 0)
    print(f"| {label(b)} | {LANG[b.removesuffix('-opt')]} | " + " | ".join(f0(g(b, u, "page", 64, 0)) for u in UCS) + f" | {f0(api)} | {'%' + f0(100 * page / api) if page and api else '—'} |")

print("\n## Optimizasyon kazancı (opt / orijinal, sayfa req/s @64)\n")
print("| Framework | Kütüphaneler | " + " | ".join(UC_TR[u] for u in UCS) + " | Katalog API |")
print("|---|---|" + "---|" * (len(UCS) + 1))
for b in [x for x in backends if x.endswith("-opt")]:
    o = b.removesuffix("-opt")
    cells = []
    for u in UCS:
        a, c = g(o, u, "page", 64, 0), g(b, u, "page", 64, 0)
        cells.append(f"{f2(c / a)}×" if a and c else "—")
    a, c = g(o, "catalog", "api", 64, 0), g(b, "catalog", "api", 64, 0)
    print(f"| {NAME[o]} | {LIBS[LANG[o]]} | " + " | ".join(cells) + f" | {f2(c / a) + '×' if a and c else '—'} |")

print("\n## Gecikme ve kaynak (Katalog sayfası)\n")
print("| Backend | p50 @1 ms | p95 @64 ms | Bellek MB (yük altında, tepe) | CPU % (ortalama) | req/s başına CPU-µs |")
print("|---|---|---|---|---|---|")
for b in sorted([b for b in backends if (b, "catalog") in rows], key=lambda b: g(b, "catalog", "page", 1, 1) or 9e9):
    r = rows[(b, "catalog")]
    rps = g(b, "catalog", "page", 64, 0)
    print(f"| {label(b)} | {f2(g(b, 'catalog', 'page', 1, 1))} | {f2(g(b, 'catalog', 'page', 64, 2))} | {f1(r['mem_mb'])} | {f0(r['cpu'])} | {f1(r['cpu'] / 100 / rps * 1e6) if rps else '—'} |")

print("\n## Her dilin en hızlı iki yolu (üç senaryonun sayfa req/s @64 geometrik ortalaması)\n")
import math
gm = {}
for b in backends:
    vals = [g(b, u, "page", 64, 0) for u in UCS]
    if all(vals):
        gm[b] = math.exp(mean(math.log(v) for v in vals))
print("| Dil | 1. | 2. |")
print("|---|---|---|")
for L in ["Zig", "C++", "Rust", "Go"]:
    top = sorted([b for b in gm if LANG[b.removesuffix("-opt")] == L], key=lambda b: -gm[b])[:2]
    print(f"| {L} | " + " | ".join(f"{label(b)} — {f0(gm[b])}" for b in top) + " |")
print("\n| Sıra | Backend | Geo. ort. sayfa req/s |")
print("|---|---|---|")
for i, b in enumerate(sorted(gm, key=lambda b: -gm[b]), 1):
    print(f"| {i} | {label(b)} | {f0(gm[b])} |")

print("\n## Hatalar ve eşdeğerlik\n")
bad = [(b, u, t, c, e, tot) for (b, u), r in rows.items() for (t, c), (rps, p50, p95, e, tot) in r["lt"].items() if e]
par = [(b, u) for (b, u), r in rows.items() if r["parity"] != 1]
print(f"- Yük testi hatası olan ölçümler: {len(bad)}" + "".join(f"\n  - {label(b)} {u} {t} c{c}: {e}/{tot}" for b, u, t, c, e, tot in bad))
print(f"- Eşdeğerlik (fonksiyonel/görsel/SEO) başarısız kombinasyon: {len(par)} {par if par else ''}")
