// light_keys.h: Light Sourcing's KEYS as functions on the open level, as inteng's frame reads them: the selected light's attributes (< and > choose one of 13, + and - change it), I (invisible), U (the changed bit
// cleared), O (the clipboard's numbers over the light), Shift + and Shift - (the ambience), and what Exit / Return write back (engine flags without bits 0x300). Written 2026-11-15 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Built on Frost's light_edit.* (which makes, removes and finds the chunks). The facts: AvP-docs\engine\inteng\drake-lighting-facts.md
// section "Light Sourcing's keys".
//
// WHERE EACH RULE COMES FROM (dump-all-2.txt):
// - the attributes (FUN_004acf30 0x4ad107-0x4ad310, FUN_004ac430 names them): 0 Brightness (+10 a step, held 0 to 0x20000 = 200%), 1 Spread (+2 a step, 1 to 0x800), 2 Range (+10000 a step, not below 0 in inteng; the game's loader
//   refuses 0, so the least here is 1), 3 Shadows (local flag 2), 4 Atten (engine flag 1), 5 Spread Atten (engine flag 2), 6 Omni (engine flag 4), 7 Runtime (local flag 0x10), 8 / 9 / 10 Red / Green / Blue of the colour
//   (+1 a step, 0 to 255), 11 No PreLight (local flag 0x40; on also puts 0x304 in the engine flags, off takes 0x300 out), 12 Specular (engine flag 0x800 taken out when going up, put in when going down). A step of 1 is made
//   10 after the key was held 4 frames and 100 after 31 (the caller's count). Every change puts the local flag 4 ("changed") in.
// - I (FUN_004acf30 0x4ad315): local flag 0x20 (invisible) flipped. U (FUN_004acf30 start): the local flag 4 taken out.
// - O (0x4ad364-0x4ad3f5): the clipboard light's engine record and local flags (with 4 put in) over the light; its place and orientation and number stay.
// - Shift + and Shift - (FUN_004aab60 0x4ab020 / 0x4ab073): the ambience goes up / down 655 (one hundredth of 65536), held 0 to 65536. "Ambience: %d%%" is shown.
// - Exit and Return (FUN_004aa910 / FUN_004aab60 0x4aaf88 -> FUN_004ad630): the light set is written again from the editor's lights, every light's engine flags with the bits 0x300 taken out (FUN_004ad630
//   0x4ae480: & 0xfffffcff, for every STDLIGHT and every placed light's PLOBJLIT); Exit first un-hides every light (FUN_004ab4d0).
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "light_edit.h"

namespace host
{
	int const kLightAttributeCount = 13;

	// The name inteng shows for an attribute ("Brightness", "Spread", ...); "" for a number out of range
	char const *LightAttribute_Name(int attribute);

	// The attribute of the STDLIGHT at that place, changed by `steps` (a whole number, up or down; the caller makes it 1, 10 or 100 by how long the key was held). Done, NoSuchLight, ...
	LightEditResult LightAttribute_Change(OpenLevelFile *file, size_t light, int attribute, int steps);

	// How inteng's panel shows the attribute: "Brightness: 100.00%", "Spread: 15.00 deg", "Range: 4.00m" (the level's scale: the caller gives metres a unit of millimetres), "Shadows: on" ...
	std::string LightAttribute_Words(OpenLevelFile *file, size_t light, int attribute);

	// I: the invisible flag flipped; *nowInvisible says which it is
	LightEditResult LightInvisible_Toggle(OpenLevelFile *file, size_t light, bool *nowInvisible);

	// U: the changed flag taken out
	LightEditResult LightChanged_Clear(OpenLevelFile *file, size_t light);

	// O with the clipboard light's bytes (light_edit.h CopyLight): its numbers and local flags over the light
	LightEditResult LightPasteOver(OpenLevelFile *file, size_t light, LightData const &clipboard);

	// O BETWEEN ANY TWO LIGHTS (FUN_004acf30 0x4ad34e-0x4ad3f5; light-closeout-frost.md TB-L1): inteng's O works on the selected light, a light of the set or a placed object's, with the copied light,
	// either kind: the copied light's engine record (brightness, spread, range, colour, engine flags) over the selected one's, and when BOTH are placed the copied PLOBJLIT's 15 numbers first
	// (0x4ad3be-0x4ad3dd; the selected one's extra words stay); its [5] (which colour T chose) taken (0x4ad3e5); its local flags with 4 (changed) in (0x4ad3f5). The write-back then writes the record
	// into the chunk (FUN_004ad630): a STDLIGHT's brightness, spread, range, colour, engine & ~0x300, local; a PLOBJLIT's brightness, range, the colour to its up ([5] 0) or down ([5] 1) colour,
	// engine & ~0x300, local. Here the chunk's bytes are changed at once, the same numbers: the colour is the copied light's up or down colour by sourceDown (its [5]; a light of the set's [5] is
	// always 0, so its colour goes to a placed light's UP colour), into the same one of a placed target (its [5] becomes the copied one's). NAMED (light-closeout-frost.md NAME-L1): a placed
	// light's spread is its PLOBJLIT's here (inteng's record keeps the headlight's 1000 for every placed light, so its O from a placed light gives a light of the set the spread 1000, and its O onto
	// a placed light changes a spread it never saves); the game does not read a placed light's spread. The engine flags' 0x300 come out at the Exit's write-back (LightsWriteBack), as inteng's.
	struct LightKeyRef
	{
		bool placed = false;          // a placed object's light (its PLOBJLIT), else one of the level's set (its STDLIGHT)
		size_t light = 0;             // the set's light: its place in the list (FindLightChunk)
		uint8_t objectId[8] = { 0 };  // the placed light's object
	};

	LightEditResult LightCopyOver(OpenLevelFile *file, LightKeyRef const &target, LightKeyRef const &source, bool sourceDown);

	// A PLACED-OBJECT light (the PLOBJLIT of the object with that 8-byte ID): the same attributes as a light of the level (FUN_004acf30 works on the light record, whatever kind of light it is). The colours are the
	// object light's UP colour, or its DOWN colour when downColour (T: FUN_004acf30 0x4ad4bf flips the light's [5], and the write-back FUN_004ad630 writes the colour to +0x34 up or +0x38 down by it).
	// INTENG BUG, FIXED (light-closeout-frost.md NAME-L1): a placed light's Spread here is its PLOBJLIT's own (its 2nd number). inteng's record of a placed light is the HEADLIGHT's with only the
	// brightness, the up colour, the range and the engine flags put in (FUN_004ae5d0 0x4aea78-0x4aea9d), so its Spread showed the headlight's 1000 (87.89 deg) for every lamp, and a change went nowhere: the
	// write-back writes a PLOBJLIT's brightness, range, colour, engine and local flags, not its spread (0x4adb03-0x4adcfe). The game does not read a placed light's spread (object_setup.cpp:3390-3420).
	bool PlacedLight_Exists(OpenLevelFile *file, uint8_t const *objectId);
	LightEditResult PlacedLightAttribute_Change(OpenLevelFile *file, uint8_t const *objectId, int attribute, int steps, bool downColour);
	std::string PlacedLightAttribute_Words(OpenLevelFile *file, uint8_t const *objectId, int attribute, bool downColour);

	// D: the numbers of a light dropped from nothing: the camera object's headlight record, FUN_00580580(light, flags 3, type 2, 0, 0, 0, 0, brightness 0x10000, spread 1000, range 4000000) with the colour 0xffffff
	// (FUN_004aa440 0x4aa709 makes it, FUN_004ab860 copies it), the local flags 5 (on, changed); the orientation is the camera's (orientation9: nine numbers, 16.16, the camera's right, down and forward; null keeps the
	// identity). The place and the number are the ones DropLight gives.
	LightData LightDefaultDrop(int32_t const *orientation9);

	// L (fly light): the light is put at the place given (the camera's), the changed flag put in; refused past 2^24 from the middle
	LightEditResult LightPlace_Set(OpenLevelFile *file, size_t light, int32_t const place[3]);

	// THE MOUSE GIZMO's move and turn of a light (movable.h, movable_light.cpp; the owner's addition, 11-18): the place and the orientation (orientation9: nine numbers,
	// 16.16, row by row, its third row the light's direction, as LightDefaultDrop takes the camera's) put in ONE write, with the changed flag, as L puts the place; the
	// rest of the light as it was. Refused past 2^24 from the middle (PlaceOutOfRange), and nothing is changed when the game's loader refuses the level.
	LightEditResult LightPose_Set(OpenLevelFile *file, size_t light, int32_t const place[3], int32_t const orientation9[9]);

	// A light's place and orientation as its STDLIGHT holds them (the payload's +4 and +16)
	LightEditResult LightPose_Get(OpenLevelFile *file, size_t light, int32_t place[3], int32_t orientation9[9]);

	// Shift + and Shift -: the ambience changed by `steps` hundredths; *percent is what it is then
	LightEditResult Ambience_Change(OpenLevelFile *file, int steps, int *percent);

	// The ambience as it is, in percent (the line "Ambience: %d%%" FUN_004aab60 draws at 0x4ab0f0), changing nothing
	LightEditResult Ambience_Percent(OpenLevelFile *file, int *percent);

	// The write-back of Exit and Return: bits 0x300 taken out of the engine flags of every light. *changed says how many lights had one. unhide: Exit also un-hides every light first.
	LightEditResult LightsWriteBack(OpenLevelFile *file, bool unhide, size_t *changed);

	// THE SCREEN'S START (FUN_004ae5d0, called by FUN_004aa440; light-closeout-frost.md TB-L7 / TB-L8): every light's record takes its chunk's local flags WITHOUT 4, changed (a STDLIGHT's 0x4aedb8, a
	// PLOBJLIT's 0x4aeaa8), and the Exit's write-back writes the record's, so a light not changed in this visit is saved without 4; and a PLOBJLIT whose flags word (its 15th number) has bit 4
	// (PlacedLightFlag_NoSpecular) loses it and gets engine flag 0x800 (LFlag_NoSpecular) instead, in the chunk itself (0x4aea2e-0x4aea3a; the game takes either the same, object_setup.cpp:3398). Here the
	// chunks are changed at once, the same end. On the stock levels no light has either, so nothing changes there. *changed: how many lights were changed.
	LightEditResult LightsAtStart(OpenLevelFile *file, size_t *changed);
}
