# uWebSockets notes

## macOS multi-core spike (plan Task 12)

`oha -z 5s -c 64 /api/catalog/products?page=3`, 8 threads, Apple M2:

| Model | Per-loop request split | req/s |
|---|---|---|
| One `App` per thread, all `listen()` on the same port (`SO_REUSEPORT`) | `0 0 32175 0 0 0 0 0` — macOS does not balance | ~6.4k |
| Acceptor `App` + 8 worker `App`s via `addChildApp` (uWS's built-in round-robin) | `11575 11118 10720 11385 11248 10723 11749 11395` | ~18k |

Chosen: acceptor + `addChildApp`. On Linux `SO_REUSEPORT` balances, so this is a macOS-specific
choice; it is uWS's own load-balancing API, not a custom dispatcher.

## Details

- Worker loops have no listen socket; a long no-op `us_timer` keeps `uWS::run()` from returning.
- uSockets is pinned to the commit uWebSockets v20.81.0 references (v0.8.8 lacks the newer
  `on_pre_open` signature); its own submodules (boringssl, lsquic) are not fetched. Built with `LIBUS_NO_SSL`, `UWS_NO_ZLIB`.
- Handlers run on the worker event loop (SQLite calls are blocking, same as Drogon's model).
- `BS_UWS_STATS=1` prints per-loop request counts every 2 s.
