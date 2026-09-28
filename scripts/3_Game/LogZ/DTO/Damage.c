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

	// Hit forensics from EEHitBy (WP-2). component == -1 means "not captured"
	// and model_pos / speed_coef must be ignored.
	int component = -1; // hit component index
	vector model_pos; // hit position in model space
	float speed_coef = -1; // projectile speed damage coefficient

	// Shot anatomy (WP-17, contract batch 3). Defaults mean "not captured": an old line, a victim
	// that is not a body, or a hit without a real-player attacker.
	float damage_blood = -1; // Blood damage of the hit, what vanilla feeds to the bleeding roll
	float damage_shock = -1; // Shock damage of the hit
	int bleeding_added = -1; // bleeding sources the hit opened (player victims only)

	// Server-side line of sight from the attacker's head to the hit point, only for real-player
	// FIRE_ARM/CLOSE_COMBAT hits on a player, zombie or animal (filters.hit_los).
	// "" - not captured, "skipped" - eligible but not traceable (no component or bone),
	// "clear" - nothing but the victim and the attacker's own gear on the ray,
	// "blocked" - los_contact / los_object name what stopped it. los_from is the ray origin.
	string los;
	vector los_contact;
	string los_object; // type of the first blocking object, empty for terrain
	vector los_from;

	/**
	    \brief Construct DTO from TotalDamageResult, type and zone.
	*/
	void LogZ_DTO_Damage(TotalDamageResult damageResult, int damageType, string dmgZone, string ammo, int hitComponent = -1, vector hitModelPos = "0 0 0", float hitSpeedCoef = -1)
	{
		if (damageResult) {
			damage = damageResult.GetDamage(dmgZone, "");
			damage_blood = damageResult.GetDamage(dmgZone, "Blood");
			damage_shock = damageResult.GetDamage(dmgZone, "Shock");
		} else {
			damage = 0.0;
		}

		damage_zone = dmgZone;
		damage_type = EnumTools.EnumToString(DamageType, damageType);
		ammo_type = ammo;

		component = hitComponent;
		model_pos = hitModelPos;
		speed_coef = hitSpeedCoef;
	}
}
#endif
