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
//   - SwitchTargets: the Multiple Target / Area Switch's list: one line per target (its drop-down, Pick, on/off, Duplicate, Remove, Extra data) and a "+" line under the last
//     that APPENDS one target (the owner: "the append behavior, not every time it resets the list"). Nothing on the other lines changes.
// A button press closes the form, does its work and opens the form again at once with the record as it is now (DEBT: our dialogs have no in-place button; the form flickers once).
//
// WHERE EACH KIND'S ROWS ARE (so the four of us never edit the same lines): each group of kinds has its own file and its own function below. A group's function answers for the kinds
// it knows: true and their rows; false for any other. A kind is switched to its form by its group function answering true for it, only once that kind is complete and tested.
#pragma once

#include <stdint.h>

#include <functional>
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
		SwitchTargets,
		Action,
		Links,   // read only: the switches that target this one, made again as the view opens (the 61 / 66 / 68 page's "Links:"), one line each, "None" for none
	};

	struct StrategyFormChoice
	{
		std::string words;   // the drop-down row's words, inteng's own where its page had them ("Wait", "Reversed (default)")
		int32_t value = 0;   // what picking it writes (Bits: within mask)
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
		int32_t pickRow = 0;                      // Target, SwitchTargets: the rule a pick writes the target with (strategy_views.h StrategyPickRow)
		std::string request;                      // Target: the request field that rule sets (the target's on/off and extra data)
		std::string pickWords;                    // Target: the words along the foot while its pick runs, inteng's ("Select target for switch")
		bool extra = false;                       // Target: an "Extra data..." button after Pick (the page's E: the target's extra message data)
		std::string button;                       // Action: the button's words; the label's words beside it are `words`
		std::string words;                        // Action: the line's words ("String : 3")
		int32_t pageX = -1, pageY = -1;           // Action: the press on the page it runs (StrategyViewClickAt), when pageY >= 0
		char key = 0;                             // Action: else the key it runs: 'E' or 'V'
		// shown only when this answers true for the record as it is (the switch-off message only for the modes 0, 1 and 2); null: always shown
		std::function<bool(int32_t kind, std::vector<uint8_t> const &payload)> shown;
	};

	// Does this kind edit in a form? Asks the four group functions below in turn
	bool StrategyForm_Has(int32_t kind);

	// The kind's rows, top to bottom (empty when it has no form)
	std::vector<StrategyFormRow> StrategyForm_Rows(int32_t kind);

	// The kind's words after the object's name in the form's title, as its page says them (" : Linked Switch"); "" for a kind not assignable
	std::string StrategyForm_KindWords(int32_t kind);

	// THE FOUR GROUPS, one file each: true and the kind's rows for a kind the group has switched to its form, false for any other.
	bool StrategyFormRows_Switches(int32_t kind, std::vector<StrategyFormRow> *rows);   // strategy_forms_switches.cpp (Vasquez): 61 Linked, 66 Multiple Target, 68 Area Switch
	bool StrategyFormRows_Modules(int32_t kind, std::vector<StrategyFormRow> *rows);    // strategy_forms_modules.cpp (Drake): 51 Lift, 50 Door, 57 Airlock, 59 Switch Door,
	                                                                                    //   54 Platform Lift, 75 Fan, 64 Lighting Effects
	bool StrategyFormRows_Objects(int32_t kind, std::vector<StrategyFormRow> *rows);    // strategy_forms_objects.cpp (Dietrich): 62 Placed Object, 77 Volume of Death,
	                                                                                    //   78 Self Destruct, 73 Message, 70 Mission Objective
	bool StrategyFormRows_Paths(int32_t kind, std::vector<StrategyFormRow> *rows);      // strategy_forms_paths.cpp (Frost): 72 / 80 Track Object, 76 Hierarchy, 69 Enemy,
	                                                                                    //   79 Generator, and the E extra-data pages
	// strategy_forms_paths.cpp (Frost): the E pages' rows, on the field "request.data" (that entry's word >> 1, bit 0 kept)
	bool StrategyFormRows_Extra(RequestPage page, RequestPageState const &state, std::vector<StrategyFormRow> *rows);
}
