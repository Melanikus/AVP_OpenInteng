// See video_import.h. Written 2026-11-18 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "video_import.h"

#include <memory>

#include "media_parsers.h"   // whose code reads the video: the container's reader and each decoder chosen and checked before the first sample (Vasquez 11-22)

#include <math.h>
#include <new>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include <windows.h>

#include <mfapi.h>
#include <mferror.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mftransform.h>
#include <wmcodecdsp.h>
#include <wrl/client.h>

namespace host
{
	namespace
	{
		// Windows' device names, which name a device in any folder and with any ending (CON.mp4 is the console): CON PRN AUX NUL, COM0-9 and LPT0-9 (and their superscript
		// 1 2 3 forms), CONIN$ CONOUT$
		bool IsDeviceName(std::wstring const &leaf)
		{
			// the name up to its first dot, spaces at its end dropped (Windows drops them too: "lpt9 .wmv" is LPT9)
			std::wstring base = leaf.substr(0, leaf.find(L'.'));

			while (!base.empty() && (base.back() == L' ' || base.back() == L'.'))
			{
				base.pop_back();
			}

			for (wchar_t &letter : base)
			{
				letter = (wchar_t)towupper(letter);
			}

			static wchar_t const *const kPlain[] = { L"CON", L"PRN", L"AUX", L"NUL", L"CONIN$", L"CONOUT$" };

			for (wchar_t const *name : kPlain)
			{
				if (base == name)
				{
					return true;
				}
			}

			if (base.size() == 4 && (base.compare(0, 3, L"COM") == 0 || base.compare(0, 3, L"LPT") == 0))
			{
				wchar_t const last = base[3];

				return (last >= L'0' && last <= L'9') || last == 0x00b9 || last == 0x00b2 || last == 0x00b3;
			}

			return false;
		}

		std::string Narrow(std::wstring const &wide)
		{
			std::string narrow;

			for (wchar_t const letter : wide)
			{
				narrow.push_back(letter >= 32 && letter < 127 ? (char)letter : '?');   // the status line's letters only
			}

			return narrow;
		}

		VideoImportResult Refuse(VideoImportResult result, std::string const &words, std::string *why)
		{
			if (why)
			{
				*why = words;
			}

			return result;
		}
	}

	char const *VideoImport_Words(VideoImportResult result)
	{
		switch (result)
		{
		case VideoImportResult::Done: return "done";
		case VideoImportResult::BadCall: return "bad call";
		case VideoImportResult::NotAFile: return "not a plain file";
		case VideoImportResult::NotAVideoName:
		{
			static std::string const words = std::string("not a video's name (") + VideoImport_EndingsWords() + ")";

			return words.c_str();
		}
		case VideoImportResult::Empty: return "the file is empty";
		case VideoImportResult::TooBig: return "the file is too big";
		case VideoImportResult::CantRead: return "Windows couldn't read the file";
		case VideoImportResult::TooLong: return "the video is too long";
		case VideoImportResult::TooLarge: return "the video's picture is too large";
		case VideoImportResult::NotAVideo: return "there's no picture in it";
		case VideoImportResult::BadPicture: return "its picture can't be decoded";
		case VideoImportResult::BadSound: return "its sound can't be decoded";
		case VideoImportResult::NoMediaFoundation: return "Windows' Media Foundation isn't there";
		case VideoImportResult::Stopped: return "the read was stopped";
		case VideoImportResult::NotWindowsCode: return "the code Windows would read it with isn't Windows' own or Microsoft's";
		}

		return "unknown";
	}

	// THE ONE LIST (video_import.h). Each ending's content type is the one whose stream handler is the ending's own in Windows' table (HKLM\SOFTWARE\Microsoft\Windows Media
	// Foundation\ByteStreamHandlers, read on the owner's PC 2026-11-22): .mp4 .m4v .mov .3gp the MPEG-4 source ({271C3902-...}), .mkv .webm the Matroska one ({1f9a2c18-...}),
	// .avi and .wmv their own, .mpg .mpeg .vob .ts .m2ts the MPEG-2 one ({40871C59-...}). The read hands Media Foundation the type, never the file's name, so the type picks the handler
	struct TakenEnding
	{
		wchar_t const *ending;
		wchar_t const *type;
	};

	TakenEnding const kTakenEndings[] = { { L".mp4", L"video/mp4" }, { L".m4v", L"video/mp4" }, { L".mov", L"video/mp4" }, { L".mkv", L"video/x-matroska" },
		{ L".webm", L"video/webm" }, { L".avi", L"video/avi" }, { L".wmv", L"video/x-ms-wmv" }, { L".mpg", L"video/mpeg" }, { L".mpeg", L"video/mpeg" },
		{ L".vob", L"video/mpeg" }, { L".ts", L"video/mpeg" }, { L".m2ts", L"video/mpeg" }, { L".3gp", L"video/3gpp" } };

	char const *VideoImport_EndingsWords()
	{
		static std::string const words = []()
		{
			std::string all;

			for (TakenEnding const &taken : kTakenEndings)
			{
				all += all.empty() ? std::string() : std::string(", ");

				for (wchar_t const *c = taken.ending; *c; c++)
				{
					all.push_back((char)*c);
				}
			}

			return all;
		}();

		return words.c_str();
	}

	std::string VideoImport_CodecWords(GUID const &subtype, bool picture)
	{
		// The streams a Windows may lack a decoder for, and the Microsoft extension (from the Microsoft Store) that adds it
		struct Known
		{
			GUID const *type;
			char const *name;
			char const *adds;
		};

		// Opus is WAVE_FORMAT_OPUS (0x704F) on the audio base; the SDK names it only for a Windows 10 target, and this builds for Windows 8 (_WIN32_WINNT 0x0602)
		GUID opus = MFAudioFormat_Base;

		opus.Data1 = 0x704F;

		Known const known[] = {
			{ &MFVideoFormat_HEVC, "HEVC", "Microsoft's HEVC Video Extension adds it" },
			{ &MFVideoFormat_HEVC_ES, "HEVC", "Microsoft's HEVC Video Extension adds it" },
			{ &MFVideoFormat_VP90, "VP9", "Microsoft's VP9 Video Extensions add it" },
			{ &MFVideoFormat_VP80, "VP8", "Microsoft's VP9 Video Extensions add it" },
			{ &MFVideoFormat_AV1, "AV1", "Microsoft's AV1 Video Extension adds it" },
			{ &MFVideoFormat_MPEG2, "MPEG-2", "Microsoft's MPEG-2 Video Extension adds it" },
			{ &MFAudioFormat_Vorbis, "Vorbis", "Microsoft's Web Media Extensions add it" },
			{ &opus, "Opus", "Microsoft's Web Media Extensions add it" },
		};
		char const *const kind = picture ? "picture" : "sound";

		for (Known const &k : known)
		{
			if (IsEqualGUID(subtype, *k.type))
			{
				return std::string(k.name) + " " + kind + ": " + k.adds;
			}
		}

		// a FOURCC type (its last 12 bytes Media Foundation's base, MFVideoFormat_Base's): its four letters, when they are letters, digits or a space
		GUID base = MFVideoFormat_Base;

		base.Data1 = subtype.Data1;

		if (!IsEqualGUID(subtype, GUID_NULL) && IsEqualGUID(subtype, base))
		{
			std::string letters;

			for (int i = 0; i < 4; i++)
			{
				char const c = (char)((subtype.Data1 >> (8 * i)) & 0xff);

				letters.push_back((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' ? c : '?');
			}

			return letters + " " + kind + " (Windows has no decoder for it)";
		}

		return std::string("this ") + kind + "'s kind (Windows has no decoder for it, and Media Foundation doesn't say which it is)";
	}

	wchar_t const *VideoImport_ContentType(std::wstring const &path)
	{
		size_t const slash = path.find_last_of(L"\\/");
		size_t const dot = path.find_last_of(L'.');

		if (dot == std::wstring::npos || (slash != std::wstring::npos && dot < slash))
		{
			return nullptr;
		}

		std::wstring ending = path.substr(dot);

		for (wchar_t &letter : ending)
		{
			letter = (wchar_t)towlower(letter);
		}

		for (TakenEnding const &taken : kTakenEndings)
		{
			if (ending == taken.ending)
			{
				return taken.type;
			}
		}

		return nullptr;
	}

	bool VideoImport_FolderTagTaken(unsigned long tag)
	{
		// the numbers are Windows' own (winnt.h), written here so an older SDK's missing names can't change what is taken
		unsigned long const kMountPoint = 0xA0000003ul;   // IO_REPARSE_TAG_MOUNT_POINT: a junction or a mounted volume
		unsigned long const kCloud = 0x9000001Aul;        // IO_REPARSE_TAG_CLOUD: OneDrive's placeholder...
		unsigned long const kCloudKinds = 0x0000F000ul;   // ...and its CLOUD_1 to CLOUD_F, which differ only in these bits (IO_REPARSE_TAG_CLOUD_MASK)

		return tag == kMountPoint || (tag & ~kCloudKinds) == kCloud;
	}

	bool VideoImport_FoldersOnTheWayTaken(std::wstring const &full, std::string *why)
	{
		auto const refuse = [why](std::string const &words)
		{
			if (why)
			{
				*why = words;
			}

			return false;
		};

		// a whole path on a drive only: "C:\\" and what follows (the drive itself is the drop's check, video_drop.h)
		if (full.size() < 4 || full[1] != L':' || full[2] != L'\\' || full.size() > 32768)
		{
			return refuse("the folders on the way to " + Narrow(full) + " can't be read: it is not a whole path on a drive");
		}

		size_t const fileFolderEnd = full.find_last_of(L'\\');   // the file's own folder ends here

		// each folder's whole path, from the first under the root (C:\a) down to the file's own (C:\a\b); an empty part (two slashes) adds no folder
		for (size_t at = full.find(L'\\', 3); at != std::wstring::npos && at <= fileFolderEnd; at = full.find(L'\\', at + 1))
		{
			if (full[at - 1] == L'\\')
			{
				continue;
			}

			std::wstring const folder = full.substr(0, at);
			DWORD const attributes = GetFileAttributesW(folder.c_str());

			if (attributes == INVALID_FILE_ATTRIBUTES)
			{
				return refuse("there's no folder " + Narrow(folder));
			}

			if (!(attributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				return refuse(Narrow(folder) + " is not a folder");
			}

			if (!(attributes & FILE_ATTRIBUTE_REPARSE_POINT))
			{
				continue;
			}

			// a link: its kind read on a handle to the link itself (FILE_FLAG_OPEN_REPARSE_POINT: never followed; FILE_FLAG_BACKUP_SEMANTICS: a folder can be opened)
			HANDLE const link = CreateFileW(folder.c_str(), FILE_READ_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
				FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
			FILE_ATTRIBUTE_TAG_INFO tag = {};
			bool const read = link != INVALID_HANDLE_VALUE && GetFileInformationByHandleEx(link, FileAttributeTagInfo, &tag, sizeof(tag));

			if (link != INVALID_HANDLE_VALUE)
			{
				CloseHandle(link);
			}

			if (!read)
			{
				return refuse("Windows won't say what kind of link the folder " + Narrow(folder) + " is, so nothing is opened through it");
			}

			if (!VideoImport_FolderTagTaken(tag.ReparseTag))
			{
				char kind[16];

				snprintf(kind, sizeof(kind), "0x%08lx", (unsigned long)tag.ReparseTag);

				return refuse("the folder " + Narrow(folder) + " on the way is " + (tag.ReparseTag == 0xA000000Cul ? std::string("a symbolic link, which can name another computer") :
					std::string("a link of a kind not taken (") + kind + ")") + ", so nothing is opened through it");
			}
		}

		return true;
	}

	VideoImportResult VideoImport_ReadFile(std::wstring const &path, std::vector<uint8_t> *bytes, std::string *why, uint64_t mostBytes)
	{
		if (!bytes)
		{
			return Refuse(VideoImportResult::BadCall, "no place for the file's bytes", why);
		}

		if (mostBytes < 1 || mostBytes > kVideoImportLongestBytes)
		{
			return Refuse(VideoImportResult::BadCall, "Import video: the most bytes asked is not 1 to " + std::to_string((unsigned long long)(kVideoImportLongestBytes >> 20)) + " MB", why);
		}

		if (!VideoImport_ContentType(path))
		{
			return Refuse(VideoImportResult::NotAVideoName, std::string("Import video: a video's name ends in one of ") + VideoImport_EndingsWords(), why);
		}

		return VideoImport_ReadPlainFile(path, bytes, why, mostBytes, "Import video", "the video");
	}

	// The file read by the same rules as a video, for any caller (the RIM import's picture: Vasquez 2026-11-23): see video_import.h
	VideoImportResult VideoImport_ReadPlainFile(std::wstring const &path, std::vector<uint8_t> *bytes, std::string *why, uint64_t mostBytes, char const *what, char const *noun)
	{
		if (!bytes || !what || !noun)
		{
			return Refuse(VideoImportResult::BadCall, "no place for the file's bytes", why);
		}

		if (mostBytes < 1 || mostBytes > kVideoImportLongestBytes)
		{
			return Refuse(VideoImportResult::BadCall, std::string(what) + ": the most bytes asked is not 1 to " + std::to_string((unsigned long long)(kVideoImportLongestBytes >> 20)) + " MB", why);
		}

		std::string const say = std::string(what) + ": ";

		// a 0 inside the name would end it early and name another file
		if (path.find(L'\0') != std::wstring::npos)
		{
			return Refuse(VideoImportResult::NotAFile, say + "the name has a 0 in it", why);
		}

		// the whole path, as Windows reads it; never a device path (\\.\) or a path that skips Windows' own reading of it (\\?\)
		DWORD const need = GetFullPathNameW(path.c_str(), 0, nullptr, nullptr);

		if (need == 0 || need > 32768)
		{
			return Refuse(VideoImportResult::NotAFile, say + "Windows can't make a whole path of that name", why);
		}

		std::wstring full(need, L'\0');
		DWORD const got = GetFullPathNameW(path.c_str(), need, &full[0], nullptr);

		if (got == 0 || got >= need)
		{
			return Refuse(VideoImportResult::NotAFile, say + "Windows can't make a whole path of that name", why);
		}

		full.resize(got);

		if (full.compare(0, 4, L"\\\\.\\") == 0 || full.compare(0, 4, L"\\\\?\\") == 0 || full.compare(0, 4, L"\\??\\") == 0)
		{
			return Refuse(VideoImportResult::NotAFile, say + "" + Narrow(full) + " is a device's or a raw path, not a plain file's", why);
		}

		// a file's own stream (name:stream): the only colon in a plain path is the drive's
		if (full.find(L':', 2) != std::wstring::npos)
		{
			return Refuse(VideoImportResult::NotAFile, say + "" + Narrow(full) + " names a file's own stream, not a plain file", why);
		}

		size_t const slash = full.find_last_of(L'\\');
		std::wstring const leaf = slash == std::wstring::npos ? full : full.substr(slash + 1);

		if (IsDeviceName(leaf))
		{
			return Refuse(VideoImportResult::NotAFile, say + "" + Narrow(leaf) + " is a device's name in Windows, not a file's", why);
		}

		// THE FOLDERS ON THE WAY (see the header): each read itself, from the root down, before the open below goes through any of them
		std::string folderWhy;

		if (!VideoImport_FoldersOnTheWayTaken(full, &folderWhy))
		{
			return Refuse(VideoImportResult::NotAFile, say + "" + folderWhy, why);
		}

		// opened once, the link itself if it is one (never followed), shared for reading only; everything below is judged on this handle
		HANDLE const file = CreateFileW(full.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_SEQUENTIAL_SCAN, nullptr);

		if (file == INVALID_HANDLE_VALUE)
		{
			DWORD const error = GetLastError();

			if (error == ERROR_FILE_NOT_FOUND || error == ERROR_PATH_NOT_FOUND)
			{
				return Refuse(VideoImportResult::NotAFile, say + "there's no file " + Narrow(full), why);
			}

			return Refuse(error == ERROR_ACCESS_DENIED ? VideoImportResult::NotAFile : VideoImportResult::CantRead, say + "Windows won't open " + Narrow(full) + " (error " + std::to_string((unsigned long)error) + ")", why);
		}

		struct Closer
		{
			HANDLE handle;
			~Closer() { CloseHandle(handle); }
		} const closer = { file };

		FILE_ATTRIBUTE_TAG_INFO tag = {};

		if (GetFileType(file) != FILE_TYPE_DISK || !GetFileInformationByHandleEx(file, FileAttributeTagInfo, &tag, sizeof(tag)))
		{
			return Refuse(VideoImportResult::NotAFile, say + "" + Narrow(full) + " is not a file on a disk", why);
		}

		if (tag.FileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
		{
			return Refuse(VideoImportResult::NotAFile, say + "" + Narrow(full) + " is a link (a shortcut Windows follows), not " + std::string(noun) + " itself", why);
		}

		if (tag.FileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_DEVICE))
		{
			return Refuse(VideoImportResult::NotAFile, say + "" + Narrow(full) + " is a folder, not a file", why);
		}

		LARGE_INTEGER size = {};

		if (!GetFileSizeEx(file, &size) || size.QuadPart < 0)
		{
			return Refuse(VideoImportResult::CantRead, say + "Windows won't tell the size of " + Narrow(full), why);
		}

		if (size.QuadPart == 0)
		{
			return Refuse(VideoImportResult::Empty, say + "" + Narrow(full) + " is empty", why);
		}

		if ((uint64_t)size.QuadPart > mostBytes)
		{
			// (the words name no program's switch: the editor says them too; mp4-to-smk adds how to raise the bound: its ReadRefused)
			return Refuse(VideoImportResult::TooBig, say + "" + Narrow(leaf) + " is " + std::to_string((unsigned long long)(size.QuadPart >> 20)) + " MB; at most " +
				std::to_string((unsigned long long)(mostBytes >> 20)) + " MB is taken", why);
		}

		// read whole: no more than the size said (a file growing meanwhile is cut there), and all of it
		std::vector<uint8_t> read;

		try
		{
			read.resize((size_t)size.QuadPart);
		}
		catch (std::bad_alloc const &)
		{
			return Refuse(VideoImportResult::CantRead, say + "there is not the memory to hold " + Narrow(leaf) + " (" + std::to_string((unsigned long long)(size.QuadPart >> 20)) + " MB)", why);
		}
		size_t done = 0;

		while (done < read.size())
		{
			DWORD const ask = (DWORD)(read.size() - done > (1u << 24) ? (1u << 24) : read.size() - done);
			DWORD part = 0;

			if (!ReadFile(file, read.data() + done, ask, &part, nullptr) || part == 0)
			{
				return Refuse(VideoImportResult::CantRead, say + "Windows stopped reading " + Narrow(leaf) + " after " + std::to_string((unsigned long long)done) + " bytes", why);
			}

			done += part;
		}

		bytes->swap(read);

		if (why)
		{
			why->clear();
		}

		return VideoImportResult::Done;
	}

	VideoImportResult VideoImport_CheckStream(uint32_t width, uint32_t height, int64_t length, std::string *why, int64_t mostSeconds)
	{
		if (mostSeconds < 1 || mostSeconds > kVideoImportLongestSeconds)
		{
			return Refuse(VideoImportResult::BadCall, "Import video: the most seconds asked, " + std::to_string((long long)mostSeconds) + ", is not 1 to " + std::to_string((long long)kVideoImportLongestSeconds), why);
		}

		if (width == 0 || height == 0 || width > kVideoImportMostSide || height > kVideoImportMostSide)
		{
			return Refuse(VideoImportResult::TooLarge, "Import video: its picture is " + std::to_string(width) + " x " + std::to_string(height) + "; at most " + std::to_string(kVideoImportMostSide) +
				" a side is taken", why);
		}

		if (length > mostSeconds * kVideoImportTimeUnitsPerSecond)
		{
			// its length in tenths of a second, rounded UP, so a video a moment past the bound never reads as the bound itself ("180.1 s long; at most 180 s", not "180 s
			// long; at most 180 s"); one decimal at most, none on a whole number, as every length the editor says (model::SmkHeader_SecondsWords). Here the length is
			// past the bound, so above 0, and is never added to (a hostile file's length can be the biggest number there is)
			int64_t const tenth = kVideoImportTimeUnitsPerSecond / 10;
			int64_t const tenths = length / tenth + (length % tenth != 0 ? 1 : 0);
			std::string const said = std::to_string((long long)(tenths / 10)) + (tenths % 10 != 0 ? "." + std::to_string((long long)(tenths % 10)) : std::string());

			// (the words name no program's switch: the editor says them too; mp4-to-smk adds how to raise the bound: its ReadRefused)
			return Refuse(VideoImportResult::TooLong, "Import video: it is " + said + " s long; at most " + std::to_string((long long)mostSeconds) + " s is taken", why);
		}

		return VideoImportResult::Done;
	}

	bool VideoImport_LetterboxArea(uint32_t width, uint32_t height, uint32_t parWidth, uint32_t parHeight, int32_t area[4])
	{
		if (!area || width == 0 || height == 0 || width > kVideoImportMostSide || height > kVideoImportMostSide)
		{
			return false;
		}

		if (parWidth == 0 || parHeight == 0)
		{
			parWidth = parHeight = 1;
		}

		if (parWidth > 16 * (uint64_t)parHeight || parHeight > 16 * (uint64_t)parWidth)
		{
			return false;
		}

		// the picture's shape as it is seen (its pixels' shape counted), against 128 : 96
		double const shape = ((double)width * parWidth) / ((double)height * parHeight);
		int32_t fitWidth = kVideoImportWidth;
		int32_t fitHeight = kVideoImportHeight;

		if (shape * kVideoImportHeight >= (double)kVideoImportWidth)
		{
			fitHeight = (int32_t)floor((double)kVideoImportWidth / shape + 0.5);
		}
		else
		{
			fitWidth = (int32_t)floor((double)kVideoImportHeight * shape + 0.5);
		}

		fitWidth = fitWidth < 1 ? 1 : (fitWidth > kVideoImportWidth ? kVideoImportWidth : fitWidth);
		fitHeight = fitHeight < 1 ? 1 : (fitHeight > kVideoImportHeight ? kVideoImportHeight : fitHeight);
		area[0] = (kVideoImportWidth - fitWidth) / 2;
		area[1] = (kVideoImportHeight - fitHeight) / 2;
		area[2] = fitWidth;
		area[3] = fitHeight;
		return true;
	}

	namespace
	{
		// The source pixels one of `count` output pixels covers along a side of `size`, each with the share of the output pixel it covers (the shares add to 1)
		struct Share
		{
			uint32_t first;
			std::vector<double> weights;
		};

		// the part of the side used: from `start`, `span` long (the whole side, or crop's window)
		std::vector<Share> Shares(double start, double span, uint32_t size, int32_t count)
		{
			std::vector<Share> shares((size_t)count);
			double const step = span / (double)count;

			for (int32_t at = 0; at < count; at++)
			{
				double const from = start + at * step;
				double const to = start + (at + 1) * step;
				uint32_t first = (uint32_t)floor(from);
				uint32_t last = (uint32_t)ceil(to);

				first = first >= size ? size - 1 : first;
				last = last > size ? size : last;
				last = last <= first ? first + 1 : last;
				shares[(size_t)at].first = first;

				for (uint32_t pixel = first; pixel < last; pixel++)
				{
					double const lo = (double)pixel > from ? (double)pixel : from;
					double const hi = (double)(pixel + 1) < to ? (double)(pixel + 1) : to;

					shares[(size_t)at].weights.push_back(hi > lo ? (hi - lo) / step : 0.0);
				}
			}

			return shares;
		}

		uint8_t Byte(double value)
		{
			double const nearest = floor(value + 0.5);

			return (uint8_t)(nearest < 0.0 ? 0.0 : (nearest > 255.0 ? 255.0 : nearest));
		}
	}

	bool VideoImport_CropWindow(uint32_t width, uint32_t height, uint32_t parWidth, uint32_t parHeight, double window[4])
	{
		int32_t area[4];

		if (!window || !VideoImport_LetterboxArea(width, height, parWidth, parHeight, area))
		{
			return false;
		}

		if (parWidth == 0 || parHeight == 0)
		{
			parWidth = parHeight = 1;
		}

		// as seen, the picture is width x parWidth / parHeight wide; the part of 128 : 96's shape: the whole height of a wider picture, the whole width of a taller one
		double const seenWidth = (double)width * parWidth / parHeight;
		double const target = (double)kVideoImportWidth / (double)kVideoImportHeight;

		if (seenWidth >= (double)height * target)
		{
			double const used = (double)height * target * parHeight / parWidth;

			window[0] = ((double)width - used) / 2.0;
			window[1] = 0.0;
			window[2] = used;
			window[3] = (double)height;
		}
		else
		{
			double const used = seenWidth / target;

			window[0] = 0.0;
			window[1] = ((double)height - used) / 2.0;
			window[2] = (double)width;
			window[3] = used;
		}

		return true;
	}

	bool VideoImport_SourceRect(uint32_t width, uint32_t height, uint32_t parWidth, uint32_t parHeight, VideoImportFit fit, double rect[4])
	{
		int32_t area[4];

		if (!rect || !VideoImport_LetterboxArea(width, height, parWidth, parHeight, area))
		{
			return false;
		}

		if (fit == VideoImportFit::Crop)
		{
			return VideoImport_CropWindow(width, height, parWidth, parHeight, rect);
		}

		if (fit == VideoImportFit::Expand)
		{
			rect[0] = 0.0;
			rect[1] = 0.0;
			rect[2] = (double)width;
			rect[3] = (double)height;
			return true;
		}

		// Fit: the rectangle of 128 : 96's shape round the whole picture, from the exact shape as seen (not the rounded area: a 16:9 picture's rectangle is exactly 4:3)
		if (parWidth == 0 || parHeight == 0)
		{
			parWidth = parHeight = 1;
		}

		double const seenWidth = (double)width * parWidth / parHeight;
		double const target = (double)kVideoImportWidth / (double)kVideoImportHeight;

		if (seenWidth >= (double)height * target)
		{
			double const tall = seenWidth / target;

			rect[0] = 0.0;
			rect[1] = ((double)height - tall) / 2.0;
			rect[2] = (double)width;
			rect[3] = tall;
		}
		else
		{
			double const wide = (double)height * target * parHeight / parWidth;

			rect[0] = ((double)width - wide) / 2.0;
			rect[1] = 0.0;
			rect[2] = wide;
			rect[3] = (double)height;
		}

		return true;
	}

	std::string VideoImport_FitWords(uint32_t width, uint32_t height, uint32_t parWidth, uint32_t parHeight, VideoImportFit fit)
	{
		int32_t area[4];
		double rect[4];

		if (!VideoImport_LetterboxArea(width, height, parWidth, parHeight, area) || !VideoImport_SourceRect(width, height, parWidth, parHeight, fit, rect))
		{
			return std::string();
		}

		std::string const size = std::to_string(width) + " x " + std::to_string(height);
		std::string const avp = std::to_string(kVideoImportWidth) + " x " + std::to_string(kVideoImportHeight);

		if (fit == VideoImportFit::Crop)
		{
			return "crop: the middle " + std::to_string((long long)floor(rect[2] + 0.5)) + " x " + std::to_string((long long)floor(rect[3] + 0.5)) + " of " + size + " becomes " + avp;
		}

		if (fit == VideoImportFit::Expand)
		{
			return "expand: all of " + size + " stretched to " + avp;
		}

		if (area[2] == kVideoImportWidth && area[3] == kVideoImportHeight)
		{
			return "fit: all of " + size + " fills " + avp;
		}

		return "fit: all of " + size + " into " + std::to_string(area[2]) + " x " + std::to_string(area[3]) + ", black bars " +
			(area[3] < kVideoImportHeight ? "above and below" : "at the sides");
	}

	bool VideoImport_IsAvpShape(uint32_t width, uint32_t height, uint32_t parWidth, uint32_t parHeight)
	{
		int32_t area[4];

		return VideoImport_LetterboxArea(width, height, parWidth, parHeight, area) && area[0] == 0 && area[1] == 0 && area[2] == kVideoImportWidth && area[3] == kVideoImportHeight;
	}

	bool VideoImport_Letterbox(uint8_t const *top, uint32_t width, uint32_t height, int64_t pitch, uint32_t parWidth, uint32_t parHeight, VideoImportFit fit, uint8_t *out)
	{
		int32_t area[4];
		double window[4] = { 0.0, 0.0, (double)width, (double)height };

		if (!top || !out || !VideoImport_LetterboxArea(width, height, parWidth, parHeight, area))
		{
			return false;
		}

		if (fit == VideoImportFit::Crop)
		{
			if (!VideoImport_CropWindow(width, height, parWidth, parHeight, window))
			{
				return false;
			}

			area[0] = 0;
			area[1] = 0;
			area[2] = kVideoImportWidth;
			area[3] = kVideoImportHeight;
		}

		// Expand: the whole picture (the window as it is) over all of 128 x 96
		if (fit == VideoImportFit::Expand)
		{
			area[0] = 0;
			area[1] = 0;
			area[2] = kVideoImportWidth;
			area[3] = kVideoImportHeight;
		}

		int64_t const rowBytes = (int64_t)width * 4;

		if ((pitch >= 0 ? pitch : -pitch) < rowBytes)
		{
			return false;
		}

		std::vector<Share> const across = Shares(window[0], window[2], width, area[2]);
		std::vector<Share> const down = Shares(window[1], window[3], height, area[3]);

		// across first: each source row the window covers to area[2] pixels (B, G, R as doubles), then down
		uint32_t const firstRow = down.front().first;
		uint32_t const endRow = down.back().first + (uint32_t)down.back().weights.size();
		std::vector<double> rows((size_t)(endRow - firstRow) * area[2] * 3);

		for (uint32_t y = firstRow; y < endRow; y++)
		{
			uint8_t const *const row = top + (int64_t)y * pitch;

			for (int32_t x = 0; x < area[2]; x++)
			{
				Share const &share = across[(size_t)x];
				double sums[3] = { 0.0, 0.0, 0.0 };

				for (size_t i = 0; i < share.weights.size(); i++)
				{
					uint8_t const *const pixel = row + (size_t)(share.first + i) * 4;

					sums[0] += share.weights[i] * pixel[0];
					sums[1] += share.weights[i] * pixel[1];
					sums[2] += share.weights[i] * pixel[2];
				}

				double *const into = &rows[((size_t)(y - firstRow) * area[2] + x) * 3];

				into[0] = sums[0];
				into[1] = sums[1];
				into[2] = sums[2];
			}
		}

		memset(out, 0, (size_t)kVideoImportFrameBytes);

		for (int32_t y = 0; y < area[3]; y++)
		{
			Share const &share = down[(size_t)y];

			for (int32_t x = 0; x < area[2]; x++)
			{
				double sums[3] = { 0.0, 0.0, 0.0 };

				for (size_t i = 0; i < share.weights.size(); i++)
				{
					double const *const from = &rows[((size_t)(share.first + i - firstRow) * area[2] + x) * 3];

					sums[0] += share.weights[i] * from[0];
					sums[1] += share.weights[i] * from[1];
					sums[2] += share.weights[i] * from[2];
				}

				uint8_t *const pixel = out + ((size_t)(area[1] + y) * kVideoImportWidth + (size_t)(area[0] + x)) * 3;

				pixel[0] = Byte(sums[2]);   // R
				pixel[1] = Byte(sums[1]);   // G
				pixel[2] = Byte(sums[0]);   // B
			}
		}

		return true;
	}

	int64_t VideoImport_TicksBefore(int64_t *next, int64_t time)
	{
		if (!next)
		{
			return 0;
		}

		int64_t const most = (kVideoImportLongestSeconds + 1) * kVideoImportTimeUnitsPerSecond;

		time = time < 0 ? 0 : (time > most ? most : time);

		// tick k (at k x 10^7 / 15, cut to a whole 100 ns) is before time when k x 10^7 < 15 x time: so the ticks before it are 0 .. ceil(15 time / 10^7) - 1
		int64_t const scaled = time * kVideoImportFramesPerSecond;
		int64_t const end = (scaled + kVideoImportTimeUnitsPerSecond - 1) / kVideoImportTimeUnitsPerSecond;

		if (end <= *next)
		{
			return 0;
		}

		int64_t const count = end - *next;

		*next = end;
		return count;
	}

	void VideoImport_SoundTo8(int16_t const *samples, size_t count, std::vector<uint8_t> *out)
	{
		if (!samples || !out)
		{
			return;
		}

		out->reserve(out->size() + count);

		for (size_t i = 0; i < count; i++)
		{
			int32_t const value = ((int32_t)samples[i] + 32768 + 128) >> 8;   // the nearest of 256 steps, a half going up

			out->push_back((uint8_t)(value > 255 ? 255 : value));
		}
	}

	namespace
	{
		// the largest 16-bit sample that still comes out below 255 + 1 in 8 bits ((s + 32896) >> 8 <= 255): the gain's ceiling, both ways
		double const kMostUnclipped = 32639.0;

		bool IsRealNumber(double value)
		{
			uint64_t bits;

			memcpy(&bits, &value, sizeof(bits));
			return (bits & 0x7ff0000000000000ull) != 0x7ff0000000000000ull;   // judged by its bits: /fp:fast may take NaN for a number
		}
	}

	double VideoImport_LevelGain(int16_t const *samples, size_t count, double targetRms)
	{
		if (!samples || count == 0 || !IsRealNumber(targetRms) || !(targetRms > 0.0))
		{
			return 1.0;
		}

		double power = 0.0;
		int32_t loudest = 0;

		for (size_t i = 0; i < count; i++)
		{
			int32_t const value = samples[i];
			int32_t const size = value < 0 ? -value : value;

			power += (double)value * value;
			loudest = size > loudest ? size : loudest;
		}

		double const rms = sqrt(power / (double)count);

		if (loudest == 0 || !(rms > 0.0))
		{
			return 1.0;
		}

		double const wanted = targetRms * 256.0 / rms;
		double const most = kMostUnclipped / (double)loudest;

		return wanted < most ? wanted : most;
	}

	void VideoImport_SoundTo8Gained(int16_t const *samples, size_t count, double gain, std::vector<uint8_t> *out)
	{
		if (!samples || !out)
		{
			return;
		}

		if (!IsRealNumber(gain) || !(gain > 0.0))
		{
			gain = 1.0;
		}

		out->reserve(out->size() + count);

		for (size_t i = 0; i < count; i++)
		{
			double const scaled = floor((double)samples[i] * gain + 0.5);
			int16_t const held = (int16_t)(scaled > 32767.0 ? 32767.0 : (scaled < -32768.0 ? -32768.0 : scaled));

			VideoImport_SoundTo8(&held, 1, out);
		}
	}

	double VideoImport_Loudness8(std::vector<uint8_t> const &sound)
	{
		if (sound.empty())
		{
			return 0.0;
		}

		double power = 0.0;

		for (uint8_t const value : sound)
		{
			power += ((double)value - 128.0) * ((double)value - 128.0);
		}

		return sqrt(power / (double)sound.size());
	}

	// ---- THE WHOLE READ, through Media Foundation

	namespace
	{
		using Microsoft::WRL::ComPtr;

		// COM on this thread for the read (a thread already in COM keeps its own kind), and Media Foundation started and stopped round it
		struct ComSession
		{
			bool uninitialise = false;
			ComSession() { uninitialise = SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)); }
			~ComSession() { if (uninitialise) CoUninitialize(); }
		};

		struct MediaFoundationSession
		{
			bool on = false;
			MediaFoundationSession() { on = SUCCEEDED(MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET)); }
			~MediaFoundationSession() { if (on) MFShutdown(); }
		};

		std::string Code(HRESULT hr)
		{
			char words[16];

			snprintf(words, sizeof(words), "0x%08lx", (unsigned long)hr);
			return words;
		}

		// The most sound kept before the fit to the frames: the read's most seconds and two more (a sound a little longer than its picture is cut later)
		size_t MostSoundSamples(int64_t mostSeconds)
		{
			return (size_t)(mostSeconds + 2) * kVideoImportSoundRate;
		}

		struct PictureShape
		{
			uint32_t width = 0;          // the decoded frame (a decoder's frame can be larger than the picture: its rows and columns made up to whole blocks)
			uint32_t height = 0;
			uint32_t left = 0;           // the picture in it (MF_MT_MINIMUM_DISPLAY_APERTURE): only this is shown, the rest is the decoder's padding
			uint32_t top = 0;
			uint32_t shownWidth = 0;
			uint32_t shownHeight = 0;
			uint32_t parWidth = 1;
			uint32_t parHeight = 1;
			int64_t stride = 0;   // the type's own (negative: bottom row first), for a buffer that is not a 2D one
		};

		// The part of the frame that is the picture: the type's minimum display aperture, else its geometric aperture, else all of it; an aperture that doesn't lie inside the frame is
		// taken as none
		void ReadShownPart(IMFMediaType *type, PictureShape *shape)
		{
			GUID const kinds[2] = { MF_MT_MINIMUM_DISPLAY_APERTURE, MF_MT_GEOMETRIC_APERTURE };

			shape->left = 0;
			shape->top = 0;
			shape->shownWidth = shape->width;
			shape->shownHeight = shape->height;

			for (GUID const &kind : kinds)
			{
				MFVideoArea area = {};
				UINT32 size = 0;

				if (FAILED(type->GetBlob(kind, reinterpret_cast<UINT8 *>(&area), sizeof(area), &size)) || size != sizeof(area))
				{
					continue;
				}

				int64_t const left = area.OffsetX.value, top = area.OffsetY.value, width = area.Area.cx, height = area.Area.cy;

				if (left >= 0 && top >= 0 && width > 0 && height > 0 && left + width <= (int64_t)shape->width && top + height <= (int64_t)shape->height)
				{
					shape->left = (uint32_t)left;
					shape->top = (uint32_t)top;
					shape->shownWidth = (uint32_t)width;
					shape->shownHeight = (uint32_t)height;
					return;
				}
			}
		}

		// The shown part of a frame whose top row is at `top`, rows `pitch` bytes apart, fitted into out
		bool FitShown(uint8_t const *top, int64_t pitch, PictureShape const &shape, VideoImportFit fit, uint8_t *out)
		{
			uint8_t const *const shown = top + (int64_t)shape.top * pitch + (int64_t)shape.left * 4;

			return VideoImport_Letterbox(shown, shape.shownWidth, shape.shownHeight, pitch, shape.parWidth, shape.parHeight, fit, out);
		}

		// One decoded picture's rows handed to `use` (its top row and the step to the next, bytes apart, negative for a picture kept bottom row first); false for a
		// buffer shorter than its picture, or when `use` says false
		bool WithPicture(IMFSample *sample, PictureShape const &shape, std::function<bool(uint8_t const *top, int64_t pitch)> const &use)
		{
			ComPtr<IMFMediaBuffer> first;
			DWORD buffers = 0;

			// a 2D buffer knows its own rows (the first row and the step to the next): used when the picture is in one
			if (SUCCEEDED(sample->GetBufferCount(&buffers)) && buffers == 1 && SUCCEEDED(sample->GetBufferByIndex(0, &first)))
			{
				ComPtr<IMF2DBuffer> rows;

				if (SUCCEEDED(first.As(&rows)))
				{
					BYTE *top = nullptr;
					LONG pitch = 0;

					if (SUCCEEDED(rows->Lock2D(&top, &pitch)))
					{
						bool const fitted = (pitch >= 0 ? pitch : -(int64_t)pitch) >= (int64_t)shape.width * 4 && use(top, pitch);

						rows->Unlock2D();
						return fitted;
					}
				}
			}

			// otherwise one run of bytes, rows of the type's stride (bottom row first when it is negative), and long enough for all of them
			ComPtr<IMFMediaBuffer> whole;
			BYTE *bytes = nullptr;
			DWORD length = 0;

			if (FAILED(sample->ConvertToContiguousBuffer(&whole)) || FAILED(whole->Lock(&bytes, nullptr, &length)))
			{
				return false;
			}

			uint64_t const step = (uint64_t)(shape.stride < 0 ? -shape.stride : shape.stride);
			bool fitted = false;

			if (step >= (uint64_t)shape.width * 4 && (uint64_t)length >= step * (shape.height - 1) + (uint64_t)shape.width * 4)
			{
				BYTE const *const top = shape.stride < 0 ? bytes + step * (shape.height - 1) : bytes;

				fitted = use(top, shape.stride);
			}

			whole->Unlock();
			return fitted;
		}

		// One decoded picture fitted into out; false for a buffer shorter than its picture
		bool FitPicture(IMFSample *sample, PictureShape const &shape, VideoImportFit fit, uint8_t *out)
		{
			return WithPicture(sample, shape, [&](uint8_t const *top, int64_t pitch) { return FitShown(top, pitch, shape, fit, out); });
		}

		// The shown part of a picture as it is seen, at most mostSide a side (never larger), R G B a pixel, the top row first: each pixel the average of the part of the
		// picture it covers (the letterbox's own sharing, Shares)
		bool StillOf(uint8_t const *top, int64_t pitch, PictureShape const &shape, uint32_t mostSide, std::vector<uint8_t> *rgb, uint32_t *width, uint32_t *height)
		{
			double const seenWidth = (double)shape.shownWidth * shape.parWidth / shape.parHeight;
			double const seenHeight = (double)shape.shownHeight;
			double const larger = seenWidth > seenHeight ? seenWidth : seenHeight;
			double const scale = larger > (double)mostSide ? (double)mostSide / larger : 1.0;
			int32_t const across = (int32_t)floor(seenWidth * scale + 0.5) < 1 ? 1 : (int32_t)floor(seenWidth * scale + 0.5);
			int32_t const down = (int32_t)floor(seenHeight * scale + 0.5) < 1 ? 1 : (int32_t)floor(seenHeight * scale + 0.5);
			uint8_t const *const shown = top + (int64_t)shape.top * pitch + (int64_t)shape.left * 4;
			std::vector<Share> const columns = Shares(0.0, (double)shape.shownWidth, shape.shownWidth, across);
			std::vector<Share> const rows = Shares(0.0, (double)shape.shownHeight, shape.shownHeight, down);

			rgb->assign((size_t)across * down * 3, 0);

			for (int32_t y = 0; y < down; y++)
			{
				for (int32_t x = 0; x < across; x++)
				{
					double sums[3] = { 0.0, 0.0, 0.0 };
					Share const &row = rows[(size_t)y];
					Share const &column = columns[(size_t)x];

					for (size_t j = 0; j < row.weights.size(); j++)
					{
						uint8_t const *const line = shown + (int64_t)(row.first + j) * pitch;

						for (size_t i = 0; i < column.weights.size(); i++)
						{
							uint8_t const *const pixel = line + (size_t)(column.first + i) * 4;
							double const weight = row.weights[j] * column.weights[i];

							sums[0] += weight * pixel[0];
							sums[1] += weight * pixel[1];
							sums[2] += weight * pixel[2];
						}
					}

					uint8_t *const out = &(*rgb)[((size_t)y * across + x) * 3];

					out[0] = Byte(sums[2]);   // R
					out[1] = Byte(sums[1]);   // G
					out[2] = Byte(sums[0]);   // B
				}
			}

			*width = (uint32_t)across;
			*height = (uint32_t)down;
			return true;
		}

		// Media Foundation's Audio Resampler (a DSP that is part of Windows): the sound as decoded in, 22050 Hz 16-bit mono out, then 8 bits
		struct Resampler
		{
			ComPtr<IMFTransform> transform;
			std::vector<int16_t> *sound = nullptr;   // 16-bit, 22050 Hz, mono
			size_t mostSamples = 0;                  // the read's bound (MostSoundSamples)

			bool Start(IMFMediaType *decoded, std::string *why)
			{
				ComPtr<IMFMediaType> wanted;

				if (FAILED(CoCreateInstance(CLSID_CResamplerMediaObject, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&transform))))
				{
					Refuse(VideoImportResult::BadSound, "Import video: Windows' Audio Resampler isn't there", why);
					return false;
				}

				ComPtr<IWMResamplerProps> quality;

				if (SUCCEEDED(transform.As(&quality)))
				{
					quality->SetHalfFilterLength(60);   // its best (1 .. 60)
				}

				HRESULT hr = transform->SetInputType(0, decoded, 0);

				if (SUCCEEDED(hr))
				{
					hr = MFCreateMediaType(&wanted);
				}

				if (SUCCEEDED(hr))
				{
					wanted->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
					wanted->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
					wanted->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, 1);
					wanted->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32)kVideoImportSoundRate);
					wanted->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
					wanted->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, 2);
					wanted->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (UINT32)kVideoImportSoundRate * 2);
					wanted->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
					hr = transform->SetOutputType(0, wanted.Get(), 0);
				}

				if (SUCCEEDED(hr))
				{
					hr = transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0);
				}

				if (SUCCEEDED(hr))
				{
					hr = transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0);
				}

				if (FAILED(hr))
				{
					Refuse(VideoImportResult::BadSound, "Import video: Windows' Audio Resampler won't take this sound (" + Code(hr) + ")", why);
					return false;
				}

				return true;
			}

			// everything it has ready, to 8 bits; false on an error or past the most sound
			bool Pull()
			{
				MFT_OUTPUT_STREAM_INFO info = {};

				if (FAILED(transform->GetOutputStreamInfo(0, &info)))
				{
					return false;
				}

				bool const itsOwn = (info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) != 0;

				for (;;)
				{
					ComPtr<IMFSample> ours;
					ComPtr<IMFMediaBuffer> room;

					if (!itsOwn && (FAILED(MFCreateSample(&ours)) || FAILED(MFCreateMemoryBuffer(info.cbSize > 65536 ? info.cbSize : 65536, &room)) || FAILED(ours->AddBuffer(room.Get()))))
					{
						return false;
					}

					MFT_OUTPUT_DATA_BUFFER data = {};
					DWORD status = 0;

					data.dwStreamID = 0;
					data.pSample = ours.Get();

					HRESULT const hr = transform->ProcessOutput(0, 1, &data, &status);

					if (data.pEvents)
					{
						data.pEvents->Release();
					}

					ComPtr<IMFSample> given;

					if (itsOwn && data.pSample)
					{
						given.Attach(data.pSample);
					}

					if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
					{
						return true;
					}

					if (FAILED(hr))
					{
						return false;
					}

					IMFSample *const out = itsOwn ? given.Get() : ours.Get();
					ComPtr<IMFMediaBuffer> whole;
					BYTE *bytes = nullptr;
					DWORD length = 0;

					if (!out || FAILED(out->ConvertToContiguousBuffer(&whole)) || FAILED(whole->Lock(&bytes, nullptr, &length)))
					{
						return false;
					}

					sound->insert(sound->end(), reinterpret_cast<int16_t const *>(bytes), reinterpret_cast<int16_t const *>(bytes) + length / 2);
					whole->Unlock();

					if (sound->size() > mostSamples)
					{
						return false;
					}
				}
			}

			bool Push(IMFSample *decoded)
			{
				HRESULT hr = transform->ProcessInput(0, decoded, 0);

				if (hr == MF_E_NOTACCEPTING)
				{
					if (!Pull())
					{
						return false;
					}

					hr = transform->ProcessInput(0, decoded, 0);
				}

				return SUCCEEDED(hr) && Pull();
			}

			bool Finish()
			{
				return SUCCEEDED(transform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0)) && SUCCEEDED(transform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0)) && Pull();
			}
		};

		// What a stream Windows couldn't decode is, in words (VideoImport_CodecWords), from its own type as the file gives it
		std::string StreamWords(IMFSourceReader *reader, DWORD stream, bool picture)
		{
			ComPtr<IMFMediaType> native;
			GUID subtype = GUID_NULL;

			if (reader && SUCCEEDED(reader->GetNativeMediaType(stream, 0, &native)) && native)
			{
				native->GetGUID(MF_MT_SUBTYPE, &subtype);
			}

			return VideoImport_CodecWords(subtype, picture);
		}

		// The file's own numbers of its first picture stream and its first sound stream (-1 for none)
		void FindStreams(IMFSourceReader *reader, DWORD *picture, DWORD *sound, bool *havePicture, bool *haveSound)
		{
			*havePicture = *haveSound = false;

			for (DWORD stream = 0; stream < 64 && !(*havePicture && *haveSound); stream++)
			{
				ComPtr<IMFMediaType> type;
				GUID major = GUID_NULL;

				if (reader->GetNativeMediaType(stream, 0, &type) == MF_E_INVALIDSTREAMNUMBER)
				{
					break;
				}

				if (!type || FAILED(type->GetMajorType(&major)))
				{
					continue;
				}

				if (major == MFMediaType_Video && !*havePicture)
				{
					*picture = stream;
					*havePicture = true;
				}
				else if (major == MFMediaType_Audio && !*haveSound)
				{
					*sound = stream;
					*haveSound = true;
				}
			}
		}
	}

	namespace
	{
		// THE DECODERS ARE DRIVEN BY THE EDITOR ITSELF (card 2c, cards-after-alpha5.md; Bishop 2's order for alpha.6, the owner 11-22: "Ship with the card"). The source
		// reader is never asked to decode: each stream it reads is set to its own native type, so it hands out the compressed samples as the file holds them and makes
		// no decoder and no converter. The editor feeds them to the decoder MediaParsers_ChooseDecoder made and proved Microsoft's (its input type set there), then to
		// Windows' own Video Processor MFT (the picture, to RGB32) or Audio Resampler (the sound). Media Foundation never picks a decoder, so a decoder that isn't Microsoft's (a
		// codec pack's, or one registered for this process) is never made and never reads the codec's headers. (Before this, the reader made its decoder itself, from
		// one registered for this process: had the reader's SetInputType on it ever failed, Media Foundation would have gone on to the next decoder on the PC, and that
		// one's SetInputType would have read the headers before the check refused it: "the one window left", now closed.)
		size_t const kMostOutputBytes = (size_t)kVideoImportMostSide * kVideoImportMostSide * 4 + (1u << 20);   // a sample the editor gives a transform: an RGB32 frame at the most side, and room

		// Windows' own Video Processor MFT (msvproc.dll), by its class as the Windows SDK's mfidl.h gives it (CLSID_VideoProcessorMFT): the SDK declares it only for
		// Windows 8.1 and on, and the editor is built for Windows 8 (_WIN32_WINNT 0x0602), so it is named here; its module is still checked to be Microsoft's
		GUID const kVideoProcessorMft = { 0x88753b26, 0x5b24, 0x49bd, { 0xb2, 0xe7, 0x0c, 0x44, 0x5c, 0x78, 0xc9, 0x82 } };

		// ONE TRANSFORM THE EDITOR DRIVES: samples in (ProcessInput), every sample it has ready out (ProcessOutput, into a sample of the editor's own when the transform
		// makes none, no larger than kMostOutputBytes). A decoder that says its output changed (MF_E_TRANSFORM_STREAM_CHANGE: an H.264 decoder does so at its first
		// picture) is given its next output type by `retype`, the chain's own choice, and the pull goes on
		struct Driven
		{
			ComPtr<IMFTransform> transform;
			std::function<bool()> retype;

			bool Start()
			{
				return SUCCEEDED(transform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0)) && SUCCEEDED(transform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0));
			}

			// every sample it has ready handed to `use`, in order; false on an error, or when `use` says false
			bool Pull(std::function<bool(IMFSample *)> const &use)
			{
				int changes = 0;

				for (;;)
				{
					MFT_OUTPUT_STREAM_INFO info = {};

					if (FAILED(transform->GetOutputStreamInfo(0, &info)))
					{
						return false;
					}

					bool const itsOwn = (info.dwFlags & (MFT_OUTPUT_STREAM_PROVIDES_SAMPLES | MFT_OUTPUT_STREAM_CAN_PROVIDE_SAMPLES)) != 0;
					ComPtr<IMFSample> ours;
					ComPtr<IMFMediaBuffer> room;
					size_t const size = info.cbSize > 0 ? (size_t)info.cbSize : 65536;

					if (!itsOwn && (size > kMostOutputBytes || FAILED(MFCreateSample(&ours)) || FAILED(MFCreateMemoryBuffer((DWORD)size, &room)) || FAILED(ours->AddBuffer(room.Get()))))
					{
						return false;
					}

					MFT_OUTPUT_DATA_BUFFER data = {};
					DWORD status = 0;

					data.dwStreamID = 0;
					data.pSample = ours.Get();

					HRESULT const hr = transform->ProcessOutput(0, 1, &data, &status);

					if (data.pEvents)
					{
						data.pEvents->Release();
					}

					ComPtr<IMFSample> given;

					if (itsOwn && data.pSample)
					{
						given.Attach(data.pSample);
					}

					if (hr == MF_E_TRANSFORM_NEED_MORE_INPUT)
					{
						return true;
					}

					// its output changed: its next output type chosen, then on (a few times at most: a decoder that changes without end is refused)
					if (hr == MF_E_TRANSFORM_STREAM_CHANGE)
					{
						if (++changes > 8 || !retype || !retype())
						{
							return false;
						}

						continue;
					}

					IMFSample *const out = itsOwn ? given.Get() : ours.Get();

					if (FAILED(hr) || !out || !use(out))
					{
						return false;
					}
				}
			}

			bool Push(IMFSample *in, std::function<bool(IMFSample *)> const &use)
			{
				HRESULT hr = transform->ProcessInput(0, in, 0);

				if (hr == MF_E_NOTACCEPTING)
				{
					if (!Pull(use))
					{
						return false;
					}

					hr = transform->ProcessInput(0, in, 0);
				}

				return SUCCEEDED(hr) && Pull(use);
			}

			// the end of its stream: everything it still holds handed out
			bool Drain(std::function<bool(IMFSample *)> const &use)
			{
				return SUCCEEDED(transform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0)) && SUCCEEDED(transform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0)) && Pull(use);
			}

			// after a seek: everything it holds let go
			void Flush()
			{
				transform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0);
			}
		};

		// The picture's shape from the types the chain uses: the frame from the converter's RGB32 (`rgb`: only the picture's own part, so all of it is shown), its pixel
		// shape from what the decoder gives (`decoded`) or the stream's own (`native`)
		VideoImportResult ShapeOfTypes(IMFMediaType *decoded, IMFMediaType *native, IMFMediaType *rgb, int64_t length, int64_t mostSeconds, PictureShape *shape, std::string *why)
		{
			UINT32 width = 0, height = 0;

			if (FAILED(MFGetAttributeSize(rgb, MF_MT_FRAME_SIZE, &width, &height)))
			{
				return Refuse(VideoImportResult::BadPicture, "Import video: Windows' converter gives no picture size", why);
			}

			VideoImportResult const bounds = VideoImport_CheckStream(width, height, length, why, mostSeconds);

			if (bounds != VideoImportResult::Done)
			{
				return bounds;
			}

			UINT32 parWidth = 1, parHeight = 1;

			if ((FAILED(MFGetAttributeRatio(decoded, MF_MT_PIXEL_ASPECT_RATIO, &parWidth, &parHeight)) && FAILED(MFGetAttributeRatio(native, MF_MT_PIXEL_ASPECT_RATIO, &parWidth, &parHeight))) ||
				parWidth == 0 || parHeight == 0 || parWidth > 16 * (uint64_t)parHeight || parHeight > 16 * (uint64_t)parWidth)
			{
				parWidth = parHeight = 1;   // none, or past 1 : 16: taken as square (the picture is still shown, maybe the wrong shape)
			}

			UINT32 stride = 0;

			shape->width = width;
			shape->height = height;
			shape->parWidth = parWidth;
			shape->parHeight = parHeight;
			shape->stride = SUCCEEDED(rgb->GetUINT32(MF_MT_DEFAULT_STRIDE, &stride)) ? (int64_t)(INT32)stride : (int64_t)width * 4;
			ReadShownPart(rgb, shape);
			return VideoImportResult::Done;
		}

		// THE PICTURE'S CHAIN: the decoder the editor chose (none for a picture that needs none), then Windows' own Video Processor MFT to RGB32, and the shape they give.
		// The processor makes ONLY the picture's own part RGB32 (its source rectangle the decoder's display aperture): a decoder's frame is made up to whole blocks (a
		// 320 x 180 H.264 picture comes as 320 x 192), and a converter of the whole frame bleeds the made-up rows' colour into the picture's last rows (measured
		// 11-23 with the Color Converter DSP: the bottom row of a red picture came out (162, 56, 0))
		struct PictureChain
		{
			Driven decoder;               // its transform empty when the picture needs no decoder (uncompressed)
			Driven converter;
			ComPtr<IMFMediaType> native;  // the stream's own type
			PictureShape shape;
			int64_t length = -1;
			int64_t mostSeconds = kVideoImportMostSeconds;
			std::string why;              // why a retype failed, for the read's words
			VideoImportResult failure = VideoImportResult::Done;   // and what that is when it was the picture's bounds (TooLarge, TooLong: VideoImport_CheckStream's own)

			// the decoder's output (the first it offers that the converter takes) and the converter's RGB32, set again (first, and after each stream change), and the
			// shape read from them
			bool Retype()
			{
				ComPtr<IMFMediaType> decoded;

				failure = VideoImportResult::Done;
				converter.transform->SetOutputType(0, nullptr, 0);
				converter.transform->SetInputType(0, nullptr, 0);

				if (decoder.transform)
				{
					for (DWORD i = 0; i < 64 && !decoded; i++)
					{
						ComPtr<IMFMediaType> offered;

						if (FAILED(decoder.transform->GetOutputAvailableType(0, i, &offered)))
						{
							break;
						}

						if (SUCCEEDED(decoder.transform->SetOutputType(0, offered.Get(), 0)) && SUCCEEDED(converter.transform->SetInputType(0, offered.Get(), 0)))
						{
							decoded = offered;
						}
					}
				}
				else if (SUCCEEDED(converter.transform->SetInputType(0, native.Get(), 0)))
				{
					decoded = native;
				}

				if (!decoded)
				{
					why = "Windows' decoder gives no picture Windows' converter takes";
					return false;
				}

				// the picture's own part of the decoded frame (its display aperture, else all of it), and the RGB32 made of exactly that, the same size
				PictureShape area;
				UINT32 frameWidth = 0, frameHeight = 0;

				if (FAILED(MFGetAttributeSize(decoded.Get(), MF_MT_FRAME_SIZE, &frameWidth, &frameHeight)) || frameWidth == 0 || frameHeight == 0 || frameWidth > kVideoImportMostSide ||
					frameHeight > kVideoImportMostSide)
				{
					why = "Windows' decoder gives no picture size it takes";
					failure = frameWidth > kVideoImportMostSide || frameHeight > kVideoImportMostSide ? VideoImportResult::TooLarge : VideoImportResult::BadPicture;
					return false;
				}

				area.width = frameWidth;
				area.height = frameHeight;
				ReadShownPart(decoded.Get(), &area);

				ComPtr<IMFMediaType> rgb;
				UINT32 parWidth = 1, parHeight = 1, rateUp = 0, rateDown = 0;
				HRESULT hr = MFCreateMediaType(&rgb);

				if (SUCCEEDED(hr))
				{
					rgb->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
					rgb->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_RGB32);
					MFSetAttributeSize(rgb.Get(), MF_MT_FRAME_SIZE, area.shownWidth, area.shownHeight);
					rgb->SetUINT32(MF_MT_DEFAULT_STRIDE, area.shownWidth * 4);
					rgb->SetUINT32(MF_MT_SAMPLE_SIZE, area.shownWidth * area.shownHeight * 4);
					rgb->SetUINT32(MF_MT_FIXED_SIZE_SAMPLES, TRUE);
					rgb->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);
					rgb->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);

					if (SUCCEEDED(MFGetAttributeRatio(decoded.Get(), MF_MT_PIXEL_ASPECT_RATIO, &parWidth, &parHeight)))
					{
						MFSetAttributeRatio(rgb.Get(), MF_MT_PIXEL_ASPECT_RATIO, parWidth, parHeight);
					}

					if (SUCCEEDED(MFGetAttributeRatio(decoded.Get(), MF_MT_FRAME_RATE, &rateUp, &rateDown)))
					{
						MFSetAttributeRatio(rgb.Get(), MF_MT_FRAME_RATE, rateUp, rateDown);
					}

					hr = converter.transform->SetOutputType(0, rgb.Get(), 0);
				}

				// its source rectangle the picture's part, its destination all of the RGB32 frame (the processor's own control; it is in Windows since 8)
				ComPtr<IMFVideoProcessorControl> control;

				if (SUCCEEDED(hr) && SUCCEEDED(converter.transform.As(&control)))
				{
					RECT const from = { (LONG)area.left, (LONG)area.top, (LONG)(area.left + area.shownWidth), (LONG)(area.top + area.shownHeight) };
					RECT const to = { 0, 0, (LONG)area.shownWidth, (LONG)area.shownHeight };

					hr = control->SetSourceRectangle(const_cast<RECT *>(&from));
					hr = SUCCEEDED(hr) ? control->SetDestinationRectangle(const_cast<RECT *>(&to)) : hr;
				}

				if (FAILED(hr))
				{
					why = "Windows' video processor won't make it RGB32 (" + Code(hr) + ")";
					return false;
				}

				VideoImportResult const shaped = ShapeOfTypes(decoded.Get(), native.Get(), rgb.Get(), length, mostSeconds, &shape, &why);

				failure = shaped == VideoImportResult::Done ? failure : shaped;
				return shaped == VideoImportResult::Done;
			}

			// one compressed sample (or, with no decoder, one uncompressed) through the chain: each RGB32 picture out handed to `use`
			bool Push(IMFSample *sample, std::function<bool(IMFSample *)> const &use)
			{
				if (!decoder.transform)
				{
					return converter.Push(sample, use);
				}

				return decoder.Push(sample, [&](IMFSample *decoded) { return converter.Push(decoded, use); });
			}

			bool Drain(std::function<bool(IMFSample *)> const &use)
			{
				return (!decoder.transform || decoder.Drain([&](IMFSample *decoded) { return converter.Push(decoded, use); })) && converter.Drain(use);
			}

			void Flush()
			{
				if (decoder.transform)
				{
					decoder.Flush();
				}

				converter.Flush();
			}
		};

		// The picture's chain made: its decoder chosen (MediaParsers_ChooseDecoder: Microsoft's, its input type the stream's own) unless it needs none, Windows' own
		// converter made and checked to be Microsoft's, their types set and both started
		VideoImportResult SetUpPicture(IMFMediaType *native, bool needsDecoder, int64_t length, int64_t mostSeconds, PictureChain *chain, std::string *why)
		{
			std::string notOurs;

			chain->native = native;
			chain->length = length;
			chain->mostSeconds = mostSeconds;

			if (needsDecoder && !MediaParsers_ChooseDecoder(native, &chain->decoder.transform, &notOurs))
			{
				return Refuse(VideoImportResult::NotWindowsCode, "Import video: not read: its picture: " + notOurs, why);
			}

			if (FAILED(CoCreateInstance(kVideoProcessorMft, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&chain->converter.transform))) || !chain->converter.transform)
			{
				return Refuse(VideoImportResult::BadPicture, "Import video: Windows' Video Processor isn't there", why);
			}

			if (!MediaParsers_CheckObject(chain->converter.transform.Get(), "picture's video processor", &notOurs))
			{
				return Refuse(VideoImportResult::NotWindowsCode, "Import video: not read: " + notOurs + ". The editor reads videos only through Windows' own code", why);
			}

			if (!chain->Retype())
			{
				// the picture's bounds say their own words (VideoImport_CheckStream: "Import video: ..."); anything else is the decoder's or the processor's
				if (chain->failure == VideoImportResult::TooLong || (chain->failure == VideoImportResult::TooLarge && chain->why.compare(0, 13, "Import video:") == 0))
				{
					return Refuse(chain->failure, chain->why, why);
				}

				return Refuse(chain->failure != VideoImportResult::Done ? chain->failure : VideoImportResult::BadPicture, "Import video: Windows can't decode this video's picture: " + chain->why,
					why);
			}

			chain->decoder.retype = [chain]() { return chain->Retype(); };

			if ((chain->decoder.transform && !chain->decoder.Start()) || !chain->converter.Start())
			{
				return Refuse(VideoImportResult::BadPicture, "Import video: Windows' decoder or converter won't start on its picture", why);
			}

			return VideoImportResult::Done;
		}

		// THE SOUND'S CHAIN: the decoder the editor chose (none for PCM), its output 16-bit PCM when it offers it, else 32-bit float (the resampler takes either)
		struct SoundChain
		{
			Driven decoder;                // its transform empty when the sound needs no decoder
			ComPtr<IMFMediaType> decoded;  // what the resampler is given

			// the decoder's output chosen: 16-bit PCM first, then 32-bit float; after a stream change only the same again is taken (the resampler is set for it)
			bool ChooseOutput()
			{
				for (int pass = 0; pass < 2; pass++)
				{
					for (DWORD i = 0; i < 64; i++)
					{
						ComPtr<IMFMediaType> offered;
						GUID subtype = GUID_NULL;
						UINT32 bits = 0;

						if (FAILED(decoder.transform->GetOutputAvailableType(0, i, &offered)))
						{
							break;
						}

						bool const wanted = SUCCEEDED(offered->GetGUID(MF_MT_SUBTYPE, &subtype)) && SUCCEEDED(offered->GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, &bits)) &&
							(pass == 0 ? subtype == MFAudioFormat_PCM && bits == 16 : subtype == MFAudioFormat_Float && bits == 32);
						BOOL same = TRUE;

						if (wanted && decoded && (FAILED(decoded->Compare(offered.Get(), MF_ATTRIBUTES_MATCH_INTERSECTION, &same)) || !same))
						{
							continue;
						}

						if (wanted && SUCCEEDED(decoder.transform->SetOutputType(0, offered.Get(), 0)))
						{
							decoded = offered;
							return true;
						}
					}
				}

				return false;
			}

			bool Push(IMFSample *sample, std::function<bool(IMFSample *)> const &use)
			{
				return decoder.transform ? decoder.Push(sample, use) : use(sample);
			}

			bool Drain(std::function<bool(IMFSample *)> const &use)
			{
				return !decoder.transform || decoder.Drain(use);
			}
		};

		// The sound's chain made: its decoder chosen unless it needs none, its output chosen, the decoder started. False with why in words (the video then comes in silent)
		bool SetUpSound(IMFMediaType *native, bool needsDecoder, SoundChain *chain, std::string *why)
		{
			if (!needsDecoder)
			{
				chain->decoded = native;
				return true;
			}

			if (!MediaParsers_ChooseDecoder(native, &chain->decoder.transform, why))
			{
				return false;
			}

			if (!chain->ChooseOutput())
			{
				*why = "Windows' decoder gives no 16-bit or float sound";
				return false;
			}

			chain->decoder.retype = [chain]() { return chain->ChooseOutput(); };

			if (!chain->decoder.Start())
			{
				*why = "Windows' decoder won't start on it";
				return false;
			}

			return true;
		}

		// Whether the reader put a transform of its own on a stream (a decoder or a converter): it must put none, as each stream is read at its own type
		bool ReaderAddedNone(IMFSourceReader *reader, DWORD stream)
		{
			ComPtr<IMFSourceReaderEx> ex;
			GUID category = GUID_NULL;
			ComPtr<IMFTransform> transform;

			return SUCCEEDED(reader->QueryInterface(IID_PPV_ARGS(&ex))) && FAILED(ex->GetTransformForStream(stream, 0, &category, &transform));
		}

		// The file opened for reading: its reader (each stream at its own type), its length, its picture's chain and shape, its first sound stream and that sound's chain
		struct Opened
		{
			std::vector<std::wstring> before;   // the modules loaded just before the reader was made: what its making and setup loaded is judged
			std::string soundLeftOut;           // a sound there but not taken: why, in words (the video then comes in silent)
			ComPtr<IMFSourceReader> reader;
			int64_t length = -1;
			DWORD pictureStream = 0;
			DWORD soundStream = 0;
			bool haveSound = false;
			ComPtr<IMFMediaType> soundNative;
			std::unique_ptr<PictureChain> picture;   // (on the heap: its decoder's retype keeps a pointer to it)
			std::unique_ptr<SoundChain> sound;
		};

		// The call's checks and the sessions both reads need (COM and Media Foundation stay on while the caller holds them)
		VideoImportResult CheckBytes(std::vector<uint8_t> const &bytes, wchar_t const *contentType, void const *place, MediaFoundationSession const &mediaFoundation, std::string *why)
		{
			if (!place || !contentType || bytes.empty())
			{
				return Refuse(VideoImportResult::BadCall, "Import video: nothing to read", why);
			}

			if (bytes.size() > kVideoImportLongestBytes)
			{
				return Refuse(VideoImportResult::TooBig, "Import video: the file is too big", why);
			}

			if (!mediaFoundation.on)
			{
				return Refuse(VideoImportResult::NoMediaFoundation, "Import video: Windows' Media Foundation won't start (an N edition of Windows needs its Media Feature Pack)", why);
			}

			return VideoImportResult::Done;
		}
	}

	static VideoImportResult OpenForRead(std::vector<uint8_t> const &bytes, wchar_t const *contentType, int64_t mostSeconds, Opened *opened, std::string *why)
	{
		// the bytes as a stream in memory: Media Foundation never opens the path
		HGLOBAL const global = GlobalAlloc(GMEM_MOVEABLE, bytes.size());
		void *const into = global ? GlobalLock(global) : nullptr;

		if (!into)
		{
			if (global)
			{
				GlobalFree(global);
			}

			return Refuse(VideoImportResult::CantRead, "Import video: no memory for the file", why);
		}

		memcpy(into, bytes.data(), bytes.size());
		GlobalUnlock(global);

		ComPtr<IStream> stream;

		if (FAILED(CreateStreamOnHGlobal(global, TRUE, &stream)))
		{
			GlobalFree(global);
			return Refuse(VideoImportResult::CantRead, "Import video: no memory for the file", why);
		}

		ComPtr<IMFByteStream> byteStream;
		ComPtr<IMFAttributes> byteStreamAttributes;
		ComPtr<IMFAttributes> readerAttributes;
		ComPtr<IMFSourceReader> reader;
		HRESULT hr = MFCreateMFByteStreamOnStream(stream.Get(), &byteStream);

		if (SUCCEEDED(hr) && SUCCEEDED(byteStream.As(&byteStreamAttributes)))
		{
			byteStreamAttributes->SetString(MF_BYTESTREAM_CONTENT_TYPE, contentType);
		}

		if (SUCCEEDED(hr))
		{
			hr = MFCreateAttributes(&readerAttributes, 1);
		}

		// THE CONTAINER'S READER, chosen here and never by the source resolver (media_parsers.h; Vasquez's Video mode review (a), Bishop 2's rule 11-22): the
		// registered handler for this kind of video whose module is Windows' own or Microsoft's, made and checked before a byte of the video is parsed
		ComPtr<IMFMediaSource> source;
		std::string notOurs;

		if (SUCCEEDED(hr))
		{
			// (no MF_SOURCE_READER_ENABLE_VIDEO_PROCESSING: the reader converts nothing, as it decodes nothing; card 2c)
			opened->before = MediaParsers_LoadedModules();
			hr = MediaParsers_CreateSource(byteStream.Get(), contentType, &source, &notOurs);

			if (hr == E_ACCESSDENIED)
			{
				return Refuse(VideoImportResult::NotWindowsCode, "Import video: not read: " + notOurs + ". The editor reads videos only through Windows' own code", why);
			}
		}

		if (SUCCEEDED(hr))
		{
			hr = MFCreateSourceReaderFromMediaSource(source.Get(), readerAttributes.Get(), &reader);
		}

		if (FAILED(hr))
		{
			return Refuse(VideoImportResult::NotAVideo, "Import video: Windows can't read it as a video (" + (notOurs.empty() ? Code(hr) : notOurs) + ")", why);
		}

		PROPVARIANT duration;

		PropVariantInit(&duration);

		if (SUCCEEDED(reader->GetPresentationAttribute(MF_SOURCE_READER_MEDIASOURCE, MF_PD_DURATION, &duration)) && duration.vt == VT_UI8)
		{
			opened->length = duration.uhVal.QuadPart > (ULONGLONG)INT64_MAX ? INT64_MAX : (int64_t)duration.uhVal.QuadPart;
		}

		PropVariantClear(&duration);

		bool havePicture = false;

		FindStreams(reader.Get(), &opened->pictureStream, &opened->soundStream, &havePicture, &opened->haveSound);

		if (!havePicture)
		{
			return Refuse(VideoImportResult::NotAVideo, "Import video: there's no picture in it", why);
		}

		// THE FORMATS, from the source's own word, before any decoder exists (media_parsers.h, the list by measure): a picture in a format the editor doesn't take
		// refuses the file; a sound that isn't taken is left out (the video comes in silent, and says why). Each decoder taken is the one the editor chose, and
		// the editor drives it itself (card 2c): the reader is given no decoder to make
		ComPtr<IMFMediaType> pictureNative;
		GUID pictureFormat = GUID_NULL;

		if (FAILED(reader->GetNativeMediaType(opened->pictureStream, 0, &pictureNative)) || FAILED(pictureNative->GetGUID(MF_MT_SUBTYPE, &pictureFormat)))
		{
			return Refuse(VideoImportResult::NotAVideo, "Import video: its picture says no format", why);
		}

		if (!MediaParsers_FormatTaken(MFMediaType_Video, pictureFormat))
		{
			return Refuse(VideoImportResult::NotWindowsCode, "Import video: not read: its picture is " + MediaParsers_FormatWords(pictureFormat) +
				", which no decoder of Windows' own reads on this PC (the editor takes H.264, HEVC, VP9, VP8, AV1, MPEG-1 and -2, MPEG-4 part 2, WMV, MJPEG, DV, or none)", why);
		}

		opened->picture.reset(new PictureChain());

		VideoImportResult const pictureSet = SetUpPicture(pictureNative.Get(), !MediaParsers_NeedsNoDecoder(pictureFormat), opened->length, mostSeconds, opened->picture.get(), why);

		if (pictureSet != VideoImportResult::Done)
		{
			return pictureSet;
		}

		if (opened->haveSound)
		{
			ComPtr<IMFMediaType> soundNative;
			GUID soundFormat = GUID_NULL;
			std::string soundWhy;
			bool taken = SUCCEEDED(reader->GetNativeMediaType(opened->soundStream, 0, &soundNative)) && SUCCEEDED(soundNative->GetGUID(MF_MT_SUBTYPE, &soundFormat));

			if (taken && !MediaParsers_FormatTaken(MFMediaType_Audio, soundFormat))
			{
				taken = false;
				soundWhy = "it isn't a sound the editor takes" + std::string(MediaParsers_FormatWords(soundFormat) == "Vorbis" ?
					" (Windows reads Vorbis only through Web Media Extensions, whose decoder is FFmpeg's code, not Microsoft's own)" : "");
			}

			if (taken)
			{
				opened->sound.reset(new SoundChain());
				taken = SetUpSound(soundNative.Get(), !MediaParsers_NeedsNoDecoder(soundFormat), opened->sound.get(), &soundWhy);   // (PCM needs none)
				opened->soundNative = soundNative;
			}

			if (!taken)
			{
				opened->haveSound = false;
				opened->sound.reset();
				opened->soundNative.Reset();
				opened->soundLeftOut = "its sound (" + MediaParsers_FormatWords(soundFormat) + ") is left out: " + soundWhy +
					". The sound the editor takes is AAC, MP3, MPEG audio, WMA, FLAC, ALAC, Opus, Dolby or PCM, each read by Windows' own code; convert the video to MP4 with "
					"AAC sound to keep its sound";
			}
		}

		reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE);
		reader->SetStreamSelection(opened->pictureStream, TRUE);

		// the picture read AT ITS OWN TYPE (card 2c): the reader hands out its compressed samples and makes no decoder; the editor's chain decodes them
		hr = reader->SetCurrentMediaType(opened->pictureStream, nullptr, pictureNative.Get());

		if (FAILED(hr))
		{
			return Refuse(VideoImportResult::BadPicture, "Import video: Windows can't read this video's " + StreamWords(reader.Get(), opened->pictureStream, true) + " (" + Code(hr) + ")",
				why);
		}

		// WHOSE CODE THE READER USED (media_parsers.h, check (C)): its container's reader and every parser its setup loaded, before a single sample is read; anything
		// not Windows' own or Microsoft's is refused, named. And no transform of the reader's own on the picture: it decodes nothing
		if (!MediaParsers_CheckReader(reader.Get(), { opened->pictureStream }, opened->before, &notOurs))
		{
			return Refuse(VideoImportResult::NotWindowsCode, "Import video: not read: " + notOurs + ". The editor reads videos only through Windows' own code", why);
		}

		if (!ReaderAddedNone(reader.Get(), opened->pictureStream))
		{
			return Refuse(VideoImportResult::NotWindowsCode, "Import video: not read: Windows' reader put a decoder of its own choosing on the picture. The editor decodes it only with the "
				"decoder it chose", why);
		}

		opened->reader = reader;
		return VideoImportResult::Done;
	}

	VideoImportResult VideoImport_Probe(std::vector<uint8_t> const &bytes, wchar_t const *contentType, VideoImportInfo *info, std::string *why, int64_t mostSeconds)
	{
		ComSession const com;
		MediaFoundationSession const mediaFoundation;
		VideoImportResult result = CheckBytes(bytes, contentType, info, mediaFoundation, why);
		Opened opened;

		if (result == VideoImportResult::Done)
		{
			result = OpenForRead(bytes, contentType, mostSeconds, &opened, why);
		}

		if (result != VideoImportResult::Done)
		{
			return result;
		}

		info->width = opened.picture->shape.shownWidth;
		info->height = opened.picture->shape.shownHeight;
		info->parWidth = opened.picture->shape.parWidth;
		info->parHeight = opened.picture->shape.parHeight;
		info->length = opened.length;
		info->hasSound = opened.haveSound;
		info->soundLeftOut = opened.soundLeftOut;
		return VideoImportResult::Done;
	}

	VideoImportResult VideoImport_ReadBytes(std::vector<uint8_t> const &bytes, wchar_t const *contentType, VideoImportFit fit, VideoImportClip *clip, std::string *why, int64_t mostSeconds,
		VideoImportProgress const &progress)
	{
		ComSession const com;
		MediaFoundationSession const mediaFoundation;
		VideoImportResult result = CheckBytes(bytes, contentType, clip, mediaFoundation, why);
		Opened opened;

		if (result == VideoImportResult::Done)
		{
			result = OpenForRead(bytes, contentType, mostSeconds, &opened, why);
		}

		if (result != VideoImportResult::Done)
		{
			return result;
		}

		ComPtr<IMFSourceReader> const reader = opened.reader;
		DWORD const pictureStream = opened.pictureStream;
		DWORD const soundStream = opened.soundStream;
		bool const haveSound = opened.haveSound;
		PictureChain &picture = *opened.picture;   // its shape follows the decoder's stream changes (PictureChain::Retype)
		VideoImportClip read;
		HRESULT hr = S_OK;

		read.length = opened.length;
		read.soundLeftOut = opened.soundLeftOut;
		read.sourceWidth = picture.shape.shownWidth;
		read.sourceHeight = picture.shape.shownHeight;

		// the sound as its decoder gives it (16-bit PCM or float: the editor's own SoundChain), then the resampler
		Resampler resampler;

		resampler.sound = &read.sound16;
		resampler.mostSamples = MostSoundSamples(mostSeconds);

		// the frames' room taken at once when the file gives its length (a 32-bit program: a block that grows by doubling may find no room)
		if (read.length > 0)
		{
			read.frames.reserve((size_t)((read.length * kVideoImportFramesPerSecond) / kVideoImportTimeUnitsPerSecond + 2) * kVideoImportFrameBytes);
		}

		if (haveSound)
		{
			ComPtr<IMFMediaType> const decoded = opened.sound->decoded;

			// the sound read AT ITS OWN TYPE too (card 2c): its compressed samples go to the editor's own chain
			reader->SetStreamSelection(soundStream, TRUE);
			hr = reader->SetCurrentMediaType(soundStream, nullptr, opened.soundNative.Get());

			UINT32 channels = 0, rate = 0;

			if (FAILED(hr) || !decoded || FAILED(decoded->GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, &channels)) || FAILED(decoded->GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, &rate)) ||
				channels < 1 || channels > 8 || rate < 1000 || rate > 384000)
			{
				return Refuse(VideoImportResult::BadSound, "Import video: Windows can't decode this video's " + StreamWords(reader.Get(), soundStream, false) + " (" + Code(hr) + ")", why);
			}

			if (!resampler.Start(decoded.Get(), why))
			{
				return VideoImportResult::BadSound;
			}

			// WHOSE CODE NOW (media_parsers.h, check (C)), the sound set up too, before the first sample: the reader on both streams, what it loaded, no transform of
			// the reader's own on either, and the editor's own resampler
			std::string notOurs;

			if (!MediaParsers_CheckReader(reader.Get(), { pictureStream, soundStream }, opened.before, &notOurs) ||
				!MediaParsers_CheckObject(resampler.transform.Get(), "sound's resampler", &notOurs))
			{
				return Refuse(VideoImportResult::NotWindowsCode, "Import video: not read: " + notOurs + ". The editor reads videos only through Windows' own code", why);
			}

			if (!ReaderAddedNone(reader.Get(), pictureStream) || !ReaderAddedNone(reader.Get(), soundStream))
			{
				return Refuse(VideoImportResult::NotWindowsCode, "Import video: not read: Windows' reader put a decoder of its own choosing on the video. The editor decodes it only with the "
					"decoders it chose", why);
			}
		}

		std::vector<uint8_t> pending((size_t)kVideoImportFrameBytes);
		std::vector<uint8_t> fitted((size_t)kVideoImportFrameBytes);
		bool havePending = false;
		int64_t nextTick = 0;
		int64_t lastStart = 0;
		int64_t lastLength = 0;
		bool pictureDone = false;
		bool soundDone = !haveSound;
		bool soundStarted = false;
		int64_t const mostTime = mostSeconds * kVideoImportTimeUnitsPerSecond;
		size_t const mostSamples = MostSoundSamples(mostSeconds);

		int32_t toldPercent = -1;
		auto addFrames = [&read](std::vector<uint8_t> const &frame, int64_t count) {
			for (int64_t i = 0; i < count; i++)
			{
				read.frames.insert(read.frames.end(), frame.begin(), frame.end());
				read.frameCount++;
			}
		};

		// ONE DECODED PICTURE (RGB32, from the chain), placed by its own time: fitted, told, and its ticks filled. A refusal inside says its words through `failure`
		VideoImportResult failure = VideoImportResult::Done;
		auto const takePicture = [&](IMFSample *rgb) {
			LONGLONG start = 0;

			if (FAILED(rgb->GetSampleTime(&start)))
			{
				start = lastStart;   // (a picture with no time of its own follows the last)
			}

			read.sourceWidth = picture.shape.shownWidth;
			read.sourceHeight = picture.shape.shownHeight;

			if (!FitPicture(rgb, picture.shape, fit, fitted.data()))
			{
				failure = Refuse(VideoImportResult::BadPicture, "Import video: a decoded picture is shorter than its size says", why);
				return false;
			}

			// how far: this picture's time over the file's length, told when it moves on (never going back); a "stop" ends the read here
			if (progress)
			{
				int32_t const percent = read.length > 0 ? (int32_t)((start < 0 ? 0 : (start > read.length ? read.length : start)) * 100 / read.length) : 0;

				if (percent > toldPercent || toldPercent < 0)
				{
					toldPercent = percent > toldPercent ? percent : toldPercent;

					if (!progress(toldPercent))
					{
						failure = Refuse(VideoImportResult::Stopped, "Import video: the read was stopped", why);
						return false;
					}
				}
			}

			// the ticks before this picture: the one before it fills them (the first picture fills those before itself)
			int64_t const count = VideoImport_TicksBefore(&nextTick, start);

			addFrames(havePending ? pending : fitted, count);
			pending.swap(fitted);
			havePending = true;

			LONGLONG length = 0;

			lastStart = start;
			lastLength = SUCCEEDED(rgb->GetSampleDuration(&length)) && length > 0 ? length : 0;
			return true;
		};

		auto const takeSound = [&](IMFSample *pcm) {
			if (!resampler.Push(pcm))
			{
				failure = Refuse(read.sound16.size() > mostSamples ? VideoImportResult::TooLong : VideoImportResult::BadSound, "Import video: its sound couldn't be resampled, or runs too long",
					why);
				return false;
			}

			return true;
		};

		// a chain that stopped: its own words when it gave some, else the decoder's
		auto const stopped = [&](bool isPicture) {
			if (failure != VideoImportResult::Done)
			{
				return failure;
			}

			return Refuse(isPicture ? VideoImportResult::BadPicture : VideoImportResult::BadSound, std::string("Import video: Windows' decoder stopped on its ") + (isPicture ? "picture" : "sound") +
				(isPicture && !picture.why.empty() ? " (" + picture.why + ")" : std::string()), why);
		};

		while (!pictureDone || !soundDone)
		{
			DWORD streamIndex = 0, flags = 0;
			LONGLONG start = 0;
			ComPtr<IMFSample> sample;

			hr = reader->ReadSample((DWORD)MF_SOURCE_READER_ANY_STREAM, 0, &streamIndex, &flags, &start, &sample);

			bool const isPicture = streamIndex == pictureStream;

			if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR))
			{
				return Refuse(isPicture ? VideoImportResult::BadPicture : VideoImportResult::BadSound, std::string("Import video: Windows stopped reading its ") + (isPicture ? "picture" : "sound") + " (" + Code(hr) + ")", why);
			}

			// the stream's own type changed part way (another codec or its settings, mid-file): the chain was made for the first, so it is refused
			if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
			{
				return Refuse(isPicture ? VideoImportResult::BadPicture : VideoImportResult::BadSound, std::string("Import video: its ") + (isPicture ? "picture" : "sound") +
					"'s format changes part way through; it isn't read", why);
			}

			if (sample && start > mostTime)
			{
				return Refuse(VideoImportResult::TooLong, "Import video: it runs past " + std::to_string((long long)mostSeconds) + " s; at most that is taken", why);
			}

			if (sample && isPicture && !picture.Push(sample.Get(), takePicture))
			{
				return stopped(true);
			}
			else if (sample && streamIndex == soundStream && haveSound)
			{
				// a sound that starts late: silence before it, as long as the gap
				if (!soundStarted && start > 0)
				{
					size_t const gap = (size_t)((double)start * kVideoImportSoundRate / (double)kVideoImportTimeUnitsPerSecond);

					read.sound16.assign(gap < mostSamples ? gap : mostSamples, 0);
				}

				soundStarted = true;

				if (!opened.sound->Push(sample.Get(), takeSound))
				{
					return stopped(false);
				}
			}

			// a stream's end: what its chain still holds comes out
			if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
			{
				if (isPicture && !pictureDone)
				{
					pictureDone = true;

					if (!picture.Drain(takePicture))
					{
						return stopped(true);
					}
				}
				else if (!isPicture && !soundDone)
				{
					soundDone = true;

					if (haveSound && !opened.sound->Drain(takeSound))
					{
						return stopped(false);
					}
				}
			}
		}

		if (!havePending)
		{
			return Refuse(VideoImportResult::NotAVideo, "Import video: no picture could be decoded", why);
		}

		if (haveSound && !resampler.Finish())
		{
			return Refuse(read.sound16.size() > mostSamples ? VideoImportResult::TooLong : VideoImportResult::BadSound, "Import video: its sound couldn't be resampled to the end", why);
		}

		// the last picture fills the ticks to its own end; when it gives no length, to the file's length or a tick on (the file's length alone would count a sound's padding: an
		// AAC track runs a little past its pictures)
		int64_t const tickLater = lastStart + kVideoImportTimeUnitsPerSecond / kVideoImportFramesPerSecond;
		int64_t const end = lastLength > 0 ? lastStart + lastLength : (read.length > tickLater ? read.length : tickLater);

		addFrames(pending, VideoImport_TicksBefore(&nextTick, end));

		// the sound fitted to the frames: 1470 samples each, silence put in at the end or the rest cut; then its 8 bits
		if (haveSound)
		{
			read.sound16.resize((size_t)read.frameCount * kVideoImportSamplesPerFrame, 0);
			VideoImport_SoundTo8(read.sound16.data(), read.sound16.size(), &read.sound);
		}

		if (progress && toldPercent < 100 && !progress(100))
		{
			return Refuse(VideoImportResult::Stopped, "Import video: the read was stopped", why);
		}

		*clip = std::move(read);

		if (why)
		{
			why->clear();
		}

		return VideoImportResult::Done;
	}

	VideoImportResult VideoImport_Still(std::vector<uint8_t> const &bytes, wchar_t const *contentType, int64_t at, uint32_t mostSide, std::vector<uint8_t> *rgb, uint32_t *width,
		uint32_t *height, std::string *why)
	{
		if (!rgb || !width || !height || mostSide == 0 || at < 0)
		{
			return Refuse(VideoImportResult::BadCall, "Import video: a still needs a place, a size and a time from the start", why);
		}

		ComSession const com;
		MediaFoundationSession const mediaFoundation;
		VideoImportResult result = CheckBytes(bytes, contentType, rgb, mediaFoundation, why);
		Opened opened;

		if (result == VideoImportResult::Done)
		{
			result = OpenForRead(bytes, contentType, kVideoImportLongestSeconds, &opened, why);
		}

		if (result != VideoImportResult::Done)
		{
			return result;
		}

		if (opened.length > 0 && at >= opened.length)
		{
			return Refuse(VideoImportResult::NotAVideo, "Import video: there is no picture that late in it", why);
		}

		PROPVARIANT position;

		PropVariantInit(&position);
		position.vt = VT_I8;
		position.hVal.QuadPart = at;

		HRESULT const moved = opened.reader->SetCurrentPosition(GUID_NULL, position);

		PropVariantClear(&position);

		if (FAILED(moved))
		{
			return Refuse(VideoImportResult::BadPicture, "Import video: Windows can't go to that time in it (" + Code(moved) + ")", why);
		}

		// the seek let the chain's held pictures go: the decoder starts again at the key picture the reader lands on (card 2c: the editor drives it)
		PictureChain &picture = *opened.picture;

		picture.Flush();

		// the first picture at or after the time (a seek lands on the key picture before it, so the ones before are passed over); at most a few seconds' worth
		bool found = false, made = false;
		auto const take = [&](IMFSample *decoded) {
			LONGLONG start = 0;

			if (!found && SUCCEEDED(decoded->GetSampleTime(&start)) && start + kVideoImportTimeUnitsPerSecond / (2 * kVideoImportFramesPerSecond) >= at)
			{
				found = true;
				made = WithPicture(decoded, picture.shape, [&](uint8_t const *top, int64_t pitch) { return StillOf(top, pitch, picture.shape, mostSide, rgb, width, height); });
			}

			return true;
		};

		for (int tries = 0; tries < 2000 && !found; tries++)
		{
			DWORD streamIndex = 0, flags = 0;
			LONGLONG start = 0;
			ComPtr<IMFSample> sample;
			HRESULT const hr = opened.reader->ReadSample(opened.pictureStream, 0, &streamIndex, &flags, &start, &sample);

			if (FAILED(hr) || (flags & MF_SOURCE_READERF_ERROR))
			{
				return Refuse(VideoImportResult::BadPicture, "Import video: Windows stopped reading its picture (" + Code(hr) + ")", why);
			}

			if (flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED)
			{
				return Refuse(VideoImportResult::BadPicture, "Import video: its picture's format changes part way through; it isn't read", why);
			}

			if (sample && !picture.Push(sample.Get(), take))
			{
				return Refuse(VideoImportResult::BadPicture, "Import video: Windows' decoder stopped on its picture" + (picture.why.empty() ? std::string() : " (" + picture.why + ")"), why);
			}

			if (!found && (flags & MF_SOURCE_READERF_ENDOFSTREAM))
			{
				if (!picture.Drain(take))
				{
					return Refuse(VideoImportResult::BadPicture, "Import video: Windows' decoder stopped on its picture" + (picture.why.empty() ? std::string() : " (" + picture.why + ")"), why);
				}

				break;
			}
		}

		if (found)
		{
			if (made && why)
			{
				why->clear();
			}

			return made ? VideoImportResult::Done : Refuse(VideoImportResult::BadPicture, "Import video: a decoded picture is shorter than its size says", why);
		}

		return Refuse(VideoImportResult::NotAVideo, "Import video: there is no picture that late in it", why);
	}

	VideoImportResult VideoImport_Read(std::wstring const &path, VideoImportFit fit, VideoImportClip *clip, std::string *why, int64_t mostSeconds)
	{
		if (!clip)
		{
			return Refuse(VideoImportResult::BadCall, "Import video: no place for the clip", why);
		}

		std::vector<uint8_t> bytes;
		VideoImportResult const read = VideoImport_ReadFile(path, &bytes, why);

		if (read != VideoImportResult::Done)
		{
			return read;
		}

		return VideoImport_ReadBytes(bytes, VideoImport_ContentType(path), fit, clip, why, mostSeconds);
	}
}
