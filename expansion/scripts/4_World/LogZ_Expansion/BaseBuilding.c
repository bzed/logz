/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
#ifdef EXPANSIONMODBASEBUILDING
/**
    \brief Expansion BaseBuilding code locks (compiled only when Expansion BaseBuilding is loaded).
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
	    \brief Log a code lock event of a player on a lockable item (gate, safe, tent, code lock).
	*/
	static void WithCodeLock(ItemBase target, PlayerIdentity sender, string msg, LogZ_Level lvl, map<string, string> extra = null)
	{
		if (!target || !LogZ_Levels.IsEnabled(lvl) || !LogZ_Events.IsEnabled(LogZ_Event.CODE_LOCK))
			return;

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (LogZ_GameLogger.SerializeObject(target, json))
			dto.Insert("target", json);

		PlayerBase player;
		if (sender)
			player = PlayerBase.Cast(sender.GetPlayer());

		if (player) {
			if (LogZ_GameLogger.SerializeObject(player, json, true))
				dto.Insert("player", json);

			dto.Insert("distance", LogZ_Utils.Distance(target, player));
		}

		if (extra) {
			foreach (string key, string value : extra)
				dto.Insert(key, value);
		}

		LogZ.Log(msg, lvl, LogZ_Event.CODE_LOCK, dto);
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
