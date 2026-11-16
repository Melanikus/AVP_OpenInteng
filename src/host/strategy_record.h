// strategy_record.h: turns one strategy (an AVPSTRAT payload from a level) into the field panel host's record, read-only. Written
// 2026-11-14 by Melanikus (Drake) for AVP_OpenInteng (the two-dev sprint: the back end of the strategy rows' inspect step).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT IT DOES. The field panel host (field_panel_host.h) shows a title and a list of fields, each with its old-editor label, its limits
// and its value. This builds that list for one strategy, from the field model's tables (src/model/field_model.h): the label, help,
// units, bit names and choice names come from the tables, and the values are read out of the payload by the model's own readers
// (ReadField, ReadFieldAxis, ReadFieldObject), which check that each field is the kind's own and fits in the payload before they read a
// byte. Everything is read-only: nothing here writes to the payload.
//
// THE RULES, which are the old editor's (inteng is the spec), field by field:
// - A field the old editor showed no label for (its label is "") isn't shown, and its bytes are never touched. (A flags field is the
//   exception to the label rule: it has no label of its own, and each of its bits is shown by the bit's own label, bits the editor always
//   sets when it saves left out.)
// - A flags field is one Bit for each named bit; a choice is one Choice box with its named values; a field of three numbers (a corner of
//   a box) is three numbers, x, y and z; an object field is one Text, "none" or its two halves in hex; any other field is a Number with
//   its limits and its unit's words. An orientation isn't shown yet: orientations come with the edit path.
// - A value is shown as the file holds it. A number outside its range, a "not a number" and a choice nobody named are all kept, and the
//   panel flags them.
// - Every word is the editor's own, cut to what a descriptor takes (field_descriptor.h) and with each letter the font doesn't have made a
//   "?", so the record always passes the panel's own check. A record the check refuses anyway is a mistake in our tables, and is refused
//   whole, with the reason.
//
// WHO KEEPS WHAT. A descriptor points to its words, its choices and its value's bytes, and the record holds all of them, so the record
// has to be kept alive while the host shows it, and it can't be copied (a copy would point into the one it came from). It can be moved.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <deque>
#include <string>
#include <vector>

#include "choice_box.h"   // ui::Choice
#include "field_record.h"
#include "field_descriptor.h"
#include "field_panel.h"

namespace model
{
	struct StrategyKind;
}

namespace host
{
	// The record is the field panel's own, shared with every other record's builder (field_record.h). StrategyRecord is the same type under the
	// name the strategy rows use.
	using StrategyRecord = FieldRecord;

	// Builds the record. Gives back false, with *why (if not null) in plain words, and an empty record, for:
	// - a null record, or a null payload;
	// - a payload the field model's StrategySizeIsRight refuses (the reason is the model's own: SizeCheckWords);
	// - a kind the field model doesn't know (the kind's number is the int32 at payload +8, little-endian, as the model reads it);
	// - a field of the kind that couldn't be read, or more fields than the panel host shows (kFieldPanelHostMostFields), or a record the
	//   panel's own check (ui::CheckFieldList) refuses: those are mistakes in the tables.
	// *why points to a text that's static, or in the record, so it's good as long as the record is.
	bool BuildStrategyRecord(const uint8_t *payload, size_t length, FieldRecord *record, const char **why);

	// The same for the kind it's given, which the caller has found: BuildStrategyRecord is this, once the payload's size is right and
	// its kind is known. It doesn't check the payload's size, but it can't read past it, since every field is checked to fit before it's
	// read. It's here so a test can build a kind of its own, with a field of each storage, which no real kind has.
	bool BuildStrategyRecordOfKind(const model::StrategyKind &kind, const uint8_t *payload, size_t length, FieldRecord *record,
	                               const char **why);
}
