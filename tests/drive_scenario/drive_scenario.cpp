// drive_scenario.cpp: the SCENARIO harness: a hidden run of the editor that does a mode's main job from a small text file, step by step, and writes a picture after each step. Written 2026-11-15 by
// Melanikus (Apone) for AVP_OpenInteng, at Bishop's order (Melanikus: "an MVP means IT WORKS"): a mode's MVP claim needs its scenario passing. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// It is Hicks's simulated-input harness (tests/drive_editor/drive_editor.cpp, whose parts it uses) with a script: the editor is started with -picture (hidden, nobody at the screen) on a screen,
// and every step is a window message posted to the editor's own windows, as Windows would hand them over. It never moves the real mouse, never types on the real keyboard and never brings a
// window forward; at the end it checks that the cursor and the foreground window are where they were. The editor answers six messages only in a -picture run (editor_main.cpp, kHarness*).
//
// USAGE: drive_scenario.exe <the editor's .exe> <a folder for the pictures and the record> <a scenario file>
// It prints "ok" or "FAILED" lines, writes the pictures (picture-<n>.png, panels-<n>.png) into the record folder, and gives back 0 only if every check passed.
//
// THE SCENARIO FILE: one step a line; a line starting with # and an empty line do nothing.
//   screen <Name>               first: the editor starts on that screen's bar (-screen), as the Screens menu names it ("Waypoint", "Module Linking", "Path")
//   level <a .rif path>         the level the editor's settings open (for verify's {orig})
//   make-level <command>        before the editor starts: a TEST TOOL makes a level the scenario then opens (tests/scenarios/make_strategy_level.cpp: a stock copy with the strategy bytes a
//                               kind no stock level holds needs); {exedir} and {scenariodir} as in start-args. It runs fresh every run; an exit other than 0 fails the scenario and the
//                               editor is not started. (Added 2026-11-17 by Drake, strategy-closeout-drake.md E-5: made levels are made by a tool, never by hand.)
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
//   click-wp <index|current> <top|side|front> [right] [shift]   a click on the middle of that waypoint's box in that one of the three views (the editor says where it is: kHarnessLocate)
//   drag-win <x0> <y0> <x1> <y1>        press at the first place, move to the second in steps, let go
//   type <text>                 the characters, one WM_CHAR each, to the editor's window (into an open prompt); key <Enter|Esc|Back|Tab|Left|Right|Up|Down|Space>
//                               (F1 to F12 and PrintScreen too); key-up <k>: only the key's letting go, as Windows hands Print Screen to a window
//   camera <x> <y> <z> <turn>   puts the camera there (millimetres; turn 0 looks along z, 1024 along x) (kHarnessCamera)
//   face-light <n>              Light Sourcing: selects light n (counting from 1) and turns the camera on the spot to face it (kHarnessFaceLight)
//   repeat <most> <key> <hold ms> <status_words> | <step> | <step>   the steps, again with the key held between, until the status holds the words (_ is a space): picks that depend on the camera
//   wait <milliseconds>; wait-status <most milliseconds> <words>: the state asked every half second until the status line holds the words (FAILED if it never does in that time)
//   picture <n>                 a picture now (1 to 999); the automatic ones are numbered from 101
//   expect-pixels-changed <screen|panels|picture> <x> <y> <w> <h> [share]   the newest picture against the one before: at least that share
//                               (default 0.05) of the box changed (a dialog appeared, a gun moved); expect-pixels-same ... [share]: at most
//                               that share (default 0.01) changed (nothing happened there)
//   expect-number <field> <label> <least> <most>   the number right after <label> (_ a space) in that state-line field is in the range
//   expect-pixels-changed-since <n> <kind> <x> <y> <w> <h> [share]   the same, the newest picture against picture n (one taken with picture <n>),
//                               not the one before: dialog steps between them take their own pictures; expect-pixels-same-since likewise
//   expect-colour <kind> <x> <y> <w> <h> <rrggbb> [share] [tolerance]   that share of the box is that colour (channels within the tolerance)
//   expect-not-empty [share]    the newest picture of the 3D view has at least that share (default 0.10) of pixels away from its background
//   expect-dialog <words> / expect-no-dialog / expect-dialog-rows <a|b|...> / click-row <n>   inteng's list dialogs, from the state line's
//                               dialog / dialogrows / dialogrow1 fields (see the contract at click-row); a row is chosen by a click
//   expect-dialog-kind <list|message|confirm|text> / click-button <label> / expect-dialog-text <words>   message, confirm and text dialogs
//                               (fields dialogkind, dialogbuttons, dialogtext; contract at expect-dialog-kind); a button is clicked, never keyed
//   save-copy                   Save a copy, as File > Save a copy does; the copy's path is {copy} from here on
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
static bool AskState(Scenario *s, DWORD waitMs = 10000)
{
	Drain(s);
	PostMessageW(s->editor.window, WM_APP + 6, 0, 0);

	std::string const line = WaitForLine(&s->editor, "AVP_OpenInteng: scenario: ", waitMs);

	if (line.empty())
	{
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
static void DragWindow(Scenario *s, int x0, int y0, int x1, int y1)
{
	int ax = x0, ay = y0, bx = x1, by = y1;
	HWND const target = TargetOf(s, &ax, &ay);

	TargetOf(s, &bx, &by);

	// the second place is in the same window's pixels as the first: the view's origin is taken off both
	bx = x1 - (x0 - ax);
	by = y1 - (y0 - ay);

	PostMessageW(target, WM_MOUSEMOVE, 0, MAKELPARAM(ax, ay));
	PostMessageW(target, WM_LBUTTONDOWN, MK_LBUTTON, MAKELPARAM(ax, ay));
	Sleep(150);

	for (int step = 1; step <= 8; step++)
	{
		PostMessageW(target, WM_MOUSEMOVE, MK_LBUTTON, MAKELPARAM(ax + (bx - ax) * step / 8, ay + (by - ay) * step / 8));
		Sleep(80);
	}

	PostMessageW(target, WM_LBUTTONUP, 0, MAKELPARAM(bx, by));
	Sleep(600);
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
	// F1 to F9: the camera slots' keys (Shift+F1 saves slot 0, F1 restores it)
	if (name.size() == 2 && name[0] == 'F' && name[1] >= '1' && name[1] <= '9') return VK_F1 + (UINT)(name[1] - '1');
	if (name == "F10") return VK_F10;
	// F11 and F12 restore camera slots 10 and 11; with Shift they save nothing (FUN_004705a0); Print Screen takes the editor's screenshot (#78) (all three added 2026-11-17 by Drake)
	if (name == "F11") return VK_F11;
	if (name == "F12") return VK_F12;
	if (name == "PrintScreen") return VK_SNAPSHOT;
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

		Check(said.find(" done:") != std::string::npos, label + " -> " + (said.empty() ? "no answer" : said.substr(said.find("item "))));

		// the state line the item itself asked for comes with the next frame, then its geometry line
		std::string const state = WaitForLine(&s->editor, "AVP_OpenInteng: scenario: ", 10000);

		if (state.empty())
		{
			Check(false, label + " (the editor gave no state line)");
			return false;
		}

		s->last = state;
		TakeGeometry(s);
		printf("  (%s)\n", s->last.substr(s->last.find("scenario: ")).c_str());
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

		// a list of many rows is paged: when the row is not on the page shown, the Next button is clicked and the page searched again (at most 12 pages)
		for (int page = 0; page < 12 && !clicked; page++)
		{
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
		printf("  (%s)\n", s->last.empty() ? "no state line" : s->last.substr(s->last.find("scenario: ")).c_str());
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
		printf("  (%s)\n", s->last.empty() ? "no state line" : s->last.substr(s->last.find("scenario: ")).c_str());
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

	if (verb == "move-win" && w.size() >= 3)
	{
		int x = atoi(w[1].c_str()), y = atoi(w[2].c_str());
		HWND const target = TargetOf(s, &x, &y);

		PostMessageW(target, WM_MOUSEMOVE, 0, MAKELPARAM(x, y));
		Sleep(600);
		AskState(s);
		Check(true, label);
		printf("  (%s)\n", s->last.empty() ? "no state line" : s->last.substr(s->last.find("scenario: ")).c_str());
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

		ClickWindow(s, atoi(w[1].c_str()), atoi(w[2].c_str()), right, shift);
		AskState(s);
		Check(true, label);
		printf("  (%s)\n", s->last.empty() ? "no state line" : s->last.substr(s->last.find("scenario: ")).c_str());
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
	//   click-button <label>          a click in the middle of that button where it is drawn (_ for a space in the label)
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

		while (!clicked && from <= buttons.size() && !buttons.empty())
		{
			size_t const to = buttons.find('|', from);
			std::vector<std::string> const parts = Words(buttons.substr(from, to == std::string::npos ? std::string::npos : to - from));

			if (parts.size() >= 5 && parts[0] == w[1])
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
		printf("  (%s)\n", s->last.empty() ? "no state line" : s->last.substr(s->last.find("scenario: ")).c_str());
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
		printf("  (%s)\n", s->last.empty() ? "no state line" : s->last.substr(s->last.find("scenario: ")).c_str());

		if (!quick)
		{
			PictureAfter(s, line);
		}

		return true;
	}

	// FORM COMBO BOXES (dialog_box.h): the state line's dialogcombos "<picked words>_<x> <y> <w> <h>|..." (spaces in the words are _) in the form's order. click-combo <n> opens the n-th list (from 1); click-combo-row <words> presses the row
	// with those words (any capitals, in the list that is open); expect-dialog-combo-rows <word>,<word>,... says which rows the open list holds.
	if ((verb == "click-combo" && w.size() >= 2) || (verb == "click-combo-row" && w.size() >= 2))
	{
		AskState(s);

		bool const row = verb == "click-combo-row";
		std::string const list = Field(s->last, row ? "dialogcomborows" : "dialogcombos");
		std::string want = line.substr(line.find(w[1]));

		for (char &c : want)
		{
			c = (char)tolower((unsigned char)c);
		}

		int at = 0;
		size_t from = 0;
		bool clicked = false;

		while (!clicked && !list.empty() && from <= list.size())
		{
			size_t const to = list.find('|', from);
			std::string const one = list.substr(from, to == std::string::npos ? std::string::npos : to - from);
			std::vector<std::string> const parts = Words(one);
			std::string words = parts.empty() ? std::string() : parts[0];

			for (char &c : words)
			{
				c = c == '_' ? ' ' : (char)tolower((unsigned char)c);
			}

			if (parts.size() >= 5 && (row ? words == want : ++at == atoi(w[1].c_str())))
			{
				// a row is pressed in its middle; a combo on its ARROW (8 pixels in from its right side), as a person opens one: an editable combo's box takes letters left of its
				// arrow (dialog_box.h), and a plain combo opens anywhere in its box (Vasquez 11-16)
				int const x = row ? atoi(parts[1].c_str()) + atoi(parts[3].c_str()) / 2 : atoi(parts[1].c_str()) + atoi(parts[3].c_str()) - 8;

				ClickWindow(s, x, atoi(parts[2].c_str()) + atoi(parts[4].c_str()) / 2, false, false);
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
			Check(false, label + " (nothing to press in \"" + list + "\")");
			return true;
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
		printf("  (%s)\n", s->last.empty() ? "no state line" : s->last.substr(s->last.find("scenario: ")).c_str());
		PictureAfter(s, line);
		return true;
	}

	if ((verb == "click-view" || verb == "click-three") && w.size() >= 3)
	{
		bool const right = std::find(w.begin(), w.end(), "right") != w.end();
		bool const shift = std::find(w.begin(), w.end(), "shift") != w.end();
		// "delete": the Delete key held over the click (Module Linking's un-pick, inteng's slot 0x55 = VK 0x2e): its key-down is posted before the press, its key-up after the release.
		// "ctrl": the Ctrl key held over the click the same way (Strategy Data's collector: Ctrl + click adds the clicked object's whole shape group, FUN_00526bf0 0x526e3b; the press carries MK_CONTROL too)
		bool const held = std::find(w.begin(), w.end(), "delete") != w.end();
		bool const ctrl = std::find(w.begin(), w.end(), "ctrl") != w.end();

		AskState(s);   // the box's place now

		int const *const box = verb == "click-view" ? s->view : s->three;

		if (box[2] < 8 || box[3] < 8)
		{
			Check(false, label + " (the editor has no " + (verb == "click-view" ? "view" : "three views' box") + " shown)");
			return false;
		}

		int const x = box[0] + (int)(atof(w[1].c_str()) * box[2]);
		int const y = box[1] + (int)(atof(w[2].c_str()) * box[3]);
		UINT const heldKey = held ? VK_DELETE : VK_CONTROL;
		UINT const heldScan = MapVirtualKeyW(heldKey, MAPVK_VK_TO_VSC);

		if (held || ctrl)
		{
			PostMessageW(s->editor.window, WM_KEYDOWN, heldKey, 1 | (heldScan << 16));
			Sleep(60);
		}

		ClickWindow(s, x, y, right, shift, 600, ctrl);

		if (held || ctrl)
		{
			PostMessageW(s->editor.window, WM_KEYUP, heldKey, 1 | (heldScan << 16) | (1u << 30) | (1u << 31));
			Sleep(200);
		}

		AskState(s);
		Check(true, label + " at " + std::to_string(x) + "," + std::to_string(y));
		printf("  (%s)\n", s->last.empty() ? "no state line" : s->last.substr(s->last.find("scenario: ")).c_str());
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

		printf("  (%s)\n", said.substr(said.find("locate")).c_str());
		ClickWindow(s, x, y, right, shift);
		AskState(s);
		Check(true, label + " at " + std::to_string(x) + "," + std::to_string(y));
		printf("  (%s)\n", s->last.empty() ? "no state line" : s->last.substr(s->last.find("scenario: ")).c_str());
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

		printf("  (%s)\n", said.substr(said.find("locate")).c_str());
		DragWindow(s, x0, y0, x0 + atoi(w[4].c_str()), y0 + atoi(w[5].c_str()));
		AskState(s);
		Check(true, label + " from " + std::to_string(x0) + "," + std::to_string(y0));
		printf("  (%s)\n", s->last.empty() ? "no state line" : s->last.substr(s->last.find("scenario: ")).c_str());
		PictureAfter(s, line);
		return true;
	}

	if (verb == "drag-win" && w.size() >= 5)
	{
		DragWindow(s, atoi(w[1].c_str()), atoi(w[2].c_str()), atoi(w[3].c_str()), atoi(w[4].c_str()));
		AskState(s);
		Check(true, label);
		printf("  (%s)\n", s->last.empty() ? "no state line" : s->last.substr(s->last.find("scenario: ")).c_str());
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
		printf("  (%s)\n", s->last.empty() ? "no state line" : s->last.substr(s->last.find("scenario: ")).c_str());
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
		printf("  (%s)\n", s->last.empty() ? "no state line" : s->last.substr(s->last.find("scenario: ")).c_str());
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
		printf("  (%s)\n", s->last.substr(s->last.find("scenario: ")).c_str());
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

		Check(!said.empty(), label + " -> " + (said.empty() ? "no answer" : said.substr(said.find("camera"))));
		return true;
	}

	// camera-look <look> <roll>: the camera's look (4096ths, signed: plus is down, held to a quarter turn either way) and roll; "camera" sets the place and the turn only (kHarnessCamera parts 4 and 5)
	if (verb == "camera-look" && w.size() >= 3)
	{
		Drain(s);
		PostMessageW(s->editor.window, WM_APP + 10, (WPARAM)4, (LPARAM)strtol(w[1].c_str(), NULL, 0));
		PostMessageW(s->editor.window, WM_APP + 10, (WPARAM)5, (LPARAM)strtol(w[2].c_str(), NULL, 0));

		std::string const said = WaitForLine(&s->editor, "AVP_OpenInteng: camera: ", 10000);

		Check(!said.empty() && said.find(" look ") != std::string::npos, label + " -> " + (said.empty() ? "no answer" : said.substr(said.find("camera"))));
		return true;
	}

	if (verb == "pick-module" && w.size() >= 2)
	{
		// Adjacent Modules: picks the module with that object number, as a click on it would (kHarnessPickModule)
		Drain(s);
		PostMessageW(s->editor.window, WM_APP + 11, (WPARAM)strtol(w[1].c_str(), NULL, 0), 0);

		std::string const said = WaitForLine(&s->editor, "AVP_OpenInteng: pick-module: ", 10000);

		Check(said.find("picked") != std::string::npos, label + " -> " + (said.empty() ? "no answer" : said.substr(said.find("pick-module"))));
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

		Check(said.find("picked") != std::string::npos, label + " -> " + (said.empty() ? "no answer" : said.substr(said.find("pick-marker"))));
		PictureAfter(s, line);
		return true;
	}

	if (verb == "face-light" && w.size() >= 2)
	{
		// Light Sourcing: selects that light (counting from 1) and turns the camera on the spot to face it (kHarnessFaceLight)
		Drain(s);
		PostMessageW(s->editor.window, WM_APP + 8, (WPARAM)strtol(w[1].c_str(), NULL, 0), 0);

		std::string const said = WaitForLine(&s->editor, "AVP_OpenInteng: face-light: ", 10000);

		Check(said.find("selected") != std::string::npos, label + " -> " + (said.empty() ? "no answer" : said.substr(said.find("face-light"))));
		return true;
	}

	if (verb == "wait" && w.size() >= 2)
	{
		Sleep((DWORD)atoi(w[1].c_str()));
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
			AskState(s);

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

	bool const started = made && StartTheEditor(exe, s.recordFolder, &s.editor, startArguments);

	Check(started, "the editor starts, with -picture" + Narrow(startArguments) + ", and its window and view are found");

	if (started)
	{
		s.editor.keepLines = true;
		Check(AskState(&s, 60000), "the editor says its state");
		printf("  (%s)\n", s.last.empty() ? "no state line" : s.last.substr(s.last.find("scenario: ")).c_str());

		for (size_t i = 0; i < steps.size(); i++)
		{
			if (!DoStep(&s, steps[i], (int)i + 1))
			{
				printf("  the scenario stops here\n");
				break;
			}
		}
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

	CoUninitialize();
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
