// particle_edit.h: inteng's own "Particle generator properties" window (Object Placement's Edit of a particle generator: 0x4e68f0 -> FUN_004e7310 builds it, FUN_004e78c0 fills it, FUN_004e7750 runs it
// and keeps what it holds; its method FUN_004e79b0 answers the buttons) as numbers. The window is drawn by the editor (editor_main.cpp ParticleWindowShow).
// Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng (A-8, reassess/object-placement.md OP-134). AVP_OpenInteng's own code: see License.txt.
//
// WHAT inteng DOES (re-read 11-16 on dump-full-3 / dump-orphans-3 / listing-all-3):
// - The window (FUN_0057d170(400, 0xfa)): Ok (175, 225, 80 x 18); "Type" (18, 40, 70 x 18) and the type's name (100, 40); "Sound" (18, 60) and the sound's name, "None" (0x5d126c) when there is none (100, 60);
//   "Remove Sound" (18, 80, 120 x 18); "Active" (40, 100) and its box (18, 100), ticked when flag bit 0 is clear; the Frequency edit (18, 120, 100 x 18, 10 letters) "%.1f" (0x5e8f88) of time x 0.1 and
//   "Frequency (seconds) (0-6000)" (118, 120); the Probability number box (18, 140) and "Probability(0-100)" (118, 140); the Quantity number box (18, 160) and "Quantity(1-1000)" (118, 160); the Speed edit
//   (18, 180, 10 letters) "%.2f" (0x5e8f80) of speed x 0.01 and "Speed (m/s) (0-600)" (118, 180).
// - Type (control 2) opens "Select Particle Generator Type" (FUN_004e6950: the list Intermittent Sparks, Steam, Black smoke, Flame, FUN_004d42f0) and shows the pick's name; the window stays.
//   Remove Sound (6) takes the sound away ("None"); the window stays. Sound (4): no sound: a new one is made (FUN_004ebc80(0, 1)); a sound: its Sound Edit (FUN_004ebd60); either way the window ENDS.
//   Ok (1) ends it. Return and Escape end it too, and EVERY way out keeps what the window holds (FUN_004e7750 reads the controls whatever FUN_0057c800 answered).
// - What is kept (FUN_004e7750): flag bit 0 = NOT Active; the frequency read by sscanf "%f" (0x5d3f10), kept when 0 <= v < 6000.0: time = ftol(v x 10); the speed alike, 0 <= v < 600.0: speed = ftol(v x 100);
//   the probability kept when 0 .. 100, the quantity when 1 .. 1000; then by the type: 0 -> speed and quantity 0; 1 .. 3 -> probability and time 0.
// INTENG BUG, FIXED (MAPPING-ORDERS section 15; named): the two floats are read into a variable sscanf leaves as it was when the text is no number (0x4e77ab, 0x4e7800: not set before), so a word kept
// whatever the stack held; ours keeps the old time / speed then. And ftol(v x 10.0f) / ftol(v x 100.0f) truncate (2.3 seconds kept 22 tenths, 0x4e77ec / 0x4e7841): ours rounds to the
// nearest, as the generator's rate (generator_settings.h).
#pragma once

#include <stdint.h>

#include <string>

#include "special_objects_edit.h"

namespace host
{
	// FUN_004d42f0: a type's words ("Unknown" past 3)
	std::string Particle_TypeName(int32_t type);

	// What the window shows (FUN_004e78c0)
	struct ParticleShown
	{
		std::string typeName;
		std::string soundName;     // "None" when the generator has no sound
		bool active = false;
		std::string frequency;     // "%.1f"
		int32_t probability = 0;
		int32_t quantity = 0;
		std::string speed;         // "%.2f"
	};

	ParticleShown Particle_Shown(SpecialObject const &generator);

	// What the window holds when it ends
	struct ParticleTyped
	{
		int32_t type = 0;
		bool active = false;
		std::string frequency;
		int32_t probability = 0;   // the number box's value (FUN_0057d620)
		int32_t quantity = 0;
		std::string speed;
	};

	// FUN_004e7750's write-back: the generator with what the window holds
	SpecialObject Particle_Ok(SpecialObject const &generator, ParticleTyped const &typed);
}
