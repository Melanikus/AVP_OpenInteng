// drive_editor.cpp: the simulated-input harness (AvP-docs\review\sim-input-harness-hicks.md). Written 2026-11-14 by Melanikus (Hicks)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: the viewer's checklist (AvP-docs\engine\inteng\owner-intent.md) asks whether the menus open where they're clicked, whether the
// draw modes change the picture, and whether the camera moves where it faces. Those are questions about the real window, so this
// drives the real editor. It runs with nobody at the screen: it posts keys and clicks to the editor's own windows as window messages,
// the way Windows hands them over, and never moves the real mouse, never types on the real keyboard and never brings a window forward.
// So whoever is at the computer keeps it. At the end it checks that the cursor and the foreground window are where they were.
//
// HOW: it starts the editor with -picture, in the editor's own folder, with the editor's output caught in a file, and finds its windows.
// Two answers the editor gives only in a -picture run (editor_main.cpp; the contract is Dietrich's, 2026-11-14) let it ask questions:
//   WM_APP + 1, wParam n (1 to 999): on its next drawn frame the editor saves picture-<n>.png (the view's back buffer), panels-<n>.png
//     (the panels' back buffer) and screen-<n>.png (the window as Windows shows it) beside the program, then writes exactly one line,
//     "AVP_OpenInteng: picture <n> saved; the screen <n> ok" (or "...NOT THE EDITOR'S WINDOW");
//   WM_APP + 2: one line, "AVP_OpenInteng: state: menu <0-3|none> item <n|none>, camera <x> <y> <z> <turn> <look>, view mode <word>,
//     view <w>x<h> at <x>,<y>, window <w>x<h>, panels <w>x<h>".
// The camera's forward and right come from its turn, as the game makes them: CreateEulerMatrix (maths.cpp:355-397) with only a turn
// round the up axis gives the rows (cos, 0, sin), (0, 1, 0) and (-sin, 0, cos), and the camera's forward is the third, its right the
// first. Pictures are read with Windows' own image decoder (WIC). No script is read from anywhere: every step is written below.
//
// Usage: drive_editor.exe <the editor's .exe> <a folder for the pictures and the record> <camera|menus|modes|all>
// It prints "ok" or "FAILED" lines, as the unit tests do, and gives back 0 only if every check passed. A step the editor doesn't answer
// within its time fails, and the editor is closed.

#include <windows.h>
#include <wincodec.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

static int checksFailed = 0;

// ALLOW-FAIL (drive_scenario's "allow-fail <reason> | <check line>", Bishop / Dietrich 2026-11-18): while checkAllowedReason holds a reason, a check that fails is logged ALLOWED with the
// reason and not counted as failed (checksAllowed counts it); one that passes counts in checkAllowedPassed, so the step can say the prefix may go
static std::string checkAllowedReason;
static int checksAllowed = 0, checkAllowedPassed = 0, checkAllowedFailed = 0;

static void Check(bool good, std::string const &what)
{
	if (!good && !checkAllowedReason.empty())
	{
		printf("  ALLOWED  %s (%s)\n", what.c_str(), checkAllowedReason.c_str());
		fflush(stdout);
		checksAllowed++;
		checkAllowedFailed++;
		return;
	}

	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	fflush(stdout);
	if (!good)
	{
		checksFailed++;
	}
	else if (!checkAllowedReason.empty())
	{
		checkAllowedPassed++;
	}
}

// ---- The editor: started, found, asked, closed ------------------------------------------------------------------------------------

static UINT const kSavePicturesMessage = WM_APP + 1;
static UINT const kSayStateMessage = WM_APP + 2;

struct Editor
{
	PROCESS_INFORMATION process = {};
	std::wstring folder;           // the editor's own folder, where it saves its pictures
	std::wstring outputPath;       // its error and normal output, caught in a file
	size_t outputRead = 0;         // how much of that file the harness has read
	HWND window = NULL;            // the editor's own window: the panels, and where keys go
	HWND view = NULL;              // the level's view, a child window
	bool keepLines = false;        // the scenario harness: the lines that came after the one waited for, in the same read, are kept for the next wait (a picture's line and its panels' line can come together)
	std::vector<std::string> kept;
};

struct FoundWindow
{
	DWORD processId;
	HWND window;
};

static BOOL CALLBACK FindTheEditorsWindow(HWND window, LPARAM context)
{
	FoundWindow *const found = (FoundWindow *)context;
	DWORD processId = 0;
	GetWindowThreadProcessId(window, &processId);
	wchar_t className[64] = { 0 };
	GetClassNameW(window, className, 64);
	if (processId == found->processId && wcscmp(className, L"AVP_OpenInteng") == 0)
	{
		found->window = window;
		return FALSE;
	}
	return TRUE;
}

// extraArguments: words added to the command line after -picture (the scenario harness starts on a screen with " -screen Waypoint"); empty for the other runs
static bool StartTheEditor(std::wstring const &exe, std::wstring const &recordFolder, Editor *editor, std::wstring const &extraArguments = std::wstring())
{
	size_t const slash = exe.find_last_of(L"\\/");
	if (slash == std::wstring::npos)
	{
		return false;
	}
	editor->folder = exe.substr(0, slash + 1);
	editor->outputPath = recordFolder + L"\\editor-output.txt";

	SECURITY_ATTRIBUTES inherit = { sizeof(inherit), NULL, TRUE };
	HANDLE const output = CreateFileW(editor->outputPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &inherit, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (output == INVALID_HANDLE_VALUE)
	{
		return false;
	}
	STARTUPINFOW start = {};
	start.cb = sizeof(start);
	start.dwFlags = STARTF_USESTDHANDLES;
	start.hStdInput = NULL;
	start.hStdOutput = output;
	start.hStdError = output;
	std::wstring commandLine = L"\"" + exe + L"\" -picture 100000" + extraArguments;
	bool const started = CreateProcessW(exe.c_str(), &commandLine[0], NULL, NULL, TRUE, 0, NULL, editor->folder.c_str(), &start, &editor->process) != 0;
	CloseHandle(output);
	if (!started)
	{
		return false;
	}

	// Its window, within 60 seconds: the level loads first
	FoundWindow found = { editor->process.dwProcessId, NULL };
	for (int tries = 0; tries < 600 && !found.window; tries++)
	{
		if (WaitForSingleObject(editor->process.hProcess, 100) == WAIT_OBJECT_0)
		{
			return false;
		}
		EnumWindows(FindTheEditorsWindow, (LPARAM)&found);
	}
	editor->window = found.window;
	editor->view = found.window ? FindWindowExW(found.window, NULL, L"AVP_OpenInteng_View", NULL) : NULL;
	return editor->window && editor->view;
}

// The editor's output lines written since the harness last read
static std::vector<std::string> NewLines(Editor *editor)
{
	std::vector<std::string> lines;
	HANDLE const file = CreateFileW(editor->outputPath.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, NULL,
		OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (file == INVALID_HANDLE_VALUE)
	{
		return lines;
	}
	LARGE_INTEGER size = {};
	GetFileSizeEx(file, &size);
	if ((size_t)size.QuadPart > editor->outputRead && size.QuadPart < (1 << 26))
	{
		std::string text((size_t)size.QuadPart - editor->outputRead, '\0');
		LARGE_INTEGER at = {};
		at.QuadPart = (LONGLONG)editor->outputRead;
		DWORD got = 0;
		SetFilePointerEx(file, at, NULL, FILE_BEGIN);
		ReadFile(file, &text[0], (DWORD)text.size(), &got, NULL);
		text.resize(got);
		// Only whole lines are taken; a part line waits for its end
		size_t const lastEnd = text.find_last_of('\n');
		if (lastEnd != std::string::npos)
		{
			editor->outputRead += lastEnd + 1;
			size_t from = 0;
			for (size_t end = text.find('\n'); end != std::string::npos && end <= lastEnd; end = text.find('\n', from))
			{
				std::string line = text.substr(from, end - from);
				if (!line.empty() && line.back() == '\r')
				{
					line.pop_back();
				}
				lines.push_back(line);
				from = end + 1;
			}
		}
	}
	CloseHandle(file);
	return lines;
}

// Waits for a line starting with these words, for up to timeoutMs; gives back the line, or "" if none came
static std::string WaitForLine(Editor *editor, std::string const &start, DWORD timeoutMs)
{
	DWORD const began = GetTickCount();
	while (GetTickCount() - began < timeoutMs)
	{
		std::vector<std::string> lines;
		if (editor->keepLines)
		{
			lines.swap(editor->kept);
		}
		for (std::string const &line : NewLines(editor))
		{
			lines.push_back(line);
		}
		for (size_t at = 0; at < lines.size(); at++)
		{
			if (lines[at].compare(0, start.size(), start) == 0)
			{
				if (editor->keepLines)
				{
					editor->kept.assign(lines.begin() + (long)at + 1, lines.end());
				}
				return lines[at];
			}
		}
		if (editor->keepLines)
		{
			editor->kept.clear();   // lines that came before the one waited for and weren't it are let go, as without keeping
		}
		if (WaitForSingleObject(editor->process.hProcess, 50) == WAIT_OBJECT_0)
		{
			return std::string();
		}
	}
	return std::string();
}

struct State
{
	bool read = false;
	int menu = -1;                // the open menu's number, or -1 for none
	int item = -1;                // its lit item's number, or -1 for none
	double camera[5] = {};        // x y z turn look
	double forward[3] = {};       // from the turn, as the game makes it (above)
	double right[3] = {};
	char mode[32] = { 0 };
	int viewWidth = 0, viewHeight = 0, viewX = 0, viewY = 0, windowWidth = 0, windowHeight = 0, panelsWidth = 0, panelsHeight = 0;
};

// One state line read into its parts; read is false unless every part was there
static State ParseState(std::string const &line)
{
	State state;
	char menu[16] = { 0 };
	char item[16] = { 0 };
	int const got = sscanf_s(line.c_str(),
		"AVP_OpenInteng: state: menu %15s item %15[^,], camera %lf %lf %lf %lf %lf, view mode %31[^,], view %dx%d at %d,%d, window %dx%d, panels %dx%d",
		menu, (unsigned)sizeof(menu), item, (unsigned)sizeof(item), &state.camera[0], &state.camera[1], &state.camera[2], &state.camera[3],
		&state.camera[4], state.mode, (unsigned)sizeof(state.mode), &state.viewWidth, &state.viewHeight, &state.viewX, &state.viewY,
		&state.windowWidth, &state.windowHeight, &state.panelsWidth, &state.panelsHeight);
	state.read = got == 16;   // menu, item, the camera's 5, the mode, and 8 sizes
	state.menu = strcmp(menu, "none") == 0 ? -1 : atoi(menu);
	state.item = strcmp(item, "none") == 0 ? -1 : atoi(item);
	double const angle = state.camera[3] * 2.0 * 3.14159265358979 / 4096.0;
	state.forward[0] = -sin(angle);
	state.forward[2] = cos(angle);
	state.right[0] = cos(angle);
	state.right[2] = sin(angle);
	return state;
}

static State AskTheState(Editor *editor)
{
	PostMessageW(editor->window, kSayStateMessage, 0, 0);
	std::string const line = WaitForLine(editor, "AVP_OpenInteng: state: ", 10000);
	return line.empty() ? State() : ParseState(line);
}

// Asks for pictures n, waits for them, checks the editor found its own window on the screen, and moves the three pictures into the
// record's folder. Gives back false if they didn't all come.
static bool TakePictures(Editor *editor, std::wstring const &recordFolder, int n)
{
	PostMessageW(editor->window, kSavePicturesMessage, (WPARAM)n, 0);
	std::string const said = WaitForLine(editor, "AVP_OpenInteng: picture " + std::to_string(n) + " saved", 20000);
	if (said.empty())
	{
		return false;
	}
	// In a hidden run the editor's window is never shown, so the screen picture can't be it: that's said, not failed. In a run with the
	// window shown (only ever with the owner's word, while he's away), it has to be.
	bool const screenIsTheEditors = said.find("the screen " + std::to_string(n) + " ok") != std::string::npos;
	printf("  (picture %d: the screen picture %s)\n", n, screenIsTheEditors ? "is the editor's window" : "isn't the editor's window, as in a hidden run");

	// The panels' picture is saved inside the game's draw to their swap chain, before it's shown, and says so on a line of its own
	// (Dietrich's contract): "AVP_OpenInteng: panels <n> saved", or "... NOT SAVED"
	std::string const panelsSaid = WaitForLine(editor, "AVP_OpenInteng: panels " + std::to_string(n) + " ", 20000);
	if (panelsSaid.empty() || panelsSaid.find("NOT SAVED") != std::string::npos)
	{
		printf("  (picture %d: the panels' picture: %s)\n", n, panelsSaid.empty() ? "no line came" : panelsSaid.c_str());
		return false;
	}
	bool moved = true;
	for (const wchar_t *kind : { L"picture", L"panels" })
	{
		std::wstring const name = std::wstring(kind) + L"-" + std::to_wstring(n) + L".png";
		moved = MoveFileExW((editor->folder + name).c_str(), (recordFolder + L"\\" + name).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED) && moved;
	}
	std::wstring const screenName = L"screen-" + std::to_wstring(n) + L".png";
	MoveFileExW((editor->folder + screenName).c_str(), (recordFolder + L"\\" + screenName).c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
	return moved;
}

// A key held for holdMs, as Windows posts one: a press with the key's scan code, and a release with the "was down" and "going up" bits.
// F10 is a system key: Windows posts it as WM_SYSKEYDOWN and WM_SYSKEYUP, and the editor's menu bar takes it only that way
// (window_events.cpp:55), so it's posted that way here too.
static void HoldAKey(Editor *editor, UINT key, DWORD holdMs)
{
	UINT const scan = MapVirtualKeyW(key, MAPVK_VK_TO_VSC);
	bool const systemKey = key == VK_F10;
	PostMessageW(editor->window, systemKey ? WM_SYSKEYDOWN : WM_KEYDOWN, key, 1 | (scan << 16));
	Sleep(holdMs);
	PostMessageW(editor->window, systemKey ? WM_SYSKEYUP : WM_KEYUP, key, 1 | (scan << 16) | (1u << 30) | (1u << 31));
	Sleep(150);
}

// A left click at a place in a window's own client pixels
static void ClickAt(HWND window, int x, int y)
{
	LPARAM const at = MAKELPARAM(x, y);
	PostMessageW(window, WM_MOUSEMOVE, 0, at);
	PostMessageW(window, WM_LBUTTONDOWN, MK_LBUTTON, at);
	PostMessageW(window, WM_LBUTTONUP, 0, at);
	Sleep(200);
}

static void CloseTheEditor(Editor *editor)
{
	if (!editor->process.hProcess)
	{
		return;
	}
	if (editor->window)
	{
		PostMessageW(editor->window, WM_CLOSE, 0, 0);
	}
	if (WaitForSingleObject(editor->process.hProcess, 30000) != WAIT_OBJECT_0)
	{
		TerminateProcess(editor->process.hProcess, 99);
		Check(false, "the editor closed when asked (it didn't, within 30 seconds, and was ended)");
	}
	CloseHandle(editor->process.hProcess);
	CloseHandle(editor->process.hThread);
	editor->process = PROCESS_INFORMATION();
}

// ---- Pictures, read with Windows' own decoder ---------------------------------------------------------------------------------------

struct Picture
{
	UINT width = 0, height = 0;
	std::vector<BYTE> bgra;       // 4 bytes a pixel, top row first
	bool Read() const { return width > 0 && height > 0; }
	int Brightness(UINT x, UINT y) const { BYTE const *p = &bgra[(y * width + x) * 4]; return p[0] + p[1] + p[2]; }
};

static Picture ReadPicture(std::wstring const &path)
{
	Picture picture;
	IWICImagingFactory *factory = NULL;
	IWICBitmapDecoder *decoder = NULL;
	IWICBitmapFrameDecode *frame = NULL;
	IWICFormatConverter *converter = NULL;
	if (SUCCEEDED(CoCreateInstance(CLSID_WICImagingFactory, NULL, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory))) &&
		SUCCEEDED(factory->CreateDecoderFromFilename(path.c_str(), NULL, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder)) &&
		SUCCEEDED(decoder->GetFrame(0, &frame)) && SUCCEEDED(factory->CreateFormatConverter(&converter)) &&
		SUCCEEDED(converter->Initialize(frame, GUID_WICPixelFormat32bppBGRA, WICBitmapDitherTypeNone, NULL, 0.0, WICBitmapPaletteTypeCustom)))
	{
		UINT width = 0, height = 0;
		converter->GetSize(&width, &height);
		if (width > 0 && height > 0 && width <= 16384 && height <= 16384)
		{
			picture.bgra.resize((size_t)width * height * 4);
			if (SUCCEEDED(converter->CopyPixels(NULL, width * 4, (UINT)picture.bgra.size(), picture.bgra.data())))
			{
				picture.width = width;
				picture.height = height;
			}
		}
	}
	if (converter) converter->Release();
	if (frame) frame->Release();
	if (decoder) decoder->Release();
	if (factory) factory->Release();
	return picture;
}

// The share of pixels that differ between two pictures of the same size by more than a little, 0 to 1 (or -1 if they can't be compared)
static double ShareDifferent(Picture const &a, Picture const &b)
{
	if (!a.Read() || !b.Read() || a.width != b.width || a.height != b.height)
	{
		return -1.0;
	}
	size_t different = 0;
	for (UINT y = 0; y < a.height; y++)
	{
		for (UINT x = 0; x < a.width; x++)
		{
			different += abs(a.Brightness(x, y) - b.Brightness(x, y)) > 24 ? 1 : 0;
		}
	}
	return (double)different / ((double)a.width * a.height);
}

// Where the menu titles are DRAWN, found from the panels' picture: the top band of rows with lit text in it, and the runs of lit columns
// in that band, a gap of more than 9 pixels starting a new title. Never the editor's own idea of where they are: a difference between
// the two is the bug this looks for (Melanikus, 2026-11-14: a click on the first title's right side opened the second).
struct Span
{
	int left, right, top, bottom;
};

static std::vector<Span> FindTitles(Picture const &panels)
{
	std::vector<Span> titles;
	if (!panels.Read())
	{
		return titles;
	}
	int const background = panels.Brightness(0, 0);
	auto lit = [&](UINT x, UINT y) { return abs(panels.Brightness(x, y) - background) > 96; };
	UINT const searchRows = panels.height < 80 ? panels.height : 80;
	int top = -1, bottom = -1;
	for (UINT y = 0; y < searchRows; y++)
	{
		bool any = false;
		for (UINT x = 0; x < panels.width && !any; x++)
		{
			any = lit(x, y);
		}
		if (any && top < 0)
		{
			top = (int)y;
		}
		if (!any && top >= 0)
		{
			bottom = (int)y - 1;
			break;
		}
	}
	if (top < 0 || bottom < top)
	{
		return titles;
	}
	int runStart = -1, lastLit = -100;
	for (UINT x = 0; x <= panels.width; x++)
	{
		bool column = false;
		for (int y = top; x < panels.width && y <= bottom && !column; y++)
		{
			column = lit(x, (UINT)y);
		}
		if (column)
		{
			if (runStart >= 0 && (int)x - lastLit > 9)
			{
				titles.push_back({ runStart, lastLit, top, bottom });
				runStart = -1;
			}
			if (runStart < 0)
			{
				runStart = (int)x;
			}
			lastLit = (int)x;
		}
	}
	if (runStart >= 0)
	{
		titles.push_back({ runStart, lastLit, top, bottom });
	}
	return titles;
}

// ---- The checks --------------------------------------------------------------------------------------------------------------------

static double Angle(double const a[3], double const b[3])
{
	double const dot = a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
	double const la = sqrt(a[0] * a[0] + a[1] * a[1] + a[2] * a[2]);
	double const lb = sqrt(b[0] * b[0] + b[1] * b[1] + b[2] * b[2]);
	return la > 0 && lb > 0 ? acos(fmax(-1.0, fmin(1.0, dot / (la * lb)))) * 180.0 / 3.14159265358979 : 180.0;
}

// The camera moves where it faces: turned a part of a turn, W moves it along its forward, and D along its right, within 5 degrees
static void CheckTheCamera(Editor *editor)
{
	printf("The camera:\n");
	State const before = AskTheState(editor);
	Check(before.read, "the editor says its state");
	HoldAKey(editor, VK_RIGHT, 600);
	State const turned = AskTheState(editor);
	Check(turned.read && turned.camera[3] != before.camera[3], "the right arrow turns the camera");

	HoldAKey(editor, 'W', 600);
	State const forward = AskTheState(editor);
	double const moved[3] = { forward.camera[0] - turned.camera[0], forward.camera[1] - turned.camera[1], forward.camera[2] - turned.camera[2] };
	double const angle = Angle(moved, turned.forward);
	Check(forward.read && angle < 5.0, "W moves it along the way it faces, once turned (" + std::to_string(angle) + " degrees off)");

	HoldAKey(editor, 'D', 600);
	State const sideways = AskTheState(editor);
	double const movedRight[3] = { sideways.camera[0] - forward.camera[0], sideways.camera[1] - forward.camera[1], sideways.camera[2] - forward.camera[2] };
	double const rightAngle = Angle(movedRight, forward.right);
	Check(sideways.read && rightAngle < 5.0, "D moves it to its own right (" + std::to_string(rightAngle) + " degrees off)");

	Check(sideways.read && (double)sideways.viewWidth * sideways.viewHeight >= 0.75 * (double)sideways.windowWidth * sideways.windowHeight,
		"the view is at least 75% of the window (owner-intent.md): " + std::to_string(sideways.viewWidth) + "x" + std::to_string(sideways.viewHeight) +
		" in " + std::to_string(sideways.windowWidth) + "x" + std::to_string(sideways.windowHeight));
}

// The menus open where they're clicked: at the left, the middle and the right of each title, as shown, at the window's size now
static void CheckTheMenusAtThisSize(Editor *editor, std::wstring const &recordFolder, int pictureNumber)
{
	bool const pictured = TakePictures(editor, recordFolder, pictureNumber);
	Check(pictured, "the editor saves its pictures when asked");

	// The titles are found where they're DRAWN, in the panels' picture: in a hidden run the window is never on the screen, so the
	// panels' own back buffer is what there is. It's drawn at the panels' size (the state line's "panels"), and a click lands in the
	// window's client pixels, so each place is carried from the one to the other by their sizes: the same carrying the window's own
	// mapping must do (host_frame.h, HostFrame_WindowToPanels, proven at 1.0, 1.25 and 1.5 by host_click_test).
	Picture const panels = ReadPicture(recordFolder + L"\\panels-" + std::to_wstring(pictureNumber) + L".png");
	State const sizes = AskTheState(editor);
	Check(panels.Read() && sizes.read && (int)panels.width == sizes.panelsWidth && (int)panels.height == sizes.panelsHeight,
		"the panels' picture is the panels' own size: " + std::to_string(panels.width) + "x" + std::to_string(panels.height) + " for panels of " +
		std::to_string(sizes.panelsWidth) + "x" + std::to_string(sizes.panelsHeight) + " in a window of " + std::to_string(sizes.windowWidth) + "x" +
		std::to_string(sizes.windowHeight));
	std::vector<Span> const titles = FindTitles(panels);
	Check(titles.size() >= 2, "the menu titles are found where they're drawn: " + std::to_string(titles.size()));
	double const across = sizes.panelsWidth > 0 ? (double)sizes.windowWidth / sizes.panelsWidth : 1.0;
	double const down = sizes.panelsHeight > 0 ? (double)sizes.windowHeight / sizes.panelsHeight : 1.0;
	for (size_t i = 0; i < titles.size(); i++)
	{
		Span const &drawn = titles[i];
		Span const title = { (int)(drawn.left * across), (int)(drawn.right * across), (int)(drawn.top * down), (int)(drawn.bottom * down) };
		int const y = (title.top + title.bottom) / 2;
		int const places[3] = { title.left + 1, (title.left + title.right) / 2, title.right - 1 };
		const char *const names[3] = { "left", "middle", "right" };
		for (int p = 0; p < 3; p++)
		{
			ClickAt(editor->window, places[p], y);
			State const state = AskTheState(editor);
			Check(state.read && state.menu == (int)i, "a click on the " + std::string(names[p]) + " of title " + std::to_string(i) + " (x " +
				std::to_string(places[p]) + ", y " + std::to_string(y) + ") opens menu " + std::to_string(i) + ": the editor says " +
				(state.menu < 0 ? std::string("none") : std::to_string(state.menu)));
			HoldAKey(editor, VK_ESCAPE, 50);
		}
	}
}

// At two window sizes (owner-intent.md: "x 2 sizes"): as the editor opened, then the size of the screen's work area. The window is
// sized with SetWindowPos and SWP_NOACTIVATE, so it never becomes the active one: nothing is taken from whoever is at the computer.
static void CheckTheMenus(Editor *editor, std::wstring const &recordFolder)
{
	printf("The menus, at the window's own size:\n");
	CheckTheMenusAtThisSize(editor, recordFolder, 101);

	RECT work = {};
	SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
	bool const sized = SetWindowPos(editor->window, NULL, work.left, work.top, work.right - work.left, work.bottom - work.top,
		SWP_NOACTIVATE | SWP_NOZORDER | SWP_NOOWNERZORDER) != 0;
	Sleep(1000);   // a few frames, for the panels and the view to be laid out at the new size
	printf("The menus, at the work area's size (%ldx%ld):\n", work.right - work.left, work.bottom - work.top);
	Check(sized, "the window is sized to the work area without being made active");
	CheckTheMenusAtThisSize(editor, recordFolder, 102);
}

// The View menu's item n, by the keyboard (Dietrich's menu bar, menu_bar.cpp:196-201): F10 opens File with its first item that's on
// lit, Right opens View the same way, and each Down moves to the next item that's on. The state line then says which item is lit, and
// that's checked before Enter, so a menu that moved never makes this choose the wrong item. Gives back the item lit, or -1.
static int ChooseViewItem(Editor *editor, int downs)
{
	HoldAKey(editor, VK_F10, 50);
	HoldAKey(editor, VK_RIGHT, 50);
	for (int down = 0; down < downs; down++)
	{
		HoldAKey(editor, VK_DOWN, 50);
	}
	State const lit = AskTheState(editor);
	if (!lit.read || lit.menu != 1)
	{
		HoldAKey(editor, VK_ESCAPE, 50);
		return -1;
	}
	HoldAKey(editor, VK_RETURN, 50);
	return lit.item;
}

// The draw modes change the picture: the View menu's first items that are on, each chosen and then chosen again to put it back,
// against the picture before any. The View menu is the second title, as the table of actions makes it (editor_actions.cpp).
static void CheckTheModes(Editor *editor, std::wstring const &recordFolder)
{
	printf("The draw modes:\n");
	Check(TakePictures(editor, recordFolder, 201), "a picture before any mode");
	Picture const normal = ReadPicture(recordFolder + L"\\picture-201.png");
	for (int downs = 0; downs < 2; downs++)
	{
		int const item = ChooseViewItem(editor, downs);
		Check(item >= 0, "the View menu opens by the keyboard, with an item lit (" + std::to_string(downs) + " downs: item " + std::to_string(item) + ")");
		int const n = 202 + downs;
		bool const pictured = TakePictures(editor, recordFolder, n);
		double const share = ShareDifferent(normal, ReadPicture(recordFolder + L"\\picture-" + std::to_wstring(n) + L".png"));
		Check(pictured && share > 0.01, "View item " + std::to_string(item) + " changes the picture: " + std::to_string(share * 100.0) + "% of it differs");

		// The same item again, to put it back
		Check(ChooseViewItem(editor, downs) == item, "and choosing it again puts it back");
	}
}

// The harness's own test (tests/drive_editor_parts_test.cpp) takes in this file without its start, to check its parts on made-up input
#ifndef DRIVE_EDITOR_PARTS_ONLY
int wmain(int argc, wchar_t **argv)
{
	if (argc != 4)
	{
		fprintf(stderr, "usage: drive_editor.exe <the editor's .exe> <a folder for the pictures and the record> <camera|menus|modes|all>\n");
		return 2;
	}
	std::wstring const exe = argv[1];
	std::wstring const recordFolder = argv[2];
	std::wstring const what = argv[3];
	if (what != L"camera" && what != L"menus" && what != L"modes" && what != L"all")
	{
		fprintf(stderr, "drive_editor: the last word is camera, menus, modes or all\n");
		return 2;
	}
	CreateDirectoryW(recordFolder.c_str(), NULL);
	if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED)))
	{
		fprintf(stderr, "drive_editor: COM wouldn't start, so no picture could be read\n");
		return 2;
	}

	// Whoever is at the computer keeps it: where the cursor and the foreground window are now, to compare at the end
	POINT cursorBefore = {};
	GetCursorPos(&cursorBefore);
	HWND const foregroundBefore = GetForegroundWindow();

	Editor editor;
	bool const started = StartTheEditor(exe, recordFolder, &editor);
	Check(started, "the editor starts, with -picture, and its window and view are found");
	if (started)
	{
		// The pictures first, from where the level starts the camera; the camera's own check last, as it moves the camera away, perhaps
		// out of every room (the first run, 2026-11-14 21:01: every picture after the camera check was black, the camera seven metres off)
		if (what == L"modes" || what == L"all")
		{
			CheckTheModes(&editor, recordFolder);
		}
		if (what == L"menus" || what == L"all")
		{
			CheckTheMenus(&editor, recordFolder);
		}
		if (what == L"camera" || what == L"all")
		{
			CheckTheCamera(&editor);
		}
	}
	CloseTheEditor(&editor);

	POINT cursorAfter = {};
	GetCursorPos(&cursorAfter);
	Check(cursorAfter.x == cursorBefore.x && cursorAfter.y == cursorBefore.y && GetForegroundWindow() == foregroundBefore,
		"the real cursor never moved and the foreground window never changed: nobody at the computer lost it");

	CoUninitialize();
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
#endif
