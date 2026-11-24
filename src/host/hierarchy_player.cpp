// hierarchy_player.cpp: see hierarchy_player.h. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "3dc.h"
#include "gameplat.h"
#include "platform.h"
#include "hmodel.h"
#include "chunk_load.hpp"
#include "mishchnk.hpp"
#include "environment_chunk.hpp"
#include "projload.hpp"

#include <string.h>

#include <algorithm>
#include <string>
#include <vector>

#include "hierarchy_player.h"
#include "hierarchy_view.h"   // HierarchyView_KeepCopiedRif: the copied model given back at the level's close

// avpx hmodel.cpp: the section placing (not declared in hmodel.h), and the library avpx projload.cpp keeps (not declared in projload.hpp)
extern void Process_Section(HMODELCONTROLLER *controller, SECTION_DATA *this_section_data, VECTORCH *parent_position, MATRIXCH *parent_orientation, int frame_timer, int sequence_type, int subsequence, int render);
extern List<Global_Hierarchy_Store *> Global_Hierarchy_Library;

namespace host
{
	struct HierarchyPlayer::State
	{
		RIFFHANDLE handle = INVALID_RIFFHANDLE;
		std::string riffName;                  // the library's name for the file (its RIFFNAME)
		SECTION *root = nullptr;
		HMODELCONTROLLER controller;
		bool created = false;
		bool started = false;
		bool looped = true;
		std::vector<PosedSection> sections;
	};

	HierarchyPlayer::HierarchyPlayer() : state(new State)
	{
		memset(&state->controller, 0, sizeof(state->controller));
	}

	HierarchyPlayer::~HierarchyPlayer()
	{
		Unload();
		delete state;
	}

	bool HierarchyPlayer::Loaded() const
	{
		return state->root != nullptr;
	}

	void HierarchyPlayer::Unload()
	{
		if (state->created)
		{
			Dispel_HModel(&state->controller);
			state->created = false;
		}

		state->started = false;
		state->root = nullptr;
		state->sections.clear();
		memset(&state->controller, 0, sizeof(state->controller));
		// the game's own store (Global_Hierarchy_Library) keeps the shapes it copied until the level's close gives them back (HierarchyView_GiveBackCopiedRifs)
		state->handle = INVALID_RIFFHANDLE;
	}

	bool HierarchyPlayer::Load(std::string const &modelName, std::string const &hierarchy, std::vector<std::string> *names, std::string *why)
	{
		Unload();

		std::string const path = modelName.compare(0, 9, "avp_rifs/") == 0 ? modelName + ".rif" : "avp_huds/" + modelName + ".rif";
		RIFFHANDLE const handle = avp_load_rif_non_env(path.c_str());

		if (handle == INVALID_RIFFHANDLE)
		{
			*why = "the game's loader does not take " + path;
			return false;
		}

		// a file named differently from its RIFFNAME, whose RIFFNAME the library already holds (Melanikus's MelPyth_hnpcmarine.rif is "hnpcmarine"): loaded under a name of its own, as the view does
		{
			RIF_Name_Chunk *const nameChunk = handle->envd ? (RIF_Name_Chunk *)handle->envd->lookup_single_child("RIFFNAME") : nullptr;

			if (nameChunk && nameChunk->rif_name && _stricmp(modelName.c_str(), nameChunk->rif_name) != 0)
			{
				bool held = false;

				for (LIF<Global_Hierarchy_Store *> each(&Global_Hierarchy_Library); !each.done(); each.next())
				{
					held = held || _stricmp(each()->riffname, nameChunk->rif_name) == 0;
				}

				if (held)
				{
					std::string const own = modelName + "~hv";

					delete[] nameChunk->rif_name;
					nameChunk->rif_name = new char[own.size() + 1];
					memcpy(nameChunk->rif_name, own.c_str(), own.size() + 1);
				}
			}
		}

		if (!copy_rif_data(handle, CCF_IMAGEGROUPSET + CCF_LOAD_AS_HIERARCHY_IF_EXISTS + CCF_DONT_INITIALISE_TEXTURES, 0, 0))
		{
			unload_rif(handle);
			HierarchyView_KeepCopiedRif(handle);   // whatever it copied before it refused goes back with the level (hierarchy_view.h)
			*why = "the game's loader does not take the hierarchies of " + path;
			return false;
		}

		unload_rif(handle);
		HierarchyView_KeepCopiedRif(handle);   // its shapes are the level's memory: the level's close gives them back (hierarchy_view.h)

		// the store this file made is the one holding handle
		Global_Hierarchy_Store *store = nullptr;

		for (LIF<Global_Hierarchy_Store *> each(&Global_Hierarchy_Library); !each.done(); each.next())
		{
			if (each()->rif_hand == handle)
			{
				store = each();
			}
		}

		if (!store)
		{
			*why = path + " holds no hierarchy";
			return false;
		}

		Hierarchy_Descriptor *chosen = nullptr;

		for (LIF<Hierarchy_Descriptor *> each(&store->hierarchy_list); !each.done(); each.next())
		{
			if (names)
			{
				names->push_back(each()->hierarchy_name);
			}

			if (!chosen && each()->hierarchy_root && (hierarchy.empty() || !strcmp(each()->hierarchy_name, hierarchy.c_str())))
			{
				chosen = each();
			}
		}

		if (!chosen)
		{
			*why = hierarchy.empty() ? path + " holds no hierarchy" : path + " has no hierarchy named " + hierarchy;
			return false;
		}

		state->handle = handle;
		state->riffName = store->riffname ? store->riffname : "";
		state->root = chosen->hierarchy_root;
		return true;
	}

	bool HierarchyPlayer::Sequences(std::vector<PlayedSequence> *out) const
	{
		if (!state->root)
		{
			return false;
		}

		out->clear();

		for (int i = 0; i < state->root->num_sequences; i++)
		{
			SEQUENCE const &each = state->root->sequence_array[i];
			PlayedSequence made;

			made.type = each.sequence_id & 0xffff;
			made.subsequence = each.sequence_id >> 16;
			made.seconds = each.Time;
			made.frames = 0;

			for (KEYFRAME_DATA const *frame = each.first_frame; frame; )
			{
				made.frames++;

				if (frame->last_frame || made.frames > 100000)
				{
					break;
				}

				frame = frame->Next_Frame;
			}

			out->push_back(made);
		}

		std::sort(out->begin(), out->end(), [](PlayedSequence const &a, PlayedSequence const &b) { return a.type != b.type ? a.type < b.type : a.subsequence < b.subsequence; });
		return true;
	}

	bool HierarchyPlayer::Start(int type, int subsequence, std::string *why)
	{
		if (!state->root)
		{
			*why = "no hierarchy is loaded";
			return false;
		}

		bool found = false;
		int const id = (subsequence << 16) + type;

		for (int i = 0; i < state->root->num_sequences; i++)
		{
			found = found || state->root->sequence_array[i].sequence_id == id;
		}

		if (!found || !state->root->sequence_array[0].first_frame)
		{
			*why = "the hierarchy has no such sequence";
			return false;
		}

		if (state->created)
		{
			Dispel_HModel(&state->controller);
			state->created = false;
		}

		memset(&state->controller, 0, sizeof(state->controller));
		Create_HModel(&state->controller, state->root);
		state->created = true;
		InitHModelSequence(&state->controller, type, subsequence, -1);
		state->controller.Playing = 0;
		state->started = true;
		Pose(0);
		return true;
	}

	int HierarchyPlayer::Position() const
	{
		return state->started ? state->controller.sequence_timer : 0;
	}

	int HierarchyPlayer::Length() const
	{
		return state->started ? state->controller.Seconds_For_Sequence : 0;
	}

	void HierarchyPlayer::Pose(int position)
	{
		if (!state->started)
		{
			return;
		}

		HMODELCONTROLLER &controller = state->controller;
		VECTORCH place = { 0, 0, 0 };
		MATRIXCH orientation = { ONE_FIXED, 0, 0, 0, ONE_FIXED, 0, 0, 0, ONE_FIXED };

		controller.sequence_timer = position < 0 ? 0 : (position > ONE_FIXED - 1 ? ONE_FIXED - 1 : position);
		Process_Section(&controller, controller.section_data, &place, &orientation, controller.sequence_timer, controller.Sequence_Type, controller.Sub_Sequence, 0);

		// the sections, root first, each with the index of its parent
		state->sections.clear();

		std::vector<std::pair<SECTION_DATA *, int>> work;

		work.push_back(std::make_pair(controller.section_data, -1));

		for (size_t at = 0; at < work.size(); at++)
		{
			SECTION_DATA *const data = work[at].first;
			PosedSection made;

			made.name = data->sempai && data->sempai->Section_Name ? data->sempai->Section_Name : "";
			made.parent = work[at].second;
			made.place[0] = data->World_Offset.vx;
			made.place[1] = data->World_Offset.vy;
			made.place[2] = data->World_Offset.vz;
			made.matrix[0] = data->SecMat.mat11; made.matrix[1] = data->SecMat.mat12; made.matrix[2] = data->SecMat.mat13;
			made.matrix[3] = data->SecMat.mat21; made.matrix[4] = data->SecMat.mat22; made.matrix[5] = data->SecMat.mat23;
			made.matrix[6] = data->SecMat.mat31; made.matrix[7] = data->SecMat.mat32; made.matrix[8] = data->SecMat.mat33;
			made.shape = data->Shape;
			made.shapeNumber = data->ShapeNum;
			made.data = data;
			state->sections.push_back(made);

			if ((data->flags & section_data_terminate_here) == 0)
			{
				for (SECTION_DATA *child = data->First_Child; child; child = child->Next_Sibling)
				{
					work.push_back(std::make_pair(child, (int)at));
				}
			}
		}
	}

	void HierarchyPlayer::Step(int dt)
	{
		if (!state->started)
		{
			return;
		}

		HMODELCONTROLLER &controller = state->controller;
		int next = controller.sequence_timer + MUL_FIXED(controller.timer_increment, dt);

		if (state->looped)
		{
			next &= (ONE_FIXED - 1);
		}

		Pose(next);
	}

	bool HierarchyPlayer::Box(std::vector<int> const &times, int32_t minimum[3], int32_t maximum[3], bool rootInPlace)
	{
		if (!state->started || times.empty())
		{
			return false;
		}

		bool any = false;

		for (int const time : times)
		{
			Pose(time);

			int32_t const rootX = rootInPlace && !state->sections.empty() ? state->sections[0].place[0] : 0;
			int32_t const rootZ = rootInPlace && !state->sections.empty() ? state->sections[0].place[2] : 0;

			for (PosedSection const &section : state->sections)
			{
				SHAPEHEADER const *const shape = section.data ? section.data->Shape : nullptr;

				if (!shape || shape->numpoints <= 0 || !shape->points || !shape->points[0])
				{
					continue;
				}

				for (int i = 0; i < shape->numpoints; i++)
				{
					VECTORCH point = { shape->points[0][i * 3], shape->points[0][i * 3 + 1], shape->points[0][i * 3 + 2] };
					VECTORCH turned;

					RotateAndCopyVector(&point, &turned, &section.data->SecMat);
					turned.vx += section.data->World_Offset.vx;
					turned.vy += section.data->World_Offset.vy;
					turned.vz += section.data->World_Offset.vz;

					int32_t const at[3] = { turned.vx - rootX, turned.vy, turned.vz - rootZ };

					for (int axis = 0; axis < 3; axis++)
					{
						if (!any || at[axis] < minimum[axis])
						{
							minimum[axis] = any ? at[axis] : at[axis];
						}

						if (!any || at[axis] > maximum[axis])
						{
							maximum[axis] = any ? at[axis] : at[axis];
						}
					}

					any = true;
				}
			}
		}

		return any;
	}

	std::vector<PosedSection> const &HierarchyPlayer::Sections() const
	{
		return state->sections;
	}
}
