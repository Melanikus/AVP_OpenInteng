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
	// "avp_rifs/<file>": the OPENED file itself (inteng's hierarchies are those of the opened file); a plain name is a file of avp_huds
	bool const ofTheLevel = modelName && strncmp(modelName, "avp_rifs/", 9) == 0;

	if (!modelName || !modelName[0] || strlen(modelName) > 80 || strpbrk(ofTheLevel ? modelName + 9 : modelName, "\\/:.*?\"<>|"))
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
	if (already && _stricmp(modelName, inside.c_str()) != 0)
	{
		std::string const own = std::string(modelName) + "~hv";

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

bool HierarchyView_Show(const char *modelName, const char *hierarchyName, int sequence, int sub, int32_t seconds, const int32_t place[3], int32_t heading, std::string *why)
{
	HierarchyView_Close();

	if (!place || !HierarchyView_LoadModel(modelName, why))
	{
		return false;
	}

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
	shown->ObEuler.EulerX = 0;
	shown->ObEuler.EulerY = heading & 4095;
	shown->ObEuler.EulerZ = 0;
	CreateEulerMatrix(&shown->ObEuler, &shown->ObMat);
	TransposeMatrixCH(&shown->ObMat);
	shown->HModelControlBlock = &controller;
	controller.Playing = 0;
	controller.Looped = 1;
	GlobalFrameCounter++;
	ProveHModel(&controller, shown);
	return true;
}

void HierarchyView_Play(bool on, bool loop)
{
	if (shown)
	{
		// P (case 5, FUN_0047b8b0, 0x488ad6) sets the running switch (+0x28) of the main player and of the turret / delta players; O (case 6, FUN_0047b940) clears them. A player is made NOT running and looping
		// (FUN_0047d6f0: +0x28 = 0, +0x2c = 1), so choosing a sequence shows it standing still at position 0 until P.
		controller.Playing = on ? 1 : 0;
		controller.Looped = loop ? 1 : 0;

		for (DELTA_CONTROLLER *each = controller.Deltas; each; each = each->next_controller)
		{
			each->Playing = on ? 1 : 0;
		}
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

void HierarchyView_StepFrame(int direction)
{
	if (!shown || !controller.Root_Section)
	{
		return;
	}

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
	HierarchyView_SetPosition(starts[current]);
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

bool HierarchyView_TurretFacts(int *timer, int *milliseconds)
{
	if (!shown || !turretOn || !timer || !milliseconds || !controller.Root_Section)
	{
		return false;
	}

	SEQUENCE *const sequence = GetSequencePointer(turretSequence, turretSub, controller.Root_Section);
	int const seconds = sequence && sequence->Time > 0 ? sequence->Time : ONE_FIXED;

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

	if (forcedDetail == 0)
	{
		GlobalLevelOfDetail_Hierarchical = 0;      // the full shape: the nearest band
		return;
	}

	if (!HierarchyView_DetailDistance(forcedDetail, &distance) || distance <= 0)
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
	modelObjects.clear();
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
