// all_stock_maps_host.cpp: see all_stock_maps.h. Written 2026-11-22 by Melanikus (Dietrich) for AVP_OpenInteng (the owner's priority: "my waypoint all
// maps"). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// THE STEPS of one level (Vasquez's plan, AvP-docs director/plan-all-stock-maps.md), one a frame, so the editor keeps drawing:
//   Check    <data>\avp_rifs\<name>.RIF opened without following a link, judged on its handle (a plain file on a disk, one name, at most 16 MB),
//            read whole and hashed (SHA-256); anything else is Skipped, named
//   Backup   <name>.RIF.bak written CREATE_NEW from the bytes just read; one there already is kept as it is (the oldest copy is always kept); one
//            that can't be written makes the level Refused, untouched
//   Open     the level before closed and this one opened, in the same sitting (level_reopen.h)
//   Plot     the alien auto waypoints on every AI module (AutoPlotCommand_Run, or RunAnswered with the Overwrite tick), then waited for; how it ended
//            is read from AutoPlotCommand_LastResult, never from its words
//   Recheck  the file opened and hashed again: the same bytes as at Check, or Refused ("changed on disk while it was open")
//   Save     LevelSave_Over with no backup name of its own (the .bak from Backup stays exactly as it is), then marked in the sitting (R11)
// After the last level, or a stop, the level open before the run is opened again.
#include "all_stock_maps.h"

#include <windows.h>

#include <string.h>

#include <vector>

#include "autoplot_command.h"
#include "autoplot_switch.h"
#include "editor_shared.h"
#include "env_info.h"
#include "level_file.h"
#include "level_reopen.h"
#include "level_save.h"
#include "level_view.h"     // FlyCamera: the owner's camera, kept across the run's levels
#include "sha256_stream.h"
#include "waypoint_mode.h"

extern char LevelName[MAX_PATH];   // the game's level name (winmain.cpp:137), which SetLevelToLoad writes
host::LevelSaveSitting &EditorSaveSitting();   // editor_main.cpp

namespace
{
	uint64_t const kLevelMostBytes = 16ull * 1024 * 1024;   // open_with.cpp's cap on a level file (Vasquez's plan, step 2)

	enum class Step
	{
		Idle,
		Check,
		Backup,
		Open,
		StartPlot,
		Plotting,
		Recheck,
		Save,
		Next,
		ReopenBefore
	};

	struct Run
	{
		Step step = Step::Idle;
		std::vector<std::string> names;              // env_info's level names, "derelict"
		size_t at = 0;
		std::vector<host::AllStockMapsLevel> report;
		host::AutoPlotVolumeCounts counts;
		bool overwriteLinked = false;
		bool stopAsked = false;
		std::string before;                          // the level open when the run began, opened again at its end
		bool beforeOpenedWith = false;               // ... unless it was opened with "Open with": then it is not (AllStockMaps_Start says why)
		std::string beforeFile;                      // that level's file name, for the report (the name only: the owner records the run, so no folder of his is shown)
		bool closedBefore = false;                   // the first level's open has closed the level open before
		FlyCamera camera = { 0, 0, 0, 0, 0, 0 };     // the camera as the run began, put back on every level (AllStockMaps_Start)
		bool cameraKept = false;
		std::vector<char> bytes;                     // the level's file as Check read it
		std::string sha256;
		std::string stage;
		DWORD lastFrameAt = 0;
		DWORD worstFrameMs = 0;
	} run;

	std::wstring Wide(std::string const &narrow)
	{
		int const letters = MultiByteToWideChar(CP_ACP, 0, narrow.c_str(), (int)narrow.size(), nullptr, 0);
		std::wstring wide(letters > 0 ? (size_t)letters : 0, L'\0');

		if (letters > 0)
		{
			MultiByteToWideChar(CP_ACP, 0, narrow.c_str(), (int)narrow.size(), &wide[0], letters);
		}

		return wide;
	}

	// a wide path's last part (its file name), in the code page the editor's words are in; empty when it can't be told
	std::string FileNameOf(std::wstring const &path)
	{
		size_t const slash = path.find_last_of(L"\\/");
		std::wstring const name = slash == std::wstring::npos ? path : path.substr(slash + 1);
		char narrow[MAX_PATH] = "";

		if (name.empty() || WideCharToMultiByte(CP_ACP, 0, name.c_str(), -1, narrow, (int)sizeof(narrow), nullptr, nullptr) <= 0)
		{
			return std::string();
		}

		return narrow;
	}

	// THE OWNER'S CAMERA, put back (AllStockMaps_Start): a level's open puts the camera at the origin (OpenTheLevel, inteng's first frame), so
	// it is set again right after, before the plot's first frame draws
	void PutTheCameraBack()
	{
		if (run.cameraKept && theCamera)
		{
			*theCamera = run.camera;
		}
	}

	// the level's file, BUILT from the data folder and the name, never taken from a folder listing
	std::string PathOf(std::string const &name)
	{
		std::string folder = theGameFolder;

		while (!folder.empty() && (folder.back() == '\\' || folder.back() == '/'))
		{
			folder.pop_back();
		}

		return folder + "\\avp_rifs\\" + name + ".RIF";
	}

	std::string Hex(uint8_t const digest[32])
	{
		static char const digits[] = "0123456789abcdef";
		std::string out;

		for (int i = 0; i < 32; i++)
		{
			out += digits[digest[i] >> 4];
			out += digits[digest[i] & 15];
		}

		return out;
	}

	// Check and Recheck: the file opened without following a link (FILE_FLAG_OPEN_REPARSE_POINT, sharing read only, so nothing writes it while it
	// is read), judged on the OPENED handle, read whole and hashed. "" when it may be used, else why not.
	std::string ReadLevel(std::string const &path, std::vector<char> *bytes, std::string *sha256)
	{
		HANDLE const file = CreateFileW(Wide(path).c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
			FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			DWORD const error = GetLastError();

			return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? "missing" : "can't be opened (Windows error " + std::to_string((unsigned long)error) + ")";
		}

		struct Closer
		{
			HANDLE h;
			~Closer() { CloseHandle(h); }
		} const closer = { file };

		BY_HANDLE_FILE_INFORMATION info;

		if (GetFileType(file) != FILE_TYPE_DISK || !GetFileInformationByHandle(file, &info) ||
			(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
		{
			return "a link or a folder, not a plain file";
		}

		if (info.nNumberOfLinks != 1)
		{
			return "the file has " + std::to_string((unsigned long)info.nNumberOfLinks) + " names (a hard link): writing it would change the others too";
		}

		uint64_t const size = ((uint64_t)info.nFileSizeHigh << 32) | info.nFileSizeLow;

		if (size == 0 || size > kLevelMostBytes)
		{
			return size == 0 ? "empty" : "larger than 16 MB";
		}

		bytes->assign((size_t)size, 0);

		size_t got = 0;

		while (got < bytes->size())
		{
			DWORD read = 0;
			DWORD const ask = (DWORD)(bytes->size() - got > 1024 * 1024 ? 1024 * 1024 : bytes->size() - got);

			if (!ReadFile(file, bytes->data() + got, ask, &read, nullptr) || read == 0)
			{
				return "can't be read to its end";
			}

			got += read;
		}

		Sha256Stream stream;
		uint8_t digest[32];

		Sha256_Start(&stream);
		Sha256_Add(&stream, bytes->data(), bytes->size());
		Sha256_Finish(&stream, digest);
		*sha256 = Hex(digest);
		return std::string();
	}

	// Backup: CREATE_NEW, so a .bak there is never written over. "" written, "kept" already there, else why it couldn't be written
	std::string WriteBackup(std::string const &path, std::vector<char> const &bytes)
	{
		std::wstring const bak = Wide(path + ".bak");
		HANDLE const file = CreateFileW(bak.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			DWORD const error = GetLastError();

			if (error != ERROR_FILE_EXISTS && error != ERROR_ALREADY_EXISTS)
			{
				return "Windows error " + std::to_string((unsigned long)error);
			}

			// SOMETHING IS THERE ALREADY (Vasquez's MUST 2): it counts as the kept backup only if it is a plain file, as the editor's own Save rules
			// (level_file.cpp, F7 L6: "a folder or a link of that name isn't the file's copy"). Judged on its handle: on a disk, not a folder or a
			// reparse point (a link, a dangling one too), one name only (a hard link is another file's), not empty. Anything else is no backup.
			HANDLE const there = CreateFileW(bak.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
				FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
			BY_HANDLE_FILE_INFORMATION info;
			bool const plain = there != INVALID_HANDLE_VALUE && GetFileType(there) == FILE_TYPE_DISK && GetFileInformationByHandle(there, &info) &&
				!(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) && info.nNumberOfLinks == 1 &&
				(info.nFileSizeHigh != 0 || info.nFileSizeLow != 0);

			if (there != INVALID_HANDLE_VALUE)
			{
				CloseHandle(there);
			}

			return plain ? "kept" : "it is there but isn't a plain file (a folder, a link, a second name of another file, or empty), so it is no backup";
		}

		DWORD put = 0;
		bool const ok = WriteFile(file, bytes.data(), (DWORD)bytes.size(), &put, nullptr) && put == bytes.size() && FlushFileBuffers(file);

		CloseHandle(file);

		if (!ok)
		{
			DeleteFileW(bak.c_str());   // the half-written one is this run's own, just made by CREATE_NEW
			return "the backup could not be written whole";
		}

		return std::string();
	}

	// THE GAME LOADER'S OWN LOG (avpx/3dc/level_log.h): a level that doesn't load, or loads with something refused, leaves <name>_errorlog.txt beside
	// it. The batch's open made it, so the batch tidies it (Bishop 2, 2026-11-22: never left in avp_rifs): a log that wasn't there before the open is
	// moved into the editor's own folder, logs\all-stock-maps\, under a name of its own, never over a file there. One there before keeps its bytes
	// (PutBackOldLoaderLog: the loader writes over it).
	std::string LoaderLogPath(std::string const &name)
	{
		std::string folder = theGameFolder;

		while (!folder.empty() && (folder.back() == '\\' || folder.back() == '/'))
		{
			folder.pop_back();
		}

		return folder + "\\avp_rifs\\" + name + "_errorlog.txt";
	}

	// "" when no new log is there; else where it is now (moved), or why it is still where the loader put it
	std::string TidyLoaderLog(std::string const &log, std::string const &name)
	{
		std::wstring const from = Wide(log);
		DWORD const attributes = GetFileAttributesW(from.c_str());

		if (attributes == INVALID_FILE_ATTRIBUTES)
		{
			return std::string();
		}

		wchar_t exe[MAX_PATH] = {};
		DWORD const length = GetModuleFileNameW(nullptr, exe, MAX_PATH);
		std::wstring folder(exe, length);
		size_t const slash = folder.find_last_of(L"\\/");

		if (length == 0 || length >= MAX_PATH || slash == std::wstring::npos)
		{
			return log + " (left there: the editor's own folder can't be found)";
		}

		folder = folder.substr(0, slash) + L"\\logs";
		CreateDirectoryW(folder.c_str(), nullptr);
		folder += L"\\all-stock-maps";
		CreateDirectoryW(folder.c_str(), nullptr);

		SYSTEMTIME now;

		GetLocalTime(&now);

		wchar_t stamp[32];

		swprintf_s(stamp, L"%04u%02u%02u-%02u%02u%02u", now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond);

		std::wstring const to = folder + L"\\" + Wide(name) + L"_errorlog-" + stamp + L".txt";

		// MoveFileExW with no MOVEFILE_REPLACE_EXISTING: never over a file there; a link is moved as the link, never followed
		if (!MoveFileExW(from.c_str(), to.c_str(), MOVEFILE_COPY_ALLOWED))
		{
			return log + " (left there: it could not be moved, Windows error " + std::to_string((unsigned long)GetLastError()) + ")";
		}

		int const bytes = WideCharToMultiByte(CP_ACP, 0, to.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string narrow(bytes > 1 ? (size_t)bytes - 1 : 0, '\0');

		if (bytes > 1)
		{
			WideCharToMultiByte(CP_ACP, 0, to.c_str(), -1, &narrow[0], bytes, nullptr, nullptr);
		}

		return narrow;
	}

	// AN OLD LOG, THERE BEFORE THE OPEN (2026-11-23, the alpha.6 final round: stock Breakout_P's old log in avp_rifs came out holding the batch's log). The
	// loader writes its log over any file of that name, so leaving an old one "alone" wasn't enough. Its bytes are kept before the open (read as a level is
	// read, ReadLevel: a plain file of one name, not a link, 1 byte to 16 MB). Here, after the open, when they changed: the loader's new log is moved out
	// as a new one is (TidyLoaderLog), and the old bytes are put back under its name (CREATE_NEW: the name is free once the new log is moved; a half-written
	// copy is deleted). "" when the open left it as it was; else the words for the level's report (where the new log went, and anything that failed).
	std::string PutBackOldLoaderLog(std::string const &log, std::string const &name, std::vector<char> const &before, std::string const &beforeSha)
	{
		std::vector<char> after;
		std::string afterSha;
		std::string const read = ReadLevel(log, &after, &afterSha);

		if (read.empty() && afterSha == beforeSha)
		{
			return std::string();
		}

		std::string const moved = read == "missing" ? std::string() : TidyLoaderLog(log, name);
		std::wstring const path = Wide(log);

		if (GetFileAttributesW(path.c_str()) != INVALID_FILE_ATTRIBUTES)
		{
			return moved + "; the log that was there before couldn't be put back: the loader's new one is still in its place";
		}

		HANDLE const file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			return moved + "; the log that was there before couldn't be put back (Windows error " + std::to_string((unsigned long)GetLastError()) + ")";
		}

		DWORD put = 0;
		bool const ok = WriteFile(file, before.data(), (DWORD)before.size(), &put, nullptr) && put == before.size() && FlushFileBuffers(file);

		CloseHandle(file);

		if (!ok)
		{
			DeleteFileW(path.c_str());   // the half-written copy is this run's own, just made by CREATE_NEW
			return moved + "; the log that was there before couldn't be written back whole";
		}

		return moved;
	}

	host::AllStockMapsLevel &Here()
	{
		return run.report[run.at];
	}

	void EndLevel(host::StockMapOutcome outcome, std::string const &why)
	{
		Here().outcome = outcome;
		Here().why = why;

		// a loader's log the open left (moved out of avp_rifs) is named in every outcome's words, once
		if (!Here().loaderLog.empty() && why.find(Here().loaderLog) == std::string::npos)
		{
			Here().why += "; the loader's log is " + Here().loaderLog;
		}

		run.step = Step::Next;
	}

	std::vector<std::string> LevelNames()
	{
		std::vector<std::string> names;
		std::string const kept = LevelName;   // the game's own level name, put back after the table is read (SetLevelToLoad writes it)

		for (int const campaign : { kCampaignMarine, kCampaignPredator })
		{
			for (int place = 0; place < Campaign_LevelCount(campaign); place++)
			{
				int const environment = Campaign_Environment(campaign, place);

				if (environment < 0)
				{
					continue;
				}

				SetLevelToLoad((AVP_ENVIRONMENT_ID)environment);
				names.push_back(LevelName);
			}
		}

		strcpy_s(LevelName, sizeof(LevelName), kept.c_str());   // the game's own buffer, MAX_PATH long (winmain.cpp), and kept came from it
		return names;
	}
}

std::vector<std::string> host::AllStockMaps_LevelFiles()
{
	std::vector<std::string> files;

	for (std::string const &name : LevelNames())
	{
		files.push_back(name + ".RIF");
	}

	return files;
}

bool host::AllStockMaps_ChangesNotSaved()
{
	return editedLevel && editedLevel->HasChanges();
}

std::string host::AllStockMaps_WhyNot(bool changesAllowed)
{
	std::string said;

	if (run.step != Step::Idle)
	{
		said = "a run is going";
	}
	else if (!theGameFolder[0])
	{
		said = "the editor has no data folder";
	}
	else if (!changesAllowed && AllStockMaps_ChangesNotSaved())
	{
		said = "the open level has changes not saved: save it first, so nothing of yours is lost";
	}
	else if (AutoPlotCommand_Busy() || AutoLinkRunning() || DeleteLinksRunning())
	{
		said = "a plot, AutoLink or Delete Links is running: wait for its end";
	}
	else if (host::AutoPlot_CommandHidden(host::kWaypointAutoPlotAll, host::AutoPlot_Shown()))
	{
		said = "AutoPlot is off in the editor's ini (AutoPlot=1 turns it on): the plot would do nothing";
	}
	else if (!OnWaypointScreen() || waypointMode.Mode() != 0)
	{
		// the plot runs only on the Waypoint screen in its first mode (autoplot_command.cpp), so anywhere else every level would come out FAILED
		// with a new .bak (Vasquez's review, SHOULD 4)
		said = "start it from the Waypoint screen in its first mode, where the plot runs";
	}

	return said;
}

bool host::AllStockMaps_Start(host::AutoPlotVolumeCounts const &counts, bool overwriteLinked, bool changesAllowed, std::string *why)
{
	std::string const said = AllStockMaps_WhyNot(changesAllowed);

	if (!said.empty())
	{
		if (why)
		{
			*why = said;
		}

		return false;
	}

	run = Run();
	run.names = LevelNames();
	run.counts = counts;
	run.overwriteLinked = overwriteLinked;
	run.before = openLevelName;
	// A LEVEL OPENED WITH "OPEN WITH" RUNS TOO (the owner's first real test, 2026-11-23: refused here until then, on Vasquez's review SHOULD 5).
	// That review's two points stand, so such a level is never opened again at the end: by its name it would be another file (the editor's
	// copy in avp_rifs\OpenedByEditor, not his), and its Save would no longer know where it came from. The report says so instead, naming
	// the file. Nothing of his is touched: a level is closed with nothing to save (or his answer to "Save your level first?"), and every
	// level's open clears levelSourcePath (level_reopen.h), so no save of a stock level can go to his file.
	run.beforeOpenedWith = !levelSourcePath.empty();
	run.beforeFile = run.beforeOpenedWith ? FileNameOf(levelSourcePath) : std::string();
	// THE OWNER'S CAMERA (all_stock_maps.h): its place from the world's origin and its turn, look and roll, as they are now
	run.cameraKept = theCamera != nullptr;

	if (theCamera)
	{
		run.camera = *theCamera;
	}

	for (std::string const &name : run.names)
	{
		host::AllStockMapsLevel level;

		level.file = name + ".RIF";
		run.report.push_back(level);
	}

	run.step = run.names.empty() ? Step::ReopenBefore : Step::Check;
	return true;
}

void host::AllStockMaps_Frame()
{
	if (run.step == Step::Idle || !OnWaypointScreen())
	{
		return;   // the run moves only on the Waypoint screen, where the plot moves
	}

	DWORD const now = GetTickCount();

	if (run.step == Step::Plotting && run.lastFrameAt)
	{
		DWORD const took = now - run.lastFrameAt;

		run.worstFrameMs = took > run.worstFrameMs ? took : run.worstFrameMs;
	}

	run.lastFrameAt = now;

	std::string const name = run.at < run.names.size() ? run.names[run.at] : std::string();
	std::string const path = PathOf(name);

	switch (run.step)
	{
		case Step::Check:
		{
			run.stage = "checking";
			run.worstFrameMs = 0;
			std::string const problem = ReadLevel(path, &run.bytes, &run.sha256);

			if (!problem.empty())
			{
				EndLevel(host::StockMapOutcome::Skipped, problem);
				break;
			}

			run.step = Step::Backup;
			break;
		}

		case Step::Backup:
		{
			run.stage = "backing up";
			std::string const written = WriteBackup(path, run.bytes);

			if (written == "kept")
			{
				Here().backupKept = true;
			}
			else if (!written.empty())
			{
				EndLevel(host::StockMapOutcome::Refused, "the backup " + name + ".RIF.bak couldn't be written: " + written);
				break;
			}
			else
			{
				Here().backupMade = true;
			}

			run.step = Step::Open;
			break;
		}

		case Step::Open:
		{
			run.stage = "opening";
			std::string said;
			std::string const log = LoaderLogPath(name);
			bool const logThere = GetFileAttributesW(Wide(log).c_str()) != INVALID_FILE_ATTRIBUTES;
			// a log there before: its bytes kept, to be put back if the open writes over them (PutBackOldLoaderLog); one that can't be read as a plain file
			// isn't kept, and the report says so
			std::vector<char> oldLog;
			std::string oldLogSha;
			std::string const notKept = logThere ? ReadLevel(log, &oldLog, &oldLogSha) : std::string();
			bool const opened = LevelReopen_Open(name.c_str(), &said);
			std::string const moved = !logThere ? TidyLoaderLog(log, name) : notKept.empty() ? PutBackOldLoaderLog(log, name, oldLog, oldLogSha) :
				log + " (there before the run and not kept: " + notKept + "; the loader may have written over it)";

			run.closedBefore = true;   // the open closed the level before, whether this one opened or not
			PutTheCameraBack();

			Here().loaderLog = moved;

			if (!opened)
			{
				EndLevel(host::StockMapOutcome::Failed, "the level couldn't be opened: " + said + (moved.empty() ? std::string() : "; the loader's log is " + moved));
				break;
			}

			run.step = Step::StartPlot;
			break;
		}

		case Step::StartPlot:
		{
			run.stage = "plotting";
			bool const started = run.overwriteLinked ? AutoPlotCommand_RunAnswered({}, run.counts) : AutoPlotCommand_Run({}, run.counts, false);

			if (!started)
			{
				EndLevel(host::StockMapOutcome::Failed, "the plot did not start");
				break;
			}

			run.step = Step::Plotting;
			break;
		}

		case Step::Plotting:
		{
			if (AutoPlotCommand_Busy())
			{
				// the stage: the plot's start message while it has one; in its search, its own last progress words ("Auto-plot: module 69 of 120 (CAVERNH-02),
				// way 860 of 1418: searching", said last frame under the run's lead: status_words.h), so the line the screen draws shows them (until the
				// alpha.6 cut it said only "plotting" there, for most of each level's time); "plotting" only before the plot has said anything
				std::string const message = AutoPlotCommand_Message();
				std::string const said = statusWords.LedWords();

				run.stage = !message.empty() ? message : !said.empty() ? said : std::string("plotting");
				break;
			}

			AutoPlotResult const result = AutoPlotCommand_LastResult();

			if (!result.ended)
			{
				EndLevel(host::StockMapOutcome::Failed, "the plot ended without a result (it refused at once): nothing saved");
				break;
			}

			if (!result.plotted)
			{
				EndLevel(host::StockMapOutcome::Failed, "auto waypoint said " + result.why + ": nothing saved");
				break;
			}

			Here().why = std::to_string((unsigned long long)result.written) + " AI modules written, " + std::to_string((unsigned long long)result.refused) +
				" refused, " + std::to_string((unsigned long long)result.kept) + " kept";

			if (result.written == 0)
			{
				EndLevel(host::StockMapOutcome::Skipped, Here().why + ": nothing written, so the file is left as it was");
				break;
			}

			run.step = Step::Recheck;
			break;
		}

		case Step::Recheck:
		{
			run.stage = "checking again";
			std::vector<char> again;
			std::string sha;
			std::string const problem = ReadLevel(path, &again, &sha);

			if (!problem.empty() || sha != run.sha256)
			{
				EndLevel(host::StockMapOutcome::Refused, "changed on disk while it was open" + (problem.empty() ? std::string() : " (" + problem + ")") + ": not saved");
				break;
			}

			run.step = Step::Save;
			break;
		}

		case Step::Save:
		{
			run.stage = "saving";
			std::string said;
			host::LevelSaveOver const saved = editedLevel ? host::LevelSave_Over(editedLevel, path.c_str(), false, nullptr, &said) : host::LevelSaveOver::BadTarget;

			if (saved != host::LevelSaveOver::Saved)
			{
				EndLevel(host::StockMapOutcome::Refused, std::string("the save refused it: ") + host::LevelSaveOverWords(saved) + (said.empty() ? std::string() : ": " + said));
				break;
			}

			EditorSaveSitting().MarkSaved(path.c_str());
			EndLevel(host::StockMapOutcome::Changed, Here().why);
			break;
		}

		case Step::Next:
		{
			run.bytes.clear();
			run.at++;
			run.step = run.stopAsked || run.at >= run.names.size() ? Step::ReopenBefore : Step::Check;
			break;
		}

		case Step::ReopenBefore:
		{
			run.stage = "opening the level before";
			std::string said;

			bool reopened = true;
			bool const notReopened = run.beforeOpenedWith && run.closedBefore;   // AllStockMaps_Start says why

			if (!run.beforeOpenedWith && !run.before.empty() && openLevelName != run.before)
			{
				reopened = LevelReopen_Open(run.before.c_str(), &said);
			}

			PutTheCameraBack();   // the camera as it was when the run began
			run.step = Step::Idle;
			// said, never silent (Vasquez's review, SHOULD 5): the editor may end holding no level, or another one
			run.stage = notReopened ? "your level " + (run.beforeFile.empty() ? std::string("open before") : run.beforeFile) +
				" was opened with Open with, so it was not opened again: double-click it to open it again" :
				reopened ? "done" : "done, but the level open before, " + run.before + ", could not be opened again: " + said;
			break;
		}

		default:
			break;
	}
}

bool host::AllStockMaps_Busy()
{
	return run.step != Step::Idle;
}

void host::AllStockMaps_Stop()
{
	run.stopAsked = true;

	// between levels it stops at once; during a level, that level ends whole first (Next looks at stopAsked)
	if (run.step == Step::Check)
	{
		run.step = Step::ReopenBefore;
	}
}

host::AllStockMapsProgress host::AllStockMaps_Progress()
{
	host::AllStockMapsProgress progress;

	// opening the level before (the run's end, or a stop between levels): no level is being worked, so it says how many were reached and names
	// none, never the first one not started (Drake's find: "level 8 of 22, xxx.RIF: opening the level before" read as if level 8 ran)
	bool const ending = run.step == Step::ReopenBefore;

	progress.level = run.step == Step::Idle ? 0 : (ending ? run.at : run.at + 1);
	progress.levels = run.names.size();
	progress.file = !ending && run.at < run.report.size() ? run.report[run.at].file : std::string();
	progress.stage = run.stage;
	progress.stopping = run.stopAsked;
	return progress;
}

std::vector<host::AllStockMapsLevel> host::AllStockMaps_Report()
{
	return run.report;
}

std::string host::AllStockMaps_StateWords()
{
	host::AllStockMapsProgress const p = AllStockMaps_Progress();
	std::string last = "none";

	for (size_t i = 0; i < run.report.size() && i < run.at; i++)
	{
		static char const *const words[] = { "changed", "skipped", "refused", "failed", "notstarted" };

		last = run.report[i].file + " " + words[(int)run.report[i].outcome];
	}

	return std::string(AllStockMaps_Busy() ? "busy" : "idle") + "; level " + std::to_string((unsigned long long)p.level) + " of " +
		std::to_string((unsigned long long)p.levels) + "; " + (p.file.empty() ? "-" : p.file) + "; " + (p.stage.empty() ? "-" : p.stage) + "; last " + last +
		"; worst frame " + std::to_string((unsigned long)run.worstFrameMs) + " ms" + (p.stopping ? "; stopping" : "");
}
