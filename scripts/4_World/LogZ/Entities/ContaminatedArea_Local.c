/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief Zone audit for the short-lived gas zone (WP-14, plan 2.15).
    \details
        Vanilla creates ContaminatedArea_Local from a 40mm gas claim (DayZGame.FirearmEffects), a
        chemical grenade and a destroyed 40mm pile. The line says where the server really put it
        and which vanilla creator made it (origin: claim, grenade, ammo_pile or unknown; see
        LogZ_WorldLogger.BeginZoneOrigin and, for a claim, LogZ_GameLogger.WithClaim).
*/
modded class ContaminatedArea_Local
{
	override void EEInit()
	{
		super.EEInit();

		LogZ_WorldLogger.WithContaminatedArea(this);
	}
}
#endif
