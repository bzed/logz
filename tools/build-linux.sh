#!/usr/bin/env bash
# Build the logz PBO on Linux with dayz-dev-tools (`pbo`); see AGENTS.md section 3.
# Output: build/@LogZ/addons/logz.pbo -- copy the @LogZ folder to the server as a servermod.
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# The PBO prefix must be "logz/", so pack relative to the directory that holds the repo.
root="$(dirname "$repo")"
name="$(basename "$repo")"
out="$repo/build/@LogZ/addons/logz.pbo"

if [ "$name" != "logz" ]; then
  echo "expected the repository directory to be named 'logz', got '$name'" >&2
  exit 1
fi

mkdir -p "$(dirname "$out")"
rm -f "$out"
pbo "$out" -C "$root" "$name/config.cpp" "$name/scripts" "$name/LICENSE"
unpbo --list "$out" | tail -n 3
echo "Built $out"
