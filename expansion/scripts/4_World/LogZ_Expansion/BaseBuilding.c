/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
#ifdef EXPANSIONMODBASEBUILDING
/**
    \brief Shared logger of the Expansion BaseBuilding hooks, plus the code lock hooks (compiled only when
        Expansion BaseBuilding is loaded). Territories are in Territory.c, structures and raids in Structures.c.
    \details
        Expansion checks and changes codes on the server in the ItemBase RPC handlers
        RPC_Expansion_Unlock, RPC_Expansion_Lock, RPC_Expansion_SetCode and RPC_Expansion_ChangeCode.
        The entered code is read from the RPC context inside them, so it cannot be read here without
        consuming it: each hook compares the lock state before and after the handler instead.
        Codes are never logged. RPC_Expansion_ChangeCode has no server-side check that the sender
        knows the code or that the lock is open, so the change line carries both.
*/
class LogZ_ExpansionLogger
{
	/**
	    \brief Write one line with the usual target / player / distance fields.
	    \param ev     Event type (CODE_LOCK or BASE_BUILDING).
	    \param target Object the action was about (may be null, for example a deleted flag).
	    \param player Acting player (may be null).
	*/
	protected static void Write(LogZ_Event ev, string msg, LogZ_Level lvl, Object target, PlayerBase player, map<string, string> extra)
	{
		if (!LogZ_Levels.IsEnabled(lvl) || !LogZ_Events.IsEnabled(ev))
			return;

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (target && LogZ_GameLogger.SerializeObject(target, json))
			dto.Insert("target", json);

		if (player) {
			if (LogZ_GameLogger.SerializeObject(player, json, true))
				dto.Insert("player", json);

			if (target)
				dto.Insert("distance", LogZ_Utils.Distance(target, player));
		}

		if (extra) {
			foreach (string key, string value : extra)
				dto.Insert(key, value);
		}

		LogZ.Log(msg, lvl, ev, dto);
	}

	/**
	    \brief Log a code lock event of a player on a lockable item (gate, safe, tent, code lock).
	*/
	static void WithCodeLock(ItemBase target, PlayerIdentity sender, string msg, LogZ_Level lvl, map<string, string> extra = null)
	{
		if (!target)
			return;

		PlayerBase player;
		if (sender)
			player = PlayerBase.Cast(sender.GetPlayer());

		// territory members may open locks in their territory without the code (settings), so the
		// territory of the lock and whether the player belongs to it go with every line
		string uid;
		if (player)
			uid = player.GetIdentityUID();

		ref map<string, string> fields = new map<string, string>();
		AddTerritory(fields, target.GetPosition(), uid);

		if (extra) {
			foreach (string key, string value : extra)
				fields.Insert(key, value);
		}

		Write(LogZ_Event.CODE_LOCK, msg, lvl, target, player, fields);
	}

	/**
	    \brief Log a base-building event: territory change, placement, part build/dismantle/destroy, raid damage.
	*/
	static void WithBase(string msg, LogZ_Level lvl, Object target, PlayerBase player, map<string, string> extra = null)
	{
		Write(LogZ_Event.BASE_BUILDING, msg, lvl, target, player, extra);
	}

	/**
	    \brief Add the territory a position lies in: <prefix>_id (-1 = none), _owner_uid and, for a uid, _member.
	    \details Ask the server's territory module, so it works for any position (a placed object, a raided
	        wall, the player). Territory members are checked against the territory's own member list.
	*/
	static void AddTerritory(map<string, string> extra, vector pos, string uid, string prefix = "territory")
	{
		int id = -1;
		ExpansionTerritoryModule territories = ExpansionTerritoryModule.s_Instance;

		if (territories) {
			TerritoryFlag flag = territories.GetFlagAtPosition3D(pos);
			if (flag) {
				id = flag.GetTerritoryID();
				extra.Insert(prefix + "_owner_uid", flag.GetOwnerID());

				ExpansionTerritory territory = flag.GetTerritory();
				if (territory && uid != "")
					extra.Insert(prefix + "_member", Flag(territory.IsMember(uid)));
			}
		}

		extra.Insert(prefix + "_id", id.ToString());
	}

	static string Flag(bool value)
	{
		if (value)
			return "1";

		return "0";
	}
}

modded class ItemBase
{
	protected bool LogZ_IsKnownCodeLockUser(PlayerIdentity sender)
	{
		if (!sender)
			return false;

		PlayerBase player = PlayerBase.Cast(sender.GetPlayer());
		return (player && IsKnownUser(player));
	}

	override void RPC_Expansion_Unlock(PlayerIdentity sender, ParamsReadContext ctx)
	{
		bool wasLocked = HasCode() && ExpansionIsLocked();
		bool knownUser = LogZ_IsKnownCodeLockUser(sender);

		super.RPC_Expansion_Unlock(sender, ctx);

		if (!wasLocked)
			return;

		ref map<string, string> extra = new map<string, string>();
		extra.Insert("known_user", LogZ_ExpansionLogger.Flag(knownUser));

		if (ExpansionIsLocked())
			LogZ_ExpansionLogger.WithCodeLock(this, sender, "code lock wrong code", LogZ_Level.WARN, extra);
		else
			LogZ_ExpansionLogger.WithCodeLock(this, sender, "code lock unlocked", LogZ_Level.INFO, extra);
	}

	override void RPC_Expansion_Lock(PlayerIdentity sender, ParamsReadContext ctx)
	{
		bool wasLocked = ExpansionIsLocked();

		super.RPC_Expansion_Lock(sender, ctx);

		if (!wasLocked && ExpansionIsLocked())
			LogZ_ExpansionLogger.WithCodeLock(this, sender, "code lock locked", LogZ_Level.DEBUG);
	}

	override void RPC_Expansion_SetCode(PlayerIdentity sender, ParamsReadContext ctx)
	{
		bool hadCode = HasCode();

		super.RPC_Expansion_SetCode(sender, ctx);

		if (!hadCode && HasCode())
			LogZ_ExpansionLogger.WithCodeLock(this, sender, "code lock code set", LogZ_Level.INFO);
	}

	override void RPC_Expansion_ChangeCode(PlayerIdentity sender, ParamsReadContext ctx)
	{
		string oldCode = GetCode();
		bool wasLocked = HasCode() && ExpansionIsLocked();
		bool knownUser = LogZ_IsKnownCodeLockUser(sender);

		super.RPC_Expansion_ChangeCode(sender, ctx);

		if (GetCode() == oldCode)
			return;

		ref map<string, string> extra = new map<string, string>();
		extra.Insert("was_locked", LogZ_ExpansionLogger.Flag(wasLocked));
		extra.Insert("known_user", LogZ_ExpansionLogger.Flag(knownUser));
		LogZ_ExpansionLogger.WithCodeLock(this, sender, "code lock code changed", LogZ_Level.WARN, extra);
	}
}
#endif
#endif
