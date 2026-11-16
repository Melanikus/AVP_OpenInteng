// field_adapters.h: the record the field panel shows for what's selected: the ONE place every inspect row's adapter plugs in. Written
// 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (the two-dev sprint). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's
// own code".
//
// The editor asks once each time the selection changes (selection.h), and shows what it's given in the field panel host
// (field_panel_host.h). Each kind of thing the mapper can select gets its own adapter, which turns it into a record (field_record.h) from the
// field model's tables: an object's header (its name, ID and place: coverage D1) and an object's strategy (BuildStrategyRecord). They plug
// in inside FieldAdapters_Build, one kind of thing at a time, and nothing else changes: the editor's loop, the panel and the host stay as
// they are.
//
// WHAT EACH KIND GETS NOW. An object the open level holds with a strategy gets its strategy's fields (strategy_record.h) and then its
// header's six boxes of dialog 129 (object_header.h), under "<name>: <kind> (<number>)"; one with no strategy gets its header alone.
// An object with no name has the stand-in's title, its ID ("Object
// 0123456789ABCDEF"); an object the level doesn't hold, and a module, get the stand-in alone: a record with no fields whose title says
// what's selected. OURS, UNPROVEN: the old inteng has nothing like the stand-in (vasquez-ui.md says what its panels show).
//
// Everything an adapter reads from a level is hostile (R-INT-6, the rules in field_panel_host.h): the host reads the title through the safe
// text, and every descriptor is checked whole before it's shown. The header's name is copied only to its first 0 and within the game's 64
// bytes (level_objects.cpp).

#pragma once

#include "field_edit.h"
#include "field_model.h"
#include <stddef.h>

#include <vector>

#include "field_record.h"
#include "selection.h"

namespace host
{
	// One run of an editable record's fields, and the chunk their values are written to. A strategy's fields have strategyKind; a chunk's
	// (the module's flags, the level's settings) have chunkKind. (Added 2026-11-15 by Melanikus (Drake), S3d, G and L.)
	struct EditPart
	{
		EditPart() : first(0), count(0), chunk(0), strategyKind(0), chunkKind(0) {}

		size_t first;   // the record's field this run starts at
		size_t count;   // how many fields
		Chunk *chunk;   // whose bytes the store holds
		model::StrategyKind const *strategyKind;   // set when the chunk is a strategy
		model::ChunkKind const *chunkKind;         // set when the chunk is one the model has a table for
	};

	// What an EDITABLE record (below) can be edited through: its runs of fields and the chunk of each. A field in none of them can't be changed.
	struct ShownEdit
	{
		std::vector<EditPart> parts;

		// The run a field is in, or null
		EditPart const *PartOf(size_t field) const
		{
			for (EditPart const &part : parts)
			{
				if (field >= part.first && field - part.first < part.count)
				{
					return &part;
				}
			}

			return 0;
		}
	};

	// Builds the record for what's selected, in *record (what it held before is let go of). Gives back true when there's something to show.
	// It gives back false, with an empty record, for: nothing selected, a kind of selection it doesn't know, a module numbered below 0, an
	// object whose ID is all 0 (the game's own "no name", which the selection refuses too), and no place to put the record.
	bool FieldAdapters_Build(const Selected &selected, FieldRecord *record);

	// The same, but EDITABLE (S3d): the strategy's fields come from the bytes the STORE holds now (so an edit already made is shown), with
	// the fields the model can set opened for typing (record_edit.h). *edit says which chunk and kind each run of opened fields writes to; it
	// is emptied first, and stays empty when the record has nothing to edit. An object that is a module (it has a MODFLAGS chunk) shows its
	// flags after the strategy's fields and the header's boxes (G). The header's boxes stay read-only. A null store or edit gives back what the
	// one above does.
	bool FieldAdapters_Build(const Selected &selected, FieldRecord *record, PayloadStore const *store, ShownEdit *edit);

	// A placed-object light's properties (Light Sourcing, the P key, FUN_004b0470 "Light properties"): the PLOBJLIT of the object with that 8-byte ID, EDITABLE. NAMED: the whole record is shown (brightness,
	// spread, range, colours, flags and the times), inteng's dialog has the times, the two types and the two flags. False when the object has no placed light or no level is open.
	bool FieldAdapters_BuildPlacedLight(uint8_t const *objectId, FieldRecord *record, PayloadStore const *store, ShownEdit *edit);

	// The level's own settings (L), EDITABLE, from the bytes the store holds: the sky colour and the kit (AVPENVIR), the enemy generated
	// (GLOGENDC), the scale (ENVSDSCL) and the sound folder (SOUNDDIR), each as the level has it. Gives back false, with an empty record, when
	// no level is open or it has none of them. *edit is as above.
	bool FieldAdapters_BuildLevelSettings(PayloadStore const *store, FieldRecord *record, ShownEdit *edit);
}
