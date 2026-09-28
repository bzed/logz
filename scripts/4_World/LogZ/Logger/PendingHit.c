/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2025 WoozyMasta
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief A hit line built before vanilla's EEHitBy body ran and written after it (WP-17).
*/
class LogZ_PendingHit
{
	ref map<string, string> fields = new map<string, string>();
	ref LogZ_DTO_Damage damage;
	string message;
	LogZ_Level level;
	LogZ_Event event_type;
}
#endif
