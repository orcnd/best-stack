#!/usr/bin/env python3
"""Run-3 tables (server-only): python3 scripts/run3_report.py <results.sqlite> [more.sqlite ...] > tables.md

With several result files (repeat passes) each load-test cell shows the best pass and, in brackets, the lowest.
Best-of assumes interference only slows a run down; it is not a confidence interval. Give every backend the same
number of passes, or the best-of favours the ones with more tries."""
import sqlite3
import sys
from statistics import mean

LANG = {"crow": "C++", "uwebsockets": "C++", "axum": "Rust", "actix": "Rust", "gohttp": "Go", "fiber": "Go", "httpzig": "Zig", "zap": "Zig"}
NAME = {"crow": "Crow", "uwebsockets": "uWebSockets", "axum": "Axum", "actix": "Actix-web", "gohttp": "net/http", "fiber": "Fiber", "httpzig": "http.zig", "zap": "zap"}
LIBS = {"C++": "glaze + mimalloc", "Rust": "askama + mimalloc", "Go": "templ + go-json", "Zig": "—"}
UCS = ["catalog", "kanban", "dashboard"]
UC_TR = {"catalog": "Katalog", "kanban": "Kanban", "dashboard": "Dashboard"}

dbs = []
for arg in sys.argv[1:]:
    path, _, only = arg.partition(":")  # results.sqlite[:usecase,usecase] limits a file to some use cases
    d = sqlite3.connect(f"file:{path}?immutable=1", uri=True)
    dbs.append((d, set(only.split(",")) if only else set(UCS), d.execute("select run_id from env order by started_at desc limit 1").fetchone()[0]))


def label(b):
    base = b.removesuffix("-opt")
    return NAME[base] + (" (opt)" if b.endswith("-opt") else "")


rows = {}
for db, only, run in dbs:
    for cid, b, u in db.execute("select combo_id, backend, usecase from combos"):
        lt = {(t, c): (rps, p50, p95, err, tot) for t, c, rps, p50, p95, err, tot in db.execute(
            "select target, concurrency, rps, p50, p95, errors, total from load_tests where run_id=? and combo_id=?", (run, cid))}
        if not lt or u not in only:
            continue
        mem = db.execute("select max(footprint_kb) from resources where run_id=? and combo_id=? and process_role='backend' and phase like 'load_test%'", (run, cid)).fetchone()[0]
        cpu = db.execute("select avg(cpu_pct) from resources where run_id=? and combo_id=? and process_role='backend' and phase='load_test_page'", (run, cid)).fetchone()[0]
        par = db.execute("select parity_ok from parity where run_id=? and combo_id=?", (run, cid)).fetchone()
        new = dict(lo={k: v[0] for k, v in lt.items()}, lt=lt, mem_mb=(mem or 0) / 1024, cpu=cpu or 0, parity=par[0] if par else None)
        cur = rows.get((b, u))
        if cur is None:
            rows[(b, u)] = new
            continue
        for k, v in new["lt"].items():
            cur["lo"][k] = min(cur["lo"].get(k, v[0]), v[0])
            if k not in cur["lt"] or v[0] > cur["lt"][k][0]:
                cur["lt"][k] = v
                if k == ("page", 64):
                    cur["mem_mb"], cur["cpu"] = new["mem_mb"], new["cpu"]
        cur["parity"] = min(x for x in (cur["parity"], new["parity"]) if x is not None) if new["parity"] is not None else cur["parity"]

backends = sorted({b for b, _ in rows})
g = lambda b, u, t, c, i: rows[(b, u)]["lt"].get((t, c), (None,) * 5)[i] if (b, u) in rows else None
f0 = lambda v: "—" if v is None else f"{v:,.0f}".replace(",", ".")
f1 = lambda v: "—" if v is None else f"{v:.1f}".replace(".", ",")
f2 = lambda v: "—" if v is None else f"{v:.2f}".replace(".", ",")

print("Run: " + ", ".join(f"`{r}`" for _, _, r in dbs) + (" (her hücre: turların en iyisi)" if len(dbs) > 1 else "") + "\n")
def cell(b, u, t):
    best = g(b, u, t, 64, 0)
    return "—" if best is None else f0(best) + (f" ({f0(rows[(b, u)]['lo'][(t, 64)])})" if len(dbs) > 1 else "")


print("## Sayfa (HTML) req/s, 64 eşzamanlı istemci" + (": en iyi tur (en düşük tur)" if len(dbs) > 1 else "") + "\n")
print("| Backend | Dil | " + " | ".join(UC_TR[u] for u in UCS) + " | Katalog API |")
print("|---|---|" + "---|" * (len(UCS) + 1))
for b in sorted(backends, key=lambda b: -(g(b, "catalog", "page", 64, 0) or 0)):
    print(f"| {label(b)} | {LANG[b.removesuffix('-opt')]} | " + " | ".join(cell(b, u, "page") for u in UCS) + f" | {cell(b, 'catalog', 'api')} |")

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
print("| Backend | p50 @1 ms | p95 @64 ms | Bellek MB (yük altında, tepe) | CPU % (ortalama) | İstek başına CPU-µs |")
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
