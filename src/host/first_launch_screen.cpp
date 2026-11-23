// first_launch_screen.cpp: the first-launch setup's Windows part at the editor's start (first_launch_screen.h says what and why). Written 2026-11-23 by Melanikus
// (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "first_launch_screen.h"

#include <windows.h>
#include <shobjidl.h>   // IFileOpenDialog: Windows' own folder picker
#include <stdio.h>

#include <string>
#include <vector>

#include "first_launch_ui.h"
#include "nobody_watching.h"
#include "panel_paint.h"   // PanelPaint_SafeText: the editor's one rule for safe text
#include "save_folder.h"   // SaveFolder_WhyNotUsable: the data folder's own rule

namespace
{
	size_t const kSettingsMostBytes = 64 * 1024;   // AVP_OpenInteng.ini is read whole before its one line is written: the zip's own is under 2 KB

	// The words to the error output too, a line each, so a hidden run's harness and a console read them ("first launch: ...")
	void Say(std::string const &words)
	{
		std::string line;

		for (char const letter : words)
		{
			line += letter == '\n' ? ' ' : letter;
		}

		fprintf(stderr, "AVP_OpenInteng: first launch: %s\n", line.c_str());
		fflush(stderr);
	}

	std::string WithoutLastSlash(std::string folder)
	{
		while (folder.size() > 3 && (folder.back() == '\\' || folder.back() == '/'))
		{
			folder.pop_back();
		}

		return folder;
	}

	// The folder above: "D:\Downloads\AVP_OpenInteng" -> "D:\Downloads"; a drive's own folder ("C:\") has none
	std::string Above(std::string const &folder)
	{
		size_t const cut = folder.find_last_of("\\/");

		if (cut == std::string::npos || cut < 2)
		{
			return std::string();
		}

		return cut == 2 ? folder.substr(0, 3) : folder.substr(0, cut);
	}

	// WINDOWS' OWN FOLDER PICKER (IFileOpenDialog, folders only, file system folders only). True with *picked; false with *why empty when the picker was cancelled,
	// else why the folder can't be taken. The name comes back in Windows' wide letters; the editor works in the system's code page, so a name that doesn't turn
	// into it exactly (WC_NO_BEST_FIT_CHARS: no letter swapped for a look-alike) is refused rather than changed
	bool PickFolder(std::string *picked, std::string *why)
	{
		why->clear();

		HRESULT const started = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		bool const stop = SUCCEEDED(started);   // only a start of ours is stopped
		IFileOpenDialog *dialog = nullptr;
		bool got = false;

		if (SUCCEEDED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog))))
		{
			FILEOPENDIALOGOPTIONS options = 0;

			dialog->GetOptions(&options);
			dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR);
			dialog->SetTitle(L"Your game's folder: the one that holds avp_rifs");

			IShellItem *item = nullptr;
			PWSTR name = nullptr;

			if (SUCCEEDED(dialog->Show(nullptr)) && SUCCEEDED(dialog->GetResult(&item)) && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &name)))
			{
				char narrow[MAX_PATH];
				BOOL changed = FALSE;
				int const made = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, name, -1, narrow, (int)sizeof(narrow), nullptr, &changed);

				if (made > 0 && !changed)
				{
					*picked = narrow;
					got = true;
				}
				else
				{
					*why = made > 0 ? "its name has letters the editor can't use (outside this computer's code page)" : "its name is too long";
				}
			}

			if (name)
			{
				CoTaskMemFree(name);
			}

			if (item)
			{
				item->Release();
			}

			dialog->Release();
		}
		else
		{
			*why = "Windows' folder picker couldn't be opened";
		}

		if (stop)
		{
			CoUninitialize();
		}

		return got;
	}

	// The data folder's checks, before anything is written: the same as the start makes of a written DataFolder= (editor_main.cpp)
	char const *WhyNotTheGame(std::string const &folder, std::string *checked)
	{
		char usable[MAX_PATH];

		if (char const *const why = SaveFolder_WhyNotUsable(folder.c_str(), usable, sizeof(usable)))
		{
			return why;
		}

		std::string const rifs = std::string(usable) + "\\avp_rifs";
		DWORD const kind = GetFileAttributesA(rifs.c_str());

		if (kind == INVALID_FILE_ATTRIBUTES || !(kind & FILE_ATTRIBUTE_DIRECTORY))
		{
			return "it has no avp_rifs folder in it, so it isn't the game's folder";
		}

		if (kind & FILE_ATTRIBUTE_REPARSE_POINT)
		{
			return "its avp_rifs folder is a link, or another reparse point, not a plain folder";
		}

		*checked = usable;
		return nullptr;
	}

	// DataFolder=<folder> into AVP_OpenInteng.ini, every other byte kept: read whole (at most kSettingsMostBytes, a plain file, never a link), the line put in
	// (first_launch_ui.h), written to "<ini>.writing" made new (never through a file or link already there), then put in the ini's place by Windows in one step
	bool WriteDataFolder(char const *settingsFile, std::string const &folder, std::string *why)
	{
		std::string text;
		DWORD const kind = GetFileAttributesA(settingsFile);
		bool const there = kind != INVALID_FILE_ATTRIBUTES;

		if (there && (kind & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)))
		{
			*why = "it isn't a plain file (a folder, a link or another reparse point)";
			return false;
		}

		if (there)
		{
			// judged on its handle, its link never followed (as Save judges the level it writes over, F-a): a plain file with one name only
			HANDLE const file = CreateFileA(settingsFile, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT,
				nullptr);
			BY_HANDLE_FILE_INFORMATION about = {};
			LARGE_INTEGER size = {};

			if (file != INVALID_HANDLE_VALUE && (!GetFileInformationByHandle(file, &about) || (about.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY |
				FILE_ATTRIBUTE_REPARSE_POINT)) || about.nNumberOfLinks != 1))
			{
				CloseHandle(file);
				*why = "it isn't a plain file with one name (a link, a folder, or one name of a file with several)";
				return false;
			}

			if (file == INVALID_HANDLE_VALUE || !GetFileSizeEx(file, &size) || size.QuadPart < 0 || (unsigned long long)size.QuadPart > kSettingsMostBytes)
			{
				if (file != INVALID_HANDLE_VALUE)
				{
					CloseHandle(file);
				}

				*why = "it can't be read, or it is larger than 64 KB";
				return false;
			}

			text.resize((size_t)size.QuadPart);
			DWORD read = 0;
			bool const whole = text.empty() || (ReadFile(file, &text[0], (DWORD)text.size(), &read, nullptr) && read == text.size());

			CloseHandle(file);

			if (!whole)
			{
				*why = "it couldn't be read whole";
				return false;
			}
		}

		std::string written;

		if (!host::FirstLaunch_IniWithLine(text, "Editor", "DataFolder", folder, &written))
		{
			*why = "the folder's name can't be written as one line";
			return false;
		}

		std::string const temporary = std::string(settingsFile) + ".writing";
		HANDLE out = CreateFileA(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);

		if (out == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_EXISTS && DeleteFileA(temporary.c_str()))   // a leftover of ours (a link itself is deleted, never its target)
		{
			out = CreateFileA(temporary.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
		}

		if (out == INVALID_HANDLE_VALUE)
		{
			*why = "its new copy couldn't be made beside it";
			return false;
		}

		DWORD wrote = 0;
		bool const all = WriteFile(out, written.data(), (DWORD)written.size(), &wrote, nullptr) && wrote == written.size() && FlushFileBuffers(out);

		CloseHandle(out);

		bool const placed = all && (there ? ReplaceFileA(settingsFile, temporary.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr) != FALSE :
			MoveFileExA(temporary.c_str(), settingsFile, MOVEFILE_WRITE_THROUGH) != FALSE);

		if (!placed)
		{
			DeleteFileA(temporary.c_str());
			*why = all ? "Windows wouldn't put the new copy in its place" : "its new copy couldn't be written";
			return false;
		}

		return true;
	}
}

bool host::FirstLaunch_NotFound(char const *programFolder, char const *settingsFile, std::string *dataFolder)
{
	FirstLaunchFacts facts;

	facts.programFolder = WithoutLastSlash(programFolder ? programFolder : "");

	std::string const above = Above(facts.programFolder);

	if (!above.empty())
	{
		facts.looked.push_back(above);
	}

	std::string const words = FirstLaunch_Words(facts);

	Say(words);

	if (NobodyWatching() || !dataFolder || !settingsFile)
	{
		Say("Not now (nobody at the screen)");
		return false;
	}

	std::string ask = words;

	for (;;)
	{
		if (MessageBoxA(NULL, ask.c_str(), kFirstLaunchTitle, MB_YESNO | MB_ICONINFORMATION | MB_SETFOREGROUND) != IDYES)
		{
			Say("Not now");
			return false;
		}

		std::string picked;
		std::string why;
		std::string checked;

		if (!PickFolder(&picked, &why))
		{
			ask = why.empty() ? words : "That folder can't be used: " + why + ". Nothing was written.\n\nPick another folder?";
			continue;
		}

		if (char const *const whyNot = WhyNotTheGame(picked, &checked))
		{
			ask = "That folder can't be used: " + std::string(whyNot) + ". Nothing was written.\n\n" + PanelPaint_SafeText(picked) + "\n\nPick another folder?";
			Say(ask);
			continue;
		}

		if (!WriteDataFolder(settingsFile, checked, &why))
		{
			ask = "That folder is right, but AVP_OpenInteng.ini couldn't be written: " + why + ". Nothing was written.\n\nPick a folder again?";
			Say(ask);
			continue;
		}

		Say("DataFolder=" + PanelPaint_SafeText(checked) + " written in AVP_OpenInteng.ini");
		*dataFolder = checked;
		return true;
	}
}
