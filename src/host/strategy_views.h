// strategy_views.h: Strategy Data's Edit Strategy views, one per strategy kind, as inteng draws them (each kind's class has the view as its method +0x14: the vtables in
// AvP-docs\engine\inteng\strategy-kind-vtables.md). A view is NOT a dialog: it draws lines of text on the screen (FUN_0057bd90 at x, y, the first always "%s : <Kind>" at (200, 0)),
// waits, and answers a left click on a line by its y (a toggle of a bit, or a text dialog holding a number), a click on the right button or the Escape key ends it.
// This file knows each kind's lines and what a click on each does, from the strategy's bytes; the editor draws the lines and runs the dialogs (editor_main.cpp).
// Written 2026-11-16 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <functional>
#include <string>
#include <vector>

namespace host
{
	struct StrategyViewLine
	{
		int32_t x;
		int32_t y;
		std::string text;
	};

	// What a click on a line asks for
	struct StrategyViewClick
	{
		enum Action
		{
			None,      // the click lands on nothing
			Toggle,    // the bits of mask in the field are flipped
			Set,       // the field is set to `value` (a mode switch that changes several bits)
			Number,    // a text dialog holds a number (title, initialText); an answer whose value times factor lies inside [lowest, highest) (or up to highest when highestIncluded) is kept as trunc(answer * factor + bias)
			Pick,      // the next click on an object in the view makes it the object of the object field
			Collect,   // a list of objects is collected (Frost's collector, FUN_00526bf0): row says which list (kCollect...), index an entry to fly to first (-1 none)
			Window,    // a window of buttons asks (row: kWindow...)
			Run,       // the editor does the row's work at once (row: kRun...)
		};

		Action action = None;
		std::string field;       // the model's name for the field (field_model.h)
		uint32_t mask = 0;       // Toggle
		double value = 0;        // Set
		std::string title;       // Number
		std::string initialText; // Number: what the dialog starts with
		bool isFloat = false;    // Number: the answer is read as a real number (FUN_00474a10), else a whole number (FUN_00474340)
		double factor = 1.0;
		double bias = 0.0;
		double lowest = 0;
		double highest = 0;
		bool highestIncluded = true;
		bool clampBelow = false;     // an answer below lowest is kept as lowest (FUN_00535c90's countdown: a negative time is 0), not refused
		std::string pickWords;   // Pick: the words drawn along the foot while the pick waits ("Select associated door")
		int32_t pickRow = 0;     // Pick: what happens once it ends (StrategyPickRow), beyond the field taking the object; Collect / Window / Run: which one (StrategyViewRow)
		int32_t index = -1;      // Collect: the list entry the camera goes to first (-1 none)
		std::string requestField;   // Pick of a target (kPickTarget): the request field set to 1 when the target changed
	};

	// The Lift's other actions (S-K51-7, -11, -12)
	enum StrategyViewRow
	{
		kCollectLiftPositions = 101,   // FUN_0052b360
		kWindowLiftStart = 102,        // FUN_0052af00
		kRunLiftExit = 103,            // 0x52ca92: flag 8 flips, FUN_0052b770
		kWindowMissionText = 104,      // FUN_00532550: the 500 x 400 text window; index 0 the Description, 1 the Completed Message
		kCollectMissionTargets = 105,  // FUN_00532ea0: the alterations collector; index = the target row clicked, -1 none
		kWindowTrackPoint = 106,       // "Which track point?" (S-K72-2)
		kOpenTrackPage = 107,          // an effect's line: index = its point (S-K72-3)
		kCollectTrackTargets = 108,    // FUN_00534df0: a point page's targets editor; index = the target row clicked (x >= 190), -1 none
		kRunTrackRequest = 109,        // a target row, x < 50: its request word (index = the row)
		kRunTrackDirection = 110,      // a target row, 50 <= x < 190: its direction word cycles
		kWindowMessageText = 111,      // FUN_00532550 for a Message's string number (S-K73-3)
		kWindowEnemyMission = 112,     // "Select Mission Type" for an enemy (FUN_00531fb0 a marine's six, FUN_00532230 a predator's two; S-K69-2)
		kWindowGeneratorMission = 113, // "Select Mission Type" for a generator (FUN_005323c0: Hunt, Path follower; S-K79-2)
		kWindowSelectPath = 114,       // "Select Path", the list of the level's paths (FUN_004fb6a0; S-K69-3, S-K79-3)
	};

	// What a view needs to know of the level beyond its own bytes (the Lift's radius and start; null where not needed)
	struct StrategyViewWorld
	{
		std::function<bool(uint8_t const *id, double *radius)> radius;   // FUN_00529850: false when the object is not in the level (1.0 then)
		std::function<bool(uint8_t const *id)> liftStartsHere;          // the object's strategy is a Lift (0x33) with flag 1
		std::function<std::string(uint8_t const *id)> targetWords;      // S-KC-12 (FUN_00529eb0): "<name> - <its kind's word>" for an ID that is not the null ID
		std::function<int32_t(uint8_t const *id)> trackSections;        // the object's OBJTRAK2 +0x28, 0 when it has none (a Track Object's view)
		std::function<int32_t(uint8_t const *id)> species;              // an enemy's species (its AVPGENER's type, inteng's record +0x38: 7 marine, 2 predator ...), -1 when none
		std::function<bool(int32_t number, std::string *name)> path;    // the level's path with that number (FUN_004fbaf0: SPECLOBJ > AVPPATH2 +0x34) and its name; false when none has it
		int32_t page = -1;                                              // the page of a view with pages (a Track Object's point page: the point), -1 its main page
	};

	// What the end of an object pick does (S-KC-5 and each kind's pick row, reassess/strategy-data-kinds.md)
	enum StrategyPickRow
	{
		kPickTarget = 1,       // the field = the picked object's ID (any object: FUN_00531160, FUN_00531b70, FUN_005353c0); when it changed, the request field = 1
		kPickSwitchDoor = 2,   // FUN_0052d490: the door = the picked object (a module or placed object); with no strategy it gets a Switch Door naming this one; a Switch Door is made to name this one
		kPickTeleport = 3,     // FUN_00530ce0: the target = the picked object; with no strategy it gets a Teleport of the same Type naming this one
		kPickLiftDoor = 4,     // FUN_0052bd70
		kPickLiftCall = 5,     // FUN_0052bf80
		kPickLiftFloor = 6,    // FUN_0052c170
		kPickSwitchTarget = 7, // FUN_0052db10 (52 / 61, strategy-data.md S-K52-4): the field = the picked object's ID, else the null ID; when it changed, the request field = 0
	};

	// Where the view finds the name of an object, by its ID (the 8 bytes a strategy holds): false when the level has no such object
	typedef std::function<bool(uint8_t const *id, std::string *name)> StrategyViewNames;

	// Whether the kind has a view built
	bool StrategyView_Has(int32_t kind);

	// ownId is the 8 bytes of the strategy's own object: an object field that names it shows no name (inteng compares it with the current object's ID, 0x914930, and writes nothing then)
	// The lines of the kind's view for a strategy whose payload (the bytes after the chunk's 12-byte heading, what the lookup gives) is these, of the object named name; areaHeight is the
	// text area's height in pixels (some views write a line along its foot)
	bool StrategyView_Lines(int32_t kind, std::vector<uint8_t> const &payload, std::string const &name, int32_t areaHeight, StrategyViewNames const &names, uint8_t const *ownId, std::vector<StrategyViewLine> *lines,
	                        StrategyViewWorld const *world = nullptr);

	// A Track Object's point effects (72 / 80; the game's ReadPointEffects): per effect its point, a pad word and its targets (ID, request word, direction word: bit 1 forward, 2 backward, 4 opposite back)
	struct TrackTarget
	{
		std::array<uint8_t, 8> id;
		int32_t request = 0;
		int32_t direction = 0;
	};

	struct TrackEffect
	{
		int32_t point = 0;
		int32_t pad = 0;
		std::vector<TrackTarget> targets;
	};

	// The effects, read with the bytes' bounds checked: false when a count does not fit (such a track is not changed by the view's edits)
	bool StrategyView_TrackEffects(std::vector<uint8_t> const &payload, std::vector<TrackEffect> *effects);

	// The payload with these effects in place of its own, the words after them as they were
	bool StrategyView_TrackWith(std::vector<uint8_t> const &payload, std::vector<TrackEffect> const &effects, std::vector<uint8_t> *out);

	// A Mission Objective's alteration targets (S-K70-1): each entry's ID and word (bit 1 make visible, bit 2 make possible), read with the bytes' bounds checked; false when the list does not fit
	struct MissionTarget
	{
		std::array<uint8_t, 8> id;
		int32_t word;
	};

	bool StrategyView_MissionTargets(std::vector<uint8_t> const &payload, std::vector<MissionTarget> *targets);

	// The alteration row under the pointer's y (from the window's top; S-K70-7, -8): its index, or false when y is not on one
	bool StrategyView_MissionTargetAt(std::vector<uint8_t> const &payload, int32_t y, size_t *index);

	// The Lift's positions (the IDs of the list after NumAssocLifts), read with the bytes' bounds checked
	bool StrategyView_LiftPositions(std::vector<uint8_t> const &payload, std::vector<std::array<uint8_t, 8>> *ids);

	// What a view does the moment it is entered (FUN_...+0x14 is called, then it draws): the numbers it writes into the strategy first (a door's two times of 0 become 1.0 and 0.5 seconds). field / value pairs, none for most.
	struct StrategyViewFix
	{
		std::string field;
		double value;
	};
	void StrategyView_OnOpen(int32_t kind, std::vector<uint8_t> const &payload, std::vector<StrategyViewFix> *fixes);

	// The object (8 bytes) an object field of the payload names: false when the field is unknown or names none (the null ID, 0 0)
	bool StrategyView_FieldObject(int32_t kind, std::vector<uint8_t> const &payload, char const *field, uint8_t *id);

	// One whole number of the strategy's payload by the model's field name, as the model reads it (mask and shift done); false when the kind or the field is unknown or the bytes are too short
	bool StrategyView_FieldNumber(int32_t kind, std::vector<uint8_t> const &payload, char const *field, int32_t *value);

	// What a left click at (x, y) of the text area (its top is 0) does in the kind's view
	// species: an enemy's (69) species, which says which lines its view has (S-K69-1); -1 for the other kinds
	bool StrategyView_Click(int32_t kind, std::vector<uint8_t> const &payload, int32_t x, int32_t y, StrategyViewClick *out, int32_t page = -1, int32_t species = -1);
}
