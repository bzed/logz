/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2025 WoozyMasta
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief High-level world logger helpers.
*/
class LogZ_WorldLogger
{
	/**
	    \brief Log full player snapshot with optional message.
	    \param player    PlayerBase subject.
	    \param msg       Message to log (optional).
	    \param lvl       Log level (default INFO).
	    \param eventType Event type (must be enabled).
	*/
	static void WithPlayer(PlayerBase player, string msg = "", LogZ_Level lvl = 2, LogZ_Event eventType = 0)
	{
		if (!player || !LogZ_Levels.IsEnabled(lvl) || !LogZ_Events.IsEnabled(eventType))
			return;

		if (msg == string.Empty)
			msg = player.ClassName();

		ref map<string, string> dto = new map<string, string>();
		string json;

		LogZ_DTO_Player playerDTO = new LogZ_DTO_Player(player);
		if (LogZ.GetSerializer().WriteToString(playerDTO, false, json))
			dto.Insert("player", json);

		LogZ.Log(msg, lvl, eventType, dto);
	}

	/**
	    \brief Log kill/death event with victim and killer context.
	    \param victim Victim object.
	    \param killer Attacker object or null.
	    \param lvl    Log level (default INFO).
	*/
	static void WithKiller(Object victim, Object killer, LogZ_Level lvl = 2)
	{
		if (!LogZ_Config.IsLoaded() || !victim || !LogZ_Levels.IsEnabled(lvl))
			return;

		if (LogZ_Config.Get().filters.only_player_suicide && killer == victim)
			return;

		LogZ_Event eventType = ResolveVictimEvent(victim, false);
		if (!LogZ_Events.IsEnabled(eventType))
			return;

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (LogZ_GameLogger.SerializeObject(victim, json))
			dto.Insert("victim", json);

		if (LogZ_GameLogger.SerializeParentObject(victim, json))
			dto.Insert("victim_parent", json);

		if (!killer) {
			LogZ.Log(string.Format("%1 died", LogZ_Object.GetType(victim)), lvl, eventType, dto);
			return;
		}

		if (killer == victim) {
			LogZ.Log(string.Format("%1 death or suicide", LogZ_Object.GetType(victim)), lvl, eventType, dto);
			return;
		}

		dto.Insert("distance", LogZ_Utils.Distance(killer, victim));

		EntityAI killerEntity = EntityAI.Cast(killer);

		if (LogZ_GameLogger.SerializeObject(killer, json))
			dto.Insert("attacker", json);

		if (LogZ_GameLogger.SerializeParentObject(killer, json))
			dto.Insert("attacker_parent", json);

		LogZ.Log(string.Format("%1 killed", LogZ_Object.GetType(victim)), lvl, eventType, dto);
	}

	/**
	    \brief Log hit / damage event with victim, attacker and damage payload.
	    \details
	        The caller skips victims whose kill is already logged (LogZ_IsAlreadyKilled), not
	        destroyed ones: the lethal hit arrives with the victim already destroyed.
	    \param victim       Damaged object.
	    \param source       Damage source (EntityAI) or null.
	    \param damageResult TotalDamageResult or null.
	    \param damageType   DamageType enum value.
	    \param component    Hit component index from EEHitBy.
	    \param dmgZone      Damage zone name.
	    \param ammo         Ammo type name.
	    \param modelPos     Hit position in the victim's model space.
	    \param speedCoef    Projectile speed damage coefficient.
	    \param lvl          Log level (default INFO).
	*/
	static void WithHit(Object victim, EntityAI source, TotalDamageResult damageResult, int damageType, int component, string dmgZone, string ammo, vector modelPos, float speedCoef, LogZ_Level lvl = 2)
	{
		if (!LogZ_Config.IsLoaded() || !victim || !LogZ_Levels.IsEnabled(lvl))
			return;

		LogZ_Event eventType = ResolveVictimEvent(victim, true);
		if (!LogZ_Events.IsEnabled(eventType))
			return;

		if (damageResult) {
			float damage = damageResult.GetDamage(dmgZone, "");
			if (damage < LogZ_Config.Get().thresholds.hit_damage)
				return;

			if (source && source.IsTransport() && damage < LogZ_Config.Get().thresholds.hit_damage_vehicle)
				return;
		}

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (LogZ_GameLogger.SerializeObject(victim, json))
			dto.Insert("victim", json);

		if (LogZ_GameLogger.SerializeParentObject(victim, json))
			dto.Insert("victim_parent", json);

		LogZ_DTO_Damage damageDTO = new LogZ_DTO_Damage(damageResult, damageType, dmgZone, ammo, component, modelPos, speedCoef);
		if (LogZ.GetSerializer().WriteToString(damageDTO, false, json))
			dto.Insert("damage", json);

		if (!source) {
			LogZ.Log(string.Format("%1 damaged", LogZ_Object.GetType(victim)), lvl, eventType, dto);
			return;
		}

		if (source == victim) {
			LogZ.Log(string.Format("%1 hit self", LogZ_Object.GetType(victim)), lvl, eventType, dto);
			return;
		}

		dto.Insert("distance", LogZ_Utils.Distance(source, victim));

		if (LogZ_GameLogger.SerializeObject(source, json))
			dto.Insert("attacker", json);

		if (LogZ_GameLogger.SerializeParentObject(source, json))
			dto.Insert("attacker_parent", json);

		LogZ.Log(string.Format("%1 hit", LogZ_Object.GetType(victim)), lvl, eventType, dto);
	}

	/**
	    \brief Log one weapon shot with weapon, shooter and jam/stamina context.
	    \details
	        Same roles as hit events: the weapon is "attacker", the shooter "attacker_parent"
	        (its yaw is the aim direction). Skips shooters without an identity (eAI) when
	        filters.skip_ai_weapon_fire is set. chance_to_jam is only meaningful above zero:
	        bolt-actions, single-shots and archery can never jam.
	    \param weapon Fired weapon.
	    \param muzzle Muzzle index from Weapon_Base.OnFire.
	*/
	static void WithWeaponFire(Weapon_Base weapon, int muzzle)
	{
		if (!LogZ_Config.IsLoaded() || !weapon || !LogZ_Levels.IsEnabled(LogZ_Level.INFO) || !LogZ_Events.IsEnabled(LogZ_Event.WEAPON_FIRE))
			return;

		PlayerBase shooter = PlayerBase.Cast(weapon.GetHierarchyRootPlayer());
		if (!shooter)
			return;

		if (LogZ_Config.Get().filters.skip_ai_weapon_fire && !shooter.GetIdentity())
			return;

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (LogZ_GameLogger.SerializeObject(weapon, json))
			dto.Insert("attacker", json);

		if (LogZ_GameLogger.SerializeObject(shooter, json))
			dto.Insert("attacker_parent", json);

		dto.Insert("muzzle", muzzle.ToString());
		dto.Insert("mode", weapon.GetCurrentModeName(muzzle));
		dto.Insert("mode_index", weapon.GetCurrentMode(muzzle).ToString());
		dto.Insert("burst_count", weapon.GetBurstCount().ToString());
		dto.Insert("stamina", shooter.GetStatStamina().Get().ToString());
		dto.Insert("chance_to_jam", weapon.GetSyncChanceToJam().ToString());

		if (weapon.IsJammed())
			dto.Insert("is_jammed", "1");
		else
			dto.Insert("is_jammed", "0");

		LogZ.Log("weapon fired", LogZ_Level.INFO, LogZ_Event.WEAPON_FIRE, dto);
	}

	/**
	    \brief Log a periodic movement/stamina snapshot of a player (WP-5).
	    \details
	        Wire event is PLAYER_ACTIVITY (no free enum bit) with msg "player snapshot"; the
	        player is the "player" key, the sample the "movement" key. The caller throttles.
	*/
	static void WithPlayerSnapshot(PlayerBase player)
	{
		if (!LogZ_Levels.IsEnabled(LogZ_Level.INFO) || !LogZ_Events.IsEnabled(LogZ_Event.PLAYER_ACTIVITY))
			return;

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (LogZ_GameLogger.SerializeObject(player, json))
			dto.Insert("player", json);

		LogZ_DTO_Movement movement = new LogZ_DTO_Movement(player);
		if (LogZ.GetSerializer().WriteToString(movement, false, json))
			dto.Insert("movement", json);

		LogZ.Log("player snapshot", LogZ_Level.INFO, LogZ_Event.PLAYER_ACTIVITY, dto);
	}

	/**
	    \brief Log a periodic transport snapshot (WP-6).
	    \details
	        Wire event is SYSTEM_WORLD (no free enum bit) with msg "transport snapshot"; the
	        LogZ_DTO_TransportState is the "object". The caller throttles and checks the driver.
	*/
	static void WithTransportSnapshot(CarScript car)
	{
		if (!LogZ_Levels.IsEnabled(LogZ_Level.INFO) || !LogZ_Events.IsEnabled(LogZ_Event.SYSTEM_WORLD))
			return;

		ref map<string, string> dto = new map<string, string>();
		string json;

		LogZ_DTO_TransportState state = new LogZ_DTO_TransportState(car);
		if (LogZ.GetSerializer().WriteToString(state, false, json))
			dto.Insert("object", json);

		LogZ.Log("transport snapshot", LogZ_Level.INFO, LogZ_Event.SYSTEM_WORLD, dto);
	}

	/**
	    \brief Log a session-forensics line for a player the server kills at logout or respawn (WP-7).
	    \details
	        Same payload as WithObject plus "unconscious" and "restrained" (1/0) and, when given,
	        a top-level "kick_reason".
	    \param player     Player being killed by the server.
	    \param msg        Message string.
	    \param kickReason EClientKicked name, or empty to omit (respawn has none).
	*/
	static void WithSessionKill(PlayerBase player, string msg, string kickReason)
	{
		if (!player || !LogZ_Levels.IsEnabled(LogZ_Level.INFO) || !LogZ_Events.IsEnabled(LogZ_Event.PLAYER_SESSION))
			return;

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (LogZ_GameLogger.SerializeObject(player, json))
			dto.Insert("object", json);

		if (kickReason != "")
			dto.Insert("kick_reason", kickReason);

		if (player.IsUnconscious())
			dto.Insert("unconscious", "1");
		else
			dto.Insert("unconscious", "0");

		if (player.IsRestrained())
			dto.Insert("restrained", "1");
		else
			dto.Insert("restrained", "0");

		LogZ.Log(msg, LogZ_Level.INFO, LogZ_Event.PLAYER_SESSION, dto);
	}

	/**
	    \brief Log action start/end with attached context.
	    \param action_data ActionData instance.
	    \param isStart     True for start, false for end.
	    \param lvl         Log level.
	*/
	static void WithActionData(ActionData action_data, bool isStart, LogZ_Level lvl)
	{
		if (!action_data || !action_data.LogZ_IsAllowed() || !LogZ_Levels.IsEnabled(lvl))
			return;

		LogZ_Event eventType;
		string msg;

		if (isStart) {
			eventType = LogZ_Event.ACTION_START;
			msg = "action start";
		} else {
			eventType = LogZ_Event.ACTION_END;
			msg = "action end";
		}

		ref map<string, string> dto = new map<string, string>();
		string json;

		LogZ_DTO_ActionData actionDTO = new LogZ_DTO_ActionData(action_data);
		if (LogZ.GetSerializer().WriteToString(actionDTO, false, json))
			dto.Insert("action_data", json);

		if (action_data.m_Player) {
			if (LogZ_GameLogger.SerializeObject(action_data.m_Player, json, true))
				dto.Insert("player", json);
		}

		if (action_data.m_MainItem) {
			if (LogZ_GameLogger.SerializeObject(action_data.m_MainItem, json, true))
				dto.Insert("item", json);
		}

		if (action_data.m_Target) {
			Object targetObj = action_data.m_Target.GetObject();
			if (targetObj && targetObj != action_data.m_MainItem) {
				if (action_data.m_MainItem)
					dto.Insert("distance", LogZ_Utils.Distance(targetObj, action_data.m_MainItem));
				else if (action_data.m_Player)
					dto.Insert("distance", LogZ_Utils.Distance(targetObj, action_data.m_Player));

				if (LogZ_GameLogger.SerializeObject(targetObj, json))
					dto.Insert("target", json);
			}

			Object targetParentObj = action_data.m_Target.GetParent();
			if (targetParentObj && targetParentObj != action_data.m_Player) {
				if (!targetObj && action_data.m_MainItem)
					dto.Insert("distance", LogZ_Utils.Distance(targetParentObj, action_data.m_MainItem));
				else if (!targetObj && action_data.m_Player)
					dto.Insert("distance", LogZ_Utils.Distance(targetParentObj, action_data.m_Player));

				if (LogZ_GameLogger.SerializeObject(targetParentObj, json))
					dto.Insert("target_parent", json);
			}
		}

		LogZ.Log(msg, lvl, eventType, dto);
	}

	/**
	    \brief Resolve hit/kill event type by victim and hit flag.
	    \param victim Victim object.
	    \param isHit  True for hit, false for kill.
	    \return LogZ_Event Event type for this victim/hit combo.
	*/
	protected static LogZ_Event ResolveVictimEvent(Object victim, bool isHit)
	{
		if (victim.IsDayZCreature()) {
			if (isHit)
				return LogZ_Event.CREATURE_HIT;

			return LogZ_Event.CREATURE_KILL;
		}

		if (victim.IsTransport()) {
			if (isHit)
				return LogZ_Event.TRANSPORT_HIT;

			return LogZ_Event.TRANSPORT_KILL;
		}

		if (victim.CanUseConstruction() || victim.IsBuilding() || victim.IsFuelStation()) {
			if (isHit)
				return LogZ_Event.BUILDING_HIT;

			return LogZ_Event.BUILDING_KILL;
		}

		if (victim.IsMan()) {
#ifdef EXPANSIONMODAI
			if (victim.IsInherited(eAIBase)) {
				if (isHit)
					return LogZ_Event.CREATURE_HIT;

				return LogZ_Event.CREATURE_KILL;
			}
#endif

			if (isHit)
				return LogZ_Event.PLAYER_HIT;

			return LogZ_Event.PLAYER_KILL;
		}

		if (isHit)
			return LogZ_Event.ENTITY_HIT;

		return LogZ_Event.ENTITY_KILL;
	}
}
#endif
