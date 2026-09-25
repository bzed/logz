/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief Projectile stop callbacks (WP-11), logged only when filters.projectile_events is set.
    \details
        The engine calls these on both sides (the vanilla arrow spawning in them is unguarded);
        LogZ_GameLogger.WithProjectile returns unless this is the dedicated server. The log call
        comes first so a failure in the vanilla body cannot lose the line. A modded class has to
        live in the module of the original (DayZGame is 3_Game), so this file cannot use 4_World
        types such as PlayerBase.
*/
modded class DayZGame
{
	override void OnProjectileStopped(ProjectileStoppedInfo info)
	{
		LogZ_GameLogger.WithProjectile(info, "projectile stopped", null, -1, false);

		super.OnProjectileStopped(info);
	}

	override void OnProjectileStoppedInTerrain(TerrainCollisionInfo info)
	{
		if (info)
			LogZ_GameLogger.WithProjectile(info, "projectile hit terrain", null, -1, info.GetIsWater());

		super.OnProjectileStoppedInTerrain(info);
	}

	override void OnProjectileStoppedInObject(ObjectCollisionInfo info)
	{
		if (info)
			LogZ_GameLogger.WithProjectile(info, "projectile hit object", info.GetHitObj(), info.GetComponentIndex(), false);

		super.OnProjectileStoppedInObject(info);
	}
}
#endif
