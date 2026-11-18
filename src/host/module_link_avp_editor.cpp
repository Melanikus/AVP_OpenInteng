// module_link_avp_editor.cpp: LINK WITH AVP's editor side (module_link_avp.h says what inteng does and what ours does): the switch, the program losing and getting the focus, the
// harness's fields. Kept out of editor_main.cpp, which is at the size Snyk Code reads (editor_shared.h): it only calls these. Written 2026-11-18 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <windows.h>

#include <string>
#include <vector>

#include "editor_shared.h"
#include "level_objects.h"
#include "level_view.h"
#include "module_link_avp.h"
#include "object_edit.h"
#include "state_line.h"

static_assert(host::kLinkAvpMostPlace == kFlyCameraMostPlace, "module.bbb's place is held to the camera's own limit");

namespace
{
	bool linkWithAvp = false;       // DAT_0070436c
	std::string lastAaa = "none";   // the harness's linkavpaaa: the last module.aaa written, or why not
	std::string lastBbb = "none";   // linkavpbbb: the last module.bbb found, taken or why not (a focus with no file leaves it as it was)

	// <the game's folder>\avp_rifs\<name>: where the game's own "avp_rifs/module.aaa" is (projload.cpp:3208 / 3215: the game's working folder); the editor's data folder is the game's
	// folder (theGameFolder). inteng names the files with no folder ("module.aaa"): its working folder.
	std::string InTheGamesRifs(char const *name)
	{
		std::string folder = theGameFolder;

		if (!folder.empty() && folder.back() != '\\' && folder.back() != '/')
		{
			folder += '\\';
		}

		return folder + "avp_rifs\\" + name;
	}

	// A folder or a link (a reparse point) at that name: never written over, read or deleted (a plain file, or none, is)
	bool NotAPlainFile(std::string const &path)
	{
		DWORD const kind = GetFileAttributesA(path.c_str());

		return kind != INVALID_FILE_ATTRIBUTES && (kind & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) != 0;
	}

	void NotWritten(std::string const &why)
	{
		lastAaa = "not written: " + why;
		Answer("Link with AVP: module.aaa not written: " + why);   // bug (2): inteng wrote to a file it never made, without a word
	}

	// FUN_004c0f00: the program lost the focus
	void WriteForTheGame()
	{
		OpenLevelFile *const open = LevelObjects_OpenForEdit();
		char opened[MAX_PATH] = "";

		if (!open || !LevelView_FilePath(opened, sizeof(opened)))
		{
			lastAaa = "not written: no level is open";
			return;
		}

		// 0x4c0f08-0x4c0f39: the current module's tree goes into the level first, when it changed (FUN_004c10c0); WriteTheLinks says why when it can't, and then module.aaa would
		// carry the links from before, so it is not written
		if (!WriteTheLinks())
		{
			lastAaa = "not written: the current module's links could not be put in the level";
			return;
		}

		// 0x4c0fa8-0x4c107e: every object of the level in the file's order whose (first) MODULEDT holds a (first) VMDARRAY, as the snapshots read them (object_edit.h)
		std::vector<host::ModuleSnapshot> snapshots;
		host::ObjectEditResult const read = host::ReadModuleSnapshots(open, &snapshots);

		if (read != host::ObjectEditResult::Done)
		{
			NotWritten(host::ObjectEditWords(read));
			return;
		}

		std::vector<host::LinkAvpModule> modules;

		for (host::ModuleSnapshot const &snapshot : snapshots)
		{
			if (!snapshot.module || !snapshot.hasLinks)
			{
				continue;
			}

			host::LinkAvpModule one;

			one.index = snapshot.index;

			for (host::ModuleLinkItem const &item : snapshot.links)
			{
				one.items.push_back({ item.objectIndex, item.branchNo });
			}

			modules.push_back(one);
		}

		std::vector<uint8_t> bytes;
		std::string why;

		if (!host::LinkAvp_AaaBytes(opened, modules, &bytes, &why))
		{
			NotWritten(why);
			return;
		}

		if (bytes.size() > 0x7fffffffu)
		{
			NotWritten("the links are too many for one file");
			return;
		}

		std::string const path = InTheGamesRifs("module.aaa");

		if (NotAPlainFile(path))
		{
			NotWritten(path + " is a folder or a link");
			return;
		}

		// as inteng opens it (0x4c0f5b): for writing, not shared (the game's read waits for the whole file), made new
		HANDLE const file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_FLAG_RANDOM_ACCESS, nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			NotWritten(path + " can't be made");
			return;
		}

		DWORD wrote = 0;
		bool const whole = WriteFile(file, bytes.data(), (DWORD)bytes.size(), &wrote, nullptr) && wrote == bytes.size();

		CloseHandle(file);

		if (!whole)
		{
			// a part of the file would be read by the game as links: none is left
			DeleteFileA(path.c_str());
			NotWritten(path + " could not be written whole");
			return;
		}

		lastAaa = "wrote " + std::to_string((unsigned long long)bytes.size()) + " bytes, " + std::to_string((unsigned long long)modules.size()) + " modules";
	}

	void NotTaken(std::string const &why)
	{
		lastBbb = "refused: " + why;
		Answer("Link with AVP: the game's camera (module.bbb) not taken: " + why);
	}

	// FUN_004c19a0: the program got the focus back
	void TakeTheGamesCamera()
	{
		std::string const path = InTheGamesRifs("module.bbb");

		if (NotAPlainFile(path))
		{
			NotTaken(path + " is a folder or a link");
			return;
		}

		// 0x4c19b8: for reading, not shared, only when it is there (none: nothing, as inteng)
		HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			return;
		}

		// HOSTILE INPUT (module_link_avp.h bug (3)): its size first; at most 48 bytes are read, so a big file costs nothing
		LARGE_INTEGER size = {};
		bool const sized = GetFileSizeEx(file, &size) != 0;
		uint8_t bytes[host::kLinkAvpCameraBytes] = {};
		DWORD got = 0;

		if (sized && size.QuadPart == (LONGLONG)host::kLinkAvpCameraBytes && !ReadFile(file, bytes, (DWORD)sizeof(bytes), &got, nullptr))
		{
			got = 0;
		}

		CloseHandle(file);

		// 0x4c1a31: inteng deletes the file it read, whatever was in it
		DeleteFileA(path.c_str());

		// a size that is not 48 is refused by its size alone (nothing of it was read); 48 is refused unless all 48 were read
		size_t const length = !sized ? 0 : size.QuadPart != (LONGLONG)host::kLinkAvpCameraBytes ? (size_t)(size.QuadPart > 0x7fffffff ? 0x7fffffff : size.QuadPart) : (size_t)got;
		host::LinkAvpCamera camera;
		std::string why;

		if (!host::LinkAvp_ReadCamera(bytes, length, &camera, &why))
		{
			NotTaken(why);
			return;
		}

		if (!theCamera)
		{
			NotTaken("no level is open");
			return;
		}

		// 0x4c1a41-0x4c1a79: the view takes the game's place and turn (ours in mm and the game's own matrix form: module_link_avp.h, OURS)
		FlyCamera taken = *theCamera;

		taken.x = camera.place[0];
		taken.y = camera.place[1];
		taken.z = camera.place[2];
		LevelView_AnglesOfMatrix(camera.matrix, &taken.turn, &taken.look, &taken.roll);
		FlyCamera_Hold(&taken);
		*theCamera = taken;
		lastBbb = "taken: " + std::to_string(taken.x) + " " + std::to_string(taken.y) + " " + std::to_string(taken.z) + " " + std::to_string(taken.turn) + " " + std::to_string(taken.look)
			+ " " + std::to_string(taken.roll);
	}
}

namespace host
{
	bool ModuleLinkAvp_On()
	{
		return linkWithAvp;
	}

	void ModuleLinkAvp_Toggle()
	{
		if (AutoLinkRunning() || DeleteLinksRunning())
		{
			Answer("AutoLink is running: right click to cancel");
			return;
		}

		linkWithAvp = !linkWithAvp;   // 0x4c36c4-0x4c36d1; the tick is drawn from it (editor_panels.cpp)
	}

	void ModuleLinkAvp_Off()
	{
		linkWithAvp = false;
	}

	void ModuleLinkAvp_AppActive(bool active)
	{
		if (!linkWithAvp)
		{
			return;
		}

		// while AutoLink or Delete Links runs (inteng holds the program; ours runs it on a worker thread), the trees are the run's: nothing is read or written
		if (AutoLinkRunning() || DeleteLinksRunning())
		{
			(active ? lastBbb : lastAaa) = "skipped: AutoLink is running";
			return;
		}

		if (active)
		{
			TakeTheGamesCamera();
		}
		else
		{
			WriteForTheGame();
		}
	}

	void ModuleLinkAvp_StateLine()
	{
		StateLine_Add("linkavp", linkWithAvp ? "on" : "off");
		StateLine_Add("linkavpaaa", lastAaa);
		StateLine_Add("linkavpbbb", lastBbb);
	}
}
