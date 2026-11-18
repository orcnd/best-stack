// Scores a run from raw rows in results/results.sqlite. Reads only raw tables; writes `scores` + report.md.
// Usage: node scoring/score.ts [--run <run_id>] [--weights scoring/weights.json] [--out results/report.md]
import { readFileSync, writeFileSync } from "node:fs";
import { join } from "node:path";
import { ResultsDb } from "../bench/harness/resultsdb.ts";

export interface Weights {
  categories: Record<Category, number>;
  metrics: Record<Category, Record<string, number>>;
  load_concurrency: number;
}
type Category = "user" | "server" | "dx" | "stability";
const CATEGORIES: Category[] = ["user", "server", "dx", "stability"];

export interface ComboScore {
  combo_id: number; backend: string; frontend: string; usecase: string; ranked: boolean;
  values: Record<string, number | null>; scores: Record<string, number | null>;
  categories: Record<Category, number>; total: number;
}
export interface Result {
  run_id: string; combos: ComboScore[]; excluded: { backend: string; frontend: string; usecase: string; reason: string }[];
  overall: { backend: string; frontend: string; total: number; categories: Record<Category, number> }[];
  backends: { name: string; total: number }[]; frontends: { name: string; total: number }[];
}

// ---------- normalization ----------
const finite = (v: number | null | undefined): v is number => typeof v === "number" && Number.isFinite(v);
export const lowerBetter = (vs: (number | null)[], best = Math.min(...vs.filter(finite).filter((v) => v > 0))) =>
  vs.map((v) => (finite(v) && v > 0 ? Math.min(100, (best / v) * 100) : finite(v) ? 100 : null)) as number[];
export const higherBetter = (vs: (number | null)[], best = Math.max(...vs.filter(finite))) =>
  vs.map((v) => (finite(v) && best > 0 ? Math.min(100, (v / best) * 100) : null)) as number[];
export const errorScore = (rate: number) => Math.round(100 * (1 - rate) * 1e9) / 1e9;
export const cvScore = (cvPct: number) => Math.max(0, 100 - 5 * cvPct);

const median = (xs: number[]) => {
  const s = xs.filter(finite).sort((a, b) => a - b);
  if (!s.length) return null;
  const m = s.length >> 1;
  return s.length % 2 ? s[m] : (s[m - 1] + s[m]) / 2;
};
const mean = (xs: (number | null)[]) => {
  const v = xs.filter(finite);
  return v.length ? v.reduce((a, b) => a + b, 0) / v.length : null;
};
const weightedMean = (scores: Record<string, number | null>, w: Record<string, number>) => {
  let sum = 0, tw = 0;
  for (const [k, wk] of Object.entries(w)) if (finite(scores[k]) && wk > 0) { sum += scores[k]! * wk; tw += wk; }
  return tw ? sum / tw : 0;
};

// ---------- extraction ----------
type Raw = { combo_id: number; backend: string; frontend: string; usecase: string; parity_ok: number | null; details: string | null };

function rows<T>(db: ResultsDb, sql: string, ...p: (string | number)[]): T[] {
  return db.db.prepare(sql).all(...p) as T[];
}

function comboValues(db: ResultsDb, run: string, c: Raw, w: Weights, profiles: string[]) {
  const v: Record<string, number | null> = {};
  for (const profile of profiles) {
    const pl = rows<{ page: string; ttfb: number; lcp: number | null; fcp: number | null; bytes: number }>(db,
      "SELECT page, ttfb, lcp, fcp, bytes FROM page_loads WHERE run_id = ? AND combo_id = ? AND profile = ?", run, c.combo_id, profile);
    const pages = [...new Set(pl.map((r) => r.page))];
    v[`lcp_${profile}`] = mean(pages.map((p) => median(pl.filter((r) => r.page === p).map((r) => r.lcp ?? r.fcp ?? NaN))));
    v[`ttfb_${profile}`] = mean(pages.map((p) => median(pl.filter((r) => r.page === p).map((r) => r.ttfb))));
    v[`bytes_${profile}`] = mean(pages.map((p) => median(pl.filter((r) => r.page === p).map((r) => r.bytes))));
    const it = rows<{ iter: number; total: number; bad: number }>(db,
      "SELECT iter, sum(duration_ms) AS total, sum(1 - ok) AS bad FROM interactions WHERE run_id = ? AND combo_id = ? AND profile = ? GROUP BY iter", run, c.combo_id, profile);
    v[`interaction_${profile}`] = median(it.filter((r) => !r.bad).map((r) => r.total));
  }
  const concs = rows<{ concurrency: number }>(db, "SELECT DISTINCT concurrency FROM load_tests WHERE run_id = ? AND combo_id = ? AND target = 'page'", run, c.combo_id).map((r) => r.concurrency);
  const conc = concs.includes(w.load_concurrency) ? w.load_concurrency : Math.max(...concs, 0);
  const lt = rows<{ rps: number; p95: number | null }>(db, "SELECT rps, p95 FROM load_tests WHERE run_id = ? AND combo_id = ? AND target = 'page' AND concurrency = ?", run, c.combo_id, conc)[0];
  v.rps = lt?.rps ?? null;
  v.p95 = lt?.p95 ?? null;
  // peak RSS during the page load test: per-role peak, summed (Bun combos have two processes)
  v.rss = rows<{ peak: number }>(db, "SELECT max(rss_kb) AS peak FROM resources WHERE run_id = ? AND combo_id = ? AND phase = 'load_test_page' GROUP BY process_role", run, c.combo_id)
    .reduce((a, r) => a + (r.peak ?? 0), 0) || null;
  v.cold_start = rows<{ ms: number }>(db, "SELECT cold_start_ms AS ms FROM starts WHERE run_id = ? AND combo_id = ?", run, c.combo_id).reduce((a, r) => a + r.ms, 0) || null;
  const errs = rows<{ e: number; t: number }>(db, "SELECT coalesce(sum(errors), 0) AS e, coalesce(sum(total), 0) AS t FROM load_tests WHERE run_id = ? AND combo_id = ?", run, c.combo_id)[0];
  const ints = rows<{ bad: number; n: number }>(db, "SELECT coalesce(sum(1 - ok), 0) AS bad, count(*) AS n FROM interactions WHERE run_id = ? AND combo_id = ?", run, c.combo_id)[0];
  const denom = errs.t + ints.n;
  v.error_rate = denom ? (errs.e + ints.bad) / denom : 0;
  const lcpRows = rows<{ page: string; lcp: number | null; fcp: number | null }>(db, "SELECT page, lcp, fcp FROM page_loads WHERE run_id = ? AND combo_id = ? AND profile = 'desktop'", run, c.combo_id);
  const cvs = [...new Set(lcpRows.map((r) => r.page))].map((p) => {
    const xs = lcpRows.filter((r) => r.page === p).map((r) => r.lcp ?? r.fcp ?? NaN).filter(finite);
    const m = mean(xs);
    if (!m || xs.length < 2) return null;
    const sd = Math.sqrt(xs.reduce((a, x) => a + (x - m) ** 2, 0) / (xs.length - 1));
    return (sd / m) * 100;
  });
  v.variance_cv = median(cvs.filter(finite));
  return v;
}

function componentScores(db: ResultsDb, run: string, component: "backend" | "frontend", names: string[]) {
  const dx = rows<{ name: string; loc: number; deps: number; build_s: number | null; artifact_kb: number | null }>(db,
    "SELECT name, loc, deps, build_s, artifact_kb FROM dx_components WHERE run_id = ? AND component = ?", run, component).filter((r) => names.includes(r.name));
  const out: Record<string, number | null> = {};
  if (!dx.length) return out;
  const cols = ["loc", "deps", "build_s", "artifact_kb"] as const;
  const per = cols.map((k) => lowerBetter(dx.map((r) => (k === "deps" ? r[k] + 1 : r[k])))); // deps+1: zero deps is best, not undefined
  dx.forEach((r, i) => { out[r.name] = mean(per.map((s) => s[i])); });
  return out;
}

export function latestRun(db: ResultsDb): string {
  const r = db.db.prepare("SELECT run_id FROM env WHERE finished_at IS NOT NULL ORDER BY started_at DESC LIMIT 1").get() as { run_id: string } | undefined;
  if (!r) throw new Error("no finished run in results db");
  return r.run_id;
}

export function score(db: ResultsDb, run: string, w: Weights, opts: { write?: boolean } = {}): Result {
  const combos = rows<Raw>(db,
    `SELECT c.combo_id, c.backend, c.frontend, c.usecase, p.parity_ok, p.details_json AS details
     FROM combos c JOIN parity p ON p.combo_id = c.combo_id AND p.run_id = ? ORDER BY c.usecase, c.backend, c.frontend`, run);
  const profiles = rows<{ profile: string }>(db, "SELECT DISTINCT profile FROM page_loads WHERE run_id = ?", run).map((r) => r.profile);
  const ranked = (c: Raw) => c.parity_ok === 1;
  const vals = new Map(combos.map((c) => [c.combo_id, comboValues(db, run, c, w, profiles)]));
  const beB = componentScores(db, run, "backend", [...new Set(combos.filter(ranked).map((c) => c.backend))]);
  const feB = componentScores(db, run, "frontend", [...new Set(combos.filter(ranked).map((c) => c.frontend))]);

  const out: ComboScore[] = [];
  for (const uc of [...new Set(combos.map((c) => c.usecase))]) {
    const group = combos.filter((c) => c.usecase === uc);
    const ref = group.filter(ranked);
    const best = (k: string, dir: "low" | "high") => {
      const xs = ref.map((c) => vals.get(c.combo_id)![k]).filter(finite).filter((x) => dir === "high" || x > 0);
      return xs.length ? (dir === "low" ? Math.min(...xs) : Math.max(...xs)) : NaN;
    };
    const low = (k: string, v: number | null) => lowerBetter([v], best(k, "low"))[0];
    const high = (k: string, v: number | null) => higherBetter([v], best(k, "high"))[0];
    for (const c of group) {
      const v = vals.get(c.combo_id)!;
      const s: Record<string, number | null> = {};
      for (const m of ["lcp", "ttfb", "interaction", "bytes"]) s[m] = mean(profiles.map((p) => low(`${m}_${p}`, v[`${m}_${p}`])));
      s.rps = high("rps", v.rps);
      s.p95 = low("p95", v.p95);
      s.rss = low("rss", v.rss);
      s.backend = beB[c.backend] ?? null;
      s.frontend = feB[c.frontend] ?? null;
      s.cold_start = low("cold_start", v.cold_start);
      s.error_rate = errorScore(v.error_rate ?? 0);
      s.variance = finite(v.variance_cv) ? cvScore(v.variance_cv) : null;
      const cats = Object.fromEntries(CATEGORIES.map((k) => [k, weightedMean(s, w.metrics[k])])) as Record<Category, number>;
      const tw = CATEGORIES.reduce((a, k) => a + w.categories[k], 0);
      const total = CATEGORIES.reduce((a, k) => a + cats[k] * w.categories[k], 0) / tw;
      out.push({ combo_id: c.combo_id, backend: c.backend, frontend: c.frontend, usecase: uc, ranked: ranked(c), values: v, scores: s, categories: cats, total });
    }
  }

  const rankedScores = out.filter((c) => c.ranked);
  const pairs = [...new Set(rankedScores.map((c) => `${c.backend}|${c.frontend}`))];
  const overall = pairs.map((p) => {
    const [backend, frontend] = p.split("|");
    const xs = rankedScores.filter((c) => c.backend === backend && c.frontend === frontend);
    return { backend, frontend, total: mean(xs.map((x) => x.total))!, categories: Object.fromEntries(CATEGORIES.map((k) => [k, mean(xs.map((x) => x.categories[k]))!])) as Record<Category, number> };
  }).sort((a, b) => b.total - a.total);
  const avgBy = (key: "backend" | "frontend") => [...new Set(rankedScores.map((c) => c[key]))]
    .map((name) => ({ name, total: mean(rankedScores.filter((c) => c[key] === name).map((c) => c.total))! })).sort((a, b) => b.total - a.total);

  const result: Result = {
    run_id: run, combos: out,
    excluded: combos.filter((c) => !ranked(c)).map((c) => ({ backend: c.backend, frontend: c.frontend, usecase: c.usecase, reason: c.details ?? "" })),
    overall, backends: avgBy("backend"), frontends: avgBy("frontend"),
  };

  if (opts.write) {
    db.db.exec("CREATE TABLE IF NOT EXISTS scores (run_id TEXT NOT NULL, combo_id INTEGER NOT NULL, category TEXT NOT NULL, metric TEXT NOT NULL, value REAL, score REAL)");
    db.db.prepare("DELETE FROM scores WHERE run_id = ?").run(run);
    const rowsOut: Record<string, string | number | null>[] = [];
    for (const c of out) {
      for (const k of CATEGORIES) {
        for (const m of Object.keys(w.metrics[k])) rowsOut.push({ run_id: run, combo_id: c.combo_id, category: k, metric: m, value: null, score: c.scores[m] ?? null });
        rowsOut.push({ run_id: run, combo_id: c.combo_id, category: k, metric: "_category", value: null, score: c.categories[k] });
      }
      for (const [m, val] of Object.entries(c.values)) rowsOut.push({ run_id: run, combo_id: c.combo_id, category: "raw", metric: m, value: val, score: null });
      rowsOut.push({ run_id: run, combo_id: c.combo_id, category: "total", metric: "_total", value: null, score: c.total });
    }
    db.insertMany("scores", rowsOut);
  }
  return result;
}

// ---------- report ----------
const f0 = (x: number | null | undefined) => (finite(x) ? Math.round(x).toString() : "–");
const f1 = (x: number | null | undefined) => (finite(x) ? x.toFixed(1) : "–");

export function report(r: Result, w: Weights, env: Record<string, unknown>): string {
  const L: string[] = [];
  L.push(`# Best Stack — sonuç raporu`, "", `Run: \`${r.run_id}\` · ${env.machine} · ${env.os} · başlangıç ${env.started_at} · bitiş ${env.finished_at}`, "");
  L.push(`Ağırlıklar: Kullanıcı hızı ${w.categories.user} · Sunucu ${w.categories.server} · DX ${w.categories.dx} · Kararlılık ${w.categories.stability}. Her hücre 0–100; toplam ağırlıklı ortalama (/100).`, "");
  L.push("## Genel sıralama (3 senaryo ortalaması)", "", "| # | Backend | Frontend | Kullanıcı | Sunucu | DX | Kararlılık | **Toplam** |", "|---|---|---|---|---|---|---|---|");
  r.overall.forEach((o, i) => L.push(`| ${i + 1} | ${o.backend} | ${o.frontend} | ${f0(o.categories.user)} | ${f0(o.categories.server)} | ${f0(o.categories.dx)} | ${f0(o.categories.stability)} | **${f1(o.total)}** |`));
  L.push("", "## Backend ortalamaları", "", "| Backend | Toplam |", "|---|---|", ...r.backends.map((b) => `| ${b.name} | ${f1(b.total)} |`));
  L.push("", "## Frontend ortalamaları", "", "| Frontend | Toplam |", "|---|---|", ...r.frontends.map((b) => `| ${b.name} | ${f1(b.total)} |`));
  for (const uc of [...new Set(r.combos.map((c) => c.usecase))]) {
    const xs = r.combos.filter((c) => c.usecase === uc && c.ranked).sort((a, b) => b.total - a.total);
    L.push("", `## Senaryo: ${uc}`, "", "| # | Backend | Frontend | LCP ms (masaüstü) | Etkileşim ms (masaüstü) | KB | req/s @" + w.load_concurrency + " | p95 ms | RSS MB | Kullanıcı | Sunucu | DX | Kararlılık | **Toplam** |", "|---|---|---|---|---|---|---|---|---|---|---|---|---|---|");
    xs.forEach((c, i) => L.push(`| ${i + 1} | ${c.backend} | ${c.frontend} | ${f0(c.values.lcp_desktop)} | ${f0(c.values.interaction_desktop)} | ${f0((c.values.bytes_desktop ?? NaN) / 1024)} | ${f0(c.values.rps)} | ${f1(c.values.p95)} | ${f0((c.values.rss ?? NaN) / 1024)} | ${f0(c.categories.user)} | ${f0(c.categories.server)} | ${f0(c.categories.dx)} | ${f0(c.categories.stability)} | **${f1(c.total)}** |`));
  }
  L.push("", "## Parite dışı kalanlar", "");
  if (!r.excluded.length) L.push("Yok — tüm kombinasyonlar fonksiyonel, görsel ve SEO kontrollerini geçti.");
  for (const e of r.excluded) L.push(`- ${e.backend} × ${e.frontend} × ${e.usecase}: \`${e.reason.slice(0, 300)}\``);
  return L.join("\n") + "\n";
}

if (import.meta.main) {
  const a = process.argv.slice(2);
  const get = (k: string, d: string) => { const i = a.indexOf(`--${k}`); return i >= 0 ? a[i + 1] : d; };
  const root = join(import.meta.dirname, "..");
  const db = new ResultsDb(get("db", join(root, "results", "results.sqlite")));
  const w = JSON.parse(readFileSync(get("weights", join(root, "scoring", "weights.json")), "utf8")) as Weights;
  const run = get("run", "") || latestRun(db);
  const r = score(db, run, w, { write: true });
  const env = db.db.prepare("SELECT * FROM env WHERE run_id = ?").get(run) as Record<string, unknown>;
  const out = get("out", join(root, "results", "report.md"));
  writeFileSync(out, report(r, w, env));
  console.log(`scored ${r.combos.length} combos (${r.excluded.length} excluded) → ${out}`);
}
