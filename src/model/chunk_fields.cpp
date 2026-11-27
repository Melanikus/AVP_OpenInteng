// The tables for the chunks the editor edits directly (not strategies): their fields, and the names of their bits and choices.
// Written 2026-11-14 by Melanikus for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Every layout was read from the game's own reader at the editor's dev pin, AvPx commit 491deb22; paths are under its 3dc folder. The old
// editor's limits come from a study of its program (fields-spec.md, "Lights and sounds in the old editor"). A light's starting
// values in the old editor aren't known yet, so no light field claims one. A new sound's volume (127) is the only sound value
// traced in the old editor; its ranges were only inferred, so they aren't claimed either.

#include <string.h>

#include "field_model.h"

namespace model
{
	namespace
	{
		double const LightRangeMost = 268435456.0;   // 2^28
		double const PositionMost = 16777216.0;      // 2^24

		char const LightRangeWhy[] = "a light's range is at least 1 and at most 2^28 game units, or the loader refuses the level (CheckLightRange, chunk_light.cpp:121-129)";
		char const PositionWhy[] =
			"a position is within 2^24 game units (about 16.8 km) of the level's middle along each of x, y and z, or the loader refuses the level (CheckPlace, chunk.hpp:488-510)";
		char const LightTimeWhy[] = "the game refuses a light time outside 0 to 32767 thousandths of a second";
		char const LightTypeWhy[] = "the game knows three light types (standard, strobe, flicker); any other number does nothing";
		char const OnOffTypeWhy[] = "the game knows three ways to switch a light (standard, fade, flicker); any other number does nothing";

		char const LightReader[] = "win95/chunk_light.cpp:131-164";
		char const PlacedLightReader[] = "win95/chunk_light.cpp:370-407";
		char const PlacedLightTimes[] = "win95/chunk_light.cpp:379-390";
		char const OldEditorLights[] = "the old editor's light attributes (fields-spec.md: FUN_004acf30)";

		// The bits of a light's flag words a mapper may change, as the old editor offered them. The others belong to the game's
		// running of the light, and are kept as they are.
		uint32_t const EngineLightBits = 0x1 | 0x2 | 0x4 | 0x10 | 0x800;   // LFlag_CosAtten, _CosSpreadAtten, _Omni, _NoShadows, _NoSpecular
		// A level's own light (STDLIGHT): inteng's light mode changes engine 0x1, 0x2, 0x4 and 0x800 and never 0x10 (No shadows): the master record's engine word has no case for it
		// (FUN_004acf30, outightmode.txt:68-206; the 0x10 only goes into the engine's working copy from the Shadows bit, :41-47), so here it is shown and not changed
		// (Bishop's audit 2026-11-15). The placed light (PLOBJLIT) keeps the wider set until its Light FX dialog is read.
		uint32_t const StdLightEngineBits = 0x1 | 0x2 | 0x4 | 0x800;
		// A level's own light (STDLIGHT) local flags the old editor changes: Shadows (LOFlag_ShadowData 0x2), Runtime (0x10) and No PreLight (0x40)
		// (attributes 3, 7 and 11, FUN_004acf30, out/lightmode.txt:131-138, 160-167, 189-197). LOFlag_On (0x1) is NOT offered: inteng's on/off is
		// editor state it never saves (FUN_004ac240, FUN_004ad630). Named differences: frost-deviations.md.
		uint32_t const StdLightLocalBits = 0x2 | 0x10 | 0x40;
		uint32_t const LocalLightBits = 0x10 | 0x40;                       // LOFlag_Runtime, _NoPreLight
		uint32_t const PlacedLightBits = 0x1 | 0x2 | 0x4;                  // PlacedLightFlag_On, _SwapColourBright, _NoSpecular

		char const DistanceWhy[] = "the editor keeps a sound's reach to 0-2^24 game units, the bound it keeps positions to";
		char const VolumeWhy[] =
			"the game plays a volume of 0-127: a sound starts at the nearer end (avp/psnd.cpp:407-408), but a change to one below 0 "
			"plays as 127 (:612-613)";
		char const PitchWhy[] =
			"the game keeps a pitch within 4 octaves either way (6144): a sound starts at the nearer end (avp/psnd.cpp:414-415), but a "
			"change to one below -6144 plays as +6144 (:640-641)";
		char const TrackSoundIndexWhy[] = "the game refuses a track sound that isn't its middle (0), start (1) or end (2) sound";

		char const SoundReader[] = "win95/chunk_sound.cpp:50-76";
		char const SoundLimits[] = "avp/psnd.h:49-55";
		char const TrackSoundReader[] = "win95/chunk_object.cpp:1081-1107";
		char const NewSoundInOldEditor[] = "the old editor's new sound (fields-spec.md: FUN_004ecb20)";

		uint32_t const SoundBits = 0x1 | 0x2;     // SoundObjectFlag_NotPlayingAtStart, _NoLoop
		uint32_t const TrackSoundBits = 0x1;      // TrackSoundFlag_Loop

		char const GeneratorKindWhy[] = "the game knows ten kinds (0 a generator, 1-9 one placed creature or gun); any other number makes nothing";
		char const WeightWhy[] = "the loader refuses a weight outside 0 to 2^24, which keeps the sum of all fifteen inside a whole number (avp_chunk.cpp:309-320)";
		char const ChanceWhy[] = "the old editor keeps a chance to 0-100";
		char const ParticleKindWhy[] = "a particle generator is one of the game's four kinds, from 0 to 3, or the loader refuses the level (avp_chunk.cpp:817-825)";
		char const QuaternionWhy[] = "each part of an orientation is a real number from -2 to 2, or the loader refuses the level (CheckTurn, chunk.hpp:525-545); a real turn's are within 1";

		char const GeneratorReader[] = "avp/win95/avp_chunk.cpp:75-105";
		char const GeneratorSettingsReader[] = "avp/win95/avp_chunk.cpp:262-321";
		char const ParticleReader[] = "avp/win95/avp_chunk.cpp:810-841";
		char const ParticleTurn[] = "avp/win95/avp_chunk.cpp:839, chunk.hpp:525-545";
		char const StartReader[] = "avp/win95/avp_chunk.cpp:449-456";

		// The bits of a generator's flags a mapper may change: the game modes it's in, the difficulties it's not in, its own NPC
		// limit, its own rate, advanced, multiplayer start, and starting inactive. MODEFLAGSSET (0x1) isn't a choice: the old
		// editor set it on every one, and it's kept as it is.
		uint32_t const GeneratorBits = 0x3800 | 0x700000 | 0x2000000 | 0x4000000 | 0x8000000 | 0x40000000 | 0x80000000;
		uint32_t const ParticleBits = 0x1;       // ParticleGeneratorFlag_Inactive

		double const WeightMost = 16777216.0;   // 2^24, the loader's own bound (avp_chunk.cpp:316)

		// The level's own settings, which the old editor's cencon edited (vasquez-tools.md, its dialogs 139 and 143; fields-spec.md
		// section 3). (Added 2026-11-14 by Melanikus (Vasquez), M7.)
		char const EnvironmentReader[] = "avp/win95/avp_chunk.cpp:577-606, avp_chunk.hpp:257-281";
		char const CenconEnvironment[] = "cencon's dialog 143, \"AvP Environment Settings\" (vasquez-tools.md)";
		char const SkyWhy[] = "the old editor (cencon) takes a sky colour of 0-255";
		uint32_t const KitBits = 0x7f;   // the seven named bits of the kit word, low bit first (avp_chunk.hpp:269-277); spare_bits are kept

		char const GlobalGeneratorReader[] = "avp/win95/avp_chunk.cpp:376-401";
		char const CenconGenerators[] = "cencon's dialog 139, \"Global AvP generator properties\" (vasquez-tools.md)";
		char const EnemyWhy[] = "the game's loader refuses a level whose generators make anything but aliens (0) or marines (1)";

		// The ranges the level's settings forms keep to (2026-11-27, Vasquez, design-vasquez-oi-envgen.md; Drake's read of the game at the avpx
		// pin). The game reads any number here; these are the numbers it can work with. A map that already holds a number outside them keeps it
		// (the forms show it, flagged): only a newly typed one is refused.
		char const SpearsWhy[] = "the game multiplies the spear count by 65,536 into a signed number (inventory.cpp:621): more than 32,767 overflows, "
		                         "and less than 0 gives the speargun negative rounds";
		char const MostNpcsWhy[] = "the game holds Max NPCs Generated to 0-255 (bh_generator.cpp:496-499), so a bigger number does nothing more";
		char const PerMinuteWhy[] = "the game holds NPCs Generated Per Minute to 0-255 (bh_generator.cpp:500-503), so a bigger number does nothing more";
		char const ChangeWhy[] = "the game holds the Change in Generation Rate Per Minute to 0-255 (bh_generator.cpp:504-507), so a bigger number does "
		                         "nothing more";
		char const HiveWhy[] = "the game works out the hive's time as seconds x 65,536 x 7 / 8 in a signed number (bh_generator.cpp:675): more than "
		                       "4,681 seconds overflows, and at 0 or less the hive flips between attack and regroup every frame";

		char const ScaleWhy[] = "the loader refuses a scale outside 0.5-1, or one that isn't a real number (NaN or endless)";

		// MODFLAGS: inteng's flag screen (Ghidra, FUN_00525700) and the game's reader. The message number shares the word: it's bits
		// 21-27, stored as the number times 2^21. The game's MODULEFLAG_HORIZONTALDOOR (0x08000000, include/module.h:302) is one of
		// those bits, so a door module's message of 64-127 also turns that on (fields-spec.md, "MODFLAGS, the game's side"). The loader
		// refuses a flag word with any bit outside 0xFFFF0200 (chunk_object.cpp:833, :842-845); the ten flags below and the message
		// bits are exactly those bits, so the editor never sets a bit the loader would refuse (the test checks it).
		char const ModuleFlagScreen[] =
			"inteng's flag screen (fields-spec.md section 3: FUN_00525700), the game's use of the word (projload.cpp:2371-2375, :2486-2489) "
			"and the loader's own mask (chunk_object.cpp:828-846)";
		uint32_t const ModuleFlagBits = 0x80000000 | 0x40000000 | 0x20000000 | 0x10000000 | 0x00100000 | 0x00080000 | 0x00040000 |
		                                0x00020000 | 0x00010000 | 0x00000200;
		uint32_t const ModuleMessageBits = 0x0FE00000;
		char const ModuleMessageWhy[] =
			"the old editor takes a module message number of 1-127, or 0 for none, kept in bits 21-27 of the flags: the value as stored is "
			"the number times 2^21";

		// The fragment types, which cencon edited (vasquez-tools.md: its dialog 142, "Edit Fragment Types"): what a shape breaks into when
		// the object holding it is destroyed, and the sound it makes. (Added 2026-11-14 by Melanikus (Vasquez), L1.)
		char const FragmentPlaceSource[] = "win95/chunk_fragments.cpp:109-110; chunk.hpp:488-510";
		char const FragmentRangeSource[] =
			"win95/chunk_fragments.cpp:161-162, :172-175; the labels are the placed sound's, as cencon imports its dialog (edit_sound, "
			"ccamain.exe-strings.txt) and its fragment sound's Add Sound and Edit most likely use it";
		double const FragmentPiecesMost = 750.0;   // a level's display blocks: maxobjects (avp/win95/system.h:94)
		char const FragmentPiecesWhy[] =
			"the game makes this many pieces of the shape each time the object breaks, and stops at the first one it has no room for "
			"(avp/bh_debris.cpp:716-719, :777-835): a level has room for 750 objects on screen (display blocks) and 1000 behaviours "
			"(strategy blocks), avp/win95/system.h:94-96, so a number above 750 makes no more pieces than 750, and one below 0 makes none";

		// The game modes, which cencon edited (vasquez-tools.md: dialogs 144 and 145, "Game Mode Settings" and "Included data for this game
		// mode"): a mode's header names it, says whether the level defines it or includes it from another .rif, and lists those .rif files.
		// (Added 2026-11-14 by Melanikus (Vasquez), L1.)
		uint32_t const GameModeBits = 0x1 | 0x2 | 0x4;   // GameModeFlag_Editable, _Deleted, _Added (chunk_palette.hpp:490-492)
		// OBJHEAD1, an object's header (win95/chunk_object.cpp:396-452). Its flags are the object's: the bits a mapper may change are the game
		// modes the object is in (marine, alien, predator) and the difficulties it's not in, which are the same bits a generator's flags use
		// (chunk_object.hpp:22-36: "these flags have to correspond with the ones used by ed_ob_type and AVP generator flags"). The others (what
		// kind of object it is, which platform it loads on) are kept as they are. (Added 2026-11-15 by Melanikus (Frost), S9.)
		char const ObjectHeaderReader[] = "win95/chunk_object.cpp:393, :396-452";
		char const ObjectHeaderTurn[] = "win95/chunk_object.cpp:434-436; chunk.hpp:525-545";
		uint32_t const ObjectHeaderBits = 0x3800 | 0x700000;   // OBJECT_FLAG_AVPGAMEMODEMARINE, _ALIEN, _PREDATOR; OBJECT_FLAG_NOTDIFFICULTY1, 2, 3

		Anchor const S = Anchor::Start;
		Anchor const AfterList = Anchor::AfterFirstList;   // a number after a list's last item (RANTEXID's two spares)
	}

	// STDLIGHT: a light in the level's light set. 84 bytes.
	static Field const LightFields[] =
	{
		{ "light_number", "", S, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, LightReader },
		{ "location", "", S, 4, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -PositionMost, PositionMost, PositionWhy, false, 0, "win95/chunk_light.cpp:138; chunk.hpp:488-510" },
		{ "orientation", "", S, 16, Storage::IntMatrix, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, LightReader },
		{ "brightness", "Brightness", S, 52, Storage::Int32, Unit::LightBrightness, 0, Limit::Editor, 0, 131072,
		  "the old editor keeps a light's brightness to 0-200%", false, 0, OldEditorLights },
		{ "spread", "Spread", S, 56, Storage::Int32, Unit::LightSpread, 0, Limit::Editor, 1, 2048,
		  "the old editor keeps a light's spread to 1-2048, which is up to half a turn", false, 0, OldEditorLights },
		{ "range", "Range", S, 60, Storage::Int32, Unit::LightRange, 0, Limit::Loader, 1, LightRangeMost, LightRangeWhy, false, 0,
		  "win95/chunk_light.cpp:121-129, :150; the old editor allowed 0" },
		{ "colour_blue", "Blue", S, 64, Storage::Byte, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, LightReader },
		{ "colour_green", "Green", S, 65, Storage::Byte, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, LightReader },
		{ "colour_red", "Red", S, 66, Storage::Byte, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, LightReader },
		{ "engine_light_flags", "", S, 68, Storage::UInt32, Unit::Flags, StdLightEngineBits, Limit::None, 0, 0, "", false, 0, "include/prototyp.h:680-699" },
		{ "local_light_flags", "", S, 72, Storage::UInt32, Unit::Flags, StdLightLocalBits, Limit::None, 0, 0, "", false, 0, "win95/chunk_light.hpp:15-21" },
	};

	// PLOBJLIT: a placed light's own settings, in the object that holds it. 60 bytes of settings, then a count and that many 4-byte
	// items of extra data.
	static Field const PlacedLightFields[] =
	{
		{ "brightness", "", S, 0, Storage::Int32, Unit::LightBrightness, 0, Limit::None, 0, 0, "", false, 0, PlacedLightReader },
		{ "spread", "", S, 4, Storage::Int32, Unit::LightSpread, 0, Limit::None, 0, 0, "", false, 0, PlacedLightReader },
		{ "range", "", S, 8, Storage::Int32, Unit::LightRange, 0, Limit::Loader, 1, LightRangeMost, LightRangeWhy, false, 0,
		  "win95/chunk_light.cpp:121-129, :377" },
		{ "up_colour_blue", "Up colour blue", S, 12, Storage::Byte, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, PlacedLightReader },
		{ "up_colour_green", "Up colour green", S, 13, Storage::Byte, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, PlacedLightReader },
		{ "up_colour_red", "Up colour red", S, 14, Storage::Byte, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, PlacedLightReader },
		{ "down_colour_blue", "Down colour blue", S, 16, Storage::Byte, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, PlacedLightReader },
		{ "down_colour_green", "Down colour green", S, 17, Storage::Byte, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, PlacedLightReader },
		{ "down_colour_red", "Down colour red", S, 18, Storage::Byte, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, PlacedLightReader },
		{ "engine_light_flags", "", S, 20, Storage::UInt32, Unit::Flags, EngineLightBits, Limit::None, 0, 0, "", false, 0, "include/prototyp.h:680-699" },
		{ "local_light_flags", "", S, 24, Storage::UInt32, Unit::Flags, LocalLightBits, Limit::None, 0, 0, "", false, 0, "win95/chunk_light.hpp:15-21" },
		{ "fade_up_time", "Fade up time", S, 28, Storage::Int32, Unit::Milliseconds, 0, Limit::Loader, 0, 32767, LightTimeWhy, false, 0, PlacedLightTimes },
		{ "fade_down_time", "Fade down time", S, 32, Storage::Int32, Unit::Milliseconds, 0, Limit::Loader, 0, 32767, LightTimeWhy, false, 0, PlacedLightTimes },
		{ "up_time", "Time up", S, 36, Storage::Int32, Unit::Milliseconds, 0, Limit::Loader, 0, 32767, LightTimeWhy, false, 0, PlacedLightTimes },
		{ "down_time", "Time down", S, 40, Storage::Int32, Unit::Milliseconds, 0, Limit::Loader, 0, 32767, LightTimeWhy, false, 0, PlacedLightTimes },
		{ "start_time", "Timer start", S, 44, Storage::Int32, Unit::Milliseconds, 0, Limit::Loader, 0, 32767, LightTimeWhy, false, 0, PlacedLightTimes },
		{ "light_type", "Light type", S, 48, Storage::Int32, Unit::Choice, 0, Limit::GameMeaning, 0, 2, LightTypeWhy, false, 0, "avp/bh_light.h:23-28, avp/bh_light.cpp:410" },
		{ "on_off_type", "On/off type", S, 52, Storage::Int32, Unit::Choice, 0, Limit::GameMeaning, 0, 2, OnOffTypeWhy, false, 0, "avp/bh_light.h:30-35, avp/bh_light.cpp:600" },
		{ "flags", "", S, 56, Storage::UInt32, Unit::Flags, PlacedLightBits, Limit::None, 0, 0, "", false, 0, "win95/chunk_light.hpp:56-58" },
	};

	// SOUNDOB2: a sound placed in the level. 40 bytes of numbers, then its name and, straight after, its file name (name_model.h).
	static Field const SoundFields[] =
	{
		{ "position", "", S, 0, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -PositionMost, PositionMost, PositionWhy, false, 0, "win95/chunk_sound.cpp:55; chunk.hpp:488-510" },
		{ "inner_range", "Inner range", S, 12, Storage::Int32, Unit::GameUnits, 0, Limit::Editor, 0, PositionMost, DistanceWhy, false, 0, SoundReader },
		{ "outer_range", "Outer range", S, 16, Storage::Int32, Unit::GameUnits, 0, Limit::Editor, 0, PositionMost, DistanceWhy, false, 0, SoundReader },
		{ "max_volume", "Volume", S, 20, Storage::Int32, Unit::Plain, 0, Limit::GameMeaning, 0, 127, VolumeWhy, true, 127, NewSoundInOldEditor },
		{ "pitch", "Pitch", S, 24, Storage::Int32, Unit::Pitch128, 0, Limit::GameMeaning, -6144, 6144, PitchWhy, false, 0, SoundLimits },
		{ "flags", "", S, 28, Storage::UInt32, Unit::Flags, SoundBits, Limit::None, 0, 0, "", false, 0, "win95/chunk_sound.hpp:11-12" },
		{ "probability", "Probability (1 in x)", S, 32, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, SoundReader },
	};

	// TRAKSOUN: one of a moving object's three sounds. 24 bytes of numbers, then its file name (name_model.h).
	static Field const TrackSoundFields[] =
	{
		{ "inner_range", "Inner range", S, 0, Storage::UInt32, Unit::GameUnits, 0, Limit::Editor, 0, PositionMost, DistanceWhy, false, 0, TrackSoundReader },
		{ "outer_range", "Outer range", S, 4, Storage::UInt32, Unit::GameUnits, 0, Limit::Editor, 0, PositionMost, DistanceWhy, false, 0, TrackSoundReader },
		{ "max_volume", "Volume", S, 8, Storage::Int32, Unit::Plain, 0, Limit::GameMeaning, 0, 127, VolumeWhy, false, 0, SoundLimits },
		{ "pitch", "Pitch", S, 12, Storage::Int32, Unit::Pitch128, 0, Limit::GameMeaning, -6144, 6144, PitchWhy, false, 0, SoundLimits },
		{ "flags", "", S, 16, Storage::UInt32, Unit::Flags, TrackSoundBits, Limit::None, 0, 0, "", false, 0, "win95/chunk_object.hpp:423" },
		{ "index", "Which sound", S, 20, Storage::Int32, Unit::Choice, 0, Limit::Loader, 0, 2, TrackSoundIndexWhy, false, 0,
		  "win95/chunk_object.cpp:1096-1103" },
	};

	// AVPGENER: a generator, or one placed creature or gun. 28 bytes, then its name (name_model.h).
	static Field const GeneratorFields[] =
	{
		{ "location", "", S, 0, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -PositionMost, PositionMost, PositionWhy, false, 0, "avp/win95/avp_chunk.cpp:80; chunk.hpp:488-510" },
		{ "orientation", "Facing", S, 12, Storage::Int32, Unit::Angle4096, 0, Limit::None, 0, 0, "", false, 0, GeneratorReader },
		{ "type", "Kind", S, 16, Storage::Int32, Unit::Choice, 0, Limit::GameMeaning, 0, 9, GeneratorKindWhy, false, 0,
		  "avp/win95/object_setup.cpp:3195-3245" },
		{ "flags", "", S, 20, Storage::UInt32, Unit::Flags, GeneratorBits, Limit::None, 0, 0, "", false, 0, "avp/win95/avp_chunk.hpp:14-31" },
		{ "textureID", "Texturing", S, 24, Storage::Byte, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, GeneratorReader },
		// What these three mean depends on the kind (object_setup.cpp: marines :2684, predators :2788, sentry guns :3027, xenoborgs :3089)
		{ "sub_type", "NPC type", S, 25, Storage::Byte, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, GeneratorReader },
		{ "extra1", "", S, 26, Storage::Byte, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, GeneratorReader },
		{ "extra2", "", S, 27, Storage::Byte, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, GeneratorReader },
	};

	// GENEXSET: a generator's own settings. 20 bytes, then a block of weights whose first number is its own size (64 when the
	// editor writes it). The game leaves at 0 any weight a shorter block doesn't hold.
	static Field const GeneratorSettingsFields[] =
	{
		{ "GenerationRate", "Initial generation rate per minute", S, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, GeneratorSettingsReader },
		{ "GenRateIncrease", "Increase in rate per minute", S, 4, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, GeneratorSettingsReader },
		{ "GenLimit", "Max NPCs", S, 8, Storage::Byte, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, GeneratorSettingsReader },
		{ "PulseMarine_Wt", "Pulse rifle marine", S, 24, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, WeightMost, WeightWhy, false, 0, GeneratorSettingsReader },
		{ "FlameMarine_Wt", "Flamethrower marine", S, 28, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, WeightMost, WeightWhy, false, 0, GeneratorSettingsReader },
		{ "SmartMarine_Wt", "Smartgun marine", S, 32, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, WeightMost, WeightWhy, false, 0, GeneratorSettingsReader },
		{ "SadarMarine_Wt", "SADAR marine", S, 36, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, WeightMost, WeightWhy, false, 0, GeneratorSettingsReader },
		{ "GrenadeMarine_Wt", "Grenade launcher marine", S, 40, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, WeightMost, WeightWhy, false, 0, GeneratorSettingsReader },
		{ "MinigunMarine_Wt", "Minigun marine", S, 44, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, WeightMost, WeightWhy, false, 0, GeneratorSettingsReader },
		{ "ShotgunCiv_Wt", "Shotgun civilian", S, 48, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, WeightMost, WeightWhy, false, 0, GeneratorSettingsReader },
		{ "PistolCiv_Wt", "Pistol civilian", S, 52, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, WeightMost, WeightWhy, false, 0, GeneratorSettingsReader },
		{ "FlameCiv_Wt", "Flamethrower civilian", S, 56, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, WeightMost, WeightWhy, false, 0, GeneratorSettingsReader },
		{ "UnarmedCiv_Wt", "Unarmed civilian", S, 60, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, WeightMost, WeightWhy, false, 0, GeneratorSettingsReader },
		{ "MolotovCiv_Wt", "Molotov civilian", S, 64, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, WeightMost, WeightWhy, false, 0, GeneratorSettingsReader },
		{ "Alien_Wt", "Alien", S, 68, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, WeightMost, WeightWhy, false, 0, GeneratorSettingsReader },
		{ "PredAlien_Wt", "Predalien", S, 72, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, WeightMost, WeightWhy, false, 0, GeneratorSettingsReader },
		{ "Praetorian_Wt", "Praetorian", S, 76, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, WeightMost, WeightWhy, false, 0, GeneratorSettingsReader },
		{ "PistolMarine_Wt", "Pistol marine", S, 80, Storage::Int32, Unit::Plain, 0, Limit::Loader, 0, WeightMost, WeightWhy, false, 0, GeneratorSettingsReader },
	};

	// PARGENDA: a particle generator. 68 bytes, then its name, which it can do without.
	static Field const ParticleFields[] =
	{
		{ "type", "Type", S, 0, Storage::Int32, Unit::Choice, 0, Limit::Loader, 0, 3, ParticleKindWhy, false, 0,
		  "avp/win95/avp_chunk.cpp:817-825; avp/bh_particle_generator.h:12-15" },
		{ "flags", "", S, 4, Storage::UInt32, Unit::Flags, ParticleBits, Limit::None, 0, 0, "", false, 0, ParticleReader },
		{ "time", "Frequency", S, 8, Storage::UInt16, Unit::TenthsOfASecond, 0, Limit::None, 0, 0, "", false, 0, ParticleReader },
		{ "probability", "Probability", S, 10, Storage::UInt16, Unit::Percent, 0, Limit::Editor, 0, 100, ChanceWhy, false, 0,
		  "the old editor's particle dialog (fields-spec.md)" },
		{ "speed", "Speed", S, 12, Storage::UInt16, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, ParticleReader },
		{ "quantity", "Quantity", S, 14, Storage::UInt16, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, ParticleReader },
		{ "id", "", S, 24, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, ParticleReader },
		{ "parent_id", "Parent", S, 32, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, ParticleReader },
		{ "location", "", S, 40, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -PositionMost, PositionMost, PositionWhy, false, 0, "avp/win95/avp_chunk.cpp:837; chunk.hpp:488-510" },
		{ "orientation_x", "", S, 52, Storage::Float32, Unit::Plain, 0, Limit::Loader, -2, 2, QuaternionWhy, false, 0, ParticleTurn },
		{ "orientation_y", "", S, 56, Storage::Float32, Unit::Plain, 0, Limit::Loader, -2, 2, QuaternionWhy, false, 0, ParticleTurn },
		{ "orientation_z", "", S, 60, Storage::Float32, Unit::Plain, 0, Limit::Loader, -2, 2, QuaternionWhy, false, 0, ParticleTurn },
		{ "orientation_w", "", S, 64, Storage::Float32, Unit::Plain, 0, Limit::Loader, -2, 2, QuaternionWhy, false, 0, ParticleTurn },
	};

	// AVPSTART: where the player starts. 56 bytes.
	static Field const StartFields[] =
	{
		{ "location", "", S, 0, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -PositionMost, PositionMost, PositionWhy, false, 0, "avp/win95/avp_chunk.cpp:455; chunk.hpp:488-510" },
		{ "orientation", "", S, 12, Storage::IntMatrix, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, StartReader },
		{ "moduleID", "", S, 48, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, StartReader },
	};

	// AVPENVIR: the level's environment settings, in REBENVDT. A block whose first number, data_size, is its own size: 24 bytes as
	// the editor writes it. The game's reader takes any size (it is registered with no minimum, avp_chunk.cpp:574), copies the payload
	// into a block at least as long as the 24-byte struct (:580-584), and chooses what to fill in by the block's own first number, not
	// by the payload's size: under 16 the sky is 200,200,200 (:586-591), and under 24 the kit is Pistol, Plasma Caster, Disc and
	// Medicomp (0x17) with 30 spears (:592-603); then it writes the block's real size over that first number (:605). A payload under
	// 4 bytes leaves the number unset, because the block isn't cleared. The editor refuses a payload under 4 bytes, and a data_size
	// that isn't the payload's size. (Added 2026-11-14 by Melanikus (Vasquez), M7.)
	static Field const EnvironmentFields[] =
	{
		{ "data_size", "", S, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, EnvironmentReader },
		{ "sky_colour_red", "Red", S, 4, Storage::Int32, Unit::Plain, 0, Limit::Editor, 0, 255, SkyWhy, false, 0, CenconEnvironment },
		{ "sky_colour_green", "Green", S, 8, Storage::Int32, Unit::Plain, 0, Limit::Editor, 0, 255, SkyWhy, false, 0, CenconEnvironment },
		{ "sky_colour_blue", "Blue", S, 12, Storage::Int32, Unit::Plain, 0, Limit::Editor, 0, 255, SkyWhy, false, 0, CenconEnvironment },
		{ "kit", "", S, 16, Storage::UInt32, Unit::Flags, KitBits, Limit::None, 0, 0, "", false, 0, "avp/win95/avp_chunk.hpp:269-277" },
		{ "predator_num_spears", "Spears", S, 20, Storage::Int32, Unit::Plain, 0, Limit::Editor, 0, 32767, SpearsWhy, false, 0, CenconEnvironment },
	};

	// GLOGENDC: the level's generator settings, in REBENVDT. 28 bytes: five settings and two spares. (Added 2026-11-14, M7.)
	static Field const GlobalGeneratorFields[] =
	{
		{ "EnemyGenerated", "Enemy Generated", S, 0, Storage::Int32, Unit::Choice, 0, Limit::Loader, 0, 1, EnemyWhy, false, 0,
		  "avp/win95/avp_chunk.cpp:379-386" },
		{ "MaxNPCSOnLevel", "Max NPCs Generated", S, 4, Storage::Int32, Unit::Plain, 0, Limit::Editor, 0, 255, MostNpcsWhy, false, 0, CenconGenerators },
		{ "NPCSPerMinute", "NPCs Generated Per Minute", S, 8, Storage::Int32, Unit::Plain, 0, Limit::Editor, 0, 255, PerMinuteWhy, false, 0,
		  CenconGenerators },
		{ "NPCAcceleration", "Change in Generation Rate Per Minute", S, 12, Storage::Int32, Unit::Plain, 0, Limit::Editor, 0, 255, ChangeWhy, false, 0,
		  CenconGenerators },
		{ "HiveStateChangeTime", "Hive State Change Time (seconds)", S, 16, Storage::Int32, Unit::Seconds, 0, Limit::Editor, 1, 4681, HiveWhy, false, 0,
		  CenconGenerators },
		{ "spare1", "", S, 20, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, GlobalGeneratorReader },
		{ "spare2", "", S, 24, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, GlobalGeneratorReader },
	};

	// ENVSDSCL: the level's scale, in REBENVDT. 8 bytes: one double, which the game multiplies its scale by (projload.cpp:358-369). The
	// loader refuses a scale outside 0.5-1 and one that isn't a real number (environment_chunk.cpp:161-169, told from its bits by
	// real_numbers.h), so this is the loader's own limit. (Added 2026-11-14, M7.)
	static Field const ScaleFields[] =
	{
		{ "scale", "Scale", S, 0, Storage::Float64, Unit::Plain, 0, Limit::Loader, 0.5, 1, ScaleWhy, false, 0,
		  "win95/environment_chunk.cpp:161-169" },
	};

	// MODFLAGS: a module's flags, in its MODULEDT. 8 bytes: the flag word and a spare. The flag word holds the named flags and the
	// message number. (Added 2026-11-14 by Melanikus (Vasquez), module flags.)
	static Field const ModuleFlagFields[] =
	{
		{ "Flags", "", S, 0, Storage::UInt32, Unit::Flags, ModuleFlagBits, Limit::None, 0, 0, "", false, 0, ModuleFlagScreen },
		{ "message", "Message", S, 0, Storage::UInt32, Unit::Plain, ModuleMessageBits, Limit::Editor, 0, ModuleMessageBits, ModuleMessageWhy,
		  false, 0, ModuleFlagScreen },
		{ "spare", "", S, 4, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_object.cpp:838-840" },
	};

	// FRGTYPDC: a fragment type's data, in its FRAGTYPE. The type's name, then 12 bytes: three spare numbers the game reads and never
	// uses. The numbers are counted from the end of the name. (Added 2026-11-14 by Melanikus (Vasquez), L1.)
	static Field const FragmentTypeFields[] =
	{
		{ "pad1", "", S, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_fragments.cpp:50" },
		{ "pad2", "", S, 4, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_fragments.cpp:51" },
		{ "pad3", "", S, 8, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_fragments.cpp:52" },
	};

	// FRGTYPSC: one shape of a fragment type, in its FRAGTYPE. The shape's name, then 28 bytes: how many pieces of that shape, a
	// location, and three spare numbers. The numbers are counted from the end of the name. The game reads the location, checks it as
	// every place is checked, and never uses it (chunk_load.cpp:178-180 puts every piece at 0). (Added 2026-11-14, L1.)
	static Field const FragmentShapeFields[] =
	{
		{ "num_fragments", "Quantity", S, 0, Storage::Int32, Unit::Plain, 0, Limit::GameMeaning, 0, FragmentPiecesMost, FragmentPiecesWhy, false, 0,
		  "win95/chunk_fragments.cpp:108; avp/bh_debris.cpp:777; cencon's dialog 142, \"Quantity\" (vasquez-tools.md)" },
		{ "location", "", S, 4, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -PositionMost, PositionMost, PositionWhy, false, 0,
		  FragmentPlaceSource },
		{ "pad1", "", S, 16, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, "win95/chunk_fragments.cpp:97, :112" },
		{ "pad2", "", S, 20, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, "win95/chunk_fragments.cpp:97, :113" },
		{ "pad3", "", S, 24, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, "win95/chunk_fragments.cpp:97, :114" },
	};

	// FRGSOUND: the sound a fragment type, or a shape with its own pieces, makes when it breaks. 20 bytes of numbers, then its wav file's
	// name (name_model.h). A new one starts as the chunk library makes it: 5000 and 40000 for its reach, volume 127, no pitch.
	// (Added 2026-11-14 by Melanikus (Vasquez), L1.)
	static Field const FragmentSoundFields[] =
	{
		{ "inner_range", "Inner range", S, 0, Storage::UInt32, Unit::GameUnits, 0, Limit::Editor, 0, PositionMost, DistanceWhy, true, 5000,
		  FragmentRangeSource },
		{ "outer_range", "Outer range", S, 4, Storage::UInt32, Unit::GameUnits, 0, Limit::Editor, 0, PositionMost, DistanceWhy, true, 40000,
		  FragmentRangeSource },
		{ "max_volume", "Volume", S, 8, Storage::Int32, Unit::Plain, 0, Limit::GameMeaning, 0, 127, VolumeWhy, true, 127,
		  "avp/bh_debris.cpp:759; avp/psnd.cpp:407-408; win95/chunk_fragments.cpp:163, :177" },
		{ "pitch", "Pitch", S, 12, Storage::Int32, Unit::Pitch128, 0, Limit::GameMeaning, -6144, 6144, PitchWhy, true, 0,
		  "avp/bh_debris.cpp:759; avp/psnd.cpp:414-415; win95/chunk_fragments.cpp:164, :179" },
		{ "pad", "", S, 16, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, "win95/chunk_fragments.cpp:165, :181" },
	};

	// RANTEXID: a set of random textures, in REBENVDT, which cencon edited (vasquez-tools.md: dialog 141, "Available random texturings").
	// The set's name, then a count and that many whole numbers, then two spare numbers the game reads and never uses. The game reads only
	// two names, "hnpcmarine" and "hnpc_civvie" (avp/win95/projload.cpp:2841-2848), and keeps the numbers as the list of shape set
	// collections that random marines or civilians may be given: it only asks whether a collection's own number is in the list
	// (projload.cpp:830-840, :924), so every whole number is safe. The count is counted from the end of the name, and the spares from the
	// end of the items. (Added 2026-11-14 by Melanikus (Vasquez), L1.)
	static Field const RandomTextureFields[] =
	{
		{ "spare1", "", AfterList, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, "win95/environment_chunk.cpp:272, :296" },
		{ "spare2", "", AfterList, 4, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, "win95/environment_chunk.cpp:272, :297" },
	};

	// GMODHEAD: a game mode's header, in its GAMEMODE (inside REBENVDT). 20 bytes: the flags, three reserved numbers and a version number,
	// then the mode's name, then a list of the .rif files it comes from, each name ending in a zero, up to an empty name or the end of the
	// chunk (the names are in name_model.h). The game uses only the name: it picks the mode whose name is the one it was started with
	// (chunk_load.cpp:1329-1343). The editor's flags say whether the level defines the mode itself (Editable) or includes it from another
	// .rif; the other two are marks of its game mode dialog, and every one of the game's own 14 modes has just Editable.
	// (Added 2026-11-14 by Melanikus (Vasquez), L1.)
	static Field const GameModeHeaderFields[] =
	{
		{ "flags", "", S, 0, Storage::UInt32, Unit::Flags, GameModeBits, Limit::None, 0, 0, "", false, 0,
		  "win95/chunk_palette.cpp:594; chunk_palette.hpp:490-492" },
		{ "reserved1", "", S, 4, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_palette.cpp:598-601" },
		{ "reserved2", "", S, 8, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_palette.cpp:598-601" },
		{ "reserved3", "", S, 12, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_palette.cpp:598-601" },
		{ "version_num", "", S, 16, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_palette.cpp:603" },
	};

	// OBJHEAD1: an object's header, inside the object (parent RBOBJECT). 60 bytes of numbers, then the object's name (63 letters at most:
	// name_kinds.cpp, "object name"; the lock user's 16 bytes at +4 are a name too, "lock user"). The game reads a chunk shorter than 60
	// bytes as if it were padded with zeros (chunk_object.cpp:402-407), so the game registers no smallest size and needs no name: the name is
	// cut to what fits (chunk_object_name.hpp). The editor is stricter: it reads and sets a number only where the chunk holds it, so its
	// smallest size here is the 60 bytes (every header in the game's own levels has them). The place and the turn have the loader's limits
	// (CheckPlace, CheckTurn). The old editor
	// drew none of these numbers in a box, so none has a label: its prompts took a place ("Type in the new coordinates"). (Added 2026-11-15 by
	// Melanikus (Frost), S9.)
	static Field const ObjectHeaderFields[] =
	{
		{ "flags", "", S, 0, Storage::UInt32, Unit::Flags, ObjectHeaderBits, Limit::None, 0, 0, "", false, 0, "win95/chunk_object.cpp:409-416; chunk_object.hpp:18-36" },
		{ "location", "", S, 20, Storage::IntTriple, Unit::GameUnits, 0, Limit::Loader, -PositionMost, PositionMost, PositionWhy, false, 0, "win95/chunk_object.cpp:430-432; chunk.hpp:488-510" },
		{ "orientation_x", "", S, 32, Storage::Float32, Unit::Plain, 0, Limit::Loader, -2, 2, QuaternionWhy, false, 0, ObjectHeaderTurn },
		{ "orientation_y", "", S, 36, Storage::Float32, Unit::Plain, 0, Limit::Loader, -2, 2, QuaternionWhy, false, 0, ObjectHeaderTurn },
		{ "orientation_z", "", S, 40, Storage::Float32, Unit::Plain, 0, Limit::Loader, -2, 2, QuaternionWhy, false, 0, ObjectHeaderTurn },
		{ "orientation_w", "", S, 44, Storage::Float32, Unit::Plain, 0, Limit::Loader, -2, 2, QuaternionWhy, false, 0, ObjectHeaderTurn },
		{ "index_num", "", S, 48, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_object.cpp:438" },
		{ "version_no", "", S, 52, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_object.cpp:441, :517-520 (the game adds 1 when it writes the chunk)" },
		{ "shape_id_no", "", S, 56, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, "win95/chunk_object.cpp:444" },
	};

	// id, label, parent, leastBytes, fixedBytes, listCountAt, listItemBytes, fields, fieldCount, reader, namesRequired, namesOptional,
	// sizeFieldAt, nameFirst, trailerBytes, namesToEmpty (each left out when false or 0)
	ChunkKind const ChunkKinds[] =
	{
		{ "STDLIGHT", "light", "", 84, 84, -1, 0, LightFields, sizeof(LightFields) / sizeof(LightFields[0]), "win95/chunk_light.cpp:72, :131-164", 0, 0, -1 },
		{ "PLOBJLIT", "placed light", "", 60, 64, 60, 4, PlacedLightFields, sizeof(PlacedLightFields) / sizeof(PlacedLightFields[0]),
		  "win95/chunk_light.cpp:368, :370-407", 0, 0, -1 },
		{ "SOUNDOB2", "placed sound", "", 40, 40, -1, 0, SoundFields, sizeof(SoundFields) / sizeof(SoundFields[0]),
		  "win95/chunk_sound.cpp:21, :50-76", 1, 1, -1 },
		{ "TRAKSOUN", "track sound", "", 24, 24, -1, 0, TrackSoundFields, sizeof(TrackSoundFields) / sizeof(TrackSoundFields[0]),
		  "win95/chunk_object.cpp:1067, :1081-1107", 1, 0, -1 },
		{ "AVPGENER", "generator", "SPECLOBJ", 28, 28, -1, 0, GeneratorFields, sizeof(GeneratorFields) / sizeof(GeneratorFields[0]),
		  "avp/win95/avp_chunk.cpp:28, :75-105", 1, 0, -1 },
		{ "GENEXSET", "generator settings", "", 24, 84, -1, 0, GeneratorSettingsFields,
		  sizeof(GeneratorSettingsFields) / sizeof(GeneratorSettingsFields[0]), "avp/win95/avp_chunk.cpp:256, :262-321", 0, 0, 20 },
		{ "PARGENDA", "particle generator", "", 68, 68, -1, 0, ParticleFields, sizeof(ParticleFields) / sizeof(ParticleFields[0]),
		  "avp/win95/avp_chunk.cpp:795, :810-841", 0, 1, -1 },
		{ "AVPSTART", "player start", "", 56, 56, -1, 0, StartFields, sizeof(StartFields) / sizeof(StartFields[0]),
		  "avp/win95/avp_chunk.cpp:436, :449-456", 0, 0, -1 },
		{ "AVPENVIR", "environment settings", "REBENVDT", 4, 24, -1, 0, EnvironmentFields,
		  sizeof(EnvironmentFields) / sizeof(EnvironmentFields[0]), "avp/win95/avp_chunk.cpp:574, :577-606", 0, 0, 0 },
		{ "GLOGENDC", "generator settings for the level", "REBENVDT", 28, 28, -1, 0, GlobalGeneratorFields,
		  sizeof(GlobalGeneratorFields) / sizeof(GlobalGeneratorFields[0]), "avp/win95/avp_chunk.cpp:363, :376-401", 0, 0, -1 },
		{ "ENVSDSCL", "scale", "REBENVDT", 8, 8, -1, 0, ScaleFields, sizeof(ScaleFields) / sizeof(ScaleFields[0]),
		  "win95/environment_chunk.cpp:148, :157-170, environment_chunk.hpp:92-103", 0, 0, -1 },
		{ "MODFLAGS", "module flags", "MODULEDT", 8, 8, -1, 0, ModuleFlagFields, sizeof(ModuleFlagFields) / sizeof(ModuleFlagFields[0]),
		  "win95/chunk_object.cpp:820, :835-846", 0, 0, -1 },
		// A fragment type's data and a shape of it start with their name, so their numbers are counted from its end. They're read only
		// inside the type (FRAGTYPE, in REBENVDT): the game finds them through it (chunk_fragments.cpp:28-37, chunk_load.cpp:158).
		{ "FRGTYPDC", "fragment type", "FRAGTYPE", 0, 12, -1, 0, FragmentTypeFields, sizeof(FragmentTypeFields) / sizeof(FragmentTypeFields[0]),
		  "win95/chunk_fragments.cpp:40, :42-53", 0, 0, -1, true },
		{ "FRGTYPSC", "fragment shape", "FRAGTYPE", 0, 28, -1, 0, FragmentShapeFields, sizeof(FragmentShapeFields) / sizeof(FragmentShapeFields[0]),
		  "win95/chunk_fragments.cpp:88, :100-115", 0, 0, -1, true },
		// A fragment sound is read in two places, inside a fragment type (chunk_load.cpp:190) and inside a shape's own fragments, SHPFRAGS
		// (chunk_load.cpp:1075), so it's read under no one parent.
		{ "FRGSOUND", "fragment sound", "", 20, 20, -1, 0, FragmentSoundFields, sizeof(FragmentSoundFields) / sizeof(FragmentSoundFields[0]),
		  "win95/chunk_fragments.cpp:155, :168-186", 1, 0, -1 },
		// A set of random textures starts with its name, then holds a list whose count is the first number after it, then two numbers
		{ "RANTEXID", "random textures", "REBENVDT", 0, 4, 0, 4, RandomTextureFields, sizeof(RandomTextureFields) / sizeof(RandomTextureFields[0]),
		  "win95/environment_chunk.cpp:265, :276-298", 0, 0, -1, true, 8 },
		// A game mode's header is read only inside its game mode (it writes itself into its parent, chunk_palette.cpp:586, :626); it holds
		// its mode's name, and then a list of names
		{ "GMODHEAD", "game mode header", "GAMEMODE", 20, 20, -1, 0, GameModeHeaderFields, sizeof(GameModeHeaderFields) / sizeof(GameModeHeaderFields[0]),
		  "win95/chunk_palette.cpp:588, :590-627", 1, 0, -1, false, 0, true },
		// The level's sound folder holds a name and no numbers, so it has no fields: the kind says only that the name has to end inside the
		// chunk. The game finds it through the environment data (projload.cpp:505, :1486, :1932) and puts it and a "/" in front of every
		// sound file it opens for the level (object_setup.cpp:140-144; the name's own limits are in name_model.h). cencon sets it in its
		// dialog 150, "Set sound directory" (vasquez-tools.md).
		{ "SOUNDDIR", "sound folder", "REBENVDT", 0, 0, -1, 0, 0, 0, "win95/environment_chunk.cpp:232, :241-245", 1, 0, -1 },
		{ "OBJHEAD1", "object header", "RBOBJECT", 60, 60, -1, 0, ObjectHeaderFields, sizeof(ObjectHeaderFields) / sizeof(ObjectHeaderFields[0]),
		  ObjectHeaderReader, 0, 0, -1 },
	};

	size_t const ChunkKindCount = sizeof(ChunkKinds) / sizeof(ChunkKinds[0]);

	ChunkBitName const ChunkBits[] =
	{
		{ "STDLIGHT", "engine_light_flags", 0x1, "Atten" },
		{ "STDLIGHT", "engine_light_flags", 0x2, "Spread atten" },
		{ "STDLIGHT", "engine_light_flags", 0x4, "Omni" },
		{ "STDLIGHT", "engine_light_flags", 0x800, "No specular" },
		{ "STDLIGHT", "local_light_flags", 0x2, "Shadows" },
	{ "STDLIGHT", "local_light_flags", 0x10, "Runtime" },
		{ "STDLIGHT", "local_light_flags", 0x40, "No prelight" },
		{ "PLOBJLIT", "engine_light_flags", 0x1, "Atten" },
		{ "PLOBJLIT", "engine_light_flags", 0x2, "Spread atten" },
		{ "PLOBJLIT", "engine_light_flags", 0x4, "Omni" },
		{ "PLOBJLIT", "engine_light_flags", 0x10, "No shadows" },
		{ "PLOBJLIT", "engine_light_flags", 0x800, "No specular" },
		{ "PLOBJLIT", "local_light_flags", 0x10, "Runtime" },
		{ "PLOBJLIT", "local_light_flags", 0x40, "No prelight" },
		{ "PLOBJLIT", "flags", 0x1, "Start on" },
		{ "PLOBJLIT", "flags", 0x2, "Use down colour for off" },
		{ "PLOBJLIT", "flags", 0x4, "No specular" },
		{ "SOUNDOB2", "flags", 0x1, "Not playing at start" },
		{ "SOUNDOB2", "flags", 0x2, "Doesn't loop" },
		{ "TRAKSOUN", "flags", 0x1, "Loops" },
		{ "AVPGENER", "flags", 0x800, "In the marine game" },
		{ "AVPGENER", "flags", 0x1000, "In the alien game" },
		{ "AVPGENER", "flags", 0x2000, "In the predator game" },
		{ "AVPGENER", "flags", 0x100000, "Not in difficulty 1" },
		{ "AVPGENER", "flags", 0x200000, "Not in difficulty 2" },
		{ "AVPGENER", "flags", 0x400000, "Not in difficulty 3" },
		{ "AVPGENER", "flags", 0x2000000, "Use own max NPC setting" },
		{ "AVPGENER", "flags", 0x4000000, "Use own generation rate" },
		{ "AVPGENER", "flags", 0x8000000, "Advanced generator" },
		{ "AVPGENER", "flags", 0x40000000, "Multiplayer start" },
		{ "AVPGENER", "flags", 0x80000000, "Starts inactive" },
		{ "PARGENDA", "flags", 0x1, "Inactive" },
		// cencon's words (vasquez-tools.md, dialog 143): "Predator stuff" Pistol to Medicomp, "Marine stuff" Jetpack, then Stars
		{ "AVPENVIR", "kit", 0x1, "Pistol" },
		{ "AVPENVIR", "kit", 0x2, "Plasma Caster" },
		{ "AVPENVIR", "kit", 0x4, "Disc" },
		{ "AVPENVIR", "kit", 0x8, "Grappling hook" },
		{ "AVPENVIR", "kit", 0x10, "Medicomp" },
		{ "AVPENVIR", "kit", 0x20, "Jetpack" },
		{ "AVPENVIR", "kit", 0x40, "Stars" },
		// inteng's words for the module flags (raw/inteng-strings.txt 2884-2945). Two more of its flags, "Not a Room" and "Draw Back
		// Colour", are offered there too, but which bits they are isn't found yet, so they're not named here.
		{ "MODFLAGS", "Flags", 0x80000000, "Airduct" },
		{ "MODFLAGS", "Flags", 0x40000000, "Stair" },
		{ "MODFLAGS", "Flags", 0x20000000, "Sky Visible" },
		{ "MODFLAGS", "Flags", 0x10000000, "Fogged" },
		{ "MODFLAGS", "Flags", 0x00100000, "Not Passable" },
		{ "MODFLAGS", "Flags", 0x00080000, "Not a Doorway" },
		{ "MODFLAGS", "Flags", 0x00040000, "Doorway" },
		{ "MODFLAGS", "Flags", 0x00020000, "Sneaky" },
		{ "MODFLAGS", "Flags", 0x00010000, "Victory Zone" },
		{ "MODFLAGS", "Flags", 0x00000200, "Exclude From Map" },
		// The game's own words for a game mode's flags (chunk_palette.hpp:490-492)
		{ "GMODHEAD", "flags", 0x1, "Editable" },
		{ "GMODHEAD", "flags", 0x2, "Deleted" },
		{ "GMODHEAD", "flags", 0x4, "Added" },
		// An object's header: the same bits and words as a generator's (they have to correspond: chunk_object.hpp:22-36)
		{ "OBJHEAD1", "flags", 0x800, "In the marine game" },
		{ "OBJHEAD1", "flags", 0x1000, "In the alien game" },
		{ "OBJHEAD1", "flags", 0x2000, "In the predator game" },
		{ "OBJHEAD1", "flags", 0x100000, "Not in difficulty 1" },
		{ "OBJHEAD1", "flags", 0x200000, "Not in difficulty 2" },
		{ "OBJHEAD1", "flags", 0x400000, "Not in difficulty 3" },
	};

	size_t const ChunkBitCount = sizeof(ChunkBits) / sizeof(ChunkBits[0]);

	ChunkChoiceName const ChunkChoices[] =
	{
		{ "PLOBJLIT", "light_type", 0, "Standard" },
		{ "PLOBJLIT", "light_type", 1, "Strobe" },
		{ "PLOBJLIT", "light_type", 2, "Flicker" },
		{ "PLOBJLIT", "on_off_type", 0, "Standard" },
		{ "PLOBJLIT", "on_off_type", 1, "Fade" },
		{ "PLOBJLIT", "on_off_type", 2, "Flicker" },
		{ "TRAKSOUN", "index", 0, "While moving" },
		{ "TRAKSOUN", "index", 1, "At the start" },
		{ "TRAKSOUN", "index", 2, "At the end" },
		{ "AVPGENER", "type", 0, "Generator" },
		{ "AVPGENER", "type", 1, "Alien" },
		{ "AVPGENER", "type", 2, "Predator" },
		{ "AVPGENER", "type", 3, "Facehugger" },
		{ "AVPGENER", "type", 4, "Xenoborg" },
		{ "AVPGENER", "type", 5, "Predalien" },
		{ "AVPGENER", "type", 6, "Queen" },
		{ "AVPGENER", "type", 7, "Marine" },
		{ "AVPGENER", "type", 8, "Praetorian" },
		{ "AVPGENER", "type", 9, "Sentry gun" },
		{ "PARGENDA", "type", 0, "Spark" },
		{ "PARGENDA", "type", 1, "Steam" },
		{ "PARGENDA", "type", 2, "Black smoke" },
		{ "PARGENDA", "type", 3, "Flame" },
		{ "GLOGENDC", "EnemyGenerated", 0, "Aliens" },   // cencon's own combo words (vasquez-tools.md, dialog 139)
		{ "GLOGENDC", "EnemyGenerated", 1, "Marines" },
	};

	size_t const ChunkChoiceCount = sizeof(ChunkChoices) / sizeof(ChunkChoices[0]);

	ChunkKind const *FindChunkKind(char const *id)
	{
		for (size_t i = 0; i < ChunkKindCount; i++)
		{
			if (strncmp(ChunkKinds[i].id, id, 8) == 0)
			{
				return &ChunkKinds[i];
			}
		}

		return 0;
	}

	Field const *FindChunkField(ChunkKind const &kind, char const *name)
	{
		for (size_t i = 0; i < kind.fieldCount; i++)
		{
			if (strcmp(kind.fields[i].name, name) == 0)
			{
				return &kind.fields[i];
			}
		}

		return 0;
	}

	bool ChunkChoiceIsNamed(char const *chunk, char const *field, int32_t value)
	{
		for (size_t i = 0; i < ChunkChoiceCount; i++)
		{
			if (strncmp(ChunkChoices[i].chunk, chunk, 8) == 0 && strcmp(ChunkChoices[i].field, field) == 0 && ChunkChoices[i].value == value)
			{
				return true;
			}
		}

		return false;
	}
}
