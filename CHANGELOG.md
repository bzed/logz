# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog][],
and this project adheres to [Semantic Versioning][].

<!--
## Unreleased

### Added
### Changed
### Removed
-->

## Unreleased

### Added

* `CODE_LOCK` event (`code.lock`, numeric mask only) for DayZ Expansion
  BaseBuilding code locks: wrong code, unlocked, code set, code changed (with
  `was_locked`, `known_user`), locked; codes are never logged. Shipped as a
  second addon `logz_expansion.pbo` that is inert without Expansion
* `BASE_BUILDING` event (`base.building`, numeric mask only) for DayZ Expansion
  BaseBuilding: territory create/delete/invite/join/kick/promote/demote/leave,
  object placement (with the distance to the player), base part
  built/dismantled/destroyed, admin hammer builds, raid damage and tool cycles,
  flag dismantle, C4 detonation. Every line carries the territory of the
  object and whether the player is a member of it. Code lock lines now carry
  the lock's territory as well
* extra fields whose key ends in `_uid` are always written as strings (Steam
  ids exceed 2^53 and were emitted as JSON numbers)
* hit claim lines (`firearm claim` / `melee claim`, `system.game`): every hit
  claim the server processes (`DayZGame.FirearmEffects` /
  `CloseCombatEffects`) with its full payload, including hits on foliage,
  ground and loot that never reach `EEHitBy`. A 40mm gas or explosive claim
  whose source is not a launcher in the shooter's hands is WARN and never
  throttled. New `throttling.hit_claim_ms` (0 = every claim)
* `contaminated area` line (`system.world`) when the server creates a gas zone,
  with the claim (`via_claim`, `claim_launcher`, shooter) that caused it
* `LogZ_Json.Token`: attacker-supplied strings (surface, ammo type) are reduced
  to `[A-Za-z0-9_.-]`, cut at 64 characters and kept quoted, so a forged claim
  cannot break a line or inject JSON

### Changed

* refactor string concatenation and loops increment to improve performance

## [0.2.0][] - 2025-12-17

### Added

* buffered file writing system; logs are now accumulated in memory and
  flushed to disk based on size or time interval to reduce disk I/O overhead
* `buffer_size` and `flush_interval` options to control the buffering behavior
* metric **`dayz_metricz_logz_disk_flushes_total_total`** (`COUNTER`) —
  Total number of buffer flushes to disk
* metric **`dayz_metricz_logz_disk_written_bytes_total_total`** (`COUNTER`) —
  Total size of logs written to disk in bytes
* new options `settings.instance_id` and `settings.host_name` for overriding
  autodetected values

### Changed

* migrate configuration to `$profile:logz/config.json` file
* migrate logs storage path to `$profile:logz/logs/`
* log filenames now include `instance_id` by default (e.g., `logz_1.ndjson`)

[0.2.0]: https://github.com/WoozyMasta/logz/compare/0.1.0...0.2.0

## [0.1.0][] - 2025-11-23

### Added

* First public release

[0.1.0]: https://github.com/WoozyMasta/logz/tree/0.1.0

<!--links-->
[Keep a Changelog]: https://keepachangelog.com/en/1.1.0/
[Semantic Versioning]: https://semver.org/spec/v2.0.0.html
