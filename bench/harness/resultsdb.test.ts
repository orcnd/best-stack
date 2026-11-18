import assert from "node:assert/strict";
import { mkdtempSync } from "node:fs";
import { tmpdir } from "node:os";
import { join } from "node:path";
import { test } from "node:test";
import { ResultsDb } from "./resultsdb.ts";

const path = () => join(mkdtempSync(join(tmpdir(), "bs-results-")), "r.sqlite");

test("creates schema and env row", () => {
  const db = new ResultsDb(path());
  const run = db.startRun({ seed: 1, order: [], options: {}, versions: {}, machine: "m", os: "o" });
  assert.match(run, /^run-/);
  assert.equal(db.count("env"), 1);
});

test("combo ids are stable per (backend, frontend, usecase)", () => {
  const db = new ResultsDb(path());
  const a = db.comboId("drogon", "htmx", "catalog");
  assert.equal(db.comboId("drogon", "htmx", "catalog"), a);
  assert.notEqual(db.comboId("drogon", "htmx", "kanban"), a);
});

test("inserts raw samples", () => {
  const db = new ResultsDb(path());
  const run = db.startRun({ seed: 1, order: [], options: {}, versions: {}, machine: "m", os: "o" });
  const c = db.comboId("crow", "astro", "dashboard");
  db.insertPageLoad({ run_id: run, combo_id: c, profile: "desktop", page: "/dashboard", iter: 0, ttfb: 1, fcp: 2, lcp: 3, dcl: 4, load: 5, bytes: 6, js_bytes: 7, requests: 8, tbt: 0, heap: 9 });
  db.insertResource({ run_id: run, combo_id: c, phase: "idle", process_role: "cpp_backend", pid: 1, ts: 0, rss_kb: 10, cpu_pct: 0, threads: 9, fds: 12 });
  db.insertResource({ run_id: run, combo_id: c, phase: "idle", process_role: "bun_ssr", pid: 2, ts: 0, rss_kb: 20, cpu_pct: 0, threads: 9, fds: 12 });
  db.insertInteraction({ run_id: run, combo_id: c, profile: "desktop", step: "load", iter: 0, duration_ms: 12.5, ok: 1 });
  assert.equal(db.count("page_loads"), 1);
  assert.equal(db.count("resources"), 2);
  assert.equal(db.count("interactions"), 1);
});

test("reopening keeps existing rows", () => {
  const p = path();
  const db = new ResultsDb(p);
  db.comboId("httplib", "vanilla", "kanban");
  db.close();
  assert.equal(new ResultsDb(p).count("combos"), 1);
});
