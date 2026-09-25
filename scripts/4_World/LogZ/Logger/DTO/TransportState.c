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
        Filled by CarScript.LogZ_Snapshot (WP-6). Extends the plain transport DTO, so the
        base fields (id, speed, pos, members...) stay identical to other transport events.
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
		FillState(obj);
	}

	/**
	    \brief Populate car telemetry, fluids, seated crew and the doors of the occupied seats.
	*/
	void FillState(Object obj)
	{
		CarScript car;
		if (!Class.CastTo(car, obj))
			return;

		speedometer = car.GetSpeedometer();
		rpm = car.EngineGetRPM();
		gear = car.GetCurrentGear();
		engine_on = car.EngineIsOn();

		fuel = car.GetFluidFraction(CarFluid.FUEL);
		oil = car.GetFluidFraction(CarFluid.OIL);
		brake = car.GetFluidFraction(CarFluid.BRAKE);
		coolant = car.GetFluidFraction(CarFluid.COOLANT);

		crew = new array<ref LogZ_DTO_TransportSeat>();
		doors = new map<string, int>();

		int crewSize = car.CrewSize();
		for (int i = 0; i < crewSize; ++i) {
			Man man = Man.Cast(car.CrewMember(i));
			if (man) {
				LogZ_DTO_TransportSeat seatDto = new LogZ_DTO_TransportSeat();
				seatDto.seat = i;
				seatDto.member = new LogZ_DTO_Man(man);
				crew.Insert(seatDto);
			}

			string slot = car.GetDoorInvSlotNameFromSeatPos(i);
			if (slot != "")
				doors.Set(slot, car.GetCarDoorsState(slot));
		}
	}
}
#endif
