// See object_header.h. Written 2026-11-14 by Melanikus (Dietrich) for AVP_OpenInteng (the two-dev sprint, D1 and D2). AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".

#include "object_header.h"

#include <exception>

#include "field_descriptor_from_model.h"

namespace host
{
	namespace
	{
		// The help lines: the old editor had none, so these are ours, and say what the game does with each bit (object_header.h)
		char const GameModeHelp[] =
			"Ticked: the object is in this species' game. The game reads it only for pickups and objects with no strategy, and only when "
			"the object loads on the PC.";
		char const DifficultyHelp[] =
			"Ticked: the object is in the game at this difficulty. The file keeps the opposite, a bit that means not at this difficulty.";
	}

	bool AddObjectHeaderBoxes(const ObjectHeader &header, FieldRecord *record)
	{
		if (!record)
		{
			return false;
		}

		try
		{
			char const *const modeHelp = Keep(record, GameModeHelp);
			char const *const difficultyHelp = Keep(record, DifficultyHelp);

			PushBit(record, Keep(record, "Game mode: Marine"), modeHelp, kHeaderMarine, header.flags);
			PushBit(record, Keep(record, "Game mode: Alien"), modeHelp, kHeaderAlien, header.flags);
			PushBit(record, Keep(record, "Game mode: Predator"), modeHelp, kHeaderPredator, header.flags);

			// The file keeps "not at this difficulty", so each difficulty's box is given the opposite of those three bits: ticked means
			// the object is in the game then (object_setup.cpp:659-664). Every other bit of the word is the file's.
			uint32_t const difficulties = header.flags ^ (kHeaderNotEasy | kHeaderNotMedium | kHeaderNotHard);

			PushBit(record, Keep(record, "Difficulty: Easy"), difficultyHelp, kHeaderNotEasy, difficulties);
			PushBit(record, Keep(record, "Difficulty: Medium"), difficultyHelp, kHeaderNotMedium, difficulties);
			PushBit(record, Keep(record, "Difficulty: Hard"), difficultyHelp, kHeaderNotHard, difficulties);
			return true;
		}
		catch (std::exception const &)
		{
			// Running out of memory shows nothing, as any other mistake does
			EmptyRecord(record);
			return false;
		}
	}

	bool BuildObjectHeaderRecord(const ObjectHeader &header, FieldRecord *record)
	{
		if (!record)
		{
			return false;
		}

		try
		{
			EmptyRecord(record);
			record->title = header.name;
		}
		catch (std::exception const &)
		{
			EmptyRecord(record);
			return false;
		}

		return AddObjectHeaderBoxes(header, record);
	}
}
