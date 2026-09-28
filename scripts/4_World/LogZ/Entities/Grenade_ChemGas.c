/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief Names the chemical grenade as the creator of the gas zone it makes.
    \details
        Vanilla OnExplode creates ContaminatedArea_Local under IsServer (grenade_chemgas.c:19-26),
        reached from the server-guarded Grenade_Base.OnActivateFinished -> InitiateExplosion. It does
        not call super, so ExplosivesBase's "explosive detonated" line never covers it.
*/
modded class Grenade_ChemGas
{
	override protected void OnExplode()
	{
		LogZ_WorldLogger.BeginZoneOrigin("grenade", this);
		super.OnExplode();
		LogZ_WorldLogger.EndZoneOrigin();
	}
}
#endif
