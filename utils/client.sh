#!/usr/bin/env bash
set -euo pipefail

# Run the Python client load generator.
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

python3 client/runner.py "$@"
