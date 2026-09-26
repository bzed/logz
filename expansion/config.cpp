/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

// LogZ hooks for DayZ Expansion BaseBuilding (code locks), a separate addon so that its scripts
// compile after Expansion's: a modded class can only override methods that an earlier addon
// defined, and CfgPatches requiredAddons is what orders addons. LogZ itself never depends on
// Expansion; the scripts are also wrapped in #ifdef EXPANSIONMODBASEBUILDING.

class CfgMods
{
	class LogZ_Expansion
	{
		type = "mod";
		dir = "logz_expansion";
		name = "LogZ - DayZ Expansion hooks";
		credits = "WoozyMasta";
		author = "WoozyMasta";
		hideName = 1;
		hidePicture = 1;
		dependencies[] = {"world"};

		class defs
		{
			class worldScriptModule
			{
				files[] = { "logz_expansion/scripts/4_world" };
			};
		};
	};
};

class CfgPatches
{
	class LogZ_Expansion_BaseBuilding
	{
		requiredAddons[] = {
			"LogZ",
			"DayZExpansion_BaseBuilding_Scripts",
		};
	};
};
