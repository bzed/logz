/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief Names a 40mm gas pile as the creator of the gas zone it makes.
    \details
        Vanilla creates ContaminatedArea_Local when the pile is set off by an item (remote detonator,
        clock, tripwire; under IsServer) and when it is destroyed (EEKilled), ammunitionpiles.c:214-231.
*/
modded class Ammo_40mm_ChemGas
{
	override void OnActivatedByItem(notnull ItemBase item)
	{
		LogZ_WorldLogger.BeginZoneOrigin("ammo_pile", this, item);
		super.OnActivatedByItem(item);
		LogZ_WorldLogger.EndZoneOrigin();
	}

	override void EEKilled(Object killer)
	{
		LogZ_WorldLogger.BeginZoneOrigin("ammo_pile", this, killer);
		super.EEKilled(killer);
		LogZ_WorldLogger.EndZoneOrigin();
	}
}
#endif
