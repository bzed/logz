/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2025 WoozyMasta
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief Serializable damage snapshot for hit/kill events.
*/
class LogZ_DTO_Damage
{
	float damage;
	string damage_zone;
	string damage_type;
	string ammo_type;

	// Hit forensics from EEHitBy (WP-2). Until populated: component == -1
	// means "not captured" and model_pos / speed_coef must be ignored.
	int component = -1; // hit component index
	vector model_pos; // hit position in model space
	float speed_coef = -1; // projectile speed damage coefficient

	/**
	    \brief Construct DTO from TotalDamageResult, type and zone.
	*/
	void LogZ_DTO_Damage(TotalDamageResult damageResult, int damageType, string dmgZone, string ammo)
	{
		if (damageResult)
			damage = damageResult.GetDamage(dmgZone, "");
		else
			damage = 0.0;

		damage_zone = dmgZone;
		damage_type = EnumTools.EnumToString(DamageType, damageType);
		ammo_type = ammo;
	}
}
#endif
