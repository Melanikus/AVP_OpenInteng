// stock_proof_main.cpp: the F4 stock proof (the phase 2 plan, F4: every stock level opens and draws, and damaged files are refused
// cleanly). Written 2026-11-14 by Melanikus (Hicks) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's
// own code".
//
// WHY A START OF ITS OWN: it runs the editor's own code (level_view, the edge) in place of the editor's start, so nothing of the proof
// ships in the editor. The project builds it with /p:StockProof=true, which swaps this file in for editor_main.cpp.
//
// It's not a test the repository runs: it needs the game's own levels, which are never in the repository, and a Direct3D device. It's
// run by hand, in a lane, with Bishop's word. Nobody is at the screen (nobody_watching.h): its window is hidden as soon as Direct3D is in
// it, and never takes the keyboard.
//
// WHAT IT PROVES, in a data folder laid out as the game's own (avp_rifs, avp_huds, fastfile, shaders):
//   1. every level, avp_rifs\*.rif: LevelView_Open, then the editor's own hold of the level's file (LevelObjects_Hold, the call the editor
//      makes: the level's bytes, and the chunks the game's loader made from exactly those), then 30 frames drawn. Each must open, be held,
//      and give an object list with rows, before the frames and after them. Invasion's list must be what its file says: 539 objects, and
//      row 103 (rows count from 0, as the harness's do) DoorHole, with the ID 7334ED09EF81A000. (Added 2026-11-15 by Melanikus (Dietrich),
//      after checkpoint 1's hidden run found the editor's object list empty: the game lets go of its own copy of a level's file as soon
//      as the level is loaded, so the list now reads the editor's own hold of it, level_objects.h.)
//   2. every model, avp_huds\*.rif: the game's own model loader (avp_load_rif_non_env), then let go. Each must load.
//   3. every damaged file in the damaged folder, copied in as avp_rifs\proof_damaged_<n>.rif: each must be refused, with a reason. Then
//      the first level must still open and draw, three times over for each, so whatever a refusal leaves behind is tried too.
// Usage: AVP_OpenInteng.exe <data folder> <damaged folder> <result file>. It answers 0 when every check passed, and writes every step
// to the result file.

#include "3dc.h"
#include "gameplat.h"
#include "platform.h"
#include "renderer.h"
#include "d3_func.h"
#include "projload.hpp"

#include <windows.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "level_file.h"
#include "level_objects.h"
#include "level_view.h"
#include "hierarchy_player.h"
#include "nobody_watching.h"

extern HWND hWndMain;

static FILE *result = nullptr;
static int failures = 0;

static void Note(bool good, const std::string &what)
{
	fprintf(result, "%s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	fflush(result);
	failures += good ? 0 : 1;
}

// The file names in a folder that end in ".rif" (any case), sorted, without the ".rif"
static std::vector<std::string> RifNames(const std::string &folder)
{
	std::vector<std::string> names;
	WIN32_FIND_DATAA found;
	HANDLE const search = FindFirstFileA((folder + "\\*").c_str(), &found);
	if (search == INVALID_HANDLE_VALUE)
	{
		return names;
	}
	do
	{
		std::string const name = found.cFileName;
		if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && name.size() > 4 && _stricmp(name.c_str() + name.size() - 4, ".rif") == 0)
		{
			names.push_back(name.substr(0, name.size() - 4));
		}
	} while (FindNextFileA(search, &found));
	FindClose(search);
	std::sort(names.begin(), names.end());
	return names;
}

// How the editor's open of a level went, in words for the result file
static const char *OpenWords(LevelOpenResult result)
{
	switch (result)
	{
	case LevelOpenResult::Opened:
		return "opened";
	case LevelOpenResult::NoPath:
		return "no usable path";
	case LevelOpenResult::CantRead:
		return "the file couldn't be read whole";
	case LevelOpenResult::LoaderRefused:
		return "the game's loader refuses the file";
	case LevelOpenResult::ChangedWhileOpening:
		return "the file changed while it was being opened";
	case LevelOpenResult::DoesntPair:
		return "the chunks don't pair with the bytes";
	case LevelOpenResult::TooManyChunks:
		return "more chunks than the editor takes";
	}

	return "a result the proof doesn't know";
}

static int listsHeld = 0;   // the levels the editor held, with rows in their lists both before the frames and after them

// The object list of the level the editor holds (LevelObjects_Read, which the editor's list is loaded from): it has rows, and Invasion's are
// what its file says. "when" says which time. Gives back whether the list has rows. (Added 2026-11-15 by Melanikus (Dietrich).)
static bool CheckTheList(const std::string &name, const char *when)
{
	std::vector<host::LevelObject> rows;
	bool const read = LevelObjects_Read(&rows);
	bool const hasRows = read && !rows.empty();

	Note(hasRows, "the object list of " + name + " has rows " + when + " (" + std::to_string(rows.size()) + ")");

	if (hasRows && _stricmp(name.c_str(), "Invasion") == 0)
	{
		static uint8_t const doorHoleId[host::kObjectIdBytes] = { 0x73, 0x34, 0xED, 0x09, 0xEF, 0x81, 0xA0, 0x00 };

		Note(rows.size() == 539, std::string("Invasion's object list has 539 rows ") + when + " (it has " + std::to_string(rows.size()) + ")");

		// What row 103 is, in plain letters, for the result file: its ID, and its name's own bytes, never read past the game's 64 and
		// only the printable ones
		std::string actual = "there is no row 103";

		if (rows.size() > 103)
		{
			char id[2 * host::kObjectIdBytes + 1] = "";
			std::string text;

			for (size_t i = 0; i < host::kObjectIdBytes; i++)
			{
				_snprintf_s(id + i * 2, 3, _TRUNCATE, "%02X", (unsigned)rows[103].id[i]);
			}

			for (size_t i = 0; rows[103].name && i < 63 && rows[103].name[i]; i++)
			{
				char const letter = rows[103].name[i];

				text += (letter >= 32 && letter <= 126) ? letter : '?';
			}

			actual = "it is \"" + text + "\", ID " + id;
		}

		bool const isDoorHole = rows.size() > 103 && memcmp(rows[103].id, doorHoleId, sizeof(doorHoleId)) == 0 && rows[103].name &&
			strncmp(rows[103].name, "DoorHole", 9) == 0;

		Note(isDoorHole, std::string("Invasion's row 103 is DoorHole, with the ID 7334ED09EF81A000, ") + when + " (" + actual + ")");
	}

	return hasRows;
}

// Opens a level and draws 30 frames from where the level puts the player. With hold, the editor's own hold of the level's file is made
// between the open and the frames, as the editor does it (LevelObjects_Hold), kept while they're drawn, and its object list is checked
// before them and after them. Gives back whether it opened, and why not.
static bool OpenAndDraw(const std::string &name, std::string *why, bool hold = false)
{
	char reason[300] = { 0 };
	FlyCamera camera;
	if (!LevelView_Open(name.c_str(), &camera, reason, sizeof(reason)))
	{
		*why = reason;
		return false;
	}

	// Made here, so that it's let go of when this returns: before the next level opens and before the game's own teardown, as in the editor
	std::unique_ptr<OpenLevelFile> level(hold ? new OpenLevelFile : nullptr);
	bool listBefore = false;

	if (hold)
	{
		LevelOpenResult const held = LevelObjects_Hold(level.get());

		Note(held == LevelOpenResult::Opened, "the editor holds the file of " + name + ": " + OpenWords(held));
		listBefore = CheckTheList(name, "before its frames");
	}

	for (int frame = 0; frame < 30; frame++)
	{
		MSG message;
		while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
		{
			DispatchMessageA(&message);
		}
		LevelView_Draw(camera, nullptr);
	}

	if (hold)
	{
		bool const listAfter = CheckTheList(name, "after its frames");

		listsHeld += (listBefore && listAfter) ? 1 : 0;
		LevelObjects_Use(nullptr);
	}

	Note(GetForegroundWindow() != hWndMain, "the window hasn't taken the keyboard, after " + name);
	return true;
}

static LRESULT CALLBACK ProofWindowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProcA(window, message, wParam, lParam);
}

int main(int argc, char **argv)
{
	if (argc != 4)
	{
		fprintf(stderr, "usage: AVP_OpenInteng.exe <data folder> <damaged folder> <result file>\n");
		return 2;
	}
	std::string const dataFolder = argv[1];
	std::string const damagedFolder = argv[2];
	if (fopen_s(&result, argv[3], "w") != 0 || !result)
	{
		fprintf(stderr, "can't write the result file\n");
		return 2;
	}
	NobodyWatching_Set();

	DWORD const rifsKind = GetFileAttributesA((dataFolder + "\\avp_rifs").c_str());
	if (rifsKind == INVALID_FILE_ATTRIBUTES || !(rifsKind & FILE_ATTRIBUTE_DIRECTORY) || !SetCurrentDirectoryA(dataFolder.c_str()))
	{
		Note(false, "the data folder holds avp_rifs");
		return 1;
	}

	WNDCLASSEXA windowClass;
	memset(&windowClass, 0, sizeof(windowClass));
	windowClass.cbSize = sizeof(windowClass);
	windowClass.lpfnWndProc = ProofWindowProc;
	windowClass.hInstance = GetModuleHandleA(NULL);
	windowClass.lpszClassName = "AVP_OpenInteng_proof";
	RegisterClassExA(&windowClass);
	hWndMain = CreateWindowExA(WS_EX_NOACTIVATE, "AVP_OpenInteng_proof", "AVP_OpenInteng proof", WS_OVERLAPPEDWINDOW, 0, 0, 800, 600, NULL,
		NULL, windowClass.hInstance, NULL);
	WindowMode = WindowModeSubWindow;
	if (!hWndMain || !InitialiseDirect3D())
	{
		Note(false, "the game's Direct3D starts in a window");
		return 1;
	}
	// The game's sizing showed the window (without the keyboard, nobody_watching.h): hide it, so nobody's screen has it for minutes
	ShowWindow(hWndMain, SW_HIDE);
	Note(!IsWindowVisible(hWndMain) && GetForegroundWindow() != hWndMain, "the window is hidden and never had the keyboard");

	char why[300] = { 0 };
	Note(LevelView_Start(why, sizeof(why)), std::string("the game's own start: ") + why);

	// 1. every level
	std::vector<std::string> const levels = RifNames(dataFolder + "\\avp_rifs");
	int opened = 0;
	for (const std::string &level : levels)
	{
		if (level.compare(0, 14, "proof_damaged_") == 0)
		{
			continue;
		}
		std::string reason;
		bool const ok = OpenAndDraw(level, &reason, true);
		Note(ok, ok ? "level " + level + " opens and draws" : "level " + level + " was refused: " + reason);
		opened += ok ? 1 : 0;
	}
	LevelView_Close();

	// 2. every model
	std::vector<std::string> const models = RifNames(dataFolder + "\\avp_huds");
	int loaded = 0;
	for (const std::string &model : models)
	{
		RIFFHANDLE const h = avp_load_rif_non_env(("avp_huds/" + model + ".rif").c_str());
		bool const ok = h != INVALID_RIFFHANDLE;
		Note(ok, "model " + model + (ok ? " loads" : " was refused"));
		if (ok)
		{
			avp_undo_rif_load(h);
		}
		loaded += ok ? 1 : 0;
	}

	// 2b. every model plays: the game's own HModel code poses each hierarchy's sequences at four times; a sequence with several keyframes must show different poses, and the
	// numbers of one section are written to the result (the proof of the Hierarchy view's data: hierarchy_player.h)
	int posedSequences = 0, movingSequences = 0, playBad = 0;
	for (const std::string &model : models)
	{
		host::HierarchyPlayer player;
		std::vector<std::string> hierarchies;
		std::string why;
		if (!player.Load(model, "", &hierarchies, &why))
		{
			Note(hierarchies.empty(), "model " + model + " holds no hierarchy (" + why + ")");
			continue;
		}
		for (const std::string &hierarchyName : hierarchies)
		{
			host::HierarchyPlayer each;
			std::vector<host::PlayedSequence> sequences;
			if (!each.Load(model, hierarchyName, nullptr, &why) || !each.Sequences(&sequences))
			{
				Note(false, "model " + model + " hierarchy " + hierarchyName + " could not be played: " + why);
				playBad++;
				continue;
			}
			for (const host::PlayedSequence &sequence : sequences)
			{
				if (!each.Start(sequence.type, sequence.subsequence, &why))
				{
					Note(false, model + "/" + hierarchyName + " sequence " + std::to_string(sequence.type) + "," + std::to_string(sequence.subsequence) + " could not start: " + why);
					playBad++;
					continue;
				}
				std::vector<std::vector<int32_t>> poses;
				for (int quarter = 0; quarter < 4; quarter++)
				{
					each.Pose(quarter * 16384);
					std::vector<int32_t> numbers;
					for (const host::PosedSection &section : each.Sections())
					{
						numbers.insert(numbers.end(), section.place, section.place + 3);
						numbers.insert(numbers.end(), section.matrix, section.matrix + 9);
					}
					poses.push_back(numbers);
				}
				posedSequences++;
				bool differ = false;
				for (size_t q = 1; q < poses.size(); q++)
				{
					differ = differ || poses[q] != poses[0];
				}
				movingSequences += differ ? 1 : 0;
				// a sequence of several keyframes must move; one frame may stand still
				bool const good = !each.Sections().empty() && (differ || sequence.frames < 2);
				Note(good, model + "/" + hierarchyName + " sequence " + std::to_string(sequence.type) + "," + std::to_string(sequence.subsequence) + " (" + std::to_string(sequence.frames) + " frames, " +
					std::to_string(each.Sections().size()) + " sections): " + (differ ? "the poses at 0, 1/4, 1/2, 3/4 differ" : "the poses are the same"));
				playBad += good ? 0 : 1;
				{
					// the box of the shapes' points over the four poses (Calculate Bounding Box's data: HierarchyPlayer::Box): it must be a real box
					std::vector<int> quarters = { 0, 16384, 32768, 49152 };
					int32_t low[3] = { 0, 0, 0 }, high[3] = { 0, 0, 0 };
					bool const box = each.Box(quarters, low, high);
					bool const real = box && low[0] <= high[0] && low[1] <= high[1] && low[2] <= high[2];

					if (sequence.frames >= 2 && !real)
					{
						Note(false, model + "/" + hierarchyName + " sequence " + std::to_string(sequence.type) + "," + std::to_string(sequence.subsequence) + ": no bounding box");
						playBad++;
					}
					else if (real && sequence.type == sequences[0].type && sequence.subsequence == sequences[0].subsequence)
					{
						fprintf(result, "      box of the first sequence: x %d to %d, y %d to %d, z %d to %d\n", low[0], high[0], low[1], high[1], low[2], high[2]);
						fflush(result);
					}
				}
				// the root-most moving section's numbers at the four times, for the picture of what moved
				if (differ && sequence.frames >= 2 && poses[0].size() >= 12)
				{
					size_t pick = 0;
					for (size_t at = 0; at + 12 <= poses[0].size(); at += 12)
					{
						if (!std::equal(poses[0].begin() + at, poses[0].begin() + at + 12, poses[2].begin() + at))
						{
							pick = at;
							break;
						}
					}
					char line[256];
					for (int quarter = 0; quarter < 4; quarter += 2)
					{
						std::vector<int32_t> const &pose = poses[(size_t)quarter];
						_snprintf_s(line, sizeof(line), _TRUNCATE, "      first moving section (%zu) at %d/4: place %d %d %d, matrix row 1 %d %d %d", pick / 12, quarter,
							pose[pick], pose[pick + 1], pose[pick + 2], pose[pick + 3], pose[pick + 4], pose[pick + 5]);
						fprintf(result, "%s\n", line);
						fflush(result);
					}
				}
			}
		}
	}
	Note(playBad == 0 && posedSequences > 0 && movingSequences > 0, "the game's own HModel plays the stock models: " + std::to_string(posedSequences) + " sequences posed, " + std::to_string(movingSequences) + " moving");

	// 3. every damaged file: refused, and the first level still opens and draws after it, three times over
	std::vector<std::string> const damaged = RifNames(damagedFolder);
	int refused = 0;
	for (size_t i = 0; i < damaged.size() && !levels.empty(); i++)
	{
		std::string const name = "proof_damaged_" + std::to_string(i);
		std::string const copy = dataFolder + "\\avp_rifs\\" + name + ".rif";
		for (int round = 0; round < 3; round++)
		{
			if (!CopyFileA((damagedFolder + "\\" + damaged[i] + ".rif").c_str(), copy.c_str(), FALSE))
			{
				Note(false, "the damaged file " + damaged[i] + " can be copied in");
				break;
			}
			std::string reason;
			bool const wasRefused = !OpenAndDraw(name, &reason);
			Note(wasRefused, "the damaged file " + damaged[i] + (wasRefused ? " is refused: " + reason : " WAS OPENED"));
			refused += (wasRefused && round == 0) ? 1 : 0;
			std::string again;
			bool const stillOpens = OpenAndDraw(levels[0], &again);
			Note(stillOpens, "after it, " + levels[0] + (stillOpens ? " still opens and draws" : " was refused: " + again));
			DeleteFileA(copy.c_str());
		}
	}
	LevelView_Close();

	// Nothing may have tried to show a dialog: nobody is at the screen (nobody_watching.h refused any, and counted it)
	Note(NobodyWatching_DialogsRefused() == 0, std::string("no dialog was asked for") +
		(NobodyWatching_DialogsRefused() ? std::string(": the last was titled ") + NobodyWatching_LastDialogTitle() : std::string()));

	fprintf(result, "object lists: %d of %d opened levels held by the editor, with rows before and after their frames\n", listsHeld, opened);
	fprintf(result, "levels: %d of %d open and draw; models: %d of %d load; damaged files: %d of %d refused; %d checks failed\n", opened,
		(int)levels.size(), loaded, (int)models.size(), refused, (int)damaged.size(), failures);
	fclose(result);
	ReleaseDirect3D();
	DestroyWindow(hWndMain);
	return failures ? 1 : 0;
}
