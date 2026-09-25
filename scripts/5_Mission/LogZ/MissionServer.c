/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2025 WoozyMasta
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief Mission hooks that initialize LogZ.
*/
modded class MissionServer
{
	/**
	    \brief Initialize LogZ before base OnInit.
	*/
	override void OnInit()
	{
		LogZ.Init();

		super.OnInit();

#ifdef METRICZ
		MetricZ_Exporter.Register(new MetricZ_Collector_LogZ());
#endif

		// LogZ_Test.Run();
	}

	/**
	    \brief Log the combat-log kill before the vanilla body handling applies it.
	    \details
	        MissionServer.PlayerDisconnected calls HandleBody after OnDisconnect; a live,
	        unconscious or restrained player is killed there when ShouldPlayerBeKilled says so.
	        Without this line the death looks like a combat death.
	*/
	override void HandleBody(PlayerBase player)
	{
		if (player && player.IsAlive() && ShouldPlayerBeKilled(player))
			LogZ_WorldLogger.WithSessionKill(player, "player killed on logout (unconscious/restrained)", EnumTools.EnumToString(EClientKicked, player.GetKickOffReason()));

		super.HandleBody(player);
	}

	/**
	    \brief Log a respawn of an unconscious or restrained player (vanilla kills it).
	*/
	override void OnClientRespawnEvent(PlayerIdentity identity, PlayerBase player)
	{
		if (player && (player.IsUnconscious() || player.IsRestrained()))
			LogZ_WorldLogger.WithSessionKill(player, "player killed on respawn (unconscious/restrained)", "");

		super.OnClientRespawnEvent(identity, player);
	}

	/**
	    \brief Close LogZ on mission finish after base handler.
	*/
	override void OnMissionFinish()
	{
		LogZ.Close();

		super.OnMissionFinish();
	}

	/**
	    \brief Log player chat messages.
	*/
	override void OnEvent(EventType eventTypeId, Param params)
	{
		if (eventTypeId != ChatMessageEventTypeID) {
			super.OnEvent(eventTypeId, params);
			return;
		}

		ChatMessageEventParams chatParams = ChatMessageEventParams.Cast(params);
		if (chatParams) {
			string msg = string.ToString(chatParams.param3, false, false, false).Trim();
			string sender = string.ToString(chatParams.param2, false, false, false);

			Man man = LogZ_Utils.GetManByName(sender);
			if (man)
				LogZ_GameLogger.WithObject(man, msg, LogZ_Level.INFO, LogZ_Event.PLAYER_CHAT);
			else
				LogZ.Info(string.Format("chat message sender %1 say: %2", sender, msg), LogZ_Event.PLAYER_CHAT);
		}

		super.OnEvent(eventTypeId, params);
	}
}
#endif
