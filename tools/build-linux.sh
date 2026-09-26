#!/usr/bin/env bash
# Build the logz PBOs on Linux with dayz-dev-tools (`pbo`); see AGENTS.md section 3.
# Output: build/@LogZ/addons/logz.pbo and logz_expansion.pbo (hooks for DayZ Expansion, inert
# without it) -- copy the @LogZ folder to the server as a servermod.
set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# The PBO needs a "logz" prefix header with the files at the archive root (what AddonBuilder
# writes with -prefix=logz): config.cpp registers script modules as logz/scripts/3_game and the
# engine resolves them through the prefix. Without the header (files under a logz/ folder
# instead) the mod's config is read but NONE of its scripts load, silently.
out="$repo/build/@LogZ/addons/logz.pbo"
out_expansion="$repo/build/@LogZ/addons/logz_expansion.pbo"

mkdir -p "$(dirname "$out")"
rm -f "$out" "$out_expansion"
pbo -H prefix=logz "$out" -C "$repo" config.cpp scripts LICENSE 2>/dev/null
unpbo --list "$out" | tail -n 3
echo "Built $out"

# The Expansion hooks are a second addon: its CfgPatches requires Expansion's addons, which
# makes its modded classes compile after Expansion's. Without Expansion the server only warns
# ("requires addon") and the #ifdef'd scripts compile to nothing.
pbo -H prefix=logz_expansion "$out_expansion" -C "$repo/expansion" config.cpp scripts 2>/dev/null
unpbo --list "$out_expansion" | tail -n 3
echo "Built $out_expansion"
