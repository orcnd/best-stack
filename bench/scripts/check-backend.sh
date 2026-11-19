#!/usr/bin/env bash
# Usage: bench/scripts/check-backend.sh <backend> — contract + functional/visual/seo parity for the
# three C++-rendered frontends against a fresh DB. Exits non-zero on the first failure.
# BS_PORT (default 18080) picks the port; each port gets its own DB copy, so checks can run in parallel.
set -uo pipefail
cd "$(dirname "$0")/../.."
be=$1
bin=build/backends/$be/bs-$be
port=${BS_PORT:-18080}
export BS_PORT=$port BASE=http://127.0.0.1:$port
for p in $(lsof -ti tcp:$port -sTCP:LISTEN 2>/dev/null); do ps -o command= -p $p | grep -q 'build/backends/' && kill $p; done; sleep 0.3
db=data/app-$port.sqlite
rm -f $db $db-wal $db-shm && cp data/pristine.sqlite $db
"$bin" --port $port --db $db --root . --threads 8 > "/tmp/bs-$be-$port.log" 2>&1 &
pid=$!
trap 'kill $pid 2>/dev/null' EXIT
for _ in $(seq 1 50); do curl -sf 127.0.0.1:$port/healthz >/dev/null && break; sleep 0.1; done
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
