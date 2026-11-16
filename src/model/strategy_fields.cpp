// The strategy tables: every kind of behaviour an object can be given (the AVPSTRAT chunk), its fields, and the names of their bits
// and choices. Written 2026-11-14 by Melanikus for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own
// code". field_model.h says what each column means.
//
// THE LAYOUT of every kind is the game's own reader at the pinned AvPx commit: 3dc/avp/win95/strategy_chunk.cpp and .hpp, each kind's
// constructor and its GetStrategySize. A strategy's payload starts with two numbers of the chunk's own (index and spare2), then the
// strategy: its kind, a Type and an ExtraData that every kind has, and then the kind's own fields, from payload +20.
//
// THE CREATION VALUES are what Rebellion's own editor (inteng, 2000) gave a new strategy of each kind. They come from a study of that
// editor's program in 2026; it was never run. Where a field has no creation value listed, the old editor left it at 0.

#include "field_model.h"

#include <limits.h>

#include <cstring>

namespace model
{
	namespace
	{
		char const Reader[] = "the game's reader (strategy_chunk.cpp)";
		char const ReaderAndEditor[] = "the game's reader (strategy_chunk.cpp); the creation value is the old editor's";

		double const IntMax = INT_MAX;

		// Positions and box corners. The game keeps everything in a level within this many game units of the centre.
		double const FarthestPlace = 16777216.0;   // 2 to the power 24
	}

	Field const StrategyBaseFields[] =
	{
		{ "index", "", Anchor::Start, 0, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0,
		  "", false, 0, "the game's reader (strategy_chunk.cpp, AVP_Strategy_Chunk): matches a light's strategy to its light" },
		{ "spare2", "", Anchor::Start, 4, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0, Reader },
		{ "StrategyType", "", Anchor::Start, 8, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0,
		  "", false, 0, "the game's reader: the kind's number, which says how the rest is read" },
		{ "Type", "", Anchor::Start, 12, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0,
		  "", true, 0, "the game's reader (AvpStrat); a new one starts at 0, as the game's own AvpStrat(int) does" },
		{ "ExtraData", "", Anchor::Start, 16, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0,
		  "", true, 0, "the game's reader (AvpStrat); a new one starts at 0, as the game's own AvpStrat(int) does" },
	};
	size_t const StrategyBaseFieldCount = sizeof(StrategyBaseFields) / sizeof(StrategyBaseFields[0]);

	namespace
	{
		// 50: a door that opens and closes.
		Field const DoorFields[] =
		{
			{ "DoorFlags", "", Anchor::Start, 20, Storage::UInt32, Unit::Flags, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "TimeToOpen", "Time take to open (0.1 - 25 sec)", Anchor::Start, 24, Storage::Byte, Unit::TenthsOfASecond, 0, Limit::Editor,
			  1, 255, "the old editor takes 0.1 to 25.5 seconds here, 1 to 255 tenths (FUN_0052cbb0: the typed seconds times 10 must be at least 1 and below 255.5); 0, the game's own speed, is typed only by leaving it", true, 10, "the game's reader; 0 keeps the game's own speed (object_setup.cpp, add_door); creation value the old editor's" },
			{ "TimeToClose", "Time take to close (0.1 - 25 sec)", Anchor::Start, 25, Storage::Byte, Unit::TenthsOfASecond, 0,
			  Limit::Editor, 1, 255, "the old editor takes 0.1 to 25.5 seconds here, 1 to 255 tenths (FUN_0052cbb0: the typed seconds times 10 must be at least 1 and below 255.5); 0, the game's own speed, is typed only by leaving it", true, 5, "the game's reader; 0 keeps the game's own speed; creation value the old editor's" },
			{ "spare", "", Anchor::Start, 26, Storage::Int16, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 51: a lift with stops on several floors. Airlock (57) and LiftNoTel (60) are made as this kind, with a flag set.
		Field const LiftFields[] =
		{
			{ "NumAssocLifts", "", Anchor::Start, 20, Storage::Int32, Unit::ListCount, 0, Limit::None, 0, 0,
			  "", true, 0, "the game's reader: how many linked lifts follow, 8 bytes each" },
			{ "AssocDoor", "Door :", Anchor::AfterFirstList, 0, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0,
			  "", true, 0, ReaderAndEditor },
			{ "AssocCallSwitch", "Call Switch :", Anchor::AfterFirstList, 8, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0,
			  "", true, 0, ReaderAndEditor },
			{ "AssocFloorSwitch", "Floor Switch :", Anchor::AfterFirstList, 16, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0,
			  "", true, 0, ReaderAndEditor },
			{ "LiftFlags", "", Anchor::AfterFirstList, 24, Storage::UInt32, Unit::Flags, 0, Limit::None, 0, 0, "", true, 0,
			  ReaderAndEditor },
			{ "Floor", "Floor %d", Anchor::AfterFirstList, 28, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0,
			  "", true, -1, "the game's reader; a new lift starts at -1, the old editor's value" },
			{ "NumExternalLifts", "", Anchor::AfterFirstList, 32, Storage::Int32, Unit::ListCount, 0, Limit::None, 0, 0,
			  "", true, 0, "the game's reader: how many lifts elsewhere follow, 28 bytes each" },
			{ "Facing", "Facing %d", Anchor::End, 4, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 52: a switch with two states.
		Field const BinSwitchFields[] =
		{
			{ "flags", "", Anchor::Start, 20, Storage::UInt32, Unit::Flags, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "Mode", "Mode", Anchor::Start, 24, Storage::Int32, Unit::Choice, 0, Limit::Editor, 0, 5,
			  "a switch's mode is one of the five the old editor offered", true, 1, ReaderAndEditor },
			{ "Time", "seconds", Anchor::Start, 28, Storage::Int32, Unit::Fixed16Seconds, 0, Limit::Editor, 0, 1966080000,
			  "a switch's time is 0 to 30000 seconds (FUN_0052e7f0: the typed seconds are refused below 0 or above 30000, then times 65536)", true, 0, ReaderAndEditor },
			{ "Security", "Security :", Anchor::Start, 32, Storage::Int32, Unit::Plain, 0, Limit::Editor, 0, IntMax,
			  "a switch's security level can't be negative", true, 0, ReaderAndEditor },
			{ "Target", "Switch's target", Anchor::Start, 36, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0,
			  ReaderAndEditor },
			{ "request", "", Anchor::Start, 44, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 61: a two-state switch that's one of a set of linked switches.
		Field const LinkSwitchFields[] =
		{
			{ "flags", "", Anchor::Start, 20, Storage::UInt32, Unit::Flags, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "Mode", "Mode", Anchor::Start, 24, Storage::Int32, Unit::Choice, 0, Limit::Editor, 0, 5,
			  "a switch's mode is one of the five the old editor offered", true, 1, ReaderAndEditor },
			{ "Time", "seconds", Anchor::Start, 28, Storage::Int32, Unit::Fixed16Seconds, 0, Limit::Editor, 0, 1966080000,
			  "a switch's time is 0 to 30000 seconds (FUN_0052e7f0: the typed seconds are refused below 0 or above 30000, then times 65536)", true, 0, ReaderAndEditor },
			{ "Security", "Security :", Anchor::Start, 32, Storage::Int32, Unit::Plain, 0, Limit::Editor, 0, IntMax,
			  "a switch's security level can't be negative", true, 0, ReaderAndEditor },
			{ "Target", "Switch's target", Anchor::Start, 36, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0,
			  ReaderAndEditor },
			{ "request", "", Anchor::Start, 44, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "NumLinks", "Part of link switch set", Anchor::Start, 48, Storage::Int32, Unit::ListCount, 0, Limit::None, 0, 0,
			  "", true, 0, "the game's reader: how many linked switches follow, 8 bytes each" },
		};

		// 54: a platform lift.
		Field const PlatLiftFields[] =
		{
			{ "flags", "", Anchor::Start, 20, Storage::UInt32, Unit::Flags, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 59: a door worked by a switch. The old editor also kept a "Switch Operated" bit and two times in spare1, which the game
		// ignores (object_setup.cpp, add_switch_door reads only the door), so the editor keeps spare1 as it was.
		Field const SwitchDoorFields[] =
		{
			{ "AssocDoor", "Associated door", Anchor::Start, 20, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0,
			  ReaderAndEditor },
			// inteng (FUN_0052d060) keeps the door's two times in this word's top bits: open in bits 20-25, close in bits 26-31, each 0.0 to 6.3 seconds in tenths (0 to 63)
			{ "TimeToOpen", "Time take to open (0.0 - 6.3 sec)", Anchor::Start, 28, Storage::UInt32, Unit::TenthsOfASecond, 0x03F00000, Limit::Editor, 0, 63,
			  "the old editor takes 0.0 to 6.3 seconds here, 0 to 63 tenths", true, 0, "inteng FUN_0052d060: the word at 28, bits 20-25", 0, 20 },
			{ "TimeToClose", "Time take to close (0.0 - 6.3 sec)", Anchor::Start, 28, Storage::UInt32, Unit::TenthsOfASecond, 0xFC000000, Limit::Editor, 0, 63,
			  "the old editor takes 0.0 to 6.3 seconds here, 0 to 63 tenths", true, 0, "inteng FUN_0052d060: the word at 28, bits 26-31", 0, 26 },
			// inteng's "Switch Operated" box (FUN_0052d060): bit 0 of the same word; the game ignores it
			{ "SwitchOperated", "Switch Operated", Anchor::Start, 28, Storage::UInt32, Unit::Flags, 0x00000001, Limit::None, 0, 0, "", true, 0, "inteng FUN_0052d060: the word at 28, bit 0" },
			{ "spare1", "", Anchor::Start, 28, Storage::UInt32, Unit::Plain, 0x000FFFFE, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "spare2", "", Anchor::Start, 32, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 62: a placed object: scenery, furniture, a pickup, a video screen or a light.
		Field const SimpleObjectFields[] =
		{
			{ "Type", "Type of Object", Anchor::Start, 20, Storage::Int32, Unit::Choice, 0, Limit::Editor, 0, 16,
			  "an object's type is one the game knows (0-15), or 16, the old editor's placed light", true, 0, ReaderAndEditor },
			{ "ExtraData", "Extra data", Anchor::Start, 24, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0,
			  "", true, 0, "the game's reader: the weapon or ammo kind, for those two types (avp/equipment.h)" },
			{ "mass", "Mass (x100g)", Anchor::Start, 28, Storage::Int32, Unit::HundredGrams, 0, Limit::Editor, 0, 65535,
			  "an object's mass is 0 to 65535 hundred grams, as the old editor allowed", true, 50, ReaderAndEditor },
			{ "integrity", "Integrity", Anchor::Start, 32, Storage::UInt32, Unit::Plain, 0xFF, Limit::GameMeaning, 0, 255,
			  "the game reads only the lowest 8 bits of an object's integrity; above 20 it can't be destroyed", true, 2,
			  ReaderAndEditor },
			{ "flags", "", Anchor::Start, 36, Storage::UInt32, Unit::Flags, 0x3, Limit::None, 0, 0, "", true, 1, ReaderAndEditor },
			{ "explosion", "Explosion", Anchor::Start, 36, Storage::UInt32, Unit::Choice, 0x30, Limit::Editor, 0, 0x30,
			  "an object's explosion is none, small, big or molotov", true, 0, ReaderAndEditor },
			{ "target_request", "Turn Target On / Off", Anchor::Start, 40, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0,
			  "", true, 0, ReaderAndEditor },
			{ "targetID", "Event Target", Anchor::Start, 44, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0,
			  ReaderAndEditor },
		};

		// 63: a console.
		Field const ConsoleFields[] =
		{
			{ "ConsoleNum", "", Anchor::Start, 20, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", false, 0,
			  "the game's reader; the old editor works out a new one's number itself" },
			{ "spare1", "", Anchor::Start, 24, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "spare2", "", Anchor::Start, 28, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 64: a light effect for a part of the level (LightFXData).
		Field const LightingFields[] =
		{
			{ "type", "Light Types", Anchor::Start, 20, Storage::UInt32, Unit::Choice, 0, Limit::Editor, 0, 3,
			  "a light effect is one of the four kinds", true, 0, ReaderAndEditor },
			{ "init_state", "", Anchor::Start, 24, Storage::UInt32, Unit::Plain, 0, Limit::Editor, 0, 1,
			  "a light effect starts in one of its two states", true, 0, ReaderAndEditor },
			{ "fade_up_speed", "Up: Fading (sec)", Anchor::Start, 28, Storage::UInt32, Unit::Fixed16Seconds, 0, Limit::Editor, 0,
			  IntMax, "a light effect's time is a positive number of seconds", true, 0, ReaderAndEditor },
			{ "fade_down_speed", "Down: Fading (sec)", Anchor::Start, 32, Storage::UInt32, Unit::Fixed16Seconds, 0, Limit::Editor, 0,
			  IntMax, "a light effect's time is a positive number of seconds", true, 0, ReaderAndEditor },
			{ "post_fade_up_delay", "Up: After Fading (sec)", Anchor::Start, 36, Storage::UInt32, Unit::Fixed16Seconds, 0,
			  Limit::Editor, 0, IntMax, "a light effect's time is a positive number of seconds", true, 0, ReaderAndEditor },
			{ "post_fade_down_delay", "Down: After Fading (sec)", Anchor::Start, 40, Storage::UInt32, Unit::Fixed16Seconds, 0,
			  Limit::Editor, 0, IntMax, "a light effect's time is a positive number of seconds", true, 0, ReaderAndEditor },
			{ "pad1", "", Anchor::Start, 44, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "pad2", "", Anchor::Start, 48, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "pad3", "", Anchor::Start, 52, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 66: a switch with many targets.
		Field const MultiSwitchFields[] =
		{
			{ "RestState", "Rest State", Anchor::Start, 20, Storage::Int32, Unit::Choice, 0, Limit::Editor, 0, 1,
			  "a switch rests off or on", true, 0, ReaderAndEditor },
			{ "Mode", "Mode", Anchor::Start, 24, Storage::Int32, Unit::Choice, 0, Limit::Editor, 0, 5,
			  "a switch's mode is one of the five the old editor offered", true, 1, ReaderAndEditor },
			{ "Time", "seconds", Anchor::Start, 28, Storage::Int32, Unit::Fixed16Seconds, 0, Limit::Editor, 0, 1966080000,
			  "a switch's time is 0 to 30000 seconds (FUN_0052e7f0: the typed seconds are refused below 0 or above 30000, then times 65536)", true, 0, ReaderAndEditor },
			{ "Security", "Security :", Anchor::Start, 32, Storage::Int32, Unit::Plain, 0, Limit::Editor, 0, IntMax,
			  "a switch's security level can't be negative", true, 0, ReaderAndEditor },
			{ "NumTargets", "Targets", Anchor::Start, 36, Storage::Int32, Unit::ListCount, 0, Limit::None, 0, 0,
			  "", true, 0, "the game's reader: how many targets follow, 12 bytes each (an object and its request)" },
			{ "NumLinks", "", Anchor::AfterFirstList, 0, Storage::Int32, Unit::ListCount, 0, Limit::None, 0, 0,
			  "", true, 0, "the game's reader: how many linked switches follow, 8 bytes each" },
			{ "pad1", "", Anchor::End, 8, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "flags", "", Anchor::End, 4, Storage::UInt32, Unit::Flags, 0, Limit::None, 0, 0, "", true, 0x80000000u,
			  ReaderAndEditor },
		};

		// 68: a many-target switch worked by walking into a box.
		Field const AreaSwitchFields[] =
		{
			{ "RestState", "Rest State", Anchor::Start, 20, Storage::Int32, Unit::Choice, 0, Limit::Editor, 0, 1,
			  "a switch rests off or on", true, 0, ReaderAndEditor },
			{ "Mode", "Mode", Anchor::Start, 24, Storage::Int32, Unit::Choice, 0, Limit::Editor, 0, 5,
			  "a switch's mode is one of the five the old editor offered", true, 1, ReaderAndEditor },
			{ "Time", "seconds", Anchor::Start, 28, Storage::Int32, Unit::Fixed16Seconds, 0, Limit::Editor, 0, 1966080000,
			  "a switch's time is 0 to 30000 seconds (FUN_0052e7f0: the typed seconds are refused below 0 or above 30000, then times 65536)", true, 0, ReaderAndEditor },
			{ "Security", "Security :", Anchor::Start, 32, Storage::Int32, Unit::Plain, 0, Limit::Editor, 0, IntMax,
			  "a switch's security level can't be negative", true, 0, ReaderAndEditor },
			{ "NumTargets", "Targets", Anchor::Start, 36, Storage::Int32, Unit::ListCount, 0, Limit::None, 0, 0,
			  "", true, 0, "the game's reader: how many targets follow, 12 bytes each" },
			{ "NumLinks", "", Anchor::AfterFirstList, 0, Storage::Int32, Unit::ListCount, 0, Limit::None, 0, 0,
			  "", true, 0, "the game's reader: how many linked switches follow, 8 bytes each" },
			{ "pad1", "", Anchor::End, 40, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "flags", "", Anchor::End, 36, Storage::UInt32, Unit::Flags, 0, Limit::None, 0, 0, "", true, 0x80000000u,
			  ReaderAndEditor },
			{ "trigger_min", "Set volume to module extents", Anchor::End, 32, Storage::IntTriple, Unit::GameUnits, 0,
			  Limit::LoaderPlanned, -FarthestPlace, FarthestPlace,
			  "a box's corner is within 16,777,216 game units of the level's centre", true, 0, ReaderAndEditor },
			{ "trigger_max", "", Anchor::End, 20, Storage::IntTriple, Unit::GameUnits, 0, Limit::LoaderPlanned, -FarthestPlace,
			  FarthestPlace, "a box's corner is within 16,777,216 game units of the level's centre", true, 0, ReaderAndEditor },
			{ "area_pad1", "", Anchor::End, 8, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "area_pad2", "", Anchor::End, 4, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 67: a teleport.
		Field const TeleportFields[] =
		{
			{ "TeleportTo", "", Anchor::Start, 20, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0,
			  ReaderAndEditor },
			{ "Type", "", Anchor::Start, 28, Storage::Int32, Unit::Choice, 0, Limit::Editor, 0, 3,
			  "a teleport is for everyone, or for one of the three species", true, 0, ReaderAndEditor },
			{ "spare1", "", Anchor::Start, 32, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "spare2", "", Anchor::Start, 36, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 69: what an enemy placed in the level does.
		Field const EnemyFields[] =
		{
			{ "MissionType", "Mission", Anchor::Start, 20, Storage::Int32, Unit::Choice, 0, Limit::Editor, 0, 5,
			  "an enemy's mission is one of the six (a predator uses only 0 and 1)", true, 0,
			  "the game's reader; object_setup.cpp reads marines' 0-5 and predators' 0-1; creation value the old editor's" },
			{ "request", "Turn Death Target On/Off", Anchor::Start, 24, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0,
			  "", true, 1, ReaderAndEditor },
			{ "DeathTarget", "Death target", Anchor::Start, 28, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0,
			  ReaderAndEditor },
			{ "ExtraMissionData", "Path - %d", Anchor::Start, 36, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0,
			  "", true, 0, "the game's reader: for a path follower, the path's number and, above it, the step to start at" },
			{ "spare1", "", Anchor::Start, 40, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "spare2", "", Anchor::Start, 44, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "spare3", "", Anchor::Start, 48, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 70: one of the level's mission objectives.
		Field const MissionObjectiveFields[] =
		{
			{ "mission_description_string", "Mission Description", Anchor::Start, 20, Storage::Int32, Unit::TextNumber, 0,
			  Limit::Loader, 0, 450, "a level's text message is numbered 0 to 450 (0 is none)", true, 0, ReaderAndEditor },
			{ "mission_complete_string", "Completed Message", Anchor::Start, 24, Storage::Int32, Unit::TextNumber, 0, Limit::Loader,
			  0, 450, "a level's text message is numbered 0 to 450 (0 is none)", true, 0, ReaderAndEditor },
			{ "mission_number", "Mission Order Number", Anchor::Start, 28, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0,
			  "", true, 0, ReaderAndEditor },
			{ "flags", "", Anchor::Start, 32, Storage::UInt32, Unit::Flags, 0, Limit::None, 0, 0, "", true, 3, ReaderAndEditor },
			{ "mission_completion_effect", "Special", Anchor::Start, 36, Storage::Int32, Unit::Choice, 0, Limit::Editor, 0, 1,
			  "completing an objective does nothing more, or completes the level", true, 0, ReaderAndEditor },
			{ "num_mission_targets", "Mission alterations upon completion", Anchor::Start, 40, Storage::Int32, Unit::ListCount, 0,
			  Limit::None, 0, 0, "", true, 0, "the game's reader: how many objectives it alters follow, 12 bytes each" },
			{ "num_request_targets", "", Anchor::AfterFirstList, 0, Storage::Int32, Unit::ListCount, 0, Limit::None, 0, 0,
			  "", true, 0, "the game's reader: how many request targets follow, 12 bytes each" },
			{ "pad1", "", Anchor::End, 8, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "pad2", "", Anchor::End, 4, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 72: an object moving along a track, and 76: an animated hierarchy. Both hold a list of point effects.
		Field const TrackFields[] =
		{
			{ "num_point_effects", "Point Effects", Anchor::Start, 20, Storage::Int32, Unit::ListCount, 0, Limit::None, 0, 0,
			  "", true, 0, "the game's reader: how many point effects follow, each 12 bytes and 16 more per target" },
			{ "pad1", "", Anchor::End, 8, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "pad2", "", Anchor::End, 4, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 80: a track object that can be destroyed.
		Field const TrackDestructFields[] =
		{
			{ "num_point_effects", "Point Effects", Anchor::Start, 20, Storage::Int32, Unit::ListCount, 0, Limit::None, 0, 0,
			  "", true, 0, "the game's reader: how many point effects follow, each 12 bytes and 16 more per target" },
			{ "pad1", "", Anchor::End, 24, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "pad2", "", Anchor::End, 20, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "integrity", "Integrity", Anchor::End, 16, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0,
			  ReaderAndEditor },
			{ "target_request", "", Anchor::End, 12, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0,
			  ReaderAndEditor },
			{ "targetID", "Destruct Target", Anchor::End, 8, Storage::ObjectId, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0,
			  ReaderAndEditor },
		};

		// 73: a text message shown to the player.
		Field const MessageFields[] =
		{
			{ "message_string", "String :%d", Anchor::Start, 20, Storage::Int32, Unit::TextNumber, 0, Limit::Loader, 0, 450,
			  "a level's text message is numbered 0 to 450 (0 is none)", true, 0, ReaderAndEditor },
			{ "flags", "", Anchor::Start, 24, Storage::UInt32, Unit::Flags, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "pad2", "", Anchor::Start, 28, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 75: a fan, which spins and can blow the player along.
		Field const FanFields[] =
		{
			{ "speed_up_time", "Speed up time (seconds)", Anchor::Start, 20, Storage::Int32, Unit::Seconds, 0,
			  Limit::Editor, 1, 999, "the old editor takes a whole number of seconds from 1 to 999 here (FUN_005356b0: other typed values are ignored); the file keeps it times 65536", true, 10,
			  ReaderAndEditor, 0, 16 },
			{ "slow_down_time", "Slow down time (seconds)", Anchor::Start, 24, Storage::Int32, Unit::Seconds, 0,
			  Limit::Editor, 1, 999, "the old editor takes a whole number of seconds from 1 to 999 here (FUN_005356b0: other typed values are ignored); the file keeps it times 65536", true, 10,
			  ReaderAndEditor, 0, 16 },
			// A limit for typing only. 34 of the 95 fans in the game's own levels are outside it, and the game plays any such wind as 100%
			// (object_setup.cpp:1317-1320), so a check of a level being loaded must never refuse it with ValueIsAllowed.
			{ "fan_wind_strength", "Wind strength (-1000 to 1000)", Anchor::Start, 28, Storage::Int32, Unit::Percent, 0,
			  Limit::GameMeaning, -1000, 1000, "a fan's wind is -1000 to 1000 percent; the game reads anything else as 100",
			  true, 100, "the game's reader; object_setup.cpp reads only -1000 to 1000; creation value the old editor's" },
			{ "pad2", "", Anchor::Start, 32, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 77: a box that hurts or kills whoever is in it ("Volume of Death").
		Field const DeathVolumeFields[] =
		{
			{ "volume_min", "V : Edit Volume", Anchor::Start, 20, Storage::IntTriple, Unit::GameUnits, 0, Limit::LoaderPlanned,
			  -FarthestPlace, FarthestPlace, "a box's corner is within 16,777,216 game units of the level's centre", true, 0,
			  ReaderAndEditor },
			{ "volume_max", "", Anchor::Start, 32, Storage::IntTriple, Unit::GameUnits, 0, Limit::LoaderPlanned, -FarthestPlace,
			  FarthestPlace, "a box's corner is within 16,777,216 game units of the level's centre", true, 0, ReaderAndEditor },
			{ "flags", "", Anchor::Start, 44, Storage::UInt32, Unit::Flags, 0, Limit::None, 0, 0, "", true, 1, ReaderAndEditor },
			{ "damage", "Type in the damage per second. Type 0 for infinite damage", Anchor::Start, 48, Storage::Int32,
			  Unit::Plain, 0, Limit::Editor, 0, IntMax, "the damage per second can't be negative (0 means it kills at once)", true,
			  0, ReaderAndEditor },
			{ "pad2", "", Anchor::Start, 52, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 78: the level's self-destruct countdown.
		Field const SelfDestructFields[] =
		{
			{ "timer", "Time until destruction (seconds)", Anchor::Start, 20, Storage::Int32, Unit::Seconds, 0, Limit::Editor, 0,
			  32767, "the countdown is 0 to 32767 seconds: the game counts it in 16.16 fixed point", true, 60,
			  "the game's reader; object_setup.cpp shifts it into 16.16; creation value the old editor's" },
			{ "pad1", "", Anchor::Start, 24, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "pad2", "", Anchor::Start, 28, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "pad3", "", Anchor::Start, 32, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
			{ "pad4", "", Anchor::Start, 36, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0, "", true, 0, ReaderAndEditor },
		};

		// 79: what a generator's enemies do.
		Field const GeneratorFields[] =
		{
			{ "MissionType", "", Anchor::Start, 20, Storage::Int32, Unit::Choice, 0, Limit::Editor, 0, 1,
			  "a generator's enemies hunt, or follow a path", true, 0, ReaderAndEditor },
			{ "ExtraMissionData", "", Anchor::Start, 24, Storage::Int32, Unit::Plain, 0, Limit::None, 0, 0,
			  "", true, 0, "the game's reader: for a path follower, the path's number" },
		};
	}

#define FIELDS(table) table, sizeof(table) / sizeof(table[0])

	StrategyKind const StrategyKinds[] =
	{
		{ 50, "Door", "Door", SizeRule::Fixed, 28, FIELDS(DoorFields), true, true, 50 },
		{ 51, "Lift", "Lift", SizeRule::Lift, 64, FIELDS(LiftFields), true, false, 51 },
		{ 52, "BinSwitch", "Binary Switch", SizeRule::Fixed, 48, FIELDS(BinSwitchFields), true, true, 52 },
		{ 53, "SimpleObject", "", SizeRule::Fixed, 20, 0, 0, false, false, 53 },
		{ 54, "PlatLift", "Platform Lift", SizeRule::Fixed, 44, FIELDS(PlatLiftFields), true, true, 54 },
		{ 57, "Airlock", "Airlock", SizeRule::AnySize, 20, 0, 0, true, false, 51 },
		{ 59, "SwitchDoor", "Switch door", SizeRule::Fixed, 36, FIELDS(SwitchDoorFields), true, true, 59 },
		{ 60, "LiftNoTel", "Lift (switches don't teleport)", SizeRule::AnySize, 20, 0, 0, true, false, 51 },
		{ 61, "LinkSwitch", "Linked Switch", SizeRule::LinkSwitch, 52, FIELDS(LinkSwitchFields), true, true, 61 },
		{ 62, "NewSimpleObject", "Placed Object", SizeRule::Fixed, 52, FIELDS(SimpleObjectFields), true, true, 62 },
		{ 63, "Console", "Console", SizeRule::Fixed, 32, FIELDS(ConsoleFields), true, false, 63 },
		{ 64, "Lighting", "Light Object", SizeRule::Fixed, 56, FIELDS(LightingFields), true, true, 64 },
		{ 66, "MultiSwitch", "Multiple Target Switch", SizeRule::MultiSwitch, 52, FIELDS(MultiSwitchFields), true, true, 66 },
		{ 67, "Teleport", "Teleport", SizeRule::Fixed, 40, FIELDS(TeleportFields), true, false, 67 },
		{ 68, "AreaSwitch", "Area Switch", SizeRule::MultiSwitch, 84, FIELDS(AreaSwitchFields), true, true, 68 },
		{ 69, "Enemy", "Enemy", SizeRule::Fixed, 52, FIELDS(EnemyFields), true, true, 69 },
		{ 70, "MissionObjective", "Mission Objective", SizeRule::MissionObjective, 56, FIELDS(MissionObjectiveFields), true, true,
		  70 },
		{ 71, "MissionHint", "", SizeRule::AnySize, 20, 0, 0, false, false, 71 },
		{ 72, "Track", "Track", SizeRule::PointEffects, 32, FIELDS(TrackFields), true, true, 72 },
		{ 73, "Message", "Message", SizeRule::Fixed, 32, FIELDS(MessageFields), true, true, 73 },
		{ 75, "Fan", "Fan", SizeRule::Fixed, 36, FIELDS(FanFields), true, true, 75 },
		{ 76, "Hierarchy", "Hierarchy", SizeRule::PointEffects, 32, FIELDS(TrackFields), true, true, 76 },
		{ 77, "DeathVolume", "Volume of Death", SizeRule::Fixed, 56, FIELDS(DeathVolumeFields), true, true, 77 },
		{ 78, "SelfDestruct", "Self Destruct", SizeRule::Fixed, 40, FIELDS(SelfDestructFields), true, true, 78 },
		{ 79, "Generator", "Generator", SizeRule::Fixed, 52, FIELDS(GeneratorFields), true, true, 79 },
		{ 80, "TrackDestruct", "Track (Destructable)", SizeRule::PointEffects, 48, FIELDS(TrackDestructFields), true, true, 80 },

		// Rebellion's other games' kinds. The game reads each field by field and refuses one that isn't its exact size
		// (strategy_chunk.cpp:189-192), but no AvP level uses them and the editor edits none of their fields. Each size is the 8 bytes
		// before the kind's numbers, AvpStrat's 12 (strategy_chunk.cpp:374-377), and the kind's own (its GetStrategySize, the line given).
		{ 1000, "SwingDoor", "", SizeRule::Fixed, 56, 0, 0, false, false, 1000 },           // strategy_chunk.cpp:1817
		{ 1001, "PlacedBomb", "", SizeRule::Fixed, 52, 0, 0, false, false, 1001 },          // :1859
		{ 1002, "R6Switch", "", SizeRule::Fixed, 52, 0, 0, false, false, 1002 },            // :1899
		{ 1003, "R6SimpleObject", "", SizeRule::Fixed, 84, 0, 0, false, false, 1003 },      // :537, with SimpleStrategy's :457
		{ 2000, "MummyInanimate", "", SizeRule::Fixed, 64, 0, 0, false, false, 2000 },      // :1960
		{ 2001, "MummyPickup", "", SizeRule::Fixed, 44, 0, 0, false, false, 2001 },         // :2015
		{ 2002, "MummySimple", "", SizeRule::Fixed, 20, 0, 0, false, false, 2002 },         // read as a plain AvpStrat, as 53 is (strategy_chunk.cpp:84-86 at 491deb22): exactly 20
		{ 2003, "MummyTriggerVolume", "", SizeRule::Fixed, 68, 0, 0, false, false, 2003 },  // :2050
		{ 2004, "MummyPivotObject_Old", "", SizeRule::Fixed, 68, 0, 0, false, false, 2004 }, // :2102, the same reader as 2007
		{ 2005, "MummyChest", "", SizeRule::Fixed, 40, 0, 0, false, false, 2005 },          // :2138
		{ 2006, "MummyAlterCameraRange", "", SizeRule::Fixed, 72, 0, 0, false, false, 2006 }, // :2177
		{ 2007, "MummyPivotObject", "", SizeRule::Fixed, 68, 0, 0, false, false, 2007 },    // :2102
	};
	size_t const StrategyKindCount = sizeof(StrategyKinds) / sizeof(StrategyKinds[0]);

#undef FIELDS

	// The kinds above are the ones the old editor offered, the game's own levels hold, or the game reads field by field (Rebellion's
	// other games' kinds, sized but not edited). Any other kind is read as a block of any size.

	// The bits' names are the game's own flag names (strategy_chunk.hpp) in words, with the old editor's wording where it had one.
	BitName const StrategyBits[] =
	{
		{ 50, "DoorFlags", 0x1, "Door is locked", false },
		{ 50, "DoorFlags", 0x2, "Proximity door", false },
		{ 50, "DoorFlags", 0x4, "Starts open", false },
		{ 50, "DoorFlags", 0x8, "Switch activated door ('lift door')", false },
		{ 50, "DoorFlags", 0x10, "Door is horizontal (ie. in floor/ceiling)", false },
		{ 51, "LiftFlags", 0x1, "Lift starts here", false },
		{ 51, "LiftFlags", 0x2, "Airlock", false },
		{ 51, "LiftFlags", 0x4, "Switches don't teleport", false },
		{ 51, "LiftFlags", 0x8, "Exit opposite side", false },
		{ 52, "flags", 0x1, "Rest state on", false },
		{ 52, "flags", 0x2, "Switch off message: same as on", false },
		{ 52, "flags", 0x4, "Switch off message: none", false },
		{ 61, "flags", 0x1, "Rest state on", false },
		{ 61, "flags", 0x2, "Switch off message: same as on", false },
		{ 61, "flags", 0x4, "Switch off message: none", false },
		{ 54, "flags", 0x1, "Starts disabled", false },
		{ 54, "flags", 0x2, "Becomes disabled after one use", false },
		{ 62, "flags", 0x1, "Tell the target when destroyed", false },
		{ 62, "flags", 0x2, "Tell the target when picked up", false },
		{ 66, "flags", 0x2, "Switch off message: same as on", false },
		{ 66, "flags", 0x4, "Switch off message: none", false },
		{ 66, "flags", 0x80000000u, "Targets' requests stored one by one", true },
		{ 68, "flags", 0x2, "Switch off message: same as on", false },
		{ 68, "flags", 0x4, "Switch off message: none", false },
		{ 68, "flags", 0x80000000u, "Targets' requests stored one by one", true },
		{ 70, "flags", 0x1, "Shown at start", false },
		{ 70, "flags", 0x2, "Possible at start", false },
		{ 73, "flags", 0x1, "Message starts deactivated", false },
		{ 77, "flags", 0x1, "Starts on", false },
		{ 77, "flags", 0x2, "Collision not required", false },
	};
	size_t const StrategyBitCount = sizeof(StrategyBits) / sizeof(StrategyBits[0]);

	ChoiceName const StrategyChoices[] =
	{
		// A switch's mode (bh_binary_switch.h, BSWITCH_MODE). 3 is the game's own, while a switch is moving, so it isn't offered.
		{ 52, "Mode", 0, "Timer" }, { 52, "Mode", 1, "Wait" }, { 52, "Mode", 2, "Toggle" },
		{ 52, "Mode", 4, "Time Delay" }, { 52, "Mode", 5, "Time Delay (Autoexec)" },
		{ 61, "Mode", 0, "Timer" }, { 61, "Mode", 1, "Wait" }, { 61, "Mode", 2, "Toggle" },
		{ 61, "Mode", 4, "Time Delay" }, { 61, "Mode", 5, "Time Delay (Autoexec)" },
		{ 66, "Mode", 0, "Timer" }, { 66, "Mode", 1, "Wait" }, { 66, "Mode", 2, "Toggle" },
		{ 66, "Mode", 4, "Time Delay" }, { 66, "Mode", 5, "Time Delay (Autoexec)" },
		{ 68, "Mode", 0, "Timer" }, { 68, "Mode", 1, "Wait" }, { 68, "Mode", 2, "Toggle" },
		{ 68, "Mode", 4, "Time Delay" }, { 68, "Mode", 5, "Time Delay (Autoexec)" },
		{ 66, "RestState", 0, "Off" }, { 66, "RestState", 1, "On" },
		{ 68, "RestState", 0, "Off" }, { 68, "RestState", 1, "On" },

		// An object's type (pvisible.h, INANIMATEOBJECT_TYPE). 16 isn't the game's: it's the old editor's value for a placed light
		// (the game makes an object a placed light by its light chunk, not by this number).
		{ 62, "Type", 0, "Static" }, { 62, "Type", 1, "Furniture" }, { 62, "Type", 2, "Weapon" }, { 62, "Type", 3, "Ammo" },
		{ 62, "Type", 4, "Health" }, { 62, "Type", 5, "Armour" }, { 62, "Type", 6, "Key" },
		{ 62, "Type", 7, "Boxed sentry gun" }, { 62, "Type", 8, "IR goggles" }, { 62, "Type", 9, "Video screen" },
		{ 62, "Type", 10, "Data tape" }, { 62, "Type", 11, "Motion tracker upgrade" }, { 62, "Type", 12, "Pheromone pod" },
		{ 62, "Type", 13, "Special pickup" }, { 62, "Type", 14, "Destroys the base when hit" },
		{ 62, "Type", 15, "Field charge" }, { 62, "Type", 16, "Placed light" },
		{ 62, "explosion", 0x00, "None" }, { 62, "explosion", 0x10, "Small" }, { 62, "explosion", 0x20, "Big" },
		{ 62, "explosion", 0x30, "Molotov" },

		// A light effect (ltfx_exp.h, LIGHT_FX_TYPE).
		{ 64, "type", 0, "Random flicker" }, { 64, "type", 1, "Strobe" }, { 64, "type", 2, "Switch" },
		{ 64, "type", 3, "Flicky switch" },

		// Who a teleport is for.
		{ 67, "Type", 0, "Everyone" }, { 67, "Type", 1, "Marine" }, { 67, "Type", 2, "Alien" }, { 67, "Type", 3, "Predator" },

		// An enemy's mission (object_setup.cpp, add_marine). A predator reads only 0 and 1, where 1 follows a path.
		{ 69, "MissionType", 0, "Wait then wander (default)" }, { 69, "MissionType", 1, "Wander" },
		{ 69, "MissionType", 2, "Guard" }, { 69, "MissionType", 3, "Noncombatant" },
		{ 69, "MissionType", 4, "Local Guard" }, { 69, "MissionType", 5, "Path follower" },

		{ 70, "mission_completion_effect", 0, "Nothing" }, { 70, "mission_completion_effect", 1, "Complete level" },

		{ 79, "MissionType", 0, "Hunt" }, { 79, "MissionType", 1, "Path follower" },
	};
	size_t const StrategyChoiceCount = sizeof(StrategyChoices) / sizeof(StrategyChoices[0]);

	bool ChoiceIsNamed(int32_t kind, char const *field, int32_t value)
	{
		for (size_t i = 0; i < StrategyChoiceCount; i++)
		{
			ChoiceName const &choice = StrategyChoices[i];

			if (choice.kind == kind && choice.value == value && std::strcmp(choice.field, field) == 0)
			{
				return true;
			}
		}

		return false;
	}

	StrategyKind const *FindStrategyKind(int32_t number)
	{
		for (size_t i = 0; i < StrategyKindCount; i++)
		{
			if (StrategyKinds[i].number == number)
			{
				return &StrategyKinds[i];
			}
		}

		return 0;
	}
}
