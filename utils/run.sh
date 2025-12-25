#!/usr/bin/env bash
set -euo pipefail

# Build and run the server from repo root.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

detect_cpus() {
  if command -v nproc >/dev/null 2>&1; then
    nproc
  elif command -v getconf >/dev/null 2>&1; then
    getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4
  else
    sysctl -n hw.ncpu 2>/dev/null || echo 4
  fi
}

JOBS="${JOBS:-$(detect_cpus)}"

make -j"${JOBS}"
exec ./kvserv "$@"
