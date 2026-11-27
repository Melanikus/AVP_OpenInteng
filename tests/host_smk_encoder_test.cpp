// The test for src/host/smk_encoder: a user's video handed to the Smacker encoder DLL and written out (the owner's additions, 11-18). Written 2026-11-18 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: the encoder is a DLL called through three plain C calls, so a field given wrong (a size, a rate, the sound's bytes) or its bytes freed twice or never would go
// unseen until a film played wrong. Stand-ins for its three calls here record what they were given. Checked: what the encoder is handed for a clip with sound and one
// without; its answer kept and freed once; its refusal passed on in its own words; a module without the calls refused; the header line exactly as smk-info.js prints
// it (the owner's Cartman line), and for short, foreign and impossible headers; a new file never written over one that is there; a replace that keeps the old file
// when it can't be done.
//
// Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "smk_encoder.h"

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

// ---- Stand-ins for libsmackerenc.dll's three calls: they keep what they were given

static host::SmackerEncodeInput given;
static int givenCount = 0;
static int answerWith = 0;
static uint8_t *handedOut = nullptr;
static int freedCount = 0;
static uint8_t *freedLast = nullptr;

static int __cdecl FakeEncode(host::SmackerEncodeInput const *in, uint8_t **smk, size_t *smkBytes)
{
	given = *in;
	givenCount++;

	if (answerWith != 0)
	{
		*smk = nullptr;
		*smkBytes = 0;
		return answerWith;
	}

	handedOut = new uint8_t[6]{ 'S', 'M', 'K', '2', 7, 9 };
	*smk = handedOut;
	*smkBytes = 6;
	return 0;
}

static void __cdecl FakeFree(uint8_t *smk)
{
	freedCount++;
	freedLast = smk;
	delete[] smk;
}

static char const *__cdecl FakeWords(int result)
{
	return result == 3 ? "the frame count is wrong" : "?";
}

static host::VideoImportClip Clip(int32_t frames, bool sound)
{
	host::VideoImportClip clip;

	clip.frameCount = frames;
	clip.frames.assign((size_t)frames * host::kVideoImportFrameBytes, 40);

	if (sound)
	{
		clip.sound.assign((size_t)frames * host::kVideoImportSamplesPerFrame, 128);
	}

	return clip;
}

static void TestEncode()
{
	puts("Handing a clip to the encoder:");

	host::SmkEncoder encoder;
	std::string why;

	encoder.encode = FakeEncode;
	encoder.freeEncoded = FakeFree;
	encoder.words = FakeWords;

	host::VideoImportClip const clip = Clip(30, true);
	std::vector<uint8_t> smk;

	Check(host::SmkEncoder_Encode(encoder, clip, &smk, &why), "a 30-frame clip with sound is encoded (" + why + ")");
	Check(given.width == 128 && given.height == 96 && given.frameCount == 30 && given.fps == 15, "the encoder is given 128 x 96, 30 frames, 15 a second");
	Check(given.rgb == clip.frames.data() && given.rgbBytes == (size_t)30 * 128 * 96 * 3, "the frames, all 30 x 128 x 96 x 3 bytes of them, as the clip holds them");
	Check(given.sampleRate == 22050 && given.pcm == clip.sound.data() && given.pcmBytes == (size_t)30 * 1470, "the sound: 22050 Hz, all 30 x 1470 samples");
	Check(smk.size() == 6 && smk[0] == 'S' && smk[5] == 9, "its answer is kept, byte for byte");
	Check(freedCount == 1 && freedLast == handedOut, "the encoder's bytes freed once, by its own call");

	host::VideoImportClip const silent = Clip(15, false);

	Check(host::SmkEncoder_Encode(encoder, silent, &smk, &why) && given.pcm == nullptr && given.pcmBytes == 0, "a clip with no sound: no sound handed (null, 0 bytes)");

	answerWith = 3;
	freedCount = 0;
	why.clear();
	smk.assign(2, 1);
	Check(!host::SmkEncoder_Encode(encoder, clip, &smk, &why) && why == "the encoder says: the frame count is wrong (3)" && smk.size() == 2,
		"the encoder's refusal passed on in its own words, the bytes untouched (" + why + ")");
	answerWith = 0;

	int const before = givenCount;
	host::VideoImportClip broken = Clip(2, true);

	broken.frames.pop_back();
	Check(!host::SmkEncoder_Encode(encoder, broken, &smk, &why) && givenCount == before, "frames a byte short of their count: refused before the encoder is called");
	Check(!host::SmkEncoder_Encode(encoder, host::VideoImportClip(), &smk, &why) && givenCount == before, "no frames: refused before the encoder is called");

	host::SmkEncoder empty;

	Check(!host::SmkEncoder_Encode(empty, clip, &smk, &why), "no encoder: refused");

}

static void Put32(std::vector<uint8_t> &header, size_t at, uint32_t value)
{
	header[at] = (uint8_t)value;
	header[at + 1] = (uint8_t)(value >> 8);
	header[at + 2] = (uint8_t)(value >> 16);
	header[at + 3] = (uint8_t)(value >> 24);
}

static std::vector<uint8_t> Header(char const *signature, uint32_t width, uint32_t height, uint32_t frames, int32_t rate, uint32_t flags, uint32_t track0)
{
	std::vector<uint8_t> header(104, 0);

	memcpy(header.data(), signature, 4);
	Put32(header, 4, width);
	Put32(header, 8, height);
	Put32(header, 12, frames);
	Put32(header, 16, (uint32_t)rate);
	Put32(header, 20, flags);
	Put32(header, 72, track0);
	return header;
}

static void TestHeaderLine()
{
	puts("The header line, as smk-info.js prints it:");

	uint32_t const compressed8Mono = 0x80000000u | 0x40000000u | 22050u;

	Check(host::Smk_HeaderLine(Header("SMK2", 128, 96, 609, -6666, 0, compressed8Mono), "Cartman.smk") ==
		"Cartman.smk: SMK2 128 x 96, 609 frames at 15.00 fps (40.6 s), flags none; track 0: 22050 Hz, 8-bit, mono, compressed",
		"the owner's film: \"Cartman.smk: SMK2 128 x 96, 609 frames at 15.00 fps (40.6 s), flags none; track 0: 22050 Hz, 8-bit, mono, compressed\"");
	Check(host::Smk_HeaderLine(Header("SMK4", 320, 200, 10, 100, 3, 0x40000000u | 0x20000000u | 0x10000000u | 44100u), "a.smk") ==
		"a.smk: SMK4 320 x 200, 10 frames at 10.00 fps (1.0 s), flags ring frame, Y interlaced; track 0: 44100 Hz, 16-bit, stereo, raw",
		"SMK4, 100 ms a frame, two flags, a 16-bit stereo raw track");
	Check(host::Smk_HeaderLine(Header("SMK2", 64, 64, 5, 0, 0, 0), "b.smk") == "b.smk: SMK2 64 x 64, 5 frames at 10.00 fps (0.5 s), flags none; no audio", "rate 0 is 10 a second; no track: no audio");
	Check(host::Smk_HeaderLine(std::vector<uint8_t>(50, 0), "c.smk") == "c.smk: too short for a Smacker header (50 bytes)", "50 bytes: too short");
	Check(host::Smk_HeaderLine(Header("AB\x01\x7f", 1, 1, 1, 1, 0, 0), "d.smk") == "d.smk: not a Smacker file (starts \"AB??\")", "a foreign start, its unprintable letters shown as ?");
	Check(host::Smk_HeaderLine(Header("SMK2", 0, 96, 1, 1, 0, 0), "e.smk").find("impossible numbers") != std::string::npos &&
		host::Smk_HeaderLine(Header("SMK2", 5000, 96, 1, 1, 0, 0), "e.smk").find("impossible numbers") != std::string::npos, "a width of 0 or past 4096: impossible numbers");
}

static std::vector<uint8_t> ReadAll(std::wstring const &path)
{
	std::vector<uint8_t> bytes;
	FILE *file = nullptr;

	if (_wfopen_s(&file, path.c_str(), L"rb") == 0 && file)
	{
		uint8_t buffer[256];
		size_t got;

		while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0)
		{
			bytes.insert(bytes.end(), buffer, buffer + got);
		}

		fclose(file);
	}

	return bytes;
}

static void TestFiles(std::wstring const &folder)
{
	puts("Writing the film:");

	std::wstring const path = folder + L"\\message101.smk";
	std::vector<uint8_t> const first = { 1, 2, 3 };
	std::vector<uint8_t> const second = { 9, 8, 7, 6 };
	std::string why;

	Check(host::Smk_WriteNew(path, first, &why) && ReadAll(path) == first, "a new film is written, every byte");
	why.clear();

	bool const refused = !host::Smk_WriteNew(path, second, &why);

	Check(refused && why.find("already exists") != std::string::npos && ReadAll(path) == first, "never over one that is there: refused, the old film untouched (" + why + ")");
	Check(host::Smk_Replace(path, second, &why) && ReadAll(path) == second, "replaced, when asked: the new bytes whole");

	WIN32_FIND_DATAW found;
	HANDLE const search = FindFirstFileW((folder + L"\\*.part-*").c_str(), &found);

	Check(search == INVALID_HANDLE_VALUE, "no part-written file left beside it");

	if (search != INVALID_HANDLE_VALUE)
	{
		FindClose(search);
	}

	Check(!host::Smk_Replace(folder + L"\\no such folder\\message5.smk", second, &why), "a replace into a folder that isn't there: refused");
	DeleteFileW(path.c_str());
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

	std::wstring const folder = std::wstring(temp) + L"host_smk_encoder_test_" + std::to_wstring(GetCurrentProcessId());

	if (!CreateDirectoryW(folder.c_str(), nullptr))
	{
		puts("  FAILED  the test's folder can't be made");
		return 1;
	}

	TestEncode();
	TestHeaderLine();
	TestFiles(folder);
	RemoveDirectoryW(folder.c_str());
	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
