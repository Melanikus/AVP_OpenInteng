// hierarchy_player.h: plays a model's hierarchy with the GAME'S OWN code (avpx hmodel.cpp and projload.cpp), for Hierarchy mode's view. Written 2026-11-15 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY THE GAME'S OWN CODE. inteng's frame maths (the sine-table Slerp, NormaliseQuat, QuatToMat of FUN_004809e0 and its neighbours) is the engine's own, the very maths of
// avpx hmodel.cpp (Slerp, QuatToMat, Handle_Section_Timer), and inteng's hierarchy.cpp is only a wrapper around frame lists (AvP-docs\engine\inteng\hierarchy-data-model.md,
// "Playback"). So a model played here by Create_HModel / InitHModelSequence / Process_Section shows exactly the poses the game shows.
//
// THE RECIPE (the game's own: projload.cpp LoadedPlacedHierarchy::load_rif :332-345 and its users):
//   Load:   avp_load_rif_non_env("avp_huds/<name>.rif"), copy_rif_data(h, CCF_IMAGEGROUPSET + CCF_LOAD_AS_HIERARCHY_IF_EXISTS + CCF_DONT_INITIALISE_TEXTURES, 0, 0), unload_rif(h);
//           the hierarchy is then in Global_Hierarchy_Library under the file's RIFFNAME: GetNamedHierarchyFromLibrary(riffName, hierarchyName) is its root SECTION.
//   Start:  Create_HModel(&controller, root); InitHModelSequence(&controller, type, subsequence, -1) (-1 = the sequence's own length; a positive number is seconds in 16.16).
//   Pose:   controller.sequence_timer = t (0 .. 65535, a fraction of the sequence in 16.16); Process_Section(&controller, controller.section_data, &place, &orientation, t, type,
//           sub, 0) fills every SECTION_DATA's World_Offset and SecMat (the section's place and matrix, the world as `place` + `orientation`).
//   Step:   t += MUL_FIXED(controller.timer_increment, dt) with dt in 16.16 seconds (the game's own line in HMTimer_Kernel), wrapped to 0 .. 65535 when looped.
//
// The sequence flags No Interpolation, Half Frame Rate, Upper and Lower are NOT read by the game's hmodel (grep over avpx: no use), so a pose here is the game's pose.
#pragma once

#include <stdint.h>

#include <string>
#include <vector>

struct section_data;
typedef struct section_data SECTION_DATA;
struct shapeheader;

namespace host
{
	// One section of the posed hierarchy (a SECTION_DATA, read after Pose)
	struct PosedSection
	{
		std::string name;           // the section's name
		int parent;                 // the index in the list of its parent, or -1 for the root
		int32_t place[3];           // World_Offset (the game's world units)
		int32_t matrix[9];          // SecMat, row by row (16.16)
		struct shapeheader *shape;  // the section's shape (0 = none), as the game keeps it
		int shapeNumber;            // the section's shape number
		SECTION_DATA *data;         // the game's own record
	};

	// One sequence a hierarchy has (type and subsequence as InitHModelSequence takes them)
	struct PlayedSequence
	{
		int type;
		int subsequence;
		int seconds;                // its own length in 16.16 seconds (SEQUENCE::Time)
		int frames;                 // the keyframes of its first section's list
	};

	class HierarchyPlayer
	{
	public:
		HierarchyPlayer();
		~HierarchyPlayer();

		// Loads avp_huds/<modelName>.rif by the game's recipe and finds the hierarchy; names lists the hierarchies the file holds. Empty `hierarchy` = the first.
		bool Load(std::string const &modelName, std::string const &hierarchy, std::vector<std::string> *names, std::string *why);

		bool Loaded() const;

		// The sequences of the loaded hierarchy (sorted by type, then subsequence)
		bool Sequences(std::vector<PlayedSequence> *out) const;

		// Starts a sequence (InitHModelSequence) and poses it at the start; false if the hierarchy has no such sequence
		bool Start(int type, int subsequence, std::string *why);

		// The position in the sequence now, 0 .. 65535 (16.16 fraction)
		int Position() const;

		// The sequence's own length in 16.16 seconds
		int Length() const;

		// Poses the model at a position (clamped to 0 .. 65535) and fills the sections
		void Pose(int position);

		// Goes on by dt (16.16 seconds, the game's own line), looping, then poses
		void Step(int dt);

		// The box of the posed sections' shape points (each point turned by its section's matrix and moved by its place, as inteng's FUN_0047f9f0 does, in the game's own units) over the
		// given times (16.16 positions of the started sequence): *minimum and *maximum are x y z. false when nothing was started or no section has a point.
		// rootInPlace: each pose is taken with the root section's horizontal place (x and z) taken away, as the Hierarchy view shows the model when it keeps the root where it stands.
		bool Box(std::vector<int> const &times, int32_t minimum[3], int32_t maximum[3], bool rootInPlace = false);

		// The sections, root first, as the last Pose left them
		std::vector<PosedSection> const &Sections() const;

		// Forgets the model (the game's own Global_Hierarchy_Library keeps what it loaded)
		void Unload();

	private:
		struct State;
		State *state;
	};
}
