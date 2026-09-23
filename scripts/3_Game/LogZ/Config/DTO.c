/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2025 WoozyMasta
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief LogZ Config DTO
*/
class LogZ_ConfigDTO
{
	void LogZ_ConfigDTO()
	{
		settings = new LogZ_ConfigDTO_Settings();
		file = new LogZ_ConfigDTO_File();
		filters = new LogZ_ConfigDTO_Filters();
		throttling = new LogZ_ConfigDTO_Throttling();
		thresholds = new LogZ_ConfigDTO_Thresholds();
		geo = new LogZ_ConfigDTO_Geo();
	}

	// Internal configuration version. **Do not modify**.
	string version = LogZ_Constants.VERSION;

	// General logging settings.
	ref LogZ_ConfigDTO_Settings settings;

	// File output and rotation settings.
	ref LogZ_ConfigDTO_File file;

	// Logic filters for specific events.
	ref LogZ_ConfigDTO_Filters filters;

	// Throttling settings to prevent log spam.
	ref LogZ_ConfigDTO_Throttling throttling;

	// Damage thresholds for hit events.
	ref LogZ_ConfigDTO_Thresholds thresholds;

	// Geographic coordinate settings.
	ref LogZ_ConfigDTO_Geo geo;

	/**
	    \brief Normalizes configuration values within valid ranges.
	*/
	void Normalize()
	{
		settings.Normalize();
		file.Normalize();
		filters.Normalize();
		throttling.Normalize();
		thresholds.Normalize();
		geo.Normalize();
	}
}

class LogZ_ConfigDTO_Settings
{
	// Overrides the Instance ID. By default, this is detected automatically from serverDZ.cfg instanceID,
	// or uses gamePort/steamQueryPort if instanceID is undefined or zero.
	// Used for log file naming.
	string instance_id;

	// Overrides the Host Name. By default, attempts to obtain it automatically.
	// In some environments, reading the host name may be limited and unavailable.
	// You can use this if you want to explicitly override the host name.
	string host_name;

	// Minimum severity level for logging.
	// Values: `trace`, `debug`, `info`, `warn`, `error`, `fatal`, `off`.
	// Default is `info`.
	string level = "info";

	// Event mask configuration.
	// Can be a specific bitmask integer or a string of characters representing event categories.
	// Example: "ABKqz" or "-1" (for all).
	// Letters cover only the first 26 events; `WEAPON_FIRE` and `INVENTORY_FAILURE`
	// have no letter and are enabled through the numeric form only.
	// Existing config files keep their saved value: set "-1" there to enable the new events.
	string events_mask = "-1";

	// Disable send minimal telemetry 10-20 minutes after server startup.
	bool disable_telemetry;

	[NonSerialized()]
	string instance_id_resolved;

	[NonSerialized()]
	string host_name_resolved;

	[NonSerialized()]
	LogZ_Level level_enum;

	[NonSerialized()]
	int events_mask_int;

	/**
	    \brief Normalizes configuration values within valid ranges.
	*/
	void Normalize()
	{
		instance_id_resolved = LogZ_Helpers.GetInstanceID(instance_id);

		if (host_name != string.Empty)
			host_name_resolved = host_name;
		else {
			string host = GetMachineName();
			host.TrimInPlace();
			host.ToLower();
			host_name_resolved = host;
		}

		// Convert string level to enum
		level_enum = LogZ_Levels.FromString(level);

		// Convert string mask to int
		if (LogZ_Utils.HasAlpha(events_mask))
			events_mask_int = LogZ_Events.FromString(events_mask);
		else
			events_mask_int = events_mask.ToInt();

		if (events_mask_int == -1)
			events_mask_int = LogZ_Event.MAX;
		else if (events_mask_int <= LogZ_Event.NONE)
			events_mask_int = LogZ_Event.NONE;
		else if (events_mask_int >= LogZ_Event.MAX)
			events_mask_int = LogZ_Event.MAX;
	}
}

class LogZ_ConfigDTO_File
{
	// Log file name override (without extension).
	// Default file path `$profile:logz/logs/logz_${instance_id}.ndjson`.
	string file_name;

	// Enables append mode.
	// true  - New logs are appended to the existing file.
	// false - The log file is truncated (cleared) on server start.
	bool append;

	// Number of rotated log files to keep.
	// 0 or 1 - Disables rotation.
	// > 1    - Keeps N rotated files (e.g., .0.ndjson, .1.ndjson).
	int rotation_keep = 5;

	// Buffer size (in lines) for file writing.
	// 0 - Disable buffer, write each line immediately (Direct mode).
	// > 0 - Accumulate lines until this limit is reached.
	int buffer_size = 32;

	// Maximum time in seconds to hold lines in the buffer before flushing.
	// Prevents logs from being stuck in memory if the server is quiet.
	int flush_interval = 5;

	[NonSerialized()]
	string full_path;
	[NonSerialized()]
	string base_path;

	/**
	    \brief Normalizes configuration values within valid ranges.
	*/
	void Normalize()
	{
		rotation_keep = Math.Clamp(rotation_keep, 0, 100);
	}
}

class LogZ_ConfigDTO_Filters
{
	// Log `INVENTORY_IN` events only when the item's parent is a player.
	// Reduces spam from item spawning on ground/zombies.
	bool only_player_inventory_input = true;

	// Log `INVENTORY_OUT` events only when the item's parent is a player.
	bool only_player_inventory_output;

	// Log suicide events (killer == victim) only for players.
	// Filters out zombies/animals killing themselves via glitches.
	bool only_player_suicide = true;

	// Skip AI (eAI) players in periodic snapshot events.
	// AI has no `steam_id`; the analyzer ignores it anyway, so this only saves log volume.
	bool skip_ai_snapshots = true;

	/**
	    \brief Normalizes configuration values within valid ranges.
	*/
	void Normalize() {}
}

class LogZ_ConfigDTO_Throttling
{
	// Throttling window in milliseconds for Weapon `OnFire()` events.
	// Prevents logging every single shot for high RPM weapons.
	int weapon_fire_ms = 250;

	// Minimum interval in milliseconds between `WEAPON_FIRE` events per weapon.
	// 0 - log every shot (default), the analyzer needs the true inter-shot intervals.
	int weapon_fire_event_ms = 0;

	// Minimum interval in milliseconds between `INVENTORY_FAILURE` events
	// per player and failure reason. 0 - log every failure.
	int inventory_failure_ms = 1000;

	// Interval in seconds between periodic player movement snapshots.
	// 0 - disabled.
	int player_snapshot_s = 10;

	// Interval in seconds between periodic transport snapshots (only while a driver is present).
	// 0 - disabled.
	int transport_snapshot_s = 15;

	// Minimum interval in milliseconds between item quantity change events per item.
	// 0 - log every change.
	int quantity_change_ms = 500;

	// Minimum interval in milliseconds between projectile stop events per shooter.
	// 0 - log every projectile.
	int projectile_ms = 200;

	/**
	    \brief Normalizes configuration values within valid ranges.
	*/
	void Normalize()
	{
		weapon_fire_ms = Math.Clamp(weapon_fire_ms, 50, 5000);
		weapon_fire_event_ms = Math.Clamp(weapon_fire_event_ms, 0, 5000);
		inventory_failure_ms = Math.Clamp(inventory_failure_ms, 0, 60000);
		player_snapshot_s = Math.Clamp(player_snapshot_s, 0, 3600);
		transport_snapshot_s = Math.Clamp(transport_snapshot_s, 0, 3600);
		quantity_change_ms = Math.Clamp(quantity_change_ms, 0, 60000);
		projectile_ms = Math.Clamp(projectile_ms, 0, 60000);
	}
}

class LogZ_ConfigDTO_Thresholds
{
	// Minimum damage required to log hit events in `EEHitBy()`.
	// Values of -1 or less disable this threshold.
	float hit_damage = 3.0;

	// Minimum damage from vehicles required to log hit events.
	// Values of -1 or less disable this threshold.
	float hit_damage_vehicle = 15.0;

	/**
	    \brief Normalizes configuration values within valid ranges.
	*/
	void Normalize()
	{
		hit_damage = Math.Clamp(hit_damage, -1, 1000);
		hit_damage_vehicle = Math.Clamp(hit_damage_vehicle, -1, 1000);
	}
}

class LogZ_ConfigDTO_Geo
{
	// Overrides the effective map tile size in world units.
	// Useful if the web map size is larger than the game world size.
	// (For example, iZurvive tiles for Chernarus have a size of `15926`, although the world size is `15360`).
	float world_effective_size;

	[NonSerialized()]
	float world_effective_size_resolved;

	[NonSerialized()]
	string world_name;

	/**
	    \brief Normalizes configuration values within valid ranges.
	*/
	void Normalize()
	{
		if (!g_Game)
			return;

		if (world_effective_size != 0) {
			world_effective_size = Math.Clamp(world_effective_size, 1000, 81920);
			world_effective_size_resolved = world_effective_size;
		} else {
			world_effective_size_resolved = g_Game.GetWorld().GetWorldSize();

			g_Game.GetWorldName(world_name);
			world_name.TrimInPlace();
			world_name.ToLower();
		}
	}
}
#endif
