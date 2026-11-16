// special_objects_edit.h: Object Placement's five special objects as chunks of the level: AVP generator (0x34), AVP Bad Guy (0x35), Player starting position (0x3a), Sound (0x37) and
// Particle Generator (0x39), read, made, edited and taken away. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHERE THEY LIVE: the level's REBENVDT > SPECLOBJ chunk (made when missing), one chunk each, in the order they were made:
//   - AVP generator and AVP Bad Guy are BOTH an AVPGENER chunk (avpx avp_chunk.hpp AVP_Generator_Chunk): the file place (3 ints), the heading (the turn about the vertical axis, 0 to 4095),
//     the type, the flags, four bytes (texture, sub type, extra 1, extra 2), the name; a generator is made with type 0, a Bad Guy with type 1 (inteng's data word 0x5e7a48), both with the flags
//     0x3801 (marine, alien, predator and the mode flags) and the four bytes 0. A generator with flag 0x8000000 also has an AVPGENEX (not made here).
//   - Player starting position is an AVPSTART chunk: the place, the turn as 9 whole numbers (a 3 x 3 of 16.16), the 8-byte ID of the module it is in (0 when none).
//   - Sound is a SOUNDOB2 chunk (chunk_sound.hpp Sound_Object_Chunk): the place, inner range, outer range, volume, pitch, flags, probability, a spare number, the sound's name, the wav name.
//   - Particle Generator is a PARGENER chunk holding a PARGENDA chunk (AVP_Particle_Generator_Data_Chunk): type, flags, time (tenths of a second), probability, speed, quantity, two spare
//     numbers, the 8-byte ID (7 bytes of md5("PARTICLE GEN" + name), then 0), the parent's ID, the place, the turn as a quaternion, the name; an INDSOUND child when it has a sound.
// WHAT inteng WRITES (its objects' save methods, vtable +0xc: FUN_004e52a0 generator, FUN_004e59c0 bad guy, FUN_00421c50 start, FUN_004ed3c0 sound, FUN_004e6dd0 particle) and which
// defaults a fresh object has (its constructors) are in AvP-docs\engine\inteng\object-placement-reads.md, each fact marked READ or ASSUMED.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

class OpenLevelFile;   // src/io/level_file.h

namespace host
{
	enum class SpecialKind
	{
		Generator,
		BadGuy,
		PlayerStart,
		Sound,
		ParticleGenerator,
	};

	enum class SpecialResult
	{
		Done,
		BadCall,          // no level open, or an argument that cannot be
		NoEnvironment,    // the level has no REBENVDT
		NotFound,         // no object of that kind has that number
		BadName,          // a name is 0 to 63 plain letters
		BadValue,         // a number a field cannot hold
		CantBuild,        // the level could not be put together from its bytes
		LoaderRefused,    // the game's own loader would not take the changed level: nothing changed
	};

	char const *SpecialWords(SpecialResult result);

	// One special object as the level holds it. Which fields mean something depends on kind (see the top of this file); the others are 0.
	struct SpecialObject
	{
		SpecialKind kind = SpecialKind::Generator;
		size_t number = 0;                   // its place among the objects of its kind, from 0, in the file's order
		std::string name;
		int32_t place[3] = { 0, 0, 0 };      // file units
		// AVPGENER
		int32_t heading = 0;
		int32_t type = 0;
		uint32_t flags = 0;
		uint8_t texture = 0, subType = 0, extra1 = 0, extra2 = 0;
		// AVPSTART
		int32_t matrix[9] = { 65536, 0, 0, 0, 65536, 0, 0, 0, 65536 };
		uint8_t module[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		// SOUNDOB2
		int32_t inner = 0, outer = 0, volume = 0, pitch = 0, soundFlags = 0, probability = 0;
		std::string wav;
		// PARGENDA
		int32_t particleType = 0;
		uint32_t particleFlags = 0;
		uint16_t time = 0, particleProbability = 0, speed = 0, quantity = 0;
		uint8_t id[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		uint8_t parentId[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		float turn[4] = { 0, 0, 0, 1 };      // x y z w
		bool hasSound = false;               // a particle generator's INDSOUND child
		// AVPGENEX > GENEXSET, the generator's own settings: a SIBLING of the AVPGENER in SPECLOBJ, found by the name in its AVPGENNM child (avp_chunk.cpp:136-166). hasSettings is
		// true when the object's flag 0x8000000 is set and such a chunk holds settings; writing an object with hasSettings makes or replaces it.
		bool hasSettings = false;
		int32_t genRate = 0, genIncrease = 0;    // GenerationRate, GenRateIncrease (inteng's units: the dialog shows them x 0.01)
		uint8_t genLimit = 0;                    // GenLimit (Max NPCs)
		int32_t weights[15] = { 0 };             // PulseMarine, FlameMarine, SmartMarine, SadarMarine, GrenadeMarine, MinigunMarine, ShotgunCiv, PistolCiv, FlameCiv, UnarmedCiv,
		                                         // MolotovCiv, Alien, PredAlien, Praetorian, PistolMarine (the order of the GENEXSET chunk)
	};

	// Where a new object goes: the file place, the heading (0 to 4095), the turn as a 3 x 3 of 16.16 numbers (row by row) and the ID of the module it is in (zeros when none)
	struct SpecialPlace
	{
		int32_t place[3] = { 0, 0, 0 };
		int32_t heading = 0;
		int32_t matrix[9] = { 65536, 0, 0, 0, 65536, 0, 0, 0, 65536 };
		uint8_t module[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
		float turn[4] = { 0, 0, 0, 1 };   // the same turn as a quaternion (x y z w), for a particle generator
	};

	// Every special object of the level (the five kinds), in the file's order
	SpecialResult SpecialObjects_Read(OpenLevelFile *file, std::vector<SpecialObject> *out);

	// What a new object of that kind is, before it has a name: inteng's constructor values (generator type 0, Bad Guy type 1, flags 0x3801; sound 5000 / 10000 mm at scale 1, volume 127;
	// particle generator time 50, probability 50)
	SpecialObject SpecialObject_Fresh(SpecialKind kind, SpecialPlace const &where, double levelScale);

	// Makes one (Object Placement > Create): the chunk goes at the end of SPECLOBJ (made when missing). inteng makes the object with NO name (the name is typed in Edit) and saves it so: an empty
	// name is taken here too. *number gets its place among the objects of its kind. AVPGENER chunks are told apart by their type: 0 is a generator, any other a Bad Guy (ASSUMED).
	SpecialResult SpecialObject_Create(OpenLevelFile *file, SpecialKind kind, SpecialPlace const &where, double levelScale, std::string const &name, size_t *number);

	// Writes an object over the one of the same kind and number (Edit): the chunk is made again from the fields, children it has beyond what this module knows (an AVPGENEX, an ALTLOCAT, an
	// INDSOUND) are kept as they are
	SpecialResult SpecialObject_Write(OpenLevelFile *file, SpecialObject const &object);

	// Takes the object away (Delete)
	SpecialResult SpecialObject_Delete(OpenLevelFile *file, SpecialKind kind, size_t number);

	// A name: 0 to 63 plain letters (32 to 126)
	bool SpecialName_Good(std::string const &name);

	// The ID a particle generator holds for a name
	void ParticleGeneratorId(std::string const &name, uint8_t id[8]);
}
