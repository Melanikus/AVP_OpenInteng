// strategy_views.h: Strategy Data's Edit Strategy views, one per strategy kind, as inteng draws them (each kind's class has the view as its method +0x14: the vtables in
// AvP-docs\engine\inteng\strategy-kind-vtables.md). A view is NOT a dialog: it draws lines of text on the screen (FUN_0057bd90 at x, y, the first always "%s : <Kind>" at (200, 0)),
// waits, and answers a left click on a line by its y (a toggle of a bit, or a text dialog holding a number), a click on the right button or the Escape key ends it.
// This file knows each kind's lines and what a click on each does, from the strategy's bytes; the editor draws the lines and runs the dialogs (editor_main.cpp).
// Written 2026-11-16 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stddef.h>
#include <stdint.h>

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
	};

	// Where the view finds the name of an object, by its ID (the 8 bytes a strategy holds): false when the level has no such object
	typedef std::function<bool(uint8_t const *id, std::string *name)> StrategyViewNames;

	// Whether the kind has a view built
	bool StrategyView_Has(int32_t kind);

	// ownId is the 8 bytes of the strategy's own object: an object field that names it shows no name (inteng compares it with the current object's ID, 0x914930, and writes nothing then)
	// The lines of the kind's view for a strategy whose payload (the bytes after the chunk's 12-byte heading, what the lookup gives) is these, of the object named name; areaHeight is the
	// text area's height in pixels (some views write a line along its foot)
	bool StrategyView_Lines(int32_t kind, std::vector<uint8_t> const &payload, std::string const &name, int32_t areaHeight, StrategyViewNames const &names, uint8_t const *ownId, std::vector<StrategyViewLine> *lines);

	// What a view does the moment it is entered (FUN_...+0x14 is called, then it draws): the numbers it writes into the strategy first (a door's two times of 0 become 1.0 and 0.5 seconds). field / value pairs, none for most.
	struct StrategyViewFix
	{
		std::string field;
		double value;
	};
	void StrategyView_OnOpen(int32_t kind, std::vector<uint8_t> const &payload, std::vector<StrategyViewFix> *fixes);

	// One whole number of the strategy's payload by the model's field name, as the model reads it (mask and shift done); false when the kind or the field is unknown or the bytes are too short
	bool StrategyView_FieldNumber(int32_t kind, std::vector<uint8_t> const &payload, char const *field, int32_t *value);

	// What a left click at (x, y) of the text area (its top is 0) does in the kind's view
	bool StrategyView_Click(int32_t kind, std::vector<uint8_t> const &payload, int32_t x, int32_t y, StrategyViewClick *out);
}
