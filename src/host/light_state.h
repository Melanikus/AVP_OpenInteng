// light_state.h: Light Sourcing's own state, the part inteng keeps in memory and never writes to a light's chunk: which lights are ON (Lights > Toggle on/off, Switch all on: FUN_004ac240 changes the light's on field and
// the engine's colour sums, nothing else), and whether the doors are CLOSED (Doors > Close / Open, FUN_004a9f40 / FUN_004aa1b0: the morph shapes' points are exchanged, and Save Intensities lights what is there).
// Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Bishop's ruling 2026-11-15: on / off is editor state only; it is used in the
// calculation exactly as inteng uses it: only lights that are on add to what Save Intensities writes.
//
// A light is named by its place among the NORMALLT set's lights, counting from 0 (light_edit.h); a light the state has no entry for is ON (inteng makes every light on when the screen starts).
#pragma once

#include <stddef.h>

#include <vector>

namespace host
{
	class LightState
	{
	public:
		LightState() : closed(false), flat(false) {}

		// Everything on, doors open: the state when the screen starts
		void Reset();

		bool IsOn(size_t light) const;
		void SetOn(size_t light, bool on);

		// Lights > Toggle on/off: the light's state turned over (FUN_004ac240(light, light is off)). Gives the new state.
		bool Toggle(size_t light);

		// Lights > Switch all on
		void SwitchAllOn();

		// How many lights are off, among the first `count`
		size_t OffCount(size_t count) const;

		bool DoorsClosed() const { return closed; }

		// Hierarchy / Sequence (0xc8 / 0xc9, FUN_004b1720) make every shape's vertex normals again from its polygons, which is what the lighting uses from then on
		bool FlatNormals() const { return flat; }
		void SetFlatNormals(bool value) { flat = value; }
		void SetDoorsClosed(bool value) { closed = value; }

	private:
		std::vector<bool> off;   // true: that light is off; shorter than the lights is fine (the rest are on)
		bool closed;
		bool flat;
	};
}
