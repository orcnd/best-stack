# Run 3: optimized backends (each language's two fastest paths)

## Why
Run 2 showed Zig ahead mainly because its core writes HTML straight into a per-request buffer, while the
C++ (nlohmann view model + inja), Rust (serde_json Value + minijinja) and Go (html/template) cores build an
intermediate tree and interpret a template at runtime. Profile of Crow on /htmx/catalog at c64: 19% SQLite,
27% malloc/free, 28% JSON view model + inja. http.zig: 66% SQLite, 3% malloc. Question: how much of the gap
closes when C++, Rust and Go use the most popular libraries that remove that layer?

## What (approved in chat, 2026-11-19)
Per language, the two most popular libraries that target the measured bottleneck:

| Language | Frameworks (unchanged) | Libraries | New backends |
|---|---|---|---|
| C++ | Crow, uWebSockets (best two C++ in run 2) | **glaze** (struct ⇄ JSON, replaces nlohmann view models) + **mimalloc**; HTML written with `std::format_to` into a reserved buffer | `crow-opt`, `uwebsockets-opt` |
| Rust | Axum, Actix-web | **askama** (compile-time templates over typed structs) + **mimalloc** (global allocator) | `axum-opt`, `actix-opt` |
| Go | net/http, Fiber | **templ** (compiled templates) + **goccy/go-json** | `gohttp-opt`, `fiber-opt` |

Zig (http.zig, zap) already writes HTML directly and is re-measured unchanged.

## Rules
- Originals stay untouched; optimized variants are separate binaries in `build/backends/<name>-opt/`.
- Same SQL, same SQLite build options, same thread counts, same HTTP framework config as the original.
- **Equivalence gate:** `node bench/parity/golden.ts <orig> <name>-opt` must pass 152/152 (HTML equal after
  whitespace normalization, JSON deep-equal, same status/content-type/location), and
  `bench/scripts/check-backend.sh <name>-opt` must PASS (contract + functional/visual/SEO parity).
- No compression, no caching of responses, no precomputed pages: every request still queries SQLite.

## Measurement
`--server-only` run: htmx frontend (backend-rendered pages), 3 use cases, page + API load tests at
c1/c16/c64, resources sampled, cold start, DX (LOC, deps, clean build). No browser page loads/flows (the
frontend is identical). Backends: the 6 optimized + their 6 originals + httpzig + zap = 14 × 3 = 42 combos,
in one session so machine state is shared. Raw data: `docs/data/results-run3-backend.sqlite`; report:
`docs/results-backend-opt.md` (per language: the two fastest paths; original vs optimized; vs Zig).

## Non-goals
Re-running browser metrics; changing scoring weights for run 2; optimizing Zig further.
