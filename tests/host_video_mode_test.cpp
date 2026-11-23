// The test for src/host/video_mode.cpp, the owner's Video mode's engine, pressed as its screen presses it. Written 2026-11-22 by Melanikus (Dietrich) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: two of the mode's refusals can't be reached from its screen (Frost's acceptance, 11-22: with no video the screen offers no control at all, and Cancel only
// while a video is read or made), so only a test that presses the engine itself proves them: "Drop a video first (or use Load -> Browse)" and "Nothing is being
// read or made". And the Save's window is the user's last look before a film is written over: its number box is one row tall (a combo's box height is its row's,
// dialog_box.cpp), and a number a stock level plays says so in the question itself (Vasquez's review, 11-22: 103 is stock Derelict's briefing), as the question
// about one of Rebellion's films no level plays already did.
//
// The windows the engine opens are stand-ins here (Dialogs_Form, Dialogs_Confirm, FmvPreview_Start/Stop, below): each keeps what it was asked, and the test
// answers as a user would. The video is made by the test (tests/video_test_clip.h: the repository holds no video): 2 s, 320 x 180, with a tone, in the test's own
// folder, which is also the "game folder" whose FMVs\ the Save looks in. No film is made or written (every question is answered No).
//
// Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <functional>
#include <string>
#include <vector>

#include <windows.h>

#include <mfapi.h>

#include "SmackerEncoder.h"
#include "dialogs.h"
#include "fmv_preview.h"
#include "status_words.h"
#include "video_mode.h"
#include "video_test_clip.h"

// the editor's own three names video_mode.cpp reads (editor_shared.h; editor_main.cpp holds them in the editor)
StatusWords statusWords;
char theGameFolder[MAX_PATH] = "";
char layoutSettingsFile[MAX_PATH] = "";

namespace
{
	int checks = 0;
	int checksFailed = 0;

	// what the stand-in windows were asked, and how to answer them
	host::DialogFormSpec askedForm;
	std::function<void(int32_t, std::vector<host::DialogWidget> const &)> formAnswer;
	int formsAsked = 0;
	std::string askedQuestion;
	std::function<void(bool)> questionAnswer;
	int questionsAsked = 0;
}

void host::Dialogs_Form(host::DialogFormSpec const &spec, std::function<void(int32_t, std::vector<host::DialogWidget> const &)> const &then)
{
	askedForm = spec;
	formAnswer = then;
	formsAsked++;
}

void host::Dialogs_Confirm(std::string const &text, std::function<void(bool)> const &then)
{
	askedQuestion = text;
	questionAnswer = then;
	questionsAsked++;
}

bool host::FmvPreview_Start(std::string const &, std::string const &, char const *, host::FmvPreviewPlace const &, std::string *words, host::FmvPreviewLayer)
{
	if (words)
	{
		*words = "the test plays no film";
	}

	return false;
}

void host::FmvPreview_Stop()
{
}

bool host::FmvPreview_On()
{
	return false;   // the test plays no film
}

bool host::FmvPreview_Ended()
{
	return false;
}

// THE ENCODER compiled into the editor (libsmackerenc, SMK_ENCODER_BUILT_IN), stood in: this test makes no film (every question is answered No), and counts the
// calls, so a film made where none should be is seen
static int encodes = 0;

extern "C" int Smacker_Encode(const SmackerEncodeInput *, uint8_t **smk, size_t *smkBytes)
{
	encodes++;

	if (smk)
	{
		*smk = nullptr;
	}

	if (smkBytes)
	{
		*smkBytes = 0;
	}

	return -1;
}

extern "C" void Smacker_FreeEncoded(uint8_t *)
{
}

extern "C" const char *Smacker_EncodeWords(int)
{
	return "the test makes no film";
}

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checks++;

	if (!good)
	{
		checksFailed++;
	}
}

// the status line's words now
static std::string Said()
{
	char const *const words = statusWords;

	return words ? words : "";
}

// a press that must be refused with `wanted`: pressed, its words read, then checked, in that order (one call can't do all three in its own words: C++ leaves the order
// of a call's arguments open, so its message could show the words of the press before)
static void CheckRefused(char const *control, std::string const &wanted, std::string const &what)
{
	bool const took = host::VideoMode_Press(control);
	std::string const said = Said();

	Check(!took && said == wanted, what + ": refused, \"" + wanted + "\" (said: \"" + said + "\")");
}

// the engine's frames run until its view reaches `phase`, at most `ms`
static bool WaitFor(host::VideoModePhase phase, int ms)
{
	for (int waited = 0; waited <= ms; waited += 20)
	{
		host::VideoMode_Frame();

		if (host::VideoMode_View().phase == phase)
		{
			return true;
		}

		Sleep(20);
	}

	return false;
}

// the Save window answered Ok with `typed` in its number box (typing makes no row picked: dialog_box.h)
static bool AnswerNumber(std::string const &typed)
{
	if (askedForm.widgets.size() < 2 || !formAnswer)
	{
		return false;
	}

	std::vector<host::DialogWidget> widgets = askedForm.widgets;

	widgets[1].selected = -1;
	widgets[1].text = typed;
	formAnswer(host::kDialogOk, widgets);
	return true;
}

// a wide name in the system's letters, for the checks' words and for ReadAll
static std::string Narrow(std::wstring const &wide)
{
	char narrow[MAX_PATH * 2] = {};

	WideCharToMultiByte(CP_ACP, 0, wide.c_str(), -1, narrow, (int)sizeof(narrow), nullptr, nullptr);
	return narrow;
}

static std::string ReadAll(std::string const &path)
{
	std::string bytes;
	FILE *file = nullptr;

	if (fopen_s(&file, path.c_str(), "rb") == 0 && file)
	{
		char block[256];
		size_t got = 0;

		while ((got = fread(block, 1, sizeof(block), file)) > 0)
		{
			bytes.append(block, got);
		}

		fclose(file);
	}

	return bytes;
}

static bool WriteAll(std::string const &path, std::string const &bytes)
{
	FILE *file = nullptr;

	if (fopen_s(&file, path.c_str(), "wb") != 0 || !file)
	{
		return false;
	}

	bool const written = fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();

	return fclose(file) == 0 && written;
}

// Save pressed, `typed` answered, and the question that follows (if any) answered No; the film there must be as it was
static void SaveAnsweredNo(std::string const &typed, std::string const &filmPath, std::string const &wantedQuestion, std::string const &what)
{
	std::string const before = ReadAll(filmPath);
	int const questionsBefore = questionsAsked;

	askedQuestion.clear();
	Check(host::VideoMode_Press("Save") && AnswerNumber(typed), what + ": Save opens the number window, and " + typed + " is typed");
	Check(questionsAsked == questionsBefore + 1 && askedQuestion == wantedQuestion, what + ": the question is \"" + wantedQuestion + "\" (asked: \"" + askedQuestion + "\")");

	if (questionAnswer)
	{
		questionAnswer(false);
	}

	std::string const name = filmPath.substr(filmPath.find_last_of('\\') + 1);

	Check(Said() == "Not saved: FMVs\\" + name + " kept as it was" && ReadAll(filmPath) == before && !before.empty(), what + ": No keeps the film there, byte for byte (said: \"" +
		Said() + "\")");
}

int main()
{
	puts("host_video_mode_test: the Video mode's engine, pressed as its screen presses it");

	wchar_t temp[MAX_PATH] = {};

	if (GetTempPathW(MAX_PATH, temp) == 0)
	{
		puts("  FAILED  no temporary folder");
		return 1;
	}

	std::wstring const folder = std::wstring(temp) + L"host_video_mode_test_" + std::to_wstring(GetCurrentProcessId());
	std::wstring const films = folder + L"\\FMVs";
	std::wstring const clip = folder + L"\\clip.mp4";

	if (!CreateDirectoryW(folder.c_str(), nullptr) || !CreateDirectoryW(films.c_str(), nullptr))
	{
		puts("  FAILED  the test's folders can't be made");
		return 1;
	}

	int const letters = WideCharToMultiByte(CP_ACP, 0, folder.c_str(), -1, theGameFolder, MAX_PATH, nullptr, nullptr);

	Check(letters > 0, "the test's folder is the game folder the Save looks in");

	std::string const filmsFolder = std::string(theGameFolder) + "\\FMVs\\";

	// ---- with no video: every press but Cancel is refused with the way to give one; Cancel, with nothing being read or made, says so (Frost's two words)
	puts("No video:");
	CheckRefused("Cancel", "Nothing is being read or made", "Cancel with nothing read or made");
	CheckRefused("Fit", "Drop a video first (or use Load -> Browse)", "Fit with no video (the empty screen's own \"Load -> Browse\")");
	CheckRefused("Save", "Drop a video first (or use Load -> Browse)", "Save with no video");
	Check(formsAsked == 0, "and no window opened");
	CheckRefused(nullptr, "Drop a video first (or use Load -> Browse)", "a press with no name");
	Check(strstr(host::kVideoModeEmptyWords, "Load -> Browse") != nullptr, "the empty screen's words say \"Load -> Browse\" too, one form (" + std::string(host::kVideoModeEmptyWords) + ")");

	// ---- a video loaded: 2 s, 320 x 180, with a tone
	puts("A video:");
	bool const com = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED));
	bool const on = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET));
	std::string why;
	bool const made = on && TestClip_Make(clip, MFVideoFormat_H264, 320, 180, 30, 60, true, [](uint32_t, uint8_t *pixels) { TestClip_Fill(pixels, 320, 0, 180, 255, 0, 0); }, &why);

	Check(made, "the test's clip is made with Media Foundation (" + why + ")");
	bool const held = host::VideoMode_Dropped({ clip }, 1) && WaitFor(host::VideoModePhase::Loaded, 30000);

	Check(held, "the clip dropped is read and held (" + Said() + ")");
	Check(host::VideoMode_View().sourceWords == "clip.mp4: 320 x 180, 2 s, with sound", "its words: \"clip.mp4: 320 x 180, 2 s, with sound\" (" + host::VideoMode_View().sourceWords + ")");
	CheckRefused("Cancel", "Nothing is being read or made", "Cancel with a video held and nothing read or made");
	Check(!host::VideoMode_Press("Nope"), "a name that is no control: refused");
	CheckRefused("Stop", "Nothing is being previewed", "Stop with no preview playing (Preview's button reads Stop only while one plays)");
	Check(!host::VideoMode_Previewing(), "no preview plays: Preview's button reads Preview");

	// ---- the Save's number window: its box one row tall
	puts("The Save's window:");
	Check(host::VideoMode_Press("Save") && formsAsked == 1 && askedForm.widgets.size() >= 2 && askedForm.widgets[1].type == host::DialogWidgetType::Combo,
		"Save opens one window, its second part the number box");

	if (askedForm.widgets.size() >= 3)
	{
		host::DialogRect const box = askedForm.widgets[1].box;

		Check(box.height == 18 && askedForm.widgets[1].editable && askedForm.widgets[1].listRows == 12, "the number box is one row tall, 18, as the editor's other boxes "
			"(a combo's height is its row's, each row of its list as tall), takes a typed number, and its list shows 12 rows (box " + std::to_string(box.x) + " " +
			std::to_string(box.y) + " " + std::to_string(box.width) + " " + std::to_string(box.height) + ")");
		Check(box.y + box.height <= askedForm.widgets[2].box.y, "the box ends above the Ok button");
	}

	if (formAnswer)
	{
		formAnswer(host::kDialogEscape, askedForm.widgets);
	}

	Check(Said() == "Not saved", "Cancel on the window: \"Not saved\"");

	// ---- a number the game doesn't play: refused in words, no question
	int const questionsBefore = questionsAsked;

	bool const answered = host::VideoMode_Press("Save") && AnswerNumber("451");

	std::string const said = Said();

	Check(answered && said == "Not saved: 451: not a number the game plays (1 to 450)" && questionsAsked == questionsBefore, "451: refused in words, nothing asked (" + said + ")");

	// ---- a film there already: replaced only after a Yes, and the question says what the number is
	puts("A film there already:");
	Check(WriteAll(filmsFolder + "message47.smk", "the owner's own film 47") && WriteAll(filmsFolder + "message417.smk", "a film of 417") &&
		WriteAll(filmsFolder + "message144.smk", "Rebellion's film 144"), "three films put in its FMVs: 47 (a stock level plays 47), 417 (free) and 144 (a stock film no level plays)");
	SaveAnsweredNo("47", filmsFolder + "message47.smk", "FMVs\\message47.smk is there already, and a stock level plays it (Genshd1.RIF): replace it with this video? That level would then play yours",
		"47, a number a stock level plays");
	SaveAnsweredNo("417", filmsFolder + "message417.smk", "FMVs\\message417.smk is there already: replace it with this video?", "417, a free number");
	SaveAnsweredNo("144", filmsFolder + "message144.smk", "FMVs\\message144.smk is Rebellion's own film (no stock level plays it): replace it with this video? Steam's \"Verify "
		"integrity of game files\" will later put Rebellion's back over yours", "144, a stock film no level plays");

	// ---- a number a stock level plays, with NO film there yet (alpha.5's known issue, "a typed stock number with no film saves silently": it was
	// said, then saved): asked first, naming the level that plays it, and a No makes nothing (Dietrich, 2026-11-23, for alpha.6)
	puts("A stock-played number with no film there:");
	{
		int const questionsBefore = questionsAsked;

		askedQuestion.clear();
		Check(host::VideoMode_Press("Save") && AnswerNumber("103"), "103: Save opens the number window, and 103 is typed");

		bool const asked = questionsAsked == questionsBefore + 1;

		Check(asked && askedQuestion.find("a stock level plays number 103") != std::string::npos && askedQuestion.find("Derelict") != std::string::npos,
			"103 (stock Derelict's briefing), no film there: asked first, naming the level (asked: \"" + askedQuestion + "\")");

		if (asked && questionAnswer)
		{
			questionAnswer(false);
		}

		std::string const said = Said();

		Check(asked && said == "Not saved: FMVs\\message103.smk not made" && ReadAll(filmsFolder + "message103.smk").empty(), "No makes nothing (said: \"" + said + "\")");
	}

	Check(encodes == 0, "no film was made: every No made nothing");

	// ---- ONE PREVIEW FILM PER EDITOR (card 2d, cards-after-alpha5.md; Dietrich 2026-11-23, Bishop 2: in alpha.6). Two editors previewing at once (run-shards'
	// shards, or the owner's two windows) shared one %TEMP%\AVP_OpenInteng-video-preview\preview.smk: the one playing it held it open, so the other's preview was
	// refused, "Windows won't put the new file in place of preview.smk (error 5)" (the alpha.6 final rounds on b0e2f932 and f881cc47: acceptance-video-mode and
	// -video-preview-over). Here two other editors' films are held open as a playing editor holds its own: the shared name of the editors before this fix, and
	// another editor's own name (process id + 1: Windows' ids are multiples of 4, so no real process has it). A held file the test didn't make is never deleted.
	puts("One preview film per editor:");
	std::wstring const previews = host::VideoMode_PreviewFolder();
	std::wstring const own = L"preview-" + std::to_wstring((unsigned long)GetCurrentProcessId()) + L".smk";
	std::wstring const others[2] = { previews + L"\\preview.smk", previews + L"\\preview-" + std::to_wstring((unsigned long)GetCurrentProcessId() + 1) + L".smk" };
	HANDLE holds[2] = { INVALID_HANDLE_VALUE, INVALID_HANDLE_VALUE };
	bool ours[2] = { false, false };

	CreateDirectoryW(previews.c_str(), nullptr);

	for (int i = 0; i < 2; i++)
	{
		holds[i] = CreateFileW(others[i].c_str(), GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		ours[i] = holds[i] != INVALID_HANDLE_VALUE;
		holds[i] = ours[i] ? holds[i] : CreateFileW(others[i].c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	}

	Check(!previews.empty() && holds[0] != INVALID_HANDLE_VALUE && holds[1] != INVALID_HANDLE_VALUE, "two other editors' films held open, as a playing editor holds its own");

	std::string const named = Narrow(host::VideoMode_PreviewName());

	Check(named == Narrow(own), "this editor's film is named for it: " + Narrow(own) + " (it is \"" + named + "\")");

	std::vector<uint8_t> const film(4096, 0x5a);
	std::string putWhy;
	bool const put = host::VideoMode_PutPreview(film, &putWhy);

	Check(put, "with them held, this editor's preview is put in place (" + putWhy + ")");
	Check(put && ReadAll(Narrow(previews + L"\\" + own)) == std::string(4096, (char)0x5a), "and its film is the one made, byte for byte");

	host::VideoMode_Leave();
	Check(host::VideoMode_View().phase == host::VideoModePhase::Empty, "Mode > Exit: the video let go");
	Check(GetFileAttributesW((previews + L"\\" + own).c_str()) == INVALID_FILE_ATTRIBUTES, "and Mode > Exit (also the editor's end) takes this editor's film away");
	Check(GetFileAttributesW(others[0].c_str()) != INVALID_FILE_ATTRIBUTES && GetFileAttributesW(others[1].c_str()) != INVALID_FILE_ATTRIBUTES,
		"the other editors' films are left where they are");

	for (int i = 0; i < 2; i++)
	{
		if (holds[i] != INVALID_HANDLE_VALUE)
		{
			CloseHandle(holds[i]);
		}

		if (ours[i])
		{
			DeleteFileW(others[i].c_str());
		}
	}

	DeleteFileW((previews + L"\\" + own).c_str());

	// ---- A TEMP FOLDER LONGER THAN A PATH (found 2026-11-23 while moving the code above into VideoMode_PreviewFolder): by its contract GetTempPathW gives back
	// the size it NEEDS when the folder doesn't fit its buffer, and the code took that size as the folder's length, which would read past the buffer's end.
	// MEASURED here (Windows 11, 2026-11-23): a TMP of 304 letters is passed over and TEMP's folder given (66 letters with the preview's own), so that size
	// doesn't come back on this Windows and the read can't be made to happen: the guard is defensive. This holds what is promised either way: the folder given
	// is at most a path's length plus the preview folder's own name.
	puts("A temp folder longer than a path:");
	{
		static wchar_t keptTmp[32768] = {};
		DWORD const hadTmp = GetEnvironmentVariableW(L"TMP", keptTmp, 32768);
		std::wstring const longTmp = L"C:\\" + std::wstring(300, L'x') + L"\\";

		SetEnvironmentVariableW(L"TMP", longTmp.c_str());

		std::wstring const longFolder = host::VideoMode_PreviewFolder();

		SetEnvironmentVariableW(L"TMP", hadTmp > 0 && hadTmp < 32768 ? keptTmp : nullptr);
		Check(longFolder.size() <= MAX_PATH + wcslen(L"AVP_OpenInteng-video-preview"), "with a TMP of 304 letters the folder given is at most a path's length "
			"plus the preview folder's name (" + std::to_string(longFolder.size()) + " letters: \"" + Narrow(longFolder) + "\")");
	}

	// ---- THE PLAYER'S PATH IN THE SYSTEM'S OWN LETTERS (Vasquez's review of 605357a1, SHOULD 1): Barry's decoder opens the preview by a name in the system's code
	// page, and the conversion's result was never looked at: a folder that doesn't fit, or letters the code page can't say, gave the player a cut name or one of '?'s.
	// Georgian letters are in no Windows code page, so the second case is the same on every PC
	puts("The player's path in the system's own letters:");
	{
		static wchar_t keptTmp[32768] = {};
		DWORD const hadTmp = GetEnvironmentVariableW(L"TMP", keptTmp, 32768);
		std::string folderForPlayer, nameForPlayer, pathWhy;
		bool const usual = host::VideoMode_PreviewPlayerPath(&folderForPlayer, &nameForPlayer, &pathWhy);

		Check(usual && folderForPlayer.size() > 1 && folderForPlayer.back() == '\\' && nameForPlayer == Narrow(host::VideoMode_PreviewName()),
			"the usual temp folder: said whole, with its last '\\', and the film's name (" + folderForPlayer + nameForPlayer + ")");

		SetEnvironmentVariableW(L"TMP", L"C:\\Temp\\\u10d0\u10d1\\");   // two Georgian letters: no code page says them
		folderForPlayer.clear();
		pathWhy.clear();

		bool const said = host::VideoMode_PreviewPlayerPath(&folderForPlayer, &nameForPlayer, &pathWhy);

		SetEnvironmentVariableW(L"TMP", hadTmp > 0 && hadTmp < 32768 ? keptTmp : nullptr);
		Check(!said && !pathWhy.empty(), "a temp folder with letters the code page can't say: refused in words, never a name of '?'s for the player (gave \"" + folderForPlayer +
			"\": \"" + pathWhy + "\")");
	}

	if (on)
	{
		MFShutdown();
	}

	if (com)
	{
		CoUninitialize();
	}

	for (wchar_t const *name : { L"\\message47.smk", L"\\message417.smk", L"\\message144.smk" })
	{
		DeleteFileW((films + name).c_str());
	}

	DeleteFileW(clip.c_str());
	RemoveDirectoryW(films.c_str());
	RemoveDirectoryW(folder.c_str());

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
