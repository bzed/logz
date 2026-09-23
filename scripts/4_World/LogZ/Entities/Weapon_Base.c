/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2025 WoozyMasta
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief Updates MetricZ counters on spawn/cleanup.
*/
modded class Weapon_Base
{
	int m_LogZ_LastFireTime;
	int m_LogZ_LastFireEventTime;

	override void OnFire(int muzzle_index)
	{
		super.OnFire(muzzle_index);

		if (!LogZ_Config.IsLoaded())
			return;

		LogZ_FireEvent(muzzle_index);
		LogZ_FireActivity();
	}

	// Authoritative per-shot event, independent of whether the shot hits anything.
	// Hooked on OnFire, not EEFired: WeaponFire.OnEntry (weaponfire.c) calls OnFire after
	// TryFireWeapon succeeded, and that state runs on the server (its OnEntry has
	// g_Game.IsServer() branches). EEFired has no script callers and its script body is
	// client-only effects, so nothing proves the engine delivers it to a dedicated server.
	protected void LogZ_FireEvent(int muzzle_index)
	{
		int intervalMs = LogZ_Config.Get().throttling.weapon_fire_event_ms;
		if (intervalMs > 0) {
			int time = g_Game.GetTime();
			if ((time - m_LogZ_LastFireEventTime) < intervalMs)
				return;

			m_LogZ_LastFireEventTime = time;
		}

		LogZ_WorldLogger.WithWeaponFire(this, muzzle_index);
	}

	// Throttled DEBUG activity event, kept as it was before the fire events existed.
	protected void LogZ_FireActivity()
	{
		int time = g_Game.GetTime();
		if ((time - m_LogZ_LastFireTime) < LogZ_Config.Get().throttling.weapon_fire_ms)
			return;

		m_LogZ_LastFireTime = time;

		LogZ_GameLogger.WithObject(
		    this, "weapon shoot",
		    LogZ_Level.DEBUG, LogZ_Event.PLAYER_ACTIVITY, "", true);
	}
}
#endif
