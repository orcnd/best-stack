-- Raw measurements. One row per sample; no aggregation happens here (see scoring/).
CREATE TABLE IF NOT EXISTS env (
  run_id TEXT PRIMARY KEY, started_at TEXT NOT NULL, finished_at TEXT,
  machine TEXT, os TEXT, versions_json TEXT, seed INTEGER, order_json TEXT, options_json TEXT, notes TEXT
);
CREATE TABLE IF NOT EXISTS combos (
  combo_id INTEGER PRIMARY KEY, backend TEXT NOT NULL, frontend TEXT NOT NULL, usecase TEXT NOT NULL,
  UNIQUE (backend, frontend, usecase)
);
CREATE TABLE IF NOT EXISTS parity (
  run_id TEXT NOT NULL, combo_id INTEGER NOT NULL, functional_ok INTEGER, visual_diff_ratio REAL, seo_ok INTEGER,
  parity_ok INTEGER, details_json TEXT, PRIMARY KEY (run_id, combo_id)
);
CREATE TABLE IF NOT EXISTS starts (
  run_id TEXT NOT NULL, combo_id INTEGER NOT NULL, process_role TEXT NOT NULL, cold_start_ms REAL
);
CREATE TABLE IF NOT EXISTS page_loads (
  run_id TEXT NOT NULL, combo_id INTEGER NOT NULL, profile TEXT NOT NULL, page TEXT NOT NULL, iter INTEGER NOT NULL,
  ttfb REAL, fcp REAL, lcp REAL, dcl REAL, load REAL, bytes INTEGER, js_bytes INTEGER, requests INTEGER, tbt REAL, heap INTEGER
);
CREATE TABLE IF NOT EXISTS interactions (
  run_id TEXT NOT NULL, combo_id INTEGER NOT NULL, profile TEXT NOT NULL, step TEXT NOT NULL, iter INTEGER NOT NULL,
  duration_ms REAL, ok INTEGER NOT NULL
);
CREATE TABLE IF NOT EXISTS load_tests (
  run_id TEXT NOT NULL, combo_id INTEGER NOT NULL, target TEXT NOT NULL, url TEXT NOT NULL, concurrency INTEGER NOT NULL,
  duration_s REAL, rps REAL, p50 REAL, p95 REAL, p99 REAL, errors INTEGER, total INTEGER, raw_json TEXT
);
CREATE TABLE IF NOT EXISTS resources (
  run_id TEXT NOT NULL, combo_id INTEGER NOT NULL, phase TEXT NOT NULL, process_role TEXT NOT NULL, pid INTEGER,
  ts REAL NOT NULL, rss_kb INTEGER, cpu_pct REAL, threads INTEGER, fds INTEGER
);
CREATE TABLE IF NOT EXISTS dx_components (
  run_id TEXT NOT NULL, component TEXT NOT NULL, name TEXT NOT NULL,
  loc INTEGER, deps INTEGER, build_s REAL, artifact_kb REAL, details_json TEXT
);
CREATE INDEX IF NOT EXISTS page_loads_combo ON page_loads (run_id, combo_id);
CREATE INDEX IF NOT EXISTS interactions_combo ON interactions (run_id, combo_id);
CREATE INDEX IF NOT EXISTS resources_combo ON resources (run_id, combo_id, phase);
