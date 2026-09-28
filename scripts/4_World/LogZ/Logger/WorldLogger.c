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
	// Which geometry the line-of-sight ray (FillLineOfSight) intersects. Fire geometry is what a bullet
	// meets; view geometry also stops at thin foliage (local server 2026-09-28: same rays, bushes vs
	// the building behind them).
	protected static const int LOS_GEOMETRY = ObjIntersectFire;

	// Most times the line-of-sight ray restarts behind the attacker's own gear or another creature.
	protected static const int LOS_MAX_STEPS = 6;

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
	        A hit whose line must wait for vanilla's own EEHitBy body (bleeding_added) uses
	        PrepareHit and EmitHit instead.
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
		LogZ_PendingHit pending = PrepareHit(victim, source, damageResult, damageType, component, dmgZone, ammo, modelPos, speedCoef, lvl);
		if (pending)
			EmitHit(pending);
	}

	/**
	    \brief Build a hit line without writing it (WP-17), null when the hit is filtered out.
	    \details
	        Everything a hit line says about the moment of the hit (victim and attacker state,
	        distance, line of sight) is captured here, before vanilla's EEHitBy body runs. EmitHit
	        writes it, optionally after that body ran, so the line can say how many bleeding sources
	        the hit opened. A line is emitted from inside the same EEHitBy call, so it still comes
	        before the kill line.
	*/
	static LogZ_PendingHit PrepareHit(Object victim, EntityAI source, TotalDamageResult damageResult, int damageType, int component, string dmgZone, string ammo, vector modelPos, float speedCoef, LogZ_Level lvl = 2)
	{
		if (!LogZ_Config.IsLoaded() || !victim || !LogZ_Levels.IsEnabled(lvl))
			return null;

		LogZ_Event eventType = ResolveVictimEvent(victim, true);
		if (!LogZ_Events.IsEnabled(eventType))
			return null;

		if (damageResult) {
			float damage = damageResult.GetDamage(dmgZone, "");
			if (damage < LogZ_Config.Get().thresholds.hit_damage)
				return null;

			if (source && source.IsTransport() && damage < LogZ_Config.Get().thresholds.hit_damage_vehicle)
				return null;
		}

		LogZ_PendingHit pending = new LogZ_PendingHit();
		pending.level = lvl;
		pending.event_type = eventType;
		string json;

		if (LogZ_GameLogger.SerializeObject(victim, json))
			pending.fields.Insert("victim", json);

		if (LogZ_GameLogger.SerializeParentObject(victim, json))
			pending.fields.Insert("victim_parent", json);

		pending.damage = new LogZ_DTO_Damage(damageResult, damageType, dmgZone, ammo, component, modelPos, speedCoef);

		if (!source) {
			pending.message = string.Format("%1 damaged", LogZ_Object.GetType(victim));
			return pending;
		}

		if (source == victim) {
			pending.message = string.Format("%1 hit self", LogZ_Object.GetType(victim));
			return pending;
		}

		FillLineOfSight(pending.damage, victim, source, damageType, component, modelPos);

		pending.fields.Insert("distance", LogZ_Utils.Distance(source, victim));

		if (LogZ_GameLogger.SerializeObject(source, json))
			pending.fields.Insert("attacker", json);

		if (LogZ_GameLogger.SerializeParentObject(source, json))
			pending.fields.Insert("attacker_parent", json);

		pending.message = string.Format("%1 hit", LogZ_Object.GetType(victim));
		return pending;
	}

	/**
	    \brief Write a line PrepareHit built.
	    \param bleedingAdded Bleeding sources the hit opened, -1 when not measured.
	*/
	static void EmitHit(LogZ_PendingHit pending, int bleedingAdded = -1)
	{
		string json;
		pending.damage.bleeding_added = bleedingAdded;
		if (LogZ.GetSerializer().WriteToString(pending.damage, false, json))
			pending.fields.Insert("damage", json);

		LogZ.Log(pending.message, pending.level, pending.event_type, pending.fields);
	}

	/**
	    \brief True for a player, zombie or animal: something a hit claims to land on, and no cover.
	*/
	protected static bool IsBody(Object obj)
	{
		if (obj.IsMan())
			return true;

		EntityAI entity = EntityAI.Cast(obj);
		return entity && (entity.IsZombie() || entity.IsAnimal());
	}

	/**
	    \brief Trace the attacker's line of sight to the hit point and record it on the damage DTO (WP-17).
	    \details
	        Vanilla checks no line of sight anywhere in the hit pipeline: the hit's zone, ammo and
	        position are claim fields the shooter's client chose, so a ray that a wall or a hill
	        blocks is the through-wall signal. Only real-player FIRE_ARM and CLOSE_COMBAT hits on a
	        player, zombie or animal are traced (an item's model position is in the item's own
	        space, and bots have no identity). The ray runs from the attacker's head bone (the
	        origin vanilla melee target selection uses, so it follows the stance; the eye height of
	        the stance when the skeleton reports the bone at the feet) to the hit point in
	        world space. The victim, the attacker and everything they carry, and any other creature
	        (a body is not cover) do not block. Fully written into dto.los*, see LogZ_DTO_Damage.
	*/
	protected static void FillLineOfSight(LogZ_DTO_Damage dto, Object victim, EntityAI source, int damageType, int component, vector modelPos)
	{
		if (!LogZ_Config.Get().filters.hit_los)
			return;

		if (damageType != DamageType.FIRE_ARM && damageType != DamageType.CLOSE_COMBAT)
			return;

		if (!IsBody(victim))
			return;

		PlayerBase attacker = PlayerBase.Cast(source.GetHierarchyRootPlayer());
		if (!attacker || attacker == victim || !attacker.GetIdentity())
			return;

		dto.los = "skipped";
		if (component < 0)
			return;

		int head = attacker.GetBoneIndexByName("Head");
		if (head < 0)
			return;

		vector from = attacker.GetBonePositionWS(head);
		vector feet = attacker.GetPosition();

		// a skeleton without animation state reports every bone at the entity origin: the ray
		// would start on the ground. Fall back to the eye height of the stance.
		if (from[1] - feet[1] < 0.3) {
			float eye = 1.6;
			if (attacker.IsPlayerInStance(DayZPlayerConstants.STANCEMASK_PRONE | DayZPlayerConstants.STANCEMASK_RAISEDPRONE))
				eye = 0.4;
			else if (attacker.IsPlayerInStance(DayZPlayerConstants.STANCEMASK_CROUCH | DayZPlayerConstants.STANCEMASK_RAISEDCROUCH))
				eye = 1.1;

			from = feet + Vector(0, eye, 0);
		}

		vector to = victim.ModelToWorld(modelPos);
		if (vector.DistanceSq(from, to) < 0.01)
			return;

		dto.los_from = from;

		// RaycastRV reports only the nearest contact, and the nearest is usually the attacker's own
		// body or rifle at the ray origin. Whatever is not cover (the attacker and their gear, the
		// victim and theirs, any other creature) is stepped over: the ray restarts just behind it.
		vector dir = vector.Direction(from, to);
		dir.Normalize();
		vector origin = from;
		vector contactPos;
		vector contactDir;
		int contactComponent;
		set<Object> results = new set<Object>();
		for (int i = 0; i < LOS_MAX_STEPS; ++i) {
			results.Clear();
			if (!DayZPhysics.RaycastRV(origin, to, contactPos, contactDir, contactComponent, results, null, victim, true, false, LOS_GEOMETRY)) {
				dto.los = "clear";
				return;
			}

			Object cover;
			foreach (Object obj : results) {
				if (obj && !IsOwnOrBody(obj, attacker, victim)) {
					cover = obj;
					break;
				}
			}

			// no object at all in the result is terrain
			if (cover || results.Count() == 0) {
				dto.los = "blocked";
				dto.los_contact = contactPos;
				if (cover)
					dto.los_object = LogZ_Json.Token(cover.GetType());

				return;
			}

			origin = contactPos + dir * 0.1;
			if (vector.DistanceSq(origin, to) < 0.04)
				break;
		}

		dto.los = "clear";
	}

	/**
	    \brief True for what does not count as cover: the attacker, the victim, what either carries or wears, and any other creature.
	*/
	protected static bool IsOwnOrBody(Object obj, PlayerBase attacker, Object victim)
	{
		if (obj == victim || obj == attacker || IsBody(obj))
			return true;

		EntityAI entity = EntityAI.Cast(obj);
		return entity && (entity.GetHierarchyRootPlayer() == attacker || entity.GetHierarchyRoot() == victim);
	}

	protected static ref map<string, string> s_ZoneOrigin; // vanilla creator running right now, see BeginZoneOrigin
	protected static int s_ZoneOriginMs;

	/**
	    \brief Record which vanilla creator is about to make a gas zone (before its vanilla body runs).
	    \details
	        Vanilla has exactly four ContaminatedArea_Local creators: the hit-claim branch of
	        DayZGame.FirearmEffects (see LogZ_GameLogger.WithClaim), Grenade_ChemGas.OnExplode and
	        Ammo_40mm_ChemGas.OnActivatedByItem / EEKilled. The zone's EEInit runs inside their
	        CreateObject call, so WithContaminatedArea can name the creator. A zone made while none of
	        them runs has origin "unknown" (a mod, an admin tool, or a path no script shows).
	    \param origin  "grenade" or "ammo_pile".
	    \param obj     The grenade or pile.
	    \param cause   What set it off (the pile's killer or activating item), may be null.
	*/
	static void BeginZoneOrigin(string origin, EntityAI obj, Object cause = null)
	{
		s_ZoneOrigin = new map<string, string>();
		s_ZoneOrigin.Insert("origin", origin);
		s_ZoneOriginMs = g_Game.GetTime();

		string json;
		if (LogZ_GameLogger.SerializeObject(obj, json))
			s_ZoneOrigin.Insert("origin_object", json);

		// a pile destroyed with no damage source names itself as the killer
		if (cause == obj)
			cause = null;

		// whoever holds the object (a grenade going off in hands), else whoever set it off
		Object who = cause;
		if (obj && obj.GetHierarchyRootPlayer())
			who = obj;

		if (cause && LogZ_GameLogger.SerializeObject(cause, json))
			s_ZoneOrigin.Insert("attacker", json);

		if (who && LogZ_GameLogger.SerializeParentObject(who, json))
			s_ZoneOrigin.Insert("attacker_parent", json);
	}

	/**
	    \brief End of the creator BeginZoneOrigin recorded, called after its vanilla body ran.
	*/
	static void EndZoneOrigin()
	{
		s_ZoneOrigin = null;
	}

	/**
	    \brief Log a gas zone the server created (WP-14, origin 2026-09-28).
	    \details
	        origin names the vanilla creator that was running: "claim" (a hit claim, see below),
	        "grenade", "ammo_pile" or "unknown". An unknown zone is WARN: no vanilla script path made it.
	        For a grenade or pile, origin_object is the grenade or pile, attacker what set it off (the
	        pile's killer or activating item) and attacker_parent the player holding either.
	        via_claim is 1 when the zone was created inside a hit claim, in which case the claim's
	        source and shooter are copied to the line (attacker, attacker_parent) with claim_ammo_type
	        and claim_launcher (1: the claim's source was a launcher in the shooter's hands). Such a
	        zone from a source that is no launcher is WARN. The radius is not written: it is set in a
	        deferred step after EEInit (10 m in vanilla).
	    \param zone New zone.
	*/
	static void WithContaminatedArea(ContaminatedArea_Local zone)
	{
		if (!zone || !LogZ_Events.IsEnabled(LogZ_Event.SYSTEM_WORLD))
			return;

		LogZ_Level lvl = LogZ_Level.INFO;
		ref map<string, string> claim = LogZ_GameLogger.GetZoneClaim();
		ref map<string, string> origin;
		if (s_ZoneOrigin && (g_Game.GetTime() - s_ZoneOriginMs) < 250)
			origin = s_ZoneOrigin;

		if (claim && claim.Get("claim_launcher") != "1")
			lvl = LogZ_Level.WARN;

		if (!claim && !origin)
			lvl = LogZ_Level.WARN;

		if (!LogZ_Levels.IsEnabled(lvl))
			return;

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (LogZ_GameLogger.SerializeObject(zone, json))
			dto.Insert("object", json);

		dto.Insert("lifetime", zone.GetRemainingTime().ToString());

		if (claim) {
			dto.Insert("origin", "claim");
			dto.Insert("via_claim", "1");
			foreach (string key, string value : claim)
				dto.Insert(key, value);
		} else {
			if (!origin)
				dto.Insert("origin", "unknown");

			dto.Insert("via_claim", "0");
			if (origin) {
				foreach (string okey, string ovalue : origin)
					dto.Insert(okey, ovalue);
			}
		}

		LogZ.Log("contaminated area", lvl, LogZ_Event.SYSTEM_WORLD, dto);
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

		// Rounds available for this muzzle (WP-9): chamber + internal magazine
		// (GetTotalCartridgeCount does not include a detachable magazine, seen on the local server)
		// plus the attached magazine. Whether the count is taken before or after the round of
		// this shot left is checked on live data.
		int ammoTotal = weapon.GetTotalCartridgeCount(muzzle);
		int ammoMax = weapon.GetTotalMaxCartridgeCount(muzzle);
		Magazine attachedMag = weapon.GetMagazine(muzzle);
		if (attachedMag) {
			ammoTotal += attachedMag.GetAmmoCount();
			ammoMax += attachedMag.GetAmmoMax();
		}

		dto.Insert("ammo_total", ammoTotal.ToString());
		dto.Insert("ammo_max", ammoMax.ToString());

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
	    \brief Log a player connect with the hive's login-position flag (WP-7).
	    \param player               Connecting player.
	    \param msg                  Message string.
	    \param loginPositionChanged 1 or 0 from Hive.CharacterIsLoginPositionChanged, -1 = unavailable.
	*/
	static void WithConnect(PlayerBase player, string msg, int loginPositionChanged)
	{
		if (!player || !LogZ_Levels.IsEnabled(LogZ_Level.INFO) || !LogZ_Events.IsEnabled(LogZ_Event.PLAYER_SESSION))
			return;

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (LogZ_GameLogger.SerializeObject(player, json))
			dto.Insert("object", json);

		if (loginPositionChanged >= 0)
			dto.Insert("login_position_changed", loginPositionChanged.ToString());

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
