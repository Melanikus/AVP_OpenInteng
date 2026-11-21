// The test for the editor's edge (R-INT-15): every Hudson hook asleep, Hudson's data, and the film and music stand-ins that answer
// quietly. Written 2026-11-14 by Melanikus (Hicks) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's
// own code".
//
// It runs none of the editor. Each check's words say what it proves. The program gives back a non-zero result if any check failed.
//
// WHY it's written by hand: tools/make-edge.js reads each Hudson hook's "off" answer out of the game's own source. This test says what
// each answer must be, as a person read it from that same source (the file and line are in each check's words). So if the script ever
// reads a hook wrongly, or the game changes a hook in a way the script reads differently, a FAILED line shows it.
//
// The void hooks are called with things they could change or would trip over, and the check is that nothing changed: with Hudson off,
// each real one returns before touching anything.
//
// The loud stand-ins (the network code, and every film or music call after an open) are never called here: each one stops the program
// with a message box. tools/make-edge.js checks that every one of them is there.

#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "hudson.h"
#include "hudson_lanes.h"
#include "hudson_plan.h"
#include "hudson_play.h"
#include "hudson_play_ribbon.h"
#include "hudson_saymarks.h"
#include "BinkDecoder.h"
#include "SmackerDecoder.h"
#include <vorbis/vorbisfile.h>
#include "nobody_watching.h"
#include "ConfigFile.h"

// Declared in no header: weapons.cpp declares it with "extern" itself (avpx/3dc/avp/weapons.cpp:2273). The linker checks this matches.
bool Hudson_SiegeTurnsAwayHit(struct strategyblock const *player, struct DAMAGE_PROFILE const *damage, int multiple);

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static bool Same(const char *text, const char *wanted)
{
	return text && strcmp(text, wanted) == 0;
}

// Hudson's data starts where the game starts it (avpx/3dc/avp/hudson.cpp:255-261), and those first values are what "asleep" rests on
static void TestHudsonsData()
{
	Check(!HudsonRunIsOn, "HudsonRunIsOn starts false (hudson.cpp:255)");
	Check(!HudsonBreaksPrisms, "HudsonBreaksPrisms starts false (hudson.cpp:256)");
	Check(!HudsonMarineIsOn, "HudsonMarineIsOn starts false (hudson.cpp:257)");
	Check(!HudsonPeopleStandStill, "HudsonPeopleStandStill starts false (hudson.cpp:258)");
	Check(HudsonWayOfDying == -1, "HudsonWayOfDying starts at -1 (hudson.cpp:259)");
	Check(!HudsonWorldIsLive, "HudsonWorldIsLive starts false (hudson.cpp:260)");
	Check(!HudsonMarineHoldsFire, "HudsonMarineHoldsFire starts false (hudson.cpp:261)");
	Check(!HudsonLanesAreOn, "HudsonLanesAreOn starts false (hudson.cpp:272)");
}

// The hooks that answer something, each with the real definition's answer when HudsonRunIsOn is false
static void TestAnswers()
{
	Check(Hudson_AccessPress() == kHudsonAccessNone, "Hudson_AccessPress gives kHudsonAccessNone (hudson.cpp:5310)");
	Check(Hudson_AccessWaitSeconds() == 0, "Hudson_AccessWaitSeconds gives 0 (hudson.cpp:5315)");
	Check(Hudson_AnswerAfterSeconds() == 0, "Hudson_AnswerAfterSeconds gives 0 (hudson.cpp:5325)");
	Check(Hudson_AnswerPress() == kHudsonAnswerNone, "Hudson_AnswerPress gives kHudsonAnswerNone (hudson.cpp:5320)");
	Check(Hudson_ClassSlider() == 0, "Hudson_ClassSlider gives 0 (hudson.cpp:1383)");
	Check(!Hudson_CustomMapHasEnded(), "Hudson_CustomMapHasEnded gives false (hudson.cpp:5251)");
	Check(Hudson_CustomMapPlace() == -1, "Hudson_CustomMapPlace gives -1 (hudson.cpp:1363)");
	Check(Hudson_Difficulty() == 0, "Hudson_Difficulty gives 0 (hudson.cpp:5768)");
	Check(Hudson_Environment() == -1, "Hudson_Environment gives -1 (hudson.cpp:1358)");
	Check(Hudson_FilmGoesOn(), "Hudson_FilmGoesOn gives TRUE, the one hook that says yes with Hudson off: the film plays on (hudson.cpp:5725)");
	Check(Same(Hudson_HostOnName(), ""), "Hudson_HostOnName gives \"\" (hudson.cpp:5359)");
	Check(Hudson_HostRow() == kHudsonHostRowNone, "Hudson_HostRow gives kHudsonHostRowNone (hudson.cpp:5352)");
	Check(!Hudson_IsASiege(), "Hudson_IsASiege gives false (hudson.cpp:5758)");
	Check(Same(Hudson_JoinOnName(), ""), "Hudson_JoinOnName gives \"\" (hudson.cpp:5364)");
	Check(!Hudson_JoinsThroughFindGames(), "Hudson_JoinsThroughFindGames gives false (hudson.cpp:5336)");
	Check(!Hudson_KeepsStill(nullptr), "Hudson_KeepsStill gives false (hudson.cpp:5555)");
	Check(!Hudson_LampCompareIsDue(), "Hudson_LampCompareIsDue gives false (hudson.cpp:1405)");
	Check(Hudson_ListPlace() == -1, "Hudson_ListPlace gives -1 (hudson.cpp:1373)");
	Check(Hudson_MarineEpisode() == -1, "Hudson_MarineEpisode gives -1 (hudson.cpp:1353)");
	Check(!Hudson_MenuPictures(), "Hudson_MenuPictures gives false (hudson.cpp:5341)");
	Check(!Hudson_NetIsCampaign(), "Hudson_NetIsCampaign gives false (hudson.cpp:8193)");
	Check(!Hudson_NetIsHost(), "Hudson_NetIsHost gives false (hudson.cpp:8375)");
	Check(!Hudson_NetIsHostile(), "Hudson_NetIsHostile gives false (hudson.cpp:8198)");
	Check(!Hudson_NetIsOn(), "Hudson_NetIsOn gives false (hudson.cpp:6861)");
	Check(!Hudson_NetPictureIsDue(), "Hudson_NetPictureIsDue gives false (hudson.cpp:8228)");
	Check(Same(Hudson_NetPictureKind(), "net"), "Hudson_NetPictureKind gives \"net\" (hudson.cpp:8215)");
	Check(!Hudson_NoFriendlyFire(), "Hudson_NoFriendlyFire gives false (hudson.cpp:5763)");
	Check(Hudson_PageHoldSeconds() == 0, "Hudson_PageHoldSeconds gives 0 (hudson.cpp:5331)");
	Check(!Hudson_PlayAsAlien(), "Hudson_PlayAsAlien gives false (hudson.cpp:1368)");
	Check(!Hudson_PlayAsPredator(), "Hudson_PlayAsPredator gives false (hudson.cpp:1378)");
	Check(!Hudson_PlaysLevelFilms(), "Hudson_PlaysLevelFilms gives false (hudson.cpp:5703)");
	Check(!Hudson_PressesAKeyAfterHeDied(), "Hudson_PressesAKeyAfterHeDied gives false (hudson.cpp:5776)");
	Check(!Hudson_SiegeTurnsAwayHit(nullptr, nullptr, 1), "Hudson_SiegeTurnsAwayHit gives false (hudson.cpp:10288)");
	Check(Hudson_YesPagePress() == kHudsonYesPageNone, "Hudson_YesPagePress gives kHudsonYesPageNone (hudson.cpp:5347)");
	Check(!HudsonPlay_RibbonIsOn(), "HudsonPlay_RibbonIsOn gives false (hudson_play.cpp:2859)");
	Check(Same(HudsonPlay_StatusLine(), ""), "HudsonPlay_StatusLine gives \"\", so the game draws no status line (hudson_play.cpp:4224)");
}

// The hooks with something to fill in: each says no and leaves it exactly as it was
static void TestNothingFilledIn()
{
	char folder[16];
	memset(folder, 'x', sizeof(folder));
	bool const said = Hudson_RunFolder(folder, sizeof(folder));
	bool untouched = true;
	for (size_t i = 0; i < sizeof(folder); i++)
	{
		untouched = untouched && folder[i] == 'x';
	}
	Check(!said && untouched, "Hudson_RunFolder gives false and writes nothing into the folder it was handed (hudson.cpp:1127)");

	int left = 11, top = 22, width = 33, height = 44;
	bool const placed = Hudson_PlaceTheWindow(&left, &top, &width, &height);
	Check(!placed && left == 11 && top == 22 && width == 33 && height == 44,
		"Hudson_PlaceTheWindow gives false and leaves the window's place and size alone (hudson.cpp:1277)");

	// The real one empties the list it's handed, even with Hudson off, so the renderer draws no ribbon
	std::vector<HudsonPlayRibbonCorner> corners(2);
	HudsonPlay_RibbonCorners(corners);
	Check(corners.empty(), "HudsonPlay_RibbonCorners empties the list it's handed (hudson_play.cpp:2864)");
}

// The hooks that answer nothing: with Hudson off, each real one returns before it touches anything, and these do nothing at all. The
// check is that every call comes back, and that Hudson's data is still where the game starts it.
static void TestDoNothing()
{
	char const said[] = "unchanged";

	Hudson_AccessRunEnds(said, true);
	Hudson_AlienStrikes(nullptr);
	Hudson_BringTheWindowForward();
	Hudson_FilmEnded();
	Hudson_FilmStarts(said);
	Hudson_GunInput(nullptr);
	Hudson_HeWasHit(nullptr, 0);
	Hudson_LooksForThePageAfterAMap(true);
	Hudson_MarineLanded(nullptr, 0, true);
	Hudson_NetInput(nullptr);
	Hudson_NetSay(said);
	Hudson_NetSayARefusedGhost(1, 2);
	Hudson_NetStopBecause(said);
	Hudson_NetWhileDead(nullptr);
	Hudson_NoteDamage(nullptr, 0);
	Hudson_PictureThisFrame(said);
	Hudson_PlayInput(nullptr);
	Hudson_PointTheCamera(nullptr, nullptr);
	Hudson_SaveLampCompareScreen(said);
	Hudson_SaveLampCompareSurface(nullptr);
	Hudson_SaveThePage(said);
	Hudson_SayWhereTheWindowWent();
	Hudson_SayWhichShaderWasCompiled(said);
	Hudson_SetTheMode();
	Hudson_StopIfTheGameWantsToAsk(said);
	Hudson_TakePictureIfDue();
	Hudson_TheLevelStartedAgain();

	Check(strcmp(said, "unchanged") == 0 && !HudsonRunIsOn && HudsonWayOfDying == -1,
		"the 27 hooks that answer nothing come back at once with Hudson off, and change none of Hudson's data (hudson.cpp: AccessRunEnds 5385, "
		"AlienStrikes 4253, BringTheWindowForward 1205, FilmEnded 5744, FilmStarts 5713, GunInput 2020, HeWasHit 4263, "
		"LooksForThePageAfterAMap 5256, MarineLanded 2498, NetInput 11112, NetSay 8380, NetSayARefusedGhost 8247, NetStopBecause 11430, "
		"NetWhileDead 8292, NoteDamage 10334, PictureThisFrame 5369, PlayInput 6781, PointTheCamera 5514, SaveLampCompareScreen 1419, "
		"SaveLampCompareSurface 1456, SaveThePage 5274, SayWhereTheWindowWent 1305, SayWhichShaderWasCompiled 1169, SetTheMode 4604, "
		"StopIfTheGameWantsToAsk 11442, TakePictureIfDue 5395, TheLevelStartedAgain 5687)");
}

// The watch-only hooks of the lane probe and of SAY marks (Frost 2026-11-21): each real one first asks whether a run of Hudson's or SAY marks
// is on, "if ((!HudsonRunIsOn && !SayMarksAreOn) || ...)" or "if (!SayMarksAreOn || HudsonRunIsOn)", and returns. In the editor neither is ever
// on: HudsonRunIsOn is the game's first value, false, and SayMarksAreOn is set only by hudson_saymarks.cpp:27, which the editor doesn't
// compile. So each comes back at once, and the words handed to SAY marks are left as they were.
static void TestWatchOnly()
{
	char const said[] = "stuck";

	HudsonLanes_AlienBefore(nullptr);
	HudsonLanes_AlienAfter(nullptr);
	HudsonLanes_HuntWent(nullptr, 1);
	HudsonLanes_Searched(nullptr, nullptr, nullptr, 1);
	HudsonSayMarks_Said(said);

	Check(strcmp(said, "stuck") == 0 && !HudsonRunIsOn && !HudsonLanesAreOn,
		"the 5 watch-only hooks come back at once with Hudson and SAY marks off, and change none of Hudson's data (hudson_lanes.cpp: AlienBefore "
		"1527, HuntWent 1584, Searched 1600, AlienAfter 1669; hudson_saymarks.cpp: Said 44)");
}

// The film and music calls that answer quietly: each answers what the real library answers for a file that won't open
static void TestQuietMedia()
{
	BinkHandle bink = Bink_Open("any.bik");
	Check(!bink.isValid && bink.instanceIndex == -1, "Bink_Open gives a handle that never opened: not valid, place -1 (BinkDecoder.cpp:86-88)");
	Bink_Close(bink);
	Check(!bink.isValid && bink.instanceIndex == -1, "Bink_Close leaves a handle that never opened as it was (BinkDecoder.cpp:135-141)");

	SmackerHandle smacker = Smacker_Open("any.smk");
	Check(!smacker.isValid && smacker.instanceIndex == -1, "Smacker_Open gives a handle that never opened: not valid, place -1 (SmackerDecoder.cpp:91-93)");
	Smacker_Close(smacker);
	Check(!smacker.isValid && smacker.instanceIndex == -1, "Smacker_Close leaves a handle that never opened as it was (SmackerDecoder.cpp:138-144)");

	// A struct full of something other than 0, so the test sees it emptied
	OggVorbis_File vorbis;
	memset(&vorbis, 0xAB, sizeof(vorbis));
	ov_callbacks callbacks;
	memset(&callbacks, 0, sizeof(callbacks));
	int datasource = 0;
	int const opened = ov_open_callbacks(&datasource, &vorbis, nullptr, 0, callbacks);

	OggVorbis_File empty;
	memset(&empty, 0, sizeof(empty));
	Check(opened < 0 && memcmp(&vorbis, &empty, sizeof(vorbis)) == 0,
		"ov_open_callbacks answers below 0 and leaves the whole OggVorbis_File empty, as libvorbis does for a file that isn't Ogg Vorbis");
	Check(ov_clear(&vorbis) == 0 && memcmp(&vorbis, &empty, sizeof(vorbis)) == 0, "ov_clear answers 0 for an empty OggVorbis_File and leaves it empty");
	Check(ov_clear(nullptr) == 0, "ov_clear answers 0 for no struct at all, as libvorbis does");
}

// The one Hudson hook the editor answers by hand (edge_own.cpp): with someone at the screen, the game's own asleep answer, no; with
// nobody there, yes, so the game's window sizing neither activates the editor's window nor brings it to the front (win_proj.cpp:684)
static void TestStayingOutOfTheWay()
{
	Check(!Hudson_StaysOutOfTheWay(), "with someone at the screen, Hudson_StaysOutOfTheWay gives the game's own asleep answer, false (hudson.cpp:1198)");
	NobodyWatching_Set();
	Check(NobodyWatching() && Hudson_StaysOutOfTheWay(), "with nobody at the screen, it gives true, so the editor's window takes nothing from anyone");

	// And with nobody at the screen no dialog is ever made. The one asked for here is never shown, so even if the guard failed, nothing
	// would appear on anyone's screen: it would be destroyed at once, and this check would fail.
	HWND const dialog = CreateWindowExA(0, "#32770", "a dialog nobody should see", 0, 0, 0, 10, 10, NULL, NULL, GetModuleHandleA(NULL), NULL);
	if (dialog)
	{
		DestroyWindow(dialog);
	}
	Check(!dialog && NobodyWatching_DialogsRefused() == 1 && strcmp(NobodyWatching_LastDialogTitle(), "a dialog nobody should see") == 0,
		"and a dialog asked for then is refused before it's made, counted, and its title kept for the run's record");
}

// The game's settings, kept by the editor in memory only (edge_own.cpp: "THE PLAYER'S GAME SETTINGS"). Each follows the game's own rule
// in ConfigFile.cpp, and none of them ever writes the player's AliensVsPredator.cfg, in the game's save folder.
static void TestSettingsInMemory()
{
	Check(Config_GetInt("[Test]", "Number", 7) == 7, "a whole number never set gives the first value asked with (ConfigFile.cpp:186-200)");
	Check(Config_GetInt("[Test]", "Number", 9) == 7, "and keeps it: asked again with another value, it gives the first");

	Config_SetInt("[Test]", "NeverAsked", 42);
	Check(Config_GetInt("[Test]", "NeverAsked", 3) == 3, "a whole number set before it was ever asked for isn't held, as in the game (ConfigFile.cpp:202-208)");
	Config_SetInt("[Test]", "NeverAsked", 42);
	Check(Config_GetInt("[Test]", "NeverAsked", 3) == 42, "once it has been asked for, setting it changes it");

	Check(Config_GetString("[Test]", "Words", "\"in quotes\"") == "\"in quotes\"", "a text never set gives the first value asked with");
	Check(Config_GetString("[Test]", "Words", "other") == "in quotes", "and, held, it's given without its quote marks (ConfigFile.cpp:218-238)");

	Check(Config_GetBool("[Test]", "YesOrNo", true), "a yes or no never set gives the first value asked with");
	Config_SetBool("[Test]", "YesOrNo", false);
	Check(!Config_GetBool("[Test]", "YesOrNo", true), "and once set to no, gives no");
	Config_SetString("[Test]", "YesOrNo", "maybe");
	Check(Config_GetBool("[Test]", "YesOrNo", true) && Config_GetString("[Test]", "YesOrNo", "") == "true",
		"a held yes or no that's neither true nor false gives the value asked with, and is set to it (ConfigFile.cpp:240-282)");

	// The player's own settings file, if this computer has one: Config_Save must leave it exactly as it was
	char documents[MAX_PATH] = { 0 };
	std::string playersFile;
	if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_MYDOCUMENTS, NULL, SHGFP_TYPE_CURRENT, documents)))
	{
		playersFile = std::string(documents) + "\My Games\Aliens versus Predator\AliensVsPredator.cfg";
	}
	WIN32_FILE_ATTRIBUTE_DATA before;
	bool const wasThere = !playersFile.empty() && GetFileAttributesExA(playersFile.c_str(), GetFileExInfoStandard, &before);
	bool const saved = Config_Save();
	WIN32_FILE_ATTRIBUTE_DATA after;
	bool const isThere = !playersFile.empty() && GetFileAttributesExA(playersFile.c_str(), GetFileExInfoStandard, &after);
	bool const untouched = wasThere == isThere && (!wasThere || (CompareFileTime(&before.ftLastWriteTime, &after.ftLastWriteTime) == 0 &&
		before.nFileSizeLow == after.nFileSizeLow && before.nFileSizeHigh == after.nFileSizeHigh));
	Check(saved && untouched, std::string("Config_Save gives true and writes nothing: the player's settings file is ") +
		(wasThere ? "unchanged, the same size and time" : "still not there"));
}

int main()
{
	TestHudsonsData();
	TestAnswers();
	TestNothingFilledIn();
	TestDoNothing();
	TestWatchOnly();
	TestQuietMedia();
	TestStayingOutOfTheWay();
	TestSettingsInMemory();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
