// The test of the Sound Edit window's numbers (sound_edit.h: inteng 0x4ed060, edobtype FUN_10006bd0, 0x10006830, 0x10007060; reassess/object-placement.md OP-133a..c). Written 2026-11-16
// by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The record from a sound's numbers, what the window shows, MFC's reading of a typed float (what is a number and what is not), its range words, OK and Cancel back into the sound
// (the round trips), the boxes under the mask, Play's frequency and loudness, and the wav list of a folder made here (files only, folders below depth first, "." folders left out).
#include "sound_edit.h"

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

using namespace host;

static int checksRun = 0, checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checksRun++;

	if (!good)
	{
		checksFailed++;
		printf("  FAILED  %s\n", what.c_str());
	}
}

static void TestShown()
{
	SoundEditRecord const r = SoundEdit_Record("drip.wav", 500000, 1000000, 128, 127, 0, 7);
	SoundEditShown const s = SoundEdit_Shown(r);

	Check(s.fields[0] == "5" && s.fields[1] == "10" && s.fields[2] == "1" && s.fields[3] == "100", "5 m, 10 m, 1 semitone, 100 % shown as MFC's %.6g (" + s.fields[0] + " " + s.fields[1] + " " + s.fields[2] + " " + s.fields[3] + ")");
	Check(s.playingAtStart && s.loops && s.playingAtStartLive && s.loopsLive, "flags 0: both boxes ticked and live (mask 3)");

	SoundEditRecord const half = SoundEdit_Record("x.wav", 123457, 0, -640, 63, 3, 0);
	SoundEditShown const h = SoundEdit_Shown(half);

	Check(h.fields[0] == "1.23457" && h.fields[2] == "-5" && h.fields[3] == "50", "1.23457 m, -5 semitones, the volume 63 shown ROUNDED to 50 % (ftol(49.6063 + 0.5)) (" + h.fields[0] + " " + h.fields[3] + ")");
	Check(!h.playingAtStart && !h.loops, "flags 3: neither box ticked");
}

static void TestRead()
{
	float v = -1;
	std::string message;

	Check(SoundEdit_ReadField(0, "5", &v, &message) && v == 5.0f, "\"5\" is a number");
	Check(SoundEdit_ReadField(0, " \t2.5 \t", &v, &message) && v == 2.5f, "spaces and tabs round one number");
	Check(SoundEdit_ReadField(0, "0", &v, &message) && v == 0.0f, "\"0\" is a number");
	Check(!SoundEdit_ReadField(0, "5x", &v, &message) && message == "Please enter a number.", "\"5x\": Please enter a number.");
	Check(!SoundEdit_ReadField(0, "", &v, &message) && message == "Please enter a number.", "nothing: Please enter a number.");
	Check(!SoundEdit_ReadField(0, "-0", &v, &message) && message == "Please enter a number.", "\"-0\" reads 0 with a first letter that is not '0': MFC's simple parse says no number");
	Check(!SoundEdit_ReadField(0, "1 2", &v, &message) && message == "Please enter a number.", "two numbers: no number");
	Check(!SoundEdit_ReadField(0, "1000.5", &v, &message) && message == "Please enter a number between 0 and 1000.", "inner 1000.5: the range words");
	Check(!SoundEdit_ReadField(1, "2001", &v, &message) && message == "Please enter a number between 0 and 2000.", "outer 2001");
	Check(SoundEdit_ReadField(2, "-48", &v, &message) && !SoundEdit_ReadField(2, "-49", &v, &message) && message == "Please enter a number between -48 and 48.", "pitch -48 / -49");
	Check(!SoundEdit_ReadField(3, "100.01", &v, &message) && message == "Please enter a number between 0 and 100.", "volume 100.01");
	Check(SoundEdit_ReadField(0, "1000.00001", &v, &message) && v == 1000.0f, "1000.00001 is 1000 once kept in a float: in range");
	Check(!SoundEdit_ReadField(4, "1", &v, &message), "no field 4");
}

static void TestBack()
{
	int32_t inner = 0, outer = 0, pitch = 0, volume = 0, probability = 9;
	uint32_t flags = 0;

	// OK with what the window showed: the volume 63 shows 50 and comes back (50 + 50/127) x 0.01 x 127 = 63.99999 -> 63
	SoundEditRecord r = SoundEdit_Record("x.wav", 500000, 1000000, 128, 63, 0, 5);
	float const shown[4] = { 5.0f, 10.0f, 1.0f, 50.0f };

	SoundEdit_Ok(&r, "y.wav", shown, true, false);
	SoundEdit_Back(r, &inner, &outer, &pitch, &volume, &flags, &probability);
	Check(inner == 500000 && outer == 1000000 && pitch == 128 && volume == 63 && r.name == "y.wav", "OK with the shown values: 500000, 1000000, 128, 63, the new name");
	Check(flags == 2 && probability == 0, "Playing at Start ticked, Sound Loops not: bit 1 set, bit 0 clear; the probability 0 in AvP");

	// CANCEL: the record as it was goes back through the floats (inteng writes it either way)
	SoundEditRecord const kept = SoundEdit_Record("x.wav", 333333, 1000000, -640, 63, 1, 5);

	SoundEdit_Back(kept, &inner, &outer, &pitch, &volume, &flags, &probability);
	Check(inner == 333332 && volume == 63 && pitch == -640 && flags == 1 && probability == 5, "Cancel: 333333 comes back 333332 (3.33333f x 100000 = 333332.99, cut), the rest as they were (" + std::to_string(inner) + ")");

	// a box whose mask bit is clear is not written
	SoundEditRecord masked = SoundEdit_Record("x.wav", 0, 0, 0, 0, 3, 0);

	masked.mask = 0;
	SoundEdit_Ok(&masked, "x.wav", shown, true, true);
	Check(masked.flags == 3, "mask 0: the flags are left as they were");
}

static void TestPlay()
{
	Check(SoundEdit_PlayFrequency(22050, 0.0f) == 22050, "pitch 0: the file's frequency");
	Check(SoundEdit_PlayFrequency(22050, 12.0f) == 44100 || SoundEdit_PlayFrequency(22050, 12.0f) == 44099, "pitch 12: twice");
	Check(SoundEdit_PlayVolume(100.0f) == 0, "100 %: 0");
	Check(SoundEdit_PlayVolume(50.0f) == -1419, "50 %: ftol(2048 x ln 0.5) = -1419 (" + std::to_string(SoundEdit_PlayVolume(50.0f)) + ")");
	Check(SoundEdit_PlayVolume(0.0f) == SoundEdit_PlayVolume(1.0f), "below 1 %: as 1 %");
}

static std::string TempFolder()
{
	char folder[MAX_PATH];
	DWORD const got = GetTempPathA(sizeof(folder), folder);

	return std::string(got > 0 && got < sizeof(folder) ? folder : ".\\") + "host_sound_edit_test";
}

static void Touch(std::string const &path)
{
	FILE *file = nullptr;

	if (fopen_s(&file, path.c_str(), "wb") == 0 && file)
	{
		fclose(file);
	}
}

static void TestWavNames()
{
	std::string const root = TempFolder();

	CreateDirectoryA(root.c_str(), nullptr);
	CreateDirectoryA((root + "\\sub").c_str(), nullptr);
	CreateDirectoryA((root + "\\.hidden").c_str(), nullptr);
	CreateDirectoryA((root + "\\folder.wav").c_str(), nullptr);
	Touch(root + "\\a.wav");
	Touch(root + "\\b.txt");
	Touch(root + "\\sub\\c.wav");
	Touch(root + "\\.hidden\\d.wav");

	std::vector<std::string> const names = SoundEdit_WavNames(root + "\\");
	bool const hasA = !names.empty() && names[0] == "a.wav";
	bool hasC = false, hasD = false, hasFolder = false, hasTxt = false;

	for (std::string const &n : names)
	{
		hasC = hasC || n == "sub\\c.wav";
		hasD = hasD || n == ".hidden\\d.wav";
		hasFolder = hasFolder || n == "folder.wav";
		hasTxt = hasTxt || n == "b.txt";
	}

	Check(hasA && hasC && !hasD && !hasFolder && !hasTxt && names.size() == 2, "the folder's own wav first, then sub\\c.wav; no folder called x.wav, no \".\" folder, no .txt (" + std::to_string(names.size()) + " names)");
	Check(SoundEdit_WavNames(root, 1).size() == 1, "at most `most` names");
	Check(SoundEdit_WavNames(root + "\\none").empty() && SoundEdit_WavNames(std::string()).empty(), "a folder not there, or none: no names");

	DeleteFileA((root + "\\a.wav").c_str());
	DeleteFileA((root + "\\b.txt").c_str());
	DeleteFileA((root + "\\sub\\c.wav").c_str());
	DeleteFileA((root + "\\.hidden\\d.wav").c_str());
	RemoveDirectoryA((root + "\\sub").c_str());
	RemoveDirectoryA((root + "\\.hidden").c_str());
	RemoveDirectoryA((root + "\\folder.wav").c_str());
	RemoveDirectoryA(root.c_str());
}

static std::string Joined(std::vector<std::string> const &names)
{
	std::string all;

	for (std::string const &n : names)
	{
		all += (all.empty() ? "" : "|") + n;
	}

	return all;
}

// The list from the fastfiles' names (Bishop's ruling 11-16): the finder's order, the hostile names left out
static void TestWavNamesFromFiles()
{
	std::vector<std::string> const files = {
		"sound/env/invasion/zap.wav", "sound/env/invasion/sub/b.wav", "sound/env/invasion/_x.wav", "sound/env/invasion/a.wav", "sound/env/invasion/sub/a.wav",
		"sound/env/invasion/a.wav",          // the same file in a second fastfile
		"sound/env/invasion/b.txt",          // no wav
		"sound/env/other/a.wav",             // another level's
		"sound/env/invasionx/a.wav",         // a folder that only starts with the same letters
		"sound/env/invasion/.hid/c.wav",     // a folder the finder skips
		"sound/env/invasion/../up.wav",      // up out of the folder
		"sound/env/invasion//d.wav",         // an empty part
		"sound/env/invasion/e:f.wav",        // a letter Windows refuses
		"sound/env/invasion/g\x01.wav",      // a letter outside ' '..'~'
		"sound/env/invasion/" + std::string(256, 'h') + ".wav",   // too long
	};

	// "_" is after the letters in capitals ('_' 0x5f > 'Z' 0x5a): NTFS gives a, zap, _x; then the folder sub with its own a, b
	std::string const got = Joined(SoundEdit_WavNamesFromFiles("Sound\\Env\\Invasion\\", files));

	Check(got == "a.wav|zap.wav|_x.wav|sub\\a.wav|sub\\b.wav", "the fastfile list in the finder's order, hostile names left out (got \"" + got + "\")");
	Check(Joined(SoundEdit_WavNamesFromFiles("sound/env/invasion", files, 2)) == "a.wav|zap.wav", "at most `most` names, a prefix with no slash at its end");
	Check(SoundEdit_WavNamesFromFiles("sound/env/none/", files).empty(), "no file under the prefix: no names");

	std::vector<std::string> deep;
	std::string path = "sound/d/";

	for (int i = 0; i < 33; i++)
	{
		path += "f/";
	}

	deep.push_back(path + "x.wav");
	Check(SoundEdit_WavNamesFromFiles("sound/d/", deep).empty(), "a name more than 32 folders deep is left out");
}

int main()
{
	setvbuf(stdout, 0, _IONBF, 0);

	TestShown();
	TestRead();
	TestBack();
	TestPlay();
	TestWavNames();
	TestWavNamesFromFiles();

	printf("%d checks, %d failed\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
