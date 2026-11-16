// sequence_speed.h: a sequence's PLATFORM SPEED and ANGLE (the OBASEQSP chunk) and the rules of inteng's Hierarchy-mode commands Set speed (0x4a) and Set angle (0x4b).
// Hierarchy mode, slice 2. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT IS. A sequence (a child of a hierarchy's animation) may hold an OBASEQSP chunk: after the 12-byte heading, the sequence's speed in millimetres a second (4),
// its angle in degrees (4, "mainly for tools use") and a spare (4) (avpx 3dc/win95/animation_objects.hpp:186-205, .cpp:431-445; the chunk is read when it holds at
// least 12 bytes). inteng's commands (AvP-docs\engine\inteng\p11-shape-handlers.md section 60):
// - Set speed 0x4a (the case at out\dump-all-2.txt:24024): prompt "Platform speed in mm/s" (default = the last speed typed); on OK a number below 0 is ignored, any other
//   becomes the last speed; and only when a hierarchy is open, a sequence is selected and the "platform" switch (0x4c, Toggle on/off) is on, FUN_0047db10 writes it.
// - Set angle 0x4b (:24034): prompt "Platform angle (0-360)" (default = the last angle); on OK a number from 0 to 360 (below 361) becomes the last angle, a display
//   turn of angle x 4096 / 360 (integer division) is made, and under the same three conditions FUN_0047db10 writes.
// - FUN_0047db10 (:"== FUN_0047db10"): finds the sequence's OBASEQSP child or makes one (its three numbers 0), then puts the LAST SPEED in the speed place and the LAST
//   ANGLE in the angle place (both, whichever command ran), leaves the spare, and marks the level changed (DAT_00637134 = 1).
// THE RULES: the payload is read and written by the game's own layout; a payload shorter than 12 bytes is refused; extra bytes are kept by the same-size writer.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

namespace model
{
	size_t const kSequenceSpeedBytes = 12;   // speed, angle, spare

	struct SequenceSpeed
	{
		int32_t speedMmPerSecond;
		int32_t angleDegrees;
		int32_t spare;
	};

	enum class SequenceSpeedResult
	{
		Ok,
		BadCall,
		TooShort,       // fewer than 12 bytes
		NotAccepted,    // a command's typed number is not one inteng takes (a negative speed, an angle outside 0 to 360)
		WrongSize,      // a writer's bytes are shorter than 12
	};

	char const *SequenceSpeedWords(SequenceSpeedResult result);

	SequenceSpeedResult ReadSequenceSpeed(uint8_t const *payload, size_t size, SequenceSpeed *out);

	// Puts the three numbers into the first 12 bytes of *payload (the vector's own size, at least 12); every byte after them is left as it was.
	SequenceSpeedResult PutSequenceSpeed(SequenceSpeed const &value, std::vector<uint8_t> *payload);

	// The state of the commands: the last speed and the last angle typed (inteng's DAT_005d4e4c and DAT_00637114; at the start of the program they hold what the exe's
	// data gives: NOT read here, the caller passes them in)
	struct PlatformState
	{
		int32_t lastSpeed;
		int32_t lastAngle;
	};

	// Set speed: typed < 0 is NotAccepted and nothing changes; else state->lastSpeed = typed. Whether the chunk is then written (ApplyPlatformState) is the caller's: it
	// knows the three conditions (a hierarchy open, a sequence selected, the platform switch on).
	SequenceSpeedResult SetPlatformSpeed(PlatformState *state, int32_t typed);

	// Set angle: typed outside 0 to 360 is NotAccepted and nothing changes; else state->lastAngle = typed. *displayTurn (when not null) gets angle x 4096 / 360 (the
	// integer division of 0x4898b5), the turn the screen shows the platform at.
	SequenceSpeedResult SetPlatformAngle(PlatformState *state, int32_t typed, int32_t *displayTurn);

	// FUN_0047db10: the chunk's speed and angle become the state's last speed and last angle. *chunk is the existing chunk's numbers, or all zeros for a new one.
	void ApplyPlatformState(PlatformState const &state, SequenceSpeed *chunk);
}
