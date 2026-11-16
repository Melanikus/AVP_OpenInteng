// record_join.h: one record made of two (field_record.h): the fields of a second record added after the first's. Written 2026-11-15 by
// Melanikus (Drake) for AVP_OpenInteng (G and L: a module's flags after its strategy's fields, the level's settings, one chunk after another).
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// A record keeps the words, choices and bytes its descriptors point to, so a record can't be copied, and the fields of another can't simply be
// added to its list. The second record is MOVED into the first's parts instead, which keeps everything it points to where it is, and the
// descriptors, values and sources are copied after the first's, pointing into it. The first record's title is kept.
#pragma once

#include <stddef.h>

#include "field_record.h"

namespace host
{
	// Adds the fields of *more after those of *record, and keeps *more's storage in *record (it is left empty). Gives back false, with
	// *record's fields as they were, when the two together would have more than mostFields fields (then *more is untouched too), or memory ran
	// out (then *more may have been moved from, and is not to be used).
	// *record's sources are made as long as its list first, so each field of *more has its own place in them, however many sources either had.
	bool JoinRecord(FieldRecord *record, FieldRecord *more, size_t mostFields);
}
