// The test for src/host/video_import: Import Video's parts that need no Media Foundation (the owner's addition 11-18, Bishop's order). Written 2026-11-18 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: the user's file is hostile input, and the frames go straight into a Smacker encoder, so a slip here is either a read of something that isn't the user's video (a device, a
// link to elsewhere, a file's hidden stream, a file too big for memory) or a picture squashed, upside down, off by a row or with stray bytes at its edges. This checks, on files the
// test makes in its own folder: the names taken and refused, a regular file read whole, a hard link read (it is the file itself), and refused with the reason: a missing file, a folder,
// a symbolic link (when this machine lets one be made), device names (NUL, CON.mp4, COM1.mp4, \\.\NUL), a file's own stream, \\?\ paths, a name with a 0 in it, an empty file and one a
// byte over the bound. Then the stream's bounds; the letterbox's area for wide, square, tall, tiny and non-square-pixel pictures; its pixels (a solid colour exact, the bars exactly
// black, a left / right split and a top / bottom split kept where they are, a picture stored bottom row first, a row with padding, an average of two columns); the 15-a-second ticks
// (a 30-a-second clip, a late first picture, the exact boundary, hostile times); and the sound's 8 bits.
//
// Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "video_import.h"

static int checks = 0;
static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checks++;

	if (!good)
	{
		checksFailed++;
	}
}

static std::wstring folderOfTest;

static std::wstring InFolder(wchar_t const *name)
{
	return folderOfTest + L"\\" + name;
}

static bool WriteBytes(std::wstring const &path, std::vector<uint8_t> const &bytes)
{
	HANDLE const file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DWORD written = 0;
	BOOL const good = bytes.empty() || WriteFile(file, bytes.data(), (DWORD)bytes.size(), &written, nullptr);

	CloseHandle(file);
	return good && written == (DWORD)bytes.size();
}

// A file of `size` bytes made without writing them (its end set): for the byte bound
static bool MakeLongFile(std::wstring const &path, uint64_t size)
{
	HANDLE const file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);

	if (file == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	LARGE_INTEGER at;

	at.QuadPart = (LONGLONG)size;

	bool const good = SetFilePointerEx(file, at, nullptr, FILE_BEGIN) && SetEndOfFile(file);

	CloseHandle(file);
	return good;
}

static void TestNames()
{
	puts("The names taken:");

	Check(host::VideoImport_ContentType(L"C:\\a\\clip.mp4") != nullptr && wcscmp(host::VideoImport_ContentType(L"C:\\a\\clip.mp4"), L"video/mp4") == 0, ".mp4 is video/mp4");
	Check(host::VideoImport_ContentType(L"C:\\a\\CLIP.MP4") != nullptr, "capitals are taken (.MP4)");
	Check(host::VideoImport_ContentType(L"clip.m4v") != nullptr && wcscmp(host::VideoImport_ContentType(L"clip.m4v"), L"video/mp4") == 0, ".m4v is video/mp4");
	Check(host::VideoImport_ContentType(L"clip.avi") != nullptr && wcscmp(host::VideoImport_ContentType(L"clip.avi"), L"video/avi") == 0, ".avi is video/avi");
	Check(host::VideoImport_ContentType(L"clip.wmv") != nullptr && wcscmp(host::VideoImport_ContentType(L"clip.wmv"), L"video/x-ms-wmv") == 0, ".wmv is video/x-ms-wmv");
	Check(host::VideoImport_ContentType(L"clip.mkv") == nullptr, ".mkv is refused (not one of the four)");
	Check(host::VideoImport_ContentType(L"clip.mp4.exe") == nullptr, "clip.mp4.exe is refused (only the last ending counts)");
	Check(host::VideoImport_ContentType(L"mp4") == nullptr && host::VideoImport_ContentType(L"") == nullptr, "a name with no ending is refused");
	Check(host::VideoImport_Words(host::VideoImportResult::TooBig) != nullptr && strlen(host::VideoImport_Words(host::VideoImportResult::TooBig)) > 0, "every answer has words (TooBig)");
}

static void TestFiles()
{
	puts("The file, judged and read:");

	std::vector<uint8_t> clip(1000);

	for (size_t i = 0; i < clip.size(); i++)
	{
		clip[i] = (uint8_t)(i * 7 + 3);
	}

	std::wstring const clipPath = InFolder(L"clip.mp4");
	std::vector<uint8_t> bytes;
	std::string why;

	Check(WriteBytes(clipPath, clip), "(the test's clip.mp4 is made)");
	Check(host::VideoImport_ReadFile(clipPath, &bytes, &why) == host::VideoImportResult::Done && bytes == clip, "a regular file is read whole, byte for byte");

	std::wstring const hardPath = InFolder(L"hard.mp4");

	bytes.clear();
	Check(CreateHardLinkW(hardPath.c_str(), clipPath.c_str(), nullptr) && host::VideoImport_ReadFile(hardPath, &bytes, &why) == host::VideoImportResult::Done && bytes == clip,
		"a hard link is read (it is the file itself, not a link to elsewhere)");

	bytes.assign(3, 9);
	why.clear();
	Check(host::VideoImport_ReadFile(InFolder(L"missing.mp4"), &bytes, &why) == host::VideoImportResult::NotAFile && !why.empty() && bytes.size() == 3, "a missing file is refused, with words, *bytes untouched");

	std::wstring const folderPath = InFolder(L"folder.mp4");

	Check(CreateDirectoryW(folderPath.c_str(), nullptr) && host::VideoImport_ReadFile(folderPath, &bytes, &why) == host::VideoImportResult::NotAFile, "a folder (named folder.mp4) is refused");
	RemoveDirectoryW(folderPath.c_str());

	std::wstring const linkPath = InFolder(L"link.mp4");

	if (CreateSymbolicLinkW(linkPath.c_str(), clipPath.c_str(), SYMBOLIC_LINK_FLAG_ALLOW_UNPRIVILEGED_CREATE))
	{
		why.clear();

		bool const refused = host::VideoImport_ReadFile(linkPath, &bytes, &why) == host::VideoImportResult::NotAFile;

		Check(refused && !why.empty(), "a symbolic link to a good clip is refused (" + why + ")");
		DeleteFileW(linkPath.c_str());
	}
	else
	{
		printf("  SKIPPED  a symbolic link is refused: this machine won't make one (no Developer Mode, no admin), so it is not checked here\n");
	}

	// The same refusal without a privilege: a file given a reparse point of a third party's kind (as a cloud file or a link is), which marks its handle as a symbolic link's does
	std::wstring const taggedPath = InFolder(L"tagged.mp4");

	if (WriteBytes(taggedPath, clip))
	{
		HANDLE const tagged = CreateFileW(taggedPath.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_FLAG_OPEN_REPARSE_POINT | FILE_FLAG_BACKUP_SEMANTICS, nullptr);
		bool made = false;

		if (tagged != INVALID_HANDLE_VALUE)
		{
			REPARSE_GUID_DATA_BUFFER point = {};
			DWORD returned = 0;

			point.ReparseTag = 0x00000123;   // not Microsoft's (the high bit clear): a third party's
			point.ReparseDataLength = 0;
			point.ReparseGuid = { 0x6f1e6d7a, 0x2b4c, 0x4d1a, { 0x9e, 0x51, 0x0a, 0x7c, 0x3b, 0x22, 0x18, 0x55 } };
			made = DeviceIoControl(tagged, FSCTL_SET_REPARSE_POINT, &point, REPARSE_GUID_DATA_BUFFER_HEADER_SIZE, nullptr, 0, &returned, nullptr) != 0;
			CloseHandle(tagged);
		}

		if (made)
		{
			why.clear();

			bool const refused = host::VideoImport_ReadFile(taggedPath, &bytes, &why) == host::VideoImportResult::NotAFile;

			Check(refused && why.find("link") != std::string::npos, "a file with a reparse point (a link's mark) is refused as a link (" + why + ")");
		}
		else
		{
			printf("  SKIPPED  a file with a reparse point is refused: this machine won't set one (error %lu)\n", GetLastError());
		}

		DeleteFileW(taggedPath.c_str());
	}

	Check(host::VideoImport_ReadFile(L"NUL", &bytes, &why) == host::VideoImportResult::NotAVideoName, "NUL (no ending) is refused by its name first");
	Check(host::VideoImport_ReadFile(L"NUL.mp4", &bytes, &why) == host::VideoImportResult::NotAFile, "NUL.mp4 (the NUL device) is refused");
	Check(host::VideoImport_ReadFile(InFolder(L"CON.mp4"), &bytes, &why) == host::VideoImportResult::NotAFile, "CON.mp4 in a folder (a device name) is refused");
	Check(host::VideoImport_ReadFile(InFolder(L"com1.MP4"), &bytes, &why) == host::VideoImportResult::NotAFile, "com1.MP4 (a device name, any capitals) is refused");
	Check(host::VideoImport_ReadFile(InFolder(L"lpt9 .wmv"), &bytes, &why) == host::VideoImportResult::NotAFile, "\"lpt9 .wmv\" (a device name with a space before the ending) is refused");
	Check(host::VideoImport_ReadFile(L"\\\\.\\NUL.mp4", &bytes, &why) == host::VideoImportResult::NotAFile, "\\\\.\\NUL.mp4 (a device path) is refused");
	Check(host::VideoImport_ReadFile(L"\\\\?\\" + clipPath, &bytes, &why) == host::VideoImportResult::NotAFile, "a \\\\?\\ path is refused, even to a good clip");
	Check(host::VideoImport_ReadFile(clipPath + L":hidden.mp4", &bytes, &why) == host::VideoImportResult::NotAFile, "a file's own stream (clip.mp4:hidden.mp4) is refused");

	std::wstring withZero = InFolder(L"clip");

	withZero.push_back(L'\0');
	withZero += L".mp4";
	Check(host::VideoImport_ReadFile(withZero, &bytes, &why) == host::VideoImportResult::NotAFile, "a name with a 0 in it is refused (it would name another file)");

	std::wstring const emptyPath = InFolder(L"empty.mp4");

	Check(WriteBytes(emptyPath, std::vector<uint8_t>()) && host::VideoImport_ReadFile(emptyPath, &bytes, &why) == host::VideoImportResult::Empty, "an empty file is refused");

	std::wstring const bigPath = InFolder(L"big.avi");

	why.clear();

	bool const tooBig = MakeLongFile(bigPath, host::kVideoImportMostBytes + 1) && host::VideoImport_ReadFile(bigPath, &bytes, &why) == host::VideoImportResult::TooBig;

	Check(tooBig && !why.empty(), "a file a byte over the bound is refused, with words (" + why + ")");

	std::wstring const atPath = InFolder(L"at.avi");

	Check(MakeLongFile(atPath, 4096) && host::VideoImport_ReadFile(atPath, &bytes, &why) == host::VideoImportResult::Done && bytes.size() == 4096, "a file of 4096 bytes of 0 is read (the bound is on size, not on content)");

	// the bound given per read (mp4-to-smk --max-input-mb): a byte under the file refuses it, with words saying how to raise it; the file's own size takes it; 0 and past the most are bad calls
	why.clear();

	bool const underRefused = host::VideoImport_ReadFile(atPath, &bytes, &why, 4095) == host::VideoImportResult::TooBig;

	Check(underRefused && why.find("--max-input-mb") != std::string::npos, "a bound of 4095 bytes refuses the 4096-byte file, the words saying how to raise it (" + why + ")");
	Check(host::VideoImport_ReadFile(atPath, &bytes, &why, 4096) == host::VideoImportResult::Done && bytes.size() == 4096, "a bound of exactly 4096 bytes takes it");
	Check(host::VideoImport_ReadFile(atPath, &bytes, &why, 0) == host::VideoImportResult::BadCall &&
		host::VideoImport_ReadFile(atPath, &bytes, &why, host::kVideoImportLongestBytes + 1) == host::VideoImportResult::BadCall, "a bound of 0, or past the most (2048 MB): a bad call, never a bigger read");

	Check(host::VideoImport_ReadFile(InFolder(L"clip.txt"), &bytes, &why) == host::VideoImportResult::NotAVideoName, "clip.txt is refused by its name");
	Check(host::VideoImport_ReadFile(std::wstring(), &bytes, &why) == host::VideoImportResult::NotAVideoName, "no name at all is refused");
	Check(host::VideoImport_ReadFile(clipPath, nullptr, &why) == host::VideoImportResult::BadCall, "no place for the bytes: BadCall");

	DeleteFileW(clipPath.c_str());
	DeleteFileW(hardPath.c_str());
	DeleteFileW(emptyPath.c_str());
	DeleteFileW(bigPath.c_str());
	DeleteFileW(atPath.c_str());
}

static void TestStream()
{
	puts("The stream's bounds:");

	std::string why;
	int64_t const second = host::kVideoImportTimeUnitsPerSecond;

	Check(host::VideoImport_CheckStream(1920, 1080, 60 * second, &why) == host::VideoImportResult::Done, "1920 x 1080, a minute: taken");
	Check(host::VideoImport_CheckStream(4096, 4096, host::kVideoImportMostSeconds * second, &why) == host::VideoImportResult::Done, "4096 x 4096 and exactly the most seconds: taken");
	Check(host::VideoImport_CheckStream(4097, 100, second, &why) == host::VideoImportResult::TooLarge, "4097 wide: refused");
	Check(host::VideoImport_CheckStream(100, 4097, second, &why) == host::VideoImportResult::TooLarge, "4097 tall: refused");
	Check(host::VideoImport_CheckStream(0, 100, second, &why) == host::VideoImportResult::TooLarge, "0 wide: refused");
	Check(host::VideoImport_CheckStream(100, 100, host::kVideoImportMostSeconds * second + 1, &why) == host::VideoImportResult::TooLong && !why.empty(), "100 ns over the most seconds: refused, with words");
	Check(host::VideoImport_CheckStream(100, 100, -1, &why) == host::VideoImportResult::Done, "a length the file doesn't give (-1): taken (the read holds the bound itself)");
	Check(host::VideoImport_CheckStream(1294, 940, 2425000000ll, &why) == host::VideoImportResult::TooLong && why.find("--max-seconds") != std::string::npos,
		"242.5 s (the owner's longer test) under the default 180: refused, the words saying how to raise it");
	Check(host::VideoImport_CheckStream(1294, 940, 2425000000ll, &why, 300) == host::VideoImportResult::Done, "the same with the most seconds raised to 300: taken");
	Check(host::VideoImport_CheckStream(100, 100, host::kVideoImportLongestSeconds * second, &why, host::kVideoImportLongestSeconds) == host::VideoImportResult::Done &&
		host::VideoImport_CheckStream(100, 100, host::kVideoImportLongestSeconds * second + 1, &why, host::kVideoImportLongestSeconds) == host::VideoImportResult::TooLong, "raised to the most (600 s): exactly 600 s taken, 100 ns more refused");
	Check(host::VideoImport_CheckStream(100, 100, second, &why, 0) == host::VideoImportResult::BadCall && host::VideoImport_CheckStream(100, 100, second, &why, host::kVideoImportLongestSeconds + 1) == host::VideoImportResult::BadCall,
		"a most of 0, or past 600 s: a bad call, never a longer read");
}

// A picture of B, G, R, unused pixels, all one colour
static std::vector<uint8_t> Solid(uint32_t width, uint32_t height, uint8_t r, uint8_t g, uint8_t b)
{
	std::vector<uint8_t> picture((size_t)width * height * 4);

	for (size_t i = 0; i < picture.size(); i += 4)
	{
		picture[i] = b;
		picture[i + 1] = g;
		picture[i + 2] = r;
		picture[i + 3] = 0xee;   // the unused byte is never read as colour
	}

	return picture;
}

static void SetPixel(std::vector<uint8_t> &picture, uint32_t width, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b)
{
	size_t const at = ((size_t)y * width + x) * 4;

	picture[at] = b;
	picture[at + 1] = g;
	picture[at + 2] = r;
}

static bool OutIs(uint8_t const *out, int32_t x, int32_t y, uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t const *const p = out + ((size_t)y * host::kVideoImportWidth + x) * 3;

	return p[0] == r && p[1] == g && p[2] == b;
}

// Every pixel inside the area is (r, g, b) and every pixel outside it exactly black
static bool AreaIs(uint8_t const *out, int32_t const area[4], uint8_t r, uint8_t g, uint8_t b)
{
	for (int32_t y = 0; y < host::kVideoImportHeight; y++)
	{
		for (int32_t x = 0; x < host::kVideoImportWidth; x++)
		{
			bool const inside = x >= area[0] && x < area[0] + area[2] && y >= area[1] && y < area[1] + area[3];

			if (inside ? !OutIs(out, x, y, r, g, b) : !OutIs(out, x, y, 0, 0, 0))
			{
				return false;
			}
		}
	}

	return true;
}

static std::string AreaWords(int32_t const area[4])
{
	return std::to_string(area[0]) + " " + std::to_string(area[1]) + " " + std::to_string(area[2]) + " " + std::to_string(area[3]);
}

static void TestLetterboxArea()
{
	puts("The letterbox's area (left, top, width, height in 128 x 96):");

	int32_t area[4] = { -1, -1, -1, -1 };

	Check(host::VideoImport_LetterboxArea(320, 180, 1, 1, area) && area[0] == 0 && area[1] == 12 && area[2] == 128 && area[3] == 72, "320 x 180 (16:9): 0 12 128 72, black above and below (" + AreaWords(area) + ")");
	Check(host::VideoImport_LetterboxArea(160, 160, 1, 1, area) && area[0] == 16 && area[1] == 0 && area[2] == 96 && area[3] == 96, "160 x 160 (square): 16 0 96 96, black at the sides (" + AreaWords(area) + ")");
	Check(host::VideoImport_LetterboxArea(640, 480, 1, 1, area) && area[0] == 0 && area[1] == 0 && area[2] == 128 && area[3] == 96, "640 x 480 (4:3): the whole 128 x 96");
	Check(host::VideoImport_LetterboxArea(720, 480, 8, 9, area) && area[0] == 0 && area[1] == 0 && area[2] == 128 && area[3] == 96, "720 x 480 with 8:9 pixels (a 4:3 picture): the whole 128 x 96");
	Check(host::VideoImport_LetterboxArea(720, 480, 32, 27, area) && area[0] == 0 && area[1] == 12 && area[2] == 128 && area[3] == 72, "720 x 480 with 32:27 pixels (a 16:9 picture): 0 12 128 72 (" + AreaWords(area) + ")");
	Check(host::VideoImport_LetterboxArea(720, 480, 0, 0, area) && area[0] == 0 && area[1] == 5 && area[2] == 128 && area[3] == 85, "pixels 0:0 are taken as square (720 x 480 is 3:2: 0 5 128 85) (" + AreaWords(area) + ")");
	Check(host::VideoImport_LetterboxArea(1, 4000, 1, 1, area) && area[2] == 1 && area[3] == 96 && area[0] == 63, "1 x 4000: one column at least, in the middle (" + AreaWords(area) + ")");
	Check(host::VideoImport_LetterboxArea(2, 2, 1, 1, area) && area[0] == 16 && area[3] == 96, "2 x 2 grows to 96 x 96");
	Check(!host::VideoImport_LetterboxArea(0, 10, 1, 1, area) && !host::VideoImport_LetterboxArea(10, 4097, 1, 1, area), "a side of 0 or over 4096: refused");
	Check(!host::VideoImport_LetterboxArea(10, 10, 17, 1, area) && !host::VideoImport_LetterboxArea(10, 10, 1, 17, area), "pixels past 16:1 or 1:16: refused");
	Check(host::VideoImport_LetterboxArea(10, 10, 16, 1, area), "pixels of exactly 16:1: taken");
}

static void TestLetterboxPixels()
{
	puts("The letterbox's pixels:");

	std::vector<uint8_t> out(host::kVideoImportFrameBytes, 0x55);
	int32_t area[4];

	// a solid colour, 16:9
	std::vector<uint8_t> picture = Solid(320, 180, 30, 20, 10);

	Check(host::VideoImport_Letterbox(picture.data(), 320, 180, 320 * 4, 1, 1, host::VideoImportFit::Fit, out.data()) && host::VideoImport_LetterboxArea(320, 180, 1, 1, area) && AreaIs(out.data(), area, 30, 20, 10),
		"320 x 180 of (30, 20, 10): the area exactly that colour as R G B, the bars exactly black, the unused byte never read");

	// a solid colour made larger
	picture = Solid(64, 48, 200, 100, 50);
	Check(host::VideoImport_Letterbox(picture.data(), 64, 48, 64 * 4, 1, 1, host::VideoImportFit::Fit, out.data()) && host::VideoImport_LetterboxArea(64, 48, 1, 1, area) && AreaIs(out.data(), area, 200, 100, 50),
		"64 x 48 made larger: exactly its colour all over");

	// left red, right blue
	picture = Solid(256, 192, 255, 0, 0);

	for (uint32_t y = 0; y < 192; y++)
	{
		for (uint32_t x = 128; x < 256; x++)
		{
			SetPixel(picture, 256, x, y, 0, 0, 255);
		}
	}

	Check(host::VideoImport_Letterbox(picture.data(), 256, 192, 256 * 4, 1, 1, host::VideoImportFit::Fit, out.data()) && OutIs(out.data(), 0, 50, 255, 0, 0) && OutIs(out.data(), 63, 50, 255, 0, 0) && OutIs(out.data(), 64, 50, 0, 0, 255) &&
		OutIs(out.data(), 127, 50, 0, 0, 255), "256 x 192, left half red, right half blue: columns 0-63 red, 64-127 blue (the split kept where it is)");

	// top green, bottom blue, kept top row first
	picture = Solid(256, 192, 0, 255, 0);

	for (uint32_t y = 96; y < 192; y++)
	{
		for (uint32_t x = 0; x < 256; x++)
		{
			SetPixel(picture, 256, x, y, 0, 0, 255);
		}
	}

	Check(host::VideoImport_Letterbox(picture.data(), 256, 192, 256 * 4, 1, 1, host::VideoImportFit::Fit, out.data()) && OutIs(out.data(), 10, 0, 0, 255, 0) && OutIs(out.data(), 10, 47, 0, 255, 0) && OutIs(out.data(), 10, 48, 0, 0, 255) &&
		OutIs(out.data(), 10, 95, 0, 0, 255), "256 x 192, top half green, bottom half blue: rows 0-47 green, 48-95 blue (top row first)");

	// the same picture kept bottom row first: the top row is the last in memory, the pitch negative
	std::vector<uint8_t> flipped(picture.size());
	size_t const row = 256 * 4;

	for (uint32_t y = 0; y < 192; y++)
	{
		memcpy(flipped.data() + (191 - y) * row, picture.data() + y * row, row);
	}

	Check(host::VideoImport_Letterbox(flipped.data() + 191 * row, 256, 192, -(int64_t)row, 1, 1, host::VideoImportFit::Fit, out.data()) && OutIs(out.data(), 10, 0, 0, 255, 0) && OutIs(out.data(), 10, 95, 0, 0, 255),
		"the same picture kept bottom row first (a negative pitch): still green at the top, blue at the bottom");

	// rows with 12 bytes of padding after each, the padding a loud colour
	uint32_t const padded = 100 * 4 + 12;
	std::vector<uint8_t> withPadding((size_t)padded * 75, 0xff);

	for (uint32_t y = 0; y < 75; y++)
	{
		for (uint32_t x = 0; x < 100; x++)
		{
			uint8_t *const p = withPadding.data() + (size_t)y * padded + x * 4;

			p[0] = 3;
			p[1] = 2;
			p[2] = 1;
			p[3] = 0;
		}
	}

	Check(host::VideoImport_Letterbox(withPadding.data(), 100, 75, padded, 1, 1, host::VideoImportFit::Fit, out.data()) && host::VideoImport_LetterboxArea(100, 75, 1, 1, area) && AreaIs(out.data(), area, 1, 2, 3),
		"100 x 75 with 12 bytes of padding a row: exactly (1, 2, 3), the padding never read");

	// two columns to one: black and white columns average to the middle
	picture = Solid(256, 192, 0, 0, 0);

	for (uint32_t y = 0; y < 192; y++)
	{
		for (uint32_t x = 1; x < 256; x += 2)
		{
			SetPixel(picture, 256, x, y, 255, 255, 255);
		}
	}

	Check(host::VideoImport_Letterbox(picture.data(), 256, 192, 256 * 4, 1, 1, host::VideoImportFit::Fit, out.data()) && OutIs(out.data(), 0, 0, 128, 128, 128) && OutIs(out.data(), 127, 95, 128, 128, 128),
		"black and white columns, two to each pixel: each pixel their average, 127.5 to the nearest (128)");

	std::vector<uint8_t> const before(out);

	Check(!host::VideoImport_Letterbox(picture.data(), 256, 192, 256 * 4 - 1, 1, 1, host::VideoImportFit::Fit, out.data()) && out == before, "a pitch shorter than a row: refused, out untouched");
	Check(!host::VideoImport_Letterbox(picture.data(), 0, 192, 256 * 4, 1, 1, host::VideoImportFit::Fit, out.data()) && out == before, "a width of 0: refused, out untouched");
	Check(!host::VideoImport_Letterbox(nullptr, 256, 192, 256 * 4, 1, 1, host::VideoImportFit::Fit, out.data()) && out == before, "no picture: refused, out untouched");
}

static std::string WindowWords(double const window[4])
{
	char words[96];

	snprintf(words, sizeof(words), "%.2f %.2f %.2f %.2f", window[0], window[1], window[2], window[3]);
	return words;
}

static bool WindowIs(double const window[4], double left, double top, double width, double height)
{
	return fabs(window[0] - left) < 1e-9 && fabs(window[1] - top) < 1e-9 && fabs(window[2] - width) < 1e-9 && fabs(window[3] - height) < 1e-9;
}

static void TestCrop()
{
	puts("Crop to fill (the owner's other answer), and when to ask:");

	double window[4] = { -1, -1, -1, -1 };

	Check(host::VideoImport_CropWindow(320, 180, 1, 1, window) && WindowIs(window, 40, 0, 240, 180), "320 x 180 (16:9): its middle 240 x 180, 40 cut at each side (" + WindowWords(window) + ")");
	Check(host::VideoImport_CropWindow(160, 160, 1, 1, window) && WindowIs(window, 0, 20, 160, 120), "160 x 160 (square): its middle 160 x 120, 20 cut above and below (" + WindowWords(window) + ")");
	Check(host::VideoImport_CropWindow(640, 480, 1, 1, window) && WindowIs(window, 0, 0, 640, 480), "640 x 480 (4:3): all of it");
	Check(host::VideoImport_CropWindow(720, 480, 32, 27, window) && WindowIs(window, 90, 0, 540, 480), "720 x 480 with 32:27 pixels (16:9 seen): its middle 540 pixels, 90 cut at each side (" + WindowWords(window) + ")");
	Check(!host::VideoImport_CropWindow(0, 480, 1, 1, window), "a side of 0: refused");

	Check(host::VideoImport_IsAvpShape(640, 480, 1, 1) && host::VideoImport_IsAvpShape(128, 96, 1, 1) && host::VideoImport_IsAvpShape(720, 480, 8, 9), "4:3 pictures (and 720 x 480 with 8:9 pixels) need no question");
	Check(!host::VideoImport_IsAvpShape(320, 180, 1, 1) && !host::VideoImport_IsAvpShape(160, 160, 1, 1) && !host::VideoImport_IsAvpShape(161, 120, 1, 1),
		"16:9, square, and 161 x 120 (a row short of filling) ask");

	std::vector<uint8_t> out(host::kVideoImportFrameBytes, 0x55);
	int32_t const whole[4] = { 0, 0, host::kVideoImportWidth, host::kVideoImportHeight };

	// 320 x 180: 40 columns red at the left, 40 blue at the right, green between
	std::vector<uint8_t> picture = Solid(320, 180, 0, 255, 0);

	for (uint32_t y = 0; y < 180; y++)
	{
		for (uint32_t x = 0; x < 40; x++)
		{
			SetPixel(picture, 320, x, y, 255, 0, 0);
			SetPixel(picture, 320, 319 - x, y, 0, 0, 255);
		}
	}

	Check(host::VideoImport_Letterbox(picture.data(), 320, 180, 320 * 4, 1, 1, host::VideoImportFit::Crop, out.data()) && AreaIs(out.data(), whole, 0, 255, 0),
		"cropped, 320 x 180 with red and blue edges: all 128 x 96 exactly green, the edges cut, no bars");
	Check(host::VideoImport_Letterbox(picture.data(), 320, 180, 320 * 4, 1, 1, host::VideoImportFit::Fit, out.data()) && OutIs(out.data(), 0, 50, 255, 0, 0) && OutIs(out.data(), 127, 50, 0, 0, 255) &&
		OutIs(out.data(), 64, 0, 0, 0, 0), "fitted, the same picture: the red and blue edges kept, the bars black");

	// 160 x 160: 20 rows red at the top, 20 blue at the bottom, green between
	picture = Solid(160, 160, 0, 255, 0);

	for (uint32_t y = 0; y < 20; y++)
	{
		for (uint32_t x = 0; x < 160; x++)
		{
			SetPixel(picture, 160, x, y, 255, 0, 0);
			SetPixel(picture, 160, x, 159 - y, 0, 0, 255);
		}
	}

	Check(host::VideoImport_Letterbox(picture.data(), 160, 160, 160 * 4, 1, 1, host::VideoImportFit::Crop, out.data()) && AreaIs(out.data(), whole, 0, 255, 0),
		"cropped, 160 x 160 with red and blue top and bottom: all exactly green");
}

static void TestTicks()
{
	puts("The 15-a-second ticks:");

	int64_t const second = host::kVideoImportTimeUnitsPerSecond;
	int64_t next = 0;
	int64_t shown = 0;

	// a 30-a-second clip of 2 s: the first picture at 0 fills nothing before it; each next fills what came before it; the end fills the rest
	shown += host::VideoImport_TicksBefore(&next, 0);

	int64_t total = 0;

	for (int i = 1; i < 60; i++)
	{
		total += host::VideoImport_TicksBefore(&next, (int64_t)i * second / 30);
	}

	total += host::VideoImport_TicksBefore(&next, 2 * second);
	Check(shown == 0 && total == 30 && next == 30, "a 30-a-second clip of 2 s: 30 ticks in all (" + std::to_string(total) + "), none before the first picture");

	next = 0;
	Check(host::VideoImport_TicksBefore(&next, second / 2) == 8 && next == 8, "a first picture at 0.5 s: ticks 0-7 (0 .. 0.467 s) come before it, the first picture fills them");

	next = 0;
	Check(host::VideoImport_TicksBefore(&next, second) == 15, "a picture at exactly 1 s: ticks 0-14 come before it; tick 15 (at 1 s) is its own");
	Check(host::VideoImport_TicksBefore(&next, second) == 0, "the same time again: no more ticks");
	Check(host::VideoImport_TicksBefore(&next, second / 2) == 0 && next == 15, "a time going back: no ticks, next unmoved");

	next = 0;
	Check(host::VideoImport_TicksBefore(&next, INT64_MAX) == (host::kVideoImportLongestSeconds + 1) * 15, "a hostile time (the largest): held to the longest read plus a second (" + std::to_string(next) + " ticks)");

	next = 0;
	Check(host::VideoImport_TicksBefore(&next, -5 * second) == 0 && next == 0, "a time before 0: no ticks");
}

static void TestSound()
{
	puts("The sound's 8 bits:");

	int16_t const samples[] = { 0, 32767, -32768, 127, 128, -129, -128 };
	std::vector<uint8_t> out(1, 77);

	host::VideoImport_SoundTo8(samples, sizeof(samples) / sizeof(samples[0]), &out);
	Check(out.size() == 8 && out[0] == 77, "the samples are appended (what was there kept)");
	Check(out.size() == 8 && out[1] == 128 && out[2] == 255 && out[3] == 0, "0 is 128 (silence), the loudest 255 and 0");
	Check(out.size() == 8 && out[4] == 128 && out[5] == 129 && out[6] == 127 && out[7] == 128, "each the nearest: 127 -> 128, 128 -> 129 (a half goes up), -129 -> 127, -128 -> 128");
}

static void TestLevel()
{
	puts("The sound's level (\"the same level of the game's videos\"):");

	// a quiet sine: amplitude 1000 of 32767 (RMS 707, 2.76 in 8-bit steps), as quiet as a soft source
	std::vector<int16_t> sine(22050);

	for (size_t i = 0; i < sine.size(); i++)
	{
		sine[i] = (int16_t)floor(1000.0 * sin(2.0 * 3.14159265358979323846 * 441.0 * (double)i / 22050.0) + 0.5);
	}

	double const gain = host::VideoImport_LevelGain(sine.data(), sine.size(), 20.0);
	std::vector<uint8_t> out;

	host::VideoImport_SoundTo8Gained(sine.data(), sine.size(), gain, &out);

	double const loudness = host::VideoImport_Loudness8(out);

	Check(fabs(gain - 20.0 * 256.0 / 707.1) < 0.05, "a quiet sine to loudness 20: the gain is 20 x 256 / its RMS (" + std::to_string(gain) + ")");
	Check(out.size() == sine.size() && fabs(loudness - 20.0) < 0.3, "after the gain its 8 bits measure 20 of 128 (" + std::to_string(loudness) + ")");

	double const held = host::VideoImport_LevelGain(sine.data(), sine.size(), 100.0);

	out.clear();
	host::VideoImport_SoundTo8Gained(sine.data(), sine.size(), held, &out);

	uint8_t lowest = 255, highest = 0;

	for (uint8_t const value : out)
	{
		lowest = value < lowest ? value : lowest;
		highest = value > highest ? value : highest;
	}

	int32_t loudest = 0;

	for (int16_t const value : sine)
	{
		loudest = abs((int32_t)value) > loudest ? abs((int32_t)value) : loudest;
	}

	Check(loudest > 900 && fabs(held - 32639.0 / loudest) < 1e-9, "to loudness 100 it would clip: the gain is held at 32639 / its loudest sample, " + std::to_string(loudest) + " (" + std::to_string(held) + ")");
	Check(highest <= 255 && lowest >= 1 && highest >= 250, "held: its loudest at the top of 8 bits, nothing past it (" + std::to_string(lowest) + " .. " + std::to_string(highest) + ")");

	std::vector<int16_t> const silence(1000, 0);

	Check(host::VideoImport_LevelGain(silence.data(), silence.size(), 20.0) == 1.0, "silence: gain 1 (nothing to bring up)");
	Check(host::VideoImport_LevelGain(sine.data(), 0, 20.0) == 1.0 && host::VideoImport_LevelGain(nullptr, 10, 20.0) == 1.0, "no samples: gain 1");
	Check(host::VideoImport_LevelGain(sine.data(), sine.size(), 0.0) == 1.0 && host::VideoImport_LevelGain(sine.data(), sine.size(), nan("")) == 1.0, "a target of 0 or not a number: gain 1");

	int16_t const loud[] = { 1000, 32767, -32768 };

	out.clear();
	host::VideoImport_SoundTo8Gained(loud, 3, 2.0, &out);
	Check(out.size() == 3 && out[0] == 136 && out[1] == 255 && out[2] == 0, "gain 2: 1000 -> 2000 -> 136; the loudest held at 255 and 0, never wrapped round");

	std::vector<uint8_t> const edges = { 0, 255 };

	Check(fabs(host::VideoImport_Loudness8(edges) - sqrt((128.0 * 128.0 + 127.0 * 127.0) / 2.0)) < 1e-9 && host::VideoImport_Loudness8(std::vector<uint8_t>()) == 0.0,
		"loudness of 0 and 255: their RMS about 128; of nothing: 0");
}

int main()
{
	wchar_t temp[MAX_PATH];
	DWORD const length = GetTempPathW(MAX_PATH, temp);

	if (length == 0 || length >= MAX_PATH)
	{
		puts("  FAILED  no folder for the test's files");
		return 1;
	}

	folderOfTest = std::wstring(temp) + L"host_video_import_test_" + std::to_wstring(GetCurrentProcessId());

	if (!CreateDirectoryW(folderOfTest.c_str(), nullptr))
	{
		puts("  FAILED  the test's folder can't be made");
		return 1;
	}

	TestNames();
	TestFiles();
	TestStream();
	TestLetterboxArea();
	TestLetterboxPixels();
	TestCrop();
	TestTicks();
	TestSound();
	TestLevel();
	RemoveDirectoryW(folderOfTest.c_str());
	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
