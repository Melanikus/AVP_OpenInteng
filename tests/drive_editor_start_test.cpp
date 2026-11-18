// drive_editor_start_test: the harness's start of the editor (tests/drive_editor/drive_editor.cpp, StartTheEditor), against a stand-in editor
// whose view comes after its window. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// WHY: "the editor didn't start under load" (the rounds' FLAKY files, 11-18): the editor makes its window, then the view inside it
// (editor_main.cpp, one CreateWindowEx after the other), and a round's load can put time between the two. The harness looked for the view once,
// right after it found the window; a look in that gap said the start failed, and the harness's close (WM_CLOSE) then ended an editor that was
// starting well, after its level had loaded: "0 frames", "the window 0 by 0". The start must wait for the window AND its view.
//
// HOW: this program is also the stand-in. StartTheEditor starts it as it starts the editor ("<exe> -picture 100000 <words>"); with -picture it
// makes a window of the editor's class, waits (its thread busy, as the editor's is, so nothing it is sent is read meanwhile), makes the view in
// it, and then answers its messages until its window is closed. -standin-exit <n> ends at once with code n, before any window;
// -standin-no-view makes no view at all.

#define DRIVE_EDITOR_PARTS_ONLY
#include "drive_editor/drive_editor.cpp"

static int checksRun = 0;

static void Expect(bool good, std::string const &what)
{
	checksRun++;
	Check(good, what);
}

static LRESULT CALLBACK StandInProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_CLOSE)
	{
		DestroyWindow(window);
		return 0;
	}

	if (message == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}

	return DefWindowProcW(window, message, wParam, lParam);
}

static int StandIn(int argc, wchar_t **argv)
{
	DWORD viewAfter = 0;
	bool noView = false;

	for (int i = 1; i < argc; i++)
	{
		if (wcscmp(argv[i], L"-standin-exit") == 0 && i + 1 < argc)
		{
			return _wtoi(argv[i + 1]);
		}

		if (wcscmp(argv[i], L"-standin-view-after") == 0 && i + 1 < argc)
		{
			viewAfter = wcstoul(argv[i + 1], nullptr, 10);
		}

		noView = noView || wcscmp(argv[i], L"-standin-no-view") == 0;
	}

	HINSTANCE const instance = GetModuleHandleW(NULL);
	WNDCLASSW windowClass = {};
	windowClass.lpfnWndProc = StandInProc;
	windowClass.hInstance = instance;
	windowClass.lpszClassName = L"AVP_OpenInteng";
	WNDCLASSW viewClass = windowClass;
	viewClass.lpfnWndProc = DefWindowProcW;
	viewClass.lpszClassName = L"AVP_OpenInteng_View";

	if (!RegisterClassW(&windowClass) || !RegisterClassW(&viewClass))
	{
		return 3;
	}

	// never shown, never active: nobody at the computer sees it
	HWND const window = CreateWindowExW(WS_EX_NOACTIVATE, L"AVP_OpenInteng", L"stand-in", WS_OVERLAPPEDWINDOW, 0, 0, 200, 100, NULL, NULL, instance, NULL);

	if (!window)
	{
		return 4;
	}

	Sleep(viewAfter);

	if (!noView && !CreateWindowExW(0, L"AVP_OpenInteng_View", L"", WS_CHILD | WS_VISIBLE, 0, 0, 100, 50, window, NULL, instance, NULL))
	{
		return 5;
	}

	MSG message;

	while (GetMessageW(&message, NULL, 0, 0) > 0)
	{
		DispatchMessageW(&message);
	}

	return 0;
}

static std::wstring ThisProgram()
{
	wchar_t path[MAX_PATH] = L"";
	DWORD const length = GetModuleFileNameW(NULL, path, MAX_PATH);
	return length > 0 && length < MAX_PATH ? std::wstring(path) : std::wstring();
}

static void TestAViewAfterItsWindow(std::wstring const &exe, std::wstring const &folder)
{
	Editor editor;
	bool const started = StartTheEditor(exe, folder, &editor, L" -standin-view-after 1500");

	Expect(started, "a stand-in whose view comes 1.5 s after its window has started (the start waits for the view, not only the window)");
	Expect(editor.window && editor.view && IsChild(editor.window, editor.view), "its window and the view in it are found");
	Expect(editor.process.hProcess && WaitForSingleObject(editor.process.hProcess, 0) == WAIT_TIMEOUT, "and it is still running: the start closed nothing");
	CloseTheEditor(&editor);
	Expect(!editor.process.hProcess, "it closes when asked");
}

// A start that fails says why, so a red's log names the cause
static void TestTheReasons(std::wstring const &exe, std::wstring const &folder)
{
	Editor ended;
	bool const endedStarted = StartTheEditor(exe, folder, &ended, L" -standin-exit 7");   // first: the words below read what it said
	Expect(!endedStarted && ended.startProblem.find("ended by itself, with exit code 7") != std::string::npos,
		"one that ends by itself before any window: the start fails and says so, with its exit code (" + ended.startProblem + ")");
	CloseTheEditor(&ended);

	Editor noView;
	DWORD const began = GetTickCount();
	bool const started = StartTheEditor(exe, folder, &noView, L" -standin-no-view", 2000);
	DWORD const took = GetTickCount() - began;
	Expect(!started && noView.window && !noView.view && noView.startProblem.find("its window, but no view in it") != std::string::npos,
		"one with a window and never a view: the start fails at its time limit and says so (" + noView.startProblem + ")");
	Expect(took >= 1900, "and it waited its whole time for the view (" + std::to_string(took) + " ms of 2000), not one look");
	CloseTheEditor(&noView);
	Expect(!noView.process.hProcess, "and it closes when asked");
}

int wmain(int argc, wchar_t **argv)
{
	for (int i = 1; i < argc; i++)
	{
		if (wcscmp(argv[i], L"-picture") == 0)
		{
			return StandIn(argc, argv);
		}
	}

	std::wstring const exe = ThisProgram();
	size_t const slash = exe.find_last_of(L"\\/");

	Expect(!exe.empty() && slash != std::wstring::npos, "this program knows its own path, to start itself as the stand-in");

	if (!exe.empty() && slash != std::wstring::npos)
	{
		std::wstring const folder = exe.substr(0, slash);

		TestAViewAfterItsWindow(exe, folder);
		TestTheReasons(exe, folder);
	}

	printf("%d checks, %d failed\n", checksRun, checksFailed);
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
