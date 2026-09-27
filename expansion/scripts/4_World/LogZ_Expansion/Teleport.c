/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
#ifdef EXPANSIONMODCORE
/**
    \brief LogZ hook for `Expansion_Teleport` (DayZ Expansion Core), compiled only when Expansion
        Core is loaded.
    \details
        Every Expansion-driven relocation of a player's character - the spawn-selection menu
        (`ExpansionRespawnHandlerModule.Exec_SelectSpawn`) and the standalone Teleporter module
        (`ExpansionTeleporterTriggerBase.Expansion_Teleport`) - calls this one method on the
        player, so a single hook here covers both instead of two separate, harder-to-find call
        sites (planned as WP-15 in the analyzer plan, "Expansion spawn-selector telemetry").
        `Expansion_Teleport` itself is `SetPosition()`/`SetOrientation()` only, with no logging of
        its own, so without this hook logz cannot tell such a jump apart from an actual position
        exploit - it can only guess from surrounding timing and activity, as a livonia finding
        (2026-09-27, `zantonator`) had to.
*/
modded class DayZPlayerImplement
{
	override void Expansion_Teleport(vector position, vector orientation = "0 0 0")
	{
		PlayerBase player = PlayerBase.Cast(this);
		vector fromPos;

		if (player)
			fromPos = player.GetPosition();

		super.Expansion_Teleport(position, orientation);

		if (!player || !LogZ_Levels.IsEnabled(LogZ_Level.INFO) || !LogZ_Events.IsEnabled(LogZ_Event.EXPANSION_TELEPORT))
			return;

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (LogZ_GameLogger.SerializeObject(player, json))
			dto.Insert("object", json);

		// the object's own "pos" (from SerializeObject above) is the destination, set by super()
		// just above; from_pos is where the character stood the instant before this call
		dto.Insert("from_pos", string.Format("[%1,%2,%3]", fromPos[0], fromPos[1], fromPos[2]));

		LogZ.Log("expansion teleport", LogZ_Level.INFO, LogZ_Event.EXPANSION_TELEPORT, dto);
	}
}
#endif
#endif
