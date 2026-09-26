/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
#ifdef EXPANSIONMODBASEBUILDING
/**
    \brief Expansion territories (compiled only when Expansion BaseBuilding is loaded).
    \details
        Every territory RPC ends in an Exec_* method on the server that carries the sender and the
        parameters, so the hooks wrap those: they read the state before, call Expansion, read it
        after and log only what really changed. Denied requests are not logged.
        Expansion checks less than the menu suggests, so some lines carry the facts to judge it:
        - Exec_CreateTerritory never checks who owns the flag it is given or how far away it is, so
          "territory created" carries had_territory / old_owner_uid (a created territory on a flag that
          already had one is a takeover) and the distance from the player to the flag.
        - Exec_AcceptInvite checks the distance to the flag but never that an invite exists, so
          "territory member joined" carries has_invite.
*/
modded class ExpansionTerritoryModule
{
	protected void LogZ_Territory(string msg, LogZ_Level lvl, PlayerIdentity sender, ExpansionTerritory territory, Object target = null, map<string, string> extra = null)
	{
		if (!LogZ_Levels.IsEnabled(lvl) || !LogZ_Events.IsEnabled(LogZ_Event.BASE_BUILDING))
			return;

		ref map<string, string> fields = new map<string, string>();

		if (territory) {
			fields.Insert("territory_id", territory.GetTerritoryID().ToString());
			fields.Insert("territory_name", territory.GetTerritoryName());
			fields.Insert("territory_owner_uid", territory.GetOwnerID());
			fields.Insert("territory_members", territory.NumberOfMembers().ToString());
			fields.Insert("territory_pos", territory.GetPosition().ToString());
		}

		if (extra) {
			foreach (string key, string value : extra)
				fields.Insert(key, value);
		}

		PlayerBase player;
		if (sender)
			player = PlayerBase.Cast(sender.GetPlayer());

		LogZ_ExpansionLogger.WithBase(msg, lvl, target, player, fields);
	}

	override void Exec_CreateTerritory(string territoryName, TerritoryFlag flag, PlayerIdentity sender)
	{
		ExpansionTerritory before;
		string ownerBefore;

		if (flag) {
			before = flag.GetTerritory();
			ownerBefore = flag.GetOwnerID();
		}

		super.Exec_CreateTerritory(territoryName, flag, sender);

		if (!flag || !sender)
			return;

		ExpansionTerritory after = flag.GetTerritory();
		if (!after || after == before)
			return;

		ref map<string, string> extra = new map<string, string>();
		extra.Insert("had_territory", LogZ_ExpansionLogger.Flag(before != null));
		extra.Insert("territories", GetPlayerTerritoriesCount(sender.GetId()).ToString());

		LogZ_Level lvl = LogZ_Level.INFO;
		if (before) {
			extra.Insert("old_owner_uid", ownerBefore);
			lvl = LogZ_Level.WARN;
		}

		LogZ_Territory("territory created", lvl, sender, after, flag, extra);
	}

	override void Exec_DeleteTerritoryPlayer(TerritoryFlag flag, PlayerIdentity sender)
	{
		ExpansionTerritory territory;
		int id = -1;
		vector pos;

		if (flag) {
			territory = flag.GetTerritory();
			pos = flag.GetPosition();
			if (territory)
				id = territory.GetTerritoryID();
		}

		super.Exec_DeleteTerritoryPlayer(flag, sender);

		// the flag is only removed for the owner, so a still registered flag means it was denied
		if (id < 0 || GetTerritoryFlag(id))
			return;

		ref map<string, string> extra = new map<string, string>();
		extra.Insert("flag_pos", pos.ToString());
		LogZ_Territory("territory deleted", LogZ_Level.INFO, sender, territory, null, extra);
	}

	// Called with a sender from the admin tools, and with a null sender whenever a territory flag is
	// deleted for any other reason (TerritoryFlag.EEDelete): destroyed, dismantled, admin-deleted.
	override void Exec_DeleteTerritoryAdmin(int territoryID, PlayerIdentity sender)
	{
		TerritoryFlag flag = GetTerritoryFlag(territoryID);
		ExpansionTerritory territory;
		vector pos;

		if (flag) {
			territory = flag.GetTerritory();
			pos = flag.GetPosition();
		}

		super.Exec_DeleteTerritoryAdmin(territoryID, sender);

		if (!territory || GetTerritoryFlag(territoryID))
			return;

		ref map<string, string> extra = new map<string, string>();
		extra.Insert("flag_pos", pos.ToString());

		if (sender) {
			extra.Insert("admin", "1");
			LogZ_Territory("territory deleted by admin", LogZ_Level.INFO, sender, territory, null, extra);
		} else {
			LogZ_Territory("territory removed", LogZ_Level.INFO, null, territory, null, extra);
		}
	}

	override void Exec_AcceptInvite(int territoryID, notnull PlayerIdentity sender)
	{
		TerritoryFlag flag = GetTerritoryFlag(territoryID);
		ExpansionTerritory territory;
		bool hadInvite;
		bool wasMember;

		if (flag)
			territory = flag.GetTerritory();

		if (territory) {
			hadInvite = territory.HasInvite(sender.GetId());
			wasMember = territory.IsMember(sender.GetId());
		}

		super.Exec_AcceptInvite(territoryID, sender);

		if (!territory || wasMember || !territory.IsMember(sender.GetId()))
			return;

		ref map<string, string> extra = new map<string, string>();
		extra.Insert("has_invite", LogZ_ExpansionLogger.Flag(hadInvite));

		LogZ_Level lvl = LogZ_Level.INFO;
		if (!hadInvite)
			lvl = LogZ_Level.WARN;

		LogZ_Territory("territory member joined", lvl, sender, territory, flag, extra);
	}

	override void Exec_RequestInvitePlayer(string targetID, TerritoryFlag flag, notnull PlayerIdentity sender)
	{
		ExpansionTerritory territory;
		bool hadInvite;

		if (flag)
			territory = flag.GetTerritory();

		if (territory)
			hadInvite = territory.HasInvite(targetID);

		super.Exec_RequestInvitePlayer(targetID, flag, sender);

		if (!territory || hadInvite || !territory.HasInvite(targetID))
			return;

		ref map<string, string> extra = new map<string, string>();
		extra.Insert("invited_uid", targetID);
		LogZ_Territory("territory invite sent", LogZ_Level.INFO, sender, territory, flag, extra);
	}

	override void Exec_KickMember(int territoryID, ExpansionTerritoryMember member, notnull PlayerIdentity sender)
	{
		TerritoryFlag flag = GetTerritoryFlag(territoryID);
		ExpansionTerritory territory;
		string kickedUid;
		string kickedName;
		bool wasMember;

		if (flag)
			territory = flag.GetTerritory();

		if (territory && member) {
			kickedUid = member.GetID();
			kickedName = member.GetName();
			wasMember = territory.IsMember(kickedUid);
		}

		super.Exec_KickMember(territoryID, member, sender);

		if (!wasMember || territory.IsMember(kickedUid))
			return;

		ref map<string, string> extra = new map<string, string>();
		extra.Insert("kicked_uid", kickedUid);
		extra.Insert("kicked_name", kickedName);
		LogZ_Territory("territory member kicked", LogZ_Level.INFO, sender, territory, flag, extra);
	}

	override void Exec_PromoteMember(int territoryID, ExpansionTerritoryMember member, PlayerIdentity sender)
	{
		TerritoryFlag flag = GetTerritoryFlag(territoryID);
		ExpansionTerritory territory;
		ExpansionTerritoryMember target;
		int rankBefore = -1;

		if (flag)
			territory = flag.GetTerritory();

		if (territory && member) {
			target = territory.GetMember(member.GetID());
			if (target)
				rankBefore = target.GetRank();
		}

		super.Exec_PromoteMember(territoryID, member, sender);

		if (!target || target.GetRank() == rankBefore)
			return;

		ref map<string, string> extra = new map<string, string>();
		extra.Insert("member_uid", target.GetID());
		extra.Insert("rank_before", rankBefore.ToString());
		extra.Insert("rank_after", target.GetRank().ToString());
		LogZ_Territory("territory member promoted", LogZ_Level.INFO, sender, territory, flag, extra);
	}

	override void Exec_DemoteMember(int territoryID, ExpansionTerritoryMember member, notnull PlayerIdentity sender)
	{
		TerritoryFlag flag = GetTerritoryFlag(territoryID);
		ExpansionTerritory territory;
		ExpansionTerritoryMember target;
		int rankBefore = -1;

		if (flag)
			territory = flag.GetTerritory();

		if (territory && member) {
			target = territory.GetMember(member.GetID());
			if (target)
				rankBefore = target.GetRank();
		}

		super.Exec_DemoteMember(territoryID, member, sender);

		if (!target || target.GetRank() == rankBefore)
			return;

		ref map<string, string> extra = new map<string, string>();
		extra.Insert("member_uid", target.GetID());
		extra.Insert("rank_before", rankBefore.ToString());
		extra.Insert("rank_after", target.GetRank().ToString());
		LogZ_Territory("territory member demoted", LogZ_Level.INFO, sender, territory, flag, extra);
	}

	override void Exec_Leave(int territoryID, notnull PlayerIdentity sender)
	{
		TerritoryFlag flag = GetTerritoryFlag(territoryID);
		ExpansionTerritory territory;
		bool wasMember;

		if (flag)
			territory = flag.GetTerritory();

		if (territory)
			wasMember = territory.IsMember(sender.GetId());

		super.Exec_Leave(territoryID, sender);

		if (!wasMember || territory.IsMember(sender.GetId()))
			return;

		LogZ_Territory("territory member left", LogZ_Level.INFO, sender, territory, flag);
	}
}
#endif
#endif
