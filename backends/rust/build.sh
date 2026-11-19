#!/usr/bin/env bash
# Builds the Rust backends and places binaries where the harness expects them.
set -euo pipefail
cd "$(dirname "$0")"
cargo build --release
for b in axum actix axum-opt actix-opt; do mkdir -p ../../build/backends/$b && cp target/release/bs-$b ../../build/backends/$b/bs-$b; done
