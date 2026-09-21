# AGENTS.md — working agreement for the logz repository

This file configures any agent (or human) working on **logz**, the DayZ
server-side mod that emits structured NDJSON log lines. The workspace
context — the pipeline, the sibling repositories, and the rule that logz
and logz-analyzer change together — lives in
[`../AGENTS.md`](../AGENTS.md); read it first. This file is the
logz-specific part.

---

## 0. Identity

- **Project:** LogZ — NDJSON structured logger for DayZ servers. Server-side
  mod (`-servermod=@LogZ`), no client key required. Upstream:
  <https://github.com/WoozyMasta/logz> (this checkout is a clone; commits here
  are by WoozyMasta upstream — do not rewrite upstream history).
- **Licence:** GPL-3.0-or-later, copyright WoozyMasta. Every `.c` file starts
  with the SPDX header and `#ifdef SERVER` as the first code line —
  `tools/validate.sh` enforces both, plus ASCII-only sources.
- **Status:** BETA. The log structure, config schema and internal API are
  subject to change without backward compatibility. The schema string is
  `logz-v1beta` (`Constants.c: SCHEMA_VERSION`).

## 1. Layout

```
config.cpp              mod definition (CfgMods/CfgPatches, script module paths)
scripts/3_Game/LogZ/    config, DTOs, logger core (Log, Level, Sink, Event)
scripts/4_World/LogZ/   entity hooks (PlayerBase, Weapon_Base, CarScript, ...)
                        and WorldLogger
scripts/5_Mission/LogZ/ MissionServer / ColletorLogZ
tools/                  validation, doc generation, formatting, build
```

Vanilla classes being modded live in `../dayz-sources/scripts/` — read them
there before changing a hook; never edit or copy from that tree into a commit.

## 2. Conventions (enforced by tools)

- Every source file: SPDX header, then `#ifdef SERVER` as the first code line,
  ASCII only. Check with `bash tools/validate.sh` (see the caveat below).
- Formatting: astyle with `.astylerc` (K&R style, tabs, `--pad-oper`,
  `--pad-comma`, `--pad-header`, `--squeeze-lines=2`, `--remove-braces`).
  `astyle` is not installed on this machine — install it (`apt install
  astyle`) before reformatting, or leave formatting to upstream's CI.
- Docs are generated, not hand-written: `CONFIG.md` is produced by
  `tools/config.sh` from `Config/DTO.c` and `Logger/Event.c`; `METRICS.md` by
  `tools/metrics.sh`. When you add a config option or event, regenerate the
  docs rather than editing them by hand.
- `tools/update-contstants.sh` stamps `VERSION`/`COMMIT_SHA`/`BUILD_DATE` into
  `Constants.c` before a build (`true` = real values, `false` = reset to dev
  placeholders). Never commit the stamped values — the tree must stay clean
  after a build round trip.

**Tool caveat:** the files in `tools/` are committed non-executable (mode
100644), so run them via `bash tools/<script>.sh`. `validate.sh` invokes
`tools/first_code_line.awk` directly and fails with "Permission denied" on this
checkout — run `awk -f tools/first_code_line.awk` yourself, or `chmod +x` the
`.awk` files locally (never commit the mode change).

## 3. Building the PBO (verified on this machine)

The upstream `tools/build.sh` targets Windows AddonBuilder. On this Linux
machine, build with dayz-dev-tools instead:

```sh
cd <workspace>
pbo <output>/logz.pbo -C <workspace> logz/config.cpp logz/scripts logz/LICENSE
```

- The `logz/` path prefix inside the PBO is required — `config.cpp` registers
  script modules as `logz/scripts/3_game` etc., and the game resolves them
  relative to the PBO root.
- Install the result as `<servermod-dir>/@LogZ/addons/logz.pbo`, matching
  upstream's build layout (`@logz/addons/logz.pbo`) and the `-servermod=@LogZ`
  launch parameter from the README.
- `pbo` is provided by dayz-dev-tools, installed from the local checkout:
  `uv tool install <workspace>/dayz-dev-tools`. `unpbo --list` verifies the
  archive contents.
- On Linux the PBO ships `config.cpp` unconverted (dayz-dev-tools only
  converts to `config.bin` when the Windows DayZ Tools registry key is
  present). Working server mods shipping `config.cpp` exist, so this is fine;
  if a server ever refuses to load the mod, convert with the Windows DayZ
  Tools first.

## 4. Changing the log output — the analyzer contract

The emitted log line is a contract with logz-analyzer
(`../logz-analyzer/`, see its `AGENTS.md`). Before merging any change to:

- `scripts/3_Game/LogZ/Logger/Event.c` (event types / `event_type` strings)
- `scripts/3_Game/LogZ/DTO/*.c` (DTO fields)
- `scripts/3_Game/LogZ/Config/DTO.c` (throttling, thresholds, filters — these
  change what the data means)
- `Constants.c` (`SCHEMA_VERSION`)

…update logz-analyzer in the same task: its `models.py` normalizer, the
detectors in `analyze.py`, and the schema documentation in
`IMPLEMENTATION_PLAN.md`. A logz change without the analyzer update is not
done. When in doubt about how a field is consumed, grep logz-analyzer for the
field name first.

## 4a. Server-side only — never call client-side functions

logz runs on the dedicated server (`-servermod=@LogZ`, every file
`#ifdef SERVER`). **Never call or hook a client-side function.** Before
hooking any vanilla method, verify in `../dayz-sources/scripts/` that it
executes server-side:

- Check for `g_Game.IsServer()` / `IsDedicatedServer()` guards,
  `#ifndef SERVER` blocks, and the full caller chain. A method whose only
  callers early-return on dedicated servers is client-only — e.g.
  `DayZPlayerInventory.OnInventoryFailure` (its caller
  `OnInventoryJunctureFailureFromServer` returns early at
  `dayzplayerinventory.c:571`; the server-side surface is the
  `ProcessInputData` → `Validate*` path instead).
- Engine-invoked callbacks whose execution side cannot be proven from the
  sources get a live-verification flag before the hook is relied on.
- Client-side data-enrichment candidates are collected in
  `../IMPLEMENTATION_PLAN.md` Appendix A — reference only, never implemented
  here.

## 5. Git workflow

- Branch off `master` before editing (`feat/…`, `fix/…`, `docs/…`). Do not
  commit directly to `master` except for trivial single-file doc fixes.
- Commit messages: imperative subject, conventional-commit style prefix
  (`feat:`, `fix:`, `ci:`, `docs:`) as upstream does.
- Never commit: generated doc state mid-build, stamped `Constants.c` values,
  `tags`, build artefacts.
- Pushing to `origin` (WoozyMasta's repo) requires explicit user approval —
  this is an upstream clone, not our remote.
