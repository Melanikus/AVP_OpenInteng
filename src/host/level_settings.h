// level_settings.h: THE LEVEL'S OWN SETTINGS, EDITABLE: the old editor's (cencon's) two dialogs "AvP Environment Settings" (the AVPENVIR chunk:
// the sky colour, the stars, the Predator's starting kit and spears, the Marine's jetpack) and "Global AvP generator properties" (the GLOGENDC
// chunk: how many NPCs the generators make, how fast, and the hive's attack and regroup time). Inteng has no edit for either; cencon's dialogs
// are the authority for the fields and their words. Written 2026-11-27 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code". The design, with the owner's two answers: AvP-docs/director/design-vasquez-oi-envgen.md.
//
// This file is the data side only: reading the two chunks as the game reads them, setting a field (refused outside its range, with the field's
// own reason from the model's table, src/model/chunk_fields.cpp), and writing the chunk back. The two forms that use it are Strategy mode's
// (strategy_screen.cpp, the commands below).
//
// HOW THE GAME READS THEM (the avpx pin, avp_chunk.cpp/.hpp; Drake's read of 2026-11-27, AvP-docs/tools/drake/oi-envgen-read-20261127.md):
//  - Both are children of the level's REBENVDT (every one of the 63 stock levels has one). The game takes the FIRST of each name.
//  - AVPENVIR: 24 bytes as cencon wrote it: its own size, the sky's red, green and blue, one word of kit bits (0 pistol, 1 plasma caster, 2 disc,
//    3 grappling hook, 4 medicomp, 5 jetpack, 6 stars; 7-31 spare), the spear count. The game chooses its defaults by the block's first number:
//    under 16, the sky is 200, 200, 200; under 24, the kit is pistol, caster, disc and medicomp (0x17) with 30 spears (avp_chunk.cpp:577-606).
//    A level with none gets those same defaults (:608-627). 57 of the 63 stock levels hold one; all are 24 bytes.
//  - GLOGENDC: 28 bytes: the enemy generated (0 aliens, 1 marines; the loader refuses anything else), max NPCs, NPCs per minute, the change per
//    minute, the hive state change time in whole seconds, and two spares. A level with none: aliens, 25, 4, 2, 60. 46 of the 63 hold one.
//
// THE RULES (the owner, 2026-11-27, through Bishop):
//  - A typed value outside its field's range is refused, with the range and the reason shown. A value the map ALREADY holds outside it (27
//    stock levels hold a hive time above 4,681 s) is kept as it is unless it's changed: the form shows it with a warning.
//  - The write goes through the game's own loader: the level is written as the next save would write it, with this one chunk in place of the
//    first of its name in REBENVDT (or, when the level has none, added last), and OpenLevelFile::ReplaceLevel reads the result with the game's
//    loader. If the loader won't take it, nothing changes. Only this chunk's bytes change; the spare kit bits and the two spares are kept.
//  - AVPENVIR is always written whole: 24 bytes (or the longer block a level holds), its first number its own size.
//  - A chunk the editor can't read as the game does is never edited: the form doesn't open, and says why. That covers an AVPENVIR under 24
//    bytes (Bishop 11-27: the game's minimum is 24 from Drake's PR #313, and every one of 1,133 known RIFs holds 24 that say 24; the pinned
//    game still reads a shorter one with its defaults, which the model's table keeps until the pin moves) and any size the model refuses.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <vector>

class OpenLevelFile;   // the editor's own open of a level (src/io/level_file.h)

namespace host
{
	// Strategy mode's two commands for these (menu-status.json "extras"; our own numbers, as Waypoint's 0x7334-0x7338 are: inteng has no command)
	int32_t const kLevelSettingsEnvironmentCommand = 0x7340;   // "AvP Environment Settings..."
	int32_t const kLevelSettingsGeneratorsCommand = 0x7341;    // "AvP Generator Properties..."

	enum class LevelSettingsKind
	{
		Environment,   // AVPENVIR
		Generators,    // GLOGENDC
	};

	enum class LevelSettingsResult
	{
		Done,
		BadCall,         // no level, or a payload of the wrong size for its chunk
		NoEnvironment,   // the level has no REBENVDT to hold the chunk
		Refused,         // the level holds the chunk, but the editor can't read it as the game would (*why says why): it isn't edited
		CantBuild,       // the level couldn't be put together from its bytes
		LoaderRefused,   // the game's own loader wouldn't take the changed level, so nothing was changed
	};

	// What an answer means, in plain words for the status line: static text, never null
	char const *LevelSettingsWords(LevelSettingsResult result);

	// The chunk's 8-letter name: "AVPENVIR" or "GLOGENDC"
	char const *LevelSettings_ChunkName(LevelSettingsKind kind);

	// The settings the game uses for a level that holds no such chunk: AVPENVIR 24 bytes (24; 200, 200, 200; 0x17; 30), GLOGENDC 28 bytes
	// (0; 25; 4; 2; 60; 0; 0)
	std::vector<uint8_t> LevelSettings_Defaults(LevelSettingsKind kind);

	// payload as the game reads it, into *out: a whole block, as it is. False, with *why, when it's an AVPENVIR under 24 bytes, when the model
	// refuses the payload's size (src/model CheckChunkSize: the game would misread it), or when GLOGENDC's enemy is neither aliens nor marines.
	bool LevelSettings_AsTheGameReads(LevelSettingsKind kind, std::vector<uint8_t> const &payload, std::vector<uint8_t> *out, char const **why);

	// What the level holds, as the game reads it, into *settings (*had: whether the level holds the chunk; when it doesn't, *settings are the
	// game's defaults). Done, NoEnvironment, Refused (*why), CantBuild or BadCall.
	LevelSettingsResult LevelSettings_Read(OpenLevelFile *file, LevelSettingsKind kind, std::vector<uint8_t> *settings, bool *had, char const **why);

	// One field of settings by its game name (the model's table: "sky_colour_red", "kit", "predator_num_spears", "EnemyGenerated",
	// "MaxNPCSOnLevel", "NPCSPerMinute", "NPCAcceleration", "HiveStateChangeTime"). Get gives it back as stored (the kit: its seven bits only).
	// Set refuses a value outside the field's range with that field's own reason in *why, and changes nothing then; for the kit, only its seven
	// named bits are set and the spare bits are kept.
	bool LevelSettings_Get(LevelSettingsKind kind, std::vector<uint8_t> const &settings, char const *field, int32_t *value);
	bool LevelSettings_Set(LevelSettingsKind kind, std::vector<uint8_t> *settings, char const *field, int32_t value, char const **why);

	// The field's range, as the model's table holds it, for showing beside its box: false when the field has none
	bool LevelSettings_Range(LevelSettingsKind kind, char const *field, int32_t *lowest, int32_t *highest);

	// Writes settings (as LevelSettings_AsTheGameReads gives them) into the level, through the game's loader, as the header says.
	LevelSettingsResult LevelSettings_Write(OpenLevelFile *file, LevelSettingsKind kind, std::vector<uint8_t> const &settings, char const **why);
}
