#!/usr/bin/env bash
# Fresh app DB from the pristine seed.
set -euo pipefail
cd "$(dirname "$0")/../.."
rm -f data/app.sqlite data/app.sqlite-wal data/app.sqlite-shm
cp data/pristine.sqlite data/app.sqlite
