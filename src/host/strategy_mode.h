// strategy_mode.h: inteng's Strategy mode (screen 9, FUN_00522980 / FUN_00524e90) commands that change the level's data, one function each, keyed by the
// command number of the menu record that runs them (engine\inteng\strategy-menu-pairs.md). Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The way every edit here is done is object_edit.h's and object_type_edit.h's: the level is written out as the next save would write it, the object's
// chunks are made again, the sizes around are made right, and OpenLevelFile::ReplaceLevel reads the result with the game's own loader; if the loader
// won't take it, nothing has changed. After a success every Chunk * the editor held is gone, and an object is named again by its 8-byte ID.
//
// DELETE STRATEGY, command 0xc (FUN_00528790, apone-reads-6 / reads2-apone-6.txt:633-666), what inteng does: when there is a current object and it has a
// strategy chunk (object + 0x14: its AVPSTRAT chunk, whose +0x30 is the strategy: FUN_00528340 reads it so), it asks "Are you sure you want to delete the
// strategy on this object?" (the string at 0x5ee254, with a line break after "the"), and only on the answer 3 (yes) it destroys THAT CHUNK (the chunk's own
// destructor with flag 1) and clears the pointer. Nothing else: the OBJPRJDT it was in stays, even when that leaves it empty (stock levels have such
// OBJPRJDTs: 2 of the 63), the object's header and flags stay, and no other object is looked at. With no object or no strategy it does nothing at all.
//
// NAMED POINTS (none changes the data effect of an allowed delete):
//  - inteng works on any object that has the chunk; so does this (a module's, a simple object's, a placed object's).
//  - Which AVPSTRAT is "the" strategy chunk when an OBJPRJDT holds more than one is not read: the FIRST AVPSTRAT of the FIRST OBJPRJDT, as Change Object
//    Type's reading of the same lookups.
//  - inteng deletes without looking whether other objects name this one (a door a switch opens); neither does this: the game's loader decides what it
//    takes. (object_edit.h's DeleteStrategy refuses in that case and takes the OBJPRJDT with it: Drake's, for the old editor's flow.)
//  - The question is the editor's (typed yes or no); the words are inteng's.
// RESET FAN WIND SPEED, command 0x43 (FUN_005354d0, reads2-apone-6.txt:1876-1959): asks "Would you like to reset the wind strength for all fans" (the string at
// 0x5efd24) and, only on the answer 3 (yes), takes every object whose strategy is of kind 0x4b (75, a fan: FUN_00536b10(list, 0x4b) collects them) and sets
// that strategy's number at +0x1c to 100 and the one at +0x20 to 0 (in the file these are the AVPSTRAT payload's own offsets: the fan's wind strength at 28 and
// the pad after it at 32, model/strategy_fields.cpp FanFields). Nothing else changes. Named: one image for all the fans (all or nothing, read by the loader first);
// a strategy too short to hold the two numbers is left alone (inteng's class always has them).
// ADD / REMOVE FLAG (FUN_0052f360(list, bit, add), dump-all-3.txt; the commands: 0xe add / 0xf remove Airduct 0x80000000, 0x11 / 0x12 Stair 0x40000000, 0x14 / 0x15 Sky Visible
// 0x20000000, 0x17 / 0x18 Fogged 0x10000000, the set inteng shows; FUN_00525700's switch, dump-all-3.txt:58754-59544): for every module of the picked list that is not a placed
// object (header flags & 0x400 clear): its MODULEDT (made, last in the object, when it is missing and the flag is being added; a removal skips a module without one), its first
// MODFLAGS (made, last in the MODULEDT, 8 zero bytes, when missing and adding; a removal skips), and the flag word is |= bit (add) or &= ~bit (remove). The count of the loop's modules does not matter: a module twice is the same. Named: one image for all the modules (read by the loader first).
// STABILITY FLAGS (command 0x37, FUN_00527370 / FUN_00527020 / FUN_00527210, dump-all-3.txt): six faces of the picked module's shape are coloured red (stable) or green (unstable)
// while the mode runs; on "Would you like to keep the changes?" (the string at 0x5ee000, answer yes) FUN_00527210 writes them into the SHAPE HEADER's flag word (SHPHEAD1's
// first number: FUN_0050e9e0 + 0x50): face 0 = bit 0x80000, 1 = 0x40000, 2 = 0x20000, 3 = 0x10000, 4 = 0x8000, 5 = 0x4000, each SET when the face is red and CLEARED when green.
// Nothing else of the header changes. Which face is which (the box FUN_00447f10 builds) is not read: the bits are named by their place in this order.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

class OpenLevelFile;   // the editor's own open of a level (src/io/level_file.h)

namespace host
{
	enum class StrategyModeResult
	{
		Done,
		BadCall,         // no level, or no ID
		NoObject,        // no object has that ID
		NoStrategy,      // the object has no strategy chunk: inteng does nothing
		CantBuild,       // the level couldn't be put together from its bytes
		LoaderRefused,   // the game's own loader wouldn't take the changed level, so nothing was changed
	};

	// What an answer means, in plain words for the editor's status line: static text, never null
	char const *StrategyModeWords(StrategyModeResult result);

	// inteng's question for Delete Strategy, as the string at 0x5ee254 holds it (the line break is a real one)
	char const *StrategyMode_DeleteQuestion();

	// Whether the object has a strategy chunk (the first AVPSTRAT of its first OBJPRJDT): Delete Strategy does nothing without one, and asks nothing.
	StrategyModeResult StrategyMode_Has(OpenLevelFile *file, uint8_t const *id);   // Done = it has one; NoStrategy = it has none

	// StrategyMode_Has's answer for EACH of the ids (8 bytes each, count of them), in their order, into answers. The same answers, but the level's image is built and walked
	// ONCE for all of them, where StrategyMode_Has builds the whole level again for each (Vasquez 2026-11-24: Strategy Data's green marks asked it for every object of the level
	// each time the level changed, about 6 ms an object: 3 s and more on Derelict after every Create Virtual Object, the owner's "3 second lock")
	void StrategyMode_HasEach(OpenLevelFile *file, uint8_t const *ids, size_t count, std::vector<StrategyModeResult> *answers);

	// inteng's question for Reset fan wind speed (the string at 0x5efd24)
	char const *StrategyMode_ResetFansQuestion();

	// Reset fan wind speed, the answer yes: *fans (when not null) is how many fans were set. NoStrategy when the level has no fan (nothing changes).
	StrategyModeResult StrategyMode_ResetFans(OpenLevelFile *file, size_t *fans);

	// The flag word change above for the modules with those 8-byte IDs (count of them). *changed (when not null) is how many modules were changed (a module with nothing to remove
	// or that is a placed object is not counted). NoStrategy is not used: Done with *changed 0 changes nothing (the level is left as it was).
	StrategyModeResult StrategyMode_SetModuleFlag(OpenLevelFile *file, uint8_t const *ids, size_t count, uint32_t bit, bool add, size_t *changed);

	// THE FLAG COMMANDS of FUN_00525700's switch (dump-all-3.txt:58905-59110): a module-flag item's command number is one of three in a row (add, remove, view), and each add or remove is one or
	// two FUN_0052f360 calls on the picked modules, in this order. Gives how many steps the command has (0: not an add or remove flag command, so nothing to do here), the steps in
	// steps[0..1]. Doorway (0x20) clears 0x80000 then sets 0x40000, Not a Doorway (0x23) clears 0x40000 then sets 0x80000; Message (0x2c / 0x2d) is the message number's bits, see
	// kStrategyMessageBits (the number is typed, 0 to 127, FUN_00525700 0x525b9e). The view commands (the third of each row) colour modules: StrategyMode_IsViewFlagCommand.
	struct StrategyFlagStep
	{
		uint32_t bit;
		bool add;
	};

	size_t StrategyMode_FlagSteps(int32_t command, StrategyFlagStep *steps);

	// A message number goes in bits 21 to 27 (0xfe00000): the old ones are cleared, then number << 21 is set
	uint32_t const kStrategyMessageBits = 0xfe00000;
	int32_t const kStrategyMessageCommand = 0x2c;
	int32_t const kStrategyMostMessage = 127;

	// A "view flag" command (FUN_0052f680(bit), the third of a row): the modules that hold the flag are coloured; the bit, or 0 when the command is not one
	uint32_t StrategyMode_ViewFlagBit(int32_t command);

	// DELETE MULTIPLE STRATEGIES (0x42, FUN_005376a0 the dialog "Select Strategies To Delete", FUN_005380d0 the work, FUN_00536bd0(type): every object with a strategy of that kind loses it, and a virtual
	// object (FUN_005396d0's kind) with one is destroyed whole; dump-all-3.txt:70439-71100, 69899-69990). The dialog's groups and what each deletes, by the strategy kinds of avpx strategy_chunk.hpp:
	//   Switches = BinSwitch 52, LinkSwitch 61, MultiSwitch 66, AreaSwitch 68 (0x34, 0x3d, 0x42, 0x44);  Lighting effects = 64 (0x40);  Mission Objective = 70 (0x46);  Messages = 73 (0x49; the
	//   dialog's text at 0x5edb98);  Volume of Death = 77 (0x4d);  Self Destruct = 78 (0x4e).
	// NOT HERE (named): Obsolete Switches (its box IS read: FUN_005380d0 calls FUN_00537030 for it; that walks the live objects), Doors (Non-Lift) (FUN_00536de0: SwitchDoor 59, and the doors 50 whose partner lift 51 is not found), Marine and Predator
	// Pickups (FUN_005373e0) and the enemy groups (Generators, Aliens, Marines, Predators, Alien Queens, Xenoborgs, Predator-Aliens, Praetorians, Sentry Guns, Facehuggers: FUN_00537030 by species).
	// The kind of a strategy is the first number after the AVPSTRAT's two numbers (avpx strategy_chunk.cpp).
	struct StrategyGroup
	{
		char const *name;
		int32_t kinds[4];
		size_t count;
	};

	size_t StrategyMode_Groups(StrategyGroup const **groups);

	// DELETE MULTIPLE's groups that work on the live objects (FUN_005373e0, FUN_00536de0, FUN_005375d0), each giving how many it deleted:
	//  - Marine / Predator Pickups: the PLACED objects whose Placed Object strategy (kind 62) has "Type" 2 or 3 (marines) or 15 (predators) are deleted, strategy and all
	//    (FUN_00538e70); a module with such a strategy keeps it, as inteng's record does nothing there
	//  - Doors (Non-Lift): every Switch Door strategy (59) goes, and every Door strategy (50) except the door a Lift (51) names as its associated door
	//  - the species groups: the AVPGENER special objects (Object Placement's generators and Bad Guys) whose type is in speciesMask (bit n = type n: 0 generator, 1 alien, 2 predator, 3 facehugger,
	//    4 xenoborg, 5 predator-alien, 6 queen, 7 marine, 8 praetorian, 9 sentry gun) are taken away themselves (the record's own delete, method +0x14)
	StrategyModeResult StrategyMode_DeletePickups(OpenLevelFile *file, bool marines, bool predators, size_t *deleted);
	StrategyModeResult StrategyMode_DeleteDoorsNonLift(OpenLevelFile *file, size_t *deleted);
	StrategyModeResult StrategyMode_DeleteSpecies(OpenLevelFile *file, uint32_t speciesMask, size_t *deleted);

	// Deletes the strategy of every object whose first OBJPRJDT's first AVPSTRAT is one of those kinds, and every VIOBJECT in SPECLOBJ whose AVPSTRAT is. *deleted (when not null) is how many were deleted.
	// Done with *deleted 0 changes nothing. One image for all (read by the game's loader first: nothing changes when it refuses).
	StrategyModeResult StrategyMode_DeleteKinds(OpenLevelFile *file, int32_t const *kinds, size_t kindCount, size_t *deleted);

	// A MODULE FLAG EDIT FROM THE FIELD PANEL goes the way the Strategy bar's Add / Remove Flag items go (Bishop's audit 2026-11-15): the panel's old and new flag words differ in some bits;
	// the bits that went away are removed and the bits that came are added, each through StrategyMode_SetModuleFlag (FUN_0052f360's rules: the MODULEDT and MODFLAGS made when a flag is being
	// added, a placed object skipped). Removing first, then adding, as Doorway's two steps do. *changed (when not null) is how many modules were changed.
	StrategyModeResult StrategyMode_FlagWordChanged(OpenLevelFile *file, uint8_t const *id, uint32_t oldWord, uint32_t newWord, size_t *changed);

	// The six stability bits, in face order (face 0 first)
	uint32_t const kStabilityBits[6] = { 0x80000, 0x40000, 0x20000, 0x10000, 0x8000, 0x4000 };

	// What the module's shape holds now: *faces gets bit i set when face i is red (the bit is set). NoObject / NoStrategy (the object has no shape header) as answers.
	StrategyModeResult StrategyMode_GetStability(OpenLevelFile *file, uint8_t const *id, uint32_t *faces);

	// Writes the six bits of the shape of the module with that ID: face i red when bit i of `faces` is set. The shape is the one the object's header names (its shape number).
	StrategyModeResult StrategyMode_SetStability(OpenLevelFile *file, uint8_t const *id, uint32_t faces);

	// Delete Strategy, the answer yes: destroys that chunk (FUN_00528790 0x5287e8-0x528804).
	StrategyModeResult StrategyMode_Delete(OpenLevelFile *file, uint8_t const *id);
}
