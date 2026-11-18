# Zig backends notes

- Zig 0.17.1 (Homebrew). http.zig `master` targets 0.17.
- **zap is vendored and patched** (`vendor/zap`, from the upstream `zig-master` branch, MIT): its latest
  release targets Zig 0.15.1. Patches, all marked `best-stack patch`: trimmed `build.zig` (library
  module only; `std.process.getEnvVarOwned` removed in 0.17), `.Debug` → `.debug` optimize enum,
  `std.posix.nanosleep` → libc `nanosleep`, `std.fmt.bufPrintZ` → `bufPrintSentinel`.
- No template engine: pages are written by `core/render.zig`; Alpine's client-side templates are the
  same static snippets as the Go backend (`core/alpine/*.html`, embedded).
- SQLite: the pinned amalgamation compiled with the same options as every other backend; one
  connection per handler thread (`threadlocal`). Text is bound with `SQLITE_STATIC` because Zig's
  translate-c cannot represent `SQLITE_TRANSIENT` (pointer −1); bound memory lives in the request arena
  until the statement is reset.
- Concurrency: http.zig — 1 event-loop worker + `--threads` handler threads; zap — `--threads`
  facil.io threads in 1 worker process.
