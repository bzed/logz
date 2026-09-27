/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2025 WoozyMasta
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief High-level helpers for game/object logging.
*/
class LogZ_GameLogger
{
	protected static ref map<int, int> s_ProjectileLastMs;
	protected static ref map<int, int> s_ClaimLastMs;
	protected static ref map<string, string> s_ZoneClaim;
	protected static int s_ZoneClaimMs;

	/**
	    \brief Log message with single object payload.
	    \details
	        - Skips log if object null, level disabled or event disabled.
	        - Serializes object into appropriate DTO (optionally with detailed stats).
	        - Optionally serializes hierarchy root as "object_parent".
	    \param obj         Subject object.
	    \param msg         Message string.
	    \param lvl         LogZ_Level Log level.
	    \param ev          LogZ_Event Event type.
	    \param slot        Optional slot name (filtered by IsAllowedSlotName).
	    \param withParent  When true, include hierarchy root as "object_parent".
	    \param withStats   When true, use *Stats DTO variant if available.
	*/
	static void WithObject(Object obj, string msg, LogZ_Level lvl, LogZ_Event ev, string slot = "", bool withParent = false, bool withStats = false)
	{
		if (!obj || !LogZ_Levels.IsEnabled(lvl) || !LogZ_Events.IsEnabled(ev))
			return;

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (SerializeObject(obj, json, withStats))
			dto.Insert("object", json);

		if (IsAllowedSlotName(slot))
			dto.Insert("slot", slot);

		if (withParent && SerializeParentObject(obj, json, withStats))
			dto.Insert("object_parent", json);

		LogZ.Log(msg, lvl, ev, dto);
	}

	/**
	    \brief Log a player disconnect with the engine's kick reason.
	    \details
	        Same payload as WithObject plus a top-level "kick_reason" (EClientKicked name, e.g.
	        LOGOUT, TIMEOUT, INPUT_HACK). Read by PlayerBase.OnDisconnect.
	    \param obj        Disconnecting player.
	    \param msg        Message string.
	    \param kickReason EClientKicked value from GetKickOffReason().
	    \param steamId    Remembered steam id, written as top-level "steam_id" when the object lost its identity.
	    \param playerName Remembered player name, same rule.
	*/
	static void WithDisconnect(Object obj, string msg, EClientKicked kickReason, string steamId = "", string playerName = "")
	{
		if (!obj || !LogZ_Levels.IsEnabled(LogZ_Level.INFO) || !LogZ_Events.IsEnabled(LogZ_Event.PLAYER_SESSION))
			return;

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (SerializeObject(obj, json))
			dto.Insert("object", json);

		dto.Insert("kick_reason", EnumTools.EnumToString(EClientKicked, kickReason));

		// only when the serialized object names nobody (identity already released)
		Man man;
		if (steamId != "" && Class.CastTo(man, obj) && !man.GetIdentity()) {
			dto.Insert("steam_id", steamId);
			dto.Insert("player_name", playerName);
		}

		LogZ.Log(msg, LogZ_Level.INFO, LogZ_Event.PLAYER_SESSION, dto);
	}

	/**
	    \brief Log where a real player's projectile stopped (WP-11, opt-in).
	    \details
	        Called from the modded DayZGame.OnProjectileStopped* callbacks, which run on both sides,
	        so it returns unless this is the dedicated server. Only shooters with an identity are
	        logged, at most one line per shooter every throttling.projectile_ms. The source is the
	        weapon or the player; the shooter is its hierarchy root player.
	    \param info      Stop info from the engine.
	    \param msg       Message string ("projectile stopped", "projectile hit terrain/object").
	    \param hitObj    Object hit, or null.
	    \param component Hit component index, -1 if none.
	    \param water     Terrain hit was water.
	*/
	static void WithProjectile(ProjectileStoppedInfo info, string msg, Object hitObj, int component, bool water)
	{
		if (!info || !g_Game.IsDedicatedServer() || !LogZ_Config.IsLoaded() || !LogZ_Config.Get().filters.projectile_events)
			return;

		if (!LogZ_Levels.IsEnabled(LogZ_Level.INFO) || !LogZ_Events.IsEnabled(LogZ_Event.SYSTEM_GAME))
			return;

		EntityAI source = EntityAI.Cast(info.GetSource());
		if (!source)
			return;

		Man shooter = source.GetHierarchyRootPlayer();
		if (!shooter || !shooter.GetIdentity())
			return;

		int intervalMs = LogZ_Config.Get().throttling.projectile_ms;
		if (intervalMs > 0) {
			if (!s_ProjectileLastMs)
				s_ProjectileLastMs = new map<int, int>();

			int now = g_Game.GetTime();
			int shooterKey = shooter.GetID();
			int last;
			if (s_ProjectileLastMs.Find(shooterKey, last) && (now - last) < intervalMs)
				return;

			s_ProjectileLastMs.Set(shooterKey, now);
		}

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (source != shooter && SerializeObject(source, json))
			dto.Insert("attacker", json);

		if (SerializeObject(shooter, json))
			dto.Insert("attacker_parent", json);

		if (hitObj && SerializeObject(hitObj, json))
			dto.Insert("victim", json);

		vector pos = info.GetPos();
		vector velocity = info.GetInVelocity();
		dto.Insert("pos", string.Format("[%1,%2,%3]", pos[0], pos[1], pos[2]));
		dto.Insert("velocity", string.Format("[%1,%2,%3]", velocity[0], velocity[1], velocity[2]));
		dto.Insert("ammo_type", info.GetAmmoType());
		dto.Insert("projectile_damage", info.GetProjectileDamage().ToString());

		if (component >= 0)
			dto.Insert("component", component.ToString());

		CollisionInfoBase collision = CollisionInfoBase.Cast(info);
		if (collision) {
			vector normal = collision.GetSurfNormal();
			dto.Insert("surface_normal", string.Format("[%1,%2,%3]", normal[0], normal[1], normal[2]));
		}

		if (water)
			dto.Insert("water", "1");

		LogZ.Log(msg, LogZ_Level.INFO, LogZ_Event.SYSTEM_GAME, dto);
	}

	/**
	    \brief Vector as a JSON array, the form Log() writes without quotes.
	*/
	protected static string Vec(vector v)
	{
		return string.Format("[%1,%2,%3]", v[0], v[1], v[2]);
	}

	/**
	    \brief Log a hit claim the server is processing (WP-14, plan 2.15).
	    \details
	        Called first thing from the modded DayZGame.FirearmEffects / CloseCombatEffects, which the
	        engine calls with the claim's own data on the server and on every client. Vanilla's
	        server branch acts on that data (an explosive-ammo claim spawns a gas zone or an
	        explosion at the claimed position when source.ShootsExplosiveAmmo() holds), and the claim
	        also covers hits on foliage, ground and loot that EEHitBy never sees.

	        Roles as on hit lines: source = "attacker" (left out when it is the shooter itself),
	        its hierarchy root player = "attacker_parent", the object hit = "victim". The strings
	        surface and ammo_type are attacker-supplied and go through LogZ_Json.Token.

	        Shooters without an identity (eAI) are skipped when filters.skip_ai_weapon_fire is set.
	        Lines are limited to one per shooter per throttling.hit_claim_ms, except the red flag: an
	        explosive-ammo claim whose source is not a launcher in the shooter's hands is WARN and
	        never throttled. A claim that makes the server spawn a zone or explosion also records who
	        did it (BeginClaim state) so the zone line can name the cause, whatever the log settings.
	    \param msg        "firearm claim" or "melee claim".
	    \param firearm    True for FirearmEffects (has exit position, speeds, deflection).
	*/
	static void WithClaim(string msg, Object source, Object directHit, int component, string surface, vector pos, vector surfNormal, vector exitPos, vector inSpeed, vector outSpeed, bool water, bool deflected, string ammoType, bool firearm)
	{
		s_ZoneClaim = null;

		if (!g_Game.IsDedicatedServer() || !LogZ_Config.IsLoaded())
			return;

		EntityAI src = EntityAI.Cast(source);
		Man shooter;
		if (src)
			shooter = src.GetHierarchyRootPlayer();

		if (shooter && !shooter.GetIdentity() && LogZ_Config.Get().filters.skip_ai_weapon_fire)
			return;

		// what the vanilla server branch will do with this claim
		bool explosiveAmmo = firearm && (ammoType == "Bullet_40mm_ChemGas" || ammoType == "Bullet_40mm_Explosive");
		bool sourceExplosive = source && source.ShootsExplosiveAmmo();
		bool sourceWeapon = source && source.IsWeapon();
		bool sourceInHands = shooter && src && shooter.GetEntityInHands() == src;
		bool spawns = explosiveAmmo && sourceExplosive && !deflected && outSpeed == vector.Zero;
		bool launcherInHands = sourceWeapon && sourceExplosive && sourceInHands;
		bool redFlag = explosiveAmmo && !launcherInHands;

		string srcJson, shooterJson;
		bool serialized;

		// serializing costs, so only when the state is kept or the line is written
		string ammo;
		if (spawns) {
			SerializeClaimActors(src, shooter, srcJson, shooterJson);
			serialized = true;
			ammo = LogZ_Json.Token(ammoType);

			s_ZoneClaim = new map<string, string>();
			s_ZoneClaim.Insert("attacker", srcJson);
			s_ZoneClaim.Insert("attacker_parent", shooterJson);
			s_ZoneClaim.Insert("claim_ammo_type", ammo);
			s_ZoneClaim.Insert("claim_launcher", FlagValue(launcherInHands));
			s_ZoneClaimMs = g_Game.GetTime();
		}

		LogZ_Level lvl = LogZ_Level.INFO;
		if (redFlag)
			lvl = LogZ_Level.WARN;

		if (!LogZ_Levels.IsEnabled(lvl) || !LogZ_Events.IsEnabled(LogZ_Event.SYSTEM_GAME))
			return;

		int intervalMs = LogZ_Config.Get().throttling.hit_claim_ms;
		if (intervalMs > 0 && !redFlag && !spawns) {
			if (!s_ClaimLastMs)
				s_ClaimLastMs = new map<int, int>();

			int shooterKey = 0;
			if (shooter)
				shooterKey = shooter.GetID();

			int now = g_Game.GetTime();
			int last;
			if (s_ClaimLastMs.Find(shooterKey, last) && (now - last) < intervalMs)
				return;

			s_ClaimLastMs.Set(shooterKey, now);
		}

		if (!serialized) {
			SerializeClaimActors(src, shooter, srcJson, shooterJson);
			ammo = LogZ_Json.Token(ammoType);
		}

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (srcJson != string.Empty)
			dto.Insert("attacker", srcJson);

		if (shooterJson != string.Empty)
			dto.Insert("attacker_parent", shooterJson);

		if (directHit && SerializeObject(directHit, json))
			dto.Insert("victim", json);

		dto.Insert("component", component.ToString());
		dto.Insert("surface", LogZ_Json.Token(surface));
		dto.Insert("ammo_type", ammo);
		dto.Insert("pos", Vec(pos));
		dto.Insert("surface_normal", Vec(surfNormal));

		if (water)
			dto.Insert("water", "1");

		if (firearm) {
			dto.Insert("exit_pos", Vec(exitPos));
			dto.Insert("in_speed", Vec(inSpeed));
			dto.Insert("out_speed", Vec(outSpeed));
			dto.Insert("deflected", FlagValue(deflected));
			dto.Insert("explosive_ammo", FlagValue(explosiveAmmo));
			dto.Insert("spawns", FlagValue(spawns));
			dto.Insert("source_explosive", FlagValue(sourceExplosive));
			dto.Insert("source_weapon", FlagValue(sourceWeapon));
			dto.Insert("source_in_hands", FlagValue(sourceInHands));
		}

		if (!src)
			dto.Insert("no_source", "1");
		else if (!shooter)
			dto.Insert("no_shooter", "1");

		LogZ.Log(msg, lvl, LogZ_Event.SYSTEM_GAME, dto);
	}

	/**
	    \brief Serialize the source (unless it is the shooter itself) and the shooter of a claim.
	*/
	protected static void SerializeClaimActors(EntityAI src, Man shooter, out string srcJson, out string shooterJson)
	{
		if (src && shooter != src)
			SerializeObject(src, srcJson);

		if (shooter)
			SerializeObject(shooter, shooterJson);
	}

	/**
	    \brief End of the claim WithClaim recorded, called after the vanilla body ran.
	*/
	static void EndClaim()
	{
		s_ZoneClaim = null;
	}

	/**
	    \brief The claim that is being processed right now, when it makes the server spawn a zone.
	    \details
	        ContaminatedArea_Local.EEInit runs inside g_Game.CreateObject, i.e. inside the vanilla
	        FirearmEffects that WithClaim precedes. Empty outside of that (a chemical grenade, a
	        destroyed 40mm pile), or when the state is stale.
	*/
	static map<string, string> GetZoneClaim()
	{
		if (s_ZoneClaim && (g_Game.GetTime() - s_ZoneClaimMs) < 250)
			return s_ZoneClaim;

		return null;
	}

	protected static string FlagValue(bool value)
	{
		if (value)
			return "1";

		return "0";
	}

	/**
	    \brief Log message with object and owner/parent container.
	    \details
	        - Typical use: inventory operations, attachments, triggers.
	        - "object" is usually item, "owner" is container/player/vehicle.
	        - Optionally includes parent hierarchy for both.
	    \param obj          Primary object (item, victim, etc.).
	    \param owner        Owner or related object (container, trigger, vehicle...).
	    \param msg          Message string.
	    \param lvl          LogZ_Level Log level.
	    \param ev           LogZ_Event Event type.
	    \param slot         Optional slot name (filtered by IsAllowedSlotName).
	    \param withParents  When true, serialize hierarchy root for both object and owner.
	    \param withStats    When true, use *Stats DTO where possible.
	*/
	static void WithObjectAndOwner(Object obj, Object owner, string msg, LogZ_Level lvl, LogZ_Event ev, string slot = "", bool withParents = false, bool withStats = false)
	{
		if (!obj || !LogZ_Levels.IsEnabled(lvl) || !LogZ_Events.IsEnabled(ev))
			return;

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (SerializeObject(obj, json, withStats))
			dto.Insert("object", json);

		if (withParents && SerializeParentObject(obj, json, withStats))
			dto.Insert("object_parent", json);

		if (IsAllowedSlotName(slot))
			dto.Insert("slot", slot);

		if (owner && obj != owner) {
			if (SerializeObject(owner, json, withStats))
				dto.Insert("owner", json);

			if (withParents && SerializeParentObject(obj, json, withStats))
				dto.Insert("owner_parent", json);
		}

		LogZ.Log(msg, lvl, ev, dto);
	}

	/**
	    \brief Serialize object into a JSON DTO string.
	    \details
	        - Selects DTO type by object runtime type:
	            Man -> LogZ_DTO_Man / LogZ_DTO_ManStats
	            Transport -> LogZ_DTO_Transport / LogZ_DTO_TransportStats
	            EntityAI -> LogZ_DTO_Entity / LogZ_DTO_EntityStats
	            other -> LogZ_DTO_Object / LogZ_DTO_ObjectStats
	        - Returns false if serializer or object invalid.
	    \param obj       Source object.
	    \param json[out] Resulting JSON string.
	    \param withStats When true, use *Stats DTO variant.
	    \return bool     True on success.
	*/
	static bool SerializeObject(Object obj, out string json, bool withStats = false)
	{
		if (!obj)
			return false;

		if (obj.IsMan()) {
			if (withStats) {
				LogZ_DTO_ManStats manStatsDTO = new LogZ_DTO_ManStats(obj);
				return LogZ.GetSerializer().WriteToString(manStatsDTO, false, json);
			}

			LogZ_DTO_Man manDTO = new LogZ_DTO_Man(obj);
			return (LogZ.GetSerializer().WriteToString(manDTO, false, json));
		}

		if (obj.IsTransport()) {
			if (withStats) {
				LogZ_DTO_TransportStats vehicleStatsDTO = new LogZ_DTO_TransportStats(obj);
				return LogZ.GetSerializer().WriteToString(vehicleStatsDTO, false, json);
			}

			LogZ_DTO_Transport vehicleDTO = new LogZ_DTO_Transport(obj);
			return (LogZ.GetSerializer().WriteToString(vehicleDTO, false, json));
		}

		if (obj.IsEntityAI()) {
			if (withStats) {
				LogZ_DTO_EntityStats entityStatsDTO = new LogZ_DTO_EntityStats(obj);
				return LogZ.GetSerializer().WriteToString(entityStatsDTO, false, json);
			}

			LogZ_DTO_Entity entityDTO = new LogZ_DTO_Entity(obj);
			return LogZ.GetSerializer().WriteToString(entityDTO, false, json);
		}

		if (withStats) {
			LogZ_DTO_ObjectStats objStatsDTO = new LogZ_DTO_ObjectStats(obj);
			return LogZ.GetSerializer().WriteToString(objStatsDTO, false, json);
		}

		LogZ_DTO_Object objDTO = new LogZ_DTO_Object(obj);
		return LogZ.GetSerializer().WriteToString(objDTO, false, json);
	}

	/**
	    \brief Serialize hierarchy root of an EntityAI as parent.
	    \details
	        - Only works for EntityAI.
	        - Skips when root equals the object itself.
	    \param obj       Child object.
	    \param json[out] JSON for parent object.
	    \param withStats Use *Stats DTO for parent if true.
	    \return bool     True when parent exists and serialized.
	*/
	static bool SerializeParentObject(Object obj, out string json, bool withStats = false)
	{
		if (!obj || !obj.IsEntityAI())
			return false;

		EntityAI eai = EntityAI.Cast(obj);
		if (!eai)
			return false;

		EntityAI parent = eai.GetHierarchyRoot();
		if (!parent || parent == eai)
			return false;

		return SerializeObject(parent, json, withStats);
	}

	/**
	    \brief Whitelist filter for slot names.
	    \return bool True if slot name can be logged.
	*/
	protected static bool IsAllowedSlotName(string slot)
	{
		if (slot == string.Empty)
			return false;

		if (slot == "RevolverCylinder" || slot == "RevolverEjector")
			return false;

		return true;
	}
}
#endif
