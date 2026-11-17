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
		kWindowSwitchProperties = 115, // "Switch Properties" (FUN_0052e1a0 + FUN_0052e7f0; 52 / 61, strategy-data.md S-K52-3 / S-K52-6)
		kRunMultiToggle = 116,         // 66: a press at x <= 0x31 on a target row: its request's bit 0 flips (S-K66-3); index = the target
		kCollectMultiTargets = 117,    // 66: a press at x >= 0x50 in the targets' rows (or T): the camera to the row's target (index, -1 none), then "Select targets for switch" (FUN_0052ff10, S-K66-5)
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
		std::vector<std::array<uint8_t, 8>> const *links = nullptr;     // a Linked / Multi / Area switch's Links as FUN_0052dc90 made them again when its view opened (StrategyView_SwitchLinks)
		std::function<int32_t(uint8_t const *id)> strategyKind;         // the kind of the strategy of the object with that ID (FUN_00529820's record +4), 0 when it has none
	};

	// ---- THE SWITCHES' LINKS (strategy-data.md S-K61-1, S-K61-3, S-K66-2; FUN_0052dc90 with FUN_005218b0 / FUN_005218e0; Vasquez 11-16). Checked on every stock level: for all 1852 Linked,
	// Multi and Area switches the Links inteng saved are exactly the ones made again here, in the same order (inteng's Save makes them again first, S-K61-3).
	// A strategy object as inteng's records hold it (DAT_00914954, made by FUN_00521cb0 in its objects' order: the level's objects, then its virtual objects (VIOBJECT), then its enemies and
	// generators (AVPGENER), then its placed hierarchies (PLACHIER)): its ID and its strategy's kind and payload (kind 0, no payload: none)
	struct StrategyRecordRef
	{
		std::array<uint8_t, 8> id;
		int32_t kind = 0;
		std::vector<uint8_t> payload;
	};

	// The Links of a Linked (61), Multi (66) or Area (68) switch made again as FUN_0052dc90 makes them: the IDs of every record, in the records' order, whose strategy is a switch (52, 61, 66,
	// 68: FUN_005218b0) that targets ownId (FUN_005218e0): a 52 / 61 by its Target (+0x24), a 66 / 68 by the first of its targets with that ID; when ownKind is 66 / 68 only if that target's
	// request has bit 1 (& 2: "Part of link switch set", S-84). Any other ownKind: none (0x52dcc7).
	std::vector<std::array<uint8_t, 8>> StrategyView_SwitchLinks(int32_t ownKind, uint8_t const *ownId, std::vector<StrategyRecordRef> const &records);

	// The Links a 61 / 66 / 68 strategy holds (its NumLinks and the 8-byte IDs after it); false when the kind has none or the payload is too short for them
	bool StrategyView_StoredLinks(int32_t kind, std::vector<uint8_t> const &payload, std::vector<std::array<uint8_t, 8>> *links);

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

	// A Multiple Target / Area Switch's targets (66 / 68: the count at payload 36, 12 bytes each from 40, an object and its request); false, and none, for a count the bytes do not hold
	struct MultiTarget
	{
		std::array<uint8_t, 8> id;
		uint32_t request;
	};

	bool StrategyView_MultiTargets(std::vector<uint8_t> const &payload, std::vector<MultiTarget> *targets);

	// The target row of a 66 view showing page `page` under y (from the view's top; FUN_0052f700 0x52fc2b-0x52fc72: (y - the Targets line's y - 16) / 16, from 0 to 14, a target of that
	// page): its index in the list. False when y is on none, the "Targets :" line included (see the S row: INTENG BUG, FIXED)
	bool StrategyView_MultiTargetAt(std::vector<uint8_t> const &payload, int32_t page, int32_t y, size_t *index);

	// The payload with its targets replaced (the count and the list), the links, pad and flags after them as they were; false (and *out untouched) when the payload's list can't be read
	bool StrategyView_MultiWith(std::vector<uint8_t> const &payload, std::vector<MultiTarget> const &targets, std::vector<uint8_t> *out);

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

	// E: "Edit extra message data for target" (reassess/strategy-data-kinds.md S-KC-8, S-K62-5, S-K69-6, S-K72-6; strategy-data.md S-82..S-85). The view's E calls FUN_00529b30(target ID,
	// &request word): the target's wrapper (FUN_00529570: none, or the view's own object -> nothing) runs its +0x10: an ENEMY's wrapper its own page (FUN_0053a470), a placed hierarchy's
	// FUN_0053b830, every other wrapper (FUN_005393a0) its STRATEGY's +0x18 (no strategy: nothing). The page edits the SWITCH's request word for that target, not the target.
	enum class RequestPage
	{
		Nothing,        // no such target, or a target with no strategy (FUN_005393a0 calls nothing)
		NotUsed,        // FUN_0052b220 (S-85, S-KC-8's default): the word &= 1 and the message "The target strategy doesn't use extended request data", Ok
		PlacedObject,   // 62's +0x18 FUN_00531240 (S-K62-E)
		Switch,         // 66 / 68's +0x18 FUN_00530510 (S-84)
		Mission,        // 70's +0x18 0x5333a0 (S-K70-E)
		Track,          // 72 / 80's +0x18 0x535210 (S-K72-E)
		Message,        // 73's +0x18 FUN_00533880 (S-K73-E)
		Enemy,          // an enemy's wrapper FUN_0053a470 (S-K69-E, S-82), a marine: its page
		NotMarine,      // FUN_0053a470 for any other species: the message "Only available for marines/civilians", Ok; the word kept
		PlacedHierarchy,   // a placed hierarchy's wrapper FUN_0053b830 (S-K76-E, S-83), whatever strategy it holds: "Sequence index %d" = the word's bits 8-15; the word is NOT cut (Vasquez 11-16)
	};

	// The page for a target: found (the level has an object of that ID, not the view's own), enemy (an AVPGENER with a type other than 0) and its species, whether it holds a strategy and of which
	// kind; placedHierarchy: the target is a placed hierarchy (its own page, S-K76-E)
	RequestPage StrategyView_RequestPageOf(bool found, bool enemy, int32_t species, bool hasStrategy, int32_t targetKind, bool placedHierarchy = false);

	// What the page edits (each page cuts the word to bit 0 on entry, works on data = word >> 1, and puts word | data << 1 back at its end)
	struct RequestPageState
	{
		RequestPage page = RequestPage::Nothing;
		std::string targetName;            // the target's name (its wrapper's +4)
		uint32_t low = 0;                  // the word's bit 0, kept
		uint32_t data = 0;                 // the word >> 1, arithmetic (SAR: 0x531264, 0x5333be, 0x535227, 0x533887, 0x53a4ad): the bits the page edits
		int32_t targetType = 0;            // a Placed Object target's Type (+0x14): 16, a placed light, has four more lines
		int32_t targetIntegrity = 0;       // a Placed Object target's integrity (+0x20): "New integrity value" starts with its lowest 8 bits
		std::vector<StrategyViewLine> drawn;   // Mission / Switch: the page's lines of this pass (INTENG BUG, FIXED: inteng empties its text list only when the page opens, so every line
		                                       // drawn since stayed on the screen, an old box glyph under the new one; here each pass starts it empty)
	};

	// The page opens on the word (the cut and the shift). False for Nothing / NotUsed / NotMarine, which have no page (the caller says the message and, for NotUsed, writes word & 1)
	bool StrategyView_RequestOpen(RequestPage page, uint32_t word, std::string const &targetName, int32_t targetType, int32_t targetIntegrity, RequestPageState *state);

	// The word the page puts back at its end: bit 0 as it was, the data above it (the data's top bit is lost, as inteng's data * 2)
	uint32_t StrategyView_RequestWord(RequestPageState const &state);

	// The page's lines for a text area areaHeight high; path gives a path's name by its number (the Enemy page's "Path - %d  %s", FUN_004fbaf0), null for none
	void StrategyView_RequestLines(RequestPageState *state, int32_t areaHeight, std::function<bool(int32_t number, std::string *name)> const &path, std::vector<StrategyViewLine> *lines);

	// What a left click at y (from the text area's top) asks beyond the change it makes to the data at once
	enum class RequestAsk
	{
		None,
		Integrity,    // the whole-number dialog "New integrity value" (0x5ef380) holding the target's integrity & 0xff: StrategyView_RequestIntegrity with the answer on Ok
		SelectPath,   // the list "Select Path" (FUN_004fb6a0): StrategyView_RequestPath with the chosen path's number
		SelectSequence,   // the placed hierarchy's sequences (its PLHISEQU chunks): none -> the message "No sequences!!!!!!" (0x5f0394); else the list "Select Sequence Index" (0x5f03a8) with a row "%d"
		                  // per sequence's index (+0x28): StrategyView_RequestSequence with the chosen one's (FUN_0053b830 0x53b8c3-0x53bade)
	};

	RequestAsk StrategyView_RequestClick(RequestPageState *state, int32_t y);
	void StrategyView_RequestIntegrity(RequestPageState *state, int64_t typed);   // FUN_00531240 0x5314bb-0x5314d6: below 1 -> 0, above 254 -> 255; data = n << 7 | (data & 0xffff807f) | 1
	void StrategyView_RequestPath(RequestPageState *state, int32_t number);       // FUN_0053a470 0x53a65c: data |= number << 15
	void StrategyView_RequestSequence(RequestPageState *state, int32_t index);    // FUN_0053b830 0x53bad8: word = (word & 0xffff00ff) | index << 8 (the index not masked, as inteng's)

	// Whether Escape on the page also ends the view under it: the Mission, Track and Message pages clear only the mouse at their end (0x5335d3, 0x53535c, 0x5339f4), so the Escape slot is still set when
	// the view's loop tests it; the others clear it (0x531587, 0x530632, 0x53a681)
	bool StrategyView_RequestEscapeEndsView(RequestPage page);
}
