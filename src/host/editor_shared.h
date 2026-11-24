// editor_shared.h: what the editor's own files share, the names one of them makes and another uses (editor_main.cpp, texture_map_screen.cpp, strategy_screen.cpp, editor_proofs.cpp).
// Written 2026-11-17 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: editor_main.cpp grew past the size Snyk Code reads (about 1,500 KB), and a file it can't read it skips without a word. So whole
// parts of it moved into files of their own, as they were: these declarations are the only new thing. A function or a variable here is
// made in the file named above it; a type or a constant is made here. They are in the order editor_main.cpp had them, so each one comes
// after what it uses. This header is included after the editor's own includes (each of the files has editor_main.cpp's list).
#pragma once

// WHAT THIS HEADER USES, so a file can include it first and alone (tests/host_editor_shared_test builds a file that does just that, and fails
// when a name is missing). A name the header only names (a pointer, a reference, an extern variable, a function's argument or answer) is
// declared ahead here, with its header beside it; a name it needs whole (an array's size, a member of a struct made here, an enum used by
// value) comes from its header. A new name used further down goes here too.
#include <windows.h>   // HWND, UINT, WPARAM, DWORD, ULONGLONG, POINT, MAX_PATH

#include <stddef.h>
#include <stdint.h>

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "editor_bar.h"             // kEditorBarMostRows (an array's size); EditorBarRows, EditorBarRoom
#include "editor_icons.h"           // host::IconWorldTriangle (IconForPick's triangles)
#include "key_layout.h"             // KeyLayout (an enum, used by value)
#include "object_edit.h"            // host::ObjectEditResult (an enum, used by value)
#include "selection.h"              // host::PickReport (PlacementPick's report); host::Selected
#include "special_objects_edit.h"   // host::SpecialKind (PlacementPick's kind, and its value Generator)
#include "status_words.h"           // StatusWords (statusWords, used by value)
#include "tool_buttons.h"           // host::kToolButtonMost (an array's size); host::ToolButton

class OpenLevelFile;        // src/io/level_file.h
struct EditorHostState;     // editor_actions.h
struct FlyCamera;           // level_view.h
struct LevelViewTriangle;   // level_view.h
struct PanelExtras;         // panel_pass.h
enum class LevelOpenResult;   // level_file.h

namespace host
{
	class AdjacentList;         // adjacent_list.h
	class AutoLinkRunner;       // autolink_runner.h
	class FieldPanelHost;       // field_panel_host.h
	class LightState;           // light_state.h
	class LinkTree;             // module_links.h
	class ObjectList;           // object_list.h
	class WaypointMode;         // waypoint_mode.h
	struct DialogField;         // dialog_box.h
	struct EditorOptions;       // editor_options.h
	struct EntryMarker;         // entry_markers.h
	struct HierarchyMode;       // hierarchy_mode.h
	struct HierarchyStep;       // hierarchy_mode.h
	struct ObjectTypeCall;      // object_type_edit.h
	struct PaintBox;            // panel_paint.h
	struct PaintText;           // panel_paint.h
	struct PathNode;            // path_mode.h
	struct PickCamera;          // pick_ray.h
	struct PlacedSoundIn;       // placed_sounds_game.h
	struct PolyCountRow;        // poly_counts.h
	struct PolyVertexList;      // poly_edit.h
	struct RequestPageState;    // strategy_views.h
	struct ShapeChoice;         // shape_chooser.h
	struct SoundEditRecord;     // sound_edit.h
	struct SpecialMode;         // special_mode.h
	struct TrackClip;           // track_edit.h
	struct TrackEditor;         // track_editor.h
	struct TrackLine;           // track_view.h
	struct TrackPose;           // track_play_game.h
	struct VirtualObjectInfo;   // virtual_object.h
	enum class TrackEditResult;   // track_edit.h
}

namespace ui
{
	class MenuBar;        // menu_bar.h
	struct Box;           // panel_layout.h
	struct PanelLayout;   // panel_layout.h
}

// made in editor_main.cpp
extern bool bRunning;
extern HWND editorWindow;
extern size_t barRowCount;
extern EditorBarRows barRows;
extern size_t barScreen;
extern StatusWords statusWords;   // the status line's words, a copy it owns (status_words.h)
extern KeyLayout keyLayout;
bool LoadTheMenus();
extern bool keyDown[256];
bool KeyHeldNow(int vk);
extern bool running;
extern int32_t threeCursor[3];
bool ThreeViewsOn();

// made in texture_map_screen.cpp
void TexturePolygonClicked(OpenLevelFile *open, uint8_t const *target, bool isModule, int32_t moduleIndex, int32_t polygon, bool deselect, bool copyNow);

// made in editor_main.cpp
extern int harnessPictureAsked;
extern bool pickAsked;
extern bool pickInBox;
extern bool pickWithS;
extern bool pickShift;
extern int pickX;
extern int pickY;
extern host::FieldPanelHost fieldHost;
extern double local_scale;
void LetGoOfTheFieldPanel();
void ShowTheSelected(const host::Selected &selected);
extern uint32_t openClock;
extern uint32_t fieldsOpenedAt;
extern int32_t promptFor;
extern std::string promptText;
extern int32_t barPointerY;
extern int32_t barPointerX;
void DialogMessage(std::string const &text, std::function<void()> const &then = nullptr);
void DialogConfirm(std::string const &text, std::function<void(bool)> const &then);
void DialogText(std::string const &text, std::vector<host::DialogField> const &fields, std::function<void(int32_t, std::vector<std::string> const &)> const &then);
void DialogList(std::string const &title, std::vector<std::string> const &rows, std::function<void(int32_t)> const &then, std::function<void(int32_t)> const &onRight = nullptr);
extern bool editorHiddenRun;
extern FlyCamera *theCamera;
extern OpenLevelFile *editedLevel;
extern const char *textPanelText;
void OpenAPrompt(int32_t kind);
extern bool onHierarchy;
extern bool hierarchyModelOpen;
extern host::HierarchyMode hierarchyMode;

// made in texture_map_screen.cpp
void TextureMapCommand(int32_t command);
bool TextureMapKeyCommandRun(uint8_t key, bool shift, bool ctrl);   // a key as inteng's key table gives it (texture_map_screen.cpp, TM-A); true when it had a command
bool TextureMapLoopWaits();   // one of Texture Map's waits inteng runs as a loop of its own is up: Home and Insert do nothing then (CAM-9)

// made in editor_main.cpp
extern std::vector<std::string> promptList;
void SaveTheCopy();

// made in strategy_screen.cpp
void StrategySave();

// made in editor_main.cpp
void LoadTheObjectListAgain();
std::vector<uint8_t> PickedModuleIdBytes();

// made in strategy_screen.cpp
void StrategyModeCommand(int32_t command);
void RunStrategyDelete(std::string const &typed);

// made in editor_main.cpp
extern int strategyPick;
size_t KindsTheObjectCanTake(uint8_t const *id, int32_t *kinds);
extern std::vector<host::VirtualObjectInfo> destroyChoices;
extern uint8_t destroyId[8];
extern uint8_t assignId[8];
extern int64_t virtualPlace[3];
extern uint8_t stabilityId[8];
void ShowTheFieldsAgain();
bool ModuleObject(OpenLevelFile *open, int32_t moduleIndex, uint8_t id[8]);
extern bool placementSplit;
bool SelectedObjectId(uint8_t *id);
extern std::string savedWords;
extern int32_t strategyViewAdd;
void CameraKey(int n, bool save);
extern bool strategyFromPlacement;
extern bool strategyMarkedChanged;
extern uint32_t strategyViewBit;

// made in texture_map_screen.cpp
void TextureAnimFlush();
void TexturePolyApplyRemoved();

// made in editor_main.cpp
extern bool trackShowStart;
extern bool trackTurnAboutPivot;
extern host::TrackEditor trackEd;
extern host::TrackPose trackPose;
extern bool trackPlaying;
bool OnTheTrackScreen();

// ---- THE TRACK DIALOGS as inteng draws them (Track Properties: check boxes "Loop Track", "Loop back and forth", "Playing at start", "Use Track Smoothing" with OK and Cancel, FUN_004f4090; the sound dialog:
// a file name, the ranges, the volume and pitch, loop, with OK and Cancel, edit_sound): a form over the level, in the same gray overlay as a prompt. Up / Down / Tab move between the rows, Space ticks a
// box, letters go into the row that has a text, Enter is OK, Esc is Cancel; a click on a row selects it (and ticks a box), a click on OK or Cancel does that. The form answers the same functions as the
// prompts' typed answers do (the letters L B P S; "name inner outer volume pitch loop"), so the checks are the same. (Frost, 2026-11-15)
struct FormRow
{
	std::string label;
	bool isCheck = false;
	bool checked = false;
	std::string text;
};

// made in editor_main.cpp
extern std::vector<FormRow> formRows;
void RunThePrompt();
void DeleteMultipleForm();
void ConfirmAs(int32_t kind, std::string const &text);
void TextAs(int32_t kind, std::string const &text, std::string const &initial);
void FormKey(UINT message, WPARAM key);
extern std::string exitWords;
bool LevelChangedSinceSave();
extern bool strategyFlagPage;
extern bool strategyPanelOn;

// THE VOLUME EDITOR (FUN_005277c0, "V : Edit Volume"; Frost 2026-11-16): inteng takes the screen over: the volume is drawn as a RED WIREFRAME CUBE (FUN_00583130 with 0xff0000, block flag 0x10000000); the text lines at (16, 0 ..):
// "S : Set volume to module extents" (0x5ee12c), "X : Expand volume to include module" (0x5ee108), "Shift+Left Click : Select face of cube" (0x5ee0e0), "+/- : Move selected face" (0x5ee0c4), "Hold shift to move face faster"
// (0x5ee0a4), "Press return when finished" (0x5edfe4). A left click picks a MODULE (it turns red: FUN_00529470); Shift + left click picks a FACE of the cube (FUN_00559a60: 0 min X, 1 max X, 2 min Y, 3 max Y, 4 min Z, 5 max Z; the
// face turns green, 0x00ff00); S sets the cube to the picked module's box, X grows it to include that box; + / - move the selected face out / in by 200000 units a second (2 metres, 20 with Shift: FUN_00527e00 is the 16.16 product of
// the speed and the frame time, at most 0x199a = 0.1 s), a face never crossing its opposite; Return or the right button ends (Escape does nothing). The numbers are in inteng's units (file x 100 x scale); here the cube is in millimetres.
struct VolumeEditor
{
	bool on = false;
	uint8_t id[8] = { 0 };
	int32_t kind = 0;
	double lo[3] = { 0, 0, 0 };
	double hi[3] = { 0, 0, 0 };
	int face = -1;
	int module = -1;          // the picked module's game number, -1 none
	bool objectPicked = false;   // a placed object was picked instead (FUN_00559720 gives any record): its ID
	uint8_t object[8] = { 0 };
	bool iconPicked = false;     // that record is one of the editor's icons (a virtual object, generator, sound ...): S / X take its drawn box (StrategyIconBox), S-VOL-4
	ULONGLONG last = 0;       // the last frame's time
};

// made in editor_main.cpp
extern VolumeEditor volume;
void VolumeBegin();
extern int32_t strategyViewTop;
extern int32_t strategyViewLeft;
extern int32_t strategyViewBox[4];
// Every screen change, a bar pick's or a mode's own Exit / Tab (ShowTheScreen): was -> now, once, with barScreen already now. A mode's start and leave hooks go in it (editor_main.cpp).
void OnScreenChanged(size_t was, size_t now);
extern std::string strategyPickField;
extern std::string strategyViewPickWords;
extern int32_t strategyEditKind;
extern std::vector<std::array<uint8_t, 8>> strategyViewLinks;
extern bool strategyEditFan;

// made in strategy_screen.cpp
void StrategyViewEnd();

// made in editor_main.cpp
extern uint8_t strategyEditId[8];
extern bool strategyPickHave;
extern uint8_t strategyPickId[8];
extern int32_t strategyPickRow;
extern std::string strategyPickRequest;

// made in strategy_screen.cpp
void StrategyPickEnd();
void LiftPositionsEnd();
void MissionTargetsEnd();
void MissionToggleUnderPointer(int32_t bit);

// made in editor_main.cpp
extern int32_t trackPage;
extern int32_t multiPage;

// made in strategy_screen.cpp
void MultiNextPage();
void MultiKeyUnderPointer(int key, bool shift);
void MultiTargetsEnd();

// made in editor_main.cpp
extern bool requestPageOn;
extern host::RequestPageState requestPage;
extern uint8_t requestPageTarget[8];
extern std::string requestPageField;
extern int32_t requestPageRow;

// made in strategy_screen.cpp
void RequestPageEnd(bool escaped);
void StrategyRequestE();
void TrackTargetsEnd();
void TrackPageBack();
void TrackAskPoint(uint8_t const *subject);
void StrategyViewClickAt(int32_t x, int32_t y);

// COPY STRATEGY (FUN_00528340 with the collector FUN_00526bf0): the object to copy from is the current one; then the collector runs: a click adds the object under the pointer to the list (when it is not in it), Shift + click takes it out, L empties the list,
// Return or the right button ends; the screen draws "Press return when finished" at (16, 0) and the names from the third row on (rows of 16; DAT_00914928 is incremented twice before the first name), and "Select objects to copy to" along the foot (200, height - 20).
// Then each target in the list is dealt with in order: the source itself and a target whose class cannot take the kind are passed; one that has a strategy FUN_00521860 does not call replaceable gets the four-answer question (CopyQuestion, FUN_00529f30: Yes All / Yes / No / No All; Escape replaces); a copy that fails says "Unable to copy this strategy" and the rest is abandoned.
// Ctrl + click (S-79, 0x526e3b-0x526ecc) adds every object of the level that uses the clicked object's shape (CopyCollectShapeGroup).
struct CopyTarget { uint8_t id[8]; };

// made in editor_main.cpp
extern bool copyCollecting;
extern int32_t copyPurpose;
extern uint8_t copyFrom[8];
extern int32_t copyKind;
extern std::vector<CopyTarget> copyTargets;
void StrategyMessage(std::string const &words);
void ExitTheMode();

static const size_t kTexturePolygonsMost = 20000;

struct TexturePolygon
{
	uint8_t id[8];       // the object whose shape the polygon is in
	int32_t polygon;     // the polygon's number in the file's SHPPOLYS
	bool isModule;       // the click was on a module of the level (its shape is drawn from the module's display block, not an object's)
	int32_t moduleIndex; // for a module: the game's number for it
};

// made in editor_main.cpp
extern std::vector<TexturePolygon> texturePolygons;
extern std::string textureWords;
void ShowTheTexturePolygons();
bool TexturePolygonOutlineOff(TexturePolygon const &one);   // texture_map_screen.cpp: I (0x105) hid this selected polygon's outline
void TextureOutlineAllShown();   // every selected polygon's outline shown again (FUN_0056f9d0, the windows' leaving)
void TextureRecentredBox(uint8_t const *target);   // Recentre Shape's "Recentred %s" box (0xd6)
// texture_colour_pick.cpp: Select Colour (Shift+K, 0x40) and Apply Colour (Ctrl+K, 0x41)
extern bool textureColourPicking;   // the grid is up, waiting for a press
void TextureColourPickStart();
bool TextureColourPickPress(int32_t x, int32_t y, bool left);
void TextureColourDraw(int32_t frameW, int32_t frameH);
void TextureColourApplyCurrent();

// made in texture_map_screen.cpp
void TextureForEveryShape(OpenLevelFile *open, std::function<void(const uint8_t *id, bool isModule, int32_t moduleIndex)> const &each);

// made in editor_main.cpp
extern int texturePlaneMode;

// made in texture_map_screen.cpp
size_t TextureSelectedIndex(uint8_t const *id, int32_t polygon);
bool TextureSelectPolygon(uint8_t const *id, bool isModule, int32_t moduleIndex, int32_t polygon, bool select);

// made in editor_main.cpp
extern uint32_t textureFlagViewMask;
extern size_t textureFlagViewCount;

// made in texture_map_screen.cpp
bool TexturePolyMarkedForSplit(uint8_t const *id, int32_t polygon);

// made in editor_main.cpp
extern bool textureViewAnimated;

// made in texture_map_screen.cpp
void TextureModeStateReset();

// made in editor_main.cpp
extern size_t textureShapesNotDrawn;
bool ShowTheModelInFront(char const *model, char const *hierarchy, int sequence, int sub, int32_t place[3], std::string *why, int32_t seconds = 0);
extern bool hierarchyCameraRoot;

// made in editor_proofs.cpp
bool PressTheBarItem(char const *words);

// made in editor_main.cpp
extern bool flyRelAxes;
extern bool flyIncBox;
extern bool trackFly;

// made in texture_map_screen.cpp
void TextureMapDeselectAll();
extern int textureSelectMode;
extern host::PolyVertexList polyVertices;
extern std::vector<int32_t> polyShapeWorld;
extern int texturePickViewW, texturePickViewH;
extern int textureArrowVertical, textureArrowHorizontal;
extern int textureAutoMinutes;
extern bool textureAutoPrompt;
extern DWORD textureAutoLast;
extern bool textureSHeld;
extern bool textureFlagList;
extern std::vector<std::string> textureFlagLines;
extern int textureHoverX, textureHoverY;
extern bool textureHoverSet;
extern int textureArrowAsked;
extern int textureRepeatX, textureRepeatY;
extern bool textureRectActive, textureRectAsked, textureRectShift;
extern int textureRectX0, textureRectY0, textureRectX1, textureRectY1, textureRectViewW, textureRectViewH;
std::string TextureAnimTitle(int ask);
extern bool textureAnimPick;
extern int textureMultiMode;
void TextureMultiDo(bool relative);
extern double textureGotoDistance;
void TextureMapEdit(int32_t command);
void TextureMapStepPicture(int step);
extern bool textureWindowOn;
extern int textureSubMode;
extern bool textureTextShown;   // DAT_005fd69c, P (0x32) in the polygon screen and Merge (texture_map_screen.cpp)
bool TextureBarsAlwaysShown();   // FUN_00543fa0 0x544030: Texture Map draws its menus every frame (the bar shows by itself)
// made in texture_submodes.cpp
bool TextureWindowCommand(int32_t command);
struct LevelViewTriangle;
bool TextureMergeOn();
bool TextureMergeHoldsTheExit();
bool TextureFakeMergeOn();
void TextureMergeEnter();
void TextureMergeLeave();
void TextureMultiLeave();   // sub-mode 3 left (texture_submodes.cpp)
extern bool hierarchyPlayAll;   // the Hierarchy mode's Y switch (editor_main.cpp)
void TextureTab();   // Tab on the Texture Map screen (texture_map_screen.cpp)
extern bool textureWinRightDrag;   // the texture windows' right drag (texture_map_screen.cpp)
bool TextureWindowRightPress(int32_t x, int32_t y, int32_t windowWidth, int32_t windowHeight);
void TextureWindowRightDragTo(int32_t x, int32_t y, int32_t windowWidth, int32_t windowHeight, bool ctrl);
void TextureWindowRightRelease(int32_t windowWidth, int32_t windowHeight);
void TextureMapViewPass(bool here);   // Texture Map's view distance rule, every frame (texture_map_screen.cpp)
bool TextureMergeKey(uint8_t key);
void TextureMergeClicked(uint8_t const *id, bool isModule, int32_t moduleIndex, int32_t polygon);
bool TextureMergeFrame(std::vector<LevelViewTriangle> *triangles);
void TextureMergeLines(std::vector<std::pair<int32_t, std::string>> *lines);
namespace model { struct TextureWindowState; }
extern model::TextureWindowState textureWin;   // the texture window's layout and Texture Resize (texture_map_screen.cpp; model/texture_window.h)
void TextureWindowScale(int32_t command);   // 0xec-0xf4: the window's scale
void TextureWindowMouseAt(int32_t x, int32_t y);   // the mouse over the frame, for the scale keys
bool TextureWindowLoad();
void TextureWindowApply();
extern bool textureWinAllNodes;
extern bool textureAnimCopyPick;
extern int textureCopyObject;
bool AnimPlayStop();
void AnimPlayTick();
void TextureGoto(int direction);
extern bool textureCopyPending;
extern bool textureInvertPending;
extern int textureAsk;
void TextureSelectRect(int x0, int y0, int x1, int y1, int viewW, int viewH, bool insideOnly, bool deselect);
void TextureInvert(OpenLevelFile *open, uint8_t const *id, bool isModule, int32_t moduleIndex, bool allShapes);
void TextureWindowToggle();
void TextureMapTypedAnswer(std::string const &typed);
extern int textureWinCount;
extern int textureWinNode;
extern bool textureWinDrag;
extern int32_t textureWinFrameW, textureWinFrameH;
void TextureWindowMoveNode(int node, float du, float dv, bool all);
bool TextureWindowPress(int32_t x, int32_t y, int32_t windowWidth, int32_t windowHeight);
void TextureWindowDragTo(int32_t x, int32_t y, int32_t windowWidth, int32_t windowHeight);
void TextureWindowDraw(int32_t windowWidth, int32_t windowHeight);
void TextureFlagListMake(host::PickReport const &report);
void TextureArrowSelect(int direction, int steps, int viewW, int viewH);
extern bool textureChooserOn;
extern int textureChooserMouseX, textureChooserMouseY;
void TextureChooserDraw(int32_t windowWidth, int32_t windowHeight);
void TextureChooserClick(int x, int y);

// THE FIND ITEMS of the Misc menu (0xb7 Untextured polys, 0xb8 textured poly, 0xb9 Invalid PSX UV coords, and 0xba / 0xbb / 0xbc the same with "Find and select"; FUN_0054d260 cases 0xb7-0xbc, FUN_00561970, FUN_00561e00, FUN_00562770): the polygons
// of every shape that is not imported are listed - the ones with no picture (0xb7), the ones using the current texture (0xb8), the ones with a texture coordinate outside the picture (0xb9: for the first three corners, a number below 0 or not below
// the picture's width: FUN_00562770, 0x562909 / 0x5629df) - one for each pair of triangles (the first of a quad's two); the selecting forms also select them (FUN_0056db80). G (0xbd) goes to the next polygon of the list, Shift+G to the previous: the camera is put
// in front of the polygon, 2.5 metres out along its normal, looking at it (FUN_00563100, dist 250000 of inteng's unit). NAMED: inteng asks "N polygons haven't been done in <object>. Would you like to go to this object?" and goes to the first
// object on a yes; here the list is kept, the first polygon is gone to at once, and "All polygons have been done." is said for an empty list; the animated-texture lists (the polygons whose texture has animation frames) are not walked; the camera's look up/down
// sign is the game's pan angle (positive looks up) as read from its Euler matrix, not tried against inteng.
struct TextureFound
{
	uint8_t id[8];
	bool isModule;
	int32_t moduleIndex;
	int32_t polygon;
};

// made in texture_map_screen.cpp
extern std::vector<TextureFound> textureFound;
extern size_t textureFoundAt;
void TextureMapClicked(host::PickReport const &report, bool deselect);
void RunThePolygonMapCheck();
void RunTheFlipProof();
void SelectSomePolygonsForTheProof();
void MoveTheCentre(bool top);
void RecalculateTheNormals();

// made in editor_main.cpp
void RunAction(int32_t id);
extern int zoomStage;
extern float zoomScale;
extern int zoomCentre[2];
extern int zoomHalf;
extern ui::Box zoomViewBox;
bool ZoomPress(int x, int y, bool right);
void ZoomMove(int x, int y);
std::string HierarchyOfObject(uint8_t const *id, std::string *objectName);
std::string HexOf(const uint8_t *id);
extern size_t strategyTextureCount;
bool StrategyIconUnderClick(int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight, int32_t levelDistance, uint8_t id[8]);
extern size_t flagViewTintCount;
extern std::string strategyWords;
extern uint8_t strategyDeleteId[8];
void StabilityBegin();

// made in strategy_screen.cpp
bool StrategyReplaceableWithoutAsking(std::vector<uint8_t> const &payload);
host::ObjectEditResult WriteStrategyBytes(OpenLevelFile *open, uint8_t const *id, std::vector<uint8_t> const &bytes);
bool StrategyViewNameOf(uint8_t const *id, std::string *name);
bool StrategyViewSubject(std::vector<uint8_t> *payload, std::string *name, int32_t *kind, uint8_t *id);
bool StrategyOfObject(uint8_t const *id, int32_t *kind, std::vector<uint8_t> *payload);
std::string StrategyTargetWords(uint8_t const *id);
bool StrategyViewOpen();
void StrategyFlyTo(uint8_t const *id);
void StrategyViewPickClicked(host::PickReport const &report, int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight);
int32_t EnemySpeciesOf(uint8_t const *id);
bool LevelPathNamed(int32_t number, std::string *name);
int32_t TrackSectionsOf(uint8_t const *id);

// made in editor_proofs.cpp
void RunTheStrategyProof(long frame);
extern int proofViewWidth, proofViewHeight;
void RunTheAllKindsProof(long frame);
void FeedTheAutoLinkShow(int frames);
extern bool trackSoundOnly;
extern bool trackPoseOnly;
void RunTheTrackProof(long frame);
void RunTheModelProof(long frame);
void RunTheSClickProof(long frame);
void RunTheExitProof(long frame);
void RunTheMovementProof(long frame);

// ---- THE SECOND SPLIT (2026-11-17, Dietrich): Snyk Code reads a file only up to about 1 MiB, and editor_main.cpp had grown past it again. These are
// the names light_screen.cpp, construction_modes_screen.cpp, editor_screen_keys.cpp, strategy_data_screen.cpp, path_screen.cpp, editor_panels.cpp share with the other files: the same rules as above.

// made in editor_main.cpp
extern bool bRunning;
extern ui::MenuBar menuBar;
extern ui::MenuBar lowerBar[kEditorBarMostRows - 1];
extern size_t barRowCount;
extern EditorBarRows barRows;
extern size_t barScreen;
extern char cutOffWords[160];
extern StatusWords statusWords;   // the status line's words, a copy it owns (status_words.h)
extern KeyLayout keyLayout;
extern char layoutSettingsFile[MAX_PATH];
extern host::EditorOptions editorOptions;
bool LoadTheMenus();
extern bool keyDown[256];
bool KeyHeldNow(int vk);
bool ShiftDown();
extern int speedUnits;
extern bool running;
extern ui::MenuBar popupBar;
extern EditorBarRoom popupRoom;
extern POINT popupAt;
extern host::ToolButton toolButtons[host::kToolButtonMost];
extern host::ToolButton panelButtons[host::kToolButtonMost];
extern ui::Box toolBoxes[host::kToolButtonMost];
extern ui::Box panelBoxes[host::kToolButtonMost];
extern size_t toolPlaced;
extern size_t panelPlaced;
extern bool trackDirty;
extern host::WaypointMode waypointMode;
bool OnWaypointScreen();
bool WaypointWorldOn();
extern bool texturesSaved;
ui::MenuBar &BarRow(size_t row);
bool ALowerMenuIsOpen();
bool AMenuIsOpen();

// made in light_screen.cpp
void HarnessFaceLight(size_t light);
void HarnessPickModule(int32_t index);

// made in editor_main.cpp
void LinkClick(host::PickReport const &used, bool shift, bool held);
extern bool harnessPanelAsked;
extern host::ObjectList objectList;
extern uint32_t listSawChanges;

static int32_t const kListMostRows = 48;

// made in editor_main.cpp
extern host::FieldPanelHost fieldHost;
extern uint32_t fieldsSawChanges;
extern bool fieldsHaveKeys;
extern char theCopiesFolder[MAX_PATH];
extern char theGameFolder[MAX_PATH];
std::string MakeTheCopiesFolder();
extern double local_scale;

// made in light_screen.cpp
bool OnALightScreen();

// made in editor_main.cpp
void LetGoOfTheFieldPanel();
void ShowTheSelected(const host::Selected &selected);
extern uint32_t openClock;
extern uint32_t listOpenedAt;
extern uint32_t fieldsOpenedAt;
extern ui::Box hitLower[kEditorBarMostRows - 1];
extern ui::Box hitBar;
extern ui::Box hitList;
extern ui::Box hitFields;
extern ui::Box hitStatus;
extern int32_t promptFor;
extern std::string promptText;
extern ui::Box hitPrompt;
extern int32_t barPointerY;
extern int32_t barPointerX;
extern bool barVisible;
bool MenuLoopUp();
extern std::string adjacentBoo;
void DialogList(std::string const &title, std::vector<std::string> const &rows, std::function<void(int32_t)> const &then, std::function<void(int32_t)> const &onRight);
extern FlyCamera *theCamera;
extern OpenLevelFile *editedLevel;
extern const char *textPanelText;
extern const char *textPanelNext;
void OpenAPrompt(int32_t kind);
extern bool onHierarchy;
extern int hierarchySaveAsked;
extern std::string openLevelName;
extern host::HierarchyMode hierarchyMode;
bool IsTheHierarchyScreen(size_t screen);
void LeaveHierarchy();
void NameObjectBox(std::string const &start, std::function<void(std::string const &)> const &named);

// Every editor object's icon as it is drawn, with the object's id, for Strategy Data's click (an icon is a thing the click can land on, as inteng's display blocks are)
struct IconForPick { uint8_t id[8]; std::vector<host::IconWorldTriangle> triangles; };

// made in editor_main.cpp
void SpecialIconTriangles(std::vector<LevelViewTriangle> *triangles, uint32_t frame, bool strategy, std::vector<IconForPick> *forPick = nullptr);
extern std::vector<std::string> promptList;

// made in construction_modes_screen.cpp
void AdjacentEdit(int32_t what);

// made in editor_main.cpp
extern bool chooserActive;
extern std::string chooserText;

// made in construction_modes_screen.cpp
void ShapeChooserCommand(int32_t what, size_t *screen);
// the placement picker's chosen shape (shape_picker_screen.cpp): the name is asked, then the object made; number -1: the file is brought in (Vasquez 2026-11-23).
// known: the identity table's row of the very shape placed (shape_identity.h), or null when the table doesn't know it; the object is made as its kind says
// (ShapeIdentity_PlacedAs, the closed list), else static (typed placement, Dietrich 2026-11-23). resizedWords: when that row was found by the shape's size, not
// its SHA-256 (a resized copy of a stock pickup, shape_resized.h, Vasquez 2026-11-24), the picker's words for it, said on the status line; "" otherwise
namespace host
{
	struct ShapeIdentityRow;
}
void ShapeChooserCreateFromPicker(std::string const &name, int32_t number, host::ShapeIdentityRow const *known, std::string const &resizedWords = std::string());

// made in editor_main.cpp
void CameraPositionSaved(int slot);

// made in construction_modes_screen.cpp
bool ShapeChooserKey(WPARAM key);
void ShapeChooserPick(std::string const &typed);

// made in editor_screen_keys.cpp
void LinkExit();
void DeleteModuleLinks(bool all);

// made in construction_modes_screen.cpp
void AddReverseLinksCommand(bool all);
void AutoLinkCommand(bool all, bool deleteOld);
bool AutoLinkRunning();
void AutoLinkFrame();
void ModuleLinkingCleanTheLinks();

// made in editor_main.cpp
extern std::wstring levelSourcePath;

// made in editor_screen_keys.cpp
void DeleteLinksFrame();
bool DeleteLinksRunning();
void ShowTheWorkingBytes();
void PutTheNewObjectInPlay();   // OP-26c: a new object is in the game's view at once (ShowTheWorkingBytes, not while a live copy flies or a ghost shows)
void AutoLinkLoadFileCommand();
void GenerateAdjacency(bool newOnly);

// made in editor_main.cpp
extern std::string linkMessage;
extern std::string linkMessageShown;

// made in construction_modes_screen.cpp
void DoWaypointCommand(int32_t command, EditorHostState &host);
void MakeTheCurrentModuleCurrentAgain(int32_t current);

// made in editor_main.cpp
extern int32_t visibleDistance;

// made in construction_modes_screen.cpp
void DeleteTheModuleData();
void MergeTheSelectedObjects();
void LinkSetCurrent();

// made in editor_screen_keys.cpp
void LinkFollowOff();
bool RunScreenKey(uint8_t key, bool shift);

// made in light_screen.cpp
bool RunLightKey(int32_t command);
void LightKeysEveryFrame();

// made in editor_main.cpp
extern std::string lightMessage;
extern int lightSavePending;
extern bool lightSaveTools;
void SaveTheCopy();
void LoadTheObjectListAgain();

// made in construction_modes_screen.cpp
bool PickAnEntryMarker(int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight);
void StopFlyingTheEntryPoint(char const *why);
void FlyTheEntryPoint();
void RefreshTheEntryMarkers();
void EntryMarkerLines(std::vector<host::TrackLine> *lines);

// made in editor_main.cpp
extern bool entryFlying;
extern std::vector<host::EntryMarker> entryMarkers;
extern int entryMarkerPicked;
extern int32_t entryFlyPlace[3];

// made in editor_screen_keys.cpp
void LinkFollowPass(bool onModuleLinking);

// made in strategy_data_screen.cpp
void LinkEdit(int32_t what);
void HideObjectsCommand(int32_t what);

// made in editor_screen_keys.cpp
std::vector<uint8_t> PickedModuleIdBytes();

// made in strategy_data_screen.cpp
bool OnModuleLinkingScreen();
bool OnAdjacentModulesScreen();
host::PickReport PickTheNearestCentre(int32_t clickX, int32_t clickY, int32_t viewWidth, int32_t viewHeight, bool placedOnly = false);

// made in editor_main.cpp
void Answer(const std::string &words);

// made in strategy_data_screen.cpp
void PolyCountsCommand();

// made in editor_main.cpp
extern bool polyScreenOpen;

// made in strategy_data_screen.cpp
void PolyScreenPage(int32_t direction);
void PolyScreenPress(int32_t x, int32_t y, bool right);

// made in editor_main.cpp
bool WriteTheLinks();
extern std::vector<int32_t> showAllModules;
extern std::vector<uint32_t> showAllColours;
extern bool showAllHeld;
void ShowAllRelease();
extern bool viewDistanceOn;
extern bool adjacentViewDistanceOn;
extern int32_t viewDistance;
int32_t ViewDistanceHeld(int64_t next);
extern bool onlyDrawLinked;
extern std::vector<int32_t> linkedGameModules;
extern std::string viewWords;
int32_t ActionOfCommand(char const *table, int32_t command);
bool PlacementGravityOn();   // Object Placement's Misc > Toggle Gravity (DAT_008f4f50; editor_main.cpp): off at every start of the mode but the Shape Chooser's return (V-24)
bool PlacementLightingOn();   // Object Placement's Misc > Lighting (DAT_008f4f0c; editor_main.cpp): the placed objects with baked colours lit; off when the editor starts, then kept (OP-44)
int32_t HierarchyBarAction(int32_t command);

// made in strategy_data_screen.cpp
void UpdateModuleTints(bool onAdjacent);

// made in construction_modes_screen.cpp
void WriteTheAdjacentListThen(std::function<void(bool)> const &then, bool keepChanged = false);
void AdjacentMakeCurrentFromList();
extern bool adjacentExitHeld;

// made in editor_main.cpp
std::string ModuleNameOfIndex(int32_t index);

// made in strategy_data_screen.cpp
void ToggleModuleOverlaps();
void UpdateHiddenModules();

// made in path_screen.cpp
void PathModeCommand(int32_t command);
bool OnThePathScreen();
void RunPathPrompt(int32_t kind, std::string const &typed);

// made in editor_main.cpp
extern host::SpecialMode specialMode;
extern int placedHierarchyCurrent;
bool ModuleObject(OpenLevelFile *open, int32_t moduleIndex, uint8_t id[8]);
bool SelectedObjectId(uint8_t *id);
extern std::string quickCreateWords;
void ViewFilePlace(int32_t out[3]);
void MakePlacedObjectCurrent(uint8_t const *id);
extern std::string savedWords;
extern std::string cameraWords;
extern bool cameraSavedDraw;
extern unsigned cameraSavedDrawn;
extern bool strategyFromPlacement;
extern bool strategyMarkedChanged;
extern uint32_t strategyViewBit;
std::string SaveAtIntengsPoint();
// FUN_00403d50 with its "modules don't touch" questions (AD-2 / AD-6): every entry point worked out, one Yes / No box per pair that doesn't touch, the Yes entries and
// those naming no object taken out, then done(words) once all is written. False (done never called) when there is no level or the run could not be worked out.
// onlyModule >= 0: that module's entries alone (Adjacent's list write-back, FUN_00404040).
bool StartEntryRecalc(std::string const &who, std::function<void(std::string const &)> const &done, int32_t onlyModule = -1);

// made in light_screen.cpp
extern bool clickPicksLights;
extern std::string lightWords;
bool IsALightScreen(size_t screen);
void RelightTheView();
void LightStateOfThisLevel();
void EnterLightSourcing();
void LeaveLightSourcing();
extern host::LightState lightState;
extern int32_t lightContributionOff;

// made in editor_main.cpp
void HierarchyShow(host::HierarchyStep const &step);
OpenLevelFile *HierarchyFile();

// made in light_screen.cpp
extern int lightAttribute;
extern bool lightFlying;
bool PlacedColourDown(uint8_t const *id);
bool SelectedPlacedLight(uint8_t id[8]);
bool SelectedLight(size_t *index);

// made in editor_main.cpp
void NameInUseBox(std::string const &typed, std::function<void(std::string const &)> const &named);

// made in light_screen.cpp
void RunLightWork(int32_t work);

// made in construction_modes_screen.cpp
size_t LightMarkerUnderPointer(host::PickCamera const &camera, int32_t x, int32_t y, double *distance);

// made in light_screen.cpp
void PickTheLight(host::PickReport const &under, int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight);
void PickTheLightScreenObject(int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight);
extern std::string screenshotPending;
extern std::string screenshotWords;
bool ScreenshotNameTaken(std::string const &name, void *folder);
void TakeAScreenshot();

// made in editor_main.cpp
extern std::string shotPending;
extern std::string shotWords;
extern bool shotPrintScreenSeen;
extern bool shotHintGiven;
extern bool shotFromShiftF12;
std::string ShotShownPlace(std::string const &name);
extern host::TrackEditor trackEd;
size_t ScreenOfTable(char const *table);
bool OnAcousticScreen();
bool OnTheTrackScreen();
extern std::vector<FormRow> formRows;

// made in strategy_data_screen.cpp
bool OnStrategyScreen();

// made in editor_main.cpp
bool StrategyInASubLoop();   // Strategy Data runs one of the loops whose frame is FUN_00529370 (the flag page, Stability, the volume editor, a collector, the object pick), from a view or not
void ConfirmAs(int32_t kind, std::string const &text);
void TextAs(int32_t kind, std::string const &text, std::string const &initial);
void FormLines(std::string const &title, std::vector<std::string> *words);

// made in path_screen.cpp
void ExitCleanup(size_t screen);

// made in editor_main.cpp
bool LevelChangedSinceSave();
extern bool strategyFlagPage;
extern bool strategyPanelOn;
extern VolumeEditor volume;
extern int32_t strategyViewTop;
extern int32_t strategyViewLeft;
extern int32_t strategyViewBox[4];
// Every screen change, a bar pick's or a mode's own Exit / Tab (ShowTheScreen): was -> now, once, with barScreen already now. A mode's start and leave hooks go in it (editor_main.cpp).
void OnScreenChanged(size_t was, size_t now);
extern int32_t textAreaTop;
extern std::string strategyViewWord;
extern std::string strategyPickField;
extern std::string strategyViewPickWords;
extern int32_t strategyEditKind;
extern std::vector<std::array<uint8_t, 8>> strategyViewLinks;
extern bool strategyEditFan;
extern uint8_t strategyEditId[8];
extern bool strategyPickHave;
extern uint8_t strategyPickId[8];
extern int32_t trackPage;
extern int32_t multiPage;
extern bool requestPageOn;
extern host::RequestPageState requestPage;
extern bool copyCollecting;
extern int32_t copyPurpose;
extern std::vector<CopyTarget> copyTargets;
extern bool stabilityActive;
extern std::string strategyMessage;
extern ULONGLONG strategyMessageAt;
extern bool moveRelWaiting;
extern bool rotationPickWaiting;   // Scroll Lock's pick of the rotation centre waits for its press (editor_main.cpp, CAM-8)
extern int zoomStage;              // Insert's zoom: 0 none, 1 waiting for the centre, 2 the rectangle moves (editor_main.cpp)
void RotationCentreDefault();     // the rotation centre back on the level objects' average: every start of the hub (FUN_00470e70 from FUN_00447550, CAM-7; editor_main.cpp)
extern bool placementPolyCounts;
extern bool onTextureMap;
extern std::string textureWords;
bool IsTheTextureMapScreen(size_t screen);
void LeaveTextureMap();
extern int32_t hierarchyShownPlace[3];
extern std::vector<int32_t> hierarchySkin;
extern bool hierarchyBones;

// THE HIERARCHY VIEW'S TEXT, as inteng draws it (FUN_004889d0 0x48a07a to 0x48a96b, every frame, FUN_0057b1b0(font, x, y, format, ...)): lines at x 0, y = 16 * row, the row starting at 3 and going down by one per line
// (the first, at 200, doesn't count). The order and the words are the exe's: "(Text off)" when T has turned the text off, then, as each applies, "Play commands affect all sequences" (x 200), "Vertex Selection Mode", Offset / Rel Quat
// of the selected section, "Platform speed %.3f m/s (%.3f)", "Poly Count %d", "Use Camera Root", "Detail level %d", "Detail distance %.3fm", "Current distance %.3fm", the main sequence's name, its flags (FUN_0047dc80:
// Upper / Lower Sequence, No Interpolation, 1/2 Frame Rate), the frame flags and sounds (FUN_00478bb0: "Flag %s at 3,7", "Sound %d at %d"; the names FUN_00478f00), "Key frame :%d" (FUN_004790a0), "Timer :%d" (the
// player's position, 0 to 65535: FUN_0047dbc0 adds the frame time to it while it runs and wraps it), "Seq. Time :%.1f sec" or "... sec  (%.1f)" with its OBASEQTM time, "(Saving Seq. Times)".
// NOT A GAP (Vasquez 11-17, hierarchy-closeout-frost.md TB-5): "Variant %d of %s" (0x48a0e0-0x48a124) is drawn only when FUN_0057aa00(4) says the game kind DAT_00915104 is 4, and AvP's inteng
// only ever sets it to 0 (FUN_0057a850 0x57a918): never in AvP. NAMED GAPS (not built, the lines are missing when they would show): the delta-state lines of the other game's mode; Poly Count shows 0 (the exe's hierarchy code only ever sets it to 0). The selected section's
// Offset / Rel Quat are the constants the exe's own code leaves in them (0 and the identity: FUN_0047ef70 writes them each frame); the order of the delta players' blocks is the game's list order (newest first).
struct HierarchyTextLine
{
	int32_t x;
	int32_t row;
	std::string text;
	int32_t pixelY = -1;   // a line inteng places from the window's foot (Snap To Vertex's, at height - 60): drawn there, its row its place / 16 for the state line
	bool grayBehind = false;   // ours: drawn on the status line's gray, as wide as its words (the waypoint display's names over its dense lines, Vasquez 11-20); inteng's lines never
};

// made in editor_main.cpp
extern std::vector<HierarchyTextLine> hierarchyTextNow;
extern std::string screenTextNow;
extern std::string pathLinesNow;
void AcousticTextLines(std::vector<HierarchyTextLine> *out);
extern std::vector<std::array<uint8_t, 8>> acousticPicks;
void PlacementTextLines(std::vector<HierarchyTextLine> *out);
void TrackTextLines(std::vector<HierarchyTextLine> *out);
void WaypointTextLines(std::vector<HierarchyTextLine> *out);
void HierarchyTextLines(std::vector<HierarchyTextLine> *out);
void RunAction(int32_t id);
int32_t BarActionOfCommand(int32_t command);
extern int zoomStage;
extern int zoomCentre[2];
extern int zoomHalf;
extern ui::Box zoomViewBox;
host::PaintBox GrayBehind(int32_t x, int32_t y, int32_t width, int32_t height);
std::string HexOf(const uint8_t *id);
extern std::unique_ptr<host::LinkTree> linkTree;
extern std::vector<int32_t> linkPicks;
void LinkPicksLetGo();
std::vector<int32_t> PickedModules();

// made in path_screen.cpp
int32_t GameModuleOfNumber(int32_t number);

// made in construction_modes_screen.cpp
extern std::vector<host::ShapeChoice> chooserList;
extern size_t chooserAt;
extern int32_t chooserCameraZ;
bool ObjectNameInUse(std::string const &name);
void CreateNamedAtTheView(std::string const &typed);
extern std::unique_ptr<host::AdjacentList> adjacentList;
extern bool adjacentShowAllPaint;
bool LinkMakeCurrent(int32_t module);
extern host::AutoLinkRunner autoLinkRun;
extern int32_t autoLinkKept;
extern bool autoLinkFromFile;
extern std::string autoLinkFileWords;
extern std::string lightSpinWord;
void LightMarkerTriangles(std::vector<LevelViewTriangle> *triangles, uint32_t frame);
void WaypointBoxClick(int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight);
int32_t WaypointPickedObject(OpenLevelFile *open);

// made in editor_screen_keys.cpp
extern bool linkFollowCamera;
extern unsigned viewChanges;
extern size_t strategyTextureCount;

// made in strategy_data_screen.cpp
bool StrategyIconUnderClick(int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight, int32_t levelDistance, uint8_t id[8]);
bool StrategyIconBox(uint8_t const id[8], int32_t mins[3], int32_t maxs[3]);   // the drawn box of the icon with that ID (the volume editor's S / X on an icon, S-VOL-4)
void StrategyListClick(host::PickReport const &report, bool shift, int32_t x, int32_t y, int32_t viewWidth, int32_t viewHeight);
void StrategySelectByTexture(host::PickReport const &report);
extern bool hiddenModulesStale;
void ShotFinish(bool drawn, FlyCamera const &camera, bool levelDrawn, int windowWidth, int windowHeight);

// OBJECT PLACEMENT'S CLICK (reassess/object-placement.md OP-120; FUN_004d9af0 0x4da83b-0x4da957 calls FUN_004d6520 on a left press in state 0 when not snapping): every object of the PLACEMENT LIST that has a
// block not hidden is a candidate: the placed shapes AND the special objects (generators, bad guys, sounds, player starts, particle generators); FUN_0046ddb0 gives each the pixel distance from the pointer
// to its CENTRE projected on the screen (-1.0 behind the camera); the smallest with 0.0 <= d wins wherever it is (the best starts at 100000000.0, no nearest-enough limit); FUN_004d64a0 makes it THE current
// object (one object: no Shift, no set). No candidate in front of the view: FUN_004d64a0(0), none current. The "select by polygons" option of the Construction screen is not read here.
struct PlacementPick
{
	int what = 0;                    // 0 none, 1 a placed shape (report), 2 a special object (kind, number), 3 a placed hierarchy (number)
	host::PickReport report;
	host::SpecialKind kind = host::SpecialKind::Generator;
	size_t number = 0;
	std::string name;
};

// made in strategy_data_screen.cpp
PlacementPick PlacementClickPick(int32_t clickX, int32_t clickY, int32_t viewWidth, int32_t viewHeight);
void PlacementClickUse(PlacementPick const &pick);
extern std::vector<host::PolyCountRow> polyRows;
extern size_t polyPage;

static size_t const polyRowsPerPage = 50;

// made in strategy_data_screen.cpp
extern bool moduleOverlapsOn;
void ModuleOverlapsPicked(uint8_t const *id);   // TB-C11: a click on the hub while Module Overlaps is on (the picked module's overlaps only)
void ModuleOverlapsAll();                        // TB-C11: L on the hub while Module Overlaps is on (every module against every other)
void UpdateDummyObject();
void WaypointHidePass();
bool WaypointAllPass();   // THE WAYPOINT DISPLAY (Vasquez 11-19): mode 0 sends every AI module's alien network with the markers; true when it sent them (strategy_data_screen.cpp)
void WriteTheAsc(std::string const &typed, bool doorsClosed);

// made in path_screen.cpp
extern int32_t pathNumber;
extern std::string pathName;
extern std::vector<host::PathNode> pathNodes;
void PathTintsEveryFrame();

// made in editor_main.cpp
void PromptWords(std::string *title, std::string *hint);
void AddSpecialMarkers(const ui::PanelLayout &layout, host::PaintText *lines, size_t *lineCount, host::PaintBox *boxes, size_t *boxCount);

// made in editor_panels.cpp
PanelExtras PlanThePanels(const ui::PanelLayout &layout, host::PaintText *lines, host::PaintBox *boxes, ui::Box *titleBoxes);

// made in editor_main.cpp
int main(int, char **);

// ---- THE THIRD SPLIT (2026-11-18, Frost): editor_main.cpp was 920 KB, near the 900 KB that checks.yml stops at (Snyk Code skips a file over about 1 MiB without a
// word). These are the names special_windows.cpp, track_edit_screen.cpp, hierarchy_screen.cpp, placed_hierarchy_screen.cpp and editor_prompts.cpp share with the other
// files: the same rules as above.

// made in editor_main.cpp
extern int32_t trackSection;
const char *NotHeldWords(LevelOpenResult result);
bool ADialogIsOpen();
extern OpenLevelFile hierarchyModelFile;
extern unsigned hierarchyModelSavedGeneration;
extern DWORD hierarchyAnswerUntil;
extern bool hierarchyPromptFresh;
extern std::string hierarchyShownModel;
extern std::string hierarchyShownName;
extern std::string hierarchyTitle;
extern int32_t assignKind;
extern bool specialNameGiven;
extern std::vector<host::PlacedHierarchyInfo> placementHierarchies;

// THE NEW-SOUND DEFAULTS (DAT_005e7a60 / 64 / 68 inner, outer, volume; DAT_008f4f98 / 9c / a0 pitch, flags, probability: the constructor FUN_004ecb20 reads them): inteng's constants until P on a sound makes them
// that sound's (OP-26, FUN_004d9af0 0x4dbe1c-0x4dbf6c); every later Create > Sound takes them. The ranges are kept in the game's millimetres (inteng keeps its own units, the file's x 100 x the scale).
struct SoundDefaults
{
	bool set = false;
	double innerMm = 0.0, outerMm = 0.0;
	int32_t volume = 0, pitch = 0, flags = 0, probability = 0;
};

extern SoundDefaults soundDefaults;
void PlacementFind();
extern int32_t tearTolerance;
extern bool tearAll;
extern bool splitCentreOn;
extern bool placementGravity;
extern std::string shapeFolderSetting;   // [Editor] ShapeFolder= as the ini writes it: the lookup orders (shape_search.h) judge it each time they look
extern unsigned strategyCleanGeneration;
void StrategyCleanNow();
bool StrategyChanged();
extern std::string hierarchyWords;
extern bool hierarchyText;
extern bool hierarchyWire;
extern std::vector<int32_t> hierarchySkinFaces;
extern int hierarchyPickViewW, hierarchyPickViewH;
extern int hierarchyDetail;
void HierarchyStatus();
bool PlacementIsFlying();
bool PlacementFlyHoldsTheCopy();   // Object Placement's F is flying the object as its live copy (defined with the fly state)
bool TrackMarkerSnapping();        // V snaps the track's flying end marker now (T-3b; defined with the snap state)
size_t PlacementObjectCount();   // "Num Objects" (inteng's DAT_008f4ef4): the placement list's size, the same in Object Placement and Track Editing (T-4b; defined with the list)
bool SelectedObjectOrModuleId(uint8_t *id);
extern int32_t smallLimit;
extern std::vector<host::ObjectTypeCall> typeCalls;
extern size_t typeAsking;
void RunTypeAnswer(std::string const &typed);

// made in special_windows.cpp
extern std::string specialTitle;
extern bool specialPromptFresh;
void SimpleEditOpen();
void SpecialRunCommand(int32_t command, bool soundsScreen);
void SpecialTypedAnswer(std::string const &typed);
void SoundEditWindowOpen(host::SoundEditRecord const &record, std::string const &folder, std::string const &soundDirectory, std::function<void(host::SoundEditRecord const &, bool)> const &done);
void PlacedSoundEdit();

// made in track_edit_screen.cpp
void CopyTheTrack();
void PasteTheTrackAbsolute();
void PasteTheTrackRelative();
extern int trackFlyBase[3];
extern int32_t trackFlyOffset[3];
bool TrackPivotEditing();
extern std::vector<host::TrackLine> trackPlayBase;
extern bool trackPlayBaseWanted;
extern std::string trackEditWords;
extern int trackPromptSound;
size_t ScreenOfName(char const *name);
bool IsTheAcousticScreen(size_t screen);
void ShowTheScreen(size_t screen);
void TrackEditCommand(int32_t command);
void UpdateTrackPositions();
bool TrackEditEnter(bool followTheObject = true);
void TrackEditLeftByOtherWay();
void RunTrackPrompt(int32_t kind, std::string const &typed);
host::TrackEditResult TrackClipNow(OpenLevelFile *open, uint8_t const *id, host::TrackClip *clip);
void TrackWorldLines();
void TrackPlayFrame();

// made in hierarchy_screen.cpp
bool HierarchyCtrlKey(WPARAM key);
bool HierarchyModeKey(WPARAM key);
void EnterHierarchy();
void HierarchyClicked(host::PickReport const &report);
void HierarchySyncView(bool restart = false);
void HierarchyStepSequence(int direction);
void HierarchyRunCommand(int32_t command);
void HierarchyDialogAnswer(std::string const &typed);
void HierarchyDialogCancel();
void HierarchyTypedAnswer(std::string const &typed);
void HierarchyOpenOnSelectedObject();

// made in placed_hierarchy_screen.cpp
extern OpenLevelFile *placementRowsOpen;
extern unsigned placementRowsGeneration;
extern unsigned placementRowsFor;
extern OpenLevelFile *placedSoundsOpen;
extern unsigned placedSoundsFor;
extern unsigned placedSoundsGeneration;
std::vector<host::PlacedSoundIn> PlacedSoundsOfLevel(std::vector<host::SpecialObject> const &specials);
int32_t PlacementHideDistance();
extern bool placementHideOn;
extern int placedHierarchyShown;
extern size_t placedHierarchyRecord;
void PlacedHierarchyFrame(bool here);
void PlacedHierarchyCommand(int32_t command);

// OutsideShapes_RefreshFile's answer (editor_prompts.cpp): what the refresh of a whole level file did
enum class OutsideRefresh
{
	Changed,     // at least one outside shape changed, and the file was written with them
	Unchanged,   // nothing to change (no outside shape, none in the folder, or every one refused: the log says which)
	Refused,     // the file couldn't be opened by the game's loader, or the new file couldn't be written: it is as it was
};

// made in editor_prompts.cpp
void RunDeleteMulti(std::string const &typed);
void RunAssignPrompt(int32_t kind, std::string const &typed);
void RunDestroyPrompt(int32_t kind, std::string const &typed);
void StrategyPickClicked(host::PickReport const &report);
void RunStrategyStability(std::string const &typed);
void RunStrategyMessage(std::string const &typed);
void RunStrategyFans(std::string const &typed);
void DialogLine(std::string const &title, std::string const &label, std::string const &initial, size_t most, int32_t editHeight, std::function<void(std::string const &)> const &ok);
int OutsideShapesUpdate(OpenLevelFile *open, bool forced, std::string const &onlyObject, double *before, double *after, std::vector<std::string> *names = nullptr);
extern std::string outsideShapesWords;
std::string OutsideShapesWordsOf(std::vector<std::string> const &refreshed);
OutsideRefresh OutsideShapes_RefreshFile(const char *rifPath, std::string *why);
