// The test of Object Placement's special-object commands (src/host/special_mode.cpp) on the game's own levels given on the command line: Create of each kind, Edit answered with
// nothing changes only what the dialogs themselves change, typed answers land in the right fields with the dialogs' units and limits, Delete and Find. Written 2026-11-15 by Melanikus
// (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// HOW IT RUNS: tests/host_special_mode_test.args builds it; host_special_mode_test <level file> [...]. A non-zero result means a check failed.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "special_mode.h"

static int checksFailed = 0;
static int checksRun = 0;

static void Check(bool good, std::string const & what)
{
	checksRun++;

	if (!good)
	{
		printf("  FAILED  %s\n", what.c_str());
		checksFailed++;
	}
}

HANDLE avp_CreateFile(const char * name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (access != GENERIC_READ || creation != OPEN_EXISTING)
	{
		return INVALID_HANDLE_VALUE;
	}

	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char * name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char * message, int)
{
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

using namespace host;

static bool Find(OpenLevelFile * file, SpecialKind kind, size_t number, SpecialObject * out)
{
	std::vector<SpecialObject> all;

	SpecialObjects_Read(file, &all);

	for (SpecialObject const & each : all)
	{
		if (each.kind == kind && each.number == number)
		{
			*out = each;
			return true;
		}
	}

	return false;
}

// Runs Edit on the object, answering the given texts in order and "" (keep) after them; returns how many questions were asked
static int EditWith(OpenLevelFile * file, SpecialMode * mode, std::vector<std::string> const & answers, std::string * last)
{
	SpecialStep step = SpecialMode_Command(file, mode, 0x18);
	int asked = 0;

	while (step.prompt && asked < 200)
	{
		std::string const reply = (size_t)asked < answers.size() ? answers[(size_t)asked] : std::string();

		asked++;
		step = SpecialMode_Typed(file, mode, reply);
	}

	if (last)
	{
		*last = step.answer;
	}

	return asked;
}

static void TestFile(std::string const & path)
{
	printf("%s\n", path.c_str());

	OpenLevelFile file;

	if (file.Open(path.c_str()) != LevelOpenResult::Opened)
	{
		Check(false, "the level opens: " + path);
		return;
	}

	std::vector<SpecialObject> all;

	SpecialObjects_Read(&file, &all);

	// Edit with every question answered "keep" changes only what the dialogs themselves change
	for (SpecialObject const & before : all)
	{
		if (before.kind == SpecialKind::PlayerStart)
		{
			continue;
		}

		SpecialMode mode;
		std::string words;

		mode.haveCurrent = true;
		mode.kind = before.kind;
		mode.number = before.number;
		EditWith(&file, &mode, {}, &words);

		SpecialObject after;

		if (!Find(&file, before.kind, before.number, &after))
		{
			Check(false, "the object is still there after Edit: " + before.name);
			return;
		}

		bool same = after.name == before.name && memcmp(after.place, before.place, 12) == 0;

		if (before.kind == SpecialKind::Generator || before.kind == SpecialKind::BadGuy)
		{
			uint32_t expected = before.flags;

			if (before.kind == SpecialKind::Generator)
			{
				expected |= 0x8000000u;      // the dialog's constructor forces the advanced generator on
			}

			same = same && after.flags == expected && after.type == before.type && after.heading == before.heading;

			if (before.kind == SpecialKind::BadGuy)
			{
				same = same && after.extra1 == before.extra1 && after.extra2 == before.extra2;
			}
			else if (before.hasSettings)
			{
				same = same && after.hasSettings && after.genRate == before.genRate && memcmp(after.weights, before.weights, sizeof(before.weights)) == 0 && after.genLimit == before.genLimit;
			}
		}
		else if (before.kind == SpecialKind::Sound)
		{
			// AvP hides Probability and the dialog writes 0
			same = same && after.inner == before.inner && after.outer == before.outer && after.wav == before.wav && after.pitch == before.pitch && after.soundFlags == before.soundFlags && after.probability == 0 &&
				after.volume == before.volume;
		}
		else if (before.kind == SpecialKind::ParticleGenerator)
		{
			same = same && after.particleType == before.particleType && after.particleFlags == before.particleFlags;
		}

		Check(same, "Edit answered with nothing keeps " + SpecialObject_Words(before) + " as it was (" + words + ")");
	}

	// both generator flag bits stay where they are: 0x2000000 (own max npc) and 0x4000000 (own rate) in each of their four combinations
	for (uint32_t bits = 0; bits < 4; bits++)
	{
		SpecialPlace where;
		size_t number = 0;

		if (SpecialObject_Create(&file, SpecialKind::Generator, where, 1.0, "flagtest", &number) != SpecialResult::Done)
		{
			Check(false, "a generator is made");
			return;
		}

		SpecialObject made;

		Find(&file, SpecialKind::Generator, number, &made);
		made.flags = (made.flags & ~0x6000000u) | (bits << 25) | 0x8000000u;
		made.hasSettings = true;
		SpecialObject_Write(&file, made);

		SpecialMode mode;

		mode.haveCurrent = true;
		mode.kind = SpecialKind::Generator;
		mode.number = number;
		EditWith(&file, &mode, {}, nullptr);

		SpecialObject after;

		Find(&file, SpecialKind::Generator, number, &after);
		Check(after.flags == made.flags, "Edit with no change leaves 'global times' and 'own max npc' alone (bits " + std::to_string(bits) + ")");
		SpecialObject_Delete(&file, SpecialKind::Generator, number);
	}

	// Create makes the object current, Edit takes typed answers with the dialogs' units and limits, Delete takes it away
	SpecialMode mode;

	mode.where.place[0] = 100;
	mode.where.place[1] = 200;
	mode.where.place[2] = 300;
	mode.levelScale = 1.0;

	// sound: name, wav, inner, outer, pitch, volume, playing at start, loops
	SpecialStep made = SpecialMode_Command(&file, &mode, 0x37);

	Check(!made.passOn && mode.haveCurrent && mode.kind == SpecialKind::Sound, "Create Sound makes a sound and selects it: " + made.answer);

	std::string words;
	int asked = EditWith(&file, &mode, { "snd test", "step.wav", "12.5", "40", "2", "80", "n", "y" }, &words);

	Check(asked == 8, "the sound dialog asks 8 questions (" + std::to_string(asked) + ")");

	SpecialObject sound;

	Check(Find(&file, SpecialKind::Sound, mode.number, &sound), "the sound is in the level");
	Check(sound.name == "snd test" && sound.wav == "step.wav", "name and wav");
	Check(sound.inner == 12500 && sound.outer == 40000, "inner 12.5 m and outer 40 m in file units (" + std::to_string(sound.inner) + ", " + std::to_string(sound.outer) + ")");
	Check(sound.pitch == 256, "pitch 2 semitones is 256 (" + std::to_string(sound.pitch) + ")");
	Check(sound.volume == 102, "volume 80 % is 102 of 127 (" + std::to_string(sound.volume) + ")");
	Check((sound.soundFlags & 1) == 1 && (sound.soundFlags & 2) == 0, "'Playing at Start' n sets bit 0, 'Sound Loops' y clears bit 1");
	Check(sound.probability == 0, "probability is written 0");

	// an answer out of range asks again and changes nothing
	{
		SpecialStep step = SpecialMode_Command(&file, &mode, 0x18);

		SpecialMode_Typed(&file, &mode, "snd test");   // name
		SpecialMode_Typed(&file, &mode, "step.wav");   // wav
		step = SpecialMode_Typed(&file, &mode, "1001");   // inner: over 1000
		Check(step.prompt && step.title.find("Inner Range") != std::string::npos, "inner 1001 m asks again");
		SpecialMode_Cancel(&mode);
		Check(Find(&file, SpecialKind::Sound, mode.number, &sound) && sound.inner == 12500, "Esc leaves the sound as it was");
	}

	// every volume percent shown comes back the same
	int badVolume = 0;

	for (int v = 0; v <= 127; v++)
	{
		SpecialObject s = sound;

		s.volume = v;
		SpecialObject_Write(&file, s);

		SpecialMode m;

		m.haveCurrent = true;
		m.kind = SpecialKind::Sound;
		m.number = mode.number;

		SpecialStep step = SpecialMode_Command(&file, &m, 0x18);
		int number = 0;
		std::string shown;

		while (step.prompt && number < 40)
		{
			std::string reply;

			if (step.title.find("Volume") != std::string::npos)
			{
				shown = step.defaultText;
				reply = shown;       // retype the percent the dialog showed
			}

			number++;
			step = SpecialMode_Typed(&file, &m, reply);
		}

		SpecialObject back;

		Find(&file, SpecialKind::Sound, mode.number, &back);

		if (abs(back.volume - v) > 1 || ((v == 0 || v == 127) && back.volume != v))
		{
			Check(false, "volume " + std::to_string(v) + " shown as " + shown + " % comes back as " + std::to_string(back.volume));
			badVolume++;
		}
	}

	Check(badVolume == 0, "every volume 0 to 127 comes back within 1 (127 steps shown as whole percent, as inteng does) when its shown percent is typed again");

	// Delete takes the current sound away; with none current it is not this module's
	{
		SpecialStep gone = SpecialMode_Command(&file, &mode, 0x16);

		Check(!gone.passOn && !mode.haveCurrent && !Find(&file, SpecialKind::Sound, mode.number, &sound), "Delete takes the selected sound away: " + gone.answer);
		Check(SpecialMode_Command(&file, &mode, 0x16).passOn, "Delete with nothing selected is left to the editor");
		Check(SpecialMode_Command(&file, &mode, 0x19).passOn, "Find on the main screen is left to the editor");
	}

	// particle generator: type Steam zeroes probability and frequency
	SpecialMode_Command(&file, &mode, 0x39);
	EditWith(&file, &mode, { "steamy", "2", "9.5", "3.25", "70", "20", "y" }, &words);

	SpecialObject particle;

	Check(Find(&file, SpecialKind::ParticleGenerator, mode.number, &particle) && particle.particleType == 1, "particle generator of type Steam");
	Check(particle.time == 0 && particle.particleProbability == 0, "types 1 to 3 zero frequency and probability");
	Check(particle.speed == 325 && particle.quantity == 20 && (particle.particleFlags & 1) == 0, "speed 3.25 m/s is 325, quantity 20, active");
	SpecialMode_Command(&file, &mode, 0x16);

	// generator: Easy off, weights, rate
	SpecialMode_Command(&file, &mode, 0x34);

	std::vector<std::string> generatorAnswers = { "gentest", "n" };

	asked = EditWith(&file, &mode, generatorAnswers, &words);
	Check(asked >= 28, "the generator dialog asks its fields (" + std::to_string(asked) + ")");

	SpecialObject generator;

	Check(Find(&file, SpecialKind::Generator, mode.number, &generator) && generator.name == "gentest" && generator.hasSettings && (generator.flags & 0x8000000u) && (generator.flags & 0x100000u),
		"Easy answered n sets the NOT-easy bit and the generator has its settings");
	SpecialMode_Command(&file, &mode, 0x16);

	// bad guy: marine with a subtype and a texture
	SpecialMode_Command(&file, &mode, 0x35);
	EditWith(&file, &mode, { "marine1", "marine", "pistol", "9" }, &words);

	SpecialObject bad;

	Check(Find(&file, SpecialKind::BadGuy, mode.number, &bad) && bad.type == 7 && bad.subType == 5 && bad.texture == 9, "a marine with the Pistol subtype and texture 9");
	EditWith(&file, &mode, { "", "sentry" , "300" }, &words);
	Check(Find(&file, SpecialKind::BadGuy, mode.number, &bad) && bad.type == 9 && bad.texture == 0 && bad.extra1 == 1 && bad.extra2 == 44, "a sentry gun with 300 rounds: extra1 1, extra2 44; texture back to 0");
	SpecialMode_Command(&file, &mode, 0x16);

	// player starting position has no dialog
	SpecialMode_Command(&file, &mode, 0x3a);

	SpecialStep start = SpecialMode_Command(&file, &mode, 0x18);

	Check(!start.prompt && !start.passOn, "Player starting position has no Edit dialog");
	SpecialMode_Command(&file, &mode, 0x16);

	// Find on the sounds screen
	mode.soundsOnly = true;
	SpecialMode_Command(&file, &mode, 0x37);
	SpecialMode_Command(&file, &mode, 0x37);

	SpecialStep find = SpecialMode_Command(&file, &mode, 0x19);

	Check(find.prompt && mode.found.size() >= 2, "Find lists the sounds");

	SpecialStep picked = SpecialMode_Typed(&file, &mode, "1");

	Check(!picked.prompt && mode.haveCurrent && mode.kind == SpecialKind::Sound, "and the one picked is selected: " + picked.answer);
}

int main(int argc, char ** argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 2)
	{
		printf("usage: host_special_mode_test <level file> [<level file> ...]\n");
		return 2;
	}

	for (int i = 1; i < argc; i++)
	{
		TestFile(argv[i]);
	}

	printf("%d checks, %d failed\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
