/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief Serializable movement/stamina snapshot of a player (top-level "movement" key).
    \details
        Filled by PlayerBase.LogZ_Snapshot (WP-5). Stamina configuration comes from the
        server's cfggameplay.json, so the analyzer must compare against the values
        emitted here, never against vanilla constants.
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
	bool allow_damage; // Object.GetAllowDamage(): false = godmode, visible without the player acting

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

	/**
	    \brief Sample the player's current movement, stamina and server stamina config.
	*/
	void LogZ_DTO_Movement(PlayerBase player)
	{
		HumanMovementState state = new HumanMovementState();
		player.GetMovementState(state);
		stance = state.m_iStanceIdx;
		movement = state.m_iMovement;
		lean = state.m_fLeaning;

		// not player.IsSprinting(): it reads the cached m_MovementState, which is not refreshed for
		// a remote player on the server (livonia: always 0 while movement was sprint)
		is_sprinting = (state.m_iMovement == DayZPlayerConstants.MOVEMENT_SPRINT);
		sprint_full = player.IsSprintFull();
		velocity = GetVelocity(player);
		allow_damage = player.GetAllowDamage();

		stamina = player.GetStatStamina().Get();
		StaminaHandler handler = player.GetStaminaHandler();
		if (handler)
			stamina_cap = handler.GetStaminaCap();

		stamina_max = CfgGameplayHandler.GetStaminaMax();
		stamina_min_cap = CfgGameplayHandler.GetStaminaMinCap();
		sprint_drain_erc = CfgGameplayHandler.GetSprintStaminaModifierErc();
		sprint_drain_cro = CfgGameplayHandler.GetSprintStaminaModifierCro();
		cfg_gameplay_enabled = g_Game.ServerConfigGetInt("enableCfgGameplayFile");
		cfg_gameplay_version = CfgGameplayHandler.m_Data.version;

		PlayerIdentity identity = player.GetIdentity();
		if (identity) {
			ping_avg = identity.GetPingAvg();
			ping_max = identity.GetPingMax();
		}
	}
}
#endif
