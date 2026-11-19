# Run 4: remove the SQLite bottleneck on the dashboard

## Why
In run 3 every backend landed at ~160 req/s on `/dashboard` (range=30): the four dashboard queries scan the
16,809 events of the window four times (~40 ms of SQLite CPU per request), so language and template work are
invisible there. Goal (user, 2026-11-19): remove that bottleneck while staying on SQLite.

## Approaches considered (measured on a copy of the seed DB)
| | Query time / request | Verdict |
|---|---|---|
| A. Covering index on `events` | ~40 → ~22 ms | Not enough alone |
| B. Per-day rollup tables kept by a trigger | ~40 → ~8 ms | **Chosen (A+B)**: rollups are WITHOUT ROWID with day-first keys, i.e. clustered covering indexes |
| C. Precomputed KPIs per range | ~0.1 ms | Rejected: a precomputed answer, breaks "no caching" |
| D. DuckDB for events | — | Rejected: breaks "SQLite only", new dependency in 4 languages |

## Design
- `shared/schema.sql`: `ev_day`, `ev_country_day`, `ev_item_day`, `ev_user` + `events_rollup` AFTER INSERT
  trigger (UPSERTs). `events` is append-only in every app, so no update/delete triggers.
- Distinct users (first attempt, run 4a): `ev_user_day (day, user_id, purchased)` + `count(DISTINCT)`. Measured:
  dashboard 160 → ~650–870 req/s, but all 14 backends still converged (SQLite-bound, ~7 ms/request in the DISTINCT).
- Distinct users (final, run 4b): `ev_user (user_id, last_day, last_purchase_day)` with indexes. The window always
  ends at the anchor ("last N days") and no event is at/after it, so active users = `count(*) WHERE last_day >= lo`
  (exact, ~0.1 ms). The seed test checks it against `count(DISTINCT)` over raw events for 7/13/30/90 days.
- The four dashboard queries in the four DB layers (C++ `shared/cpp/src/db.cpp`, Rust `core/src/db.rs`, Go
  `core/db.go`, Zig `core/db.zig`) read the rollups. Bind parameters are unchanged (ts values; the SQL divides by
  86400 — the window is day-aligned because the anchor is midnight UTC). All 14 backends use them (opt variants
  share these DB layers).
- Every request still queries SQLite; no response caching.

## Gate
- `shared/seed/seed.test.ts` `rollups_match_events`: every rollup row equals the raw `GROUP BY` over `events`.
- Golden: pre-change binary vs post-change binary of the same backend on the new pristine DB, 152/152, for one
  backend per language; C++/Rust/Go/Zig unit tests; `check-backend.sh` for all 14.

## Measurement
Dashboard use case only, two passes (best of), server-only, htmx, all 14 backends, same harness. Results:
`results/run4-dashboard-p{1,2}.sqlite` (4a archived as `results/old/run4a-*`) → `docs/data/`, reported next to run 3 in `docs/results-backend-opt.md`.
