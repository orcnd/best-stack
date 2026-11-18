#!/usr/bin/env bash
# Usage: bench/scripts/check-backend.sh <backend> — contract + functional/visual/seo parity for the
# three C++-rendered frontends against a fresh DB. Exits non-zero on the first failure.
set -uo pipefail
cd "$(dirname "$0")/../.."
be=$1
bin=build/backends/$be/bs-$be
pkill -f 'build/backends/.*/bs-' 2>/dev/null; sleep 0.3
bench/scripts/reset-db.sh
"$bin" --db data/app.sqlite --root . --threads 8 > "/tmp/bs-$be.log" 2>&1 &
pid=$!
trap 'kill $pid 2>/dev/null' EXIT
for _ in $(seq 1 50); do curl -sf localhost:18080/healthz >/dev/null && break; sleep 0.1; done
status=0
bun test bench/contract 2>&1 | grep -E "\(fail\)|^ *[0-9]+ (pass|fail)$" || status=1
bun test bench/contract >/dev/null 2>&1 || status=1
for fe in htmx alpine vanilla; do
  node bench/parity/functional.ts $fe || status=1
  node bench/parity/visual.ts $fe | python3 -c 'import json,sys; d=json.loads(sys.stdin.read()); print(d["frontend"], "visual ok" if d["ok"] else [p for p in d["pages"] if p["ratio"]>0])' || status=1
  node bench/parity/seo.ts $fe || status=1
done
echo "check-backend $be: $([ $status = 0 ] && echo PASS || echo FAIL)"
exit $status
