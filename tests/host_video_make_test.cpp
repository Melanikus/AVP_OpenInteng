// The test for src/host/video_make (what is done to a read video before it is encoded: the cut, the level and volume, the save with its backup), shared by mp4-to-smk
// and the editor's Video mode. Written 2026-11-22 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: the owner's Video mode (11-22) cuts a video's start and end and turns its volume, and a slip there is a film a frame short, its sound out of step with its
// picture, a sound that clips, or a film of his replaced and lost. Checked: the frame at a time; the cut's frames and exactly their 1470 samples a frame, in 8 and
// 16 bits; refused cuts leave the clip as it was; the level brought to AvP's and to the source's own, the volume's steps up and down, never past 8 bits; the
// volume in words; the save: a new file written, an existing one kept in the backup folder before it is replaced, a backup never written over.
//
// Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "video_make.h"

static int checks = 0;
static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	checks++;
	checksFailed += good ? 0 : 1;
}

// A clip of `frames` frames, frame k all of the value k (so a frame's place shows), with a sound whose sample i of frame k is a tone of the frame's own loudness
static host::VideoImportClip MadeClip(int32_t frames, int16_t loudest)
{
	host::VideoImportClip clip;

	clip.frameCount = frames;
	clip.frames.assign((size_t)frames * host::kVideoImportFrameBytes, 0);

	for (int32_t k = 0; k < frames; k++)
	{
		memset(clip.frames.data() + (size_t)k * host::kVideoImportFrameBytes, (int)(k & 0xff), (size_t)host::kVideoImportFrameBytes);

		for (int32_t i = 0; i < host::kVideoImportSamplesPerFrame; i++)
		{
			clip.sound16.push_back((int16_t)(loudest * sin(6.283185307179586 * 440.0 * (double)((int64_t)k * host::kVideoImportSamplesPerFrame + i) / 22050.0)));
		}
	}

	host::VideoImport_SoundTo8(clip.sound16.data(), clip.sound16.size(), &clip.sound);
	clip.sourceWidth = 320;
	clip.sourceHeight = 180;
	return clip;
}

static bool WriteText(std::wstring const &path, char const *text)
{
	HANDLE const file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
	DWORD wrote = 0;
	bool const done = file != INVALID_HANDLE_VALUE && WriteFile(file, text, (DWORD)strlen(text), &wrote, nullptr) && wrote == strlen(text);

	if (file != INVALID_HANDLE_VALUE)
	{
		CloseHandle(file);
	}

	return done;
}

static std::string ReadText(std::wstring const &path)
{
	HANDLE const file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
	char text[256] = {};
	DWORD got = 0;

	if (file != INVALID_HANDLE_VALUE)
	{
		ReadFile(file, text, sizeof(text) - 1, &got, nullptr);
		CloseHandle(file);
	}

	return std::string(text, got);
}

static void TestTimes()
{
	puts("A time's frame:");
	Check(host::VideoMake_FrameAt(1.0, 90) == 15 && host::VideoMake_FrameAt(4.0, 90) == 60, "1 s is frame 15 and 4 s frame 60 (15 a second)");
	Check(host::VideoMake_FrameAt(0.5, 90) == 8, "half a second is the nearest frame, 8 (7.5 rounded up)");
	Check(host::VideoMake_FrameAt(-3.0, 90) == 0 && host::VideoMake_FrameAt(100.0, 90) == 90, "a time before the start or past the end is held to the clip");
	Check(host::VideoMake_FrameAt(NAN, 90) == 0, "a time that isn't a number is the start");
}

static void TestCut()
{
	puts("The cut:");

	host::VideoImportClip clip = MadeClip(90, 8000);

	Check(host::VideoMake_Cut(&clip, 15, 60), "the cut from frame 15 to 60 is made");
	Check(clip.frameCount == 45 && clip.frames.size() == (size_t)45 * host::kVideoImportFrameBytes, "45 frames are left");
	Check(clip.frames[0] == 15 && clip.frames.back() == 59, "they are frames 15 to 59, the first and the last in their places");
	Check(clip.sound.size() == (size_t)45 * host::kVideoImportSamplesPerFrame && clip.sound16.size() == clip.sound.size(), "and exactly their sound, 1470 samples a frame, in 8 and 16 bits");

	host::VideoImportClip whole = MadeClip(90, 8000);

	Check(clip.sound16.front() == whole.sound16[(size_t)15 * host::kVideoImportSamplesPerFrame] && clip.sound.back() == whole.sound[(size_t)60 * host::kVideoImportSamplesPerFrame - 1],
		"its sound is frame 15's first sample to frame 59's last: in step with its picture");

	host::VideoImportClip const before = MadeClip(90, 8000);
	host::VideoImportClip refused = before;

	Check(!host::VideoMake_Cut(&refused, 60, 60) && !host::VideoMake_Cut(&refused, 61, 60) && !host::VideoMake_Cut(&refused, -1, 10) && !host::VideoMake_Cut(&refused, 0, 91) &&
		!host::VideoMake_Cut(nullptr, 0, 10), "an empty, backward or outside cut is refused");
	Check(refused.frameCount == 90 && refused.frames == before.frames && refused.sound == before.sound && refused.sound16 == before.sound16, "and leaves the clip as it was");

	host::VideoImportClip silent = MadeClip(30, 0);

	silent.sound.clear();
	silent.sound16.clear();
	Check(host::VideoMake_Cut(&silent, 5, 20) && silent.frameCount == 15 && silent.sound.empty(), "a clip with no sound is cut with no sound");
}

static void TestLevel()
{
	puts("The level and the volume:");

	host::VideoImportClip quiet = MadeClip(30, 2000);
	std::string words;

	host::VideoMake_Level(&quiet, host::kVideoMakeStockLevel, 0.0, &words);
	Check(fabs(host::VideoImport_Loudness8(quiet.sound) - host::kVideoMakeStockLevel) < 0.6, "a quiet sound is brought to AvP's level, 22.2 (" + words + ")");
	Check(words.find("sound level") != std::string::npos && words.find("target 22.2") != std::string::npos, "its words say so, as mp4-to-smk prints them");

	host::VideoImportClip up = MadeClip(30, 2000);
	host::VideoMake_Level(&up, host::kVideoMakeStockLevel, 6.0, nullptr);
	double const ratio = host::VideoImport_Loudness8(up.sound) / host::VideoImport_Loudness8(quiet.sound);

	Check(ratio > 1.9 && ratio < 2.1, "6 dB up is twice as loud (x" + std::to_string(ratio) + ")");

	host::VideoImportClip down = MadeClip(30, 2000);
	host::VideoMake_Level(&down, host::kVideoMakeStockLevel, -6.0, nullptr);
	double const half = host::VideoImport_Loudness8(down.sound) / host::VideoImport_Loudness8(quiet.sound);

	Check(half > 0.45 && half < 0.55, "6 dB down is half as loud (x" + std::to_string(half) + ")");

	host::VideoImportClip loud = MadeClip(30, 20000);
	host::VideoMake_Level(&loud, host::kVideoMakeStockLevel, 24.0, nullptr);
	// held: a sine that fits 8 bits touches 0 or 255 only near its peaks (about 6 in 100 samples); one clipped by a 16-times gain sits there most of the time
	size_t atTheEnds = 0;

	for (uint8_t s : loud.sound)
	{
		atTheEnds += s == 0 || s == 255 ? 1 : 0;
	}

	Check(atTheEnds * 10 < loud.sound.size(), "24 dB up on a loud sound is held so that it isn't clipped: " + std::to_string((unsigned long long)atTheEnds) + " of " +
		std::to_string((unsigned long long)loud.sound.size()) + " samples at 0 or 255");

	host::VideoImportClip own = MadeClip(30, 2000);
	double const ownBefore = host::VideoImport_Loudness8(own.sound);

	host::VideoMake_Level(&own, 0.0, 0.0, nullptr);
	Check(fabs(host::VideoImport_Loudness8(own.sound) - ownBefore) < 0.01, "the source's own level, no volume: the sound as it was");

	host::VideoImportClip ownUp = MadeClip(30, 2000);
	host::VideoMake_Level(&ownUp, 0.0, 6.0, nullptr);
	double const ownRatio = host::VideoImport_Loudness8(ownUp.sound) / ownBefore;

	Check(ownRatio > 1.9 && ownRatio < 2.1, "the source's own level, 6 dB up: twice as loud (x" + std::to_string(ownRatio) + ")");

	host::VideoImportClip beyond = MadeClip(30, 100);
	host::VideoMake_Level(&beyond, host::kVideoMakeStockLevel, 100.0, nullptr);
	host::VideoImportClip most = MadeClip(30, 100);
	host::VideoMake_Level(&most, host::kVideoMakeStockLevel, host::kVideoMakeMostVolumeDb, nullptr);
	Check(beyond.sound == most.sound, "a volume past +24 dB is held to +24 dB");

	host::VideoImportClip silent = MadeClip(10, 0);

	silent.sound.clear();
	silent.sound16.clear();
	host::VideoMake_Level(&silent, host::kVideoMakeStockLevel, 6.0, &words);
	Check(silent.sound.empty(), "a clip with no sound stays with none");

	Check(host::VideoMake_VolumeWords(false, 0.0) == "AvP's level", "the volume's words: AvP's level");
	Check(host::VideoMake_VolumeWords(false, 3.0) == "AvP's level +3 dB" && host::VideoMake_VolumeWords(false, -6.0) == "AvP's level -6 dB", "with its steps up and down");
	Check(host::VideoMake_VolumeWords(true, 0.0) == "the video's own level" && host::VideoMake_VolumeWords(true, 3.0) == "the video's own level +3 dB", "and the video's own level");
	Check(host::VideoMake_VolumeWords(false, 99.0) == "AvP's level +24 dB", "held to +24 dB in words too");
}

static void TestSave(std::wstring const &folder)
{
	puts("The save:");

	std::wstring const fmvs = folder + L"\\FMVs";
	std::wstring const backup = fmvs + L"\\backup";
	std::wstring const film = fmvs + L"\\message417.smk";
	std::vector<uint8_t> const first = { 'S', 'M', 'K', '2', '1' };
	std::vector<uint8_t> const second = { 'S', 'M', 'K', '2', '2' };
	std::wstring kept;
	std::string why;

	CreateDirectoryW(fmvs.c_str(), nullptr);
	Check(host::VideoMake_Save(film, first, backup, L"20261122-131500", &kept, &why) && kept.empty() && ReadText(film) == "SMK21", "a new film is written, with no backup (" + why + ")");
	Check(host::VideoMake_Save(film, second, backup, L"20261122-131600", &kept, &why) && ReadText(film) == "SMK22", "a film there is replaced (" + why + ")");
	Check(kept == backup + L"\\message417.smk.20261122-131600" && ReadText(kept) == "SMK21", "and the one it replaced is kept first, in FMVs\\backup, named with its time");

	WriteText(backup + L"\\message417.smk.20261122-131700", "an earlier backup");
	Check(!host::VideoMake_Save(film, first, backup, L"20261122-131700", &kept, &why) && ReadText(film) == "SMK22" && ReadText(backup + L"\\message417.smk.20261122-131700") == "an earlier backup",
		"a backup is never written over: the save stops, and the film is left as it was (" + why + ")");

	DeleteFileW((backup + L"\\message417.smk.20261122-131600").c_str());
	DeleteFileW((backup + L"\\message417.smk.20261122-131700").c_str());
	RemoveDirectoryW(backup.c_str());
	DeleteFileW(film.c_str());
	RemoveDirectoryW(fmvs.c_str());
}

int main()
{
	wchar_t temp[MAX_PATH + 1] = {};
	DWORD const length = GetTempPathW(MAX_PATH, temp);
	std::wstring const folder = std::wstring(temp, length) + L"video-make-test-" + std::to_wstring(GetCurrentProcessId());

	TestTimes();
	TestCut();
	TestLevel();

	if (length > 0 && CreateDirectoryW(folder.c_str(), nullptr))
	{
		TestSave(folder);
		RemoveDirectoryW(folder.c_str());
	}
	else
	{
		Check(false, "the test's own folder is made");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
