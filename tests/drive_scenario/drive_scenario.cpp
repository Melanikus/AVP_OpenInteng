// drive_scenario.cpp: the SCENARIO harness: a hidden run of the editor that does a mode's main job from a small text file, step by step, and writes a picture after each step. Written 2026-11-15 by
// Melanikus (Apone) for AVP_OpenInteng, at Bishop's order (Melanikus: "an MVP means IT WORKS"): a mode's MVP claim needs its scenario passing. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// It is Hicks's simulated-input harness (tests/drive_editor/drive_editor.cpp, whose parts it uses) with a script: the editor is started with -picture (hidden, nobody at the screen) on a screen,
// and every step is a window message posted to the editor's own windows, as Windows would hand them over. It never moves the real mouse, never types on the real keyboard and never brings a
// window forward; at the end it checks that the cursor and the foreground window are where they were. The editor answers six messages only in a -picture run (editor_main.cpp, kHarness*).
//
// USAGE: drive_scenario.exe <the editor's .exe> <a folder for the pictures and the record> <a scenario file>
//    or: drive_scenario.exe --shot-to-scenario <a shot .png, or a pattern with *>: prints the from-shot line and the -at line it stands for (from_shot.h); starts nothing
// It prints "ok" or "FAILED" lines, writes the pictures (picture-<n>.png, panels-<n>.png) into the record folder, and gives back 0 only if every check passed.
// IDLE_SECONDS=<n> in the environment (idle_gate.h, Frost 2026-11-22): the editor starts only when nobody has touched the machine for n seconds, else nothing
// is written and it gives back 24 (run-shards tries again later); a touch during the run is a FAILED check.
//    or: drive_scenario.exe --idle-ms: prints "idle <ms>", how long ago the machine was last touched (run-shards asks it during a run); starts nothing
//
// THE SCENARIO FILE: one step a line; a line starting with # and an empty line do nothing.
//   screen <Name>               first: the editor starts on that screen's bar (-screen), as the Screens menu names it ("Waypoint", "Module Linking", "Path")
//   level <a .rif path>         the level the editor's settings open (for verify's {orig}): only its NAME is written, as Level=, so the editor opens its data folder's avp_rifs/<name>.rif;
//                               a path whose file is not that one byte for byte is REFUSED before the editor starts, unless start-args opens that same path (Drake 11-18)
//   make-level <command>        before the editor starts: a TEST TOOL makes a level the scenario then opens (tests/scenarios/make_strategy_level.cpp: a stock copy with the strategy bytes a
//                               kind no stock level holds needs); {exedir} and {scenariodir} as in start-args. It runs fresh every run; an exit other than 0 fails the scenario and the
//                               editor is not started. (Added 2026-11-17 by Drake, strategy-closeout-drake.md E-5: made levels are made by a tool, never by hand.)
//   from-shot <png>             first: the editor starts FROM A SCREENSHOT (#78's PNG; a * in the name takes the newest that matches; {exedir} and {scenariodir} as in start-args), by
//                               its own -shot (open_at.h): the shot's map from its data folder (refused, both named, when the file's sha256 is not the shot's), the shot's screen,
//                               camera and first selected object. The PNG is hostile: every field bounded (shot_read.h). (Added 2026-11-18 by Drake, Bishop from the owner.)
//                               The same by numbers: start-args <level name> -at <x> <y> <z> <turn> <look> ["<screen>"].
//   env <AVP_NAME> <value>      before the editor starts: that variable in the editor's environment (it inherits the harness's), so a scenario carries the test hook it needs whatever runs it
//                               (hierarchy-turret-sentry: AVP_HV_TURRET pivot, hierarchy_view.cpp). Only names that start with AVP_ are taken: a scenario never changes PATH or any
//                               other variable of Windows. (Added 2026-11-17 by Frost: run-shards sets no variable, so the scenario went red in Bishop's round 1.)
//   pictures on|off             a picture after each step that changes something (on by default)
//   item <command>              does the item of the bars (first bar, then second) with that inteng command number, as a click on it does (decimal or 0x hex); FAILED if no item has it
//   expect-selected <n>         how many objects are selected (the set, with Shift+click or select-rows)
//   expect-screen <name>        the screen shown now; expect-status <words>: the status line contains these words; expect-status-not <words>; expect-prompt <n> (0 = none);
//                               expect-bar1 <title> / expect-bar2 <title>: a menu with that title is on the first / second bar
//   state                       asks the editor's state line again and prints it (every item, click and save asks for it too)
//   select-row <n> [add]        selects the n-th row of the object list (as a click on it does); with add it joins the selection
//   select-rows <n> <n> ...     the first row is selected, the others join (several objects selected, as Shift+clicks do)
//   click-win <x> <y> [right] [shift]   a click at a place in the editor window's client pixels
//   click-tile <name> [twice]   a click in the middle of the placement picker's tile of that shape (the state line's pickerboxes); twice: again after 400 ms (select, then place);
//                               a tile not on the screen is paged to first by the picker's scroll bar (pickerscroll's "below")
//   click-in <field> <name>   a click in the middle of the box <name> in a state field of "<name>~<x> <y> <w> <h>|..." (pickerboxes, pickerscroll)
//   move-in <field> <name>    the pointer moved to the middle of that box, no button (a hover)
//   drag-in <field> <name> <to name>   a drag from the middle of <name> to the middle of <to name> in that field (the picker's scroll thumb)
//   press-in <field> <name>   the left button pressed at the middle of that box and held (not let go)
//   expect-tile-drawn <name> [share]   the newest picture-<n>.png has at least that share (0.10) of that tile's picture part away from the tile's background
//   expect-tile-backdrop <name> <n> <low> <high>   that tile's bottom corners in the newest picture are low..high times as bright as in picture n (the level alone)
//   drag-win <x0> <y0> <x1> <y1> [right] [ctrl]   a drag, with the right button and / or Control held
//   drag-texwin <x0> <y0> <x1> <y1> [right] [ctrl] / click-texwin <x> <y> [shift]   the same at places from the Texture Map window's top left (the state line's texwin)
//   allow-fail <reason> | <check line>   the check as usual, but a failure is "ALLOWED" (with the reason) and not counted; a pass gets a NOTE to take the prefix away
//   expect-texwin <width> <height> <view left> <top> <right> <bottom> <scale>   the Texture Map window is that big, shows that view at that scale, in the frame's bottom-right corner
//   click-wp <index|current> <top|side|front> [right] [shift]   a click on the middle of that waypoint's box in that one of the three views (the editor says where it is: kHarnessLocate)
//   drag-win <x0> <y0> <x1> <y1>        press at the first place, move to the second in steps, let go
//   type <text>                 the characters, one WM_CHAR each, to the editor's window (into an open prompt); key <Enter|Esc|Back|Tab|Left|Right|Up|Down|Space>
//                               (F1 to F12 and PrintScreen too); key-up <k>: only the key's letting go, as Windows hands Print Screen to a window
//   drop-file <path> [<path> ...]   the files dropped on the editor, as a drag and drop from Explorer hands them over (video_drop.h; WM_COPYDATA, as WM_DROPFILES's own list)
//   click-video <name>          a click on one of Video mode's own on-screen controls, by its name (the state line's videocontrols)
//   camera <x> <y> <z> <turn>   puts the camera there (millimetres; turn 0 looks along +z, 1024 along -x (measured 11-18 against the editor's matrix; it said +x)) (kHarnessCamera); the axes: src/host/level_view.h FlyCamera
//   camera-orbit <object> [<around> <up> <distance>]   the camera ORBITING that object (or module), looking at its box's centre: 45 round, 45 up and 2.5 half diagonals off (1 to 4 metres)
//                               by default (src/host/orbit_command.h, camera_orbit.h; the owner 11-18: test cameras at 45 degrees round the subject)
//   camera-corner <object>      the four upper diagonal orbit spots (45 / 135 / 225 / 315 round, 45 up) tried in turn: the first from which the editor's own pick at the middle of the view
//                               is the subject is kept; none: the first, said. Both are sent as WM_COPYDATA text and answered with the camera line; the state line's camera field reads it.
//                               <object> is the rest of the line ("camera-corner ppump palm"), camera-orbit's last 1 to 3 plain numbers aside; a name in double quotes for one that ends in
//                               a number ("camera-orbit "door 2" 90 30"). A name not in play fails with the editor's words ("no object or module named ... is in play").
//                               "light <n>" (from 1) is the level's light n when no object or module has that name: "camera-corner light 123" (Light Sourcing's marker pick).
//   face-light <n>              Light Sourcing: selects light n (counting from 1) and turns the camera on the spot to face it (kHarnessFaceLight)
//   repeat <most> <key> <hold ms> <status_words> | <step> | <step>   the steps, again with the key held between, until the status holds the words (_ is a space): picks that depend on the camera
//   wait <milliseconds>; wait-status <most milliseconds> <words>: the state asked every half second until the status line holds the words (FAILED if it never does in that time)
//   picture <n>                 a picture now (1 to 999); the automatic ones are numbered from 101
//   expect-pixels-changed <screen|panels|picture> <x> <y> <w> <h> [share]   the newest picture against the one before: at least that share
//                               (default 0.05) of the box changed (a dialog appeared, a gun moved); expect-pixels-same ... [share]: at most
//                               that share (default 0.01) changed (nothing happened there)
//   expect-number <field> <label> <least> <most>   the number right after <label> (_ a space) in that state-line field is in the range
//   expect-numbers-near <field> <word|-> <remembered field> <word|-> <count> <within>   <count> numbers of the field now (after <word>, or its last for -)
//                               each at most <within> from the remembered field's (remember-field first): a turn kept to within rounding
//   wait-field <field> <label> <n> [<most ms>]   asks again every 100 ms until the number right after <label> (_ a space) in that field is n or more; FAILED at the limit (20000 ms)
//   wait-moving <stall ms> <field> <words>   asks again every half second until that field holds the words, for as long as the field keeps CHANGING; FAILED only
//                               when it stays the same for <stall ms> (1 to 600000), or after an hour in all, and then the scenario stops, as after a failed
//                               wait-status (moving_wait.h: a wait that follows the run)
//   A DEAD EDITOR (editor_ended.h): when the editor's process has ended, or its log (<its folder>settings\avp_log.txt) has a crash line written since
//                               the harness started it, every wait fails AT ONCE, as CRASHED or ENDED, quoting the log's crash lines and the exit code; and
//                               whatever the step, the scenario stops after it, saying so (kill-editor's end is on purpose: verify steps still run after it)
//   app-active <0|1>            the editor's window is told the program lost (0) or got (1) the focus, as Windows tells it on an Alt+Tab (WM_ACTIVATEAPP: Link with AVP reads it)
//   kill-editor                 the editor is ended at once, as a crash or Task Manager ends it (TerminateProcess): nothing it does when it closes happens; only verify steps make sense after it
//   expect-pixels-changed-since <n> <kind> <x> <y> <w> <h> [share]   the same, the newest picture against picture n (one taken with picture <n>),
//                               not the one before: dialog steps between them take their own pictures; expect-pixels-same-since likewise
//   expect-colour <kind> <x> <y> <w> <h> <rrggbb> [share] [tolerance]   that share of the box is that colour (channels within the tolerance)
//   expect-not-empty [share]    the newest picture of the 3D view has at least that share (default 0.10) of pixels away from its background
//   expect-dialog <words> / expect-no-dialog / expect-dialog-rows <a|b|...> / click-row <n>   inteng's list dialogs, from the state line's
//                               dialog / dialogrows / dialogrow1 fields (see the contract at click-row); a row is chosen by a click
//   expect-dialog-kind <list|message|confirm|text> / click-button <label> / expect-dialog-text <words>   message, confirm and text dialogs
//                               (fields dialogkind, dialogbuttons, dialogtext; contract at expect-dialog-kind); a button is clicked, never keyed
//   save-copy                   Save a copy, as File > Save a copy does; the copy's path is {copy} from here on
//   reopen-level                the open level opened again from its file, as the shapes search's reopen does (the editor's kHarnessReopen): its answer "reopen: ...";
//                               FAILED unless it says the level opened again (Dietrich 2026-11-23: the proofs of what a level change does to what is open)
//   reopen-level refused        the same ask, which must be REFUSED: FAILED unless the answer is "reopen: not opened again: ..." (the editor won't open the level
//                               again while something runs on it: a plot, AutoLink, a links delete, All stock maps; Vasquez's MUST 1 on 5422d4f3)
//   verify <program> [arguments]   runs a program (its path, then its words; {orig}, {prev} (the copy before the last one, or the level), {copy}, {exedir} (the editor's folder, with its closing backslash), {record} (the run's record folder, its pictures, likewise) and {save1} to {save9} (the first to the ninth Save a copy of the run) are replaced by the level's and the copies' paths); its exit code 0 is a pass, and its lines are shown
// A step the editor doesn't answer within its time fails, and the editor is closed.

#define DRIVE_EDITOR_PARTS_ONLY
#include "drive_editor/drive_editor.cpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <fstream>
#include <sstream>

#include "from_shot.h"   // from-shot <png>, and --shot-to-scenario (Drake 11-18)
#include "idle_gate.h"   // IDLE_SECONDS: no run while someone is at the machine (Frost 11-22)
#include "moving_wait.h" // wait-moving: a wait that follows the run, failing only when it stalls (Frost 11-23)
#include "editor_ended.h" // a dead editor fails every wait at once, as CRASHED or ENDED, in its log's own words (Frost 11-23)

static std::string Narrow(std::wstring const &text)
{
	std::string out;

	for (wchar_t const letter : text)
	{
		out.push_back(letter < 128 ? (char)letter : '?');
	}

	return out;
}

static std::wstring Widen(std::string const &text)
{
	std::wstring out;

	for (char const letter : text)
	{
		out.push_back((unsigned char)letter);
	}

	return out;
}

// The biggest level file the "level" check below reads to compare: every stock level is under 3 MB, so 256 MB is never a real one (the scenario file names it, so it is bounded too)
static std::streamoff const kLevelCheckMostBytes = 256 * 1024 * 1024;

// The whole of a file, or false when it can't be read or is bigger than most
static bool ReadWholeFile(std::string const &path, std::streamoff most, std::string *bytes)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);

	if (!file)
	{
		return false;
	}

	std::streamoff const size = file.tellg();

	if (size < 0 || size > most)
	{
		return false;
	}

	bytes->assign((size_t)size, '\0');
	file.seekg(0);
	return size == 0 || (bool)file.read(&(*bytes)[0], size);
}

// A path with every \ as / and every letter small, so two spellings of one path compare the same
static std::string SamePathSpelling(std::string text)
{
	for (char &letter : text)
	{
		letter = letter == '\\' ? '/' : (char)tolower((unsigned char)letter);
	}

	return text;
}

// A file a scenario names, as the full path Windows makes of it: false when its name holds "..", the full path is MAX_PATH long or more, or there is no plain file there
// (a folder or a link is not read). The scenario file itself is held to the same (main).
static bool PlainFileFullPath(std::string const &given, std::string *full)
{
	if (given.find("..") != std::string::npos)
	{
		return false;
	}

	char made[MAX_PATH];
	DWORD const length = GetFullPathNameA(given.c_str(), MAX_PATH, made, NULL);

	if (length == 0 || length >= MAX_PATH)
	{
		return false;
	}

	DWORD const attributes = GetFileAttributesA(made);

	if (attributes == INVALID_FILE_ATTRIBUTES || (attributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0)
	{
		return false;
	}

	*full = made;
	return true;
}

// LEVEL IS A NAME (Drake 2026-11-18, Bishop): "level <path>" writes only <path>'s name as Level= in the editor's ini, so the editor opens <its DataFolder>\avp_rifs\<name>.rif, not <path>, while
// verify's {orig} is <path>. A <path> whose file is not that one, byte for byte, would test another file than the scenario names (Drake's owner's-path walk on 11-18 ran the stock Genshd1
// where it named his own, and its AutoLink check compared the two). True when the editor will open the very file named: the data folder's file is it, byte for byte, or the editor's
// command line opens <path> itself (start-args <path>, as a double click does). Otherwise false, and why says which file the editor opens and what to write instead.
static bool LevelLineIsTheFileOpened(std::string const &level, std::string const &name, std::string const &ini, std::wstring const &startArguments, std::string *why)
{
	if (SamePathSpelling(Narrow(startArguments)).find(SamePathSpelling(level)) != std::string::npos)
	{
		return true;
	}

	char folder[MAX_PATH] = "";
	GetPrivateProfileStringA("Editor", "DataFolder", "", folder, sizeof(folder), ini.c_str());

	if (!folder[0])
	{
		*why = "the editor's settings (" + ini + ") name no DataFolder, so the file it opens for Level=" + name + " can't be checked against this one: use start-args " + level;
		return false;
	}

	std::string const opened = std::string(folder) + "\\avp_rifs\\" + name + ".rif";
	std::string named, real, levelFull;

	if (!PlainFileFullPath(level, &levelFull))
	{
		*why = "the file isn't read: its name holds \"..\", or there is no plain file there (a folder or a link isn't read)";
		return false;
	}

	if (!ReadWholeFile(levelFull, kLevelCheckMostBytes, &named))
	{
		*why = "the file can't be read (or is over 256 MB)";
		return false;
	}

	std::string openedFull;

	if (!PlainFileFullPath(opened, &openedFull) || !ReadWholeFile(openedFull, kLevelCheckMostBytes, &real) || real != named)
	{
		*why = "the editor opens " + opened + " (Level=" + name + " in its data folder), and that is not this file byte for byte (or is not there): to test this file, use start-args " +
			level + " (the double-click path), or put it in {datadir}/avp_rifs";
		return false;
	}

	return true;
}

struct Scenario
{
	Editor editor;
	std::wstring recordFolder;
	std::vector<std::string> saves;   // every Save a copy of the run, in order: {save1}, {save2}, ... in a verify line
	std::string level, copy, previous;   // previous: the copy before the last Save a copy ({prev}; the level itself before the first)
	bool pictures = true;
	int nextPicture = 101;
	int lastPicture = 0, pictureBefore = 0;   // the newest picture taken and the one before it: the behaviour checks compare these two
	std::string last;                 // the editor's last "scenario:" line
	int view[4] = {}, three[4] = {};  // the level's view and the three views' box, in the window's client pixels (x, y, width, height), from the editor's last geometry line
	bool threeViewsOn = false;
	std::map<std::string, std::string> remembered;   // remember-field: a field's words as they were
	std::map<std::string, std::array<long long, 3>> cameraNotes;   // camera-note <name>: the camera's place then
	bool dragHeld = false;            // drag-gizmo ... hold: the button is still down at heldX, heldY in heldTarget (release-gizmo lets it go)
	HWND heldTarget = nullptr;
	int heldX = 0, heldY = 0;
	bool heldShift = false;
	ULONGLONG lastAnswerMs = 0;       // how long the last state ask waited for its answer (AskState): a stuck editor answers late (expect-last-answer-within)
	std::wstring logPath;             // the editor's log: <its folder>settings\avp_log.txt (editor_main.cpp: its save folder is its own folder's settings\)
	uint64_t logRead = 0;             // how much of that log was there before this editor started, then how much the harness has read
	editor_ended::Reader logReader;   // the crash lines in what was read
	std::string gone;                 // the editor is DEAD: the words that say so (editor_ended::Words, or kill-editor's); "" while it lives
	bool goneOnPurpose = false;       // kill-editor ended it: the steps after it (verify) go on
	bool goneSaid = false;            // a failed check has said the words already
};

// The words of a field of the scenario line: <name> "<words>"
static std::string Field(std::string const &line, std::string const &name)
{
	std::string const key = name + " \"";
	size_t at = line.find("; " + key);

	if (at != std::string::npos)
	{
		at += 2;
	}
	else
	{
		at = line.find("scenario: " + key);

		if (at != std::string::npos)
		{
			at += 10;
		}
	}

	if (at == std::string::npos)
	{
		return std::string();
	}

	size_t const from = at + key.size();
	size_t const to = line.find('"', from);

	return to == std::string::npos ? std::string() : line.substr(from, to - from);
}

static int PromptOf(std::string const &line)
{
	size_t const at = line.find("; prompt ");

	return at == std::string::npos ? -1 : atoi(line.c_str() + at + 9);
}

static void PostChar(Scenario *s, wchar_t letter)
{
	PostMessageW(s->editor.window, WM_CHAR, (WPARAM)letter, 1);
	Sleep(30);
}

// What the editor printed before this is let go of, so an answer is the answer to what is asked now
static void Drain(Scenario *s)
{
	s->editor.kept.clear();
	NewLines(&s->editor);
}

// Takes the geometry line that comes with every state line
static void TakeGeometry(Scenario *s)
{
	std::string const line = WaitForLine(&s->editor, "AVP_OpenInteng: scenario-geometry: ", 3000);

	if (!line.empty())
	{
		char on[8] = { 0 };

		if (sscanf_s(line.c_str(), "AVP_OpenInteng: scenario-geometry: view %d %d %d %d; three %d %d %d %d; threeviews %7s", &s->view[0], &s->view[1], &s->view[2], &s->view[3],
			&s->three[0], &s->three[1], &s->three[2], &s->three[3], on, (unsigned)sizeof(on)) == 9)
		{
			s->threeViewsOn = strcmp(on, "on") == 0;
		}
	}
}

// waitMs: how long the editor has to answer. 10 seconds after a step; the first ask, just after the editor starts, gets longer (see where
// it's called): the editor answers only once its level is loaded, and with several editors starting at once (run-shards.sh) a big level
// took longer than 10 seconds, so the run failed at its first check although every step after it passed (Bishop, 2026-11-17).
// The editor's log, read on from where the harness got to: only its crash lines are kept (editor_ended.h). A log shorter than what was read has been
// emptied since, so all of it is newer: it is read again from its start. Shared for writing and deleting, as the running editor still writes it.
static void ReadLogOn(Scenario *s)
{
	if (s->logPath.empty())
	{
		return;
	}

	HANDLE const file = CreateFileW(s->logPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		return;
	}

	LARGE_INTEGER size = {};

	if (GetFileSizeEx(file, &size) && size.QuadPart >= 0)
	{
		if ((uint64_t)size.QuadPart < s->logRead)
		{
			s->logRead = 0;
		}

		LARGE_INTEGER at = {};
		at.QuadPart = (LONGLONG)s->logRead;
		std::vector<char> piece((size_t)1 << 16);
		DWORD got = 0;

		if (SetFilePointerEx(file, at, NULL, FILE_BEGIN))
		{
			while (s->logRead < (uint64_t)size.QuadPart && ReadFile(file, piece.data(), (DWORD)piece.size(), &got, NULL) && got > 0)
			{
				editor_ended::Take(&s->logReader, piece.data(), got);
				s->logRead += got;
			}
		}
	}

	CloseHandle(file);
}

// IS THE EDITOR DEAD (editor_ended.h)? Its process has ended, or its log has a crash line written since the harness started it. The process is
// looked at first and the log read after, so the lines of an editor that wrote them as it ended are all read. Once dead, the words stay in s->gone.
static bool EditorGone(Scenario *s)
{
	if (!s->gone.empty())
	{
		return true;
	}

	bool ended = false;
	DWORD code = 0;

	if (s->editor.process.hProcess && WaitForSingleObject(s->editor.process.hProcess, 0) == WAIT_OBJECT_0)
	{
		ended = true;
		GetExitCodeProcess(s->editor.process.hProcess, &code);
	}

	ReadLogOn(s);
	s->gone = editor_ended::Words(ended, (uint32_t)code, s->logReader);
	return !s->gone.empty();
}

static bool AskState(Scenario *s, DWORD waitMs = 10000)
{
	// a dead editor answers nothing: asked again, each ask would only wait out its time (an editor held by Windows' crash report, 10 s an ask)
	if (!s->gone.empty())
	{
		return false;
	}

	Drain(s);
	PostMessageW(s->editor.window, WM_APP + 6, 0, 0);

	ULONGLONG const asked = GetTickCount64();
	std::string const line = WaitForLine(&s->editor, "AVP_OpenInteng: scenario: ", waitMs);

	s->lastAnswerMs = GetTickCount64() - asked;   // the editor answers the ask in its next frame: a long wait here is a frame that took that long

	if (line.empty())
	{
		EditorGone(s);   // if it didn't answer because it is dead, the words are kept in s->gone for the step to say
		return false;
	}

	s->last = line;
	TakeGeometry(s);
	return true;
}

static void PictureAfter(Scenario *s, std::string const &step)
{
	if (!s->pictures)
	{
		return;
	}

	int const n = s->nextPicture++;
	bool const taken = TakePictures(&s->editor, s->recordFolder, n);

	Check(taken, "picture " + std::to_string(n) + " after: " + step);

	if (taken)
	{
		s->pictureBefore = s->lastPicture;
		s->lastPicture = n;
	}
}

// BEHAVIOUR CHECKS ON THE PICTURES (Bishop, 2026-11-16; Melanikus: "DO I SEE THE PIXELS OF A POP UP", "CLICKING THAT SHIT ACTUALLY DOES
// SOMETHING"). They look at what a person at the screen would see, never at what the editor says about itself.
// The share of the pixels of a box (x, y, width, height; clipped to the picture) whose brightness differs between two pictures of one
// size; -1 when either picture couldn't be read or the sizes differ.
static double ShareChangedIn(Picture const &a, Picture const &b, int x, int y, int width, int height)
{
	if (!a.Read() || !b.Read() || a.width != b.width || a.height != b.height)
	{
		return -1.0;
	}

	int const left = (std::max)(0, x), top = (std::max)(0, y);   // (std::max) in brackets: windows.h's max macro would take it
	int const right = (std::min)((int)a.width, x + width), bottom = (std::min)((int)a.height, y + height);

	if (right <= left || bottom <= top)
	{
		return -1.0;
	}

	size_t changed = 0;

	for (int row = top; row < bottom; row++)
	{
		for (int column = left; column < right; column++)
		{
			changed += abs(a.Brightness((UINT)column, (UINT)row) - b.Brightness((UINT)column, (UINT)row)) > 24 ? 1 : 0;
		}
	}

	return (double)changed / ((double)(right - left) * (bottom - top));
}

// The share of a picture's pixels whose brightness is more than 24 away from its most common brightness: an empty view (one clear
// colour) gives about 0, a view with things drawn in it gives more
static double ShareNotBackground(Picture const &p)
{
	if (!p.Read())
	{
		return -1.0;
	}

	std::vector<size_t> counts(766, 0);

	for (UINT row = 0; row < p.height; row++)
	{
		for (UINT column = 0; column < p.width; column++)
		{
			counts[(size_t)p.Brightness(column, row)]++;
		}
	}

	int const background = (int)(std::max_element(counts.begin(), counts.end()) - counts.begin());
	size_t away = 0;

	for (size_t level = 0; level < counts.size(); level++)
	{
		away += abs((int)level - background) > 24 ? counts[level] : 0;
	}

	return (double)away / ((double)p.width * p.height);
}

// THE PLACEMENT PICKER'S TILES (added 2026-11-23 by Vasquez): the box of the tile named `name` in the state line's pickerboxes ("<name>~<x> <y> <w> <h>|...",
// the tiles on screen now, client pixels: the picture part of each tile, without the name under it). False when no tile of that name is on screen.
static bool TileBox(std::string const &boxes, std::string const &name, int box[4])
{
	size_t from = 0;

	while (from <= boxes.size())
	{
		size_t const to = boxes.find('|', from);
		std::string const entry = boxes.substr(from, to == std::string::npos ? std::string::npos : to - from);
		size_t const tilde = entry.rfind('~');

		if (tilde != std::string::npos && entry.substr(0, tilde) == name && sscanf_s(entry.c_str() + tilde + 1, "%d %d %d %d", &box[0], &box[1], &box[2], &box[3]) == 4 &&
			box[2] > 0 && box[3] > 0)
		{
			return true;
		}

		from = to == std::string::npos ? boxes.size() + 1 : to + 1;
	}

	return false;
}

// The share of a box of a picture whose brightness is more than 24 away from the box's own top-left pixel (a tile's background): about 0 for a tile with
// nothing drawn in it, more for a shape drawn there. -1 when the picture can't be read or the box is outside it.
static double ShareDrawnInBox(Picture const &p, int const box[4])
{
	if (!p.Read() || box[0] < 0 || box[1] < 0 || box[2] <= 0 || box[3] <= 0 || (UINT)(box[0] + box[2]) > p.width || (UINT)(box[1] + box[3]) > p.height)
	{
		return -1.0;
	}

	int const background = p.Brightness((UINT)box[0], (UINT)box[1]);
	size_t away = 0;

	for (int row = box[1]; row < box[1] + box[3]; row++)
	{
		for (int column = box[0]; column < box[0] + box[2]; column++)
		{
			away += abs(p.Brightness((UINT)column, (UINT)row) - background) > 24 ? 1 : 0;
		}
	}

	return (double)away / ((double)box[2] * box[3]);
}

// The mean brightness (0 to 765) of a picker tile's two BOTTOM corners in a picture (Frost 2026-11-24): squares of an eighth of the tile's side (at least 2
// pixels), 2 pixels in from its left, right and bottom edges. Those pixels show what is behind the tile's shape: a fitted shape keeps to the circle inside the
// square, the green border is on the square's edge pixel, and a "can't be drawn" tile's words start at its top. False when the picture can't be read or the
// box isn't inside it.
static bool BottomCornersBrightness(Picture const &p, int const box[4], double *mean)
{
	int const side = (std::max)(2, (std::min)(box[2], box[3]) / 8);

	if (!p.Read() || box[0] < 0 || box[1] < 0 || box[2] < side + 4 || box[3] < side + 4 || (UINT)(box[0] + box[2]) > p.width || (UINT)(box[1] + box[3]) > p.height)
	{
		return false;
	}

	int const top = box[1] + box[3] - 2 - side;
	int const lefts[2] = { box[0] + 2, box[0] + box[2] - 2 - side };
	double sum = 0.0;
	size_t count = 0;

	for (int corner = 0; corner < 2; corner++)
	{
		for (int row = top; row < top + side; row++)
		{
			for (int column = lefts[corner]; column < lefts[corner] + side; column++)
			{
				sum += p.Brightness((UINT)column, (UINT)row);
				count++;
			}
		}
	}

	*mean = sum / (double)count;
	return true;
}

// The record folder's <kind>-<n>.png (kind: screen, panels or picture)
static Picture PictureOf(Scenario const *s, std::string const &kind, int n)
{
	return ReadPicture(s->recordFolder + L"\\" + std::wstring(kind.begin(), kind.end()) + L"-" + std::to_wstring(n) + L".png");
}

// Words of a line, split on spaces
static std::vector<std::string> Words(std::string const &line)
{
	std::vector<std::string> words;
	std::istringstream in(line);
	std::string word;

	while (in >> word)
	{
		words.push_back(word);
	}

	return words;
}

// The line from these words on, or the whole line when they are not in it (the substr of a find that fails throws std::out_of_range; Bishop 11-18)
static std::string From(std::string const &line, char const *words)
{
	size_t const at = line.find(words);

	return at == std::string::npos ? line : line.substr(at);
}

static std::string Rest(std::string const &line, size_t wordsToSkip)
{
	size_t at = 0;

	for (size_t w = 0; w < wordsToSkip; w++)
	{
		at = line.find_first_not_of(" \t", at);
		at = at == std::string::npos ? line.size() : line.find_first_of(" \t", at);
	}

	at = line.find_first_not_of(" \t", at == std::string::npos ? line.size() : at);

	return at == std::string::npos ? std::string() : line.substr(at);
}

// Windows hands a mouse message to the child window under the pointer: the level's view is a child of the editor's window, so a place inside it goes to the view, in its own pixels.
// Gives the window and turns x, y (the editor window's client pixels) into that window's.
static HWND TargetOf(Scenario *s, int *x, int *y)
{
	HWND target = s->editor.window;
	RECT viewRect = {};
	POINT origin = { 0, 0 };

	if (s->editor.view && GetWindowRect(s->editor.view, &viewRect) && ClientToScreen(s->editor.window, &origin))
	{
		int const left = viewRect.left - origin.x, top = viewRect.top - origin.y, right_ = viewRect.right - origin.x, bottom = viewRect.bottom - origin.y;

		if (*x >= left && *x < right_ && *y >= top && *y < bottom)
		{
			target = s->editor.view;
			*x -= left;
			*y -= top;
		}
	}

	return target;
}

// A press at the first place, the pointer moved to the second in steps with the button down, then let go
// holdOn: the button is NOT let go: it stays down at the second place (s->dragHeld), so a state and a picture can be taken in the middle of the drag (release-gizmo)
static void DragWindow(Scenario *s, int x0, int y0, int x1, int y1, bool right = false, bool control = false, bool shift = false, bool escapeFirst = false, bool holdOn = false)
{
	int ax = x0, ay = y0, bx = x1, by = y1;
	HWND const target = TargetOf(s, &ax, &ay);

	TargetOf(s, &bx, &by);

	// the second place is in the same window's pixels as the first: the view's origin is taken off both
	bx = x1 - (x0 - ax);
	by = y1 - (y0 - ay);

	// shift: Shift held through the drag, as the button's flags say it (the mouse gizmo's 15-degree snap, Drake 11-18)
	WPARAM const held = (right ? MK_RBUTTON : MK_LBUTTON) | (control ? MK_CONTROL : 0) | (shift ? MK_SHIFT : 0);

	PostMessageW(target, WM_MOUSEMOVE, control ? MK_CONTROL : 0, MAKELPARAM(ax, ay));
	PostMessageW(target, right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN, held, MAKELPARAM(ax, ay));
	Sleep(150);

	for (int step = 1; step <= 8; step++)
	{
		PostMessageW(target, WM_MOUSEMOVE, held, MAKELPARAM(ax + (bx - ax) * step / 8, ay + (by - ay) * step / 8));
		Sleep(80);
	}

	// escapeFirst: Esc pressed while the button is still down (the mouse gizmo gives the drag up), then the release
	if (escapeFirst)
	{
		PostMessageW(s->editor.window, WM_KEYDOWN, VK_ESCAPE, 1 | (MapVirtualKeyW(VK_ESCAPE, MAPVK_VK_TO_VSC) << 16));
		Sleep(80);
		PostMessageW(s->editor.window, WM_KEYUP, VK_ESCAPE, 1 | (MapVirtualKeyW(VK_ESCAPE, MAPVK_VK_TO_VSC) << 16) | (1u << 30) | (1u << 31));
		Sleep(150);
	}

	if (holdOn)
	{
		s->dragHeld = true;
		s->heldTarget = target;
		s->heldX = bx;
		s->heldY = by;
		s->heldShift = shift;
		Sleep(600);
		return;
	}

	PostMessageW(target, right ? WM_RBUTTONUP : WM_LBUTTONUP, control ? MK_CONTROL : 0, MAKELPARAM(bx, by));
	Sleep(600);
}

// A window-pixel click or move made while a Strategy Edit view is up (the state asked now: a step like wait asks none): a NOTE, as a view's lines are measured from the view's box
// (click-box / move-box) since the owner's box (11-17), so a scenario written for the whole-window page meant a view line, at the same numbers from the box's top left. A view is up when
// the state line's strategybox holds a box; on a build from before the box (no strategybox field at all) when its strategyview holds lines and they are not the object pick's (which is the
// whole window, its clicks on the level: "Press return when finished"). So a run of the old scenarios on an OLD build, which follows each one's own path, names exactly the clicks to move.
static void WarnWindowClickInBox(Scenario *s, char const *verb, int x, int y)
{
	AskState(s);

	bool const boxKnown = s->last.find("strategybox \"") != std::string::npos;
	std::string const boxWords = Field(s->last, "strategybox");
	std::string const viewWords = Field(s->last, "strategyview");
	bool const viewUp = boxKnown ? !boxWords.empty() : (!viewWords.empty() && viewWords.find("Press return when finished") == std::string::npos);

	if (viewUp)
	{
		printf("  NOTE  %s at %d,%d while an Edit view is up (strategybox \"%s\"): a view line's click is click-box / move-box, from the box's top left\n", verb, x, y, boxWords.c_str());
	}
}


static void ClickWindow(Scenario *s, int x, int y, bool right, bool shift, DWORD settle = 600, bool control = false)
{
	HWND const target = TargetOf(s, &x, &y);

	LPARAM const at = MAKELPARAM(x, y);
	WPARAM const held = (shift ? MK_SHIFT : 0) | (control ? MK_CONTROL : 0);
	WPARAM const keys = (right ? MK_RBUTTON : MK_LBUTTON) | held;

	PostMessageW(target, WM_MOUSEMOVE, held, at);
	PostMessageW(target, right ? WM_RBUTTONDOWN : WM_LBUTTONDOWN, keys, at);
	Sleep(60);
	PostMessageW(target, right ? WM_RBUTTONUP : WM_LBUTTONUP, held, at);
	Sleep(settle);   // the frame that answers a pick is drawn after the press, so the state is asked after it
}

// The mouse wheel turned by notches (Windows' WHEEL_DELTA, 120, a notch; positive: toward the person, which scrolls a list DOWN), the pointer in the middle of the editor's window.
// Windows hands WM_MOUSEWHEEL to the window with the keyboard's focus, and a child window that doesn't take it passes it on to its parent (DefWindowProc): so it goes to the editor's
// own window here. Added 2026-11-21 by Melanikus (Vasquez), for the combo lists that scroll.
static void WheelWindow(Scenario *s, int notches, DWORD settle = 300)
{
	RECT client = {};
	POINT middle = { 0, 0 };

	notches = notches < -200 ? -200 : (notches > 200 ? 200 : notches);   // WM_MOUSEWHEEL's delta is a short

	if (GetClientRect(s->editor.window, &client))
	{
		middle.x = client.right / 2;
		middle.y = client.bottom / 2;
	}

	ClientToScreen(s->editor.window, &middle);   // the wheel's place is in the screen's pixels
	PostMessageW(s->editor.window, WM_MOUSEWHEEL, MAKEWPARAM(0, (WORD)(short)(-120 * notches)), MAKELPARAM(middle.x, middle.y));
	Sleep(settle);   // the frame that scrolls is drawn after the message, so the state is asked after it
}

// Runs a program, capturing its lines; gives its exit code, or -1 if it didn't start
static int RunProgram(std::string const &commandLine, std::vector<std::string> *lines)
{
	SECURITY_ATTRIBUTES inherit = { sizeof(inherit), NULL, TRUE };
	HANDLE readEnd = NULL, writeEnd = NULL;

	if (!CreatePipe(&readEnd, &writeEnd, &inherit, 0))
	{
		return -1;
	}

	SetHandleInformation(readEnd, HANDLE_FLAG_INHERIT, 0);

	STARTUPINFOA start = {};
	start.cb = sizeof(start);
	start.dwFlags = STARTF_USESTDHANDLES;
	start.hStdOutput = writeEnd;
	start.hStdError = writeEnd;

	PROCESS_INFORMATION process = {};
	std::string mutableLine = commandLine;
	bool const started = CreateProcessA(NULL, &mutableLine[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &start, &process) != 0;

	CloseHandle(writeEnd);

	if (!started)
	{
		CloseHandle(readEnd);
		return -1;
	}

	std::string text;
	char buffer[4096];
	DWORD got = 0;

	while (ReadFile(readEnd, buffer, sizeof(buffer), &got, NULL) && got > 0 && text.size() < (1u << 24))
	{
		text.append(buffer, got);
	}

	CloseHandle(readEnd);
	WaitForSingleObject(process.hProcess, 300000);

	DWORD code = 99;

	GetExitCodeProcess(process.hProcess, &code);
	CloseHandle(process.hProcess);
	CloseHandle(process.hThread);

	std::istringstream in(text);
	std::string line;

	while (std::getline(in, line))
	{
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();
		}

		lines->push_back(line);
	}

	return (int)code;
}

static std::string Replace(std::string text, std::string const &from, std::string const &to)
{
	for (size_t at = text.find(from); at != std::string::npos && !from.empty(); at = text.find(from, at + to.size()))
	{
		text.replace(at, from.size(), to);
	}

	return text;
}

static UINT KeyOf(std::string const &name)
{
	if (name == "Enter") return VK_RETURN;
	if (name == "Shift") return VK_SHIFT;
	if (name == "Ctrl") return VK_CONTROL;
	if (name == "Esc") return VK_ESCAPE;
	if (name == "Back") return VK_BACK;
	if (name == "Tab") return VK_TAB;
	if (name == "Left") return VK_LEFT;
	if (name == "Right") return VK_RIGHT;
	if (name == "Up") return VK_UP;
	if (name == "Down") return VK_DOWN;
	if (name == "Space") return VK_SPACE;
	if (name == "Home") return VK_HOME;
	if (name == "Insert") return VK_INSERT;
	if (name == "Delete") return VK_DELETE;   // Waypoint by mouse's Delete (Dietrich 11-18, P4)
	// F1 to F9: the camera slots' keys (Shift+F1 saves slot 0, F1 restores it)
	if (name.size() == 2 && name[0] == 'F' && name[1] >= '1' && name[1] <= '9') return VK_F1 + (UINT)(name[1] - '1');
	if (name == "F10") return VK_F10;
	// F11 and F12 restore camera slots 10 and 11; with Shift they save nothing (FUN_004705a0); Print Screen takes the editor's screenshot (#78) (all three added 2026-11-17 by Drake)
	if (name == "F11") return VK_F11;
	if (name == "F12") return VK_F12;
	if (name == "PrintScreen") return VK_SNAPSHOT;
	if (name == "Pause") return VK_PAUSE;           // inteng's screenshot key in every mode (FUN_0058c010 case 0x13; bible-check.md M-1, Frost 11-18)
	if (name == "End") return VK_END;
	if (name == "PageDown") return VK_NEXT;
	if (name == "PageUp") return VK_PRIOR;
	if (name == "Plus") return VK_OEM_PLUS;
	if (name == "Minus") return VK_OEM_MINUS;
	if (name == "Comma") return VK_OEM_COMMA;
	if (name == "Period") return VK_OEM_PERIOD;
	// the Classic layout's keypad keys: 1 and 3 left and right, 7 and 9 roll, plus and minus (down and up)
	if (name == "Numpad1") return VK_NUMPAD1;
	if (name == "Numpad3") return VK_NUMPAD3;
	if (name == "Numpad7") return VK_NUMPAD7;
	if (name == "Numpad9") return VK_NUMPAD9;
	if (name == "Add") return VK_ADD;
	if (name == "Subtract") return VK_SUBTRACT;
	// and the turn keys 4 6 (turn), 8 2 (look), the orbit's with C held (Object Placement's R link), and '*' (level; Shift: square) and '/'
	if (name == "Numpad2") return VK_NUMPAD2;
	if (name == "Numpad4") return VK_NUMPAD4;
	if (name == "Numpad6") return VK_NUMPAD6;
	if (name == "Numpad8") return VK_NUMPAD8;
	if (name == "Multiply") return VK_MULTIPLY;
	if (name == "Divide") return VK_DIVIDE;
	if (name == "Slash") return VK_OEM_2;          // the main keyboard's '/' (inteng's "/ + Rotation Keys", camera-closeout-drake.md CAM-3)
	if (name == "ScrollLock") return VK_SCROLL;    // inteng's "Scroll Lock + Object: Set Object For Rotation Axes" (CAM-8)
	// one letter or digit is its own key (the Hierarchy view's P O N M, the key frame flags 0 to 9, Q)
	if (name.size() == 1 && ((name[0] >= 'A' && name[0] <= 'Z') || (name[0] >= '0' && name[0] <= '9'))) return (UINT)name[0];
	return 0;
}

// Does one step; gives false when the scenario can't go on
static bool DoStep(Scenario *s, std::string const &line, int number)
{
	std::vector<std::string> const w = Words(line);
	std::string const &verb = w[0];
	std::string const label = "step " + std::to_string(number) + ": " + line;

	// allow-fail <reason> | <check line> (Bishop / Dietrich 2026-11-18): the check runs as usual; a failure is ALLOWED (logged "  ALLOWED  step N: <check> (<reason>)", not counted, so
	// the file can stay GREEN), a pass is ok with a NOTE that the prefix may go. For a check that can't SEE what it looks for after a couple of camera tries (the owner's rule: allowed to
	// fail, and he looks at the picture; failed-report.js lists the ALLOWED lines on his page). A step that can't go on still stops the scenario.
	if (verb == "allow-fail")
	{
		size_t const bar = line.find(" | ");
		size_t const from = line.find("allow-fail") + 10;
		auto const trim = [](std::string text)
		{
			size_t const first = text.find_first_not_of(" \t");
			size_t const last = text.find_last_not_of(" \t");

			return first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
		};
		std::string const reason = bar == std::string::npos || bar < from ? std::string() : trim(line.substr(0, bar).substr(from));   // the words between the prefix and the bar
		std::string const check = bar == std::string::npos ? std::string() : trim(line.substr(bar + 3));

		if (reason.empty() || check.empty() || Words(check)[0] == "allow-fail")
		{
			Check(false, label + " (allow-fail <reason> | <check line>: one reason and one check)");
			return true;
		}

		checkAllowedReason = reason;
		checkAllowedPassed = 0;
		checkAllowedFailed = 0;

		bool const goOn = DoStep(s, check, number);

		checkAllowedReason.clear();

		if (checkAllowedFailed == 0 && checkAllowedPassed > 0)
		{
			printf("  NOTE  allowed-fail check passed: remove the prefix? (step %d: %s)\n", number, check.c_str());
		}

		return goOn;
	}

	if (verb == "item")
	{
		if (w.size() < 2)
		{
			Check(false, label + " (no command)");
			return false;
		}

		int const command = (int)strtol(w[1].c_str(), NULL, 0);

		Drain(s);
		PostMessageW(s->editor.window, WM_APP + 4, (WPARAM)command, 0);

		std::string const said = WaitForLine(&s->editor, "AVP_OpenInteng: item ", 10000);

		// item <command> refused: the editor must REFUSE it (no item of the bars has that command: a hidden item, Drake 11-19, the owner's "hide auto pilot"); its state is asked anew
		if (w.size() >= 3 && w[2] == "refused")
		{
			Check(said.find(" refused") != std::string::npos, label + " -> " + (said.empty() ? "no answer" : From(said, "item ")));
			AskState(s);
			PictureAfter(s, line);
			return true;
		}

		Check(said.find(" done:") != std::string::npos, label + " -> " + (said.empty() ? "no answer" : From(said, "item ")));

		// the state line the item itself asked for comes with the next frame, then its geometry line
		std::string const state = WaitForLine(&s->editor, "AVP_OpenInteng: scenario: ", 10000);

		if (state.empty())
		{
			Check(false, label + " (the editor gave no state line)");
			return false;
		}

		s->last = state;
		TakeGeometry(s);
		printf("  (%s)\n", From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	if (verb == "state")
	{
		Check(AskState(s), label);
		printf("  (%s)\n", s->last.c_str());
		return true;
	}

	// right-click-choice <words>: a RIGHT click in the middle of the list dialog's row (on the page shown) whose words hold these; the list stays open (inteng's list +0x6c handler: the
	// sound chooser's "Right click to listen to sound"). Dietrich, 2026-11-16.
	if (verb == "right-click-choice" && w.size() >= 2)
	{
		std::string const wanted = Rest(line, 1);
		std::string const choices = AskState(s) ? Field(s->last, "choices") : std::string();
		bool clicked = false;
		size_t from = 0;

		while (from < choices.size() && !clicked)
		{
			size_t const bar = choices.find('|', from);
			std::string const one = choices.substr(from, bar == std::string::npos ? std::string::npos : bar - from);
			size_t const at = one.rfind('@');
			int x = 0, y = 0, bw = 0, bh = 0;

			from = bar == std::string::npos ? choices.size() : bar + 1;

			if (at != std::string::npos && one.substr(0, at).find(wanted) != std::string::npos && sscanf_s(one.c_str() + at + 1, "%d,%d,%d,%d", &x, &y, &bw, &bh) == 4)
			{
				ClickWindow(s, x + bw / 2, y + bh / 2, true, false);
				Sleep(300);
				AskState(s);
				clicked = true;
			}
		}

		Check(clicked, label + (clicked ? "" : " (no row of the list has those words: \"" + choices.substr(0, 200) + "\")"));
		PictureAfter(s, line);
		return clicked;
	}

	// click-choice <words>: a left click on the middle of the button of the list chooser (inteng's Genfunc list dialog, list_picker.h) whose words contain these (the editor's state line gives the buttons of the page
	// shown as "choices"); FAILED when no button has them
	// click-choice-is <words>: as click-choice, but the row's words must be exactly those (case aside): "glass" is not "V In emergency break glass message 113"
	if ((verb == "click-choice" || verb == "click-choice-is") && w.size() >= 2)
	{
		bool const exact = verb == "click-choice-is";
		bool fresh = AskState(s);
		std::string const wanted = Rest(line, 1);
		std::string choices = fresh ? Field(s->last, "choices") : std::string();
		bool clicked = false;

		// a list of many rows is paged: when the row is not on the page shown, the Next button is clicked and the page searched again, until there is no Next, the pages come round to the
		// first again, or 500 pages (inteng's page since 11-18 is 640 x 480's: 18 rows of 2 columns on Temple's Select Object, which runs past the 12 pages this stopped at before; the
		// chooser opens at its first page, as inteng's FUN_00475bf0 does: Find Object's call at 0x4467e2 turns no page)
		std::string const firstPage = choices;

		for (int page = 0; page < 500 && !clicked; page++)
		{
			if (page > 0 && choices == firstPage)
			{
				break;
			}

		size_t from = 0;

		while (from < choices.size() && !clicked)
		{
			size_t const bar = choices.find('|', from);
			std::string const one = choices.substr(from, bar == std::string::npos ? std::string::npos : bar - from);
			size_t const at = one.rfind('@');

			from = bar == std::string::npos ? choices.size() : bar + 1;

			if (at == std::string::npos)
			{
				continue;
			}

			std::string lowerWords = one.substr(0, at), lowerWanted = wanted;

			for (char &c : lowerWords) { c = (char)tolower((unsigned char)c); }
			for (char &c : lowerWanted) { c = (char)tolower((unsigned char)c); }

			int x = 0, y = 0, bw = 0, bh = 0;

			if ((exact ? lowerWords == lowerWanted : lowerWords.find(lowerWanted) != std::string::npos) && sscanf_s(one.c_str() + at + 1, "%d,%d,%d,%d", &x, &y, &bw, &bh) == 4)
			{
				ClickWindow(s, x + bw / 2, y + bh / 2, false, false);
				Sleep(300);
				AskState(s);
				clicked = true;
			}
		}

		if (!clicked)
		{
			std::string const buttons = Field(s->last, "dialogbuttons");
			size_t const next = buttons.find("Next ");
			int nx = 0, ny = 0, nw = 0, nh = 0;

			if (next == std::string::npos || sscanf_s(buttons.c_str() + next + 5, "%d %d %d %d", &nx, &ny, &nw, &nh) != 4)
			{
				break;
			}

			ClickWindow(s, nx + nw / 2, ny + nh / 2, false, false);
			fresh = AskState(s);
			choices = fresh ? Field(s->last, "choices") : std::string();
		}
		}

		Check(clicked, label + (clicked ? "" : " (no button of the list chooser has those words: \"" + choices.substr(0, 200) + "\")"));
		PictureAfter(s, line);
		return clicked;
	}

	// expect-model-inside [<left> <top> <right> <bottom>]: the Hierarchy view's model is inside the level view, every one of its drawn points, with that many pixels kept free at each side (default 0 70 0 40: the
	// menu bars lie over the view's top and the status line over its bottom). The editor's state line gives the box of the projected points ("modelbox").
	if (verb == "expect-model-inside")
	{
		bool const fresh = AskState(s);
		int const left = w.size() > 1 ? atoi(w[1].c_str()) : 0, top = w.size() > 2 ? atoi(w[2].c_str()) : 70, right = w.size() > 3 ? atoi(w[3].c_str()) : 0, bottom = w.size() > 4 ? atoi(w[4].c_str()) : 40;
		std::string const box = fresh ? Field(s->last, "modelbox") : std::string();
		int x0 = 0, y0 = 0, x1 = 0, y1 = 0, vw = 0, vh = 0;

		if (!fresh || sscanf_s(box.c_str(), "%d %d %d %d %d %d", &x0, &y0, &x1, &y1, &vw, &vh) != 6)
		{
			Check(false, label + " (no box of the model: \"" + box + "\")");
			return false;
		}

		Check(x0 >= left && y0 >= top && x1 <= vw - right && y1 <= vh - bottom,
			label + " (the model's points are at x " + std::to_string(x0) + " to " + std::to_string(x1) + ", y " + std::to_string(y0) + " to " + std::to_string(y1) + " of a view " + std::to_string(vw) + " x " + std::to_string(vh) + ")");
		return true;
	}

	// camera-note <name>: remember the camera's place now (from the state line's camera field)
	if (verb == "camera-note" && w.size() >= 2)
	{
		if (!AskState(s))
		{
			Check(false, label + " (the editor gave no state line)");
			return false;
		}

		std::array<long long, 3> place = {};
		std::string const words = Field(s->last, "camera");

		Check(sscanf_s(words.c_str(), "%lld %lld %lld", &place[0], &place[1], &place[2]) == 3, label + " (camera \"" + words + "\")");
		s->cameraNotes[w[1]] = place;
		return true;
	}

	// expect-travel-ratio <a0> <a1> <b0> <b1> <least> <most>: the way the camera went from note b0 to b1 is between <least> and <most> times as long as from a0 to a1
	if (verb == "expect-travel-ratio" && w.size() >= 7)
	{
		auto const length = [&](std::string const &from, std::string const &to) -> double
		{
			auto const a = s->cameraNotes.find(from), b = s->cameraNotes.find(to);

			if (a == s->cameraNotes.end() || b == s->cameraNotes.end())
			{
				return -1;
			}

			double sum = 0;

			for (int i = 0; i < 3; i++)
			{
				double const d = (double)(b->second[i] - a->second[i]);

				sum += d * d;
			}

			return sqrt(sum);
		};

		double const first = length(w[1], w[2]), second = length(w[3], w[4]);
		double const ratio = first > 0 ? second / first : 0;

		Check(first > 0 && second >= 0 && ratio >= atof(w[5].c_str()) && ratio <= atof(w[6].c_str()), label + " (the first way is " + std::to_string((long long)first) + " mm, the second " + std::to_string((long long)second) + " mm, ratio " + std::to_string(ratio) + ")");
		return true;
	}

	// expect-travel-along <from> <to> <x|y|z>: the camera went from note <from> to note <to>, and at least nine tenths of the way was along that axis (a strafe goes sideways or up, not forward)
	if (verb == "expect-travel-along" && w.size() >= 4)
	{
		auto const a = s->cameraNotes.find(w[1]), b = s->cameraNotes.find(w[2]);
		int const axis = w[3] == "x" ? 0 : w[3] == "y" ? 1 : w[3] == "z" ? 2 : -1;

		if (a == s->cameraNotes.end() || b == s->cameraNotes.end() || axis < 0)
		{
			Check(false, label + " (no such notes, or the axis is not x, y or z)");
			return true;
		}

		double d[3], sum = 0;

		for (int i = 0; i < 3; i++)
		{
			d[i] = (double)(b->second[i] - a->second[i]);
			sum += d[i] * d[i];
		}

		double const length = sqrt(sum);

		Check(length > 0 && fabs(d[axis]) >= 0.9 * length, label + " (the camera went " + std::to_string((long long)d[0]) + " " + std::to_string((long long)d[1]) + " " + std::to_string((long long)d[2]) + " mm)");
		return true;
	}

	// expect-field <name> <words>: a field of the editor's scenario line is exactly those words (viewmode, camera, dialogkind, ...)
	if (verb == "expect-field" && w.size() >= 3)
	{
		bool const fresh = AskState(s);

		if (!fresh)
		{
			Check(false, label + " (the editor gave no state line)");
			return false;
		}

		Check(Field(s->last, w[1]) == Rest(line, 2), label + " (" + w[1] + " is \"" + Field(s->last, w[1]) + "\")");
		return true;
	}

	// expect-field-not-has <name> <words>: a field of the editor's scenario line does NOT contain the words (no full path on the screen: "dialog" and "status" never hold ":\",
	// Drake 2026-11-18, the privacy item)
	if (verb == "expect-field-not-has" && w.size() >= 3)
	{
		if (!AskState(s))
		{
			Check(false, label + " (the editor gave no state line)");
			return false;
		}

		Check(Field(s->last, w[1]).find(Rest(line, 2)) == std::string::npos, label + " (" + w[1] + " is \"" + Field(s->last, w[1]).substr(0, 300) + "\")");
		return true;
	}

	// expect-field-has <name> <words>: a field of the editor's scenario line contains the words (a drop-down's items in dropboxes, BAR-5)
	if (verb == "expect-field-has" && w.size() >= 3)
	{
		if (!AskState(s))
		{
			Check(false, label + " (the editor gave no state line)");
			return false;
		}

		Check(Field(s->last, w[1]).find(Rest(line, 2)) != std::string::npos, label + " (" + w[1] + " is \"" + Field(s->last, w[1]).substr(0, 300) + "\")");
		return true;
	}

	// expect-field-empty <name>: a field of the editor's scenario line is empty (nothing of it on the screen: the screen's top-left text while inteng's menu loop is up, BAR-8)
	if (verb == "expect-field-empty" && w.size() >= 2)
	{
		if (!AskState(s))
		{
			Check(false, label + " (the editor gave no state line)");
			return false;
		}

		Check(Field(s->last, w[1]).empty(), label + " (" + w[1] + " is \"" + Field(s->last, w[1]) + "\")");
		return true;
	}

	// expect-number <field> <label> <least> <most>: the whole number written right after the label's words (_ for a space) in that state-line field lies in the range, ends included
	// (a value the screen shows as text, e.g. "Timer :%d" of the Hierarchy rows in the field hierarchylines). A sixth word n (1 to 64) takes the label's n-th place in the field
	// (the Hierarchy text has a "Timer :" row for the main, the turret and each delta), else the first.
	// wait-field <field> <word> <n> [<most ms>] (Vasquez 11-18, Bishop's ask): asks the state line again every 100 ms until the number after <word> in that field (_ for a space,
	// as expect-number reads it: "Timer_" is the number after "Timer ") is n or more, so a timed event (a placed hierarchy's Timer, a track's play) is met by its own clock and not by a
	// guessed wait. FAILED, with the last number seen (or that the word never showed), when that has not happened within <most ms> (20000 if not given; 1 to 600000).
	// kill-editor (Vasquez 11-18, the camera file kept): the editor the harness started is ended by its process handle (never by a name), as a crash ends it, and waited for; the run's end then has no
	// editor to close. A step that asks the editor anything after this fails (it is gone); verify steps read what it left on disk.
	if (verb == "kill-editor")
	{
		bool killed = false;

		if (s->editor.process.hProcess)
		{
			killed = TerminateProcess(s->editor.process.hProcess, 98) && WaitForSingleObject(s->editor.process.hProcess, 10000) == WAIT_OBJECT_0;
			CloseHandle(s->editor.process.hProcess);
			CloseHandle(s->editor.process.hThread);
			s->editor.process = PROCESS_INFORMATION();
			s->editor.window = NULL;
			s->editor.view = NULL;
		}

		// its end is the step's own: a wait after it fails at once in these words, and the verify steps after it go on (editor_ended.h)
		s->gone = "ENDED: kill-editor ended the editor (exit code 98), as the scenario asks";
		s->goneOnPurpose = true;
		Check(killed, label + " (the editor ended at once, nothing of its closing run)");
		return killed;
	}

	// app-active <0|1> (Vasquez 11-18, Link with AVP): WM_ACTIVATEAPP with that word, posted as the other messages are (the editor's window takes it in its turn; the next state line
	// is after it). A real focus change sends the same message, so a scenario reads what the editor did (its fields), never only that it was told.
	if (verb == "app-active" && w.size() >= 2)
	{
		bool const active = w[1] == "1";

		if (!active && w[1] != "0")
		{
			Check(false, label + " (app-active takes 0 or 1)");
			return false;
		}

		PostMessageW(s->editor.window, WM_ACTIVATEAPP, active ? TRUE : FALSE, 0);
		return true;
	}

	if (verb == "wait-moving" && w.size() >= 4)
	{
		// wait-moving <stall ms> <field> <words to the line's end>: the WAIT THAT FOLLOWS THE RUN (moving_wait.h): until the field holds the words, for as long
		// as the field keeps changing; FAILED only when it stays word for word the same for <stall ms>, or after an hour in all (Frost 11-23, for a level whose
		// plot is slower than any fixed limit: Caverns, 9 minutes and moving, RED at a 300 s wait)
		long long const stall = atoll(w[1].c_str());
		std::string const words = Rest(line, 3);

		if (stall < 1 || (unsigned long long)stall > moving_wait::kMostStallMs || words.empty())
		{
			Check(false, label + " (the stall time is 1 to " + std::to_string(moving_wait::kMostStallMs) + " ms, and words must follow the field)");
			return true;
		}

		moving_wait::Wait wait = moving_wait::Begin((uint64_t)stall, GetTickCount64());

		for (;;)
		{
			bool const answered = AskState(s);

			// a DEAD editor fails the wait at once (editor_ended.h): no answer from it is not "no change", and its stall would only be sat out
			if (!answered && EditorGone(s))
			{
				Check(false, label + " (" + s->gone + ", after " + std::to_string(GetTickCount64() - wait.start) + " ms)");
				s->goneSaid = true;
				return false;
			}

			std::string const now = answered ? Field(s->last, w[2]) : std::string();
			moving_wait::Step const step = moving_wait::Look(&wait, answered, now, words, GetTickCount64());
			uint64_t const waited = GetTickCount64() - wait.start;

			if (step == moving_wait::Step::Reached)
			{
				Check(true, label + " (after " + std::to_string(waited) + " ms: " + w[2] + " is \"" + now.substr(0, 200) + "\")");
				return true;
			}

			if (step != moving_wait::Step::Keep)
			{
				Check(false, label + (step == moving_wait::Step::Stalled ? " (STALLED: " + w[2] + " didn't change for " + std::to_string(stall) + " ms" : std::string(" (an hour in all")) +
					", after " + std::to_string(waited) + " ms; it is \"" + wait.lastText.substr(0, 200) + "\")");
				return false;   // the scenario stops here, as a failed wait-status stops it: the steps after a stuck run would only wait out their own limits
			}

			Sleep(500);
		}
	}

	if (verb == "wait-field" && w.size() >= 4)
	{
		std::string wanted = w[2];

		for (char &letter : wanted)
		{
			letter = letter == '_' ? ' ' : letter;
		}

		long long const target = atoll(w[3].c_str());
		long long most = w.size() >= 5 ? atoll(w[4].c_str()) : 20000;

		if (most < 1 || most > 600000)
		{
			Check(false, label + " (the time limit is 1 to 600000 ms)");
			return true;
		}

		ULONGLONG const start = GetTickCount64();
		long long last = 0;
		bool seen = false;

		for (;;)
		{
			if (!AskState(s))
			{
				Check(false, label + (EditorGone(s) ? " (" + s->gone + ")" : std::string(" (the editor gave no state line)")));
				s->goneSaid = s->goneSaid || !s->gone.empty();
				return false;
			}

			std::string const now = Field(s->last, w[1]);
			size_t const at = now.find(wanted);
			long long const waited = (long long)(GetTickCount64() - start);

			if (at != std::string::npos)
			{
				last = atoll(now.c_str() + at + wanted.size());
				seen = true;

				if (last >= target)
				{
					Check(true, label + " (it is " + std::to_string(last) + " after " + std::to_string(waited) + " ms)");
					return true;
				}
			}

			if (waited >= most)
			{
				Check(false, label + (seen ? " (it was " + std::to_string(last) + " after " + std::to_string(waited) + " ms)" : " (\"" + wanted + "\" never showed in " + w[1] + " in " +
					std::to_string(waited) + " ms: \"" + now.substr(0, 200) + "\")"));
				return true;
			}

			Sleep(100);
		}
	}

	if (verb == "expect-number" && w.size() >= 5)
	{
		if (!AskState(s))
		{
			Check(false, label + " (the editor gave no state line)");
			return false;
		}

		std::string const now = Field(s->last, w[1]);
		std::string wanted = w[2];

		for (char &letter : wanted)
		{
			letter = letter == '_' ? ' ' : letter;
		}

		int const nth = w.size() >= 6 ? atoi(w[5].c_str()) : 1;

		if (nth < 1 || nth > 64)
		{
			Check(false, label + " (the place is 1 to 64)");
			return true;
		}

		size_t at = now.find(wanted);

		for (int i = 1; i < nth && at != std::string::npos; i++)
		{
			at = now.find(wanted, at + wanted.size());
		}
		long long const least = atoll(w[3].c_str()), most = atoll(w[4].c_str());

		if (at == std::string::npos)
		{
			Check(false, label + " (\"" + wanted + "\" is not in " + w[1] + ": \"" + now.substr(0, 200) + "\")");
			return true;
		}

		long long const value = atoll(now.c_str() + at + wanted.size());

		Check(value >= least && value <= most, label + " (it is " + std::to_string(value) + ")");
		return true;
	}

	// remember-number <field> <label> [n]: keeps the number expect-number would read; expect-number-moved <field> <label> <least> <most> [n]: the number now LESS the one kept lies in
	// the range, ends included, the difference taken as a 16-bit timer's (wrapped to -32768..32767: the Hierarchy timers run 0..65535 and wrap). For a step that depends on how many
	// frames the editor drew (a key held): what the check sees is the direction and size of the move, not the wall clock.
	if ((verb == "remember-number" && w.size() >= 3) || (verb == "expect-number-moved" && w.size() >= 5))
	{
		static long long kept = 0;
		static bool haveKept = false;
		bool const remember = verb == "remember-number";

		if (!AskState(s))
		{
			Check(false, label + " (the editor gave no state line)");
			return false;
		}

		std::string const now = Field(s->last, w[1]);
		std::string wanted = w[2];

		for (char &letter : wanted)
		{
			letter = letter == '_' ? ' ' : letter;
		}

		size_t const nthAt = remember ? 3 : 5;
		int const nth = w.size() > nthAt ? atoi(w[nthAt].c_str()) : 1;

		if (nth < 1 || nth > 64)
		{
			Check(false, label + " (the place is 1 to 64)");
			return true;
		}

		size_t at = now.find(wanted);

		for (int i = 1; i < nth && at != std::string::npos; i++)
		{
			at = now.find(wanted, at + wanted.size());
		}

		if (at == std::string::npos)
		{
			Check(false, label + " (\"" + wanted + "\" is not in " + w[1] + ": \"" + now.substr(0, 200) + "\")");
			return true;
		}

		long long const value = atoll(now.c_str() + at + wanted.size());

		if (remember)
		{
			kept = value;
			haveKept = true;
			printf("  (kept %lld)\n", value);
			return true;
		}

		long long const least = atoll(w[3].c_str()), most = atoll(w[4].c_str());
		long long const moved = ((value - kept + 32768) % 65536 + 65536) % 65536 - 32768;

		Check(haveKept && moved >= least && moved <= most, label + " (it is " + std::to_string(value) + ", kept " + std::to_string(kept) + ": moved " + std::to_string(moved) + ")");
		return true;
	}

	// remember-field <name>: keeps a field's words; expect-field-changed <name> / expect-field-same <name>: the field now differs from / equals what was kept (a face that moved, a value that stayed)
	if ((verb == "remember-field" || verb == "expect-field-changed" || verb == "expect-field-same") && w.size() >= 2)
	{
		if (!AskState(s))
		{
			Check(false, label + " (the editor gave no state line)");
			return false;
		}

		std::string const now = Field(s->last, w[1]);

		if (verb == "remember-field")
		{
			s->remembered[w[1]] = now;
			Check(true, label + " (" + w[1] + " is \"" + now + "\")");
			return true;
		}

		auto const was = s->remembered.find(w[1]);

		if (was == s->remembered.end())
		{
			Check(false, label + " (remember-field " + w[1] + " first)");
			return true;
		}

		bool const same = was->second == now;

		Check(verb == "expect-field-same" ? same : !same, label + " (" + w[1] + " was \"" + was->second + "\", is \"" + now + "\")");
		return true;
	}

	// expect-numbers-near <field> <word|-> <remembered field> <word|-> <count> <within> (Dietrich 2026-11-24, for Drake's attach: the F-flipped turn kept by the
	// left click): <count> whole numbers of the field NOW and <count> of the remembered one (remember-field first), each pair at most <within> apart. Each side's
	// numbers are the ones right after its <word> (a whole word of the field: "turn"), or its last <count> numbers for "-". A number is a word of digits, with a
	// '-' before it if negative, its ',' or ';' after it dropped. Fewer numbers than <count> on either side: a failed check, never a pass.
	if (verb == "expect-numbers-near" && w.size() >= 7)
	{
		if (!AskState(s))
		{
			Check(false, label + " (the editor gave no state line)");
			return false;
		}

		auto const was = s->remembered.find(w[3]);
		size_t const count = (size_t)atoi(w[5].c_str());
		long long const within = atoll(w[6].c_str());

		// the numbers of some words, after `word` or the last `count` of them
		auto const numbersOf = [count](std::string const &words, std::string const &word, std::vector<long long> *out) -> bool
		{
			std::vector<std::string> parts;
			std::string part;

			for (char c : words + " ")
			{
				if (c == ' ' || c == '|')
				{
					while (!part.empty() && (part.back() == ',' || part.back() == ';'))
					{
						part.pop_back();
					}

					if (!part.empty())
					{
						parts.push_back(part);
					}

					part.clear();
				}
				else
				{
					part += c;
				}
			}

			auto const isNumber = [](std::string const &p) {
				size_t const start = !p.empty() && p[0] == '-' ? 1 : 0;

				return p.size() > start && p.size() - start <= 18 && p.find_first_not_of("0123456789", start) == std::string::npos;
			};

			std::vector<long long> all;
			size_t from = 0;

			if (word != "-")
			{
				size_t k = 0;

				while (k < parts.size() && parts[k] != word)
				{
					k++;
				}

				if (k == parts.size())
				{
					return false;
				}

				from = k + 1;
			}

			for (size_t k = from; k < parts.size() && (word == "-" || all.size() < count); k++)
			{
				if (isNumber(parts[k]))
				{
					all.push_back(atoll(parts[k].c_str()));
				}
				else if (word != "-")
				{
					break;   // the numbers right after the word end at its first word that isn't one
				}
			}

			if (all.size() < count || count == 0)
			{
				return false;
			}

			out->assign(all.end() - (std::ptrdiff_t)count, all.end());

			if (word != "-")
			{
				out->assign(all.begin(), all.begin() + (std::ptrdiff_t)count);
			}

			return true;
		};

		std::string const now = Field(s->last, w[1]);
		std::vector<long long> a, b;
		bool const haveNow = numbersOf(now, w[2], &a);
		bool const haveWas = was != s->remembered.end() && numbersOf(was->second, w[4], &b);
		bool allNear = haveNow && haveWas;

		for (size_t k = 0; allNear && k < count; k++)
		{
			allNear = (a[k] > b[k] ? a[k] - b[k] : b[k] - a[k]) <= within;
		}

		Check(allNear, label + " (" + w[1] + " is \"" + now + "\"; " + w[3] + " was \"" + (was == s->remembered.end() ? std::string("not remembered") : was->second) + "\")");
		return true;
	}

	// expect-away <field> <remembered field> <least> <most>: the place (its first three numbers) the field says now lies at least <least> and at most <most> (the field's units)
	// from the place the remembered field held (remember-field first). Drake 2026-11-19: DURING a gizmo drag, the live copy (gizmolive) is away from where the object was
	// (gizmodrawn remembered before it), so the object is drawn moving, not only the gizmo
	if (verb == "expect-away" && w.size() >= 5)
	{
		if (!AskState(s))
		{
			Check(false, label + " (the editor gave no state line)");
			return false;
		}

		std::string const now = Field(s->last, w[1]);
		auto const was = s->remembered.find(w[2]);
		double a[3] = {}, b[3] = {};

		if (was == s->remembered.end())
		{
			Check(false, label + " (remember-field " + w[2] + " first)");
			return true;
		}

		if (sscanf_s(now.c_str(), "%lf %lf %lf", &a[0], &a[1], &a[2]) != 3 || sscanf_s(was->second.c_str(), "%lf %lf %lf", &b[0], &b[1], &b[2]) != 3)
		{
			Check(false, label + " (no place: " + w[1] + " is \"" + now + "\", " + w[2] + " was \"" + was->second + "\")");
			return true;
		}

		double const away = sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));

		Check(away >= atof(w[3].c_str()) && away <= atof(w[4].c_str()), label + " (" + std::to_string((long long)floor(away + 0.5)) + " away: " + w[1] + " is \"" + now + "\", " +
			w[2] + " was \"" + was->second + "\")");
		return true;
	}

	// expect-near <field> <x> <y> <z> <most>: the place (its first three numbers) the field says now lies at most <most> from x y z (the field's units). Drake 2026-11-19: on the split
	// screen, the centre ball's placement lands where the same drag lands in the full view
	if (verb == "expect-near" && w.size() >= 6)
	{
		if (!AskState(s))
		{
			Check(false, label + " (the editor gave no state line)");
			return false;
		}

		std::string const now = Field(s->last, w[1]);
		double a[3] = {};

		if (sscanf_s(now.c_str(), "%lf %lf %lf", &a[0], &a[1], &a[2]) != 3)
		{
			Check(false, label + " (no place: " + w[1] + " is \"" + now + "\")");
			return true;
		}

		double const dx = a[0] - atof(w[2].c_str()), dy = a[1] - atof(w[3].c_str()), dz = a[2] - atof(w[4].c_str());
		double const away = sqrt(dx * dx + dy * dy + dz * dz);

		Check(away <= atof(w[5].c_str()), label + " (" + std::to_string((long long)floor(away + 0.5)) + " away: " + w[1] + " is \"" + now + "\")");
		return true;
	}

	if (verb == "expect-screen" || verb == "expect-selected" || verb == "expect-status" || verb == "expect-status-not" || verb == "expect-prompt" || verb == "expect-bar1" || verb == "expect-bar2")
	{
		bool const fresh = AskState(s);
		std::string const want = Rest(line, 1);

		if (!fresh)
		{
			Check(false, label + " (the editor gave no state line)");
			return false;
		}

		if (verb == "expect-screen")
		{
			Check(Field(s->last, "screen") == want, label + " (it is \"" + Field(s->last, "screen") + "\")");
		}
		else if (verb == "expect-selected")
		{
			Check(Field(s->last, "selected") == want, label + " (" + Field(s->last, "selected") + " objects are selected)");
		}
		else if (verb == "expect-status")
		{
			Check(Field(s->last, "status").find(want) != std::string::npos, label + " (the status says \"" + Field(s->last, "status") + "\")");
		}
		else if (verb == "expect-status-not")
		{
			Check(Field(s->last, "status").find(want) == std::string::npos, label + " (the status says \"" + Field(s->last, "status") + "\")");
		}
		else if (verb == "expect-prompt")
		{
			Check(PromptOf(s->last) == atoi(want.c_str()), label + " (the prompt is " + std::to_string(PromptOf(s->last)) + ")");
		}
		else
		{
			std::string const bar = Field(s->last, verb == "expect-bar1" ? "bar1" : "bar2");

			Check(("|" + bar + "|").find("|" + want + "|") != std::string::npos, label + " (the bar has \"" + bar + "\")");
		}

		return true;
	}

	// select-rows <n> <n> ...: the first row is selected, the others JOIN the selection (as Shift+clicks on the rows do); select-row <n> add: one more joins
	if (verb == "select-rows" && w.size() >= 2)
	{
		for (size_t i = 1; i < w.size(); i++)
		{
			PostMessageW(s->editor.window, WM_APP + 3, (WPARAM)atoi(w[i].c_str()), i > 1 ? 1 : 0);
			Sleep(300);
		}

		Sleep(600);
		WaitForLine(&s->editor, "AVP_OpenInteng: panel", 200);
		Check(true, label);
		PictureAfter(s, line);
		return true;
	}

	if (verb == "select-row" && w.size() >= 2)
	{
		PostMessageW(s->editor.window, WM_APP + 3, (WPARAM)atoi(w[1].c_str()), w.size() >= 3 && w[2] == "add" ? 1 : 0);
		Sleep(600);
		WaitForLine(&s->editor, "AVP_OpenInteng: panel", 200);
		Check(true, label);
		PictureAfter(s, line);
		return true;
	}

	if (verb == "pick-poly" && w.size() >= 3)
	{
		// pick-poly <object row from 0> <polygon number> [deselect]: what a click on that polygon does in Texture Map, by number (the object list's row order: tests/scenarios/shape_pairs prints it)
		bool const deselect = std::find(w.begin(), w.end(), "deselect") != w.end();

		Drain(s);
		PostMessageW(s->editor.window, WM_APP + 13, (WPARAM)atoi(w[1].c_str()), (LPARAM)((unsigned)atoi(w[2].c_str()) | (deselect ? 0x40000000u : 0u)));
		Sleep(400);
		AskState(s);
		Check(true, label);
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		return true;
	}

	// CLICK-BAR (Frost 11-16, for the acceptance tests that take HIS path): the pointer goes to the top edge (the bar is drawn only there), the title is clicked where the state line's barboxes says it is, and the item
	// where the open drop-down's dropboxes says it is.   click-bar <title>                 opens the menu     click-bar <title> > <item>      opens it and picks the item (exact words of the bar)
	if (verb == "click-bar" && w.size() >= 2)
	{
		std::string const wanted = Rest(line, 1);
		size_t const arrow = wanted.find(" > ");
		std::string const title = arrow == std::string::npos ? wanted : wanted.substr(0, arrow);
		std::string const item = arrow == std::string::npos ? std::string() : wanted.substr(arrow + 3);
		int ox = 400, oy = 1;
		HWND const edge = TargetOf(s, &ox, &oy);

		PostMessageW(edge, WM_MOUSEMOVE, 0, MAKELPARAM(ox, oy));
		Sleep(600);
		AskState(s);

		auto boxOf = [](std::string const &list, std::string const &name, int *box) -> bool
		{
			size_t from = 0;

			while (from <= list.size())
			{
				size_t const to = list.find('|', from);
				std::string const entry = list.substr(from, to == std::string::npos ? std::string::npos : to - from);
				size_t const tilde = entry.rfind('~');

				if (tilde != std::string::npos && entry.substr(0, tilde) == name && sscanf_s(entry.c_str() + tilde + 1, "%d %d %d %d", &box[0], &box[1], &box[2], &box[3]) == 4)
				{
					return true;
				}

				if (to == std::string::npos)
				{
					break;
				}

				from = to + 1;
			}

			return false;
		};

		int box[4] = {};

		if (!boxOf(Field(s->last, "barboxes"), title, box))
		{
			Check(false, label + " (no title \"" + title + "\" on the bar: " + Field(s->last, "barboxes") + ")");
			return true;
		}

		ClickWindow(s, box[0] + box[2] / 2, box[1] + box[3] / 2, false, false);
		AskState(s);

		if (!item.empty())
		{
			// A CASCADE's row ("Strategy Data: Normal", reassess/dialog-look-drake.md BAR-10..12): its item is pointed at first (no press), which shows the cascade, and the state line then names the
			// cascade's rows "<item>: <row>"; the click is on that row
			size_t const colon = item.find(": ");

			if (!boxOf(Field(s->last, "dropboxes"), item, box) && colon != std::string::npos && boxOf(Field(s->last, "dropboxes"), item.substr(0, colon), box))
			{
				int px = box[0] + box[2] / 2, py = box[1] + box[3] / 2;
				HWND const target = TargetOf(s, &px, &py);

				PostMessageW(target, WM_MOUSEMOVE, 0, MAKELPARAM(px, py));
				Sleep(400);
				AskState(s);
			}

			if (!boxOf(Field(s->last, "dropboxes"), item, box))
			{
				Check(false, label + " (no item \"" + item + "\" in the drop-down: " + Field(s->last, "dropboxes") + ")");
				return true;
			}

			ClickWindow(s, box[0] + box[2] / 2, box[1] + box[3] / 2, false, false);
			AskState(s);
		}

		Check(true, label);
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	// point-bar <row>: with a drop-down open, the pointer moves (no button) to the middle of the row of that name the state line lists in dropboxes, as a person points at it (a CASCADE's item
	// shows its cascade so, BAR-12); FAILED when the open drop-down has no row of that name
	if (verb == "point-bar" && w.size() >= 2)
	{
		std::string const wanted = Rest(line, 1);

		AskState(s);

		std::string const list = Field(s->last, "dropboxes");
		size_t from = 0;
		int box[4] = {};
		bool found = false;

		while (!found && from <= list.size())
		{
			size_t const to = list.find('|', from);
			std::string const entry = list.substr(from, to == std::string::npos ? std::string::npos : to - from);
			size_t const tilde = entry.rfind('~');

			found = tilde != std::string::npos && entry.substr(0, tilde) == wanted && sscanf_s(entry.c_str() + tilde + 1, "%d %d %d %d", &box[0], &box[1], &box[2], &box[3]) == 4;
			from = to == std::string::npos ? list.size() + 1 : to + 1;
		}

		if (!found)
		{
			Check(false, label + " (no row \"" + wanted + "\" in the open drop-down: " + list.substr(0, 300) + ")");
			return true;
		}

		int px = box[0] + box[2] / 2, py = box[1] + box[3] / 2;
		HWND const target = TargetOf(s, &px, &py);

		PostMessageW(target, WM_MOUSEMOVE, 0, MAKELPARAM(px, py));
		Sleep(400);
		AskState(s);
		Check(true, label);
		PictureAfter(s, line);
		return true;
	}

	// THE BAR IS HIDDEN (Vasquez; inteng's FUN_004b7c50 shows it only while the pointer is at the top edge, y < 3, or Control is held): the state line carries barshown "1" or "0".
	//   move-win <x> <y>      the pointer moves there (no button), and the editor draws a frame
	//   expect-bar-shown      the bar is drawn now      expect-bar-hidden    it is not
	// resize-win <width> <height>: the editor window's OUTER size set to this (the device is Reset to the new back buffer, as when a person drags the window's edge), then the state and a picture
	if (verb == "resize-win" && w.size() >= 3)
	{
		SetWindowPos(s->editor.window, 0, 0, 0, atoi(w[1].c_str()), atoi(w[2].c_str()), SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
		Sleep(1500);
		AskState(s);
		Check(true, label);
		PictureAfter(s, line);
		return true;
	}

	// STRATEGY'S EDIT VIEW BOX (the owner's addition, 11-17: a kind's Edit view, its pages and E's pages are drawn in a box centred in the window, at least inteng's 640 x 480, so its lines sit
	// where inteng laid them out from the box's top left): the state line carries strategybox "<x> <y> <w> <h>" (the window's client pixels, empty with no box).
	//   click-box <x> <y> [right] [shift]   a click at (x, y) from the box's top left: the numbers inteng's 640 x 480 page used
	//   move-box <x> <y>                    the pointer moved there (a view that reads the pointer's row: the Mission's P / V, the Multi switch's rows)
	//   expect-box-centred <least w> <least h>   a box is up, centred in the window (within a pixel), at least that big, half the window or more each way and never past it
	if ((verb == "click-box" || verb == "move-box") && w.size() >= 3)
	{
		AskState(s);

		std::string const boxWords = Field(s->last, "strategybox");
		int box[4] = {};

		if (sscanf_s(boxWords.c_str(), "%d %d %d %d", &box[0], &box[1], &box[2], &box[3]) != 4 || box[2] < 8 || box[3] < 8)
		{
			Check(false, label + " (no Edit view box is up: strategybox \"" + boxWords + "\")");
			return false;
		}

		int x = box[0] + atoi(w[1].c_str()), y = box[1] + atoi(w[2].c_str());

		if (verb == "click-box")
		{
			ClickWindow(s, x, y, std::find(w.begin(), w.end(), "right") != w.end(), std::find(w.begin(), w.end(), "shift") != w.end());
		}
		else
		{
			HWND const target = TargetOf(s, &x, &y);

			PostMessageW(target, WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
			Sleep(600);
		}

		AskState(s);
		Check(true, label + " at " + std::to_string(box[0] + atoi(w[1].c_str())) + "," + std::to_string(box[1] + atoi(w[2].c_str())));
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	// click-in <field> <name>: a press at the middle of the box <name> in a state field of "<name>~<x> <y> <w> <h>|..." (window pixels: pickerboxes, pickerscroll);
	// drag-in <field> <name> <to name>: a drag from the middle of <name> to the middle of <to name> in that same field (Vasquez 2026-11-23: the placement picker's
	// scroll bar, its arrows, its track and its dragged thumb, wherever the editor says they are)
	// press-in <field> <name>: the left button pressed at the middle of that box and HELD (not let go: the next steps see it held; a later click lets it go)
	// move-in <field> <name>: the pointer moved to the middle of that box with no button, and left there (Dietrich 2026-11-24, typed placement: the picker's tile
	// under the pointer says what it is, with no click)
	if ((verb == "click-in" && w.size() >= 3) || (verb == "drag-in" && w.size() >= 4) || (verb == "press-in" && w.size() >= 3) || (verb == "move-in" && w.size() >= 3))
	{
		AskState(s);

		std::string const boxes = Field(s->last, w[1]);
		int from[4] = {}, to[4] = {};
		bool const drag = verb == "drag-in";
		bool const haveFrom = TileBox(boxes, w[2], from);
		bool const haveTo = !drag || TileBox(boxes, w[3], to);

		if (!haveFrom || !haveTo)
		{
			Check(false, label + " (no box \"" + (haveFrom ? w[3] : w[2]) + "\" in " + w[1] + ": \"" + boxes + "\")");
			return false;
		}

		int const x = from[0] + from[2] / 2, y = from[1] + from[3] / 2;

		if (drag)
		{
			DragWindow(s, x, y, to[0] + to[2] / 2, to[1] + to[3] / 2, false, false, false);
		}
		else if (verb == "press-in")
		{
			DragWindow(s, x, y, x, y, false, false, false, false, true);   // pressed where it is, and held
		}
		else if (verb == "move-in")
		{
			int mx = x, my = y;
			HWND const target = TargetOf(s, &mx, &my);

			PostMessageW(target, WM_MOUSEMOVE, 0, MAKELPARAM(mx, my));   // no button: a hover, as move-win's
			Sleep(600);
		}
		else
		{
			ClickWindow(s, x, y, false, false);
		}

		AskState(s);
		Check(true, label + " from " + std::to_string(x) + "," + std::to_string(y));
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	// CLICK-TILE (added 2026-11-23 by Vasquez, the placement picker): click-tile <name> [twice]   a click in the middle of the picker's tile of that shape, where the
	// state line's pickerboxes says it is ("<name>~<x> <y> <w> <h>|...": the tiles on screen now); "twice" clicks it again after 400 ms (a click selects, a second places)
	if (verb == "click-tile" && w.size() >= 2)
	{
		AskState(s);

		std::string boxes = Field(s->last, "pickerboxes");
		int box[4] = {};

		// a tile not on the screen now: the grid paged on by its scroll bar (pickerscroll's "below", as a mapper would) until it is, or the last page is reached
		// (Vasquez 2026-11-23: a level's own shapes fill more than a page)
		for (int pages = 0; pages < 200 && !TileBox(boxes, w[1], box); pages++)
		{
			int below[4] = {};

			if (!TileBox(Field(s->last, "pickerscroll"), "below", below))
			{
				break;   // the last page: it isn't in the grid
			}

			ClickWindow(s, below[0] + below[2] / 2, below[1] + below[3] / 2, false, false, 300);
			AskState(s);
			boxes = Field(s->last, "pickerboxes");
		}

		if (!TileBox(boxes, w[1], box))
		{
			Check(false, label + " (no tile \"" + w[1] + "\" in the grid, paged to its end: pickerboxes \"" + boxes + "\")");
			return false;
		}

		int const x = box[0] + box[2] / 2, y = box[1] + box[3] / 2;
		bool const twice = std::find(w.begin(), w.end(), "twice") != w.end();

		ClickWindow(s, x, y, false, false);

		if (twice)
		{
			Sleep(400);
			ClickWindow(s, x, y, false, false);
		}

		AskState(s);
		Check(true, label + " at " + std::to_string(x) + "," + std::to_string(y));
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	// expect-tile-drawn <name> [share]: in the newest picture, at least that share (0.10 if not given) of the picker tile's picture part (pickerboxes, as
	// the state line said when the picture was taken) is away from the tile's background: the shape is drawn there, not an empty tile (Vasquez 2026-11-23).
	// THE EDITOR'S OWN FRAME (picture-<n>.png), in the window's client pixels as pickerboxes are: a hidden run takes no screen-<n>.png, so reading that one
	// gave -1 on the acceptance's first run (11-23 16:01)
	if (verb == "expect-tile-drawn" && w.size() >= 2)
	{
		double const limit = w.size() >= 3 ? atof(w[2].c_str()) : 0.10;
		int box[4] = {};

		if (s->lastPicture == 0)
		{
			Check(false, label + " (no picture taken yet)");
			return true;
		}

		AskState(s);

		if (!TileBox(Field(s->last, "pickerboxes"), w[1], box))
		{
			Check(false, label + " (no tile \"" + w[1] + "\" on screen: pickerboxes \"" + Field(s->last, "pickerboxes") + "\")");
			return true;
		}

		double const share = ShareDrawnInBox(PictureOf(s, "picture", s->lastPicture), box);
		char words[160];

		sprintf_s(words, " (picture-%d: %.3f of the tile at %d,%d %dx%d is drawn)", s->lastPicture, share, box[0], box[1], box[2], box[3]);
		Check(share >= limit, label + words);
		return true;
	}

	// expect-tile-backdrop <name> <n> <low> <high>: WHAT SHOWS BEHIND A PICKER TILE'S SHAPE (Frost 2026-11-24; the owner: "I want a 50% oppacity background in
	// black behind the spinning images, bit hard to see"). The tile's two bottom corners (BottomCornersBrightness) in the newest picture against the same pixels
	// of picture n, taken with the picker closed (the level alone): their brightness over the level's must be from low to high (about 0.5 behind a black box at
	// half opacity, about 1 with nothing behind the shape). The tile's box is the state line's pickerboxes now; both pictures are the editor's own frame. The
	// level must be bright enough there to measure (a mean of 40 of 765 at least), or the check fails and says so.
	if (verb == "expect-tile-backdrop" && w.size() >= 5)
	{
		int const since = atoi(w[2].c_str());
		double const low = atof(w[3].c_str()), high = atof(w[4].c_str());
		int box[4] = {};

		if (since < 1 || since > 999 || s->lastPicture == 0 || since == s->lastPicture)
		{
			Check(false, label + " (n is 1 to 999, a picture taken before the newest one)");
			return true;
		}

		AskState(s);

		if (!TileBox(Field(s->last, "pickerboxes"), w[1], box))
		{
			Check(false, label + " (no tile \"" + w[1] + "\" on screen: pickerboxes \"" + Field(s->last, "pickerboxes") + "\")");
			return true;
		}

		Picture const before = PictureOf(s, "picture", since), now = PictureOf(s, "picture", s->lastPicture);
		double levelMean = 0.0, nowMean = 0.0;
		bool const measured = BottomCornersBrightness(before, box, &levelMean) && BottomCornersBrightness(now, box, &nowMean) && before.width == now.width &&
			before.height == now.height;
		char words[220];

		if (!measured || levelMean < 40.0)
		{
			sprintf_s(words, " (picture-%d against picture-%d at the tile %d,%d %dx%d: %s)", s->lastPicture, since, box[0], box[1], box[2], box[3],
				measured ? "the level is too dark there to measure" : "the pictures can't be read, differ in size, or the tile is outside them");
			Check(false, label + words);
			return true;
		}

		double const ratio = nowMean / levelMean;

		sprintf_s(words, " (picture-%d against picture-%d at the tile %d,%d %dx%d: its bottom corners %.1f, the level's %.1f, %.3f of it)", s->lastPicture, since,
			box[0], box[1], box[2], box[3], nowMean, levelMean, ratio);
		Check(ratio >= low && ratio <= high, label + words);
		return true;
	}

	if (verb == "expect-box-centred" && w.size() >= 3)
	{
		AskState(s);

		std::string const boxWords = Field(s->last, "strategybox");
		int box[4] = {};
		int const leastW = atoi(w[1].c_str()), leastH = atoi(w[2].c_str());
		int const windowW = s->view[2], windowH = s->view[3];   // the level's view is the whole client area (L-1)
		bool const have = sscanf_s(boxWords.c_str(), "%d %d %d %d", &box[0], &box[1], &box[2], &box[3]) == 4;
		bool const centred = have && abs(box[0] * 2 + box[2] - windowW) <= 1 && abs(box[1] * 2 + box[3] - windowH) <= 1;
		bool const sized = have && box[2] >= leastW && box[3] >= leastH && box[2] * 2 >= windowW - 1 && box[3] * 2 >= windowH - 1 && box[2] <= windowW && box[3] <= windowH;

		Check(centred && sized, label + " (strategybox \"" + boxWords + "\" in a window " + std::to_string(windowW) + " x " + std::to_string(windowH) + ")");
		return true;
	}

	if (verb == "move-win" && w.size() >= 3)
	{
		int x = atoi(w[1].c_str()), y = atoi(w[2].c_str());

		WarnWindowClickInBox(s, "move-win", x, y);

		HWND const target = TargetOf(s, &x, &y);

		PostMessageW(target, WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
		Sleep(600);
		AskState(s);
		Check(true, label);
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	// THE HIERARCHY VIEW'S TEXT (Vasquez; inteng draws it every frame, FUN_004889d0 0x48a07a on): the state line carries hierarchylines "<row>:<words>|<row>@200:<words>|...".
	//   expect-hierarchy-text <words>          some line of it contains the words
	//   expect-hierarchy-count <n> <words>     exactly n lines contain the words (0: none does)
	// expect-screen-text-not <words>: no line of the screen's top-left text contains the words (Module Linking's Show All legend gone after Escape or a click)
	if (verb == "expect-screen-text-not" && w.size() >= 2)
	{
		AskState(s);

		std::string const all = Field(s->last, "screentext");
		std::string const unwanted = Rest(line, 1);

		Check(all.find(unwanted) == std::string::npos, label + " (the screen's text: \"" + all.substr(0, 300) + "\")");
		return true;
	}

	// expect-screen-text <words>: a line of the screen's top-left text (the state line's screentext: Frame Rate, Speed, the hub's View Distance ...) contains the words
	if (verb == "expect-screen-text" && w.size() >= 2)
	{
		AskState(s);

		std::string const all = Field(s->last, "screentext");
		std::string const wanted = Rest(line, 1);

		Check(all.find(wanted) != std::string::npos, label + " (the screen's text: \"" + all.substr(0, 300) + "\")");
		return true;
	}

	if ((verb == "expect-hierarchy-text" && w.size() >= 2) || (verb == "expect-hierarchy-count" && w.size() >= 3))
	{
		AskState(s);

		std::string const all = Field(s->last, "hierarchylines");
		std::string const wanted = verb == "expect-hierarchy-text" ? Rest(line, 1) : Rest(line, 2);
		int found = 0;
		size_t from = 0;

		while (from <= all.size() && !all.empty())
		{
			size_t const bar = all.find('|', from);
			std::string const one = all.substr(from, bar == std::string::npos ? std::string::npos : bar - from);
			size_t const colon = one.find(':');

			if (colon != std::string::npos && one.find(wanted, colon + 1) != std::string::npos)
			{
				found++;
			}

			if (bar == std::string::npos)
			{
				break;
			}

			from = bar + 1;
		}

		bool const ok = verb == "expect-hierarchy-text" ? found > 0 : found == atoi(w[1].c_str());

		Check(ok, label + " (" + std::to_string(found) + " line(s) of the view's text contain it; the text: \"" + all.substr(0, 300) + "\")");
		return true;
	}

	if (verb == "expect-bar-shown" || verb == "expect-bar-hidden")
	{
		AskState(s);

		bool const shown = Field(s->last, "barshown") == "1";

		Check(shown == (verb == "expect-bar-shown"), label + " (the bar is " + (shown ? "drawn" : "not drawn") + ")");
		return true;
	}

	// expect-drop-ticked <a|b|...>: the open drop-down's TICKED items (the state line's dropticked: the items MenuBar::IsChecked, as the bar draws them "on"), exactly the rest of the line;
	// "none" for no ticked item (Dietrich, 2026-11-16: inteng's menu object method +0x2c ticks Hierarchy's switches)
	if (verb == "expect-drop-ticked" && w.size() >= 2)
	{
		AskState(s);

		std::string const want = line.substr(line.find(w[1]));
		std::string const ticked = Field(s->last, "dropticked");

		Check(want == "none" ? ticked.empty() : ticked == want, label + " (the ticked items: \"" + ticked + "\")");
		return true;
	}

	// expect-text-screen <words>: the full text screen over the view (inteng's FUN_004724f0, "Click to continue.": a Help page) has a first line that holds the rest of the line;
	// "none" for no text screen (the state line's textscreen)
	if (verb == "expect-text-screen" && w.size() >= 2)
	{
		AskState(s);

		std::string const want = line.substr(line.find(w[1]));
		std::string const shown = Field(s->last, "textscreen");

		Check(want == "none" ? shown.empty() : (!shown.empty() && shown.find(want) != std::string::npos), label + " (the text screen's first line: \"" + shown + "\")");
		return true;
	}

	if (verb == "click-win" && w.size() >= 3)
	{
		bool const right = std::find(w.begin(), w.end(), "right") != w.end();
		bool const shift = std::find(w.begin(), w.end(), "shift") != w.end();

		if (!right)
		{
			WarnWindowClickInBox(s, "click-win", atoi(w[1].c_str()), atoi(w[2].c_str()));   // a right press ends the view anywhere, so only a left click is noted
		}

		// "s": the S key held over the click (Strategy Data's flag page: S + click selects by texture, FUN_00525700 0x525db2; Drake 11-17, S-69): its key-down is posted before the press, its
		// key-up after the release, as click-view's "delete" / "ctrl" / "esc"
		bool const sKey = std::find(w.begin(), w.end(), "s") != w.end();
		UINT const sScan = MapVirtualKeyW('S', MAPVK_VK_TO_VSC);

		if (sKey)
		{
			PostMessageW(s->editor.window, WM_KEYDOWN, 'S', 1 | (sScan << 16));
			Sleep(60);
		}

		ClickWindow(s, atoi(w[1].c_str()), atoi(w[2].c_str()), right, shift);

		if (sKey)
		{
			PostMessageW(s->editor.window, WM_KEYUP, 'S', 1 | (sScan << 16) | (1u << 30) | (1u << 31));
			Sleep(200);
		}

		AskState(s);
		Check(true, label);
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	// THE LIST DIALOG CONTRACT (Bishop, 2026-11-16; inteng's list dialogs, FUN_00475bf0, e.g. "Select Hierarchy" / "Select Sequence"):
	// while a list dialog is open, the editor's state line carries  dialog "<title>"; dialogrows "<row 1>|<row 2>|..."; dialogrow1 "<x> <y>
	// <w> <h>"  (the first row's box in the window's client pixels; the rows below it have the same height). The checks:
	//   expect-dialog <words>         a list dialog is open and its title contains the words
	//   expect-no-dialog              no list dialog is open
	//   expect-dialog-rows <a|b|...>  its rows are exactly these, in this order (_ is a space)
	//   click-row <n>                 a click in the middle of row n (from 1), where it is drawn: chosen by clicking, never by typing
	if (verb == "expect-dialog" && w.size() >= 2)
	{
		AskState(s);
		std::string const title = Field(s->last, "dialog");
		std::string const want = line.substr(line.find(w[1]));

		Check(!title.empty() && title.find(want) != std::string::npos, label + " (the dialog open: \"" + title + "\")");
		return true;
	}

	if (verb == "expect-no-dialog")
	{
		AskState(s);
		std::string const title = Field(s->last, "dialog");

		Check(title.empty(), label + (title.empty() ? std::string() : " (a dialog is open: \"" + title + "\")"));
		return true;
	}

	if (verb == "expect-dialog-rows" && w.size() >= 2)
	{
		AskState(s);
		// the rest of the line exactly as written: row words keep their own underscores (MarineHUD_Standard_Fire) and spaces
		std::string const want = line.substr(line.find(w[1]));
		std::string const rows = Field(s->last, "dialogrows");

		Check(rows == want, label + " (the rows: \"" + rows + "\")");
		return true;
	}

	// MESSAGE, CONFIRM AND TEXT DIALOGS (inteng's FUN_0057c800 / FUN_0057d2b0 boxes and its typed-text dialogs) use the same dialog
	// field for their title (or their message, when they have no title) and add: dialogkind "list|message|confirm|text";
	// dialogbuttons "<label> <x> <y> <w> <h>|..." (each button's words, _ for a space, and its box); dialogtext "<the text typed so far>".
	//   expect-dialog-kind <list|message|confirm|text>
	//   click-button <label> [n]      a click in the middle of that button where it is drawn (_ for a space in the label); n: the n-th button of that label (from 1, in the
	//                                 window's order: a form's list has a Remove on each line; Vasquez 11-24), else the first
	//   expect-dialog-text <words>    the text dialog's field holds exactly the rest of the line
	if (verb == "expect-dialog-kind" && w.size() >= 2)
	{
		AskState(s);
		std::string const kind = Field(s->last, "dialogkind");

		Check(kind == w[1], label + " (the open dialog's kind: \"" + kind + "\")");
		return true;
	}

	if (verb == "expect-dialog-text" && w.size() >= 2)
	{
		AskState(s);
		std::string const want = line.substr(line.find(w[1]));   // the rest of the line exactly as written
		std::string const text = Field(s->last, "dialogtext");

		Check(text == want, label + " (the text field holds \"" + text + "\")");
		return true;
	}

	if (verb == "click-button" && w.size() >= 2)
	{
		AskState(s);
		std::string const buttons = Field(s->last, "dialogbuttons");
		bool clicked = false;
		size_t from = 0;
		int const nth = w.size() >= 3 && atoi(w[2].c_str()) > 0 ? atoi(w[2].c_str()) : 1;
		int seen = 0;

		while (!clicked && from <= buttons.size() && !buttons.empty())
		{
			size_t const to = buttons.find('|', from);
			std::vector<std::string> const parts = Words(buttons.substr(from, to == std::string::npos ? std::string::npos : to - from));

			if (parts.size() >= 5 && parts[0] == w[1] && ++seen == nth)
			{
				int const x = atoi(parts[1].c_str()), y = atoi(parts[2].c_str()), width = atoi(parts[3].c_str()), height = atoi(parts[4].c_str());

				ClickWindow(s, x + width / 2, y + height / 2, false, false);
				clicked = true;
			}

			if (to == std::string::npos)
			{
				break;
			}

			from = to + 1;
		}

		if (!clicked)
		{
			Check(false, label + " (no button \"" + w[1] + "\" in \"" + buttons + "\")");
			return true;
		}

		AskState(s);
		Check(true, label);
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	// THE RIGHT-CLICK POP-UP (inteng's second table of a screen, FUN_004b7c50's right press -> FUN_004b8350; Vasquez): while one is open
	// the state line carries popup "<a|b|...>" (its top-level entries), popuprow1 "<x> <y> <w> <h>" and popupstep "<dx> <dy>".
	//   expect-popup-rows <a|b|...>   the open pop-up's top-level entries, exactly the rest of the line
	//   click-popup <n>               a click in the middle of entry n (from 1) where it is drawn
	//   click-popup <n> quick         the same, its picture taken 150 ms after the click and before the state is asked: a message that is on the screen for 700 ms only
	//                                 (inteng's FUN_0057bd40 + FUN_00529220) is still in it
	if (verb == "expect-popup-rows" && w.size() >= 2)
	{
		AskState(s);
		std::string const want = line.substr(line.find(w[1]));
		std::string const rows = Field(s->last, "popup");

		Check(rows == want, label + " (the pop-up's entries: \"" + rows + "\")");
		return true;
	}

	if (verb == "click-popup" && w.size() >= 2)
	{
		AskState(s);
		int box[4] = {}, step[2] = { 0, 0 };
		int const n = atoi(w[1].c_str());

		if (Field(s->last, "popup").empty() || n < 1 || sscanf_s(Field(s->last, "popuprow1").c_str(), "%d %d %d %d", &box[0], &box[1], &box[2], &box[3]) != 4)
		{
			Check(false, label + " (no pop-up open, or the editor gave no entry box)");
			return true;
		}

		if (sscanf_s(Field(s->last, "popupstep").c_str(), "%d %d", &step[0], &step[1]) != 2)
		{
			step[0] = 0;
			step[1] = box[3];
		}

		bool const quick = w.size() >= 3 && w[2] == "quick";

		ClickWindow(s, box[0] + (n - 1) * step[0] + box[2] / 2, box[1] + (n - 1) * step[1] + box[3] / 2, false, false, quick ? 150 : 600);

		if (quick)
		{
			PictureAfter(s, line);
		}

		AskState(s);
		Check(true, label);
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());

		if (!quick)
		{
			PictureAfter(s, line);
		}

		return true;
	}

	// FORM COMBO BOXES (dialog_box.h): the state line's dialogcombos "<picked words>_<x> <y> <w> <h>|..." (spaces in the words are _) in the form's order. click-combo <n> opens the n-th list (from 1); click-combo-row <words> presses the row
	// with those words (any capitals, in the list that is open); expect-dialog-combo-rows <word>,<word>,... says which rows the open list holds.
	// A LIST THAT SCROLLS (Vasquez 2026-11-21, the owner's "We can't scroll down to see message 1001..."): dialogcomborows names EVERY row, a row scrolled out of view with the box
	// 0 0 0 0. click-combo-row turns the mouse wheel toward a row that isn't shown, a notch at a time (at most a notch for every 3 rows, and 3 more), as a person does, then presses it.
	// It refuses a row whose box lies past the editor's window: no person can press it there.
	if ((verb == "click-combo" && w.size() >= 2) || (verb == "click-combo-row" && w.size() >= 2))
	{
		bool const row = verb == "click-combo-row";
		std::string want = line.substr(line.find(w[1]));

		for (char &c : want)
		{
			c = (char)tolower((unsigned char)c);
		}

		struct ShownRow
		{
			std::string words;
			int x = 0, y = 0, width = 0, height = 0;
		};

		std::string list, why;
		int notches = 0, mostNotches = -1;
		bool clicked = false;

		while (!clicked)
		{
			AskState(s);
			list = Field(s->last, row ? "dialogcomborows" : "dialogcombos");

			std::vector<ShownRow> rows;
			size_t from = 0;

			while (!list.empty() && from <= list.size())
			{
				size_t const to = list.find('|', from);
				std::vector<std::string> const parts = Words(list.substr(from, to == std::string::npos ? std::string::npos : to - from));

				if (parts.size() >= 5)
				{
					ShownRow r;

					r.words = parts[0];
					r.x = atoi(parts[1].c_str());
					r.y = atoi(parts[2].c_str());
					r.width = atoi(parts[3].c_str());
					r.height = atoi(parts[4].c_str());

					for (char &c : r.words)
					{
						c = c == '_' ? ' ' : (char)tolower((unsigned char)c);
					}

					rows.push_back(r);
				}

				if (to == std::string::npos)
				{
					break;
				}

				from = to + 1;
			}

			int found = -1, firstShown = -1;

			for (size_t i = 0; i < rows.size(); i++)
			{
				firstShown = firstShown < 0 && rows[i].width > 0 ? (int)i : firstShown;
				found = found < 0 && (row ? rows[i].words == want : (int)i + 1 == atoi(w[1].c_str())) ? (int)i : found;
			}

			if (found < 0)
			{
				why = "nothing to press in \"" + list + "\"";
				break;
			}

			ShownRow const &r = rows[(size_t)found];

			if (row && r.width <= 0)
			{
				mostNotches = mostNotches < 0 ? (int)rows.size() / 3 + 3 : mostNotches;

				if (notches >= mostNotches)
				{
					why = "the row is still out of view after " + std::to_string(notches) + " notches of the wheel";
					break;
				}

				WheelWindow(s, firstShown >= 0 && found < firstShown ? -1 : 1);
				notches++;
				continue;
			}

			RECT client = {};

			if (row && GetClientRect(s->editor.window, &client) && (r.x < 0 || r.y < 0 || r.x + r.width > client.right || r.y + r.height > client.bottom))
			{
				why = "the row's box " + std::to_string(r.x) + " " + std::to_string(r.y) + " " + std::to_string(r.width) + " " + std::to_string(r.height) + " lies past the window (" +
					std::to_string(client.right) + " x " + std::to_string(client.bottom) + "): no person can press it";
				break;
			}

			// a row is pressed in its middle; a combo on its ARROW (8 pixels in from its right side), as a person opens one: an editable combo's box takes letters left of its
			// arrow (dialog_box.h), and a plain combo opens anywhere in its box (Vasquez 11-16)
			ClickWindow(s, row ? r.x + r.width / 2 : r.x + r.width - 8, r.y + r.height / 2, false, false);
			clicked = true;
		}

		if (!clicked)
		{
			Check(false, label + " (" + why + ")");
			return true;
		}

		if (notches > 0)
		{
			printf("  (the mouse wheel turned %d notch%s first, to bring the row into view)\n", notches, notches == 1 ? "" : "es");
		}

		AskState(s);
		Check(true, label);
		PictureAfter(s, line);
		return true;
	}

	if (verb == "expect-dialog-combo-rows" && w.size() >= 2)
	{
		AskState(s);

		std::string const list = Field(s->last, "dialogcomborows");
		std::string now;
		size_t from = 0;

		while (!list.empty() && from <= list.size())
		{
			size_t const to = list.find('|', from);
			std::vector<std::string> const parts = Words(list.substr(from, to == std::string::npos ? std::string::npos : to - from));
			std::string words = parts.empty() ? std::string() : parts[0];

			for (char &c : words)
			{
				c = c == '_' ? ' ' : c;
			}

			now += (now.empty() ? "" : ",") + words;

			if (to == std::string::npos)
			{
				break;
			}

			from = to + 1;
		}

		std::string const want = line.substr(line.find(w[1]));

		Check(now == want, label + " (the open list holds \"" + now + "\")");
		return true;
	}

	// FORM CHECK BOXES (dialogs.h): the state line's dialogchecks "<on|off> <x> <y> <w> <h>|..." in the form's order. click-check <n> clicks the n-th box (from 1); expect-dialog-checks <on|off,on|off,...> says how they stand now.
	if (verb == "click-check" && w.size() >= 2)
	{
		AskState(s);

		std::string const checks = Field(s->last, "dialogchecks");
		int const wanted = atoi(w[1].c_str());
		int at = 0;
		size_t from = 0;
		bool clicked = false;

		while (!clicked && !checks.empty() && from <= checks.size())
		{
			size_t const to = checks.find('|', from);
			std::vector<std::string> const parts = Words(checks.substr(from, to == std::string::npos ? std::string::npos : to - from));

			if (++at == wanted && parts.size() >= 5)
			{
				ClickWindow(s, atoi(parts[1].c_str()) + atoi(parts[3].c_str()) / 2, atoi(parts[2].c_str()) + atoi(parts[4].c_str()) / 2, false, false);
				clicked = true;
			}

			if (to == std::string::npos)
			{
				break;
			}

			from = to + 1;
		}

		if (!clicked)
		{
			Check(false, label + " (no check box " + w[1] + " in \"" + checks + "\")");
			return true;
		}

		AskState(s);
		Check(true, label);
		PictureAfter(s, line);
		return true;
	}

	if (verb == "expect-dialog-checks" && w.size() >= 2)
	{
		AskState(s);

		std::string const checks = Field(s->last, "dialogchecks");
		std::string now;
		size_t from = 0;

		while (!checks.empty() && from <= checks.size())
		{
			size_t const to = checks.find('|', from);
			std::vector<std::string> const parts = Words(checks.substr(from, to == std::string::npos ? std::string::npos : to - from));

			now += (now.empty() ? "" : ",") + (parts.empty() ? std::string("?") : parts[0]);

			if (to == std::string::npos)
			{
				break;
			}

			from = to + 1;
		}

		Check(now == w[1], label + " (the boxes: \"" + now + "\")");
		return true;
	}

	if (verb == "click-row" && w.size() >= 2)
	{
		AskState(s);
		int box[4] = {};
		std::string const first = Field(s->last, "dialogrow1");
		int const n = atoi(w[1].c_str());

		if (Field(s->last, "dialog").empty() || n < 1 || sscanf_s(first.c_str(), "%d %d %d %d", &box[0], &box[1], &box[2], &box[3]) != 4 || box[3] <= 0)
		{
			Check(false, label + " (no list dialog open, or the editor gave no row box)");
			return true;
		}

		// inteng's list dialog lays its entry buttons out in columns (FUN_00475bf0): the editor may add dialogcols "<rows in a column>"
		// and dialogstep "<dx> <dy>" (from one column / row to the next; inteng's: the button's width + 20, and 20)
		int rowsInColumn = 0, step[2] = { 0, box[3] };
		std::string const columns = Field(s->last, "dialogcols"), steps = Field(s->last, "dialogstep");

		if (!columns.empty())
		{
			rowsInColumn = atoi(columns.c_str());
		}

		if (!steps.empty() && sscanf_s(steps.c_str(), "%d %d", &step[0], &step[1]) != 2)
		{
			step[0] = 0;
			step[1] = box[3];
		}

		int const column = rowsInColumn > 0 ? (n - 1) / rowsInColumn : 0;
		int const row = rowsInColumn > 0 ? (n - 1) % rowsInColumn : n - 1;

		ClickWindow(s, box[0] + column * step[0] + box[2] / 2, box[1] + row * step[1] + box[3] / 2, false, false);
		AskState(s);
		Check(true, label);
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	if ((verb == "click-view" || verb == "click-three") && w.size() >= 3)
	{
		bool const right = std::find(w.begin(), w.end(), "right") != w.end();
		bool const shift = std::find(w.begin(), w.end(), "shift") != w.end();
		// "delete": the Delete key held over the click (Module Linking's un-pick, inteng's slot 0x55 = VK 0x2e): its key-down is posted before the press, its key-up after the release.
		// "ctrl": the Ctrl key held over the click the same way (Strategy Data's collector: Ctrl + click adds the clicked object's whole shape group, FUN_00526bf0 0x526e3b; the press carries MK_CONTROL too)
		// "esc": the Esc key held over the click the same way (Scroll Lock's pick reads Esc only at a press: Esc held then gives it up, FUN_004720c0 0x472139; Drake 11-17)
		bool const held = std::find(w.begin(), w.end(), "delete") != w.end();
		bool const ctrl = std::find(w.begin(), w.end(), "ctrl") != w.end();
		bool const esc = std::find(w.begin(), w.end(), "esc") != w.end();

		AskState(s);   // the box's place now

		int const *const box = verb == "click-view" ? s->view : s->three;

		if (box[2] < 8 || box[3] < 8)
		{
			Check(false, label + " (the editor has no " + (verb == "click-view" ? "view" : "three views' box") + " shown)");
			return false;
		}

		int const x = box[0] + (int)(atof(w[1].c_str()) * box[2]);
		int const y = box[1] + (int)(atof(w[2].c_str()) * box[3]);
		UINT const heldKey = held ? VK_DELETE : (esc ? VK_ESCAPE : VK_CONTROL);
		UINT const heldScan = MapVirtualKeyW(heldKey, MAPVK_VK_TO_VSC);

		if (held || ctrl || esc)
		{
			PostMessageW(s->editor.window, WM_KEYDOWN, heldKey, 1 | (heldScan << 16));
			Sleep(60);
		}

		ClickWindow(s, x, y, right, shift, 600, ctrl);

		if (held || ctrl || esc)
		{
			PostMessageW(s->editor.window, WM_KEYUP, heldKey, 1 | (heldScan << 16) | (1u << 30) | (1u << 31));
			Sleep(200);
		}

		AskState(s);
		Check(true, label + " at " + std::to_string(x) + "," + std::to_string(y));
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	if (verb == "click-wp" && w.size() >= 3)
	{
		bool const right = std::find(w.begin(), w.end(), "right") != w.end();
		bool const shift = std::find(w.begin(), w.end(), "shift") != w.end();
		WPARAM const index = w[1] == "current" ? 0xffff : (WPARAM)atoi(w[1].c_str());

		AskState(s);   // a frame with the views fitted is drawn first
		PostMessageW(s->editor.window, WM_APP + 7, index, 0);

		std::string const said = WaitForLine(&s->editor, "AVP_OpenInteng: locate: ", 10000);
		std::string const key = w[2] + " ";
		size_t const at = said.find("; " + key);
		int x = -1, y = -1;

		if (at != std::string::npos)
		{
			sscanf_s(said.c_str() + at + 2 + key.size(), "%d %d", &x, &y);
		}

		if (said.empty() || at == std::string::npos || x < 0 || y < 0)
		{
			Check(false, label + " -> " + (said.empty() ? "no answer" : said));
			return false;
		}

		printf("  (%s)\n", From(said, "locate").c_str());
		ClickWindow(s, x, y, right, shift);
		AskState(s);
		Check(true, label + " at " + std::to_string(x) + "," + std::to_string(y));
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	if ((verb == "drag-wp" || verb == "click-link") && w.size() >= 4)
	{
		// drag-wp <from> <to> <view> [shift] [ctrl]: press on waypoint <from>'s box where that view draws its centre (as click-wp), move to waypoint <to>'s, let go: Waypoint by mouse's link drag
		// (Dietrich 11-18, P4). Shift held at the press: an aliens-only link; ctrl: one way
		bool const shift = std::find(w.begin(), w.end(), "shift") != w.end();
		bool const control = std::find(w.begin(), w.end(), "ctrl") != w.end();
		int places[2][2] = { { -1, -1 }, { -1, -1 } };
		std::string const key = w[3] + " ";

		for (int end = 0; end < 2; end++)
		{
			AskState(s);
			PostMessageW(s->editor.window, WM_APP + 7, (WPARAM)atoi(w[1 + end].c_str()), 0);

			std::string const said = WaitForLine(&s->editor, "AVP_OpenInteng: locate: ", 10000);
			size_t const at = said.find("; " + key);

			if (at != std::string::npos)
			{
				sscanf_s(said.c_str() + at + 2 + key.size(), "%d %d", &places[end][0], &places[end][1]);
			}

			if (said.empty() || at == std::string::npos || places[end][0] < 0 || places[end][1] < 0)
			{
				Check(false, label + " -> " + (said.empty() ? "no answer" : said));
				return false;
			}
		}

		if (w[0] == "click-link")
		{
			// click-link <a> <b> <view>: a click halfway between the two boxes' drawn centres, which is on the link between their aim points when those are the boxes' middles (a line
			// stays a line on the screen); Waypoint by mouse's link pick (Dietrich 11-18, P4)
			int const midX = (places[0][0] + places[1][0]) / 2, midY = (places[0][1] + places[1][1]) / 2;

			ClickWindow(s, midX, midY, false, false);
			AskState(s);
			Check(true, label + " at " + std::to_string(midX) + "," + std::to_string(midY));
			printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
			PictureAfter(s, line);
			return true;
		}

		DragWindow(s, places[0][0], places[0][1], places[1][0], places[1][1], false, control, shift);
		AskState(s);
		Check(true, label + " from " + std::to_string(places[0][0]) + "," + std::to_string(places[0][1]) + " to " + std::to_string(places[1][0]) + "," + std::to_string(places[1][1]));
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	if (verb == "drag-face" && w.size() >= 6)
	{
		// drag-face <index|current> <top|side|front> <left|right|up|down> <dx> <dy>: press on that face of the waypoint's box in that view, move by (dx, dy) window pixels, let go
		WPARAM const index = w[1] == "current" ? 0xffff : (WPARAM)atoi(w[1].c_str());

		AskState(s);
		PostMessageW(s->editor.window, WM_APP + 7, index, 0);

		std::string const said = WaitForLine(&s->editor, "AVP_OpenInteng: locate: ", 10000);
		int lowX = -1, lowY = -1, highX = -1, highY = -1;
		size_t const lowAt = said.find("; " + w[2] + "low "), highAt = said.find("; " + w[2] + "high ");

		if (lowAt != std::string::npos)
		{
			sscanf_s(said.c_str() + lowAt + 2 + w[2].size() + 4, "%d %d", &lowX, &lowY);
		}

		if (highAt != std::string::npos)
		{
			sscanf_s(said.c_str() + highAt + 2 + w[2].size() + 5, "%d %d", &highX, &highY);
		}

		if (said.empty() || lowX < 0 || highX < 0)
		{
			Check(false, label + " -> " + (said.empty() ? "no answer" : said));
			return false;
		}

		int const minX = lowX < highX ? lowX : highX, maxX = lowX < highX ? highX : lowX, minY = lowY < highY ? lowY : highY, maxY = lowY < highY ? highY : lowY;
		int x0 = (minX + maxX) / 2, y0 = (minY + maxY) / 2;

		if (w[3] == "left")
		{
			x0 = minX;
		}
		else if (w[3] == "right")
		{
			x0 = maxX;
		}
		else if (w[3] == "up")
		{
			y0 = minY;
		}
		else
		{
			y0 = maxY;
		}

		printf("  (%s)\n", From(said, "locate").c_str());
		DragWindow(s, x0, y0, x0 + atoi(w[4].c_str()), y0 + atoi(w[5].c_str()));
		AskState(s);
		Check(true, label + " from " + std::to_string(x0) + "," + std::to_string(y0));
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	if (verb == "drag-win" && w.size() >= 5)
	{
		// drag-win <x0> <y0> <x1> <y1> [right] [ctrl] [shift]: the right button's drag, and with Control held (Dietrich, TM-B 2026-11-18: the texture windows' right drag); shift: Shift
		// held at the press (Dietrich 11-18, P4: Waypoint by mouse's Shift drag, an aliens-only link). The words in any order after the four numbers.
		bool right = false, control = false, shift = false;

		for (size_t k = 5; k < w.size(); k++)
		{
			right = right || w[k] == "right";
			control = control || w[k] == "ctrl";
			shift = shift || w[k] == "shift";
		}

		DragWindow(s, atoi(w[1].c_str()), atoi(w[2].c_str()), atoi(w[3].c_str()), atoi(w[4].c_str()), right, control, shift);
		AskState(s);
		Check(true, label);
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	// drag-gizmo <move|turn> <x|y|z> <dx> <dy> [shift] [esc] [hold]: a drag from where the editor's gizmopoints field says that handle of the mouse gizmo is (window pixels: "move x 612 377|..."),
	// by dx, dy; shift: Shift held through it (the turn's 15-degree snap); esc: Esc pressed before the button is let go (the drag given up). Drake 2026-11-18 (P1): the handles move
	// with the camera and the object, so no fixed pixel. hold (Drake 2026-11-19, the owner's "the object doesn't move with it"): the button stays down there, so the steps after it
	// (state, expect-field, picture) see the drag IN THE MIDDLE; release-gizmo lets it go (or esc-gizmo gives it up)
	if (verb == "drag-gizmo" && w.size() >= 5)
	{
		AskState(s);

		std::string const name = w[1] + " " + w[2] + " ";
		// the waypoint-box gizmo's handles first (Frost 11-18, waypoint_gizmo.h: "min x" .. "max z" and its own "move x" / "centre ball"; it takes the press before Drake's gizmo on
		// its screen), then Drake's
		std::string const all = Field(s->last, "boxgizmopoints") + "|" + Field(s->last, "gizmopoints");
		int x = 0, y = 0;
		bool found = false;

		for (size_t at = 0; at <= all.size() && !found;)
		{
			size_t const end = all.find('|', at);
			std::string const part = all.substr(at, end == std::string::npos ? std::string::npos : end - at);

			found = part.compare(0, name.size(), name) == 0 && sscanf_s(part.c_str() + name.size(), "%d %d", &x, &y) == 2;
			at = end == std::string::npos ? all.size() + 1 : end + 1;
		}

		if (!found)
		{
			Check(false, label + " (no \"" + w[1] + " " + w[2] + "\" in gizmopoints \"" + all.substr(0, 200) + "\")");
			return true;
		}

		bool const shift = std::find(w.begin() + 5, w.end(), "shift") != w.end();
		bool const escape = std::find(w.begin() + 5, w.end(), "esc") != w.end();
		bool const holdOn = std::find(w.begin() + 5, w.end(), "hold") != w.end();

		if (s->dragHeld)
		{
			Check(false, label + " (a drag is held already: release-gizmo first)");
			return true;
		}

		DragWindow(s, x, y, x + atoi(w[3].c_str()), y + atoi(w[4].c_str()), false, false, shift, escape && !holdOn, holdOn);
		AskState(s);
		Check(true, label + " (from " + std::to_string(x) + "," + std::to_string(y) + ")");
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	// release-gizmo: the button held by drag-gizmo ... hold let go where it is (the drag kept, as a real release); esc-gizmo: Esc first, then the release (the drag given up)
	// move-held <dx> <dy> (Frost 11-19, the owner's G toggle while the centre ball is held: "slide a little with UP on"): the pointer moved by (dx, dy) window pixels from where the
	// drag-gizmo ... hold left it, the button still down; the state and a picture after it
	if (verb == "move-held" && w.size() >= 3)
	{
		if (!s->dragHeld)
		{
			Check(false, label + " (no drag is held: drag-gizmo ... hold first)");
			return true;
		}

		int const tx = s->heldX + atoi(w[1].c_str()), ty = s->heldY + atoi(w[2].c_str());
		WPARAM const held = MK_LBUTTON | (s->heldShift ? MK_SHIFT : 0);

		for (int step = 1; step <= 4; step++)
		{
			PostMessageW(s->heldTarget, WM_MOUSEMOVE, held, MAKELPARAM(s->heldX + (tx - s->heldX) * step / 4, s->heldY + (ty - s->heldY) * step / 4));
			Sleep(80);
		}

		s->heldX = tx;
		s->heldY = ty;
		Sleep(400);
		AskState(s);
		Check(true, label + " (to " + std::to_string(tx) + "," + std::to_string(ty) + ")");
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	if (verb == "release-gizmo" || verb == "esc-gizmo")
	{
		if (!s->dragHeld)
		{
			Check(false, label + " (no drag is held: drag-gizmo ... hold first)");
			return true;
		}

		if (verb == "esc-gizmo")
		{
			PostMessageW(s->editor.window, WM_KEYDOWN, VK_ESCAPE, 1 | (MapVirtualKeyW(VK_ESCAPE, MAPVK_VK_TO_VSC) << 16));
			Sleep(80);
			PostMessageW(s->editor.window, WM_KEYUP, VK_ESCAPE, 1 | (MapVirtualKeyW(VK_ESCAPE, MAPVK_VK_TO_VSC) << 16) | (1u << 30) | (1u << 31));
			Sleep(150);
		}

		PostMessageW(s->heldTarget, WM_LBUTTONUP, 0, MAKELPARAM(s->heldX, s->heldY));
		s->dragHeld = false;
		Sleep(600);
		AskState(s);
		Check(true, label + " (at " + std::to_string(s->heldX) + "," + std::to_string(s->heldY) + ")");
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	// THE TEXTURE MAP WINDOW (Dietrich, TM-B T 2026-11-18): inteng lays it out in the screen's bottom-right corner, its size the view's times the scale, so its places follow the frame's
	// size; the editor's texwin field says where it is: left top right bottom, the view's left top right bottom (texels), the scale's number, "resizing" while Texture Resize is on.
	// drag-texwin / click-texwin take places from its top left; expect-texwin checks the size, the view and the scale, and that its far edges are the frame's right and the frame's
	// bottom less the status line (36)
	if ((verb == "drag-texwin" && w.size() >= 5) || (verb == "click-texwin" && w.size() >= 3) || (verb == "expect-texwin" && w.size() >= 8))
	{
		AskState(s);

		std::string const words = Field(s->last, "texwin");
		int win[4] = {}, view[4] = {}, scale = 0;

		if (sscanf_s(words.c_str(), "%d %d %d %d %d %d %d %d %d", &win[0], &win[1], &win[2], &win[3], &view[0], &view[1], &view[2], &view[3], &scale) != 9)
		{
			Check(false, label + " (no texture window on the state line: texwin \"" + words + "\")");
			return true;
		}

		if (verb == "expect-texwin")
		{
			RECT client = {};

			GetClientRect(s->editor.window, &client);

			bool const same = win[2] - win[0] == atoi(w[1].c_str()) && win[3] - win[1] == atoi(w[2].c_str()) && view[0] == atoi(w[3].c_str()) && view[1] == atoi(w[4].c_str()) &&
				view[2] == atoi(w[5].c_str()) && view[3] == atoi(w[6].c_str()) && scale == atoi(w[7].c_str());
			bool const corner = win[2] == client.right - client.left && win[3] == client.bottom - client.top - 36;

			Check(same && corner, label + " (texwin \"" + words + "\", the frame " + std::to_string(client.right - client.left) + " x " + std::to_string(client.bottom - client.top) + ")");
			return true;
		}

		if (verb == "click-texwin")
		{
			ClickWindow(s, win[0] + atoi(w[1].c_str()), win[1] + atoi(w[2].c_str()), false, std::find(w.begin(), w.end(), "shift") != w.end());
		}
		else
		{
			bool const right = std::find(w.begin(), w.end(), "right") != w.end();
			bool const control = std::find(w.begin(), w.end(), "ctrl") != w.end();

			DragWindow(s, win[0] + atoi(w[1].c_str()), win[1] + atoi(w[2].c_str()), win[0] + atoi(w[3].c_str()), win[1] + atoi(w[4].c_str()), right, control);
		}

		AskState(s);
		Check(true, label + " (the window's top left " + std::to_string(win[0]) + "," + std::to_string(win[1]) + ")");
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	// form-field <n> <text>: in an open FORM (a window of edits and check boxes, the state line's dialogfields) a click on the n-th edit (from 1), its text taken away with Backspace and <text> typed in. No text: only cleared.
	if (verb == "form-field" && w.size() >= 2)
	{
		AskState(s);

		std::string const fields = Field(s->last, "dialogfields");
		size_t from = 0;
		int index = 0;
		bool done = false;
		int const wanted = atoi(w[1].c_str());

		while (!done && from <= fields.size() && !fields.empty())
		{
			size_t const bar = fields.find('|', from);
			std::vector<std::string> const parts = Words(fields.substr(from, bar == std::string::npos ? std::string::npos : bar - from));

			if (++index == wanted && parts.size() >= 5)
			{
				ClickWindow(s, atoi(parts[1].c_str()) + atoi(parts[3].c_str()) / 2, atoi(parts[2].c_str()) + atoi(parts[4].c_str()) / 2, false, false);

				for (int i = 0; i < 70; i++)
				{
					PostMessageW(s->editor.window, WM_KEYDOWN, VK_BACK, 1);
					Sleep(8);
				}

				std::string const text = Rest(line, 2);

				for (wchar_t const letter : Widen(text))
				{
					PostChar(s, letter);
				}

				done = true;
			}

			if (bar == std::string::npos)
			{
				break;
			}

			from = bar + 1;
		}

		Sleep(150);
		AskState(s);
		Check(done, label + (done ? "" : " (no such edit in \"" + fields + "\")"));
		return done;
	}

	if (verb == "type")
	{
		for (wchar_t const letter : Widen(Rest(line, 1)))
		{
			PostChar(s, letter);
		}

		Sleep(150);
		AskState(s);
		Check(true, label);
		PictureAfter(s, line);
		return true;
	}

	if (verb == "key-up" && w.size() >= 2)
	{
		// key-up <k>: only the key's letting go is posted, no press: the way Windows hands Print Screen to a window (WM_KEYUP with VK_SNAPSHOT; added 2026-11-17 by Drake for #78)
		UINT const key = KeyOf(w[1]);

		if (!key)
		{
			Check(false, label + " (a key the scenario doesn't know)");
			return false;
		}

		UINT const scan = MapVirtualKeyW(key, MAPVK_VK_TO_VSC);

		PostMessageW(s->editor.window, WM_KEYUP, key, 1 | (scan << 16) | (1u << 30) | (1u << 31));
		Sleep(200);
		AskState(s);
		Check(true, label);
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	// expect-last-answer-within <ms>: the last state the step before asked for came within that many milliseconds of its ask (AskState times every ask; the editor answers in its next
	// frame), so the editor's window kept drawing through that step. Vasquez 2026-11-21, the owner's "her video is long so it got stuck for a few seconds": a stuck editor answers late.
	if (verb == "expect-last-answer-within" && w.size() >= 2)
	{
		long long const most = atoll(w[1].c_str());

		if (most < 1 || most > 60000)
		{
			Check(false, label + " (1 to 60000 ms)");
			return false;
		}

		Check(s->lastAnswerMs <= (ULONGLONG)most, label + " (the last answer came " + std::to_string((unsigned long long)s->lastAnswerMs) + " ms after its ask)");
		return true;
	}

	// wheel up|down <notches>: the mouse wheel turned, a notch at a time, the pointer in the window's middle (Vasquez 2026-11-21: an open combo list scrolls by it, 3 rows a notch)
	if (verb == "wheel" && w.size() >= 3 && (w[1] == "up" || w[1] == "down"))
	{
		int const notches = atoi(w[2].c_str());

		if (notches < 1 || notches > 200)
		{
			Check(false, label + " (1 to 200 notches)");
			return false;
		}

		for (int n = 0; n < notches; n++)
		{
			WheelWindow(s, w[1] == "up" ? -1 : 1, 60);
		}

		Sleep(300);
		AskState(s);
		Check(true, label);
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	if (verb == "key" && w.size() >= 2)
	{
		// key <k> shift: the key with Shift held down (a chord: Shift goes down, the key is pressed and let go, Shift goes up); Ctrl the same way: key <k> ctrl
		if (w.size() >= 3 && (w[2] == "shift" || w[2] == "ctrl"))
		{
			return DoStep(s, std::string("chord ") + (w[2] == "shift" ? "Shift " : "Ctrl ") + w[1], number);
		}

		UINT const key = KeyOf(w[1]);

		if (!key)
		{
			Check(false, label + " (a key the scenario doesn't know)");
			return false;
		}

		HoldAKey(&s->editor, key, 120);   // three of inteng's frames (25 a second, the editor's frame cap): a shorter press can be let go of within one frame and never be seen held
		AskState(s);
		Check(true, label);
		printf("  (%s)\n", s->last.empty() ? "no state line" : From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	if (verb == "repeat" && w.size() >= 6)
	{
		// repeat <most> <key> <hold ms> <status words, no spaces: use _ for a space> | <step> | <step> ...: the steps are done in order; if the status line then does not hold the words, the key is held for
		// that long (the camera moves, and how far depends on the frame rate: a run on a busy machine goes a different way) and the steps are done again, at most <most> times. A scenario that picks something
		// by a click needs this: the picture it clicks on is not the same on every run.
		int const most = atoi(w[1].c_str());
		std::string wanted = w[4];

		for (char &letter : wanted)
		{
			letter = letter == '_' ? ' ' : letter;
		}

		std::vector<std::string> subs;
		std::string tail = Rest(line, 5);

		if (tail.compare(0, 2, "| ") == 0)
		{
			tail = tail.substr(2);   // the first step follows the first bar
		}

		// the steps between the bars, each cut off the front of what is left (no sum or difference of places in the line: Snyk Code, PR #79)
		std::string left = tail;
		bool more = !left.empty();

		while (more)
		{
			size_t const bar = left.find(" | ");

			subs.push_back(left.substr(0, bar));
			more = bar != std::string::npos;

			if (more)
			{
				left = left.substr(bar).substr(3);
			}
		}

		for (int round = 1; round <= most; round++)
		{
			for (std::string const &sub : subs)
			{
				if (!DoStep(s, sub, number))
				{
					return false;
				}
			}

			AskState(s);

			if (Field(s->last, "status").find(wanted) != std::string::npos)
			{
				Check(true, label + " (after " + std::to_string(round) + " round(s): the status says \"" + Field(s->last, "status") + "\")");
				return true;
			}

			HoldAKey(&s->editor, KeyOf(w[2]) ? KeyOf(w[2]) : (UINT)w[2][0], (DWORD)atoi(w[3].c_str()));
		}

		Check(false, label + " (the status never said \"" + wanted + "\" in " + std::to_string(most) + " rounds; it says \"" + Field(s->last, "status") + "\")");
		return false;
}

	// alt-release <key>: the key goes down, then Alt; the key is let go WHILE Alt is down (Windows sends that as WM_SYSKEYUP, not WM_KEYUP), then Alt goes up. The way a
	// key is let go under Alt in real use: the editor kept such a key held in its table (ML-M-76; added 2026-11-17 by Melanikus (Dietrich))
	if (verb == "alt-release" && w.size() >= 2)
	{
		UINT const key = KeyOf(w[1]);

		if (!key)
		{
			Check(false, label + " (a key the scenario doesn't know)");
			return false;
		}

		HWND const window = s->editor.window;
		UINT const keyScan = MapVirtualKeyW(key, MAPVK_VK_TO_VSC), altScan = MapVirtualKeyW(VK_MENU, MAPVK_VK_TO_VSC);

		PostMessageW(window, WM_KEYDOWN, key, 1 | (keyScan << 16));
		Sleep(80);
		PostMessageW(window, WM_SYSKEYDOWN, VK_MENU, 1 | (altScan << 16) | (1u << 29));
		Sleep(80);
		PostMessageW(window, WM_SYSKEYUP, key, 1 | (keyScan << 16) | (1u << 29) | (1u << 30) | (1u << 31));
		Sleep(80);
		PostMessageW(window, WM_SYSKEYUP, VK_MENU, 1 | (altScan << 16) | (1u << 30) | (1u << 31));
		Sleep(150);
		AskState(s);
		Check(true, label);
		PictureAfter(s, line);
		return true;
	}

	if (verb == "chord" && w.size() >= 3)
	{
		// chord <held key> <key>: the first key goes down, the second is pressed and let go while it is down, then the first goes up (the editor reads its own table of keys held, so "S and an arrow" reaches it; Shift, Ctrl and Alt cannot be held this way)
		UINT held = KeyOf(w[1]);
		UINT const second = KeyOf(w[2]);

		if (!held && w[1].size() == 1 && ((w[1][0] >= 'A' && w[1][0] <= 'Z') || (w[1][0] >= '0' && w[1][0] <= '9')))
		{
			held = (UINT)w[1][0];
		}

		if (!held || !second)
		{
			Check(false, label + " (a key the scenario doesn't know)");
			return false;
		}

		HWND const window = s->editor.window;
		UINT const heldScan = MapVirtualKeyW(held, MAPVK_VK_TO_VSC), secondScan = MapVirtualKeyW(second, MAPVK_VK_TO_VSC);

		PostMessageW(window, WM_KEYDOWN, held, 1 | (heldScan << 16));
		Sleep(80);
		PostMessageW(window, WM_KEYDOWN, second, 1 | (secondScan << 16));
		Sleep(120);   // the second key is held three frames (the 25-a-second cap)
		PostMessageW(window, WM_KEYUP, second, 1 | (secondScan << 16) | (1u << 30) | (1u << 31));
		Sleep(80);
		PostMessageW(window, WM_KEYUP, held, 1 | (heldScan << 16) | (1u << 30) | (1u << 31));
		Sleep(150);
		AskState(s);
		Check(true, label);
		PictureAfter(s, line);
		return true;
	}

	if (verb == "show-page" && w.size() >= 2)
	{
		// show-page <n>: the bars show page n of the code-built menus now (the editor answers only in a -picture run); the state line says what is on the bars
		Drain(s);
		PostMessageW(s->editor.window, WM_APP + 9, (WPARAM)atoi(w[1].c_str()), 0);

		std::string const state = WaitForLine(&s->editor, "AVP_OpenInteng: scenario: ", 10000);

		if (state.empty())
		{
			Check(false, label + " (the editor gave no state line)");
			return false;
		}

		s->last = state;
		TakeGeometry(s);
		Check(true, label);
		printf("  (%s)\n", From(s->last, "scenario: ").c_str());
		PictureAfter(s, line);
		return true;
	}

	if (verb == "hold" && w.size() >= 3)
	{
		// hold <a letter or a key's name> <milliseconds>: the key held down that long (the camera moves while it is: Classic keys, A forward and Z back). "Shift+A" holds Shift down all that time as well;
		// "<key>+<key>" (Comma+Period) holds the first key down all that time too (two keys held together)
		std::string keyName = w[1];
		bool const withShift = keyName.compare(0, 6, "Shift+") == 0;
		UINT alsoHeld = 0;

		if (withShift)
		{
			keyName = keyName.substr(6);
		}
		else if (keyName.find('+') != std::string::npos && keyName.find('+') > 0 && keyName.find('+') + 1 < keyName.size())
		{
			std::string const first = keyName.substr(0, keyName.find('+'));

			alsoHeld = KeyOf(first);

			if (!alsoHeld && first.size() == 1 && ((first[0] >= 'A' && first[0] <= 'Z') || (first[0] >= '0' && first[0] <= '9')))
			{
				alsoHeld = (UINT)first[0];
			}

			if (!alsoHeld)
			{
				Check(false, label + " (a key the scenario doesn't know)");
				return false;
			}

			keyName = keyName.substr(keyName.find('+') + 1);
		}

		UINT key = KeyOf(keyName);

		if (!key && keyName.size() == 1 && ((keyName[0] >= 'A' && keyName[0] <= 'Z') || (keyName[0] >= '0' && keyName[0] <= '9')))
		{
			key = (UINT)keyName[0];
		}

		if (!key)
		{
			Check(false, label + " (a key the scenario doesn't know)");
			return false;
		}

		UINT const heldFirst = withShift ? VK_SHIFT : alsoHeld;

		if (heldFirst)
		{
			PostMessageW(s->editor.window, WM_KEYDOWN, heldFirst, 1 | (MapVirtualKeyW(heldFirst, MAPVK_VK_TO_VSC) << 16));
		}

		HoldAKey(&s->editor, key, (DWORD)atoi(w[2].c_str()));

		if (heldFirst)
		{
			PostMessageW(s->editor.window, WM_KEYUP, heldFirst, 1 | (MapVirtualKeyW(heldFirst, MAPVK_VK_TO_VSC) << 16) | (1u << 30) | (1u << 31));
		}

		AskState(s);
		Check(true, label);
		PictureAfter(s, line);
		return true;
	}

	if (verb == "camera" && w.size() >= 5)
	{
		// puts the camera at x y z (millimetres) with that turn (0 looks along z, 1024 along x) (kHarnessCamera)
		Drain(s);

		for (int part = 0; part < 4; part++)
		{
			PostMessageW(s->editor.window, WM_APP + 10, (WPARAM)part, (LPARAM)strtol(w[1 + part].c_str(), NULL, 0));
		}

		std::string const said = WaitForLine(&s->editor, "AVP_OpenInteng: camera: ", 10000);

		Check(!said.empty(), label + " -> " + (said.empty() ? "no answer" : From(said, "camera")));
		return true;
	}

	// camera-look <look> <roll>: the camera's look (4096ths, signed: plus is down, held to a quarter turn either way) and roll; "camera" sets the place and the turn only (kHarnessCamera parts 4 and 5)
	if ((verb == "camera-orbit" || verb == "camera-corner") && w.size() >= 2)
	{
		// the editor's OrbitCommand_Run (orbit_command.h) through WM_COPYDATA, the only message that carries a name: it is answered with the editor's camera line (Drake 11-18)
		Drain(s);

		// the object's name is the REST OF THE LINE (Bishop / Dietrich 2026-11-18: the owner's MelPyth parts have spaces, "ppump palm"), or a name in double quotes (one that ends in a
		// number: "door 2"); camera-orbit's <around> <up> <distance> are the last words that are plain numbers, at most 3, after at least one word of the name (or after the quoted name)
		auto const plainNumber = [](std::string const &word)
		{
			size_t at = word.size() > 1 && word[0] == '-' ? 1 : 0;
			size_t const digits = word.find_first_not_of("0123456789", at);

			if (digits == at)
			{
				return false;
			}

			return digits == std::string::npos || (word[digits] == '.' && digits + 1 < word.size() && word.find_first_not_of("0123456789", digits + 1) == std::string::npos);
		};
		auto const trimEnd = [](std::string *text)
		{
			while (!text->empty() && (text->back() == ' ' || text->back() == '\t' || text->back() == '\r'))
			{
				text->pop_back();
			}
		};
		std::string name = Rest(line, 1);
		std::vector<std::string> numbers;
		std::string wrong;

		trimEnd(&name);

		if (!name.empty() && name[0] == '"')
		{
			size_t const close = name.find('"', 1);

			numbers = close == std::string::npos ? std::vector<std::string>() : Words(name.substr(close + 1));
			wrong = close == std::string::npos || close == 1 ? "the quoted name has no closing quote, or is empty" : std::string();
			name = close == std::string::npos ? name : name.substr(1, close - 1);
		}
		else if (verb == "camera-orbit")
		{
			std::vector<std::string> const words = Words(name);
			size_t keep = words.size();
			size_t const least = words.size() >= 2 && words[0] == "light" && plainNumber(words[1]) ? 2 : 1;   // "light <n>" is one name: its numbers come after it

			while (keep > least && words.size() - keep < 3 && plainNumber(words[keep - 1]))
			{
				keep--;
				name.erase(name.find_last_of(" \t"));
				trimEnd(&name);
			}

			numbers.assign(words.begin() + (long)keep, words.end());
		}

		for (std::string const &number : numbers)
		{
			wrong = !wrong.empty() ? wrong : verb == "camera-corner" ? "camera-corner takes only the name" : numbers.size() > 3 || !plainNumber(number) ?
				"after the quoted name, at most <around> <up> <distance>, each a plain number" : std::string();
		}

		wrong = !wrong.empty() ? wrong : name.find('|') != std::string::npos ? "a name with a | can't be sent (the command's fields are split on |)" : std::string();

		if (!wrong.empty())
		{
			Check(false, label + " -> " + wrong);
			return true;
		}

		std::string text = std::string(verb == "camera-orbit" ? "orbit|" : "corner|") + name;

		for (std::string const &number : numbers)
		{
			text += "|" + number;
		}

		COPYDATASTRUCT data = {};
		DWORD_PTR answer = 0;

		data.dwData = 0x4f524254ul;   // orbit_command.h kOrbitCommandTag ('ORBT')
		data.cbData = (DWORD)text.size();
		data.lpData = (PVOID)text.data();
		SendMessageTimeoutW(s->editor.window, WM_COPYDATA, 0, (LPARAM)&data, SMTO_ABORTIFHUNG, 10000, &answer);

		std::string const said = WaitForLine(&s->editor, "AVP_OpenInteng: camera: ", 10000);

		// no camera line: the editor's own words, from the status line ("camera-orbit: no object or module named ... is in play")
		std::string const why = said.empty() && AskState(s) ? Field(s->last, "status") : std::string();

		Check(answer == TRUE && !said.empty(), label + " -> " + (!said.empty() ? From(said, "camera") : "no camera line: the editor says \"" + why + "\""));
		return true;
	}

	// drop-file <path> [<path> ...] (Dietrich 2026-11-22, the video editor's drag and drop: the owner's "when in strategy mode, allow drag and drop"): the files, as Windows'
	// WM_DROPFILES would hand them over, given to the editor through WM_COPYDATA (a dropped handle from another program can't be posted: it is only good in the program that
	// made it), one path a line, UTF-8; the editor takes them by the same call its WM_DROPFILES does (video_drop.h), so what it does with them is what a drop does. A path
	// with spaces goes in double quotes. Answered TRUE when the editor took the list; what it made of it is on the status line
	if (verb == "drop-file" && w.size() >= 2)
	{
		Drain(s);

		// the paths a letter at a time: a space ends one unless inside double quotes (no positions are summed)
		std::string const rest = Rest(line, 1);
		std::string text, path;
		bool quoted = false, inPath = false;

		auto const endPath = [&]()
		{
			if (inPath)
			{
				text += (text.empty() ? std::string() : std::string("\n")) + path;
				path.clear();
				inPath = false;
			}
		};

		for (char const c : rest)
		{
			if (quoted)
			{
				quoted = c != '"';
				path += quoted ? std::string(1, c) : std::string();
			}
			else if (c == '"')
			{
				quoted = true;
				inPath = true;
			}
			else if (c == ' ' || c == '\t' || c == '\r')
			{
				endPath();
			}
			else
			{
				path.push_back(c);
				inPath = true;
			}
		}

		endPath();

		COPYDATASTRUCT data = {};
		DWORD_PTR answer = 0;

		data.dwData = 0x44524f50ul;   // video_drop.h kVideoDropTag ('DROP')
		data.cbData = (DWORD)text.size();
		data.lpData = (PVOID)text.data();
		SendMessageTimeoutW(s->editor.window, WM_COPYDATA, 0, (LPARAM)&data, SMTO_ABORTIFHUNG, 10000, &answer);
		Check(answer == TRUE, label + (answer == TRUE ? std::string() : std::string(" -> the editor did not take the dropped files")));
		return true;
	}

	// click-video <name> (Dietrich 2026-11-22, the owner's Video mode): a click on the middle of one of Video mode's own on-screen controls, by its name, from the state
	// line's videocontrols "<name> <x> <y> <w> <h>|..." (video_mode_screen.h: Fit, Crop, Expand, Start-, Start+, End-, End+, Volume-, Volume+, Preview, Save)
	if (verb == "click-video" && w.size() >= 2)
	{
		AskState(s);
		std::string const controls = Field(s->last, "videocontrols");
		bool clicked = false;

		// a box's number read by hand: digits only, 0 to 16384 (a window's own pixels), anything else refused, so the middle below never overflows
		auto const boxNumber = [](std::string const &word, int *out)
		{
			int value = 0;

			if (word.empty() || word.size() > 5)
			{
				return false;
			}

			for (char const c : word)
			{
				if (c < '0' || c > '9')
				{
					return false;
				}

				value = value * 10 + (c - '0');
			}

			*out = value;
			return value <= 16384;
		};

		// the controls a letter at a time, each "<name> <x> <y> <w> <h>" between the | (no positions are summed)
		std::string one;

		for (size_t i = 0; i <= controls.size() && !clicked; i++)
		{
			if (i < controls.size() && controls[i] != '|')
			{
				one.push_back(controls[i]);
				continue;
			}

			std::vector<std::string> const parts = Words(one);
			int x = 0, y = 0, width = 0, height = 0;

			one.clear();

			if (parts.size() >= 5 && parts[0] == w[1] && boxNumber(parts[1], &x) && boxNumber(parts[2], &y) && boxNumber(parts[3], &width) && boxNumber(parts[4], &height))
			{
				ClickWindow(s, x + width / 2, y + height / 2, false, false);
				clicked = true;
			}
		}

		Check(clicked, label + (clicked ? std::string() : " (no control \"" + w[1] + "\" in \"" + controls + "\")"));
		return true;
	}

	if (verb == "camera-look" && w.size() >= 3)
	{
		Drain(s);
		PostMessageW(s->editor.window, WM_APP + 10, (WPARAM)4, (LPARAM)strtol(w[1].c_str(), NULL, 0));
		PostMessageW(s->editor.window, WM_APP + 10, (WPARAM)5, (LPARAM)strtol(w[2].c_str(), NULL, 0));

		std::string const said = WaitForLine(&s->editor, "AVP_OpenInteng: camera: ", 10000);

		Check(!said.empty() && said.find(" look ") != std::string::npos, label + " -> " + (said.empty() ? "no answer" : From(said, "camera")));
		return true;
	}

	if (verb == "pick-module" && w.size() >= 2)
	{
		// Adjacent Modules: picks the module with that object number, as a click on it would (kHarnessPickModule)
		Drain(s);
		PostMessageW(s->editor.window, WM_APP + 11, (WPARAM)strtol(w[1].c_str(), NULL, 0), 0);

		std::string const said = WaitForLine(&s->editor, "AVP_OpenInteng: pick-module: ", 10000);

		Check(said.find("picked") != std::string::npos, label + " -> " + (said.empty() ? "no answer" : From(said, "pick-module")));
		PictureAfter(s, line);
		return true;
	}

	if (verb == "pick-marker" && w.size() >= 2)
	{
		// Waypoint, AI modules: picks that entry point marker (kHarnessPickMarker)
		AskState(s);
		Drain(s);
		PostMessageW(s->editor.window, WM_APP + 12, (WPARAM)strtol(w[1].c_str(), NULL, 0), 0);

		std::string const said = WaitForLine(&s->editor, "AVP_OpenInteng: pick-marker: ", 10000);

		Check(said.find("picked") != std::string::npos, label + " -> " + (said.empty() ? "no answer" : From(said, "pick-marker")));
		PictureAfter(s, line);
		return true;
	}

	if (verb == "face-light" && w.size() >= 2)
	{
		// Light Sourcing: selects that light (counting from 1) and turns the camera on the spot to face it (kHarnessFaceLight)
		Drain(s);
		PostMessageW(s->editor.window, WM_APP + 8, (WPARAM)strtol(w[1].c_str(), NULL, 0), 0);

		std::string const said = WaitForLine(&s->editor, "AVP_OpenInteng: face-light: ", 10000);

		Check(said.find("selected") != std::string::npos, label + " -> " + (said.empty() ? "no answer" : From(said, "face-light")));
		return true;
	}

	if (verb == "wait" && w.size() >= 2)
	{
		// slept half a second at a time, so an editor that dies meanwhile stops the wait at once (editor_ended.h); the step loop says why
		DWORD const most = (DWORD)atoi(w[1].c_str()), start = GetTickCount();

		while (GetTickCount() - start < most && !EditorGone(s))
		{
			DWORD const left = most - (GetTickCount() - start);
			Sleep(left < 500 ? left : 500);
		}

		return true;
	}

	if (verb == "wait-status" && w.size() >= 3)
	{
		// wait-status <most ms> <words>: the state is asked every half second until the status line holds the words, at most that long (a run that takes its own time, as AutoLink's or
		// Delete Links', is read at its end on a busy machine too: a fixed wait read the words while it still ran)
		DWORD const most = (DWORD)atoi(w[1].c_str()), start = GetTickCount();
		std::string const wanted = Rest(line, 2);

		for (;;)
		{
			// a DEAD editor fails the wait at once (editor_ended.h), never read from its last answer
			if (!AskState(s) && EditorGone(s))
			{
				Check(false, label + " (" + s->gone + ", after " + std::to_string(GetTickCount() - start) + " ms)");
				s->goneSaid = true;
				return false;
			}

			if (Field(s->last, "status").find(wanted) != std::string::npos)
			{
				Check(true, label + " (after " + std::to_string(GetTickCount() - start) + " ms: the status says \"" + Field(s->last, "status") + "\")");
				return true;
			}

			if (GetTickCount() - start >= most)
			{
				break;
			}

			Sleep(500);
		}

		Check(false, label + " (the status never said \"" + wanted + "\" in " + std::to_string(most) + " ms; it says \"" + Field(s->last, "status") + "\")");
		return false;
	}

	if (verb == "picture" && w.size() >= 2)
	{
		int const n = atoi(w[1].c_str());
		bool const taken = TakePictures(&s->editor, s->recordFolder, n);

		Check(taken, label);

		if (taken)
		{
			s->pictureBefore = s->lastPicture;
			s->lastPicture = n;
		}

		return true;
	}

	// expect-pixels-changed <screen|panels|picture> <x> <y> <w> <h> [share]: in the newest picture against the one before it, at least
	// that share (0.05 if not given) of the box's pixels changed: something appeared or moved there (a dialog opened, a gun played).
	// expect-pixels-same <screen|panels|picture> <x> <y> <w> <h> [share]: at most that share (0.01 if not given) changed: nothing
	// happened there (a refused or empty action).
	// expect-pixels-changed-since <n> ... / expect-pixels-same-since <n> ...: the same against picture n (taken earlier with picture <n>), not the one before
	if ((verb == "expect-pixels-changed-since" || verb == "expect-pixels-same-since") && w.size() >= 7)
	{
		bool const wantChange = verb == "expect-pixels-changed-since";
		int const since = atoi(w[1].c_str());
		double const limit = w.size() >= 8 ? atof(w[7].c_str()) : (wantChange ? 0.05 : 0.01);
		std::string const kind = w[2];

		if ((kind != "screen" && kind != "panels" && kind != "picture") || since < 1 || since > 999 || s->lastPicture == 0)
		{
			Check(false, label + " (the kind must be screen, panels or picture, n 1 to 999, and a picture must have been taken)");
			return true;
		}

		double const share = ShareChangedIn(PictureOf(s, kind, since), PictureOf(s, kind, s->lastPicture), atoi(w[3].c_str()),
			atoi(w[4].c_str()), atoi(w[5].c_str()), atoi(w[6].c_str()));
		char words[160];

		sprintf_s(words, " (%s-%d against %s-%d: %.3f of the box changed)", kind.c_str(), s->lastPicture, kind.c_str(), since, share);
		Check(share >= 0.0 && (wantChange ? share >= limit : share <= limit), label + words);
		return true;
	}

	if ((verb == "expect-pixels-changed" || verb == "expect-pixels-same") && w.size() >= 6)
	{
		bool const wantChange = verb == "expect-pixels-changed";
		double const limit = w.size() >= 7 ? atof(w[6].c_str()) : (wantChange ? 0.05 : 0.01);
		std::string const kind = w[1];

		if (kind != "screen" && kind != "panels" && kind != "picture")
		{
			Check(false, label + " (the kind must be screen, panels or picture)");
			return true;
		}

		if (s->lastPicture == 0 || s->pictureBefore == 0)
		{
			Check(false, label + " (two pictures are needed; take them with picture <n> or steps that change something)");
			return true;
		}

		double const share = ShareChangedIn(PictureOf(s, kind, s->pictureBefore), PictureOf(s, kind, s->lastPicture), atoi(w[2].c_str()),
			atoi(w[3].c_str()), atoi(w[4].c_str()), atoi(w[5].c_str()));
		char words[160];

		sprintf_s(words, " (%s-%d against %s-%d: %.3f of the box changed)", kind.c_str(), s->lastPicture, kind.c_str(), s->pictureBefore, share);
		Check(share >= 0.0 && (wantChange ? share >= limit : share <= limit), label + words);
		return true;
	}

	// expect-colour <screen|panels|picture> <x> <y> <w> <h> <rrggbb> [share] [tolerance]: in the newest picture, at least that share (0.01
	// if not given) of the box's pixels are that colour, each channel within the tolerance (24 if not given): a module tinted purple
	// 7f007f, a light marker's green side, a picked thing's colour (the other checks only compare brightness). expect-colour-at-most: AT MOST that share (a textured view has
	// next to no pure white, the white wireframe has a lot)
	bool const atMost = verb == "expect-colour-at-most";

	if ((verb == "expect-colour" || atMost) && w.size() >= 7)
	{
		std::string const kind = w[1];
		unsigned long const rgb = strtoul(w[6].c_str(), NULL, 16);
		double const limit = w.size() >= 8 ? atof(w[7].c_str()) : 0.01;
		int const tolerance = w.size() >= 9 ? atoi(w[8].c_str()) : 24;
		int const want[3] = { (int)((rgb >> 16) & 0xff), (int)((rgb >> 8) & 0xff), (int)(rgb & 0xff) };   // red, green, blue

		if (s->lastPicture == 0)
		{
			Check(false, label + " (no picture taken yet)");
			return true;
		}

		Picture const p = PictureOf(s, kind, s->lastPicture);
		int const x = atoi(w[2].c_str()), y = atoi(w[3].c_str());
		int const left = (std::max)(0, x), top = (std::max)(0, y);
		int const right = p.Read() ? (std::min)((int)p.width, x + atoi(w[4].c_str())) : 0;
		int const bottom = p.Read() ? (std::min)((int)p.height, y + atoi(w[5].c_str())) : 0;
		size_t matching = 0, all = 0;

		for (int row = top; row < bottom; row++)
		{
			for (int column = left; column < right; column++)
			{
				BYTE const *const at = &p.bgra[((size_t)row * p.width + (size_t)column) * 4];   // blue, green, red, alpha

				all++;
				matching += abs(at[2] - want[0]) <= tolerance && abs(at[1] - want[1]) <= tolerance && abs(at[0] - want[2]) <= tolerance ? 1 : 0;
			}
		}

		double const share = all ? (double)matching / (double)all : -1.0;
		char words[160];

		sprintf_s(words, " (%s-%d: %.4f of the box is %06lx)", kind.c_str(), s->lastPicture, share, rgb);
		Check(all > 0 && (atMost ? share <= limit : share >= limit), label + words);
		return true;
	}

	// expect-moving <screen|panels|picture> <x> <y> <w> <h> <count> <ms> [share]: takes <count> pictures <ms> apart and passes when any two
	// in a row differ by at least that share (0.01 if not given) in the box: something moves there, whatever phase it is in (an animation
	// that rests at both ends of its loop shows the same picture at two unlucky moments, so one pair is not enough)
	if (verb == "expect-moving" && w.size() >= 8)
	{
		std::string const kind = w[1];
		int const count = atoi(w[6].c_str()), gap = atoi(w[7].c_str());
		double const limit = w.size() >= 9 ? atof(w[8].c_str()) : 0.01;
		double most = -1.0;
		int previous = 0;

		for (int i = 0; i < count && i < 40; i++)
		{
			int const n = s->nextPicture++;

			if (!TakePictures(&s->editor, s->recordFolder, n))
			{
				Check(false, label + " (picture " + std::to_string(n) + " not taken)");
				return true;
			}

			if (previous != 0)
			{
				double const share = ShareChangedIn(PictureOf(s, kind, previous), PictureOf(s, kind, n), atoi(w[2].c_str()), atoi(w[3].c_str()),
					atoi(w[4].c_str()), atoi(w[5].c_str()));

				most = (std::max)(most, share);
			}

			s->pictureBefore = s->lastPicture;
			s->lastPicture = n;
			previous = n;
			Sleep((DWORD)gap);
		}

		char words[120];

		sprintf_s(words, " (the most changed between two pictures in a row: %.4f of the box)", most);
		Check(most >= limit, label + words);
		return true;
	}

	// expect-not-empty [share]: the newest picture of the 3D view (picture-<n>.png) has at least that share (0.10 if not given) of its
	// pixels away from its one background brightness: something is drawn in it, it isn't an empty view.
	if (verb == "expect-not-empty")
	{
		double const limit = w.size() >= 2 ? atof(w[1].c_str()) : 0.10;

		if (s->lastPicture == 0)
		{
			Check(false, label + " (no picture taken yet)");
			return true;
		}

		double const share = ShareNotBackground(PictureOf(s, "picture", s->lastPicture));
		char words[120];

		sprintf_s(words, " (picture-%d: %.3f of it is not the background)", s->lastPicture, share);
		Check(share >= limit, label + words);
		return true;
	}

	if (verb == "reopen-level")
	{
		Drain(s);
		PostMessageW(s->editor.window, WM_APP + 14, 0, 0);

		std::string const said = WaitForLine(&s->editor, "AVP_OpenInteng: reopen: ", 60000);

		bool const refusedAsked = w.size() >= 2 && w[1] == "refused";
		bool const refused = said.find("not opened again") != std::string::npos;

		Check(refusedAsked ? refused : said.find("opened again") != std::string::npos && !refused, label + " -> " + (said.empty() ? "no answer" : said));
		AskState(s);
		return true;
	}

	if (verb == "save-copy")
	{
		Drain(s);
		PostMessageW(s->editor.window, WM_APP + 5, 0, 0);

		std::string const said = WaitForLine(&s->editor, "AVP_OpenInteng: copy: ", 20000);
		std::string const prefix = "AVP_OpenInteng: copy: Saved a copy: ";

		if (said.compare(0, prefix.size(), prefix) != 0)
		{
			Check(false, label + " -> " + (said.empty() ? "no answer" : said));
			return false;
		}

		s->previous = s->copy.empty() ? s->level : s->copy;
		s->copy = said.substr(prefix.size());

		// each Save a copy writes the same file: a scenario that compares one copy with the one before it needs each kept, so the file is copied to Name-stage<n>.rif, which is {copy} from here on
		{
			static int stage = 0;
			std::string kept = s->copy;
			size_t const dot = kept.rfind('.');

			kept = (dot == std::string::npos ? kept : kept.substr(0, dot)) + "-stage" + std::to_string(++stage) + ".rif";

			if (CopyFileA(s->copy.c_str(), kept.c_str(), FALSE))
			{
				s->copy = kept;
			}
		}
		s->saves.push_back(s->copy);
		Check(true, label + " -> " + s->copy);
		return true;
	}

	if (verb == "verify" && w.size() >= 2)
	{
		std::string command = Rest(line, 1);

		command = Replace(command, "{orig}", "\"" + s->level + "\"");
		command = Replace(command, "{prev}", "\"" + s->previous + "\"");
		command = Replace(command, "{copy}", "\"" + s->copy + "\"");

		// the editor's own folder (the settings file AVP_OpenInteng.ini is in it), with its closing backslash
		{
			std::string folder = Narrow(s->editor.folder);

			// the editor may have been named with forward slashes, which a program such as findstr takes for the start of a switch
			for (char &letter : folder)
			{
				if (letter == '/')
				{
					letter = '\\';
				}
			}

			command = Replace(command, "{exedir}", folder);
		}

		// the run's record folder, where its pictures are (picture-<n>.png), with its closing backslash (added 2026-11-17 by Drake: #78's check compares the screenshot with them)
		command = Replace(command, "{record}", Narrow(s->recordFolder) + "\\");

		for (size_t n = 0; n < s->saves.size() && n < 9; n++)
		{
			command = Replace(command, "{save" + std::to_string(n + 1) + "}", "\"" + s->saves[n] + "\"");
		}

		std::vector<std::string> lines;
		int const code = RunProgram(command, &lines);

		for (std::string const &text : lines)
		{
			printf("    | %s\n", text.c_str());
		}

		Check(code == 0, label + " -> exit " + std::to_string(code));
		return true;
	}

	Check(false, label + " (a step the scenario file's language doesn't have)");
	return false;
}

int main(int argc, char **argv)
{
	setvbuf(stdout, NULL, _IONBF, 0);

	// --idle-ms: how long ago the machine was last touched, in milliseconds ("idle <ms>"), and nothing else (idle_gate.h): run-shards.sh asks it once a minute WHILE a
	// file that waited for nobody at the machine runs, and stops that run when someone comes back (Vasquez's review, 2026-11-22). No editor, no file, no window.
	if (argc == 2 && strcmp(argv[1], "--idle-ms") == 0)
	{
		LASTINPUTINFO info = {};
		info.cbSize = sizeof(info);

		if (!GetLastInputInfo(&info))
		{
			printf("idle unknown\n");
			return 2;
		}

		printf("idle %u\n", idle_gate::IdleMs(GetTickCount(), info.dwTime));
		return 0;
	}

	// --shot-to-scenario <png, or a pattern with *: the newest that matches>: the from-shot line a screenshot gives and what it stands for (from_shot.h), printed; no editor is started
	if (argc >= 2 && strcmp(argv[1], "--shot-to-scenario") == 0)
	{
		std::string png, why;
		std::vector<std::string> lines;

		if (argc < 3 || !fromshot::Lines(argv[2], &png, &lines, &why))
		{
			printf("shot-to-scenario: %s\n", argc < 3 ? "usage: drive_scenario --shot-to-scenario <shot .png, or a pattern with *>" : why.c_str());
			return argc < 3 ? 2 : 1;
		}

		for (std::string const &one : lines)
		{
			printf("%s\n", one.c_str());
		}

		return 0;
	}

	if (argc < 4)
	{
		printf("usage: drive_scenario <the editor's .exe> <a folder for the pictures and the record> <a scenario file>\n");
		return 2;
	}

	Scenario s;
	std::wstring const exe = Widen(argv[1]);

	s.recordFolder = Widen(argv[2]);
	CreateDirectoryW(s.recordFolder.c_str(), NULL);

	// The scenario file is named on the command line: its name may hold no "..", it is made a full path by Windows, and that path is read only when it ends in ".txt" and is a
	// plain file (not a folder, not a link), so the name reaches that one scenario and nothing beside it (Snyk Code, PR #79)
	if (strstr(argv[3], "..") != nullptr)
	{
		printf("drive_scenario: the scenario file isn't read: its name holds \"..\": %s\n", argv[3]);
		return 2;
	}

	char scenarioFull[MAX_PATH];
	DWORD const scenarioLength = GetFullPathNameA(argv[3], MAX_PATH, scenarioFull, NULL);

	if (scenarioLength < 5 || scenarioLength >= MAX_PATH || _stricmp(scenarioFull + scenarioLength - 4, ".txt") != 0)
	{
		printf("drive_scenario: the scenario file isn't read: its full path must end in .txt and be shorter than %d letters: %s\n", MAX_PATH, argv[3]);
		return 2;
	}

	std::string const scenarioPath = scenarioFull;
	DWORD const scenarioAttributes = GetFileAttributesA(scenarioPath.c_str());

	if (scenarioAttributes == INVALID_FILE_ATTRIBUTES || (scenarioAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0)
	{
		printf("drive_scenario: the scenario file isn't read: it isn't there, or it is a folder or a link: %s\n", argv[3]);
		return 2;
	}

	std::ifstream file(scenarioPath);

	if (!file)
	{
		printf("drive_scenario: the scenario file can't be read: %s\n", argv[3]);
		return 2;
	}

	std::vector<std::string> steps;
	std::string line;
	std::wstring startArguments;
	std::vector<std::string> makeLevels;   // make-level commands, run before the editor starts

	while (std::getline(file, line))
	{
		if (!line.empty() && line.back() == '\r')
		{
			line.pop_back();
		}

		size_t const first = line.find_first_not_of(" \t");

		if (first == std::string::npos || line[first] == '#')
		{
			continue;
		}

		line = line.substr(first);

		std::vector<std::string> const w = Words(line);

		if (w[0] == "screen" && w.size() >= 2 && steps.empty())
		{
			startArguments = L" -screen \"" + Widen(Rest(line, 1)) + L"\"";   // a screen's name can have a space in it ("Module Linking")
			continue;
		}

		if (w[0] == "env" && steps.empty())
		{
			// env <AVP_NAME> <value> (the usage above): only AVP_ names, letters, digits and _; the value is the rest of the line
			bool plainName = w.size() >= 3 && w[1].size() > 4 && w[1].size() <= 64 && w[1].compare(0, 4, "AVP_") == 0;

			for (size_t k = 0; plainName && k < w[1].size(); k++)
			{
				char const letter = w[1][k];

				plainName = (letter >= 'A' && letter <= 'Z') || (letter >= '0' && letter <= '9') || letter == '_';
			}

			if (!plainName || !SetEnvironmentVariableA(w[1].c_str(), Rest(line, 2).c_str()))
			{
				printf("  FAILED  env: only \"env AVP_<NAME> <value>\" is taken (%s)\n", line.c_str());
				return 1;
			}

			printf("  ok  env %s for the editor\n", w[1].c_str());
			continue;
		}

		if (w[0] == "start-args" && w.size() >= 2 && steps.empty())
		{
			// more words for the editor's command line, after -screen (for example "-viewmode headlight": the Hierarchy scenario's model is lit by it). {exedir} is the editor's folder,
			// with its closing backslash: a scenario can open the copy an earlier run saved (<exedir>AVP_OpenInteng/edited/<level>-edited.rif). (Added 2026-11-16 by Vasquez.)
			// {scenariodir} is the scenario file's own folder, whole and with its closing backslash: files a scenario needs travel with it in the repository, whichever worktree runs it
			// (startup-screen-frost.md SS-13's IntengOptions.ini folders; added 2026-11-16 by Frost).
			std::string words = Rest(line, 1);
			std::string const folderOfEditor = Narrow(exe.substr(0, exe.find_last_of(L"\\/") + 1));
			char wholeScenario[MAX_PATH] = "";
			DWORD const wholeLength = GetFullPathNameA(argv[3], sizeof(wholeScenario), wholeScenario, nullptr);
			std::string const scenarioWhole = wholeLength > 0 && wholeLength < sizeof(wholeScenario) ? std::string(wholeScenario) : std::string(argv[3]);
			std::string const folderOfScenario = scenarioWhole.substr(0, scenarioWhole.find_last_of("\\/") + 1);

			words = Replace(words, "{exedir}", folderOfEditor);
			words = Replace(words, "{scenariodir}", folderOfScenario);
			startArguments += L" " + Widen(words);
			continue;
		}

		if (w[0] == "make-level" && w.size() >= 2 && steps.empty())
		{
			// a test tool makes a level before the editor starts (added 2026-11-17 by Drake): the folders as start-args gives them, with backslashes (a program may take a / for a switch)
			std::string command = Rest(line, 1);
			std::string folderOfEditor = Narrow(exe.substr(0, exe.find_last_of(L"\\/") + 1));
			char wholeScenario[MAX_PATH] = "";
			DWORD const wholeLength = GetFullPathNameA(argv[3], sizeof(wholeScenario), wholeScenario, nullptr);
			std::string const scenarioWhole = wholeLength > 0 && wholeLength < sizeof(wholeScenario) ? std::string(wholeScenario) : std::string(argv[3]);
			std::string const folderOfScenario = scenarioWhole.substr(0, scenarioWhole.find_last_of("\\/") + 1);

			for (char &letter : folderOfEditor)
			{
				letter = letter == '/' ? '\\' : letter;
			}

			command = Replace(command, "{exedir}", folderOfEditor);
			command = Replace(command, "{scenariodir}", folderOfScenario);
			makeLevels.push_back(command);
			continue;
		}

		if (w[0] == "from-shot" && w.size() >= 2 && steps.empty())
		{
			// from-shot <png, or a pattern with *: the newest that matches> (from_shot.h): the editor started with -shot <that png> (open_at.h: the shot's map from the editor's data folder, only with
			// the shot's sha256, its camera, screen and selected object); {exedir} and {scenariodir} as start-args has them (Drake 11-18)
			std::string const folderOfEditor = Narrow(exe.substr(0, exe.find_last_of(L"\\/") + 1));
			char wholeScenario[MAX_PATH] = "";
			DWORD const wholeLength = GetFullPathNameA(argv[3], sizeof(wholeScenario), wholeScenario, nullptr);
			std::string const scenarioWhole = wholeLength > 0 && wholeLength < sizeof(wholeScenario) ? std::string(wholeScenario) : std::string(argv[3]);
			std::string const folderOfScenario = scenarioWhole.substr(0, scenarioWhole.find_last_of("\\/") + 1);
			std::string const pattern = Replace(Replace(Rest(line, 1), "{exedir}", folderOfEditor), "{scenariodir}", folderOfScenario);
			std::string why;
			std::string const png = fromshot::ShotFile(pattern, &why);

			if (png.empty())
			{
				printf("  FAILED  from-shot %s: %s\n", pattern.c_str(), why.c_str());
				printf("SOME CHECKS FAILED\n");
				return 1;
			}

			printf("  ok  from-shot %s: the editor opens it with -shot\n", png.c_str());
			startArguments += L" -shot \"" + Widen(png) + L"\"";
			continue;
		}

		if (w[0] == "level" && w.size() >= 2)
		{
			s.level = Rest(line, 1);
			continue;
		}

		if (w[0] == "pictures" && w.size() >= 2)
		{
			s.pictures = w[1] != "off";
			continue;
		}

		steps.push_back(line);
	}

	// THE IDLE GATE (idle_gate.h; Bishop 2, 2026-11-22: no run that opens the editor while the owner is at his PC): with IDLE_SECONDS in the environment
	// (run-shards.sh sets it), nothing is written and the editor isn't started unless nobody has touched the machine for that long. Without it, as before.
	int idleSeconds = 0;
	LASTINPUTINFO idleAtStart = {};
	idleAtStart.cbSize = sizeof(idleAtStart);
	{
		char given[16] = "";
		DWORD const length = GetEnvironmentVariableA("IDLE_SECONDS", given, (DWORD)sizeof(given));
		idleSeconds = length >= sizeof(given) ? -1 : idle_gate::ParseSeconds(given);
	}

	if (idleSeconds < 0)
	{
		printf("  FAILED  IDLE_SECONDS is not a whole number of seconds from 1 to %d, so the run can't tell whether the machine is left alone: not started\n", idle_gate::kMostSeconds);
		return 2;
	}

	if (idleSeconds > 0)
	{
		if (!GetLastInputInfo(&idleAtStart))
		{
			printf("  FAILED  Windows wouldn't say when the machine was last touched (GetLastInputInfo), so the run can't tell whether it is left alone: not started\n");
			return 2;
		}

		uint32_t const idleMs = idle_gate::IdleMs(GetTickCount(), idleAtStart.dwTime);

		if (!idle_gate::MayStart(idleMs, idleSeconds))
		{
			printf("  NOT STARTED  the machine was touched %u s ago, under the %d s it must be left alone (IDLE_SECONDS): the editor was not started, and nothing written "
				"(run-shards tries again later)\n", idleMs / 1000, idleSeconds);
			return idle_gate::kIdleNotStarted;
		}

		printf("  ok  nobody has touched the machine for %u s, at least the %d s IDLE_SECONDS asks: the editor may start\n", idleMs / 1000, idleSeconds);
	}

	if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)))
	{
		fprintf(stderr, "drive_scenario: COM wouldn't start, so no picture could be read\n");
		return 2;
	}

	POINT cursorBefore = {};
	GetCursorPos(&cursorBefore);
	HWND const foregroundBefore = GetForegroundWindow();

	// The level the scenario names is the one the editor's settings open: its name (the file's, without the folder and the ending) is written as Level= in the AVP_OpenInteng.ini beside the editor's program
	// (the run folder is a COPY made for the run; the harness never changes any other file)
	std::string levelName, levelIni;   // what the block below wrote, for the check after the made levels

	if (!s.level.empty())
	{
		size_t const slash = s.level.find_last_of("/\\");
		std::string name = slash == std::string::npos ? s.level : s.level.substr(slash + 1);
		size_t const dot = name.find_last_of('.');

		if (dot != std::string::npos)
		{
			name = name.substr(0, dot);
		}

		size_t const folderEnd = exe.find_last_of(L"\\/");
		std::string const ini = folderEnd == std::wstring::npos ? std::string() : Narrow(exe.substr(0, folderEnd + 1)) + "AVP_OpenInteng.ini";

		if (!ini.empty() && !name.empty() && GetFileAttributesA(ini.c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			WritePrivateProfileStringA("Editor", "Level", name.c_str(), ini.c_str());
			printf("  (the editor's level is %s: Level=%s in %s)\n", s.level.c_str(), name.c_str(), ini.c_str());
			levelName = name;
			levelIni = ini;
		}
	}

	// the made levels first, fresh every run: one that isn't made stops the scenario before the editor starts
	bool made = true;

	for (std::string const &command : makeLevels)
	{
		std::vector<std::string> lines;
		int const code = RunProgram(command, &lines);

		for (std::string const &text : lines)
		{
			printf("    | %s\n", text.c_str());
		}

		Check(code == 0, "make-level " + command + " -> exit " + std::to_string(code));
		made = made && code == 0;
	}

	// the level line's file is the one the editor opens (LevelLineIsTheFileOpened), checked after the made levels, since one of them can be it
	if (made && !levelName.empty())
	{
		std::string why;
		bool const same = LevelLineIsTheFileOpened(s.level, levelName, levelIni, startArguments, &why);

		Check(same, "level " + s.level + " is the file the editor opens" + (same ? std::string() : ": " + why));
		made = same;
	}

	// the editor's log, and how much of it is there before it starts: only what this editor writes after is read for its crash lines (editor_ended.h).
	// (run-shards empties it before each file; a run outside it may find an older run's lines there, which are left alone.)
	{
		size_t const slash = exe.find_last_of(L"\\/");
		s.logPath = slash == std::wstring::npos ? std::wstring() : exe.substr(0, slash + 1) + L"settings\\avp_log.txt";
		WIN32_FILE_ATTRIBUTE_DATA there = {};

		if (!s.logPath.empty() && GetFileAttributesExW(s.logPath.c_str(), GetFileExInfoStandard, &there))
		{
			s.logRead = ((uint64_t)there.nFileSizeHigh << 32) | there.nFileSizeLow;
		}
	}

	bool const started = made && StartTheEditor(exe, s.recordFolder, &s.editor, startArguments);

	Check(started, "the editor starts, with -picture" + Narrow(startArguments) + ", and its window and view are found" +
		(started ? std::string() : " (" + (made ? s.editor.startProblem : std::string("a made level failed, so it wasn't started")) + ")"));   // why not, from the harness's own look (Frost 11-18)

	if (started)
	{
		s.editor.keepLines = true;
		Check(AskState(&s, 60000), "the editor says its state");
		printf("  (%s)\n", s.last.empty() ? "no state line" : From(s.last, "scenario: ").c_str());

		for (size_t i = 0; i < steps.size(); i++)
		{
			bool const goOn = DoStep(&s, steps[i], (int)i + 1);

			// A DEAD EDITOR stops the scenario after any step, saying so once (editor_ended.h): a step that doesn't wait would only post to a window
			// that is gone. kill-editor's end is on purpose: the verify steps after it go on.
			if (!s.goneOnPurpose && EditorGone(&s))
			{
				if (!s.goneSaid)
				{
					Check(false, "after step " + std::to_string(i + 1) + ": " + s.gone);
					s.goneSaid = true;
				}

				printf("  the scenario stops here\n");
				break;
			}

			if (!goOn)
			{
				printf("  the scenario stops here\n");
				break;
			}
		}
	}

	// A DEAD editor isn't asked to close (Dietrich's review of 0c57dcbe, nit 1): it can't answer (Windows' crash report may still hold it), so the ask would
	// only wait its 30 s and add a second FAILED to the one that said CRASHED. It is ended at once, by its own handle, and the close below has nothing to do.
	if (!s.gone.empty() && !s.goneOnPurpose && s.editor.process.hProcess)
	{
		if (WaitForSingleObject(s.editor.process.hProcess, 0) != WAIT_OBJECT_0)
		{
			TerminateProcess(s.editor.process.hProcess, 99);
			WaitForSingleObject(s.editor.process.hProcess, 10000);
		}

		CloseHandle(s.editor.process.hProcess);
		CloseHandle(s.editor.process.hThread);
		s.editor.process = PROCESS_INFORMATION();
		printf("  (the dead editor was ended at once, not asked to close)\n");
	}

	CloseTheEditor(&s.editor);

	POINT cursorAfter = {};
	GetCursorPos(&cursorAfter);
	HWND const foregroundAfter = GetForegroundWindow();
	bool const cursorStill = cursorAfter.x == cursorBefore.x && cursorAfter.y == cursorBefore.y;
	wchar_t beforeTitle[80] = L"", afterTitle[80] = L"";

	GetWindowTextW(foregroundBefore, beforeTitle, 80);
	GetWindowTextW(foregroundAfter, afterTitle, 80);

	// The harness only POSTS messages to the editor's own windows: it never calls SetCursorPos, SendInput, SetForegroundWindow or mouse_event, so it cannot move the real cursor or change the foreground. A change
	// seen here is therefore somebody at the computer (Melanikus at his PC: the foreground is his Chrome), and it is a NOTE, not a failure of the steps (Bishop, 11-15). The other harness's cursor watch
	// (the editor's own log of cursor moves, drive_editor.cpp) is what says whether the EDITOR moved it.
	if (cursorStill && foregroundAfter == foregroundBefore)
	{
		Check(true, "the real cursor never moved and the foreground window never changed: nobody at the computer lost it");
	}
	else
	{
		printf("  NOTE  the real cursor %s and the foreground window was \"%s\" and is \"%s\": somebody used the computer during the run (the steps only post messages, they cannot do that)\n",
			cursorStill ? "did not move" : "moved", Narrow(beforeTitle).c_str(), Narrow(afterTitle).c_str());
	}

	// THE IDLE GATE, at the end: with IDLE_SECONDS, a run someone touched (any input Windows counts, the harness's own posted messages not among them) is
	// not a result: a FAILED check, so never green (run-shards flags it "touched")
	if (idleSeconds > 0)
	{
		LASTINPUTINFO idleAtEnd = {};
		idleAtEnd.cbSize = sizeof(idleAtEnd);
		bool const read = GetLastInputInfo(&idleAtEnd) != 0;
		bool const touched = !read || idle_gate::Touched(idleAtStart.dwTime, idleAtEnd.dwTime);
		std::string const words = !read ? "Windows wouldn't say when the machine was last touched, so the run can't be taken as untouched" :
			touched ? "the machine was TOUCHED during the run, " + std::to_string(idle_gate::IdleMs(GetTickCount(), idleAtEnd.dwTime) / 1000) + " s before its end: a run someone touched "
			"is not a result" : "nobody touched the machine during the run";

		Check(!touched, words + " (IDLE_SECONDS)");
	}

	CoUninitialize();

	if (checksAllowed > 0)
	{
		printf("  NOTE  %d check%s allowed to fail failed (the ALLOWED lines): not counted\n", checksAllowed, checksAllowed == 1 ? "" : "s");
	}

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
