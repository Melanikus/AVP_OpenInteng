// hierarchy_view.cpp: see hierarchy_view.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Every call is the game's own, in the order the game's own code makes a placed hierarchy: projload.cpp LoadedPlacedHierarchy::load_rif (the file), bh_placed_hierarchy.cpp PlacedHierarchyBehaveInit
// (GetNamedHierarchyFromLibrary, Create_HModel, InitHModelSequence) and MakePlacedHierarchyNear (the display block), PlacedHierarchyBehaveFun (ProveHModel each frame).

#include "3dc.h"
#include "module.h"
#include "inline.h"
#include "strategy_def.h"
#include "gamedef.h"
#include "gameplat.h"
#include "bh_types.h"
#include "dynblock.h"
#include "chunk_load.hpp"
#include "mishchnk.hpp"
#include "chunk_object.hpp"
#include "environment_chunk.hpp"
#include "projload.hpp"
#include "hmodel.h"
#include "frame_time.h"
#include "renderer.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>

#include <stdio.h>
#include <string.h>
#include <string>
#include <map>
#include <vector>

#include "hierarchy_view.h"

extern int NormalFrameTime;                          // io.cpp:74: how long the last frame took, 65536 = one second
extern int GlobalFrameCounter;                       // io.cpp:93: the game's frame number; the model works its positions out once a frame
extern MODULEMAPBLOCK VisibilityDefaultObjectMap;    // pvisible.cpp, as bh_placed_hierarchy.cpp declares it
extern SECTION *GetNamedHierarchyFromLibrary(const char *rifName, const char *hierarchyName);   // projload.cpp:1733
extern HIERARCHY_SHAPE_REPLACEMENT *GetHierarchyAlternateShapeSetFromLibrary(const char *rifName, const char *shapeSetName);   // projload.cpp:1751

extern List<Global_Hierarchy_Store *> Global_Hierarchy_Library;   // projload.cpp:459

static HMODELCONTROLLER controller;
static bool turretOn = false;                 // inteng's turret sequence is playing on the sections under "dum turret"
static int turretSequence = 0, turretSub = 0, turretPosition = 0, turretElevation = 0;
static bool turretFollows = false;            // Y: the play and stop commands also act on the turret sequence
static void ApplyTheTurret();
static int selectedSection = -1;   // the selected section: a place in the list HierarchyView_Sections gives; -1 none
static DISPLAYBLOCK *shown = nullptr;
static std::map<std::string, std::string> libraryNameOf;   // lower-case file name -> the name the game's library keeps the model under (the file's own RIFFNAME: hnpcalien.rif is "HNPCalien")

static std::string LowerName(const char *name)
{
	std::string lower = name ? name : "";

	for (char &c : lower)
	{
		c = (char)tolower((unsigned char)c);
	}

	return lower;
}

static void Say(std::string *why, const char *what)
{
	if (why)
	{
		*why = what;
	}
}

bool HierarchyView_LoadModel(const char *modelName, std::string *why)
{
	// "avp_rifs/<file>": the OPENED file itself (inteng's hierarchies are those of the opened file); a plain name is a file of avp_huds. A file he double-clicked is the editor's copy in
	// avp_rifs\OpenedByEditor (open_with.h, kOpenedFolder): "avp_rifs/OpenedByEditor/<file>", that one folder and no other
	bool const ofTheLevel = modelName && strncmp(modelName, "avp_rifs/", 9) == 0;
	const char *plain = ofTheLevel ? modelName + 9 : modelName;

	if (ofTheLevel && _strnicmp(plain, "OpenedByEditor/", 15) == 0)
	{
		plain += 15;
	}

	if (!modelName || !modelName[0] || strlen(modelName) > 80 || !plain[0] || strpbrk(plain, "\\/:.*?\"<>|"))
	{
		Say(why, "the model's name is not a plain file name");
		return false;
	}

	std::string const lower = LowerName(modelName);

	if (libraryNameOf.count(lower))
	{
		return true;
	}

	char path[100];

	_snprintf_s(path, sizeof(path), _TRUNCATE, ofTheLevel ? "%s.rif" : "avp_huds/%s.rif", modelName);

	RIFFHANDLE const handle = avp_load_rif_non_env(path);

	if (handle == INVALID_RIFFHANDLE)
	{
		Say(why, "the game's loader could not open that model file in avp_huds");
		return false;
	}

	// The name the game's library will keep it under: the file's own RIFFNAME chunk (projload.cpp Global_Hierarchy_Store). A level that already loaded this model (the NPCs it uses) has it in the library; it is not loaded again.
	RIF_Name_Chunk *const nameChunk = handle->envd ? (RIF_Name_Chunk *)handle->envd->lookup_single_child("RIFFNAME") : nullptr;

	if (!nameChunk || !nameChunk->rif_name)
	{
		unload_rif(handle);
		Say(why, "that model file has no name of its own inside");
		return false;
	}

	std::string inside = nameChunk->rif_name;
	bool already = false;

	for (LIF<Global_Hierarchy_Store *> each(&Global_Hierarchy_Library); !each.done(); each.next())
	{
		already = already || _stricmp(each()->riffname, inside.c_str()) == 0;
	}

	// The library's entry is this very file's only when the file is named as its RIFFNAME is (hnpcalien.rif holds "HNPCalien"). Any other file with that RIFFNAME is a different file (Melanikus's MelPyth_hnpcmarine.rif
	// is "hnpcmarine", as the stock NPC file is, and the game's loader insists the stock marine's hierarchies are in a file of that name): it is loaded under a name of its own, so the level's own marine stays as it is.
	// The same when the library does NOT hold that name yet: a file opened by double-click is the level itself (no stock marine loaded before it), and a file whose RIFFNAME is a stock model's
	// (his ShotgunBot.RIF and MelPyth_hnpcmarine.RIF are "hnpcmarine") would be checked as that model by the game's loader (projload.cpp Stop_On_Bad_Model: "Error loading model \"marine with pulse
	// rifle\" from file hnpcmarine") and the game would stop. So any file not named as its RIFFNAME is loaded under its own plain name + "~hv" (no folder in it).
	if (_stricmp(plain, inside.c_str()) != 0)
	{
		std::string const own = std::string(plain) + "~hv";

		delete[] nameChunk->rif_name;
		nameChunk->rif_name = new char[own.size() + 1];
		memcpy(nameChunk->rif_name, own.c_str(), own.size() + 1);
		inside = own;
		already = false;
	}

	if (already)
	{
		unload_rif(handle);
	}
	else if (!copy_rif_data(handle, CCF_IMAGEGROUPSET + CCF_LOAD_AS_HIERARCHY_IF_EXISTS + CCF_DONT_INITIALISE_TEXTURES, 0, 0))
	{
		unload_rif(handle);
		Say(why, "the game's loader refused that model file's data");
		return false;
	}
	else
	{
		unload_rif(handle);   // the copied data stays, as the game does it
	}

	libraryNameOf[lower] = inside;
	return true;
}

bool HierarchyView_Names(const char *modelName, std::vector<std::string> *names)
{
	auto const library = libraryNameOf.find(LowerName(modelName));

	if (!names || library == libraryNameOf.end())
	{
		return false;
	}

	names->clear();

	for (LIF<Global_Hierarchy_Store *> each(&Global_Hierarchy_Library); !each.done(); each.next())
	{
		if (_stricmp(each()->riffname, library->second.c_str()) == 0)
		{
			for (LIF<Hierarchy_Descriptor *> inside(&each()->hierarchy_list); !inside.done(); inside.next())
			{
				names->push_back(inside()->hierarchy_name);
			}

			return true;
		}
	}

	return false;
}

void HierarchyView_Close()
{
	if (shown)
	{
		selectedSection = -1;
		R_SetEditorSelectedSection(nullptr);
		turretOn = false;
		R_SetEditorTurret(nullptr, 0, 0, 0, 0);
		Dispel_HModel(&controller);
		DestroyActiveObject(shown);
		shown = nullptr;
	}
}

bool HierarchyView_IsShown()
{
	return shown != nullptr;
}

static bool ShowWithTurn(const char *modelName, const char *hierarchyName, int sequence, int sub, int32_t seconds, const int32_t place[3], EULER turn, std::string *why);

static int showCount = 0;
static std::string shownLibrary;   // the game's library name of the model shown (libraryNameOf)

struct SectionOwnShape
{
	SECTION_DATA *section;
	SHAPEHEADER *shape;
	int shapeNum;
	int replacementId;
};

static std::vector<SectionOwnShape> ownShapes;   // the shown model's sections' own shapes, taken at the first ApplyShapeSets after a Show
static int ownShapesFor = -1;

int HierarchyView_ShowCount()
{
	return showCount;
}

bool HierarchyView_Show(const char *modelName, const char *hierarchyName, int sequence, int sub, int32_t seconds, const int32_t place[3], int32_t heading, std::string *why)
{
	EULER turn;

	turn.EulerX = 0;
	turn.EulerY = heading & 4095;
	turn.EulerZ = 0;
	return ShowWithTurn(modelName, hierarchyName, sequence, sub, seconds, place, turn, why);
}

bool HierarchyView_ShowPlaced(const char *modelName, const char *hierarchyName, int sequence, int sub, int32_t seconds, const int32_t place[3], const float quaternion[4], std::string *why)
{
	if (!quaternion)
	{
		Say(why, "no turn was given");
		return false;
	}

	// The game's own turn for a placed hierarchy (avpx object_setup.cpp, the placed hierarchy's template): the quaternion's x, y and z negated, made a matrix (QuatToMat) and then the
	// Euler angles (MatrixToEuler) the display block keeps
	QUAT q;
	MATRIXCH m;
	EULER turn;

	q.quatx = (int)-(quaternion[0] * ONE_FIXED);
	q.quaty = (int)-(quaternion[1] * ONE_FIXED);
	q.quatz = (int)-(quaternion[2] * ONE_FIXED);
	q.quatw = (int)(quaternion[3] * ONE_FIXED);
	QuatToMat(&q, &m);
	MatrixToEuler(&m, &turn);
	return ShowWithTurn(modelName, hierarchyName, sequence, sub, seconds, place, turn, why);
}

bool HierarchyView_FirstSequence(const char *modelName, const char *hierarchyName, int *sequence, int *sub, std::string *why)
{
	if (!sequence || !sub || !HierarchyView_LoadModel(modelName, why))
	{
		return false;
	}

	SECTION *const root = GetNamedHierarchyFromLibrary(libraryNameOf[LowerName(modelName)].c_str(), hierarchyName && hierarchyName[0] ? hierarchyName : "Template");

	if (!root || !root->sequence_array || root->num_sequences <= 0)
	{
		Say(why, "the hierarchy is not in the model file, or has no sequences");
		return false;
	}

	*sequence = root->sequence_array[0].sequence_id & 0xffff;
	*sub = root->sequence_array[0].sequence_id >> 16;
	return true;
}

int HierarchyView_KeyFrame()
{
	if (!shown || !controller.Root_Section)
	{
		return 0;
	}

	// FUN_004790a0: the key frame whose time range holds the player's timer, the last one when none does
	SEQUENCE *const sequence = GetSequencePointer(controller.Sequence_Type, controller.Sub_Sequence, controller.Root_Section);
	int index = 0;
	int count = 0;
	int at = 0;
	int found = -1;

	for (KEYFRAME_DATA *frame = sequence ? sequence->first_frame : nullptr; frame && count < 4096; frame = frame->last_frame ? nullptr : frame->Next_Frame)
	{
		int const to = at + frame->Sequence_Length;

		if (found < 0 && at <= controller.sequence_timer && controller.sequence_timer < to)
		{
			found = index;
		}

		at = to;
		index++;
		count++;
	}

	return found >= 0 ? found : (count > 0 ? count - 1 : 0);
}

static bool ShowWithTurn(const char *modelName, const char *hierarchyName, int sequence, int sub, int32_t seconds, const int32_t place[3], EULER turn, std::string *why)
{
	HierarchyView_Close();

	if (!place || !HierarchyView_LoadModel(modelName, why))
	{
		return false;
	}

	showCount++;
	ownShapes.clear();
	ownShapesFor = -1;
	shownLibrary = libraryNameOf[LowerName(modelName)];

	SECTION *const root = GetNamedHierarchyFromLibrary(libraryNameOf[LowerName(modelName)].c_str(), hierarchyName && hierarchyName[0] ? hierarchyName : "Template");

	if (!root)
	{
		std::string held = "the game's hierarchy library has no such model and hierarchy name; it holds:";

		for (LIF<Global_Hierarchy_Store *> each(&Global_Hierarchy_Library); !each.done(); each.next())
		{
			held += std::string(" ") + each()->riffname + "[";

			for (LIF<Hierarchy_Descriptor *> inside(&each()->hierarchy_list); !inside.done(); inside.next())
			{
				held += std::string(inside()->hierarchy_name) + ",";
			}

			held += "]";
		}

		if (why)
		{
			*why = held;
		}

		return false;
	}

	if (!root->sequence_array || root->num_sequences <= 0)
	{
		Say(why, "the hierarchy's first section has no sequences");
		return false;
	}

	bool exists = false;

	for (int s = 0; s < root->num_sequences; s++)
	{
		exists = exists || root->sequence_array[s].sequence_id == GetSequenceID(sequence, sub);
	}

	if (!exists)
	{
		std::string ids = "the hierarchy has no such sequence; its sequence ids (the game's number (type << 8 | sub) form) are:";

		for (int s = 0; s < root->num_sequences; s++)
		{
			ids += " " + std::to_string(root->sequence_array[s].sequence_id);
		}

		if (why)
		{
			*why = ids;
		}

		return false;
	}

	Create_HModel(&controller, root);
	InitHModelSequence(&controller, sequence, sub, seconds);

	// The display block, as the game's MakePlacedHierarchyNear makes it
	MODULE tempModule;

	VisibilityDefaultObjectMap.MapShape = 0;
	tempModule.m_mapptr = &VisibilityDefaultObjectMap;
	tempModule.m_sbptr = (STRATEGYBLOCK *)NULL;
	tempModule.m_numlights = 0;
	tempModule.m_lightarray = (struct lightblock *)0;
	tempModule.m_extraitemdata = (struct extraitemdata *)0;
	tempModule.m_dptr = NULL;
	tempModule.name = NULL;

	AllocateModuleObject(&tempModule);
	shown = tempModule.m_dptr;

	if (!shown)
	{
		Dispel_HModel(&controller);
		Say(why, "the game has no room for another display block");
		return false;
	}

	shown->ObStrategyBlock = nullptr;
	shown->ObMyModule = nullptr;
	shown->ObWorld.vx = place[0];
	shown->ObWorld.vy = place[1];
	shown->ObWorld.vz = place[2];
	shown->ObEuler = turn;
	CreateEulerMatrix(&shown->ObEuler, &shown->ObMat);
	TransposeMatrixCH(&shown->ObMat);
	shown->HModelControlBlock = &controller;
	controller.Playing = 0;
	controller.Looped = 1;
	GlobalFrameCounter++;
	ProveHModel(&controller, shown);
	return true;
}

void HierarchyView_Play(bool on, bool loop, bool all)
{
	if (shown)
	{
		// P (case 5, FUN_0047b8b0, 0x488ad6) sets the running switch (+0x28) of the main player and of the turret / delta players; O (case 6, FUN_0047b940) clears them. A player is made NOT running and looping
		// (FUN_0047d6f0: +0x28 = 0, +0x2c = 1), so choosing a sequence shows it standing still at position 0 until P.
		controller.Playing = on ? 1 : 0;
		controller.Looped = loop ? 1 : 0;

		for (DELTA_CONTROLLER *each = all ? controller.Deltas : nullptr; each; each = each->next_controller)
		{
			each->Playing = on ? 1 : 0;
		}
	}
}

static void ApplyTheTurret();

void HierarchyView_ScrubMain(int32_t step, bool all)
{
	if (!shown)
	{
		return;
	}

	if (!controller.Playing)
	{
		controller.sequence_timer = (controller.sequence_timer + step) & 0xffff;
	}

	// a stopped DELTA: inteng adds the step to its +0x28, the RUNNING word, not to its timer (FUN_0047b9c0 0x47baa2-0x47baaa, FUN_0047bb20 0x47bbfd): the first scrub frame makes it run, and it plays
	// from then on; its timer is only wrapped (0x47bae5). inteng's own quirk, kept as it is (Bishop's ruling 11-16, rule 10b)
	for (DELTA_CONTROLLER *each = all ? controller.Deltas : nullptr; each; each = each->next_controller)
	{
		if (!each->Playing && step != 0)
		{
			each->Playing = 1;
		}
	}

	// the turret only while it is stopped (FUN_0047b9c0 0x47ba03: [1] +0x28 == 0); ours plays it with the main when it follows (Y)
	if (all && turretOn && !(turretFollows && controller.Playing))
	{
		turretPosition = (turretPosition + step) & 0xffff;
		ApplyTheTurret();
	}
}

void HierarchyView_ScrubSecond(int32_t step, bool delta)
{
	if (!shown)
	{
		return;
	}

	if (delta && controller.Deltas)
	{
		controller.Deltas->timer = (controller.Deltas->timer + step) & 0xffff;
	}
	else if (turretOn)
	{
		turretPosition = (turretPosition + step) & 0xffff;
		ApplyTheTurret();
	}
}

bool HierarchyView_IsPlaying()
{
	return shown && controller.Playing;
}

void HierarchyView_SetPosition(int32_t position)
{
	if (shown)
	{
		controller.Playing = 0;
		controller.sequence_timer = position < 0 ? 0 : (position > ONE_FIXED - 1 ? ONE_FIXED - 1 : position);
	}
}

int32_t HierarchyView_Position()
{
	return shown ? controller.sequence_timer : 0;
}

// Ctrl+R and Ctrl+F on the shown model's frames too, as inteng's FUN_00480eb0 / FUN_004795d0 change the player's own frames with the chunks (so the change shows at once). The game keeps each frame's
// orientation as (w, -x, -y, -z) of the file's floats (projload.cpp: q.quatx = -orientation.x ...), the same as inteng's player (FUN_00480450 0x48056c-0x4805cb: x, y, z times -65536, w times
// 65536), so inteng's own change of the player's words applies here as it is: (q0, q1, q2, q3) -> (-q2, -q3, q0, q1). It keeps the angle between neighbouring frames, so omega and the slerp sign
// the loader worked out stand.
static int RootSequenceFrames(int sequence, int sub, KEYFRAME_DATA **frames, int most)
{
	if (!shown || !controller.Root_Section)
	{
		return 0;
	}

	SEQUENCE *const found = GetSequencePointer(sequence, sub, controller.Root_Section);
	int count = 0;

	for (KEYFRAME_DATA *frame = found ? found->first_frame : nullptr; frame && count < most; frame = frame->last_frame ? nullptr : frame->Next_Frame)
	{
		frames[count++] = frame;
	}

	return count;
}

bool HierarchyView_TurnRootSequence(int sequence, int sub)
{
	static KEYFRAME_DATA *frames[4096];
	int const count = RootSequenceFrames(sequence, sub, frames, 4096);

	for (int i = 0; i < count; i++)
	{
		VECTORCH offset;
		QUAT_SHORT const q = frames[i]->QOrient;

		GetKeyFrameOffset(frames[i], &offset);
		offset.vx = (int)(0u - (unsigned)offset.vx);
		offset.vz = (int)(0u - (unsigned)offset.vz);
		SetKeyFrameOffset(frames[i], &offset);
		frames[i]->QOrient.quatw = (short)-q.quaty;
		frames[i]->QOrient.quatx = (short)-q.quatz;
		frames[i]->QOrient.quaty = q.quatw;
		frames[i]->QOrient.quatz = q.quatx;
	}

	return count > 0;
}

bool HierarchyView_LowerRootSequence(int sequence, int sub, int by)
{
	static KEYFRAME_DATA *frames[4096];
	int const count = RootSequenceFrames(sequence, sub, frames, 4096);

	for (int i = 0; i < count; i++)
	{
		VECTORCH offset;

		GetKeyFrameOffset(frames[i], &offset);
		offset.vy = (int)((unsigned)offset.vy - (unsigned)by);
		SetKeyFrameOffset(frames[i], &offset);
	}

	return count > 0;
}

static int TurretSeconds();

void HierarchyView_StepFrame(int direction, bool all)
{
	if (!shown || !controller.Root_Section)
	{
		return;
	}

	int32_t const before = controller.sequence_timer;

	SEQUENCE *const sequence = GetSequencePointer(controller.Sequence_Type, controller.Sub_Sequence, controller.Root_Section);
	int starts[4096];
	int count = 0;
	int at = 0;

	for (KEYFRAME_DATA *frame = sequence ? sequence->first_frame : nullptr; frame && count < 4096; frame = frame->last_frame ? nullptr : frame->Next_Frame)
	{
		starts[count++] = at;
		at += frame->Sequence_Length;
	}

	if (count == 0)
	{
		return;
	}

	int current = 0;

	for (int i = 0; i < count; i++)
	{
		current = controller.sequence_timer >= starts[i] ? i : current;
	}

	current = (current + (direction >= 0 ? 1 : count - 1)) % count;

	// FUN_00480ff0 / FUN_00481040 write the main's timer (+0x10) and nothing else: a playing main plays on from the key frame's start, a stopped one stays stopped (HP-79)
	controller.sequence_timer = starts[current] < 0 ? 0 : (starts[current] > ONE_FIXED - 1 ? ONE_FIXED - 1 : starts[current]);

	// M (FUN_00479110 0x47914a) / N (FUN_00479290 0x4792ca) with Y's "Play commands affect all sequences" on (+0x88): the time the main moved, its timer's change (wrapped to 0..0xffff) times its
	// sequence's time (FUN_00499b30, MUL_FIXED), moves the turret player and every delta player on (M) or back (N) by that time in its own sequence's terms (FUN_005803e0, DIV_FIXED by its time),
	// each wrapped & 0xffff (0x479193 / 0x47924c); whether they run or not is left as it is (reassess/hierarchy-play-frost.md HP-78)
	if (!all)
	{
		return;
	}

	int32_t moved = direction >= 0 ? controller.sequence_timer - before : before - controller.sequence_timer;

	if (moved < 0)
	{
		moved += 0x10000;
	}

	long long const seconds = ((long long)moved * controller.Seconds_For_Sequence) >> 16;
	int const sign = direction >= 0 ? 1 : -1;

	// a time of 0 has no share to give (inteng would divide by it; a file's sequence time is hostile input)
	auto const share = [seconds](int ownSeconds) { return ownSeconds > 0 ? (int32_t)((seconds << 16) / ownSeconds) : 0; };

	if (turretOn)
	{
		turretPosition = (turretPosition + sign * share(TurretSeconds())) & 0xffff;
		ApplyTheTurret();
	}

	for (DELTA_CONTROLLER *each = controller.Deltas; each; each = each->next_controller)
	{
		each->timer = (each->timer + sign * share(each->seconds_for_sequence)) & 0xffff;
	}

	GlobalFrameCounter++;
	ProveHModel(&controller, shown);
}

static int deltasAdded = 0;

bool HierarchyView_AddDelta(int sequence, int sub)
{
	if (!shown)
	{
		return false;
	}

	bool found = false;

	for (int i = 0; i < controller.Root_Section->num_sequences; i++)
	{
		found = found || controller.Root_Section->sequence_array[i].sequence_id == ((sub << 16) + sequence);
	}

	if (!found)
	{
		return false;
	}

	char id[32];

	_snprintf_s(id, sizeof(id), _TRUNCATE, "delta %d", ++deltasAdded);

	DELTA_CONTROLLER *const added = Add_Delta_Sequence(&controller, id, sequence, sub, -1);

	added->Playing = 0;   // a new delta player is not running (FUN_0047d6f0); P starts it
	added->Looped = 1;
	added->Active = 1;
	return true;
}

bool HierarchyView_SetDeltaPlaying(bool on)
{
	if (!shown || !controller.Deltas)
	{
		return false;
	}

	controller.Deltas->Playing = on ? 1 : 0;   // the game's list has the last added first (HierarchyView_SetDeltaMilliseconds takes the same one)
	return true;
}

void HierarchyView_RemoveDeltas()
{
	if (shown)
	{
		RemoveAllDeltas(&controller);
	}
}

bool HierarchyView_SetDeltaMilliseconds(int milliseconds)
{
	if (!shown || !controller.Deltas || milliseconds < 1)
	{
		return false;
	}

	// the newest delta is the head of the game's list (Add_Delta_Sequence puts a new one first); the game works out timer_increment from the seconds as hmodel.cpp:3509 does
	DELTA_CONTROLLER *const delta = controller.Deltas;

	delta->seconds_for_sequence = (int)(((long long)milliseconds * 65536) / 1000);
	delta->timer_increment = delta->seconds_for_sequence ? DIV_FIXED(ONE_FIXED, delta->seconds_for_sequence) : 0;
	return true;
}

int HierarchyView_DeltaMilliseconds()
{
	if (!shown || !controller.Deltas)
	{
		return 0;
	}

	return (int)(((long long)controller.Deltas->seconds_for_sequence * 1000) / 65536);
}

int HierarchyView_DeltaCount()
{
	int count = 0;

	if (shown)
	{
		for (DELTA_CONTROLLER *each = controller.Deltas; each; each = each->next_controller)
		{
			count++;
		}
	}

	return count;
}

void HierarchyView_Frame(int milliseconds)
{
	GlobalFrameCounter++;
	NormalFrameTime = FrameTime_NeverNothing((int)(((long long)(milliseconds < 0 ? 0 : milliseconds) * 65536) / 1000));

	if (shown && turretOn && turretFollows && controller.Playing && controller.Root_Section)
	{
		SEQUENCE *const sequence = GetSequencePointer(turretSequence, turretSub, controller.Root_Section);
		int const seconds = sequence && sequence->Time > 0 ? sequence->Time : ONE_FIXED;

		turretPosition = (turretPosition + (int)(((long long)NormalFrameTime * 65536) / seconds)) & 0xffff;
		ApplyTheTurret();
	}

	if (shown)
	{
		ProveHModel(&controller, shown);
	}
}

bool HierarchyView_Sequences(std::vector<HierarchyViewSequence> *sequences)
{
	if (!sequences || !shown || !controller.Root_Section)
	{
		return false;
	}

	sequences->clear();

	for (int s = 0; s < controller.Root_Section->num_sequences; s++)
	{
		SEQUENCE const &one = controller.Root_Section->sequence_array[s];
		HierarchyViewSequence entry;
		int frames = 0;

		for (KEYFRAME_DATA *frame = one.first_frame; frame && frames < 100000; frame = frame->last_frame ? nullptr : frame->Next_Frame)
		{
			frames++;
		}

		entry.sequence = one.sequence_id & 0xffff;
		entry.sub = one.sequence_id >> 16;
		entry.frames = frames;
		entry.time = one.Time;
		sequences->push_back(entry);
	}

	return true;
}

bool HierarchyView_SetSequence(int sequence, int sub)
{
	if (!shown || !controller.Root_Section)
	{
		return false;
	}

	bool exists = false;

	for (int s = 0; s < controller.Root_Section->num_sequences; s++)
	{
		exists = exists || controller.Root_Section->sequence_array[s].sequence_id == GetSequenceID(sequence, sub);
	}

	if (!exists)
	{
		return false;
	}

	unsigned const playing = controller.Playing;
	unsigned const looped = controller.Looped;

	InitHModelSequence(&controller, sequence, sub, 0);
	controller.Playing = playing;
	controller.Looped = looped;
	return true;
}

bool HierarchyView_CurrentSequence(int *sequence, int *sub)
{
	if (!shown || !sequence || !sub)
	{
		return false;
	}

	*sequence = controller.Sequence_Type;
	*sub = controller.Sub_Sequence;
	return true;
}

void HierarchyView_ChangeTime(int milliseconds)
{
	if (!shown)
	{
		return;
	}

	// inteng's - and + step the sequence's time by 100 ms, to at least 100 ms (Vasquez, hierarchy-data-model.md: commands 0xd / 0xe); the game's own speed change keeps the position
	long long seconds = (long long)controller.Seconds_For_Sequence + ((long long)milliseconds * 65536) / 1000;
	long long const least = (100LL * 65536) / 1000;

	if (seconds < least)
	{
		seconds = least;
	}

	if (seconds > 600LL * 65536)
	{
		seconds = 600LL * 65536;
	}

	HModel_ChangeSpeed(&controller, (int)seconds);
}

void HierarchyView_SetSequenceSeconds(int32_t seconds)
{
	// A placed hierarchy's Play (inteng FUN_004e9f70: the player's +0x14 = the record's time in 16.16 seconds, unclamped) keeps the position, as the game's own speed change does
	if (shown && seconds > 0)
	{
		HModel_ChangeSpeed(&controller, seconds);
	}
}

int HierarchyView_SequenceMilliseconds()
{
	return shown ? (int)(((long long)controller.Seconds_For_Sequence * 1000) / 65536) : 0;
}

void HierarchyView_SetBackwards(bool backwards)
{
	if (shown)
	{
		controller.Reversed = backwards ? 1 : 0;
	}
}

// The selected section is drawn again in yellow by the renderer (avpx R_SetEditorSelectedSection); -1: none

static void WalkSections(SECTION_DATA *section, int parent, std::vector<SECTION_DATA *> *flat, std::vector<int> *parents)
{
	for (; section && flat->size() < 4096; section = section->Next_Sibling)
	{
		int const mine = (int)flat->size();

		flat->push_back(section);
		parents->push_back(parent);
		WalkSections(section->First_Child, mine, flat, parents);
	}
}

static void FlatSections(std::vector<SECTION_DATA *> *flat, std::vector<int> *parents)
{
	flat->clear();
	parents->clear();

	if (shown && controller.section_data)
	{
		WalkSections(controller.section_data, -1, flat, parents);
	}
}

int HierarchyView_ApplyShapeSets(const std::vector<std::string> &setNames)
{
	std::vector<SECTION_DATA *> flat;
	std::vector<int> parents;

	FlatSections(&flat, &parents);

	if (flat.empty())
	{
		return 0;
	}

	if (ownShapesFor != showCount)
	{
		ownShapes.clear();

		for (SECTION_DATA *section : flat)
		{
			ownShapes.push_back(SectionOwnShape{ section, section->Shape, section->ShapeNum, section->replacement_id });
		}

		ownShapesFor = showCount;
	}

	int changed = 0;

	for (SectionOwnShape const &own : ownShapes)
	{
		if (own.section->Shape != own.shape || own.section->ShapeNum != own.shapeNum)
		{
			own.section->Shape = own.shape;
			own.section->ShapeNum = own.shapeNum;
			own.section->replacement_id = own.replacementId;
			Setup_Texture_Animation_For_Section(own.section);
			changed++;
		}
	}

	for (std::string const &name : setNames)
	{
		HIERARCHY_SHAPE_REPLACEMENT *const replacements = GetHierarchyAlternateShapeSetFromLibrary(shownLibrary.c_str(), name.c_str());

		for (int a = 0; replacements && replacements[a].replaced_section_name && a < 4096; a++)
		{
			SECTION_DATA *const target = GetThisSectionData(controller.section_data, replacements[a].replaced_section_name);

			if (target)
			{
				target->Shape = replacements[a].replacement_shape;
				target->ShapeNum = replacements[a].replacement_shape_index;
				target->replacement_id = replacements[a].replacement_id;
				Setup_Texture_Animation_For_Section(target);
				changed++;
			}
		}
	}

	GlobalFrameCounter++;
	ProveHModel(&controller, shown);
	return changed;
}

bool HierarchyView_Sections(std::vector<HierarchyViewSection> *sections)
{
	std::vector<SECTION_DATA *> flat;
	std::vector<int> parents;

	if (!sections || !shown)
	{
		return false;
	}

	FlatSections(&flat, &parents);
	sections->clear();

	for (size_t i = 0; i < flat.size(); i++)
	{
		HierarchyViewSection entry;

		entry.name = flat[i]->sempai && flat[i]->sempai->Section_Name ? flat[i]->sempai->Section_Name : "";
		entry.shapeName = flat[i]->sempai && flat[i]->sempai->ShapeName ? flat[i]->sempai->ShapeName : "";
		entry.parent = parents[i];
		entry.world[0] = flat[i]->World_Offset.vx;
		entry.world[1] = flat[i]->World_Offset.vy;
		entry.world[2] = flat[i]->World_Offset.vz;
		entry.view[0] = flat[i]->View_Offset.vx;
		entry.view[1] = flat[i]->View_Offset.vy;
		entry.view[2] = flat[i]->View_Offset.vz;
		entry.radius = flat[i]->Shape ? flat[i]->Shape->shaperadius : 0;
		entry.hasShape = flat[i]->Shape != nullptr;
		sections->push_back(entry);
	}

	return true;
}

void HierarchyView_SelectSection(int index)
{
	std::vector<SECTION_DATA *> flat;
	std::vector<int> parents;

	FlatSections(&flat, &parents);
	selectedSection = index >= 0 && (size_t)index < flat.size() ? index : -1;
	R_SetEditorSelectedSection(selectedSection >= 0 ? flat[(size_t)selectedSection] : nullptr);
}

int HierarchyView_SelectedSection()
{
	return shown ? selectedSection : -1;
}

bool HierarchyView_PickSection(const int32_t from[3], const int32_t direction[3], int *index)
{
	std::vector<SECTION_DATA *> flat;
	std::vector<int> parents;

	if (!from || !direction || !index)
	{
		return false;
	}

	FlatSections(&flat, &parents);

	double const length = sqrt((double)direction[0] * direction[0] + (double)direction[1] * direction[1] + (double)direction[2] * direction[2]);

	if (flat.empty() || !(length > 0.0))
	{
		return false;
	}

	double const d[3] = { direction[0] / length, direction[1] / length, direction[2] / length };
	double bestT = 1e30;
	int best = -1;

	// the sections whose bounding sphere (the game's own test: PointInHModel's shaperadius about the section's place) the ray crosses; the nearest along the ray wins
	for (size_t i = 0; i < flat.size(); i++)
	{
		if (!flat[i]->Shape)
		{
			continue;
		}

		double const c[3] = { (double)flat[i]->World_Offset.vx - from[0], (double)flat[i]->World_Offset.vy - from[1], (double)flat[i]->World_Offset.vz - from[2] };
		double const t = c[0] * d[0] + c[1] * d[1] + c[2] * d[2];
		double const miss2 = c[0] * c[0] + c[1] * c[1] + c[2] * c[2] - t * t;
		double const radius = flat[i]->Shape->shaperadius;

		if (t > 0.0 && miss2 <= radius * radius && t < bestT)
		{
			bestT = t;
			best = (int)i;
		}
	}

	*index = best;
	return best >= 0;
}

bool HierarchyView_Extent(int32_t mins[3], int32_t maxs[3])
{
	std::vector<SECTION_DATA *> flat;
	std::vector<int> parents;

	FlatSections(&flat, &parents);

	if (flat.empty())
	{
		return false;
	}

	for (int a = 0; a < 3; a++)
	{
		mins[a] = INT32_MAX;
		maxs[a] = INT32_MIN;
	}

	for (SECTION_DATA *section : flat)
	{
		int32_t const at[3] = { section->World_Offset.vx, section->World_Offset.vy, section->World_Offset.vz };

		for (int a = 0; a < 3; a++)
		{
			mins[a] = at[a] < mins[a] ? at[a] : mins[a];
			maxs[a] = at[a] > maxs[a] ? at[a] : maxs[a];
		}
	}

	return true;
}

void HierarchyView_MoveTo(const int32_t place[3])
{
	if (shown && place)
	{
		shown->ObWorld.vx = place[0];
		shown->ObWorld.vy = place[1];
		shown->ObWorld.vz = place[2];
		GlobalFrameCounter++;
		ProveHModel(&controller, shown);
	}
}

// ---- THE TURRET (inteng's Add 'turret delta' 0x7, the keys K L R Y; Vasquez, hierarchy-data-model.md): the sections UNDER the section named "dum turret" play a second sequence, the section itself is turned by an elevation
// ---- (K / L: 10 degrees either way), R puts every position back to 0, and Y makes the play / stop commands act on the turret sequence too. NAMED: the turret's position is advanced by the view's own clock here
// ---- (the game's controller has no such second sequence); with Y off it stands still.
static SECTION_DATA *TurretSection()
{
	std::vector<SECTION_DATA *> flat;
	std::vector<int> parents;

	FlatSections(&flat, &parents);

	for (SECTION_DATA *section : flat)
	{
		if (section->sempai && section->sempai->Section_Name && _stricmp(section->sempai->Section_Name, getenv("AVP_HV_TURRET") ? getenv("AVP_HV_TURRET") : "dum turret") == 0)
		{
			return section;
		}
	}

	return nullptr;
}

bool HierarchyView_HasTurret()
{
	return shown && TurretSection() != nullptr;
}

static void ApplyTheTurret()
{
	R_SetEditorTurret(shown && turretOn ? TurretSection() : nullptr, turretSequence, turretSub, turretPosition, turretElevation);
}

bool HierarchyView_SetTurret(int sequence, int sub)
{
	if (!shown || !controller.Root_Section || !TurretSection())
	{
		return false;
	}

	bool exists = false;

	for (int s = 0; s < controller.Root_Section->num_sequences; s++)
	{
		exists = exists || controller.Root_Section->sequence_array[s].sequence_id == GetSequenceID(sequence, sub);
	}

	if (!exists)
	{
		return false;
	}

	turretOn = true;
	turretSequence = sequence;
	turretSub = sub;
	turretPosition = 0;
	ApplyTheTurret();
	GlobalFrameCounter++;
	ProveHModel(&controller, shown);
	return true;
}

void HierarchyView_RemoveTurret()
{
	turretOn = false;
	ApplyTheTurret();

	if (shown)
	{
		GlobalFrameCounter++;
		ProveHModel(&controller, shown);
	}
}

// the turret player's sequence time (16.16 seconds): its sequence's own, else one second (FUN_0047d6f0 0x47d803: +0x14 = 0x10000)
static int TurretSeconds()
{
	SEQUENCE *const sequence = controller.Root_Section ? GetSequencePointer(turretSequence, turretSub, controller.Root_Section) : nullptr;

	return sequence && sequence->Time > 0 ? sequence->Time : ONE_FIXED;
}

bool HierarchyView_TurretFacts(int *timer, int *milliseconds)
{
	if (!shown || !turretOn || !timer || !milliseconds || !controller.Root_Section)
	{
		return false;
	}

	int const seconds = TurretSeconds();

	*timer = turretPosition;
	*milliseconds = (int)(((long long)seconds * 1000) / 65536);
	return true;
}

int HierarchyView_DeltaFacts(int *timers, int *milliseconds, int most)
{
	int count = 0;

	if (!shown || !timers || !milliseconds)
	{
		return 0;
	}

	for (DELTA_CONTROLLER *each = controller.Deltas; each && count < most; each = each->next_controller)
	{
		timers[count] = each->timer & 0xffff;
		milliseconds[count] = (int)(((long long)each->seconds_for_sequence * 1000) / 65536);
		count++;
	}

	return count;
}

bool HierarchyView_TurretSequence(int *sequence, int *sub)
{
	if (!turretOn || !sequence || !sub)
	{
		return false;
	}

	*sequence = turretSequence;
	*sub = turretSub;
	return true;
}

void HierarchyView_TurnTurret(int degrees)
{
	turretElevation = ((turretElevation + degrees) % 360 + 360) % 360;
	ApplyTheTurret();

	if (shown)
	{
		GlobalFrameCounter++;
		ProveHModel(&controller, shown);
	}
}

int HierarchyView_Elevation()
{
	return turretElevation;
}

void HierarchyView_RestartAll()
{
	if (shown)
	{
		controller.sequence_timer = 0;
		turretPosition = 0;
		ApplyTheTurret();
		GlobalFrameCounter++;
		ProveHModel(&controller, shown);
	}
}

bool HierarchyView_ToggleTurretFollows()
{
	turretFollows = !turretFollows;
	return turretFollows;
}


// ---- detail levels (see hierarchy_view.h)
extern int GlobalLevelOfDetail_Hierarchical;
extern float CameraZoomScale;
extern VIEWDESCRIPTORBLOCK *Global_VDB_Ptr;
static int forcedDetail = -1;

static ADAPTIVE_DEGRADATION_DESC *FirstDegradationArray(int *count)
{
	std::vector<SECTION_DATA *> flat;
	std::vector<int> parents;

	FlatSections(&flat, &parents);

	for (SECTION_DATA *section : flat)
	{
		if (section->Shape && section->Shape->shape_degradation_array)
		{
			ADAPTIVE_DEGRADATION_DESC *entry = section->Shape->shape_degradation_array;
			int n = 1;

			// the last entry's distance is always zero (hmodel.cpp:1241)
			while (entry[n - 1].distance != 0 && n < 10)
			{
				n++;
			}

			*count = n;
			return section->Shape->shape_degradation_array;
		}
	}

	*count = 1;
	return nullptr;
}

int HierarchyView_DetailLevels()
{
	int n = 1;

	FirstDegradationArray(&n);
	return n;
}

void HierarchyView_SetDetailLevel(int level)
{
	forcedDetail = level;
}

int HierarchyView_DetailLevel()
{
	return forcedDetail;
}

bool HierarchyView_DetailDistance(int level, int *millimetres)
{
	int n = 1;
	ADAPTIVE_DEGRADATION_DESC *array = FirstDegradationArray(&n);

	if (!array || level < 0 || level >= n || !millimetres)
	{
		return false;
	}

	// the entries are in falling level: entry n - 1 is the full shape (level 0)
	*millimetres = array[n - 1 - level].distance;
	return true;
}

bool HierarchyView_SetDetailDistance(int level, int millimetres)
{
	std::vector<SECTION_DATA *> flat;
	std::vector<int> parents;
	bool any = false;

	FlatSections(&flat, &parents);

	for (SECTION_DATA *section : flat)
	{
		if (!section->Shape || !section->Shape->shape_degradation_array)
		{
			continue;
		}

		ADAPTIVE_DEGRADATION_DESC *entry = section->Shape->shape_degradation_array;
		int n = 1;

		while (entry[n - 1].distance != 0 && n < 10)
		{
			n++;
		}

		if (level >= 0 && level < n - 1)
		{
			entry[n - 1 - level].distance = millimetres;     // (level 0, the last entry, stays 0)
			any = true;
		}
	}

	return any;
}

int HierarchyView_DistanceFromModel()
{
	if (!shown || !Global_VDB_Ptr)
	{
		return 0;
	}

	VECTORCH view;

	MakeVector(&shown->ObWorld, &Global_VDB_Ptr->VDB_World, &view);
	RotateVector(&view, &Global_VDB_Ptr->VDB_Mat);
	return view.vz;
}

static bool inPlaceOn = false;
static int32_t inPlaceBase[3] = { 0, 0, 0 };

void HierarchyView_SetInPlace(bool on, const int32_t place[3])
{
	inPlaceOn = on;

	if (place)
	{
		inPlaceBase[0] = place[0];
		inPlaceBase[1] = place[1];
		inPlaceBase[2] = place[2];
	}
}

void HierarchyView_ApplyDetail()
{
	// play in place: the root section's horizontal place is put back over the base, every frame
	if (inPlaceOn && shown && controller.section_data)
	{
		int32_t const travelX = controller.section_data->World_Offset.vx - shown->ObWorld.vx;
		int32_t const travelZ = controller.section_data->World_Offset.vz - shown->ObWorld.vz;

		if (travelX != 0 || travelZ != 0)
		{
			int32_t const place[3] = { inPlaceBase[0] - travelX, shown->ObWorld.vy, inPlaceBase[2] - travelZ };

			HierarchyView_MoveTo(place);
		}
	}

	if (forcedDetail < 0 || !shown || !Global_VDB_Ptr)
	{
		return;
	}

	int distance = 0;

	// A level beyond the model's own (inteng's are 0 to 9): FUN_0047e9f0 gives each section the "L<k>#<its shape>" object with the LARGEST k at or below the level asked (0x47ed20-0x47edf6;
	// the shape itself when none), so a level past the last shows the last
	int const levels = HierarchyView_DetailLevels();
	int const level = forcedDetail < levels ? forcedDetail : levels - 1;

	if (level <= 0)
	{
		GlobalLevelOfDetail_Hierarchical = 0;      // the full shape: the nearest band
		return;
	}

	if (!HierarchyView_DetailDistance(level, &distance) || distance <= 0)
	{
		return;
	}

	// the game works out objectDistance = viewZ * (GlobalLevelOfDetail * CameraZoomScale) / 65536 (hmodel.cpp:1220-1233); the scale that makes it the level's own distance
	int const z = HierarchyView_DistanceFromModel();

	if (z <= 0)
	{
		return;
	}

	double const zoom = CameraZoomScale > 0.01f ? CameraZoomScale : 1.0f;

	GlobalLevelOfDetail_Hierarchical = (int)((double)distance * 65536.0 / ((double)z * zoom));
}

bool HierarchyView_WorldPoints(std::vector<int32_t> *points)
{
	std::vector<SECTION_DATA *> flat;
	std::vector<int> parents;

	if (!points || !shown)
	{
		return false;
	}

	points->clear();
	FlatSections(&flat, &parents);

	for (SECTION_DATA *section : flat)
	{
		SHAPEHEADER const *const shape = section->Shape;

		if (!shape || shape->numpoints <= 0 || !shape->points || !shape->points[0])
		{
			continue;
		}

		for (int i = 0; i < shape->numpoints && points->size() < 60000 * 3; i++)
		{
			VECTORCH point = { shape->points[0][i * 3], shape->points[0][i * 3 + 1], shape->points[0][i * 3 + 2] };
			VECTORCH turned;

			RotateAndCopyVector(&point, &turned, &section->SecMat);
			points->push_back(turned.vx + section->World_Offset.vx);
			points->push_back(turned.vy + section->World_Offset.vy);
			points->push_back(turned.vz + section->World_Offset.vz);
		}
	}

	return !points->empty();
}


// ---- A FILE OF OBJECTS WITH NO MODULES (hierarchy_view.h) -------------------------------------------------------------------------------------------------------------------------------------------------

struct ModelObject
{
	uint8_t id[8];
	DISPLAYBLOCK *block;
	int32_t place[3];
	bool collapsed;     // ModelObjects_Collapse put it at the origin; its own place and turn are kept below
	EULER euler;
	MATRIXCH matrix;
};

static std::vector<ModelObject> modelObjects;

extern double local_scale;

size_t ModelObjects_Build(File_Chunk *level, std::string *report)
{
	modelObjects.clear();

	if (!level)
	{
		Say(report, "no level");
		return 0;
	}

	List<Object_Chunk *> found;

	level->list_objects(&found);

	double const scale = local_scale > 0.0 ? local_scale : 1.0;
	size_t withoutShape = 0, noRoom = 0;

	for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (!object)
		{
			continue;
		}

		int const shape = GetLoadedShapeMSL(object->object_data.o_name);

		if (shape == GLS_NOTINLIST || shape < 0)
		{
			withoutShape++;
			continue;
		}

		MODULE tempModule;

		VisibilityDefaultObjectMap.MapShape = shape;
		tempModule.m_mapptr = &VisibilityDefaultObjectMap;
		tempModule.m_sbptr = (STRATEGYBLOCK *)NULL;
		tempModule.m_numlights = 0;
		tempModule.m_lightarray = (struct lightblock *)0;
		tempModule.m_extraitemdata = (struct extraitemdata *)0;
		tempModule.m_dptr = NULL;
		tempModule.name = NULL;

		AllocateModuleObject(&tempModule);

		DISPLAYBLOCK *const block = tempModule.m_dptr;

		if (!block)
		{
			noRoom++;
			continue;
		}

		// the place and the turn as the game's add_default_object reads them (object_setup.cpp)
		QUAT q;
		MATRIXCH m;
		EULER euler;

		q.quatx = (int)-(object->object_data.orientation.x * ONE_FIXED);
		q.quaty = (int)-(object->object_data.orientation.y * ONE_FIXED);
		q.quatz = (int)-(object->object_data.orientation.z * ONE_FIXED);
		q.quatw = (int)(object->object_data.orientation.w * ONE_FIXED);
		QuatToMat(&q, &m);
		MatrixToEuler(&m, &euler);

		block->ObStrategyBlock = nullptr;
		block->ObMyModule = nullptr;
		block->ObWorld.vx = (int)(object->object_data.location.x * scale);
		block->ObWorld.vy = (int)(object->object_data.location.y * scale);
		block->ObWorld.vz = (int)(object->object_data.location.z * scale);
		block->ObEuler = euler;
		CreateEulerMatrix(&block->ObEuler, &block->ObMat);
		TransposeMatrixCH(&block->ObMat);

		ModelObject made = {};

		memcpy(made.id, &object->object_data.ID, sizeof(made.id));
		made.block = block;
		made.place[0] = block->ObWorld.vx;
		made.place[1] = block->ObWorld.vy;
		made.place[2] = block->ObWorld.vz;
		modelObjects.push_back(made);
	}

	if (report && !modelObjects.empty())
	{
		DISPLAYBLOCK const *const first = modelObjects[0].block;

		*report = "first block: shape " + std::to_string((long long)first->ObShape) + ", flags " + std::to_string((long long)first->ObFlags) + ", flags2 " + std::to_string((long long)first->ObFlags2) + ", radius " +
			std::to_string((long long)first->ObRadius) + ", at " + std::to_string((long long)first->ObWorld.vx) + " " + std::to_string((long long)first->ObWorld.vy) + " " + std::to_string((long long)first->ObWorld.vz) + "; ";
	}
	else if (report)
	{
		report->clear();
	}

	if (report)
	{
		*report += std::to_string((unsigned long long)modelObjects.size()) + " objects drawn of " + std::to_string((unsigned long long)found.size()) + " (" + std::to_string((unsigned long long)withoutShape) +
			" have no shape in play, " + std::to_string((unsigned long long)noRoom) + " found no room for a display block)";
	}

	return modelObjects.size();
}

void ModelObjects_Close()
{
	ModelObjects_Uncollapse();
	modelObjects.clear();
}

void ModelObjects_Uncollapse()
{
	for (ModelObject &each : modelObjects)
	{
		if (each.collapsed && each.block)
		{
			each.block->ObWorld.vx = each.place[0];
			each.block->ObWorld.vy = each.place[1];
			each.block->ObWorld.vz = each.place[2];
			each.block->ObEuler = each.euler;
			each.block->ObMat = each.matrix;
		}

		each.collapsed = false;
	}
}

size_t ModelObjects_Collapse(const std::vector<std::vector<uint8_t>> &ids)
{
	ModelObjects_Uncollapse();

	size_t found = 0;

	for (ModelObject &each : modelObjects)
	{
		bool wanted = false;

		for (std::vector<uint8_t> const &id : ids)
		{
			wanted = wanted || (id.size() == 8 && memcmp(id.data(), each.id, 8) == 0);
		}

		if (!wanted || !each.block)
		{
			continue;
		}

		each.euler = each.block->ObEuler;
		each.matrix = each.block->ObMat;
		each.collapsed = true;
		each.block->ObWorld.vx = 0;
		each.block->ObWorld.vy = 0;
		each.block->ObWorld.vz = 0;
		each.block->ObEuler.EulerX = 0;
		each.block->ObEuler.EulerY = 0;
		each.block->ObEuler.EulerZ = 0;
		CreateEulerMatrix(&each.block->ObEuler, &each.block->ObMat);
		TransposeMatrixCH(&each.block->ObMat);
		found++;
	}

	return found;
}

size_t ModelObjects_Blocks(std::vector<const struct displayblock *> *out)
{
	out->clear();

	for (ModelObject const &each : modelObjects)
	{
		if (each.block && !each.collapsed)
		{
			out->push_back(each.block);
		}
	}

	return out->size();
}

size_t ModelObjects_Count()
{
	return modelObjects.size();
}

bool ModelObjects_IdOfBlock(const struct displayblock *block, uint8_t id[8])
{
	for (ModelObject const &one : modelObjects)
	{
		if (one.block == block)
		{
			memcpy(id, one.id, 8);
			return true;
		}
	}

	return false;
}

struct displayblock *ModelObjects_BlockOfId(const uint8_t id[8])
{
	for (ModelObject const &one : modelObjects)
	{
		if (memcmp(one.id, id, 8) == 0)
		{
			return one.block;
		}
	}

	return nullptr;
}

bool ModelObjects_Bounds(int32_t mins[3], int32_t maxs[3])
{
	if (modelObjects.empty())
	{
		return false;
	}

	for (int axis = 0; axis < 3; axis++)
	{
		mins[axis] = maxs[axis] = modelObjects[0].place[axis];
	}

	for (ModelObject const &one : modelObjects)
	{
		for (int axis = 0; axis < 3; axis++)
		{
			mins[axis] = one.place[axis] < mins[axis] ? one.place[axis] : mins[axis];
			maxs[axis] = one.place[axis] > maxs[axis] ? one.place[axis] : maxs[axis];
		}
	}

	return true;
}

// ---- THE SHAPE CHOOSER'S PREVIEW (hierarchy_view.h) ------------------------------------------------------------------------------------------------------------------------------------------------------

static DISPLAYBLOCK *previewBlock = nullptr;
static int const kPreviewZ = 1000000;   // millimetres: where the shape stands (the level is behind the camera)

void ShapePreview_Close()
{
	if (previewBlock)
	{
		DestroyActiveObject(previewBlock);
		previewBlock = nullptr;
	}

	R_SetEditorDrawObjectsOutsideModules(false);
}

bool ShapePreview_Show(int shapeNumber, int32_t *cameraZ)
{
	ShapePreview_Close();

	SHAPEHEADER *const header = shapeNumber >= 0 ? GetShapeData(shapeNumber) : nullptr;

	if (!header)
	{
		return false;
	}

	MODULE tempModule;

	VisibilityDefaultObjectMap.MapShape = shapeNumber;
	tempModule.m_mapptr = &VisibilityDefaultObjectMap;
	tempModule.m_sbptr = (STRATEGYBLOCK *)NULL;
	tempModule.m_numlights = 0;
	tempModule.m_lightarray = (struct lightblock *)0;
	tempModule.m_extraitemdata = (struct extraitemdata *)0;
	tempModule.m_dptr = NULL;
	tempModule.name = NULL;

	AllocateModuleObject(&tempModule);

	DISPLAYBLOCK *const block = tempModule.m_dptr;

	if (!block)
	{
		return false;
	}

	block->ObStrategyBlock = nullptr;
	block->ObMyModule = nullptr;
	// inteng shows the shape alone (the level is not drawn on that screen). Here the shape stands far out on +z (kPreviewZ) and the camera is 2 x its maxx behind it looking +z, so the whole level lies behind the camera
	block->ObWorld.vx = 0;
	block->ObWorld.vy = 0;
	block->ObWorld.vz = kPreviewZ;
	block->ObEuler.EulerX = 0;
	block->ObEuler.EulerY = 0;
	block->ObEuler.EulerZ = 0;
	CreateEulerMatrix(&block->ObEuler, &block->ObMat);
	TransposeMatrixCH(&block->ObMat);
	previewBlock = block;

	R_SetEditorDrawObjectsOutsideModules(true);

	if (cameraZ)
	{
		*cameraZ = kPreviewZ - 2 * header->shapemaxx;
	}

	return true;
}
