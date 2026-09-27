/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief Juncture and RPC telemetry (plan 2.13/2.14, WP-13).
    \details
        Action juncture timeouts, client-initiated sync junctures and RPC sender/target
        mismatches all arrive through receive points every client already exercises, not a new
        capability. None of these is a verdict on its own: a timeout or a probe is only
        interesting as a per-player rate, which is why the analyzer side of this work package
        reads as a rate/audit feature, never a cheater flag by itself.
*/
class LogZ_JunctureLogger
{
	protected static ref map<int, int> s_TimeoutLastMs;
	protected static ref map<string, int> s_SyncLastMs; // key: "<player id>:<juncture id>"
	protected static ref map<int, int> s_DeleteItemCount; // player id -> count in the current window
	protected static ref map<int, int> s_DeleteItemWindowStart;
	protected static ref map<int, int> s_RpcLastMs;
	protected static ref map<int, int> s_RpcMismatchTotal;

	/**
	    \brief Log an action juncture timeout (ADMIN_ACTIVITY, INFO).
	    \details
	        WP-4 already established that a rejected inventory move is invisible to script; a
	        juncture timeout is the adjacent, hookable signal (an `event float` callback the
	        engine invokes when an action's reserved inventory locations are not confirmed in
	        time). One stuck action re-triggers this roughly once a second (the vanilla body
	        extends the juncture another 1.0 s and keeps going), so it is throttled per player;
	        the per-player *rate* over a session is the signal, never one timeout.
	    \param actionData The timed-out action.
	*/
	static void WithJunctureTimeout(ActionData actionData)
	{
		if (!actionData || !actionData.m_Player || !LogZ_Levels.IsEnabled(LogZ_Level.INFO) || !LogZ_Events.IsEnabled(LogZ_Event.ADMIN_ACTIVITY))
			return;

		int playerKey = actionData.m_Player.GetID();
		int intervalMs = LogZ_Config.Get().throttling.juncture_timeout_ms;
		if (intervalMs > 0) {
			if (!s_TimeoutLastMs)
				s_TimeoutLastMs = new map<int, int>();

			int now = g_Game.GetTime();
			int last;
			if (s_TimeoutLastMs.Find(playerKey, last) && (now - last) < intervalMs)
				return;

			s_TimeoutLastMs.Set(playerKey, now);
		}

		ref map<string, string> dto = new map<string, string>();
		string json;

		LogZ_DTO_ActionData actionDTO = new LogZ_DTO_ActionData(actionData);
		if (LogZ.GetSerializer().WriteToString(actionDTO, false, json))
			dto.Insert("action_data", json);

		if (LogZ_GameLogger.SerializeObject(actionData.m_Player, json))
			dto.Insert("player", json);

		int reserved = 0;
		if (actionData.m_ReservedInventoryLocations)
			reserved = actionData.m_ReservedInventoryLocations.Count();

		dto.Insert("reserved_locations", reserved.ToString());

		LogZ.Log("action juncture timeout", LogZ_Level.INFO, LogZ_Event.ADMIN_ACTIVITY, dto);
	}

	/**
	    \brief Name a DayZPlayerSyncJunctures constant; anything else logs as its raw number.
	    \details
	        DayZPlayerSyncJunctures is a set of plain int constants, not an enum, so there is no
	        engine ToString for it. Only the six junctures a client can initiate are named
	        (WithSyncJuncture only calls this for those); every other value the switch could ever
	        see is vanilla-internal traffic, kept nameless on purpose.
	*/
	protected static string JunctureName(int id)
	{
		switch (id) {
		case DayZPlayerSyncJunctures.SJ_DELETE_ITEM:
			return "SJ_DELETE_ITEM";

		case DayZPlayerSyncJunctures.SJ_QUICKBAR_SET_SHORTCUT:
			return "SJ_QUICKBAR_SET_SHORTCUT";

		case DayZPlayerSyncJunctures.SJ_GESTURE_REQUEST:
			return "SJ_GESTURE_REQUEST";

		case DayZPlayerSyncJunctures.SJ_KURU_REQUEST:
			return "SJ_KURU_REQUEST";

		case DayZPlayerSyncJunctures.SJ_INJURY:
			return "SJ_INJURY";

		case DayZPlayerSyncJunctures.SJ_PLAYER_STATES:
			return "SJ_PLAYER_STATES";
		}

		return id.ToString();
	}

	/**
	    \brief Log a client-initiated sync juncture (PLAYER_ACTIVITY, DEBUG; WARN on a delete-item
	        burst).
	    \details
	        Only the six junctures a client can send on its own initiative are logged
	        (`SJ_DELETE_ITEM`, `SJ_QUICKBAR_SET_SHORTCUT`, `SJ_GESTURE_REQUEST`,
	        `SJ_KURU_REQUEST`, `SJ_INJURY`, `SJ_PLAYER_STATES`); everything else this switch could
	        see is vanilla-internal (server-initiated or a reply) and is dropped. `SetToDelete`
	        marks a posted item for deletion and `CanDeleteItems()` only gates the *consumption*
	        side, so a burst of `SJ_DELETE_ITEM` while an action or throw is running (the
	        `thresholds.delete_item_burst` count within `throttling.sync_juncture_ms`, or 1000 ms
	        if that throttle is off) is the dupe-adjacent pattern and always logs at WARN,
	        bypassing the throttle - the burst counter itself is never throttled.
	    \param player     The player the juncture arrived on.
	    \param junctureId DayZPlayerSyncJunctures constant.
	*/
	static void WithSyncJuncture(PlayerBase player, int junctureId)
	{
		if (!player)
			return;

		bool isNamed;
		switch (junctureId) {
		case DayZPlayerSyncJunctures.SJ_DELETE_ITEM:
		case DayZPlayerSyncJunctures.SJ_QUICKBAR_SET_SHORTCUT:
		case DayZPlayerSyncJunctures.SJ_GESTURE_REQUEST:
		case DayZPlayerSyncJunctures.SJ_KURU_REQUEST:
		case DayZPlayerSyncJunctures.SJ_INJURY:
		case DayZPlayerSyncJunctures.SJ_PLAYER_STATES:
			isNamed = true;
			break;
		}

		if (!isNamed)
			return;

		int playerKey = player.GetID();
		bool isBurst = false;
		int burstCount = 0;

		if (junctureId == DayZPlayerSyncJunctures.SJ_DELETE_ITEM) {
			if (!s_DeleteItemCount) {
				s_DeleteItemCount = new map<int, int>();
				s_DeleteItemWindowStart = new map<int, int>();
			}

			int windowMs = LogZ_Config.Get().throttling.sync_juncture_ms;
			if (windowMs <= 0)
				windowMs = 1000;

			int now = g_Game.GetTime();
			int windowStart;
			int count;
			if (!s_DeleteItemWindowStart.Find(playerKey, windowStart) || (now - windowStart) > windowMs) {
				windowStart = now;
				count = 0;
			} else {
				s_DeleteItemCount.Find(playerKey, count);
			}

			count++;
			s_DeleteItemWindowStart.Set(playerKey, windowStart);
			s_DeleteItemCount.Set(playerKey, count);
			burstCount = count;
			isBurst = count >= LogZ_Config.Get().thresholds.delete_item_burst;
		}

		LogZ_Level lvl = LogZ_Level.DEBUG;
		if (isBurst)
			lvl = LogZ_Level.WARN;

		if (!LogZ_Levels.IsEnabled(lvl) || !LogZ_Events.IsEnabled(LogZ_Event.PLAYER_ACTIVITY))
			return;

		int intervalMs = LogZ_Config.Get().throttling.sync_juncture_ms;
		if (intervalMs > 0 && !isBurst) {
			if (!s_SyncLastMs)
				s_SyncLastMs = new map<string, int>();

			string key = string.Format("%1:%2", playerKey, junctureId);
			int now2 = g_Game.GetTime();
			int last;
			if (s_SyncLastMs.Find(key, last) && (now2 - last) < intervalMs)
				return;

			s_SyncLastMs.Set(key, now2);
		}

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (LogZ_GameLogger.SerializeObject(player, json))
			dto.Insert("player", json);

		dto.Insert("juncture", JunctureName(junctureId));

		if (isBurst)
			dto.Insert("burst_count", burstCount.ToString());

		LogZ.Log("sync juncture", lvl, LogZ_Event.PLAYER_ACTIVITY, dto);
	}

	/**
	    \brief Log an RPC whose sender does not own the target player (ADMIN_ACTIVITY, WARN).
	    \details
	        Only the owning client may target a player entity with an RPC; anything else is probe
	        traffic (a mod's `OnRPC` handler acting on wire parameters without validating
	        `sender`, cheat-check 0a's #1 real-world vector). logz cannot audit installed mods,
	        but it can make the probe traffic itself visible. `mismatch_count` is a per-player
	        running total since server start, updated on every mismatch even when the line itself
	        is throttled away, so the analyzer's per-player count (the plan's "session summary")
	        is exact even at a coarse `throttling.rpc_audit_ms`.
	    \param target  The player entity the RPC targeted (`this` in the caller).
	    \param sender  The RPC's actual sender identity.
	    \param rpcType The RPC type value; not resolved to a name because unknown mod values are
	        arbitrary ints outside `ERPCs`.
	*/
	static void WithRpcMismatch(PlayerBase target, PlayerIdentity sender, int rpcType)
	{
		if (!target || !sender)
			return;

		int playerKey = target.GetID();
		int total;
		if (!s_RpcMismatchTotal)
			s_RpcMismatchTotal = new map<int, int>();

		s_RpcMismatchTotal.Find(playerKey, total);
		total++;
		s_RpcMismatchTotal.Set(playerKey, total);

		if (!LogZ_Levels.IsEnabled(LogZ_Level.WARN) || !LogZ_Events.IsEnabled(LogZ_Event.ADMIN_ACTIVITY))
			return;

		int intervalMs = LogZ_Config.Get().throttling.rpc_audit_ms;
		if (intervalMs > 0) {
			if (!s_RpcLastMs)
				s_RpcLastMs = new map<int, int>();

			int now = g_Game.GetTime();
			int last;
			if (s_RpcLastMs.Find(playerKey, last) && (now - last) < intervalMs)
				return;

			s_RpcLastMs.Set(playerKey, now);
		}

		ref map<string, string> dto = new map<string, string>();
		string json;

		if (LogZ_GameLogger.SerializeObject(target, json))
			dto.Insert("player", json);

		dto.Insert("sender_uid", sender.GetPlainId());
		dto.Insert("sender_name", sender.GetName());
		dto.Insert("rpc_type", rpcType.ToString());
		dto.Insert("mismatch_count", total.ToString());

		LogZ.Log("rpc sender mismatch", LogZ_Level.WARN, LogZ_Event.ADMIN_ACTIVITY, dto);
	}
}
#endif
