#!/usr/bin/env bash
# Builds the Go backends and places binaries where the harness expects them.
set -euo pipefail
cd "$(dirname "$0")"
# Same SQLite option as the other languages: mattn/go-sqlite3 compiles with MEMSTATUS on, which takes a
# global mutex on every allocation (/api/dashboard at 64 clients: 46 → 225 req/s). mattn already sets
# THREADSAFE=1, DEFAULT_WAL_SYNCHRONOUS=1 and OMIT_DEPRECATED itself.
export CGO_CFLAGS="-O2 -DSQLITE_DEFAULT_MEMSTATUS=0 -DSQLITE_DQS=0 -DSQLITE_LIKE_DOESNT_MATCH_BLOBS -DSQLITE_OMIT_SHARED_CACHE -DSQLITE_USE_ALLOCA"
for b in gohttp fiber; do mkdir -p ../../build/backends/$b && go build -a -trimpath -o ../../build/backends/$b/bs-$b ./cmd/$b; done
