/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief Serializable movement/stamina snapshot of a player (top-level "movement" key).
    \details
        Contract-only until the player snapshot sampler lands (WP-5); nothing
        serializes this class yet. Stamina configuration comes from the server's
        cfggameplay.json, so the analyzer must compare against the values emitted
        here, never against vanilla constants.
*/
class LogZ_DTO_Movement
{
	int stance; // HumanMovementState m_iStanceIdx
	int movement; // HumanMovementState m_iMovement: 0 idle, 1 walk, 2 run, 3 sprint
	float lean; // HumanMovementState m_fLeaning
	bool is_sprinting;
	bool sprint_full; // DayZPlayerImplement m_SprintFull
	float stamina;
	float stamina_cap;
	vector velocity;

	// Server stamina configuration (CfgGameplayHandler)
	float stamina_max;
	float stamina_min_cap;
	float sprint_drain_erc; // GetSprintStaminaModifierErc()
	float sprint_drain_cro; // GetSprintStaminaModifierCro()
	int cfg_gameplay_enabled; // ServerConfigGetInt("enableCfgGameplayFile")
	int cfg_gameplay_version; // CfgGameplayHandler.m_Data.version, -1 = vanilla defaults

	// Connection quality (PlayerIdentity)
	int ping_avg;
	int ping_max;
}
#endif
