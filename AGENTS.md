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
- **Prefer globals over accessor calls (DayZ 1.29 convention):** always use
  `g_Game`, never `GetGame()` — the global is a variable read, the accessor a
  native call on the hot path. The same applies to the other accessor/global
  pairs: `GetPluginManager()` → `g_Plugins`, `GetDispatcher()` → `g_Dispatcher`
  (both pure accessors in vanilla; `GetPluginManager`'s null-init branch is
  commented out, so it is exactly `return g_Plugins`). `validate.sh` fails on
  `GetGame()`. Note: `GetParticleManager()` is a `proto native` method on
  `ParticleSource`, not a global accessor — it is not covered by this rule.
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

`bash tools/build-linux.sh` runs the command below and writes
`build/@LogZ/addons/logz.pbo` (git-ignored) — copy the `@LogZ` folder to the server.
The manual form:

```sh
cd <repo>   # the logz checkout
pbo -H prefix=logz <output>/logz.pbo -C . config.cpp scripts LICENSE
```

- **The `prefix=logz` header is required, and the files sit at the archive root.**
  `config.cpp` registers script modules as `logz/scripts/3_game` etc., and the engine
  resolves them through the PBO prefix (what AddonBuilder writes with `-prefix="logz"`).
  Verified on a local dedicated server 2026-09-23: packing the files under a `logz/`
  folder *without* the header makes the server read the mod's config (the `LOGZ` define
  appears) but load **none** of its scripts, silently — no error, the Game module stays
  at the vanilla 416 files / 1379 classes; with the header it loads 437 / 1418.
  Path case does not matter (a fully lowercased tree behaved the same as mixed case).
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

### 3a. Testing on the local dedicated server (headless, verified 2026-09-26)

The Steam-installed DayZ Server (`~/.steam/debian-installation/steamapps/common/DayZServer`,
found by the dayz-dev skill's `scripts/find-dayzserver.sh`) is **never written to**: no copied
mods, no edited configs, no profiles, no crash dumps, no mission persistence. Run the server
from our own tree, `~/workspace/dayz/testserver`, which symlinks into the Steam install and is
built or refreshed (after a Steam update) with the skill's `scripts/make-server-tree.sh`. The
tree has its own `serverDZ.cfg`, `profiles/`, and mission directories of per-file symlinks, so
`storage_1/` persistence stays in the tree. The mod is a symlink to the build output, so a
rebuild needs no copy step:

```sh
bash tools/build-linux.sh
ln -sfn "$PWD/build/@LogZ" ~/workspace/dayz/testserver/@LogZ     # once
cd ~/workspace/dayz/testserver && ulimit -c 0 && timeout -k 15 60 ./DayZServer \
    -config=serverDZ.cfg -profiles=profiles -servermod=@LogZ -port=2402 -nosplash -nopause -dologs
```

- Mod paths (`-servermod=`, `-mod=`) must be **relative** to the server directory. An absolute
  path is silently ignored: the server boots vanilla, with no error. Other mods, such as
  workshop items, are symlinked into the tree as `@Name` the same way (see the skill's
  `testing/local-server.md`).
- The server is ready in seconds (landscape ~1.3 s, mission ~8 s) and then idles with no
  players. A quiet script log is not a stall.
- Keep `-k 15`. After a clean shutdown the process sometimes hangs in Steam API threads, and
  only the SIGKILL ends it. In `ps` it shows up as `enfMain`.
- Success is visible in `profiles/script_*.log`: the Game module must report more than the
  vanilla 416 files, and `LogZ: loaded ...` must appear; output lands in `profiles/logz/logs/`.
  `SCRIPT    (E)` lines are compile errors (grep `'SCRIPT.*(E)'`; the tag is space-padded, so a
  literal `SCRIPT (E)` never matches). Only a server boot proves a `modded class`/`override`
  compiles: engine classes (`DayZPlayerInventory`) cannot be modded and `proto native`
  methods (`SendSyncJuncture`) cannot be overridden, and `validate.sh` accepts both. Do not `pkill -f DayZServer` from the same shell
  command line (it matches itself).
- There is no client, so hooks driven by players need a scratch copy of the mod with a
  debug call (spawn `g_Game.CreatePlayer(null, ...)`, `CreateInHands`, call the method).
  Keep such copies outside the repo. A player without identity is skipped by the AI filters.

**Required, not optional:** every logz change is verified this way before it is called done
(see `../AGENTS.md`, "Verify logz changes on the local dedicated server"). Debug `Print`
statements in scratch copies are fine.

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
