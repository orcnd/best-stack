import assert from "node:assert/strict";
import { mkdtempSync, readFileSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { test } from "node:test";
import { ResultsDb } from "../bench/harness/resultsdb.ts";
import { cvScore, errorScore, higherBetter, lowerBetter, robustCv, score } from "./score.ts";

const weights = JSON.parse(readFileSync(join(import.meta.dirname, "weights.json"), "utf8"));

test("lower-better: best gets 100, twice as bad gets 50", () => {
  assert.deepEqual(lowerBetter([10, 20, 40]), [100, 50, 25]);
});
test("higher-better: best gets 100", () => {
  assert.deepEqual(higherBetter([100, 50]), [100, 50]);
});
test("error rate 0 → 100; CV 4% → 80; CV ≥ 20% → 0", () => {
  assert.equal(errorScore(0), 100);
  assert.equal(errorScore(0.1), 90);
  assert.equal(cvScore(4), 80);
  assert.equal(cvScore(30), 0);
});

test("robust CV ignores a single spike (MAD-based)", () => {
  assert.equal(robustCv([25, 25, 25, 25, 25, 25, 25, 25, 25, 200]), 0);
  assert.ok(Math.abs(robustCv([90, 100, 110, 100, 90, 110]) - 14.826) < 0.01);
});

test("memory uses footprint_kb when present, rss_kb otherwise", () => {
  const { db, run } = fixture();
  db.db.exec("UPDATE resources SET footprint_kb = rss_kb / 10");
  const b = score(db, run, weights).combos.find((c) => c.backend === "b")!;
  assert.equal(b.values.rss, 200);
});

/** Two combos in one use case; B is exactly 2x worse than A on every lower-better metric. */
function fixture(parityB = 1) {
  const db = new ResultsDb(join(mkdtempSync(join(tmpdir(), "bs-score-")), "r.sqlite"));
  const run = db.startRun({ seed: 1, order: [], options: {}, versions: {}, machine: "m", os: "o" });
  const add = (backend: string, frontend: string, k: number, parity: number) => {
    const c = db.comboId(backend, frontend, "catalog");
    db.upsertParity({ run_id: run, combo_id: c, functional_ok: parity, visual_diff_ratio: 0, seo_ok: 1, parity_ok: parity, details_json: "{}" });
    for (const profile of ["desktop", "mobile"]) for (let i = 0; i < 5; i++)
      db.insertPageLoad({ run_id: run, combo_id: c, profile, page: "/catalog", iter: i, ttfb: 10 * k, fcp: 20 * k, lcp: 40 * k, dcl: 30 * k, load: 50 * k, bytes: 1000 * k, js_bytes: 100 * k, requests: 5, tbt: 0, heap: 1 });
    for (const profile of ["desktop", "mobile"]) for (let i = 0; i < 3; i++)
      db.insertInteraction({ run_id: run, combo_id: c, profile, step: "s", iter: i, duration_ms: 100 * k, ok: 1 });
    db.insertLoadTest({ run_id: run, combo_id: c, target: "page", url: "u", concurrency: 64, duration_s: 10, rps: 1000 / k, p50: 1 * k, p95: 2 * k, p99: 3 * k, errors: 0, total: 1000, raw_json: "{}" });
    db.insertResource({ run_id: run, combo_id: c, phase: "load_test_page", process_role: "cpp_backend", pid: 1, ts: 0, rss_kb: 1000 * k, cpu_pct: 100, threads: 8, fds: 10 });
    db.insertStart({ run_id: run, combo_id: c, process_role: "cpp_backend", cold_start_ms: 10 * k });
    db.insertDx({ run_id: run, component: "backend", name: backend, loc: 100 * k, deps: k - 1, build_s: 10 * k, artifact_kb: 100 * k, details_json: "{}" });
    db.insertDx({ run_id: run, component: "frontend", name: frontend, loc: 100 * k, deps: k - 1, build_s: 1 * k, artifact_kb: 10 * k, details_json: "{}" });
  };
  add("a", "x", 1, 1);
  add("b", "y", 2, parityB);
  return { db, run };
}

test("best combo scores 100 overall, 2x-worse combo scores 50", () => {
  const { db, run } = fixture();
  const r = score(db, run, weights);
  const a = r.combos.find((c) => c.backend === "a")!;
  const b = r.combos.find((c) => c.backend === "b")!;
  assert.equal(Math.round(a.total), 100);
  assert.equal(Math.round(b.categories.user), 50);
  assert.equal(Math.round(b.categories.server), 50);
  assert.equal(Math.round(b.categories.dx), 50);
  assert.equal(Math.round(b.categories.stability), 100);
  assert.equal(Math.round(b.total), Math.round(0.4 * 50 + 0.25 * 50 + 0.2 * 50 + 0.15 * 100));
});

test("combos failing parity are excluded from the ranking", () => {
  const { db, run } = fixture(0);
  const r = score(db, run, weights);
  assert.deepEqual(r.combos.filter((c) => c.ranked).map((c) => c.backend), ["a"]);
  assert.equal(r.excluded.length, 1);
});

test("changing weights changes the total", () => {
  const { db, run } = fixture();
  const w2 = { ...weights, categories: { user: 0, server: 0, dx: 0, stability: 100 } };
  const b = score(db, run, w2).combos.find((c) => c.backend === "b")!;
  assert.equal(Math.round(b.total), 100);
});

test("scores are written to the scores table", () => {
  const { db, run } = fixture();
  score(db, run, weights, { write: true });
  assert.ok(db.count("scores") > 0);
});
