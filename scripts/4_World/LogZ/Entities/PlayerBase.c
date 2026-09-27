/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2025 WoozyMasta
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
modded class PlayerBase
{
	protected bool m_LogZ_InitDone;
	protected bool m_LogZ_Killed;
	protected int m_LogZ_LastSnapshotTime;
	protected string m_LogZ_SteamId;
	protected string m_LogZ_PlayerName;

	bool LogZ_ShouldLogged()
	{
		return (m_PlayerSelected && m_LogZ_InitDone);
	}

	bool LogZ_IsAlreadyKilled()
	{
		return m_LogZ_Killed;
	}

	// * --- create ---
	override void EEOnCECreate()
	{
		super.EEOnCECreate();

		LogZ_GameLogger.WithObject(
		    this, "player created from CE",
		    LogZ_Level.DEBUG, LogZ_Event.SYSTEM_WORLD);
	}

	// * --- load ---
	override bool OnStoreLoad(ParamsReadContext ctx, int version)
	{
		if (!super.OnStoreLoad(ctx, version))
			return false;

		LogZ_GameLogger.WithObject(
		    this, "player loaded from DB",
		    LogZ_Level.DEBUG, LogZ_Event.SYSTEM_WORLD);

		return true;
	}

	// * --- kill ---
	override void EEKilled(Object killer)
	{
		if (!LogZ_IsAlreadyKilled())
			LogZ_WorldLogger.WithKiller(this, killer, LogZ_Level.INFO);
		m_LogZ_Killed = true;

		super.EEKilled(killer);
	}

	override void OnDamageDestroyed(int oldLevel)
	{
		super.OnDamageDestroyed(oldLevel);

		if (!LogZ_IsAlreadyKilled())
			LogZ_GameLogger.WithObject(
			    this, "player death",
			    LogZ_Level.INFO, LogZ_Event.PLAYER_KILL);
		m_LogZ_Killed = true;
	}

	// * --- hit ---
	override void EEHitBy(TotalDamageResult damageResult, int damageType, EntityAI source, int component, string dmgZone, string ammo, vector modelPos, float speedCoef)
	{
		// EEHitBy runs before OnDamageDestroyed/EEKilled, so the lethal hit is still logged;
		// only hits after the kill was logged (corpses, ruins) are skipped.
		if (!LogZ_IsAlreadyKilled())
			LogZ_WorldLogger.WithHit(this, source, damageResult, damageType, component, dmgZone, ammo, modelPos, speedCoef, LogZ_Level.INFO);

		super.EEHitBy(damageResult, damageType, source, component, dmgZone, ammo, modelPos, speedCoef);
	}

	// * --- cargo in ---
	override void EECargoIn(EntityAI item)
	{
		super.EECargoIn(item);
		LogZ_EECargoIn(item);
	}

	private void LogZ_EECargoIn(EntityAI item)
	{
		if (LogZ_ShouldLogged())
			LogZ_GameLogger.WithObjectAndOwner(
			    item, this, "added item to player",
			    LogZ_Level.INFO, LogZ_Event.INVENTORY_IN);
	}

	// * --- cargo out ---
	override void EECargoOut(EntityAI item)
	{
		super.EECargoOut(item);

		LogZ_EECargoOut(item);
	}

	private void LogZ_EECargoOut(EntityAI item)
	{
		if (LogZ_ShouldLogged())
			LogZ_GameLogger.WithObjectAndOwner(
			    item, this, "removed item from player",
			    LogZ_Level.INFO, LogZ_Event.INVENTORY_OUT);
	}

	// * --- attach ---
	override void EEItemAttached(EntityAI item, string slot_name)
	{
		super.EEItemAttached(item, slot_name);

		LogZ_EEItemAttached(item, slot_name);
	}

	private void LogZ_EEItemAttached(EntityAI item, string slot_name)
	{
		if (LogZ_ShouldLogged())
			LogZ_GameLogger.WithObjectAndOwner(
			    item, this, "attached item to player",
			    LogZ_Level.INFO, LogZ_Event.INVENTORY_IN, slot_name);
	}

	// * --- detach ---
	override void EEItemDetached(EntityAI item, string slot_name)
	{
		super.EEItemDetached(item, slot_name);

		LogZ_EEItemDetached(item, slot_name);
	}

	private void LogZ_EEItemDetached(EntityAI item, string slot_name)
	{
		if (LogZ_ShouldLogged())
			LogZ_GameLogger.WithObjectAndOwner(
			    item, this, "detached item from player",
			    LogZ_Level.INFO, LogZ_Event.INVENTORY_OUT, slot_name);
	}

	// * --- hands in ---
	override void EEItemIntoHands(EntityAI item)
	{
		super.EEItemIntoHands(item);

		LogZ_EEItemIntoHands(item);
	}

	private void LogZ_EEItemIntoHands(EntityAI item)
	{
		if (LogZ_ShouldLogged())
			LogZ_GameLogger.WithObjectAndOwner(
			    item, this, "received item into player hands",
			    LogZ_Level.INFO, LogZ_Event.INVENTORY_IN, "Hands");
	}

	// * --- hands out ---
	override void EEItemOutOfHands(EntityAI item)
	{
		super.EEItemOutOfHands(item);

		LogZ_EEItemOutOfHands(item);
	}

	private void LogZ_EEItemOutOfHands(EntityAI item)
	{
		if (item)
			LogZ_GameLogger.WithObjectAndOwner(
			    item, this, "out item from player hands",
			    LogZ_Level.INFO, LogZ_Event.INVENTORY_OUT, "Hands");
	}

	// * --- session ---
	override void OnSelectPlayer()
	{
		super.OnSelectPlayer();

		LogZ_WorldLogger.WithPlayer(
		    this, "player selected",
		    LogZ_Level.DEBUG, LogZ_Event.PLAYER_SESSION);
	}

	// The identity is released before OnDisconnect runs when the network or BattlEye ends the session
	// (UNSTABLE_NETWORK, TIMEOUT, AUTH_CANCELED, BATTLEYE), so the disconnect line would name nobody.
	// Remember it while it is available.
	protected void LogZ_RememberIdentity()
	{
		PlayerIdentity identity = GetIdentity();
		if (!identity)
			return;

		m_LogZ_SteamId = identity.GetPlainId();
		m_LogZ_PlayerName = identity.GetName();
	}

	override void OnConnect()
	{
		LogZ_RememberIdentity();

		// Hive.CharacterIsLoginPositionChanged is "only valid during login" (hive.c) and OnConnect
		// is called from MissionServer.InvokeOnConnect while the player logs in; read it before
		// the vanilla connect work. -1 = no hive, the field is omitted. Whether the value is
		// meaningful here is verified on live data (analyzer plan, "Login position").
		int loginPositionChanged = -1;
		Hive hive = GetHive();
		if (hive) {
			if (hive.CharacterIsLoginPositionChanged(this))
				loginPositionChanged = 1;
			else
				loginPositionChanged = 0;
		}

		super.OnConnect();

		m_LogZ_InitDone = true;
		LogZ_WorldLogger.WithConnect(this, "player connected", loginPositionChanged);
	}

	override void OnReconnect()
	{
		super.OnReconnect();

		LogZ_GameLogger.WithObject(
		    this, "player reconnected",
		    LogZ_Level.INFO, LogZ_Event.PLAYER_SESSION);
	}

	override void OnDisconnect()
	{
		// server-only native (dayzplayer.c); read before the vanilla disconnect work runs
		EClientKicked kickReason = GetKickOffReason();

		super.OnDisconnect();

		m_LogZ_InitDone = false;
		LogZ_GameLogger.WithDisconnect(this, "player disconnected", kickReason, m_LogZ_SteamId, m_LogZ_PlayerName);
	}

	// * --- movement snapshot ---
	// Server-side: MissionServer.TickScheduler calls OnTick -> OnScheduledTick for each
	// connected player. Players without an identity (eAI) are skipped when
	// filters.skip_ai_snapshots is set.
	override void OnScheduledTick(float deltaTime)
	{
		super.OnScheduledTick(deltaTime);
		LogZ_Snapshot();
	}

	protected void LogZ_Snapshot()
	{
		if (!LogZ_Config.IsLoaded() || !IsPlayerSelected() || !IsAlive())
			return;

		int intervalMs = LogZ_Config.Get().throttling.player_snapshot_s * 1000;
		if (intervalMs <= 0)
			return;

		if (LogZ_Config.Get().filters.skip_ai_snapshots && !GetIdentity())
			return;

		int time = g_Game.GetTime();
		if ((time - m_LogZ_LastSnapshotTime) < intervalMs)
			return;

		m_LogZ_LastSnapshotTime = time;
		LogZ_RememberIdentity();
		LogZ_WorldLogger.WithPlayerSnapshot(this);
	}

	// * --- unconscious ---
	override void OnUnconsciousStart()
	{
		super.OnUnconsciousStart();

		LogZ_GameLogger.WithObject(
		    this, "player start unconscious",
		    LogZ_Level.INFO, LogZ_Event.PLAYER_ACTIVITY);
	}

	override void OnUnconsciousStop(int pCurrentCommandID)
	{
		super.OnUnconsciousStop(pCurrentCommandID);

		LogZ_GameLogger.WithObject(
		    this, "player stop unconscious",
		    LogZ_Level.INFO, LogZ_Event.PLAYER_ACTIVITY);
	}

	// * --- restrain ---
	override void SetRestrained(bool is_restrained)
	{
		super.SetRestrained(is_restrained);

		if (is_restrained)
			LogZ_GameLogger.WithObject(
			    this, "player restrained",
			    LogZ_Level.INFO, LogZ_Event.PLAYER_ACTIVITY);
		else
			LogZ_GameLogger.WithObject(
			    this, "player stop restrain",
			    LogZ_Level.INFO, LogZ_Event.PLAYER_ACTIVITY);
	}

	// * --- movement ---
	override void OnCommandFallStart()
	{
		super.OnCommandFallStart();

		LogZ_GameLogger.WithObject(
		    this, "player start falling down",
		    LogZ_Level.DEBUG, LogZ_Event.PLAYER_ACTIVITY);
	}

	override void OnCommandFallFinish()
	{
		super.OnCommandFallFinish();

		LogZ_GameLogger.WithObject(
		    this, "player falling down",
		    LogZ_Level.INFO, LogZ_Event.PLAYER_ACTIVITY);
	}

	override void OnCommandSwimStart()
	{
		super.OnCommandSwimStart();

		LogZ_GameLogger.WithObject(
		    this, "player start swimming",
		    LogZ_Level.INFO, LogZ_Event.PLAYER_ACTIVITY);
	}

	override void OnCommandSwimFinish()
	{
		super.OnCommandSwimFinish();

		LogZ_GameLogger.WithObject(
		    this, "player finish swimming",
		    LogZ_Level.INFO, LogZ_Event.PLAYER_ACTIVITY);
	}

	override void OnJumpOutVehicleFinish(float carSpeed)
	{
		super.OnJumpOutVehicleFinish(carSpeed);

		LogZ_GameLogger.WithObject(
		    this, string.Format("player jump out from vehicle on speed %1", carSpeed),
		    LogZ_Level.INFO, LogZ_Event.PLAYER_ACTIVITY);
	}

	// Client-initiated sync junctures (WP-13, plan 2.13). super runs first: the rule from plan 2.4
	// applies here too, the override must not read from pCtx itself before the vanilla body has
	// consumed the stream for this juncture type, or later junctures desync.
	override void OnSyncJuncture(int pJunctureID, ParamsReadContext pCtx)
	{
		super.OnSyncJuncture(pJunctureID, pCtx);

		LogZ_JunctureLogger.WithSyncJuncture(this, pJunctureID);
	}

	// RPC sender/target mismatch audit (WP-13, plan 2.14): only the owning client may target this
	// player entity with an RPC, so a mismatched sender is probe traffic (a mod's OnRPC handler
	// acting on wire parameters without validating sender, or a hostile client). An eAI (no
	// identity) is never a target of a real RPC and is skipped.
	override void OnRPC(PlayerIdentity sender, int rpc_type, ParamsReadContext ctx)
	{
		super.OnRPC(sender, rpc_type, ctx);

		PlayerIdentity ownIdentity = GetIdentity();
		if (sender && ownIdentity && sender.GetPlainId() != ownIdentity.GetPlainId())
			LogZ_JunctureLogger.WithRpcMismatch(this, sender, rpc_type);
	}
}
#endif
