/*
    SPDX-License-Identifier: GPL-3.0-or-later
    Copyright (c) 2026 Bernd Zeimetz <bernd@bzed.de>
    Source: https://github.com/woozymasta/logz
*/

#ifdef SERVER
/**
    \brief Crew member with the seat index it occupies.
*/
class LogZ_DTO_TransportSeat
{
	int seat; // CrewPositionIndex
	ref LogZ_DTO_Man member;
}

/**
    \brief Serializable periodic state of a transport (msg "transport snapshot").
    \details
        Contract-only until the vehicle sampler lands (WP-6); nothing serializes
        this class yet. Extends the plain transport DTO, so the base fields
        (id, speed, pos, members...) stay identical to other transport events.
*/
class LogZ_DTO_TransportState : LogZ_DTO_Transport
{
	float speedometer; // Car.GetSpeedometer()
	float rpm; // Car.EngineGetRPM()
	int gear; // Car.GetCurrentGear()
	bool engine_on;

	// Fluid levels as fractions of capacity (0..1)
	float fuel;
	float oil;
	float brake;
	float coolant;

	ref array<ref LogZ_DTO_TransportSeat> crew;
	ref map<string, int> doors; // door slot name -> CarDoorState

	/**
	    \brief Construct DTO from object.
	*/
	void LogZ_DTO_TransportState(Object obj)
	{
		Fill(obj);
		FillTransport(obj);
	}
}
#endif
