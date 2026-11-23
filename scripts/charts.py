#!/usr/bin/env python3
"""Charts for run 3 / run 4: python3 scripts/charts.py   (reads docs/data, writes docs/charts/*.svg, stdlib only)

Each chart is written for light and dark. Both passes are always drawn: the repeat-to-repeat spread is part of the result.
The 4-combo recheck of run 3 is deliberately NOT used: it would give four backends a third try in a best-of.
"""
import math
import sqlite3
from pathlib import Path
from statistics import median

ROOT = Path(__file__).resolve().parent.parent
DATA, OUT = ROOT / "docs/data", ROOT / "docs/charts"
THEMES = {
    "light": dict(bg="#fcfcfb", ink="#0b0b0b", ink2="#52514e", grid="#e4e3df", band="#efeeea", s=["#2a78d6", "#eb6834", "#1baf7a"]),
    "dark": dict(bg="#1a1a19", ink="#f0efec", ink2="#c3c2b7", grid="#343432", band="#262624", s=["#3987e5", "#d95926", "#199e70"]),
}
FONT = "font-family=\"-apple-system,'Segoe UI',Helvetica,Arial,sans-serif\""
NAME = {"crow": "Crow", "uwebsockets": "uWebSockets", "axum": "Axum", "actix": "Actix-web", "gohttp": "net/http", "fiber": "Fiber", "httpzig": "http.zig", "zap": "zap"}
PAIRS = [("C++", ["crow", "uwebsockets"]), ("Rust", ["actix", "axum"]), ("Go", ["fiber", "gohttp"]), ("Zig", ["httpzig", "zap"])]


def load(name):
    d = sqlite3.connect(f"file:{DATA / name}?immutable=1", uri=True)
    run = d.execute("select run_id from env order by started_at desc limit 1").fetchone()[0]
    return {(b, u, t): r for b, u, t, r in d.execute(
        "select c.backend,c.usecase,l.target,l.rps from load_tests l join combos c using(combo_id) where l.run_id=? and l.concurrency=64", (run,))}


R3 = [load(f"results-run3-p{i}.sqlite") for i in (1, 2)]
R4A = [load(f"results-run4a-dashboard-p{i}.sqlite") for i in (1, 2)]
R4B = [load(f"results-run4b-dashboard-p{i}.sqlite") for i in (1, 2)]
BACKENDS = sorted({k[0] for k in R3[0]})


def fmt(v):
    return f"{v:,.0f}"


class Svg:
    def __init__(self, w, h, th, title, sub):
        self.t, self.o = th, [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}" role="img" aria-label="{title}" {FONT}>',
                              f'<rect width="{w}" height="{h}" rx="8" fill="{th["bg"]}"/>']
        self.text(24, 34, title, 18, weight=600)
        for i, line in enumerate(sub):
            self.text(24, 56 + 18 * i, line, 13, self.t["ink2"])

    def text(self, x, y, s, size=12, fill=None, anchor="start", weight=400):
        s = s.replace("&", "&amp;").replace("<", "&lt;")
        self.o.append(f'<text x="{x:.1f}" y="{y:.1f}" font-size="{size}" font-weight="{weight}" text-anchor="{anchor}" fill="{fill or self.t["ink"]}">{s}</text>')

    def line(self, x1, y1, x2, y2, c, w=1, dash=""):
        self.o.append(f'<line x1="{x1:.1f}" y1="{y1:.1f}" x2="{x2:.1f}" y2="{y2:.1f}" stroke="{c}" stroke-width="{w}"{f" stroke-dasharray=\"{dash}\"" if dash else ""}/>')

    def dot(self, x, y, c, hollow=False, r=5, op=1):
        bg = self.t["bg"]
        self.o.append(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="{r}" fill="{bg if hollow else c}" stroke="{c if hollow else bg}" stroke-width="2" opacity="{op}"/>')

    def rect(self, x, y, w, h, c):
        self.o.append(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w:.1f}" height="{h:.1f}" fill="{c}"/>')

    def save(self, name, mode):
        OUT.mkdir(exist_ok=True)
        (OUT / f"{name}-{mode}.svg").write_text("\n".join(self.o + ["</svg>"]) + "\n")


def logx(lo, hi, x0, x1):
    return lambda v: x0 + (math.log(v) - math.log(lo)) / (math.log(hi) - math.log(lo)) * (x1 - x0)


def throughput(mode, th):
    W, x0, gap, pw, row = 1080, 170, 34, 270, 22
    rows = []
    for lang, bs in PAIRS:
        rows.append((lang, None))
        for b in bs:
            rows += [(None, b)] + ([(None, b + "-opt")] if b + "-opt" in BACKENDS else [])
    top = 138
    H = top + row * len(rows) + 52
    s = Svg(W, H, th, "Page throughput at 64 concurrent clients, both passes", [
        "Requests per second, log scale. Filled = better pass, hollow = other pass; the line between them is how far a repeat moved.",
        "Catalog and Kanban: run 3. Dashboard: run 4b (new rollup tables). Load generator and server share one Apple M2."])
    col = lambda b: th["s"][2] if b in ("httpzig", "zap") else th["s"][1] if b.endswith("-opt") else th["s"][0]
    for i, (c, n) in enumerate(zip(th["s"], ["original", "optimized libraries", "Zig (unchanged)"])):
        s.dot(x0 + 6 + i * 170, 98, c)
        s.text(x0 + 18 + i * 170, 102, n, 12, th["ink2"])
    for p, (uc, label, src, lo, hi, ticks) in enumerate([
            ("catalog", "Catalog", R3, 20, 60000, [100, 1000, 10000]), ("kanban", "Kanban", R3, 200, 40000, [300, 1000, 3000, 10000, 30000]),
            ("dashboard", "Dashboard", R4B, 1500, 9000, [2000, 4000, 8000])]):
        px = x0 + p * (pw + gap)
        f = logx(lo, hi, px, px + pw)
        s.text(px, top - 8, label, 13, weight=600)
        for t in ticks:
            s.line(f(t), top, f(t), H - 44, th["grid"])
            s.text(f(t), H - 26, fmt(t), 11, th["ink2"], "middle")
        for i, (lang, b) in enumerate(rows):
            y = top + row * i + row / 2
            if b is None:
                if p == 0:
                    s.text(24, y + 4, lang, 12, weight=600)
                continue
            if p == 0:
                s.text(36, y + 4, NAME[b.removesuffix("-opt")] + (" + opt" if b.endswith("-opt") else ""), 12, th["ink2"])
            v = [x[(b, uc, "page")] for x in src]
            s.line(f(min(v)), y, f(max(v)), y, col(b), 2)
            s.dot(f(min(v)), y, col(b), hollow=True)
            s.dot(f(max(v)), y, col(b))
    s.save("throughput", mode)


def gains(mode, th):
    bs = ["crow", "uwebsockets", "actix", "axum", "fiber", "gohttp"]
    W, x0, x1, row, top = 900, 190, 820, 28, 150
    H = top + (row * 6 + 34) * 2 + 34
    s = Svg(W, H, th, "What the libraries bought: optimized / original", [
        "Ratio of page req/s at c64, log scale. Filled = ratio of the best passes; hollow = the ratio inside each single pass.",
        "Kanban (template-heavy) gains everywhere. Catalog gains in most passes; uWebSockets lost in one. 1x = no change."])
    f = logx(0.4, 100, x0, x1)
    y = top
    for uc, label in (("kanban", "Kanban"), ("catalog", "Catalog")):
        s.text(24, y - 10, label, 13, weight=600)
        for t in (0.5, 1, 2, 5, 10, 20, 50):
            s.line(f(t), y - 4, f(t), y + row * 6, th["ink2"] if t == 1 else th["grid"])
            s.text(f(t), y - 10, f"{t:g}×", 11, th["ink2"], "middle")
        for i, b in enumerate(bs):
            yy = y + row * i + row / 2
            s.text(36, yy + 4, NAME[b], 12, th["ink2"])
            best = max(x[(b + "-opt", uc, "page")] for x in R3) / max(x[(b, uc, "page")] for x in R3)
            for x in R3:
                g = x[(b + "-opt", uc, "page")] / x[(b, uc, "page")]
                if g > 100:
                    s.text(x1 + 8, yy + 4, f"{g:.0f}× ▸", 11, th["ink2"])
                else:
                    s.dot(f(g), yy, th["s"][1], hollow=True, r=4)
            s.dot(f(best), yy, th["s"][1], r=6)
            s.text(f(best), yy - 8, f"{best:.1f}×", 11, anchor="middle", weight=600)
        y += row * 6 + 60
    s.save("gains", mode)


def dashboard(mode, th):
    W, H, x0, x1, top, row = 900, 330, 260, 850, 130, 56
    s = Svg(W, H, th, "Dashboard: the schema mattered more than the language", [
        "Page req/s at c64, log scale. One dot per backend (14), better of two passes; the tick is the median.",
        "Same output at every stage, every request still queries SQLite."])
    f = logx(50, 10000, x0, x1)
    for t in (100, 300, 1000, 3000, 10000):
        s.line(f(t), top - 12, f(t), top + row * 3 - 6, th["grid"])
        s.text(f(t), top + row * 3 + 14, fmt(t), 11, th["ink2"], "middle")
    for i, (a, b, src) in enumerate([("Run 3", "scan 30 days of events", R3), ("Run 4a", "daily rollups + DISTINCT users", R4A), ("Run 4b", "rollups + last-active day", R4B)]):
        y = top + row * i + 16
        s.text(24, y - 2, a, 13, weight=600)
        s.text(24, y + 15, b, 12, th["ink2"])
        v = [max(x[(k, "dashboard", "page")] for x in src) for k in BACKENDS]
        for k in v:
            s.dot(f(k), y, th["s"][0], r=5, op=.75)
        m = median(v)
        s.line(f(m), y - 13, f(m), y + 13, th["ink"], 2)
        s.text(f(m), y - 17, f"median {fmt(m)}", 12, anchor="middle", weight=600)
    s.save("dashboard", mode)


def repeat(mode, th):
    W, H, x0, x1, top, row = 900, 350, 260, 850, 130, 60
    s = Svg(W, H, th, "How repeatable is this? Pass 2 / pass 1, identical setup", [
        "One dot per measurement (page and API, c64), log scale. Shaded: within ±20%. 1x = perfect repeat.",
        "Run 3 is noisy: treat anything under about 2x there as a tie. Run 4 repeats well apart from a few stalls."])
    f = logx(0.008, 5, x0, x1)
    s.rect(f(1 / 1.2), top - 14, f(1.2) - f(1 / 1.2), row * 3 + 2, th["band"])
    for t in (0.01, 0.1, 0.5, 1, 2, 4):
        s.line(f(t), top - 14, f(t), top + row * 3 - 12, th["ink2"] if t == 1 else th["grid"])
        s.text(f(t), top + row * 3 + 8, f"{t:g}×", 11, th["ink2"], "middle")
    for i, (a, src) in enumerate([("Run 3", R3), ("Run 4a (dashboard)", R4A), ("Run 4b (dashboard)", R4B)]):
        y = top + row * i + 16
        r = [src[1][k] / src[0][k] for k in sorted(src[0])]
        out = sum(max(v, 1 / v) > 1.2 for v in r)
        s.text(24, y - 2, a, 13, weight=600)
        s.text(24, y + 15, f"{out} of {len(r)} moved more than 20%", 12, th["ink2"])
        for j, v in enumerate(r):
            s.dot(f(v), y + (j % 5 - 2) * 6, th["s"][0], r=4, op=.75)
    s.save("repeatability", mode)


for mode, th in THEMES.items():
    for fn in (throughput, gains, dashboard, repeat):
        fn(mode, th)
print("wrote", *sorted(p.name for p in OUT.glob("*.svg")))
