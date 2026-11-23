// shape_extract_files.cpp: Option C on the disk (shape_extract_files.h says what and why). Written 2026-11-23 by Melanikus (Dietrich, the read) for
// AVP_OpenInteng, and by Melanikus (Frost, the writer). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "shape_extract_files.h"

#include <stdint.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <new>
#include <set>
#include <vector>

#include <windows.h>

#include "shape_extract.h"   // ShapeExtract_Sha256, ShapeExtract_ScanMap, ShapeExtract_ConflictName
#include "shape_identity.h"  // what a written shape is, for shapes.txt
#include "shape_import.h"    // ShapeImport_Build: each shape read back by the import before it is written

namespace host
{
	namespace
	{
		// the path as Windows' own wide letters, from the code page the editor's paths are in
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

		struct Closer
		{
			HANDLE h;
			~Closer()
			{
				if (h != INVALID_HANDLE_VALUE)
				{
					CloseHandle(h);
				}
			}
		};

		std::string WindowsError(DWORD error)
		{
			return "Windows error " + std::to_string((unsigned long)error);
		}

		// THE TARGET FOLDER: made if it isn't there (one folder, in a folder that is), then opened and judged on its OWN HANDLE: a folder on a disk, not a link or
		// a junction, and never the old tools' package (<game>\Tools\shape_rifs: the owner's, read only). The package is told by the folder's own final path,
		// as Windows gives it from the handle, so no short 8.3 name ("TOOLS~1\SHAPE_~1") and no trailing dot can stand in for it (Vasquez's review, 11-23).
		// The handle is KEPT OPEN for the whole map, sharing no deletion, so the folder can't be renamed or swapped for a junction while its shapes are written.
		// "" when it may be written in (*held is then the open handle, the caller's to close).
		std::string FolderOpen(std::string const &folder, HANDLE *held)
		{
			*held = INVALID_HANDLE_VALUE;
			std::wstring const wide = Wide(folder);

			if (GetFileAttributesW(wide.c_str()) == INVALID_FILE_ATTRIBUTES && !CreateDirectoryW(wide.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
			{
				return "it can't be made (" + WindowsError(GetLastError()) + ")";
			}

			HANDLE const handle = CreateFileW(wide.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
				FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);

			if (handle == INVALID_HANDLE_VALUE)
			{
				return "it can't be opened (" + WindowsError(GetLastError()) + ")";
			}

			BY_HANDLE_FILE_INFORMATION info;

			if (GetFileType(handle) != FILE_TYPE_DISK || !GetFileInformationByHandle(handle, &info) || !(info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ||
				(info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
			{
				CloseHandle(handle);
				return "it is a link or a junction, or not a folder on a disk";
			}

			std::vector<wchar_t> path(32768, L'\0');
			DWORD const length = GetFinalPathNameByHandleW(handle, path.data(), (DWORD)path.size(), FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);

			if (length == 0 || length >= path.size())
			{
				CloseHandle(handle);
				return "its own path can't be read from it (" + WindowsError(GetLastError()) + ")";
			}

			std::wstring final(path.data(), length);

			for (wchar_t &c : final)
			{
				c = (c >= L'A' && c <= L'Z') ? (wchar_t)(c - L'A' + L'a') : (c == L'/' ? L'\\' : c);
			}

			while (!final.empty() && final.back() == L'\\')
			{
				final.pop_back();
			}

			std::wstring const package = L"\\tools\\shape_rifs";

			if (final.size() >= package.size() && final.compare(final.size() - package.size(), package.size(), package) == 0)
			{
				CloseHandle(handle);
				return "it is the old tools' package folder (Tools\\shape_rifs), which is never written";
			}

			*held = handle;
			return std::string();
		}

		std::string LowerName(std::string name)
		{
			for (char &c : name)
			{
				c = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
			}

			return name;
		}

		// THE FOLDER, LISTED ONCE A MAP (Vasquez's MUST, 11-23: each shape listed the folder and read the files there again, about K^2/2 opens for a map's K
		// shapes, each file read whole): every name in it, in small letters, with its size and whether it is a plain file (not a folder or a link), and the
		// SHA-256 of each one read so far, so a file is read at most once a map, and only when its size is the shape's. The names this map writes join it.
		struct Entry
		{
			std::string name;        // as the folder holds it
			uint64_t size = 0;
			bool plain = false;      // a file, not a folder or a reparse point
			bool read = false;       // read this map already (its sha256 then known, "" when it couldn't be read)
			std::string sha256;
			std::vector<char> bytes; // kept only when its sha256 is one of this map's shapes' (Listing::wanted), for the byte compare without a second read
			std::vector<char> const *own = nullptr;   // a file this map wrote: its bytes ARE the map's own shape's, pointed at, not copied (Vasquez, 11-23)
		};

		struct Listing
		{
			std::map<std::string, Entry> byLowerName;
			std::multimap<uint64_t, std::string> bySize;   // plain files' sizes -> their small-letter names
			size_t reads = 0;                              // files of the folder read this map (the report's filesRead)
			std::set<std::string> wanted;                  // this map's shapes' SHA-256s: a file with one of them keeps its bytes
		};

		size_t const kMostFolderNames = 100000;   // a shape folder holding more than this is refused: stock makes 343, and the list is read at every map

		void AddEntry(Listing *listing, Entry const &entry)
		{
			std::string const lower = LowerName(entry.name);

			if (listing->byLowerName.count(lower))
			{
				return;
			}

			listing->byLowerName[lower] = entry;

			if (entry.plain)
			{
				listing->bySize.insert({ entry.size, lower });
			}
		}

		std::string ListFolder(std::string const &folder, Listing *listing)
		{
			WIN32_FIND_DATAW found;
			HANDLE const search = FindFirstFileExW(Wide(folder + "\\*").c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, 0);

			if (search == INVALID_HANDLE_VALUE)
			{
				DWORD const error = GetLastError();
				return error == ERROR_FILE_NOT_FOUND ? std::string() : "it can't be listed (" + WindowsError(error) + ")";
			}

			std::string why;

			do
			{
				// (WC_NO_BEST_FIT_CHARS: a letter the code page lacks is never "best fitted" onto another, so one name can't take another's place in the
				// listing; such a name is listed as taken, not as a plain file: Vasquez's review, 11-23, as StockCheck's RifNames does)
				BOOL unsaid = FALSE;
				int const length = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, found.cFileName, -1, nullptr, 0, nullptr, &unsaid);
				std::string name(length > 1 ? (size_t)length - 1 : 0, '\0');

				if (length > 1)
				{
					WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, found.cFileName, -1, &name[0], length, nullptr, &unsaid);
				}

				if (name.empty() || name == "." || name == "..")
				{
					continue;
				}

				if (listing->byLowerName.size() >= kMostFolderNames)
				{
					why = "it holds more than " + std::to_string(kMostFolderNames) + " names";
					break;
				}

				Entry entry;
				entry.name = name;
				entry.size = ((uint64_t)found.nFileSizeHigh << 32) | found.nFileSizeLow;
				entry.plain = !unsaid && !(found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT));
				AddEntry(listing, entry);
			} while (FindNextFileW(search, &found));

			FindClose(search);
			return why;
		}

		// Does this entry of the folder hold these very bytes? Only a plain file of the shape's size is read, AT MOST ONCE A MAP (its SHA-256 kept, and its
		// bytes too when that SHA-256 is one of this map's shapes': so the bytes are compared where the SHA-256 is the same, with no second read). The read is
		// the one hostile reader's.
		bool Holds(std::string const &folder, Listing *listing, Entry *entry, std::vector<char> const &bytes, std::string const &sha256)
		{
			if (!entry->plain || entry->size != bytes.size())
			{
				return false;
			}

			if (!entry->read)
			{
				std::vector<char> there;
				std::string sum;
				entry->read = true;
				listing->reads++;
				entry->sha256 = ShapeExtract_ReadMapFile(folder + "\\" + entry->name, &there, &sum).empty() ? sum : std::string();

				if (!entry->sha256.empty() && listing->wanted.count(entry->sha256))
				{
					entry->bytes.swap(there);
				}
			}

			return !entry->sha256.empty() && entry->sha256 == sha256 && (entry->own ? *entry->own : entry->bytes) == bytes;
		}

		// The name of a file already in the folder holding these very bytes, among the names this shape could have, or "": so the same bytes met again in
		// another map aren't written twice under two names. Every name of a shape holds its file's stem (a part's <object>_<file>, the whole file's <file>, a
		// conflict's <name>_<map>) or starts with its own name's; only the folder's files of the shape's size are looked at
		std::string SameBytesThere(std::string const &folder, Listing *listing, std::string const &plain, std::string const &fileOf, std::vector<char> const &bytes,
			std::string const &sha256)
		{
			auto stemOf = [](std::string const &name) { return name.size() > 4 ? name.substr(0, name.size() - 4) : name; };
			std::string const fileStem = stemOf(fileOf), ownStem = stemOf(plain);
			auto const range = listing->bySize.equal_range((uint64_t)bytes.size());

			for (auto it = range.first; it != range.second; ++it)
			{
				std::string const &lower = it->second;
				bool const rif = lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".rif") == 0;
				bool const ours = (!fileStem.empty() && lower.find(fileStem) != std::string::npos) || lower.compare(0, ownStem.size(), ownStem) == 0;
				Entry &entry = listing->byLowerName[lower];

				if (rif && ours && Holds(folder, listing, &entry, bytes, sha256))
				{
					return entry.name;
				}
			}

			return std::string();
		}

		// The written file renamed THROUGH ITS OWN HANDLE to `target` (a whole path), never over a name that is there (Drake's and Vasquez's SHOULD, 11-23: a
		// rename by path, after the close, could give the shape's name to whatever stood under the .part's name by then). FILE_RENAME_INFO has a variable
		// length, so it is a FIXED buffer, the name put in a letter at a time, each place checked against the buffer's own room (as tests/make_junction.h does):
		// no byte count is made from the path. True when renamed; else *error is Windows' (ERROR_ALREADY_EXISTS: the name is taken).
		size_t const kMostRenameLetters = 1024;

		bool RenameThroughHandle(HANDLE file, std::wstring const &target, DWORD *error)
		{
			alignas(FILE_RENAME_INFO) unsigned char buffer[sizeof(FILE_RENAME_INFO) + kMostRenameLetters * sizeof(WCHAR)] = {};
			FILE_RENAME_INFO *const info = reinterpret_cast<FILE_RENAME_INFO *>(buffer);
			WCHAR *const letters = info->FileName;
			size_t const room = (sizeof(buffer) - offsetof(FILE_RENAME_INFO, FileName)) / sizeof(WCHAR);

			if (target.size() + 1 > room)
			{
				*error = ERROR_FILENAME_EXCED_RANGE;
				return false;
			}

			for (size_t i = 0; i < target.size() && i + 1 < room; i++)
			{
				letters[i] = target[i];
			}

			letters[target.size()] = 0;
			info->ReplaceIfExists = FALSE;
			info->RootDirectory = nullptr;
			info->FileNameLength = (DWORD)(target.size() * sizeof(WCHAR));

			if (!SetFileInformationByHandle(file, FileRenameInfo, info, (DWORD)sizeof(buffer)))
			{
				*error = GetLastError();
				return false;
			}

			return true;
		}

		// Bytes written as a NEW file, WHOLE OR NOT AT ALL (Drake's question, 11-23: an editor killed part way, by taskkill or a power cut, must never leave a
		// half shape under a shape's name, for the import to meet later). They go first into a file of a name no reader looks at ("~<process>-<n>.part": not a
		// .rif, and short, so a folder whose path fits the .rif fits it too; CREATE_NEW, never through a link: FILE_FLAG_OPEN_REPARSE_POINT), are flushed to
		// the disk, and only then is that very file, through its own handle, renamed to the shape's name, a rename that never replaces: so <name>.rif is either
		// the whole shape or not there, and no name is used by its path after the write. A kill before the rename leaves only a .part, which nothing reads (a
		// player who finds one may delete it: shapes.txt's heading says so). "" written, "taken" when something of the shape's name is there now, else why. A
		// file that couldn't be written whole, or not named, is deleted through its own handle before it is closed (Vasquez's review, 11-23).
		std::string WriteNew(std::string const &folder, std::string const &name, std::vector<char> const &bytes)
		{
			static unsigned long count = 0;
			std::wstring const target = Wide(folder + "\\" + name);
			HANDLE file = INVALID_HANDLE_VALUE;

			for (int tries = 0; file == INVALID_HANDLE_VALUE && tries < 16; tries++)
			{
				std::wstring const part = Wide(folder + "\\~" + std::to_string((unsigned long)GetCurrentProcessId()) + "-" + std::to_string(++count) + ".part");
				file = CreateFileW(part.c_str(), GENERIC_WRITE | DELETE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);
			}

			if (file == INVALID_HANDLE_VALUE)
			{
				return "a file to write it into can't be made (" + WindowsError(GetLastError()) + ")";
			}

			DWORD put = 0, error = 0;
			bool const whole = WriteFile(file, bytes.data(), (DWORD)bytes.size(), &put, nullptr) && put == bytes.size() && FlushFileBuffers(file);
			bool const named = whole && RenameThroughHandle(file, target, &error);

			if (!named)
			{
				error = whole ? error : GetLastError();
				FILE_DISPOSITION_INFO gone = {};
				gone.DeleteFile = TRUE;
				bool const deleted = SetFileInformationByHandle(file, FileDispositionInfo, &gone, sizeof(gone)) != FALSE;
				CloseHandle(file);
				std::string const why = !whole ? "it could not be written whole" : error == ERROR_ALREADY_EXISTS || error == ERROR_FILE_EXISTS ? "taken" :
					"it could not be given its name (" + WindowsError(error) + ")";
				return deleted || why == "taken" ? why : why + ", and its .part could not be deleted";
			}

			FlushFileBuffers(file);   // (the rename to the disk too, as MOVEFILE_WRITE_THROUGH did; a rename lost to a power cut leaves only the .part)
			CloseHandle(file);
			return std::string();
		}

		// One line added to the folder's shapes.txt (made with its heading if it isn't there), judged on its own handle like any file: a plain file, one name
		std::string AddProvenance(std::string const &folder, std::string const &line)
		{
			std::wstring const path = Wide(folder + "\\shapes.txt");
			HANDLE const file = CreateFileW(path.c_str(), FILE_APPEND_DATA | FILE_READ_ATTRIBUTES, FILE_SHARE_READ, nullptr, OPEN_ALWAYS,
				FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, nullptr);

			if (file == INVALID_HANDLE_VALUE)
			{
				return "shapes.txt can't be opened (" + WindowsError(GetLastError()) + ")";
			}

			bool const made = GetLastError() != ERROR_ALREADY_EXISTS;
			Closer const closer = { file };
			BY_HANDLE_FILE_INFORMATION info;

			if (GetFileType(file) != FILE_TYPE_DISK || !GetFileInformationByHandle(file, &info) ||
				(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) || info.nNumberOfLinks != 1)
			{
				return "shapes.txt is a link, a folder or a second name of another file";
			}

			std::string text = made ? "# shapes.txt: the shape files Open Inteng made from the maps here (Option C). Nothing here is replaced; a name another file\r\n"
				"# already has gets _<map>. One line a file, by tabs: its name, its SHA-256, what it is (from the stock table, or unknown), the evidence,\r\n"
				"# the map it came from, that map's SHA-256, and the name the map gives it. A file ~<number>-<number>.part here is from an editor stopped\r\n"
				"# while writing a shape: it holds no shape, nothing reads it, and it may be deleted.\r\n" : std::string();
			text += line + "\r\n";
			DWORD put = 0;
			return WriteFile(file, text.data(), (DWORD)text.size(), &put, nullptr) && put == text.size() ? std::string() : "shapes.txt could not be written";
		}

		// shapes.txt judged BEFORE any shape is written, so a bad one can't leave a written shape without its line: missing is fine (it is made); there, it
		// must be a plain file on a disk with one name
		std::string ProvenanceWhyNot(std::string const &folder)
		{
			HANDLE const file = CreateFileW(Wide(folder + "\\shapes.txt").c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
				FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);

			if (file == INVALID_HANDLE_VALUE)
			{
				DWORD const error = GetLastError();
				return error == ERROR_FILE_NOT_FOUND ? std::string() : "shapes.txt can't be opened (" + WindowsError(error) + ")";
			}

			Closer const closer = { file };
			BY_HANDLE_FILE_INFORMATION info;

			if (GetFileType(file) != FILE_TYPE_DISK || !GetFileInformationByHandle(file, &info) ||
				(info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) || info.nNumberOfLinks != 1)
			{
				return "shapes.txt is a link, a folder or a second name of another file";
			}

			return std::string();
		}

		// A field of shapes.txt as one plain field: every letter below 32 (a tab, a line end) and 127 made a space, since a name from a stranger's map can
		// hold any byte and the file is split by tabs and lines (Vasquez's review, 11-23)
		std::string OneField(std::string text)
		{
			for (char &c : text)
			{
				c = ((unsigned char)c < 32 || (unsigned char)c == 127) ? ' ' : c;
			}

			return text;
		}

		std::string ProvenanceLine(ShapeExtractShape const &shape, std::string const &name, std::string const &mapName, std::string const &mapSha256)
		{
			ShapeIdentityRow const *const row = ShapeIdentity_Find(shape.sha256);
			std::string const what = !row ? "unknown" : row->kind[0] ? row->kind : "a stock shape no object places";
			std::string const evidence = !row ? "" : std::string(row->evidence) + (row->anchored ? ", anchored on Rebellion's own file" : "");
			return OneField(name) + "\t" + OneField(shape.sha256) + "\t" + OneField(what) + "\t" + OneField(evidence) + "\t" + OneField(mapName) + "\t" +
				OneField(mapSha256) + "\t" + OneField(shape.givenName);
		}
	}

	// THE HOSTILE MAP READ. All stock maps' own reader (all_stock_maps_host.cpp ReadLevel, on bishop-int since 3249427d), as a call of its own: this branch
	// (frost-shapes) doesn't hold All stock maps yet, so it is a COPY for now, with each refusal in words of its own; once both branches meet, ReadLevel
	// calls this one and its body goes, so there is one reader.
	// What it doesn't do: judge the FOLDERS on the way (a junction in a folder above the file is followed by Windows, as for any open): the caller
	// names the folder (the game's avp_rifs, avp_huds, a level the user opened) and checks it as it checks any folder it writes into.
	std::string ShapeExtract_ReadMapFile(std::string const &path, std::vector<char> *bytes, std::string *sha256)
	{
		bytes->clear();
		sha256->clear();

		// opened WITHOUT FOLLOWING A LINK (FILE_FLAG_OPEN_REPARSE_POINT: a junction or a symbolic link is opened as itself, so it is seen and refused below;
		// FILE_FLAG_BACKUP_SEMANTICS lets a folder be opened, so a folder named x.rif is seen too), sharing READ only, so nothing can write the file while it
		// is read: a program that has it open for writing makes this open fail, and none can open it for writing until it is closed
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

		// everything below is judged on THAT HANDLE, never on the name again, so nothing can be swapped in between
		if (GetFileType(file) != FILE_TYPE_DISK)
		{
			return "not a file on a disk (a device or a pipe)";
		}

		BY_HANDLE_FILE_INFORMATION info;

		if (!GetFileInformationByHandle(file, &info))
		{
			return "can't be judged (Windows error " + std::to_string((unsigned long)GetLastError()) + ")";
		}

		if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		{
			return "a link (a junction or a symbolic link), not a plain file: it could lead anywhere";
		}

		if (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
		{
			return "a folder, not a file";
		}

		if (info.nNumberOfLinks != 1)
		{
			return "the file has " + std::to_string((unsigned long)info.nNumberOfLinks) + " names (a hard link): only a file with one name is taken";
		}

		uint64_t const size = ((uint64_t)info.nFileSizeHigh << 32) | info.nFileSizeLow;

		if (size == 0 || size > kShapeExtractMostMapBytes)
		{
			return size == 0 ? "empty" : "larger than 16 MB";
		}

		// read WHOLE, a megabyte at a time; a read that stops early is refused (the size can't change while the handle holds it: see the open)
		bytes->assign((size_t)size, 0);

		size_t got = 0;

		while (got < bytes->size())
		{
			DWORD read = 0;
			DWORD const ask = (DWORD)(bytes->size() - got > 1024 * 1024 ? 1024 * 1024 : bytes->size() - got);

			if (!ReadFile(file, bytes->data() + got, ask, &read, nullptr) || read == 0)
			{
				bytes->clear();
				return "can't be read to its end";
			}

			got += read;
		}

		// the SHA-256 of THOSE VERY BYTES (never the name read again)
		*sha256 = ShapeExtract_Sha256(*bytes);
		return std::string();
	}

	bool ShapeExtract_WriteMapShapes(ShapeExtractMap const &map, std::string const &mapName, std::string const &shapeFolder, ShapeLevelReport *report)
	{
		report->sha256 = map.sha256;

		for (ShapeExtractSkipped const &s : map.skipped)
		{
			report->skipped.push_back(s.givenName + ": " + ShapeExtractWords(s.result));
		}

		if (!map.scanned)
		{
			report->failed = true;
			report->why = map.why;
			return false;
		}

		// the folder: opened, judged and HELD for the whole map (FolderOpen); shapes.txt judged before any write; then listed once
		HANDLE held = INVALID_HANDLE_VALUE;
		std::string folderWhy = FolderOpen(shapeFolder, &held);
		Closer const holding = { held };
		folderWhy = folderWhy.empty() ? ProvenanceWhyNot(shapeFolder) : folderWhy;
		Listing listing;
		folderWhy = folderWhy.empty() ? ListFolder(shapeFolder, &listing) : folderWhy;

		for (ShapeExtractShape const &shape : map.shapes)
		{
			listing.wanted.insert(shape.sha256);
		}

		if (!folderWhy.empty())
		{
			report->failed = true;
			report->why = "the shape folder " + shapeFolder + " isn't used: " + folderWhy;
			return false;
		}

		for (ShapeExtractShape const &shape : map.shapes)
		{
			// read back by the import before it is written: a file the import wouldn't take is never put in the folder
			std::vector<char> imported;
			std::string importWhy;

			if (ShapeImport_Build(shape.file, shape.fileName, 1, &imported, &importWhy) != ShapeImportResult::Done)
			{
				report->skipped.push_back(shape.fileName + ": the import wouldn't take it (" + importWhy + ")");
				continue;
			}

			// its name: the scan's, else (taken there by other bytes) <name>_<map>, then _2 ...; the same bytes there already under one of them: nothing. The
			// names are looked up in the listing (no file opened for a name the folder doesn't hold); a file there is read only if its size is the shape's.
			std::string const plain = shape.conflictOf.empty() ? shape.fileName : shape.conflictOf;
			std::string const same = SameBytesThere(shapeFolder, &listing, plain, shape.fileOf, shape.file, shape.sha256);
			bool done = !same.empty();

			if (done)
			{
				report->alreadyThere.push_back(same);
			}

			for (int n = 0; !done && n <= 999; n++)
			{
				std::string const name = n == 0 ? shape.fileName : ShapeExtract_ConflictName(plain, mapName, n);

				if (n > 0 && name == shape.fileName)
				{
					continue;
				}

				auto const there = listing.byLowerName.find(LowerName(name));

				if (there != listing.byLowerName.end())
				{
					if (Holds(shapeFolder, &listing, &there->second, shape.file, shape.sha256))
					{
						report->alreadyThere.push_back(there->second.name);
						done = true;
					}

					continue;   // (taken by something else: the next name)
				}

				// its shapes.txt line and its listing entry made BEFORE the write, so after it only shapes.txt's own disk write is left (Vasquez's nit, 11-23:
				// out of memory between the write and the line would leave a .rif with no line)
				std::string const line = ProvenanceLine(shape, name, mapName, map.sha256);
				Entry written;
				written.name = name;
				written.size = shape.file.size();
				written.plain = true;
				written.read = true;
				written.sha256 = shape.sha256;
				written.own = &shape.file;   // (the map outlives this listing: a later shape of the same bytes finds it with no read and no copy)
				report->written.reserve(report->written.size() + 1);
				std::string const wrote = WriteNew(shapeFolder, name, shape.file);

				if (wrote == "taken")
				{
					Entry taken;
					taken.name = name;   // (made by someone else just now: never a file of ours, so not plain here)
					AddEntry(&listing, taken);
					continue;
				}

				if (!wrote.empty())
				{
					report->failed = true;
					report->why = name + " couldn't be written: " + wrote;
					return false;
				}

				std::string const noted = AddProvenance(shapeFolder, line);
				report->written.push_back(name);
				AddEntry(&listing, written);

				if (!noted.empty())
				{
					report->failed = true;
					report->why = noted;
					return false;
				}

				done = true;
			}

			if (!done)
			{
				report->skipped.push_back(shape.fileName + ": no free name for it in the shape folder");
			}
		}

		report->filesRead = listing.reads;
		return true;
	}

	namespace
	{
		bool LevelToFolderUnguarded(std::string const &levelPath, std::string const &shapeFolder, ShapeLevelReport *report);
	}

	// OUT OF MEMORY anywhere in the one call per map (Vasquez's MUST, 11-23: a 32-bit editor, and a map at the limits holds about 250-300 MB of large blocks
	// at its peak) fails the MAP, in words, never the editor, as the level open does (level_file.cpp OpenLevelFile::Open). report->read stays as it was.
	bool ShapeExtract_LevelToFolder(std::string const &levelPath, std::string const &shapeFolder, ShapeLevelReport *report)
	{
		try
		{
			return LevelToFolderUnguarded(levelPath, shapeFolder, report);
		}
		catch (std::bad_alloc const &)
		{
			report->failed = true;

			try
			{
				report->why = "the map " + levelPath + " can't be scanned: not enough memory";
			}
			catch (std::bad_alloc const &)
			{
				report->why.clear();   // (no memory even for the words: failed says it)
			}

			return false;
		}
	}

	namespace
	{
	bool LevelToFolderUnguarded(std::string const &levelPath, std::string const &shapeFolder, ShapeLevelReport *report)
	{
		*report = ShapeLevelReport();
		std::vector<char> bytes;
		std::string sha256;
		std::string const why = ShapeExtract_ReadMapFile(levelPath, &bytes, &sha256);

		if (!why.empty())
		{
			report->failed = true;
			report->why = "the map " + levelPath + " can't be read: " + why;
			return false;
		}

		report->read = true;
		size_t const slash = levelPath.find_last_of("\\/");
		std::string const mapName = slash == std::string::npos ? levelPath : levelPath.substr(slash + 1);
		return ShapeExtract_WriteMapShapes(ShapeExtract_ScanMap(mapName, bytes), mapName, shapeFolder, report);
	}
	}

	namespace
	{
		std::string LowerAscii(std::string text)
		{
			for (char &c : text)
			{
				c = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
			}

			return text;
		}

		// A folder to READ from, judged on its own handle, opened without following a link: "" when it is a plain folder on a disk, "missing" when
		// there is none, else why not. Never made (FolderOpen, above, makes the folder it writes into; this one only reads)
		std::string ReadFolderWhyNot(std::string const &folder)
		{
			HANDLE const handle = CreateFileW(Wide(folder).c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
				OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);

			if (handle == INVALID_HANDLE_VALUE)
			{
				DWORD const error = GetLastError();

				return error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND ? "missing" : "can't be opened (" + WindowsError(error) + ")";
			}

			Closer const closer = { handle };
			BY_HANDLE_FILE_INFORMATION info;

			if (GetFileType(handle) != FILE_TYPE_DISK || !GetFileInformationByHandle(handle, &info))
			{
				return "not a folder on a disk";
			}

			if (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
			{
				return "a link (a junction or a symbolic link), not a plain folder: it could lead anywhere, so it isn't looked in";
			}

			return (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? std::string() : "a file, not a folder";
		}

		// The .rif names at a folder's top (a name ending in .rif, any capitals: Windows' own "*.rif" would also match longer endings through the old
		// short names, so every name is listed and judged here), as found, in the code page the editor's paths are in. A name with a letter that code
		// page hasn't goes in `odd` (it couldn't be opened by its name). At most `most` names are listed; *more says there were others.
		std::vector<std::string> RifNames(std::string const &folder, size_t most, std::vector<std::string> *odd, bool *more)
		{
			std::vector<std::string> names;
			WIN32_FIND_DATAW found;
			HANDLE const search = FindFirstFileExW((Wide(folder) + L"\\*").c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, 0);

			*more = false;

			if (search == INVALID_HANDLE_VALUE)
			{
				return names;
			}

			do
			{
				std::wstring const name = found.cFileName;
				size_t const letters = name.size();

				if (letters < 5 || _wcsicmp(name.c_str() + letters - 4, L".rif") != 0)
				{
					continue;   // not a .rif ("." and ".." too)
				}

				if (names.size() + odd->size() >= most)
				{
					*more = true;
					break;
				}

				char narrow[MAX_PATH] = "";
				BOOL lossy = FALSE;
				int const made = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, name.c_str(), -1, narrow, (int)sizeof(narrow), nullptr, &lossy);

				if (made <= 0 || lossy)
				{
					odd->push_back(made > 0 ? std::string(narrow) : std::string("(a name)"));
				}
				else
				{
					names.push_back(narrow);
				}
			}
			while (FindNextFileW(search, &found));

			FindClose(search);

			// in a fixed order, whatever order the disk lists them in, so the same folder always gives the same check
			std::sort(names.begin(), names.end(), [](std::string const &a, std::string const &b) { return LowerAscii(a) < LowerAscii(b); });
			return names;
		}
	}

	static ShapeStockCheck StockCheckUnguarded(std::string const &dataFolder)
	{
		ShapeStockCheck check;
		std::string base = dataFolder;

		while (!base.empty() && (base.back() == '\\' || base.back() == '/'))
		{
			base.pop_back();
		}

		for (char const *folderName : { "avp_rifs", "avp_huds" })
		{
			bool const levels = strcmp(folderName, "avp_rifs") == 0;
			std::string const folder = base + "\\" + folderName;
			std::string const why = ReadFolderWhyNot(folder);

			if (!why.empty() && why != "missing")
			{
				check.refused.push_back(std::string(folderName) + ": " + why);
				continue;   // not looked in, so none of its stock files is called missing either
			}

			// a folder that is missing lists nothing: every stock file of it is missing below
			std::vector<std::string> odd;
			bool more = false;
			std::vector<std::string> const names = why.empty() ? RifNames(folder, kShapeStockMostFiles * 16, &odd, &more) : std::vector<std::string>();
			std::vector<char> bytes;
			std::string sha256;
			size_t tried = 0;

			for (std::string const &name : odd)
			{
				check.refused.push_back(std::string(folderName) + "\\" + name + ": its name has a letter the editor's code page hasn't, so it can't be opened by it");
			}

			for (std::string const &name : names)
			{
				if (tried++ >= kShapeStockMostFiles)
				{
					check.refused.push_back(std::string(folderName) + ": more than " + std::to_string((unsigned long long)kShapeStockMostFiles) +
						" .rif files: the rest not read");
					break;
				}

				// read as the hostile input it is (ShapeExtract_ReadMapFile); its bytes let go before the next
				std::string const readWhy = ShapeExtract_ReadMapFile(folder + "\\" + name, &bytes, &sha256);

				if (!readWhy.empty())
				{
					check.refused.push_back(std::string(folderName) + "\\" + name + ": " + readWhy);
					continue;
				}

				(levels ? check.levels : check.huds)++;

				// Steam's own: the table names a stock file of that folder and name, and these are its very bytes (its size and its SHA-256)
				if (ShapeIdentityStockFile const *const stock = ShapeIdentity_StockFile(folderName, name))
				{
					if (stock->size == bytes.size() && sha256 == stock->sha256)
					{
						(levels ? check.levelsOwn : check.hudsOwn)++;
					}
					else
					{
						check.notOwn.push_back(name);
					}
				}

				bytes.clear();
				bytes.shrink_to_fit();
			}

			if (more)
			{
				check.refused.push_back(std::string(folderName) + ": more than " + std::to_string((unsigned long long)(kShapeStockMostFiles * 16)) +
					" .rif names: the rest not listed");
			}

			// MISSING: a stock file of this folder with no name like it there (a name refused above is there, so it isn't missing)
			for (size_t i = 0; i < kShapeIdentityStockFileCount; i++)
			{
				ShapeIdentityStockFile const &stock = kShapeIdentityStockFiles[i];
				bool there = false;

				for (size_t n = 0; strcmp(stock.folder, folderName) == 0 && !there && n < names.size(); n++)
				{
					there = LowerAscii(names[n]) == LowerAscii(stock.fileName);
				}

				if (strcmp(stock.folder, folderName) == 0 && !there)
				{
					check.missing.push_back(stock.fileName);
				}
			}
		}

		return check;
	}

	// OUT OF MEMORY in the stock check (Vasquez's MUST, 11-23) is said, never the end of the editor: the check comes back with the refusal in words
	ShapeStockCheck ShapeExtract_StockCheck(std::string const &dataFolder)
	{
		try
		{
			return StockCheckUnguarded(dataFolder);
		}
		catch (std::bad_alloc const &)
		{
			ShapeStockCheck check;

			try
			{
				check.refused.push_back(dataFolder + ": not enough memory to check it");
			}
			catch (std::bad_alloc const &)
			{
			}

			return check;
		}
	}
}
