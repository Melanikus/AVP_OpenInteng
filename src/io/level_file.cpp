// Opening a level to edit, and saving it back: the phase 2 plan's F7. Written 2026-11-14 by Melanikus for AVP_OpenInteng.
// Why it's done this way, and what a save does step by step, are at the top of level_file.h.

#include "level_file.h"

#include <algorithm>
#include <limits.h>
#include <memory>
#include <stddef.h>
#include <new>
#include <string.h>
#include <stdlib.h>
#include <string>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "huffman.hpp"
#include "md5.h"
#include "level_file_sum.h"

namespace
{

// Every chunk starts with a heading of 12 bytes: an 8-letter name, then its size, which counts the heading too
size_t const HeadingBytes = 12;

// The largest a level may be once it's unpacked: 32 MiB, the same as the largest file the game reads (level_file_sum.h). A package's
// heading says how big its level is, and the decoder sets that much memory aside, so this is asked before anything is unpacked. The
// largest of the game's own levels is 5,061,672 bytes unpacked (Vaults), and the largest community map 24,987,708 (Refinery, in the
// Universal Map Pack), measured 2026-11-14, so this leaves room above both.
size_t const MostUnpackedBytes = 32u << 20;

// The most chunks a level may have for the editor to open it: 2^18. Pairing a level's chunks takes time that grows with how many there
// are, so a level with far more than any real one is refused before it's paired. The game's own levels have at most 15,935 (Nost03_M),
// and the community maps of the Universal Map Pack about 20,500 (NostromoB), counted 2026-11-14.
size_t const MostChunks = 1u << 18;

// Bytes of zeros after the memory a changed chunk is written into: if the library writes past the size it gave, it shows here
size_t const GuardBytes = 256;

// A chunk's size, from the 4 bytes after its name
int SizeAt(std::vector<char> const & bytes, size_t at)
{
	int size;
	memcpy(&size, &bytes[at + 8], sizeof(size));
	return size;
}

void PutSizeAt(std::vector<char> & bytes, size_t at, int size)
{
	memcpy(&bytes[at + 8], &size, sizeof(size));
}

// A chunk that holds other chunks, as the library holds it: a Chunk_With_Children, whose chunks inside are chunks of their own. A kind
// the library doesn't know it keeps as plain bytes, whatever is inside, as a Miscellaneous_Chunk, the one kind whose r_u_miscellaneous()
// says so (chunk.hpp). That kind isn't a Chunk_With_Children, so the first test already leaves it out today; the second is asked too,
// so that if a holder ever answers r_u_miscellaneous(), it's taken as the plain bytes the library would write it as. (Comment
// rewritten 2026-11-14 by Melanikus (Drake), F7 N5.)
bool HoldsChunks(Chunk * chunk)
{
	return dynamic_cast<Chunk_With_Children *>(chunk) != 0 && !chunk->r_u_miscellaneous();
}

struct Md5
{
	unsigned char sum[16];
};

Md5 Md5Of(std::vector<char> const & bytes)
{
	MD5Context context;
	MD5Init(&context);
	MD5Update(&context, (md5byte const *)bytes.data(), (unsigned)bytes.size());

	Md5 result;
	MD5Final(result.sum, &context);
	return result;
}

// Whether the game's loader, loading a file, read exactly these bytes: the same count, and the same MD5 sum
bool LoaderReadThese(File_Chunk const & loaded, std::vector<char> const & bytes)
{
	Md5 const sum = Md5Of(bytes);
	return loaded.file_sum.bytes == bytes.size() && memcmp(loaded.file_sum.md5, sum.sum, sizeof(sum.sum)) == 0;
}

// Whether an open file is a plain file on a disk with one name only: not a device, a pipe or a folder, not a link (a reparse point),
// and not a hard link's second name, so that writing to it writes to this file alone. (Added 2026-11-14 by Melanikus (Drake), F7 L7.)
bool IsPlainDiskFile(HANDLE file)
{
	if (GetFileType(file) != FILE_TYPE_DISK)
	{
		return false;
	}

	BY_HANDLE_FILE_INFORMATION facts;

	if (!GetFileInformationByHandle(file, &facts))
	{
		return false;
	}

	return facts.nNumberOfLinks == 1 && !(facts.dwFileAttributes & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY));
}

// Reads a whole file, if it's no bigger than the largest level the game reads (level_file_sum.h). With plainOnly, a link of that name
// isn't followed (FILE_FLAG_OPEN_REPARSE_POINT) and the file has to be a plain file on a disk with one name only (IsPlainDiskFile): that's
// how the save reads the file it copies into the one-time .bak. (plainOnly added 2026-11-14 by Melanikus (Drake), F7 L7.) When asked
// (identity), it also gives which file it read on its disk (FILE_ID_INFO, 24 bytes), from the very handle it read through, and
// *identityKnown says whether that could be read. (Added 2026-11-14, F7 N3.)
bool ReadWhole(const char * path, std::vector<char> & bytes, bool plainOnly, bool * identityKnown = 0, unsigned char * identity = 0)
{
	if (identityKnown)
	{
		*identityKnown = false;
	}

	bytes.clear();

	DWORD const how = plainOnly ? (FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT) : FILE_ATTRIBUTE_NORMAL;
	HANDLE const file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, how, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	// Either way, what's read has to be a file on a disk, never a device or a pipe (GetFileType; F7 N7)
	LARGE_INTEGER size;
	size.QuadPart = 0;
	bool ok = (plainOnly ? IsPlainDiskFile(file) : GetFileType(file) == FILE_TYPE_DISK) && GetFileSizeEx(file, &size) &&
		LevelFile_SizeIsUsable(size.QuadPart);

	if (ok)
	{
		bytes.resize((size_t)size.QuadPart);
		DWORD got = 0;
		ok = ReadFile(file, bytes.data(), (DWORD)bytes.size(), &got, 0) && got == bytes.size();
	}

	if (ok && identityKnown && identity)
	{
		FILE_ID_INFO facts;
		*identityKnown = GetFileInformationByHandleEx(file, FileIdInfo, &facts, sizeof(facts)) != 0;

		if (*identityKnown)
		{
			memcpy(identity, &facts, sizeof(facts));
		}
	}

	CloseHandle(file);

	if (!ok)
	{
		bytes.clear();
	}

	return ok;
}

// The picked path made whole by GetFullPathNameW, which tidies a path the same way opening it does, so it names the same file
// CreateFileA would open: read in the code page the "A" file functions read it in. Gives back false if it can't be made.
// (Taken out of RenameHeld 2026-11-14 by Melanikus (Drake), F7 N3, which compares it too.)
bool WholePath(const char * path, std::wstring & whole)
{
	whole.clear();
	UINT const codePage = AreFileApisANSI() ? CP_ACP : CP_OEMCP;
	int const wideChars = MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, path, -1, 0, 0);

	if (wideChars <= 0)
	{
		return false;
	}

	std::vector<wchar_t> wide((size_t)wideChars);

	if (MultiByteToWideChar(codePage, MB_ERR_INVALID_CHARS, path, -1, wide.data(), wideChars) != wideChars)
	{
		return false;
	}

	// Asked with no room, GetFullPathNameW says how much room the whole path needs, its ending zero included; given the room, it says
	// how many letters it wrote, without the zero
	DWORD const room = GetFullPathNameW(wide.data(), 0, 0, 0);

	if (room == 0)
	{
		return false;
	}

	std::vector<wchar_t> made(room);
	DWORD const letters = GetFullPathNameW(wide.data(), room, made.data(), 0);

	if (letters == 0 || letters >= room)
	{
		return false;
	}

	whole.assign(made.data(), letters);
	return true;
}

// Whether a path made whole (WholePath) names a plain file. Not a device: a path starting "\\.\", which is also what a reserved name
// such as NUL or COM1 is made into. And not one of a file's streams ("level.rif:more"): its only colon may be the one after a drive
// letter, read past a "\\?\" at its start. (Added 2026-11-14 by Melanikus (Drake), F7 N7.)
bool IsPlainPath(std::wstring const & whole)
{
	if (whole.compare(0, 4, L"\\\\.\\") == 0)
	{
		return false;
	}

	size_t const start = whole.compare(0, 4, L"\\\\?\\") == 0 ? 4 : 0;

	for (size_t at = start; at < whole.size(); at++)
	{
		wchar_t const drive = whole[start];
		bool const afterDrive = at == start + 1 && ((drive >= L'A' && drive <= L'Z') || (drive >= L'a' && drive <= L'z'));

		if (whole[at] == L':' && !afterDrive)
		{
			return false;
		}
	}

	return true;
}

// Whether a file that's there is a plain file on a disk with one name only (IsPlainDiskFile): not a device or a pipe, not a link, not a
// folder, and not a hard link's second name. It's asked with nothing but its attributes, which another program having it open doesn't
// stop, and a link of that name isn't followed. (Added 2026-11-14 by Melanikus (Drake), F7 N7, as on a disk; a plain file, the same
// day, so a save refuses every file that isn't one the same way.)
bool IsPlainFileThere(const char * path)
{
	HANDLE const file = CreateFileA(path, FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0, OPEN_EXISTING,
		FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	bool const plain = IsPlainDiskFile(file);
	CloseHandle(file);
	return plain;
}

// Writes all of these bytes to a file opened for writing, and makes the disk keep them before it says yes
bool WriteAllAndFlush(HANDLE file, std::vector<char> const & bytes)
{
	size_t done = 0;

	while (done < bytes.size())
	{
		DWORD const asked = (DWORD)((bytes.size() - done) > 0x10000000 ? 0x10000000 : (bytes.size() - done));
		DWORD wrote = 0;

		if (!WriteFile(file, bytes.data() + done, asked, &wrote, 0) || wrote != asked)
		{
			return false;
		}

		done += wrote;
	}

	return FlushFileBuffers(file) != 0;
}

// LeftFile keeps Windows' numbers for a file on its disk (FILE_ID_INFO): they stay the same whatever the file is called, and a new file
// made under the same name doesn't have them. (Added 2026-11-14 by Melanikus (Drake), F7 L10.)
static_assert(sizeof(FILE_ID_INFO) == sizeof(LeftFile::identity), "LeftFile::identity holds a FILE_ID_INFO");

// Reads a file's numbers on its disk, without following a link of that name, and asking for nothing but its attributes, which another
// program having it open doesn't stop. (Added 2026-11-14 by Melanikus (Drake), F7 L10.)
bool IdentityOf(std::string const & name, unsigned char (&identity)[sizeof(FILE_ID_INFO)])
{
	HANDLE const file = CreateFileA(name.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, 0,
		OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	FILE_ID_INFO facts;
	bool const read = GetFileInformationByHandleEx(file, FileIdInfo, &facts, sizeof(facts)) != 0;
	CloseHandle(file);

	if (read)
	{
		memcpy(identity, &facts, sizeof(facts));
	}

	return read;
}

// Takes a name off the list of files left behind: once a new file of the editor's own is made under that name, the old one is gone.
// The name is plain text, so nothing here sets memory aside, and nothing can fail. (Added 2026-11-14 by Melanikus (Drake), F7 L10.)
void ForgetLeftover(const char * name, std::vector<LeftFile> & leftBehind) noexcept
{
	leftBehind.erase(std::remove_if(leftBehind.begin(), leftBehind.end(), [name](LeftFile const & left) { return left.name == name; }),
		leftBehind.end());
}

// Deletes a file the editor made itself, and gives back whether it's gone. One that can't be deleted, and is still there, goes on the
// list of files left behind, with its numbers on the disk, so the editor can tell later that it's its own (LeftBehind). This never
// throws: if even the list can't grow, the file isn't listed. (Added 2026-11-14 by Melanikus (Drake), F7 L10.)
bool DeleteOwn(const char * name, std::vector<LeftFile> & leftBehind) noexcept
{
	if (DeleteFileA(name))
	{
		return true;
	}

	DWORD const why = GetLastError();

	if (why == ERROR_FILE_NOT_FOUND || why == ERROR_PATH_NOT_FOUND)
	{
		return true;
	}

	try
	{
		LeftFile left = {};
		left.name = name;
		left.identityKnown = IdentityOf(left.name, left.identity);
		ForgetLeftover(name, leftBehind);
		leftBehind.push_back(left);
	}
	catch (std::bad_alloc const &)
	{
	}

	return false;
}

// Whether the file there now under this name is one the editor made and couldn't delete: the name is on the list of files left behind,
// and it's the very same file on the disk (its numbers match). (Added 2026-11-14 by Melanikus (Drake), F7 L10.)
bool IsOwnLeftover(std::string const & name, std::vector<LeftFile> const & leftBehind)
{
	for (LeftFile const & left : leftBehind)
	{
		unsigned char now[sizeof(FILE_ID_INFO)];

		if (left.name == name && left.identityKnown && IdentityOf(name, now) && memcmp(now, left.identity, sizeof(now)) == 0)
		{
			return true;
		}
	}

	return false;
}

// Makes a new file of the save's own, open to write: it must not be there already (CREATE_NEW, so *wasThere says whether it was), a link
// of that name is never followed (FILE_FLAG_OPEN_REPARSE_POINT), and what's made has to be a plain file on a disk with one name only.
// Gives back INVALID_HANDLE_VALUE when it can't; a file it made that turned out not to be plain is deleted again (DeleteOwn). A file
// left behind under that name earlier is gone once this one is made, so it comes off that list. (Added 2026-11-14 by Melanikus (Drake),
// F7 L7; the list, L10.)
HANDLE MakeOwnFile(const char * path, bool * wasThere, std::vector<LeftFile> & leftBehind)
{
	HANDLE const file = CreateFileA(path, GENERIC_WRITE, 0, 0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT, 0);

	if (file == INVALID_HANDLE_VALUE)
	{
		*wasThere = GetLastError() == ERROR_FILE_EXISTS;
		return INVALID_HANDLE_VALUE;
	}

	*wasThere = false;
	ForgetLeftover(path, leftBehind);

	if (!IsPlainDiskFile(file))
	{
		CloseHandle(file);
		DeleteOwn(path, leftBehind);
		return INVALID_HANDLE_VALUE;
	}

	return file;
}

// A file the save keeps open until it's done with it, closed whichever way the save ends, even when memory runs out midway.
// (Added 2026-11-14 by Melanikus (Drake), F7 L8.)
class HeldFile
{
public:
	explicit HeldFile(HANDLE opened) : file(opened)
	{
	}

	~HeldFile()
	{
		Close();
	}

	HeldFile(HeldFile const &) = delete;
	HeldFile & operator=(HeldFile const &) = delete;

	bool IsOpen() const
	{
		return file != INVALID_HANDLE_VALUE;
	}

	HANDLE Get() const
	{
		return file;
	}

	void Close()
	{
		if (file != INVALID_HANDLE_VALUE)
		{
			CloseHandle(file);
			file = INVALID_HANDLE_VALUE;
		}
	}

private:
	HANDLE file;
};

// Whether an open file holds exactly these bytes: the same size, and the same bytes from its start, compared a piece at a time.
// (Added 2026-11-14 by Melanikus (Drake), F7 L8.)
bool HoldsTheseBytes(HANDLE file, std::vector<char> const & bytes)
{
	LARGE_INTEGER size;
	LARGE_INTEGER start;
	start.QuadPart = 0;

	if (!GetFileSizeEx(file, &size) || size.QuadPart != (LONGLONG)bytes.size() || !SetFilePointerEx(file, start, 0, FILE_BEGIN))
	{
		return false;
	}

	std::vector<char> piece((size_t)1 << 16);
	size_t done = 0;

	while (done < bytes.size())
	{
		DWORD const asked = (DWORD)((bytes.size() - done) < piece.size() ? (bytes.size() - done) : piece.size());
		DWORD got = 0;

		if (!ReadFile(file, piece.data(), asked, &got, 0) || got != asked || memcmp(piece.data(), bytes.data() + done, asked) != 0)
		{
			return false;
		}

		done += asked;
	}

	return true;
}

// Renames an open file to the picked path through its handle (which needs DELETE access), so the file renamed is the one held, whatever
// has its old name by then. A file already at the picked path is replaced only when replace says so. The new name is the picked path
// made whole (WholePath), so it names the same file CreateFileA would open. (Added 2026-11-14 by Melanikus (Drake), F7 L8; the path
// made whole by the caller since N3.)
//
// It's asked first as a POSIX rename (FileRenameInfoEx with FILE_RENAME_FLAG_POSIX_SEMANTICS, Windows 10 1607 and later). That
// replaces a file another program has open while sharing delete with it; a plain rename is refused then, and some program does hold a
// file for a moment after it's written. Measured 2026-11-14, saving 1,500 times over a file written a moment before, twice:
// the plain rename was refused 23 times in 3,000 (MoveFileExA 13 in 3,000), the POSIX rename 0 times. A program that has the file open
// without sharing delete still stops it, and the save is refused with the file as it was. When Windows doesn't know that kind of
// rename (older than Windows 10 1607, or a file system without it: ERROR_INVALID_PARAMETER or ERROR_NOT_SUPPORTED), the plain rename
// (FileRenameInfo) is asked instead. (POSIX rename added 2026-11-14 by Melanikus (Drake), at Hicks's word.)
bool RenameHeld(HANDLE file, std::wstring const & whole, bool replace)
{
	if (whole.empty())
	{
		return false;
	}

	// Windows' own numbers for these (winbase.h), which it declares only when building for Windows 10 1607 or later: FileRenameInfoEx,
	// and the flags in the DWORD at the start of FILE_RENAME_INFO, where ReplaceIfExists is for the plain rename
	FILE_INFO_BY_HANDLE_CLASS const RenameInfoEx = (FILE_INFO_BY_HANDLE_CLASS)22;
	DWORD const RenameReplaceIfExists = 0x1;
	DWORD const RenamePosixSemantics = 0x2;

	// FILE_RENAME_INFO ends in the new name, so it's made in memory big enough for the name. A vector's memory comes from operator new,
	// which lines it up for any of the types inside FILE_RENAME_INFO.
	size_t const nameBytes = whole.size() * sizeof(wchar_t);
	std::vector<char> asked(sizeof(FILE_RENAME_INFO) + nameBytes);
	FILE_RENAME_INFO * const rename = (FILE_RENAME_INFO *)asked.data();
	DWORD const flags = (replace ? RenameReplaceIfExists : 0) | RenamePosixSemantics;
	memcpy(rename, &flags, sizeof(flags));
	rename->RootDirectory = 0;
	rename->FileNameLength = (DWORD)nameBytes;
	memcpy(rename->FileName, whole.c_str(), nameBytes + sizeof(wchar_t));

	if (SetFileInformationByHandle(file, RenameInfoEx, rename, (DWORD)asked.size()))
	{
		return true;
	}

	DWORD const why = GetLastError();

	if (why != ERROR_INVALID_PARAMETER && why != ERROR_NOT_SUPPORTED)
	{
		return false;
	}

	// The plain rename: the DWORD of flags made back into ReplaceIfExists, nothing else changed
	DWORD const none = 0;
	memcpy(rename, &none, sizeof(none));
	rename->ReplaceIfExists = replace ? TRUE : FALSE;
	return SetFileInformationByHandle(file, FileRenameInfo, rename, (DWORD)asked.size()) != 0;
}

// Gives up a save's ".saving" file. While it's held, it's deleted through the handle (which needs DELETE access), so the file deleted is
// the save's own; otherwise by its name (DeleteOwn). (Added 2026-11-14 by Melanikus (Drake), F7 L8; DeleteOwn, L10.)
void DropSaving(HeldFile & held, std::string const & saving, std::vector<LeftFile> & leftBehind)
{
	if (held.IsOpen())
	{
		FILE_DISPOSITION_INFO gone;
		gone.DeleteFile = TRUE;
		bool const marked = SetFileInformationByHandle(held.Get(), FileDispositionInfo, &gone, sizeof(gone)) != 0;
		held.Close();

		if (marked)
		{
			return;
		}
	}

	DeleteOwn(saving.c_str(), leftBehind);
}

// A copy of some bytes in a file of the editor's own, for the game's loader to read: the loader only reads a file by its name, so this
// is how it's handed exactly the bytes the editor holds, never the picked file a second time. The copy is a new file (MakeOwnFile) in
// the user's own temporary folder (GetTempPathA), named after this process's number and a count, and it's deleted again when this goes
// out of reach. Until AvPx's loader can read bytes it's handed (an AvPx card), this is how the editor loads. (Added 2026-11-14 by
// Melanikus (Drake), F7 L9.)
class PrivateCopy
{
public:
	// A copy that can't be deleted again goes on leftBehind (DeleteOwn)
	PrivateCopy(std::vector<char> const & bytes, std::vector<LeftFile> & leftBehind) : leftBehind(leftBehind)
	{
		char folder[MAX_PATH + 1];
		DWORD const length = GetTempPathA(sizeof(folder), folder);

		if (length == 0 || length >= sizeof(folder))
		{
			return;
		}

		// A name that's there already isn't this copy's, so the next count is tried, a few times at most
		static LONG count = 0;

		for (int tries = 0; tries < 16; tries++)
		{
			std::string name = std::string(folder) + "AVP_OpenInteng-" + std::to_string(GetCurrentProcessId()) + "-" +
				std::to_string(InterlockedIncrement(&count)) + ".rif";
			bool wasThere = false;
			HANDLE const file = MakeOwnFile(name.c_str(), &wasThere, leftBehind);

			if (file == INVALID_HANDLE_VALUE)
			{
				if (wasThere)
				{
					continue;
				}

				return;
			}

			bool const written = WriteAllAndFlush(file, bytes);
			bool const closed = CloseHandle(file) != 0;

			if (written && closed)
			{
				path.swap(name);
			}
			else
			{
				DeleteOwn(name.c_str(), leftBehind);
			}

			return;
		}
	}

	~PrivateCopy()
	{
		if (!path.empty())
		{
			DeleteOwn(path.c_str(), leftBehind);
		}
	}

	PrivateCopy(PrivateCopy const &) = delete;
	PrivateCopy & operator=(PrivateCopy const &) = delete;

	bool Made() const
	{
		return !path.empty();
	}

	const char * Path() const
	{
		return path.c_str();
	}

private:
	std::string path;
	std::vector<LeftFile> & leftBehind;
};

// The library's "file being loaded" (Parent_File), put back as it was when this goes out of reach, whichever way that is, running out
// of memory included, unless Keep says the new one stays. Loading a level sets it to the level being loaded, and the library's readers
// use it as they read; left pointing at a level that's been deleted, the next reader would use memory that's been given back.
// (Added 2026-11-14 by Melanikus (Drake), F7 L11.)
class ParentFileGuard
{
public:
	ParentFileGuard() : before(Parent_File), keep(false)
	{
	}

	~ParentFileGuard()
	{
		if (!keep)
		{
			Parent_File = before;
		}
	}

	ParentFileGuard(ParentFileGuard const &) = delete;
	ParentFileGuard & operator=(ParentFileGuard const &) = delete;

	// The level just loaded stays the file being loaded
	void Keep()
	{
		keep = true;
	}

private:
	Chunk * const before;
	bool keep;
};

// Whether a file is there, and its attributes if it is. False for a file that isn't, and also when that can't be told: the caller
// decides what that means.
bool IsThere(const char * path, DWORD * attributes, bool * cantTell)
{
	*attributes = GetFileAttributesA(path);
	*cantTell = false;

	if (*attributes != INVALID_FILE_ATTRIBUTES)
	{
		return true;
	}

	DWORD const why = GetLastError();
	*cantTell = (why != ERROR_FILE_NOT_FOUND && why != ERROR_PATH_NOT_FOUND);
	return false;
}

}

OpenLevelFile::OpenLevelFile()
: level(0), packed(false), paired(false), chunksSeen(0), tooManyChunks(false), standsForKnown(false), restructured(false), generation(0)
{
	memset(standsForIdentity, 0, sizeof(standsForIdentity));
}

OpenLevelFile::~OpenLevelFile()
{
	Close();
}

void OpenLevelFile::Close()
{
	// The library's "file being loaded" is emptied first if it's this level, so it never points at a level that's been deleted.
	// (Added 2026-11-14 by Melanikus (Drake), F7 L12.)
	if (level && Parent_File == level)
	{
		Parent_File = 0;
	}

	delete level;
	level = 0;
	fileBytes.clear();
	levelBytes.clear();
	packed = false;
	paired = false;
	chunkAt.clear();
	holderOf.clear();
	holders.clear();
	changed.clear();
	edited.clear();
	classIsStale.clear();
	placeOf.clear();
	holdsAChange.clear();
	standsForPath.clear();
	standsForKnown = false;
	restructured = false;
	generation = 0;
}

// Running out of memory, anywhere in the open (the decoder, the game's loader, the pairing), refuses the open, with nothing left open
LevelOpenResult OpenLevelFile::Open(const char * path)
{
	try
	{
		return OpenUnguarded(path);
	}
	catch (std::bad_alloc const &)
	{
		Close();
		return LevelOpenResult::CantRead;
	}
}

LevelOpenResult OpenLevelFile::OpenUnguarded(const char * path)
{
	Close();

	// The path made whole has to name a plain file, not a device or a file's stream (IsPlainPath; F7 N7)
	std::wstring whole;

	if (!path || !path[0] || !WholePath(path, whole) || !IsPlainPath(whole))
	{
		return LevelOpenResult::NoPath;
	}

	// The file's bytes, read once, before the game's loader reads it, and which file it is: its whole path, and its numbers on the disk
	// from the very handle it was read through (F7 N3). ReadWhole reads only a file on a disk (F7 N7).
	if (!ReadWhole(path, fileBytes, false, &standsForKnown, standsForIdentity))
	{
		return LevelOpenResult::CantRead;
	}

	standsForPath.swap(whole);

	// The level inside it. A packed file is unpacked with the game's own checked decoder, the one its loader uses, which refuses a
	// package that doesn't add up. What it gives back is the whole level, its own 12-byte heading first.
	packed = fileBytes.size() >= 8 && memcmp(fileBytes.data(), COMPRESSED_RIF_IDENTIFIER, 8) == 0;

	if (packed)
	{
		// The size the package's heading gives its level is asked first, before the decoder sets any memory aside for it: the whole
		// heading has to be there, and the level has to be at least a chunk's heading and at most MostUnpackedBytes
		int unpackedBytes = 0;

		if (fileBytes.size() >= sizeof(HuffmanPackage))
		{
			memcpy(&unpackedBytes, fileBytes.data() + offsetof(HuffmanPackage, UncompressedDataSize), sizeof(unpackedBytes));
		}

		if (fileBytes.size() < sizeof(HuffmanPackage) || unpackedBytes < (int)HeadingBytes || (size_t)unpackedBytes > MostUnpackedBytes)
		{
			Close();
			return LevelOpenResult::CantRead;
		}

		// The decoder's memory is its own (malloc), held here so that it's given back on every way out, an exception included
		std::unique_ptr<char, void (*)(void *)> const unpacked(
			HuffmanDecompressChecked((HuffmanPackage const *)fileBytes.data(), (int)fileBytes.size()), free);

		if (!unpacked)
		{
			Close();
			return LevelOpenResult::LoaderRefused;
		}

		levelBytes.assign(unpacked.get(), unpacked.get() + unpackedBytes);
	}
	else
	{
		levelBytes = fileBytes;
	}

	// The game's own loader, as the game loads a level, given a private copy of the bytes read above (PrivateCopy) instead of the
	// picked file, so it reads those very bytes. The MD5 sum it keeps of what it read is still asked to be the sum of those bytes: if it
	// isn't, the copy changed in between, and the two can't be trusted together. The level keeps the copy's name, which is gone once
	// it's loaded; the editor never uses that name, and never asks the library to save the level. (Changed 2026-11-14 by Melanikus
	// (Drake), F7 L9; it was the picked file, read by name a second time.)
	PrivateCopy const forTheLoader(fileBytes, leftBehind);

	if (!forTheLoader.Made())
	{
		Close();
		return LevelOpenResult::CantRead;
	}

	// The loader makes the level the library's "file being loaded". That stays so only once the level is open for editing; on every
	// other way out, running out of memory included, it's put back as it was (ParentFileGuard). (Added 2026-11-14 by Melanikus (Drake),
	// F7 L11.)
	ParentFileGuard loading;
	level = new File_Chunk(forTheLoader.Path());

	// CHUNK_FAILED_ON_LOAD is the loader saying it couldn't open or read the file whole, so that's a file that couldn't be read
	// (CantRead); any other code is the loader refusing what it read. (Added 2026-11-14 by Melanikus (Drake), F7 N1.)
	if (level->error_code)
	{
		bool const unread = level->error_code == CHUNK_FAILED_ON_LOAD;
		Close();
		return unread ? LevelOpenResult::CantRead : LevelOpenResult::LoaderRefused;
	}

	if (!LoaderReadThese(*level, fileBytes))
	{
		Close();
		return LevelOpenResult::ChangedWhileOpening;
	}

	// Every loaded chunk is paired with its own bytes. If that can't be done one for one, the editor can't promise to write back
	// what it didn't change, so it doesn't open the level for editing.
	if (levelBytes.size() < HeadingBytes || memcmp(levelBytes.data(), "REBINFF2", 8) != 0 || !PairLevel())
	{
		bool const tooMany = tooManyChunks;
		Close();
		return tooMany ? LevelOpenResult::TooManyChunks : LevelOpenResult::DoesntPair;
	}

	paired = true;
	loading.Keep();
	return LevelOpenResult::Opened;
}

// Running out of memory, anywhere in the replacing, refuses it, with the level the editor holds as it was
LevelOpenResult OpenLevelFile::ReplaceLevel(std::vector<char> const & image)
{
	try
	{
		return ReplaceLevelUnguarded(image);
	}
	catch (std::bad_alloc const &)
	{
		return LevelOpenResult::CantRead;
	}
}

LevelOpenResult OpenLevelFile::ReplaceLevelUnguarded(std::vector<char> const & image)
{
	if (!level || !paired)
	{
		return LevelOpenResult::NothingOpen;
	}

	// A whole level, and its own heading says its own size: what BuildLevel gives
	if (image.size() < HeadingBytes || image.size() > MostUnpackedBytes || memcmp(image.data(), "REBINFF2", 8) != 0 ||
		(size_t)SizeAt(image, 0) != image.size())
	{
		return LevelOpenResult::CantRead;
	}

	// The new level is read and paired in a second OpenLevelFile, as Open does for the first, so the level held now is not touched until the
	// new one is whole. The game's loader is given a private copy of the bytes (PrivateCopy), and its MD5 sum of what it read has to be the
	// sum of those bytes (LoaderReadThese), as in Open.
	PrivateCopy const forTheLoader(image, leftBehind);

	if (!forTheLoader.Made())
	{
		return LevelOpenResult::CantRead;
	}

	OpenLevelFile next;

	// The loader makes the new level the library's "file being loaded"; that stays so only if the new level is swapped in (ParentFileGuard,
	// which goes out of reach before `next`, so a level that's refused is never the library's current one when it's deleted)
	ParentFileGuard loading;
	next.level = new File_Chunk(forTheLoader.Path());

	if (next.level->error_code)
	{
		return next.level->error_code == CHUNK_FAILED_ON_LOAD ? LevelOpenResult::CantRead : LevelOpenResult::LoaderRefused;
	}

	if (!LoaderReadThese(*next.level, image))
	{
		return LevelOpenResult::ChangedWhileOpening;
	}

	next.levelBytes = image;

	if (!next.PairLevel())
	{
		return next.tooManyChunks ? LevelOpenResult::TooManyChunks : LevelOpenResult::DoesntPair;
	}

	next.paired = true;

	// Nothing below can fail: the new level and everything that goes with it change places with the old, which `next` then deletes. What
	// belongs to the file (its bytes, which file it is, the files left behind) stays as it is.
	loading.Keep();
	std::swap(level, next.level);
	levelBytes.swap(next.levelBytes);
	chunkAt.swap(next.chunkAt);
	holderOf.swap(next.holderOf);
	holders.swap(next.holders);
	placeOf.swap(next.placeOf);
	changed.swap(next.changed);
	edited.swap(next.edited);
	classIsStale.swap(next.classIsStale);
	holdsAChange.swap(next.holdsAChange);
	std::swap(chunksSeen, next.chunksSeen);
	std::swap(tooManyChunks, next.tooManyChunks);
	paired = true;
	restructured = true;
	generation++;
	return LevelOpenResult::Opened;
}

// Pairs the whole level's chunks with the bytes they were read from, starting afresh: the place of every chunk, and the chunk each is in
bool OpenLevelFile::PairLevel()
{
	chunkAt.clear();
	holderOf.clear();
	holders.clear();
	placeOf.clear();
	chunksSeen = 0;
	tooManyChunks = false;

	return levelBytes.size() >= HeadingBytes && Pair(level, HeadingBytes, levelBytes.size());
}

// Pairs the chunks a holder holds with the bytes they were read from, from "from" up to "to" in levelBytes, and then the chunks inside
// each of those. The library keeps a holder's chunks in the order it read them, and can give back all those of one kind, in that order.
// So the chunks are first grouped by name, in one walk of the bytes, and then the n-th chunk of a name in the bytes is the n-th the
// library holds by that name, each one's name asked to be the same, all 8 bytes of it. A name has to have exactly as many chunks in the
// library as in the bytes, and none is paired twice. The game's own levels all pair.
//
// A level with more chunks than MostChunks is refused (tooManyChunks), counted as the bytes are walked, before any of them is paired.
bool OpenLevelFile::Pair(Chunk_With_Children * holder, size_t from, size_t to)
{
	// Where each of the holder's chunks is, grouped by its name, in the file's order
	std::map<std::string, std::vector<size_t> > byName;
	std::vector<size_t> inFileOrder;

	for (size_t at = from; at < to; )
	{
		if (to - at < HeadingBytes)
		{
			return false;
		}

		int const size = SizeAt(levelBytes, at);

		if (size < (int)HeadingBytes || (size_t)size > to - at)
		{
			return false;
		}

		if (++chunksSeen > MostChunks)
		{
			tooManyChunks = true;
			return false;
		}

		byName[std::string(&levelBytes[at], 8)].push_back(at);
		inFileOrder.push_back(at);
		at += (size_t)size;
	}

	// Each name's chunks, paired with the library's of that name, one for one
	for (const auto & name : byName)
	{
		// The library looks a kind up by its name as text, so up to the first zero byte, if there is one
		char asText[9];
		memcpy(asText, name.first.data(), 8);
		asText[8] = 0;

		List<Chunk *> found;
		holder->lookup_child(asText, found);

		std::vector<Chunk *> inOrder;

		for (LIF<Chunk *> each(&found); !each.done(); each.next())
		{
			inOrder.push_back(each());
		}

		if (inOrder.size() != name.second.size())
		{
			return false;
		}

		for (size_t n = 0; n < inOrder.size(); n++)
		{
			Chunk * const chunk = inOrder[n];
			size_t const at = name.second[n];

			if (memcmp(chunk->identifier, name.first.data(), 8) != 0 || holderOf.count(chunk) != 0 || chunkAt.count(at) != 0)
			{
				return false;
			}

			// Its size was checked as the bytes were walked, above: at least a heading, and inside the holder
			size_t const bytes = (size_t)SizeAt(levelBytes, at);
			chunkAt[at] = ChunkPlace{ chunk, bytes };
			placeOf[chunk] = at;
			holderOf[chunk] = holder;

			// Whether it holds other chunks is asked here, while it's certainly the library's, and kept (holders), so nothing later has to
			// ask the chunk itself. (Added 2026-11-14 by Melanikus (Drake), F7, at Hicks's word.)
			if (HoldsChunks(chunk))
			{
				holders.insert(chunk);
			}

			// A chunk that holds only numbers has to be the size in the file that the library would write it as, so that a change to it
			// writes back all of what the file held, and nothing else. Every one in the game's own levels is. (Added 2026-11-14 by Melanikus
			// (Drake), F7 L3.)
			if (holders.count(chunk) == 0 && chunk->size_chunk() != bytes)
			{
				return false;
			}
		}
	}

	// Then the chunks inside each chunk that holds others, in the file's order
	for (size_t const at : inFileOrder)
	{
		ChunkPlace const place = chunkAt[at];

		if (holders.count(place.chunk) && !Pair((Chunk_With_Children *)place.chunk, at + HeadingBytes, at + place.bytes))
		{
			return false;
		}
	}

	// Every chunk the library holds in this holder has to be one of those paired. The library can't be asked for all of a holder's chunks,
	// only for those of a name, so it's asked for the holder's size instead: its 12-byte heading and the size of every chunk it holds
	// (Chunk_With_Children::size_chunk, chunk.cpp), worked out from what it holds. A chunk it holds that the file doesn't would make that
	// larger than the holder's bytes, as every chunk is at least its 12-byte heading. (Added 2026-11-14 by Melanikus (Drake), F7 L2.)
	return holder->size_chunk() == (to - from) + HeadingBytes;
}

// The chunk handed in is never used, only looked up: whether it's paired (holderOf) and whether it holds others (holders) were both
// found when it was paired, so a chunk the library has deleted since is never read. (Changed 2026-11-14 by Melanikus (Drake), F7, at
// Hicks's word: it was asked HoldsChunks itself.)
bool OpenLevelFile::MarkChanged(Chunk * chunk)
{
	// A chunk whose bytes SetPayload has replaced can't be written the library's way: the library's class for it still says what the file
	// held before the edit, and writing that would undo the edit
	if (!level || !paired || !chunk || holderOf.count(chunk) == 0 || holders.count(chunk) != 0 || classIsStale.count(chunk) != 0)
	{
		return false;
	}

	NoteChange(chunk);
	return true;
}

// Marks a chunk changed, and every chunk on the way out from it to the level as holding a change (MarkChanged's work, and SetPayload's)
void OpenLevelFile::NoteChange(Chunk * chunk)
{
	changed.insert(chunk);

	// Every chunk on the way out from it to the level holds a change now, so its size is worked out again at the save. Each step out is
	// looked up with find, so nothing is ever added to holderOf here. (find since 2026-11-14 by Melanikus (Drake), F7 N6.)
	for (auto up = holderOf.find(chunk); up != holderOf.end() && up->second && up->second != level; up = holderOf.find(up->second))
	{
		holdsAChange.insert(up->second);
	}
}

// The bytes a chunk holds now (level_file.h). Looked up by the chunk's place, found when it was paired, so a chunk handed in is never used.
bool OpenLevelFile::PayloadOf(Chunk * chunk, std::vector<unsigned char> * payload) const
{
	// A chunk that isn't one of this level's has no place (placeOf is empty with no level open, and a null chunk is never in it)
	if (!payload || holders.count(chunk) != 0)
	{
		return false;
	}

	const auto place = placeOf.find(chunk);

	if (place == placeOf.end())
	{
		return false;
	}

	const auto edit = edited.find(chunk);

	if (edit != edited.end())
	{
		*payload = edit->second;
		return true;
	}

	const auto found = chunkAt.find(place->second);

	// Its place and size were checked when it was paired: inside levelBytes, and at least a heading
	if (found == chunkAt.end())
	{
		return false;
	}

	unsigned char const * const from = (unsigned char const *)levelBytes.data() + place->second;

	payload->assign(from + HeadingBytes, from + found->second.bytes);
	return true;
}

// Puts the bytes in (level_file.h)
bool OpenLevelFile::SetPayload(Chunk * chunk, std::vector<unsigned char> const & payload)
{
	if (holders.count(chunk) != 0)
	{
		return false;
	}

	// A chunk that isn't one of this level's has no place, and its size was checked when it was paired: at least a heading
	const auto place = placeOf.find(chunk);
	const auto found = place == placeOf.end() ? chunkAt.end() : chunkAt.find(place->second);

	if (found == chunkAt.end() || payload.size() != found->second.bytes - HeadingBytes)
	{
		return false;
	}

	// Running out of memory refuses the edit, with nothing changed
	try
	{
		std::vector<unsigned char> copy(payload);
		bool const wasChanged = changed.count(chunk) != 0;

		NoteChange(chunk);

		try
		{
			edited[chunk].swap(copy);
			classIsStale.insert(chunk);
		}
		catch (std::bad_alloc const &)
		{
			if (!wasChanged)
			{
				changed.erase(chunk);
			}

			return false;
		}

		return true;
	}
	catch (std::bad_alloc const &)
	{
		return false;
	}
}

// Writes the chunks from "from" up to "to" in levelBytes, as the next save has them, on the end of out
bool OpenLevelFile::Write(size_t from, size_t to, std::vector<char> & out)
{
	size_t at = from;

	while (at < to)
	{
		// The place is looked up first, so nothing is read at a place that isn't a paired chunk's, and its size is the one checked then
		const auto found = chunkAt.find(at);

		if (found == chunkAt.end())
		{
			return false;
		}

		Chunk * const chunk = found->second.chunk;
		size_t const size = found->second.bytes;

		if (edited.count(chunk))
		{
			// The editor's own bytes for it (SetPayload), which are as many as it had: its heading as it was, then them. Nothing else of it
			// is written: not a byte of the library's class for it, which an edit never changes.
			auto const & bytesMade = edited.find(chunk)->second;

			out.insert(out.end(), levelBytes.begin() + at, levelBytes.begin() + at + HeadingBytes);
			out.insert(out.end(), (char const *)bytesMade.data(), (char const *)bytesMade.data() + bytesMade.size());
		}
		else if (changed.count(chunk))
		{
			// Written by the library itself, into memory filled with zeros first, so the bytes it doesn't set are zeros and never
			// whatever was in the editor's memory. size_chunk works out the size and keeps it in the chunk, for fill_data_block.
			size_t const bytes = chunk->size_chunk();

			if (bytes < HeadingBytes || bytes > (size_t)INT_MAX)
			{
				return false;
			}

			// GuardBytes of zeros follow, and have to be zeros still afterwards. What it wrote has to start with the chunk's own name and
			// the size it gave, too; otherwise the level can't be put together (CantBuild), and nothing is saved. (Added 2026-11-14 by
			// Melanikus (Drake), F7 L5.)
			std::vector<char> written(bytes + GuardBytes, 0);
			chunk->fill_data_block(written.data());

			for (size_t i = bytes; i < written.size(); i++)
			{
				if (written[i] != 0)
				{
					return false;
				}
			}

			if (memcmp(written.data(), chunk->identifier, 8) != 0 || (size_t)SizeAt(written, 0) != bytes)
			{
				return false;
			}

			out.insert(out.end(), written.begin(), written.begin() + bytes);
		}
		else if (holdsAChange.count(chunk))
		{
			// Its heading as it was, then what's inside it, and then its new size in the heading
			size_t const start = out.size();
			out.insert(out.end(), levelBytes.begin() + at, levelBytes.begin() + at + HeadingBytes);

			if (!Write(at + HeadingBytes, at + size, out) || out.size() - start > (size_t)INT_MAX)
			{
				return false;
			}

			PutSizeAt(out, start, (int)(out.size() - start));
		}
		else
		{
			// Not changed, and nothing inside it changed: the very bytes it was read from
			out.insert(out.end(), levelBytes.begin() + at, levelBytes.begin() + at + size);
		}

		at += size;
	}

	return true;
}

// Whether the chunks the library holds in this holder are still exactly the ones paired with the bytes from "from" up to "to", and
// the same is true inside each of them; *bytesNow is the size the holder has now. The tree handed out (Level) may have chunks'
// numbers changed, but never a chunk added or removed: one removed would leave a paired chunk the library no longer holds, and one
// added would never be written. So, for each name, the library's chunks of that name have to be the very ones paired, in the same
// order, before anything inside them is looked at, so a chunk that's been removed is never used. And the holder's size, which the
// library works out from every chunk it holds, has to be the paired chunks' sizes added up, so no chunk of any other name is in it
// either. (Added 2026-11-14 by Melanikus (Drake), F7 L13.)
bool OpenLevelFile::TreeIsAsPaired(Chunk_With_Children * holder, size_t from, size_t to, size_t * bytesNow)
{
	// The paired chunks in the holder, grouped by name, in the file's order
	std::map<std::string, std::vector<Chunk *> > byName;
	std::vector<size_t> inFileOrder;

	for (size_t at = from; at < to; )
	{
		const auto found = chunkAt.find(at);

		if (found == chunkAt.end())
		{
			return false;
		}

		byName[std::string(&levelBytes[at], 8)].push_back(found->second.chunk);
		inFileOrder.push_back(at);
		at += found->second.bytes;
	}

	for (const auto & name : byName)
	{
		// The library looks a kind up by its name as text, so up to the first zero byte, if there is one (as Pair does)
		char asText[9];
		memcpy(asText, name.first.data(), 8);
		asText[8] = 0;

		List<Chunk *> found;
		holder->lookup_child(asText, found);
		size_t n = 0;

		for (LIF<Chunk *> each(&found); !each.done(); each.next(), n++)
		{
			if (n >= name.second.size() || each() != name.second[n])
			{
				return false;
			}
		}

		if (n != name.second.size())
		{
			return false;
		}
	}

	// Every paired chunk is still held, so each can be asked its size now: a chunk that holds others from what's inside it, a changed
	// one as the library works it out, and any other its size as it was paired
	size_t total = HeadingBytes;

	for (size_t const at : inFileOrder)
	{
		ChunkPlace const & place = chunkAt.find(at)->second;
		size_t bytes = place.bytes;

		if (holders.count(place.chunk))
		{
			if (!TreeIsAsPaired((Chunk_With_Children *)place.chunk, at + HeadingBytes, at + place.bytes, &bytes))
			{
				return false;
			}
		}
		else if (changed.count(place.chunk))
		{
			bytes = place.chunk->size_chunk();
		}

		total += bytes;
	}

	*bytesNow = total;
	return holder->size_chunk() == total;
}

bool OpenLevelFile::BuildLevel(std::vector<char> & out)
{
	out.clear();

	if (!level || !paired)
	{
		return false;
	}

	// The level handed out has to be the tree that was paired, chunk for chunk: none added, none removed (TreeIsAsPaired). That's
	// asked even with nothing changed, as a chunk added would be left out of the file without a word. (Added 2026-11-14 by Melanikus
	// (Drake), F7 L13.)
	size_t bytesNow = 0;

	if (!TreeIsAsPaired(level, HeadingBytes, levelBytes.size(), &bytesNow))
	{
		return false;
	}

	if (changed.empty())
	{
		out = levelBytes;
		return true;
	}

	out.insert(out.end(), levelBytes.begin(), levelBytes.begin() + HeadingBytes);

	if (!Write(HeadingBytes, levelBytes.size(), out) || out.size() > (size_t)INT_MAX)
	{
		out.clear();
		return false;
	}

	PutSizeAt(out, 0, (int)out.size());
	return true;
}

// Running out of memory, anywhere in the save, refuses it: a ".saving" file this save made is deleted, and the file picked is left as it
// was
LevelSaveResult OpenLevelFile::SaveAs(const char * path)
{
	savingMade.clear();

	try
	{
		LevelSaveResult const result = SaveAsUnguarded(path);
		savingMade.clear();
		return result;
	}
	catch (std::bad_alloc const &)
	{
		if (!savingMade.empty())
		{
			DeleteOwn(savingMade.c_str(), leftBehind);
			savingMade.clear();
		}

		return LevelSaveResult::CantBuild;
	}
}

// inteng's save (File_Chunk::write_file, mishchnk.cpp:391, which FUN_004be2b0 is; FUN_004719e0 calls it): the library calls prepare_for_output() on the WHOLE
// tree (every Object_Header's version_no goes up, as do the environment, enum and sprite headers'; a Shape_Header gets its file_id_num when it has none, its list of
// the names of the objects that use it is made again and each of those objects' headers is given its shape_id_no; the morphing chunks prepare too), then writes the
// whole tree. This does the same on the level the editor holds: the image is read by the game's own loader, the tree is prepared, and the tree's own bytes are the
// level that is saved. Gives GameWouldRefuse when the loader refuses the image, CantBuild when the tree can't be made or written, Saved when it is made. (Added 2026-11-15 by Melanikus (Drake), Bishop's audit.)
static LevelSaveResult PrepareAsIntengDoes(std::vector<char> const & image, std::vector<LeftFile> & leftBehind, std::vector<char> & prepared)
{
	prepared.clear();

	PrivateCopy const forTheLoader(image, leftBehind);

	if (!forTheLoader.Made())
	{
		return LevelSaveResult::CantBuild;
	}

	ParentFileGuard const putBack;
	std::unique_ptr<File_Chunk> const tree(new File_Chunk(forTheLoader.Path()));

	// the game's loader refusing the level is the same refusal the save has always given
	if (tree->error_code != 0)
	{
		return LevelSaveResult::GameWouldRefuse;
	}

	tree->prepare_for_output();

	size_t const size = tree->size_chunk();

	if (size < HeadingBytes || size > MostUnpackedBytes)
	{
		return LevelSaveResult::CantBuild;
	}

	// The library writes into a block it is given: one of zeros, as BuildLevel's, so the rounding bytes of a name are zeros (the library's own block would be
	// whatever the memory held, which would put bytes of the editor's memory in the file)
	std::vector<char> written(size, 0);
	tree->fill_data_block(written.data());
	prepared.swap(written);
	return LevelSaveResult::Saved;
}

LevelSaveResult OpenLevelFile::SaveAsUnguarded(const char * path)
{
	if (!level || !paired)
	{
		return LevelSaveResult::NothingOpen;
	}

	// The picked path made whole: to rename the new file to (step 5), and to tell whether it's the file the level stands for (step 4).
	// It has to name a plain file, not a device or a file's stream (IsPlainPath; F7 N7).
	std::wstring whole;

	if (!path || !path[0] || !WholePath(path, whole) || !IsPlainPath(whole))
	{
		return LevelSaveResult::NoPath;
	}

	std::string const saving = std::string(path) + ".saving";
	std::string const backup = std::string(path) + ".bak";

	bool cantTell = false;
	DWORD attributes = 0;
	bool const there = IsThere(path, &attributes, &cantTell);

	// A file that's there is replaced only if it's a plain file on a disk with one name only (IsPlainFileThere): otherwise the save is
	// refused here, before anything is made, whether or not it has a .bak yet (F7 N7)
	if (cantTell || (there && ((attributes & FILE_ATTRIBUTE_DIRECTORY) || !IsPlainFileThere(path))))
	{
		return LevelSaveResult::CantReplace;
	}

	if (there && (attributes & FILE_ATTRIBUTE_READONLY))
	{
		return LevelSaveResult::ReadOnly;
	}

	// 1. The whole level in memory, and the file it becomes: with nothing changed, the file's own bytes; otherwise packed again if it
	// was packed, with the game's own packer, whose package the game's loader reads.
	std::vector<char> newLevel;

	if (!BuildLevel(newLevel))
	{
		return LevelSaveResult::CantBuild;
	}

	// The level is held to MostUnpackedBytes before it's packed, the most the editor opens, so it never saves a level it couldn't open
	// again, and the packer never gets more than that. (Added 2026-11-14 by Melanikus (Drake), F7 N4.)
	if (newLevel.size() > MostUnpackedBytes)
	{
		return LevelSaveResult::CantBuild;
	}

	// inteng's own save prepares the whole tree before it writes it, with nothing changed too (the version numbers go up): so does this
	{
		std::vector<char> prepared;

		LevelSaveResult const made = PrepareAsIntengDoes(newLevel, leftBehind, prepared);

		if (made != LevelSaveResult::Saved)
		{
			return made;
		}

		newLevel.swap(prepared);
	}

	std::vector<char> newFile;

	if (packed)
	{
		// The packer's package is memory of its own (AllocMem, which is malloc), held here so it's freed on every way out, running out
		// of memory included. The packer never gives back nothing: when it can't have its memory, AllocMem stops the program, before
		// any file is made. (Changed 2026-11-14 by Melanikus (Drake), F7 N4; there was a test for nothing, which couldn't be reached.)
		std::unique_ptr<HuffmanPackage, void (*)(void *)> const package(
			HuffmanCompression((unsigned char *)newLevel.data(), (int)newLevel.size()), free);
		size_t const packageBytes = (size_t)package->CompressedDataSize + sizeof(HuffmanPackage);
		newFile.assign((char const *)package.get(), (char const *)package.get() + packageBytes);
	}
	else
	{
		newFile = newLevel;
	}

	if (!LevelFile_SizeIsUsable((long long)newFile.size()))
	{
		return LevelSaveResult::CantBuild;
	}

	// 2. Written to a file of its own, which must not be there already, and has to be a plain file on a disk (MakeOwnFile), and flushed
	// to the disk. Its name is kept in savingMade before it's made, as keeping a name can run out of memory, and nothing may fail
	// between making the file and SaveAs knowing it's this save's. A file in the way is never this save's, so savingMade is emptied;
	// it's said to be the editor's own only if it's the very file an earlier save left behind (IsOwnLeftover). (Changed 2026-11-14
	// by Melanikus (Drake), F7 L10.)
	savingMade = saving;
	bool savingWasThere = false;
	HANDLE const file = MakeOwnFile(saving.c_str(), &savingWasThere, leftBehind);

	if (file == INVALID_HANDLE_VALUE)
	{
		savingMade.clear();

		if (!savingWasThere)
		{
			return LevelSaveResult::CantWrite;
		}

		return IsOwnLeftover(saving, leftBehind) ? LevelSaveResult::OwnSavingFileInTheWay : LevelSaveResult::SavingFileInTheWay;
	}

	bool const written = WriteAllAndFlush(file, newFile);
	bool const closed = CloseHandle(file) != 0;

	if (!written || !closed)
	{
		DeleteOwn(saving.c_str(), leftBehind);
		return LevelSaveResult::CantWrite;
	}

	// 3. Loaded back through the game's own loader, as the game would load it, from a private copy of the new file's bytes
	// (PrivateCopy), so the loader reads exactly those. (Changed 2026-11-14 by Melanikus (Drake), F7 L9; it was the .saving, by name.)
	// Loading sets the library's "file being loaded" (it's how its readers refuse a file), so that's put back to this level's on every
	// way out of this block, running out of memory included (ParentFileGuard), after the level loaded back is deleted (unique_ptr; the
	// two are given back in the opposite order to the one they're made in). (Changed 2026-11-14 by Melanikus (Drake), F7 L11.)
	{
		PrivateCopy const forTheLoader(newFile, leftBehind);

		if (!forTheLoader.Made())
		{
			DeleteOwn(saving.c_str(), leftBehind);
			return LevelSaveResult::CantWrite;
		}

		ParentFileGuard const putBack;
		std::unique_ptr<File_Chunk> const loadedBack(new File_Chunk(forTheLoader.Path()));
		// The loader couldn't open or read the copy whole (CHUNK_FAILED_ON_LOAD): the new file couldn't be checked, which is CantWrite,
		// not the game refusing it; any other code is the game's loader refusing it. (Added 2026-11-14 by Melanikus (Drake), F7 N1.)
		bool const unread = loadedBack->error_code == CHUNK_FAILED_ON_LOAD;
		bool const refused = loadedBack->error_code != 0 && !unread;
		bool const sameBytes = loadedBack->error_code == 0 && LoaderReadThese(*loadedBack, newFile);

		if (!sameBytes)
		{
			DeleteOwn(saving.c_str(), leftBehind);
			return refused ? LevelSaveResult::GameWouldRefuse : LevelSaveResult::CantWrite;
		}
	}

	// The .saving opened again, and held from here until it's in place: to read it, and to rename or delete it (GENERIC_READ | DELETE).
	// It's shared for reading only (FILE_SHARE_READ), so while it's held nothing else can write to it, rename it or delete it. A link of
	// that name isn't followed, it has to be a plain file on a disk with one name only, and it has to hold exactly the new file's bytes,
	// which the game's loader read above: so the file put in place in step 5 is that file. FILE_FLAG_WRITE_THROUGH asks for its rename to
	// be on the disk before the rename says it's done. (Added 2026-11-14 by Melanikus (Drake), F7 L8.)
	HeldFile held(CreateFileA(saving.c_str(), GENERIC_READ | DELETE, FILE_SHARE_READ, 0, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_WRITE_THROUGH, 0));

	if (!held.IsOpen() || !IsPlainDiskFile(held.Get()) || !HoldsTheseBytes(held.Get(), newFile))
	{
		DropSaving(held, saving, leftBehind);
		return LevelSaveResult::CantWrite;
	}

	// 4. The first time a file is saved over, a copy of it as it was. A .bak that's already there is kept, never replaced
	// (MakeOwnFile's CREATE_NEW says so), and only a copy made here is ever deleted here. A .bak that's there counts only if it's a
	// plain file: a folder or a link of that name isn't the file's copy, so the save is refused instead. (Added 2026-11-14 by
	// Melanikus (Drake), F7 L6.) Nor does a .bak that an earlier save began and couldn't delete (IsOwnLeftover): it may not hold
	// the whole file. (Added 2026-11-14, F7 L10.)
	if (there)
	{
		// The file as it is now, read once when it's needed: to compare with the file the level stands for, or for the one-time .bak
		std::vector<char> asItIs;
		bool asItIsRead = false;

		// When the file picked is the one the level stands for (the same whole path, compared as the file system compares names, or the
		// very same file on its disk), it has to be as it was when it was opened, or last saved: the same bytes, read as a plain file.
		// If something else has changed it since, it isn't replaced (ChangedOnDisk). (Added 2026-11-14 by Melanikus (Drake), F7 N3.)
		unsigned char now[sizeof(FILE_ID_INFO)];
		bool const samePath = !standsForPath.empty() &&
			CompareStringOrdinal(whole.c_str(), (int)whole.size(), standsForPath.c_str(), (int)standsForPath.size(), TRUE) == CSTR_EQUAL;
		bool const sameFile = standsForKnown && IdentityOf(path, now) && memcmp(now, standsForIdentity, sizeof(now)) == 0;

		if (samePath || sameFile)
		{
			asItIsRead = ReadWhole(path, asItIs, true);

			if (!asItIsRead)
			{
				DropSaving(held, saving, leftBehind);
				return LevelSaveResult::CantReplace;
			}

			if (asItIs != fileBytes)
			{
				DropSaving(held, saving, leftBehind);
				return LevelSaveResult::ChangedOnDisk;
			}
		}

		bool backupCantTell = false;
		DWORD backupAttributes = 0;
		bool const backupThere = IsThere(backup.c_str(), &backupAttributes, &backupCantTell);

		if (backupThere &&
			((backupAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT)) || IsOwnLeftover(backup, leftBehind)))
		{
			DropSaving(held, saving, leftBehind);
			return LevelSaveResult::CantKeepBackup;
		}

		if (!backupThere)
		{
			// The file as it is now, read as a plain file, then written into a .bak made the same way as the .saving (MakeOwnFile), and
			// flushed, so the .bak is a plain file holding this file's bytes and nothing else. (Changed 2026-11-14 by Melanikus (Drake),
			// F7 L7; it was CopyFileA.)
			bool backupWasThere = false;
			bool const read = !backupCantTell && (asItIsRead || ReadWhole(path, asItIs, true));
			HANDLE const copy = read ? MakeOwnFile(backup.c_str(), &backupWasThere, leftBehind) : INVALID_HANDLE_VALUE;
			bool kept = false;

			if (copy != INVALID_HANDLE_VALUE)
			{
				bool const copied = WriteAllAndFlush(copy, asItIs);
				bool const copyClosed = CloseHandle(copy) != 0;
				kept = copied && copyClosed;

				if (!kept)
				{
					DeleteOwn(backup.c_str(), leftBehind);
				}
			}

			if (!kept)
			{
				DropSaving(held, saving, leftBehind);
				return LevelSaveResult::CantKeepBackup;
			}
		}
	}

	// 5. The new file in place of the old, in one step: the .saving held since the end of step 3 is renamed to the picked path through
	// its handle, replacing the file there only if there was one at the start (RenameHeld). (Changed 2026-11-14 by Melanikus (Drake),
	// F7 L8; it was MoveFileExA by name.)
	if (!RenameHeld(held.Get(), whole, there))
	{
		DropSaving(held, saving, leftBehind);
		return LevelSaveResult::CantReplace;
	}

	// From here on the level stands for the file just put in place: its whole path, and its numbers on the disk, read through the handle
	// it was renamed through. Nothing here can fail or run out of memory, as the file is saved by now. (Added 2026-11-14, F7 N3.)
	FILE_ID_INFO placed;
	standsForKnown = GetFileInformationByHandleEx(held.Get(), FileIdInfo, &placed, sizeof(placed)) != 0;

	if (standsForKnown)
	{
		memcpy(standsForIdentity, &placed, sizeof(placed));
	}

	standsForPath.swap(whole);
	held.Close();

	// The ".saving" file is the picked file now, so it's no longer this save's to delete
	savingMade.clear();

	// The level now stands for the file just written. Its chunks are paired with the new bytes, so that the next save writes back what
	// this one wrote. If that can't be done, no more saves are made from it.
	fileBytes.swap(newFile);
	levelBytes.swap(newLevel);
	changed.clear();
	holdsAChange.clear();
	restructured = false;
	// The file is saved by now, so running out of memory here doesn't undo that: no more saves are made from this level
	try
	{
		paired = PairLevel();
	}
	catch (std::bad_alloc const &)
	{
		paired = false;
	}

	return LevelSaveResult::Saved;
}
