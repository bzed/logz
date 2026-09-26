/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
#ifdef EXPANSIONMODBASEBUILDING
/**
    \brief Expansion base structures: placing, building, dismantling, destroying and raiding.
    \details
        Expansion's rules (territories, raid schedule, tools) are checked on the server, so a line
        here is a fact the server accepted. Each line carries the territory of the object and
        whether the acting player is a member of it (<prefix>_id / _owner_uid / _member), and the tool
        in the player's hands, so a log reader can tell own base work from raiding.
*/
class LogZ_ExpansionStructures
{
	/**
	    \brief Fields shared by all structure lines: the tool in hands, the object's territory and
	        the player's own territory.
	*/
	static ref map<string, string> Context(Object at, PlayerBase player, vector pos)
	{
		ref map<string, string> extra = new map<string, string>();
		string uid;

		if (player) {
			uid = player.GetIdentityUID();

			ItemBase tool = ItemBase.Cast(player.GetItemInHands());
			if (tool)
				extra.Insert("tool", tool.GetType());

			extra.Insert("player_pos", player.GetPosition().ToString());
			LogZ_ExpansionLogger.AddTerritory(extra, player.GetPosition(), uid, "player_territory");
		}

		LogZ_ExpansionLogger.AddTerritory(extra, pos, uid);
		return extra;
	}

	/**
	    \brief Log a part event of a base structure.
	*/
	static void WithPart(BaseBuildingBase structure, string msg, LogZ_Level lvl, Man man, string part, int actionId, map<string, string> more = null)
	{
		if (!structure || !LogZ_Levels.IsEnabled(lvl) || !LogZ_Events.IsEnabled(LogZ_Event.BASE_BUILDING))
			return;

		PlayerBase player = PlayerBase.Cast(man);
		ref map<string, string> extra = Context(structure, player, structure.GetPosition());
		extra.Insert("part", part);
		extra.Insert("action_id", actionId.ToString());

		if (more) {
			foreach (string key, string value : more)
				extra.Insert(key, value);
		}

		LogZ_ExpansionLogger.WithBase(msg, lvl, structure, player, extra);
	}
}

modded class BaseBuildingBase
{
	override void OnPartBuiltServer(notnull Man player, string part_name, int action_id)
	{
		super.OnPartBuiltServer(player, part_name, action_id);
		LogZ_ExpansionStructures.WithPart(this, "base part built", LogZ_Level.INFO, player, part_name, action_id);
	}

	override void OnPartDismantledServer(notnull Man player, string part_name, int action_id)
	{
		super.OnPartDismantledServer(player, part_name, action_id);
		LogZ_ExpansionStructures.WithPart(this, "base part dismantled", LogZ_Level.INFO, player, part_name, action_id);
	}

	override void OnPartDestroyedServer(Man player, string part_name, int action_id, bool destroyed_by_connected_part = false)
	{
		super.OnPartDestroyedServer(player, part_name, action_id, destroyed_by_connected_part);

		// parts that fall with the destroyed one are a consequence, not an action
		if (destroyed_by_connected_part) {
			LogZ_ExpansionStructures.WithPart(this, "base part destroyed", LogZ_Level.DEBUG, player, part_name, action_id);
			return;
		}

		LogZ_ExpansionStructures.WithPart(this, "base part destroyed", LogZ_Level.INFO, player, part_name, action_id);
	}
}

modded class Construction
{
	// Expansion builds a part without materials when the player holds an ExpansionAdminHammer. Nothing
	// else is checked, so whoever holds one (spawned by an admin, or not) builds for free. The
	// territory flag kit uses the same path for its pole, which is not a hammer build.
	override void Expansion_AdminBuildPartServer(notnull Man player, string part_name, int action_id)
	{
		super.Expansion_AdminBuildPartServer(player, part_name, action_id);

		PlayerBase pb = PlayerBase.Cast(player);
		if (!pb)
			return;

		ItemBase tool = ItemBase.Cast(pb.GetItemInHands());
		if (!tool || tool.GetType() != "ExpansionAdminHammer")
			return;

		BaseBuildingBase structure = BaseBuildingBase.Cast(GetParent());
		LogZ_ExpansionStructures.WithPart(structure, "base part built with admin hammer", LogZ_Level.WARN, player, part_name, action_id);
	}
}

modded class ItemBase
{
	// Any deployable goes through here (kits, tents, barrels, explosives, ...). Expansion checks the
	// territory rules against the player's position, not the position the client sends for the
	// object, so both are logged: a large distance with a different territory at the object is a
	// placement into someone else's base from outside.
	override void OnPlacementComplete(Man player, vector position = "0 0 0", vector orientation = "0 0 0")
	{
		super.OnPlacementComplete(player, position, orientation);

		if (!LogZ_Levels.IsEnabled(LogZ_Level.INFO) || !LogZ_Events.IsEnabled(LogZ_Event.BASE_BUILDING))
			return;

		PlayerBase pb = PlayerBase.Cast(player);
		if (!pb)
			return;

		vector where = position;
		if (where == "0 0 0")
			where = GetPosition();

		ref map<string, string> extra = LogZ_ExpansionStructures.Context(this, pb, where);
		extra.Insert("place_pos", where.ToString());
		extra.Insert("place_distance", vector.Distance(pb.GetPosition(), where).ToString());

		auto settings = GetExpansionSettings().GetBaseBuilding(false);
		if (settings && settings.IsLoaded()) {
			bool whitelisted;
			foreach (string deployable : settings.DeployableInsideAEnemyTerritory) {
				if (IsKindOf(deployable)) {
					whitelisted = true;
					break;
				}
			}
			extra.Insert("enemy_territory_ok", LogZ_ExpansionLogger.Flag(whitelisted));
		}

		LogZ_ExpansionLogger.WithBase("object placed", LogZ_Level.INFO, this, pb, extra);
	}

	// Raid damage: called for every hit that damaged a base structure or safe, and for every finished
	// cycle of a destroy action (tool in hands: lock, barbed wire, safe). Tool cycles have no ammo.
	// Weapon and explosion hits are also written as building.hit lines, so those are only DEBUG here.
	override void RaidLog(EntityAI source, string damageZone, string ammo, float health, float dmg, float damageMultiplier)
	{
		super.RaidLog(source, damageZone, ammo, health, dmg, damageMultiplier);

		if ((dmg * damageMultiplier) == 0)
			return;

		LogZ_Level lvl = LogZ_Level.INFO;
		if (ammo != "")
			lvl = LogZ_Level.DEBUG;

		if (!LogZ_Levels.IsEnabled(lvl) || !LogZ_Events.IsEnabled(LogZ_Event.BASE_BUILDING))
			return;

		PlayerBase player;
		if (source)
			player = PlayerBase.Cast(source.GetHierarchyRootPlayer());

		ref map<string, string> extra = LogZ_ExpansionStructures.Context(this, player, GetPosition());

		string zone = damageZone;
		if (zone == "")
			zone = "GlobalHealth";

		extra.Insert("damage", (dmg * damageMultiplier).ToString());
		extra.Insert("multiplier", damageMultiplier.ToString());
		extra.Insert("health_before", health.ToString());
		extra.Insert("health_after", GetHealth(damageZone, "Health").ToString());
		extra.Insert("health_max", GetMaxHealth(damageZone, "Health").ToString());
		extra.Insert("zone", zone);
		extra.Insert("can_be_damaged", LogZ_ExpansionLogger.Flag(CanBeDamaged()));

		if (ammo != "")
			extra.Insert("ammo", ammo);

		if (source)
			extra.Insert("source", source.GetType());

		auto raid = GetExpansionSettings().GetRaid(false);
		if (raid && raid.IsLoaded()) {
			extra.Insert("raidable_now", LogZ_ExpansionLogger.Flag(raid.IsRaidableNow()));
			extra.Insert("raid_mode", raid.BaseBuildingRaidMode.ToString());
		}

		LogZ_ExpansionLogger.WithBase("raid damage", lvl, this, player, extra);
	}
}

// The "simple territory" flag is folded up in one action instead of being dismantled part by part, so
// it has no part events. Away from the own territory it is the raid version and takes 30 s instead of 5.
modded class ExpansionActionDismantleFlag
{
	override void OnFinishProgressServer(ActionData action_data)
	{
		PlayerBase player = action_data.m_Player;
		Object flag = action_data.m_Target.GetObject();

		if (flag && LogZ_Levels.IsEnabled(LogZ_Level.INFO) && LogZ_Events.IsEnabled(LogZ_Event.BASE_BUILDING)) {
			ref map<string, string> extra = LogZ_ExpansionStructures.Context(flag, player, flag.GetPosition());
			TerritoryFlag territoryFlag = TerritoryFlag.Cast(flag);
			if (territoryFlag)
				extra.Insert("had_territory", LogZ_ExpansionLogger.Flag(territoryFlag.HasExpansionTerritoryInformation()));

			LogZ_ExpansionLogger.WithBase("territory flag dismantled", LogZ_Level.INFO, flag, player, extra);
		}

		super.OnFinishProgressServer(action_data);
	}
}

// C4: armed by placing it, exploded by a timer (or by power). The vanilla EXPLOSIVE events only cover
// ExplosivesBase, which this is not.
modded class ExpansionExplosive
{
	override void TriggerExplosion()
	{
		bool wasExploded = m_Exploded;

		super.TriggerExplosion();

		if (wasExploded || !m_Exploded)
			return;

		ref map<string, string> extra = new map<string, string>();
		LogZ_ExpansionLogger.AddTerritory(extra, GetPosition(), "");
		extra.Insert("pos", GetPosition().ToString());
		LogZ_ExpansionLogger.WithBase("expansion explosive detonated", LogZ_Level.INFO, this, null, extra);
	}
}
#endif
#endif
