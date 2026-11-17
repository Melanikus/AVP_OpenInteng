// editor_shared.h: what the editor's own files share, the names one of them makes and another uses (editor_main.cpp, texture_map_screen.cpp, strategy_screen.cpp, editor_proofs.cpp).
// Written 2026-11-17 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: editor_main.cpp grew past the size Snyk Code reads (about 1,500 KB), and a file it can't read it skips without a word. So whole
// parts of it moved into files of their own, as they were: these declarations are the only new thing. A function or a variable here is
// made in the file named above it; a type or a constant is made here. They are in the order editor_main.cpp had them, so each one comes
// after what it uses. This header is included after the editor's own includes (each of the files has editor_main.cpp's list).
#pragma once

// made in editor_main.cpp
extern bool bRunning;
extern HWND editorWindow;
extern size_t barRowCount;
extern EditorBarRows barRows;
extern size_t barScreen;
extern const char *statusWords;
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
	ULONGLONG last = 0;       // the last frame's time
};

// made in editor_main.cpp
extern VolumeEditor volume;
void VolumeBegin();
extern int32_t strategyViewTop;
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
extern int textureWinScale;
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
