// strategy_forms.h: EDIT STRATEGY AS A FORM of our own drop-downs, number boxes and buttons (the owner, 2026-11-24: "no more text that can be clicked, our pretty drop downs like we
// have across the rest of inteng that saves the new picked trigger target when selected or a 'pick' button to click in the world"). A NAMED DIFFERENCE from inteng, whose Edit
// Strategy pages are lines of text that a click or a key changes. Written 2026-11-24 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// HOW IT WORKS: a kind that HAS A FORM (StrategyForm_Has) opens it where inteng's page opened; every other kind keeps its page, so the editor can be released after any merge (the
// owner: "MVPs so we can cut a release in the middle"). A form is one of our dialogs (dialogs.h Dialogs_Form), built from the kind's rows below:
//   - Choice: a label and a drop-down; picking a row writes its value into the field at once (StrategyViewApply, the same writer as inteng's own forms).
//   - Bits: the same, but the drop-down owns only `mask`'s bits of the field (the switch-off message is bits 2 and 4 of the flags); the other bits are kept.
//   - Check: a check box for one bit (`mask`) of the field, written as it is ticked.
//   - Number: a label and a number box; written when Ok is pressed, typed x factor, refused outside [lowest, highest] with the range shown (as the page's Number action).
//   - Target: a label, a drop-down of the level's objects (every object but this one, by name) and a Pick button that picks in the world as T does today; either one writes
//     the target with the rule of `pickRow` (strategy_views.h kPickTarget, kPickSwitchTarget ...: the request field set as inteng's pick sets it).
//   - Action: a label, the line's words and a button that runs what the kind's page does today for that line or key (Dietrich 11-24: 73's String, 77's V, 62's E): a press
//     on the page at (pageX, pageY), or the key (E: the target's extra data page; V: Edit Volume). The form opens again once whatever it opened has ended.
//   - List: a list of entries, each an object and its word (the Multiple Target / Area Switch's targets and their requests, the Mission Objective's alterations): one line per
//     entry (its drop-down and Pick, a check box for each of entryChecks' bits of its word, Extra data... and Duplicate where the kind has them, Remove) and a "+" line under
//     the last whose drop-down or Pick APPENDS one entry with newWord (the owner: "the append behavior, not every time it resets the list"). Nothing on the other lines changes.
//     The group reads and writes the entries (readList / writeList: its own byte layout); ten lines a page, with < and > for the others.
//     A LIST OF LISTS (Frost 11-24, a track's points and their targets): each entry's line is followed by the lines of its sub-list (subRow says what they hold) and
//     that sub-list's own "+" line, indented; three entries a page. An entry can be named by a number (entryWords: "Point 3"; its "+" asks for the number), and a line
//     can have a drop-down for its entry's choice (entryChoices: a target's direction).
// A button press closes the form, does its work and opens the form again at once with the record as it is now (DEBT: our dialogs have no in-place button; the form flickers once).
//
// WHERE EACH KIND'S ROWS ARE (so the four of us never edit the same lines): each group of kinds has its own file and its own function below. A group's function answers for the kinds
// it knows: true and their rows; false for any other. A kind is switched to its form by its group function answering true for it, only once that kind is complete and tested.
#pragma once

#include <stdint.h>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "strategy_views.h"

namespace host
{
	enum class StrategyFormRowKind
	{
		Choice,
		Bits,
		Check,
		Number,
		Target,
		List,
		SwitchTargets = List,   // the first name of List (Vasquez 11-24, before Frost's and Dietrich's lists made it general)
		Action,
		PageWords,   // read only: the page's own lines whose words start with one of pageWordsStarts, in the page's order (the Lift's "Radius", "Floor", "Facing")
		Links,   // read only: the switches that target this one, made again as the view opens (the 61 / 66 / 68 page's "Links:"), one line each, "None" for none
	};

	struct StrategyFormChoice
	{
		std::string words;   // the drop-down row's words, inteng's own where its page had them ("Wait", "Reversed (default)")
		int32_t value = 0;   // what picking it writes (Bits: within mask)
	};

	// A List's entry: an object and its 32-bit word (a switch target and its request; a Mission Objective's alteration and its bits)
	struct StrategyFormEntry
	{
		std::array<uint8_t, 8> id = {};
		uint32_t word = 0;
		int32_t choice = 0;                    // the line's drop-down of entryChoices (a track target's direction: 1, 2, 7, 3)
		std::vector<StrategyFormEntry> sub;    // the entries under it, a list of subRow's kind (a track point's or a hierarchy flag's targets)
	};

	// The words a List row's callbacks give (a line's "Point 3", why a number is refused): std::string by another name, AND WHY (Frost 2026-11-24): Snyk Code could not
	// parse this header while those members were written std::function<std::string(...)> (the gate's "1 not analysed", which fails it, as any file it cannot read); written
	// std::function<StrategyFormWords(...)> it reads and analyses the whole file. Nothing else changes: it is the same type
	typedef std::string StrategyFormWords;

	// A List line's check box: one bit of the entry's word (a switch target's "On" 1; an alteration's "Visible" 1, "Possible" 2)
	struct StrategyFormEntryCheck
	{
		std::string label;
		uint32_t mask = 0;
	};

	// A field written with a Choice when, and only when, the Choice really changed the value (Frost 11-24: a Mission change also makes ExtraMissionData 0, MissionTypeWindow)
	struct StrategyFormAlso
	{
		std::string field;
		double value = 0.0;
	};

	struct StrategyFormRow
	{
		StrategyFormRowKind kind = StrategyFormRowKind::Choice;
		std::string label;                        // the words left of the drop-down or box ("Rest State", "Mode", "Target")
		std::string field;                        // the model's name of the field (field_model.h), as StrategyViewApply and StrategyView_FieldNumber take it
		uint32_t mask = 0;                        // Bits, Check: the field's bits this row owns
		std::vector<StrategyFormChoice> choices;  // Choice, Bits. A Choice shows as picked the row whose value EQUALS the field (none: the box is empty)
		// Choice, Bits: when set, the rows are made as the form opens, from the record as it is (the level's paths for an Enemy's Path), in place of `choices`
		std::function<std::vector<StrategyFormChoice>(int32_t kind, std::vector<uint8_t> const &payload)> choicesFrom;
		std::vector<StrategyFormAlso> alsoOnChange;   // Choice: written after it, only when its pick changed the field
		double factor = 1.0;                      // Number: the value written is the typed number x factor, + bias, then made whole (as the page's StrategyViewClick::Number):
		double bias = 0.0;                        //   a time in seconds x 65536 is inteng's 16.16; a door's tenths are x 10 + 0.01 (DoorTime, strategy_views.cpp)
		bool isFloat = false;                     // Number: the box takes a real number (else a whole number)
		double lowest = 0.0, highest = 0.0;       // Number: the range the TYPED number must lie in, ends included (shown beside the box)
		uint32_t shift = 0;                       // Number with a mask: the typed number goes into the mask's bits, shifted up this far: field = (field & ~mask) | ((n << shift) & mask)
		uint32_t alsoBits = 0;                    //   | alsoBits (Frost 11-24: a Placed Object target's integrity, bits 7..14, sets bit 0 "Set integrity" too)
		uint32_t clearAlso = 0;                   // Check: these bits cleared too when the box is UNticked (the integrity's bits with "Set integrity", as inteng's page)
		int32_t pickRow = 0;                      // Target, SwitchTargets: the rule a pick writes the target with (strategy_views.h StrategyPickRow)
		std::string request;                      // Target: the request field that rule sets (the target's on/off and extra data)
		std::string pickWords;                    // Target: the words along the foot while its pick runs, inteng's ("Select target for switch")
		bool extra = false;                       // Target: an "Extra data..." button after Pick (the page's E: the target's extra message data)
		std::string button;                       // Action: the button's words; the label's words beside it are `words`
		std::string words;                        // Action: the line's words ("String : 3")
		int32_t pageX = -1, pageY = -1;           // Action: the press on the page it runs (StrategyViewClickAt), when pageY >= 0
		char key = 0;                             // Action: else the key it runs: 'E' or 'V'
		int32_t pagePickRow = 0;                  // Action: else the press on the page's line whose click is this (strategy_views.h kWindowLiftStart ...): the page's own dispatch,
		                                          //   wherever the line is for this record (Vasquez 11-25: the Lift's lines move with its positions)
		std::vector<std::string> pageWordsStarts; // Action (with no `words`), PageWords: the page's lines shown are those starting with one of these ("Lift starts at", "Exit ")
		int32_t pageWordsX = -1;                  // PageWords, Action: only the page's lines at this x (the Lift's foot lines are at 200; its "Floor Switch :" at 16); -1 any
		std::string addWords;                     // List: the "+" line's words (empty: "add a target"; a Lift's "add a position")
		int32_t canTakeKind = 0;                  // Target, List: only objects that can TAKE this strategy kind (the editor's rule, Assign's: a Lift's positions are modules)
		bool liftPositions = false;               // List: a Lift's positions: written as the page's collector writes them, then its question "Would you like the other
		                                          //   positions altered to agree with this one?" (Yes: they are made to agree); an object already in the list is not added again
		// List: the entries read from the record, and the record's whole bytes with them in place of its old ones (false: not readable / not written)
		std::function<bool(std::vector<uint8_t> const &payload, std::vector<StrategyFormEntry> *entries)> readList;
		std::function<bool(std::vector<uint8_t> const &payload, std::vector<StrategyFormEntry> const &entries, std::vector<uint8_t> *bytes)> writeList;
		std::vector<StrategyFormEntryCheck> entryChecks;   // List: a check box on each line for each
		uint32_t newWord = 1;                              // List: the word of an entry the "+" line appends
		bool entryExtra = false;                           // List: Extra data... on each line (the page's E on that line)
		bool entryDuplicate = false;                       // List: Duplicate on each line (the page's D: a copy just after it)
		std::vector<int32_t> targetKinds;                  // Target, List: only objects holding one of these strategies (a Mission Objective's 70, 71); empty: any object
		std::vector<StrategyFormChoice> entryChoices;      // List: a drop-down on each line for its entry's choice (Frost 11-24: a track target's direction)
		int32_t newChoice = 0;                             // List: the choice of an entry the "+" line appends
		// List: when set, each line shows these words in place of an object's drop-down and Pick ("Point 3", "Flag 3"), and the "+" line asks for a number (newAsk) that
		// becomes the new entry's word; newNumberRefused gives "" for a number the record takes, else the words why not (then nothing is appended)
		std::function<StrategyFormWords(StrategyFormEntry const &entry)> entryWords;
		std::string newAsk;
		std::function<StrategyFormWords(std::vector<uint8_t> const &payload, int32_t number)> newNumberRefused;
		// List: the list under each entry (its entryChecks, entryChoices, newWord / newChoice, entryExtra ...; its own readList / writeList are not used: this row's read
		// and write the whole tree, the lists under the entries in each entry's sub). Null: none
		std::shared_ptr<StrategyFormRow> subRow;
		// shown only when this answers true for the record as it is (the switch-off message only for the modes 0, 1 and 2); null: always shown
		std::function<bool(int32_t kind, std::vector<uint8_t> const &payload)> shown;
	};

	// Does this kind edit in a form? Asks the four group functions below in turn
	bool StrategyForm_Has(int32_t kind);

	// The kind's rows, top to bottom (empty when it has no form)
	std::vector<StrategyFormRow> StrategyForm_Rows(int32_t kind);

	// The kind's words after the object's name in the form's title, as its page says them (" : Linked Switch"); "" for a kind not assignable
	std::string StrategyForm_KindWords(int32_t kind);

	// The same for this record, where the page picks its words from it (a 51 with LiftFlags bit 2 is " : Airlock", with bit 4 " : Lift (switches don't teleport)")
	std::string StrategyForm_TitleWords(int32_t kind, std::vector<uint8_t> const &payload);

	// THE FOUR GROUPS, one file each: true and the kind's rows for a kind the group has switched to its form, false for any other.
	bool StrategyFormRows_Switches(int32_t kind, std::vector<StrategyFormRow> *rows);   // strategy_forms_switches.cpp (Vasquez): 61 Linked, 66 Multiple Target, 68 Area Switch
	bool StrategyFormRows_Modules(int32_t kind, std::vector<StrategyFormRow> *rows);    // strategy_forms_modules.cpp (Drake): 51 Lift, 50 Door, 57 Airlock, 59 Switch Door,
	                                                                                    //   54 Platform Lift, 75 Fan, 64 Lighting Effects
	bool StrategyFormRows_Objects(int32_t kind, std::vector<StrategyFormRow> *rows);    // strategy_forms_objects.cpp (Dietrich): 62 Placed Object, 77 Volume of Death,
	                                                                                    //   78 Self Destruct, 73 Message, 70 Mission Objective
	bool StrategyFormRows_Paths(int32_t kind, std::vector<StrategyFormRow> *rows);      // strategy_forms_paths.cpp (Frost): 72 / 80 Track Object, 76 Hierarchy, 69 Enemy,
	                                                                                    //   79 Generator, and the E extra-data pages
	// strategy_forms_paths.cpp (Frost): the E pages' rows, on the field "request.data" (that entry's word >> 1, bit 0 kept). Answering true with rows, Extra data... opens them as a
	// form over the level in place of the page (Vasquez 11-24): every row reads and writes the page's data whatever its field says, its shown / choicesFrom get the view's kind and the
	// data's 4 bytes (little-endian) as the payload, and Ok or Escape ends it as the page's Escape ends the page (the word, bit 0 kept, written where the page writes it)
	bool StrategyFormRows_Extra(RequestPage page, RequestPageState const &state, std::vector<StrategyFormRow> *rows);
}
