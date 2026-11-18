#!/usr/bin/env bash
# Builds the Zig backends (Zig 0.17) and places binaries where the harness expects them.
set -euo pipefail
cd "$(dirname "$0")"
zig build -Doptimize=ReleaseFast
for b in httpzig zap; do mkdir -p ../../build/backends/$b && cp zig-out/bin/bs-$b ../../build/backends/$b/bs-$b; done
