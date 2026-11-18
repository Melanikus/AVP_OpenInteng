// hierarchy_view.h: the Hierarchy mode's view, a model drawn and played with the GAME'S OWN code. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// inteng's playback maths is the engine's (Vasquez, p11 section 66: FUN_004809e0 = the game's Slerp / New_Analyse_Keyframe_Data), and the editor project already compiles the game's hmodel.cpp
// and projload.cpp, so nothing here is a model of the animation: the model file is loaded as the game loads an NPC's (avp_load_rif_non_env, copy_rif_data with
// CCF_LOAD_AS_HIERARCHY_IF_EXISTS: projload.cpp LoadedPlacedHierarchy::load_rif), its template is found in the game's hierarchy library, a controller is made with Create_HModel and a
// sequence started with InitHModelSequence, a display block carries the controller (bh_placed_hierarchy.cpp MakePlacedHierarchyNear), and ProveHModel puts every section where the sequence
// says at the controller's timer; the renderer draws it through kshape.cpp's DoHModel as it draws any NPC. The model is drawn in the level the game has loaded (the game's loader
// takes levels, not model files).
//
// Time: a sequence's position is a 16.16 fraction (0 = the first frame, 65535 = the end); a sequence of `seconds` runs through it in that long.

#ifndef HIERARCHY_VIEW_H
#define HIERARCHY_VIEW_H

#include <stdint.h>
#include <string>
#include <vector>

// Loads the model file avp_huds/<modelName>.rif into the game's hierarchy library (once; a second call for the same name is a success). false with `why` when the file is not there or the game's loader refuses it.
bool HierarchyView_LoadModel(const char *modelName, std::string *why);

// The names of the hierarchies of a loaded model, in the game's library order (the model's file name without .rif)
bool HierarchyView_Names(const char *modelName, std::vector<std::string> *names);

// Shows hierarchy `hierarchyName` of the loaded model (the game's name for the main one is "Template") at the world place `place` (millimetres), turned `heading` units (the game's 4096 = a full turn) about the vertical axis,
// playing sequence `sequence`.`sub` for `seconds` 16.16 seconds (0: the sequence's own time). false with `why` when the model or the hierarchy or the sequence is not there. A model already shown is taken away first.
bool HierarchyView_Show(const char *modelName, const char *hierarchyName, int sequence, int sub, int32_t seconds, const int32_t place[3], int32_t heading, std::string *why);

// The same for a PLACED HIERARCHY of the level (Object Placement's kind 5, reassess/object-placement.md PH-1): turned by its own quaternion (x y z w, as PLACHIDT keeps it) the game's way,
// at `place` in the game's units. Added 2026-11-16 by Melanikus (Vasquez).
bool HierarchyView_ShowPlaced(const char *modelName, const char *hierarchyName, int sequence, int sub, int32_t seconds, const int32_t place[3], const float quaternion[4], std::string *why);

// The shown sequence's length in 16.16 seconds, set as it is (a placed hierarchy's record time, PH-2), the position kept
void HierarchyView_SetSequenceSeconds(int32_t seconds);

// The first sequence of a model's hierarchy (the game's root section's list, the file's order: inteng's sequence index 0), the model loaded when it isn't yet; false with `why` when there is none
bool HierarchyView_FirstSequence(const char *modelName, const char *hierarchyName, int *sequence, int *sub, std::string *why);

// The key frame the shown sequence's timer is in (inteng FUN_004790a0: the frame whose range holds the timer, the last one when none does); 0 when nothing is shown
int HierarchyView_KeyFrame();

// The sequences of the shown hierarchy's root section: the game's type and sub number, how many keyframes it has, and its own length in 16.16 seconds (0 when the file gives none)
struct HierarchyViewSequence
{
	int sequence;
	int sub;
	int frames;
	int time;
};

bool HierarchyView_Sequences(std::vector<HierarchyViewSequence> *sequences);

// Starts another sequence of the shown hierarchy at its beginning, keeping play / stop (the game's InitHModelSequence with the sequence's own time); false when the hierarchy has no such sequence
bool HierarchyView_SetSequence(int sequence, int sub);
bool HierarchyView_CurrentSequence(int *sequence, int *sub);

// Takes the model away (its display block, its controller); safe when none is shown
void HierarchyView_Close();
bool HierarchyView_IsShown();

// Play / stop (the sequence loops while it plays unless loop is false) and the position, 0..65535 of the sequence; setting the position stops it
// , / . HELD (inteng FUN_004889d0 0x489f9d-0x48a030, reassess/hierarchy-play-frost.md HP-17), called each frame with the frame's step (a quarter of the frame time in 16.16 seconds, signed): Main moves a
// STOPPED main sequence's timer by it (wrapped to 0..0xffff; a running one is not touched) and, with all, the stopped turret too and a stopped DELTA is made to RUN (inteng's quirk: the step goes into its running
// word, FUN_0047b9c0 0x47baaa); Second moves the timer of inteng's [2] (the newest
// delta when there is one, else the turret), running or not (the Shift variant)
void HierarchyView_ScrubMain(int32_t step, bool all);
void HierarchyView_ScrubSecond(int32_t step, bool delta);
// THE ALTERNATE SHAPE SETS on the model (inteng FUN_00477b40 / FUN_00477d10 -> FUN_0047e9f0: each section's shape is the set's replacement when a set of the list names that section, else the
// hierarchy's own; reassess/hierarchy-edit-frost.md HF-0x43): the sections are put back to their own shapes, then each named set (the file's OBHALTSH, by the game's library,
// GetHierarchyAlternateShapeSetFromLibrary) replaces its sections' shapes in turn, as the game's ChangeToAlternateAccoutrementSet does. An empty list is "Original". Gives how many sections changed.
int HierarchyView_ApplyShapeSets(const std::vector<std::string> &setNames);
// Counts each model shown (HierarchyView_Show): a new one has its own shapes again
int HierarchyView_ShowCount();
// all false: the main sequence only (inteng's P / O with "Play commands affect all sequences" off: FUN_0047b8b0 0x47b8c3 skips the turret and the deltas unless the player's +0x88, Y's switch, is on)
void HierarchyView_Play(bool on, bool loop, bool all = true);
bool HierarchyView_IsPlaying();
void HierarchyView_SetPosition(int32_t position);
int32_t HierarchyView_Position();

// THE TURRET PLAYER'S AND THE DELTA PLAYERS' numbers (inteng draws a "Timer :%d" and a "Seq. Time :%.1f sec" for each): the position 0 to 65535 and the time in milliseconds. false / 0 when there is none.
bool HierarchyView_TurretFacts(int *timer, int *milliseconds);
int HierarchyView_DeltaFacts(int *timers, int *milliseconds, int most);

// One keyframe of the playing sequence forward (direction 1) or back (-1), wrapping: the position lands on the start of the next / previous keyframe of the root section's list (each keyframe's Sequence_Length is its share of the sequence); a playing sequence plays on from there (inteng writes the timer only, HP-79).
// all (Y's "Play commands affect all sequences", inteng FUN_00479110 / FUN_00479290): the turret and every delta move on / back by the same time, each in its own sequence's terms
void HierarchyView_StepFrame(int direction, bool all);

// Ctrl+R / Ctrl+F on the shown model's root section frames of one sequence (the file's bytes are hierarchy_edit.h's): turned half round the vertical (x and z negated, the orientation as
// inteng's FUN_00480eb0 turns its player's), or lowered by `by` in the game's unit (each frame's y less it). False when the shown model has no such sequence.
bool HierarchyView_TurnRootSequence(int sequence, int sub);
bool HierarchyView_LowerRootSequence(int sequence, int sub, int by);

// DETAIL LEVELS (inteng's U and I, FUN_00477fb0; Shift+D, FUN_00479a80). A model whose shapes have lower-detail versions (the game's shape_degradation_array, built from the HIDEGDIS distances) shows the one
// the camera's distance asks for (the game's Get_Degraded_Shape). HierarchyView_DetailLevels is how many levels the shown model has (1 when none), HierarchyView_SetDetailLevel forces one (0 the full shape,
// -1 the game's own choice by distance), and HierarchyView_ApplyDetail has to be called every frame with the camera in place: it sets the game's level-of-detail scale so that the distance the game works out
// falls on that level's own. HierarchyView_DetailDistance reads a level's distance (mm) from the shown model's shapes; HierarchyView_SetDetailDistance changes it there. Levels are taken to be 0 to n - 1
// with none missing (the entries of a shape's array are in order of falling level).
int HierarchyView_DetailLevels();
void HierarchyView_SetDetailLevel(int level);
int HierarchyView_DetailLevel();
bool HierarchyView_DetailDistance(int level, int *millimetres);
bool HierarchyView_SetDetailDistance(int level, int millimetres);
int HierarchyView_DistanceFromModel();   // the camera's distance from the shown model in mm (the one Shift+D stores), 0 when none is shown
void HierarchyView_ApplyDetail();

// inteng's - and + : the sequence's time up or down by `milliseconds` (never below 100 ms), and how long it runs now; play backwards or forwards (inteng's < and >)
void HierarchyView_ChangeTime(int milliseconds);
int HierarchyView_SequenceMilliseconds();
void HierarchyView_SetBackwards(bool backwards);

// The sections of the shown hierarchy, root first, each before its children (a section's `parent` is a place in this list, -1 for the root); the section's world place and the radius of its shape (0: it has none)
struct HierarchyViewSection
{
	std::string name;        // the section's name (SECTION.Section_Name)
	std::string shapeName;   // the name of the object its shape is (SECTION.ShapeName): what Change object for section works on
	int parent;
	int32_t world[3];
	int32_t view[3];         // where the last draw put it relative to the camera (SECTION_DATA.View_Offset): 0 0 0 before it was drawn
	int32_t radius;
	bool hasShape;
};

bool HierarchyView_Sections(std::vector<HierarchyViewSection> *sections);

// The selected section (a place in that list; -1 none): drawn again in yellow over the model. A click picks it by its POLYGONS (HierarchyView_SectionOfHit below), never by a bounding sphere.
void HierarchyView_SelectSection(int index);
int HierarchyView_SelectedSection();

// The section a click's polygon pick met (PickReport::hierarchySection, the game's SECTION_DATA): its place in the list above; false when it is not a section of the hierarchy shown (inteng's
// FUN_00559720 then FUN_00479da0 / FUN_0047f880: hierarchy-closeout-frost.md TB-1)
bool HierarchyView_SectionOfHit(void const *sectionData, int *index);

// The box of the sections' places (not of their shapes), and moving the whole model to another place (its root's), posed again
bool HierarchyView_Extent(int32_t mins[3], int32_t maxs[3]);
void HierarchyView_MoveTo(const int32_t place[3]);

// THE TURRET (inteng's Add 'turret delta' 0x7 and the keys K L R Y): when the hierarchy has a section named "dum turret", the sections under it can play a second sequence (the turret sequence) while the rest play the main one,
// and the turret section itself is turned by an elevation in degrees (K / L: 10 either way). R puts every position back to the start; Y (toggle) makes the play / stop commands act on the turret sequence too.
bool HierarchyView_HasTurret();
bool HierarchyView_SetTurret(int sequence, int sub);   // false when there is no turret section or no such sequence
void HierarchyView_RemoveTurret();
bool HierarchyView_TurretSequence(int *sequence, int *sub);
void HierarchyView_TurnTurret(int degrees);
int HierarchyView_Elevation();
void HierarchyView_RestartAll();
bool HierarchyView_ToggleTurretFollows();

// Call once before each frame is drawn, with the time since the last one in milliseconds: the game's frame counter and frame time move on, and the model is put where its sequence says
void HierarchyView_Frame(int milliseconds);

// Delta sequences played over the main sequence (the game's Add_Delta_Sequence: used for aiming and turrets; inteng's "Add delta" 0x9 and "Remove delta" 0xa). The sequence must be one the
// file flags as a delta (HierarchyEdit_DeltaFrame): its frames then are changes from its reference frame. It plays at its own time, looping. false when nothing is shown or the hierarchy has no such sequence.
bool HierarchyView_AddDelta(int sequence, int sub);

// Takes every delta away
void HierarchyView_RemoveDeltas();

// The CURRENT delta (the one last added: inteng's player +8, FUN_00478780) plays or stops: Shift+P (case 0xb, +0x28 = 1 at 0x488b1a) and Shift+O (case 0xc, +0x28 = 0 at 0x488b3a). false when none.
bool HierarchyView_SetDeltaPlaying(bool on);

// How many deltas play now
int HierarchyView_DeltaCount();

// Every point of the drawn model's shapes in the world as it stands now (each turned by its section's matrix and moved by its place, as the game draws it), x y z for each, in the game's millimetres; at most
// 60000 points. The scenario harness projects them to say whether the whole model is inside the view.
bool HierarchyView_WorldPoints(std::vector<int32_t> *points);

// PLAY IN PLACE: a sequence whose root travels (a leap, a run) would carry the model out of the view. With this on, every frame the model is moved back so that the ROOT section stands over the given place
// horizontally (x and z; the height is the animation's own), as a treadmill. NAMED: inteng's own view is not read to say whether it shows the travel; the keyframes and what is saved are not touched.
void HierarchyView_SetInPlace(bool on, const int32_t place[3]);

// The delta sequence last added plays at this time (milliseconds, as inteng's delta object has): the game's seconds_for_sequence and timer_increment. false when there is none.
bool HierarchyView_SetDeltaMilliseconds(int milliseconds);
// Its time in milliseconds now; 0 when there is none.
int HierarchyView_DeltaMilliseconds();


// A FILE OF OBJECTS WITH NO MODULES (the HUD weapons, the character models: stock marwep.rif, Melanikus's MelPyth_MarWep.rif) draws nothing through the game's modules: the game puts a placed object in the world only
// when the module it is in is visible, and the file has no module, so "0 objects, 0 models" were drawn. inteng's own view shows every object of such a file where the file puts it ("all the weapons all
// around"): the 55 parts of MelPyth_MarWep's four guns are objects of the file, each at its place in the assembled gun. ModelObjects_Build makes a display block of the game's own for each object of the open
// file (the shape the game loaded for it, found by its name as the game's hierarchies find their sections' shapes: GetLoadedShapeMSL; its place and turn as the game's add_default_object reads them), as the
// game's MakeObjectNear does for a placed object, with no module. Only for a file with no modules. Returns how many were made; 0 when the file has modules (the game draws those) or no objects.
class File_Chunk;
struct displayblock;
size_t ModelObjects_Build(File_Chunk *level, std::string *report);
// Lets go of the list (the blocks themselves go with the level)
void ModelObjects_Close();
size_t ModelObjects_Count();
// The object of the file a display block is, by its 8-byte ID; false for a block that is not one of these
bool ModelObjects_IdOfBlock(const struct displayblock *block, uint8_t id[8]);
// The display block of the object with this ID (null when none): what the picking finds, and what the Hierarchy hand-off starts from
struct displayblock *ModelObjects_BlockOfId(const uint8_t id[8]);
// Where the objects are (millimetres, the game's world), the smallest and largest of each axis, for the camera; false when there are none
bool ModelObjects_Bounds(int32_t mins[3], int32_t maxs[3]);
// Every display block made for the file's objects (for hiding them all: the Hierarchy view switch), leaving out the ones ModelObjects_Collapse put at the origin; gives how many
size_t ModelObjects_Blocks(std::vector<const struct displayblock *> *out);
// THE SECTIONS BEFORE A SEQUENCE (inteng FUN_0047e320 0x47e6d4-0x47e733 / 0x47e544-0x47e556, reassess/hierarchy-play-frost.md HP-21 / HP-31): a chosen hierarchy's sections are the file's own objects, put
// at the origin (DAT_00637068/6c/70, never written: 0, 0, 0) with the identity turn (DAT_005d45d0) and drawn there until a main sequence places them (FUN_00478850 does nothing without one). Collapse
// moves the blocks of the objects with these IDs there (remembering where they were) and gives how many it found; Uncollapse puts them back. A second Collapse puts the first ones back first.
size_t ModelObjects_Collapse(const std::vector<std::vector<uint8_t>> &ids);
void ModelObjects_Uncollapse();

// THE SHAPE CHOOSER'S PREVIEW (inteng FUN_004dd870 / FUN_004dd6f0 / FUN_0053c4f0): the shape with that number drawn ALONE, turned by the identity matrix, at the origin, the camera 2 x its maxx behind it. Show makes the block and draws objects outside the modules (the renderer hook; the caller hides every module and placed object, LevelView_HideModules / HideObjects), and gives the camera's place (*cameraZ = -2 * shapemaxx); false when the game has no such shape. Close takes it all back.
bool ShapePreview_Show(int shapeNumber, int32_t *cameraZ);
// The number Show wants for an outside shape: its place in the game's main shape list, found by the name the game's loader kept it by (host::ShapeChoice::gameName; GetLoadedShapeMSL, as
// the game finds its shapes by name). -1 for no name or a name the game's list does not hold. NOT the file's own shape number (SHPHEAD1 file_id_num), which is another numbering.
int ShapePreview_PlaceOf(char const *gameName);
void ShapePreview_Close();
// The preview's spin (FUN_004ddb60 0x4ddba2-0x4ddbc2, FUN_0053c4f0): one frame of it, each frame the chooser is shown (300 and 600 4096ths of a turn a second about its own x and z)
void ShapePreview_Spin();

#endif
