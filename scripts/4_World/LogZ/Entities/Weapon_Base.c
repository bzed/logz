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

		int time = g_Game.GetTime();
		if ((time - m_LogZ_LastFireTime) < LogZ_Config.Get().throttling.weapon_fire_ms)
			return;

		m_LogZ_LastFireTime = time;

		LogZ_GameLogger.WithObject(
		    this, "weapon shoot",
		    LogZ_Level.DEBUG, LogZ_Event.PLAYER_ACTIVITY, "", true);
	}

	// Authoritative per-shot event, independent of whether the shot hits anything.
	// EEFired is engine-invoked with no script callers, so that the server reaches it for
	// remote players' shots is unverified until a live server logs it (plan section 5).
	override void EEFired(int muzzleType, int mode, string ammoType)
	{
		super.EEFired(muzzleType, mode, ammoType);

		if (!LogZ_Config.IsLoaded())
			return;

		int intervalMs = LogZ_Config.Get().throttling.weapon_fire_event_ms;
		if (intervalMs > 0) {
			int time = g_Game.GetTime();
			if ((time - m_LogZ_LastFireEventTime) < intervalMs)
				return;

			m_LogZ_LastFireEventTime = time;
		}

		LogZ_WorldLogger.WithWeaponFire(this, muzzleType, mode, ammoType);
	}
}
#endif
