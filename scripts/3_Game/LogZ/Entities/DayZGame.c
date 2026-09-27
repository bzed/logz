/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief Projectile stop callbacks (WP-11, logged only when filters.projectile_events is set) and
        hit claims (WP-14).
    \details
        The engine calls these on both sides (the vanilla arrow spawning in them is unguarded);
        LogZ_GameLogger.WithProjectile / WithClaim return unless this is the dedicated server. The
        log call comes first so a failure in the vanilla body cannot lose the line. A modded class
        has to live in the module of the original (DayZGame is 3_Game), so this file cannot use
        4_World types such as PlayerBase.

        FirearmEffects and CloseCombatEffects are plain script methods that native hit processing
        invokes with the claim's data; their IsServer() branches act on it (gas zone and explosion
        for 40mm ammo, AI noise). Every claim the server processes passes here, including hits on
        foliage, ground and loot that never reach EEHitBy.
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

	override void FirearmEffects(Object source, Object directHit, int componentIndex, string surface, vector pos, vector surfNormal, vector exitPos, vector inSpeed, vector outSpeed, bool isWater, bool deflected, string ammoType)
	{
		LogZ_GameLogger.WithClaim("firearm claim", source, directHit, componentIndex, surface, pos, surfNormal, exitPos, inSpeed, outSpeed, isWater, deflected, ammoType, true);

		super.FirearmEffects(source, directHit, componentIndex, surface, pos, surfNormal, exitPos, inSpeed, outSpeed, isWater, deflected, ammoType);

		LogZ_GameLogger.EndClaim();
	}

	override void CloseCombatEffects(Object source, Object directHit, int componentIndex, string surface, vector pos, vector surfNormal, bool isWater, string ammoType)
	{
		LogZ_GameLogger.WithClaim("melee claim", source, directHit, componentIndex, surface, pos, surfNormal, vector.Zero, vector.Zero, vector.Zero, isWater, false, ammoType, false);

		super.CloseCombatEffects(source, directHit, componentIndex, surface, pos, surfNormal, isWater, ammoType);
	}
}
#endif
