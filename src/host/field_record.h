// field_record.h: the record the field panel host shows, with everything it points to. Written 2026-11-14 by Melanikus (Dietrich) for
// AVP_OpenInteng (the two-dev sprint). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Every inspect row's adapter fills one of these (field_adapters.h is where they plug in), and the editor keeps it alive while the field
// panel host (field_panel_host.h) shows it. A descriptor points to its words, its choices and its value's bytes, and the host copies the
// descriptors but not what they point to, so all of that lives here, in the same record:
// - in deques, because a deque keeps each element where it is when more are added and when the whole deque is moved, so the pointers into
//   them hold (a vector of strings would move short strings about);
// - so the record can't be copied (a copy would point into the one it came from), only moved.
//
// A strategy's builder (BuildStrategyRecord, Drake's strategy_record.h) fills this same record, so an object's header and its strategy
// can be shown in one.

#pragma once

#include <stdint.h>

#include <deque>
#include <list>
#include <string>
#include <vector>

#include "choice_box.h"   // ui::Choice
#include "field_descriptor.h"
#include "field_panel.h"

namespace model
{
	struct Field;
}

namespace host
{
	// What a descriptor's value is, in the field model's terms (record_edit.h turns a change in the panel back into a field's value)
	enum class SourceKind
	{
		None,    // nothing the model knows (an object's header bit, a title): never edited through the model
		Number,  // one number of a field: a number, a fraction, a count
		Axis,    // one of the three numbers of a three-number field (axis 0, 1 or 2)
		Bit,     // one bit of a flags field
		Choice,  // a field's named choice
		Object   // an object field, shown as text
	};

	// Where one descriptor's value comes from. The pointer is to the field model's own table entry, which is static, so it never dangles.
	struct FieldSource
	{
		FieldSource() : what(SourceKind::None), field(0), axis(0), bit(0), ofChunk(false), strategyKind(0)
		{
			for (int i = 0; i < 9; i++)
			{
				chunk[i] = 0;
			}
		}

		SourceKind what;
		model::Field const *field;   // the table entry the descriptor came from, or null
		int axis;                    // for Axis
		uint32_t bit;                // for Bit: the one bit
		bool ofChunk;                // set when the record is made editable: whose field it is, a chunk's or a strategy's
		int32_t strategyKind;        // for a strategy's: the kind's number
		char chunk[9];               // for a chunk's: the chunk's 8-letter name and a zero
	};

	struct FieldRecord
	{
		FieldRecord() {}
		FieldRecord(FieldRecord &&) = default;
		FieldRecord &operator=(FieldRecord &&) = default;
		FieldRecord(const FieldRecord &) = delete;
		FieldRecord &operator=(const FieldRecord &) = delete;

		std::string title;                                // the title the host shows first
		std::vector<ui::FieldDescriptor> list;
		std::vector<ui::FieldValue> values;               // one for each descriptor, in the same order
		std::deque<std::string> text;                     // the words the descriptors point to (labels, help lines, units)
		std::deque<std::vector<ui::Choice>> choiceLists;  // each choice field's list
		std::deque<std::string> valueText;                // the bytes a text value points to
		std::deque<FieldSource> sources;                  // where each descriptor's value comes from, in the same order: the first ones, or
		                                                  // fewer than there are descriptors (the rest come from nowhere the model knows)
		std::list<FieldRecord> parts;                     // records joined into this one (record_join.h): they keep the words, choices and
		                                                  // bytes the joined descriptors point to. A list, as it keeps each element where it is.
	};
}
