#!/usr/bin/env bash
# Builds the Go backends and places binaries where the harness expects them.
set -euo pipefail
cd "$(dirname "$0")"
for b in gohttp fiber; do mkdir -p ../../build/backends/$b && go build -trimpath -o ../../build/backends/$b/bs-$b ./cmd/$b; done
