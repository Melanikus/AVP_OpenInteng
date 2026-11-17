// acoustic_mode.h: Object Placement's ACOUSTIC MODE, the data side (reassess/object-placement.md section 2, AC-1 .. AC-9). Written 2026-11-17 by
// Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHAT INTENG DOES (dump-full-3, read 11-17: FUN_004f57b0 the mode's frame and commands, FUN_004f5cf0 / FUN_004f63a0 the Module items,
// FUN_004f6010 / FUN_004f6560 the Default items, FUN_004f6670 / FUN_004f6760 the records, FUN_004f69e0 the names):
// - A MODULE's acoustics are its MODACOUS chunk, inside the MODULEDT chunk of the module's object: a type (0..25, or -1: the level's default)
//   and a reverb (-1.0: the default). The LEVEL's are the ENVACOUS chunk inside REBENVDT: a type and a reverb. Each holds a third word, spare.
// - FUN_004f6670(module, make): the module's MODULEDT, made at the end of its object's chunks when missing (only when make), then its MODACOUS,
//   made at the end of MODULEDT when missing (type -1, reverb -1.0, spare 0: avpx's Module_Acoustics_Chunk). FUN_004f6760(make): REBENVDT's
//   ENVACOUS, made at its end when missing (type 0, reverb 2.0, spare 0: Environment_Acoustics_Chunk).
// - Module > Acoustic type: each picked module's record, made when missing, takes the type picked. Module > Reverb: the same for the reverb.
//   Set To Default Acoustic Type / Reverb: only the picked modules that HAVE a record (none is made) get -1 / -1.0. Default > Acoustic type: the
//   level's record, made when missing, takes the type. Default > Reverb: the level's record is made when missing BEFORE the number window
//   opens (0x4f657e), so a Cancel still leaves it made; Ok writes the reverb.
// - A typed reverb is kept as: above 1.0 -> 2.0; at or below 0.0 -> 0.0; anything between as typed.
//
// WHAT THIS DOES: the same, on the editor's open level. A record that is there is changed in place (OpenLevelFile::SetPayload, its 12 bytes);
// records that have to be made are put in one new image of the level (OpenLevelFile::ReplaceLevel), so the game's own loader decides, and
// nothing changes when it refuses. After a ReplaceLevel every Chunk * the editor held is gone (object_edit.h says the same).
//
// NAMED, a guard (hostile input): inteng looks ENVACOUS up inside a REBENVDT it never checks for (FUN_004f6760 0x4f6781-0x4f678f); a level
// with no REBENVDT has no level record here (NoEnvironment), and nothing is made.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <string>
#include <vector>

class OpenLevelFile;   // the editor's own open of a level (src/io/level_file.h)

namespace host
{
	int32_t const kAcousticTypeCount = 26;   // FUN_004f5cf0 0x4f5e5c: 0x1a names, 20 bytes each from 0x5e4e90

	// FUN_004f69e0: the type's name, "Generic" .. "Psychotic", or "Invalid type" (0x5e7b44) outside 0 .. 25. Static text, never null.
	char const *AcousticTypeName(int32_t type);

	// The number windows' Ok (FUN_004f63a0 0x4f64b7-0x4f64df, FUN_004f6560 0x4f6609-0x4f662f): above 1.0 -> 2.0, at or below 0.0 -> 0.0
	float AcousticReverbKept(float typed);

	// A record as the level holds it now (its bytes as edited so far)
	struct AcousticRecord
	{
		int32_t type = 0;
		float reverb = 0.0f;
		int32_t spare = 0;
	};

	enum class AcousticResult
	{
		Done,
		BadCall,         // no level, or nothing named
		NoObject,        // no object has that ID
		NoEnvironment,   // the level has no REBENVDT (the NAMED guard above)
		BadRecord,       // a record is not 12 bytes, or a MODULEDT / REBENVDT is not a chunk holding chunks
		CantBuild,       // the level couldn't be put together from its bytes
		LoaderRefused,   // the game's own loader wouldn't take the changed level, so nothing was changed
	};

	// What an answer means, in plain words for the editor's status line: static text, never null
	char const *AcousticWords(AcousticResult result);

	// The module's record (its object's MODULEDT > MODACOUS), or false when it has none
	bool Acoustic_ModuleRecord(OpenLevelFile *file, uint8_t const *moduleId, AcousticRecord *out);

	// The level's record (REBENVDT > ENVACOUS), or false when it has none
	bool Acoustic_LevelRecord(OpenLevelFile *file, AcousticRecord *out);

	enum class AcousticField
	{
		Type,     // the record's first word
		Reverb,   // its second
	};

	// One field of every listed module's record: made when missing when make is true (Module > Acoustic type, Reverb), only the records there
	// when it is false (Set To Default ...). *changed (when not null) is how many records were written. A module named twice is written once.
	AcousticResult Acoustic_SetModules(OpenLevelFile *file, std::vector<std::array<uint8_t, 8>> const &modules, AcousticField field, int32_t type, float reverb,
		bool make, size_t *changed);

	// One field of the level's record, made when missing (Default > Acoustic type, Reverb's Ok)
	AcousticResult Acoustic_SetLevel(OpenLevelFile *file, AcousticField field, int32_t type, float reverb);

	// The level's record made when missing, nothing else (Default > Reverb, before its window: FUN_004f6760(1) at 0x4f657e)
	AcousticResult Acoustic_MakeLevelRecord(OpenLevelFile *file);

	// Module > Reverb's window starts at the first listed module whose record's reverb is not -1.0 (its bits 0xbf800000, 0x4f640b), else 2.0
	float Acoustic_ModuleReverbStart(OpenLevelFile *file, std::vector<std::array<uint8_t, 8>> const &modules);

	// A line of the mode's frame text, at (x, y) of the screen
	struct AcousticLine
	{
		int32_t x;
		int32_t y;
		std::string words;
	};

	// FUN_004f57b0 0x4f5b7f-0x4f5cda (the listing: ESI = 0, then FUN_004f6760(0), so nothing is made): "Default Type : %s" (16, 32: the level
	// record's type, "Invalid type" outside 0..25; with no record type 0, "Generic"), "Default Reverb : %.2f" (16, 48: 0 with no record); for
	// the module under the pointer (moduleId, null for none): its name (16, 64), "Type : %s" or "Type : Default" (16, 80: no record or a type
	// below 0), "Reverb : %.2f" or "Reverb : Default" (16, 96: no record or a reverb below 0.0, FCOMP with 0.0 at 0x4f5c87)
	void Acoustic_FrameLines(OpenLevelFile *file, uint8_t const *moduleId, std::vector<AcousticLine> *lines);
}
