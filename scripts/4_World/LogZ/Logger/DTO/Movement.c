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

	// Vitals beyond the health/blood/shock of the player DTO (PlayerStat), so an admin-set value
	// shows as a jump between two snapshots
	float water;
	float energy;
	float heat_comfort;
	float heat_buffer;
	float wet;
	float toxicity;
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

		// Not player.IsSprinting(), and not DayZPlayerConstants.MOVEMENT_SPRINT: m_iMovement holds the
		// movement *index* (MOVEMENTIDX_*: 0 idle, 1 walk, 2 run, 3 sprint), while MOVEMENT_SPRINT is a
		// mask constant with another value, which is what vanilla IsSprinting() compares against
		// (livonia and onlyup: 0 on every snapshot, including tier 3 at 6.7 m/s).
		is_sprinting = (state.m_iMovement == DayZPlayerConstants.MOVEMENTIDX_SPRINT);
		sprint_full = player.IsSprintFull();
		velocity = GetVelocity(player);
		allow_damage = player.GetAllowDamage();

		stamina = player.GetStatStamina().Get();
		water = player.GetStatWater().Get();
		energy = player.GetStatEnergy().Get();
		heat_comfort = player.GetStatHeatComfort().Get();
		heat_buffer = player.GetStatHeatBuffer().Get();
		wet = player.GetStatWet().Get();
		toxicity = player.GetStatToxicity().Get();
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
