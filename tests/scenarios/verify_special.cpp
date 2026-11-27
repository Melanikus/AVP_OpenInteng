// verify_special.cpp: the check program of the Object Placement scenario (tests/scenarios/object-placement-main-job.txt): a level edited in the running editor and saved by Save a copy is read back
// through the game's own loader and the special objects it holds are compared with what the scenario typed. Written 2026-11-15 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own
// code: see License.txt, "AVP_OpenInteng's own code".
//
// USAGE: verify_special <the saved copy> <check> [<check> ...]; a check is  <kind>|<name>|<field>=<value>|...   with kind generator, badguy, start, sound or particle (a start has no name: give *),
// fields: for a sound wav, volume, pitch, soundflags, probability; for generator and badguy type, subtype, texture, extra1, extra2, flagson (hex bits that must be set), flagsoff (hex bits that must be
// clear), rate, increase, limit, weight0 .. weight14; for a particle ptype, time, pprob, speed, quantity, pflagson, pflagsoff, and its sound (OP-135) psound (0 / 1), psoundwav, psoundindex,
// psoundinner, psoundouter (mm), psoundvolume, psoundpitch, psoundflags. A check with only a name passes when the object is there. It passes
// (exit 0) when every check does; the lines say what was found. One more check is no object: sounddir|<folder>, the level's own sound folder (SOUNDDIR).

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "environment_chunk.hpp"   // Sound_Directory_Chunk: the level's sound folder (the sounddir check)
#include "mishchnk.hpp"
#include "level_file.h"
#include "special_objects_edit.h"

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	if (access != GENERIC_READ || creation != OPEN_EXISTING)
	{
		return INVALID_HANDLE_VALUE;
	}

	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char *message, int)
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

static int failed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok    " : "FAILED", what.c_str());
	failed += good ? 0 : 1;
}

static std::vector<std::string> Split(std::string const &text, char at)
{
	std::vector<std::string> out;
	size_t from = 0;

	for (;;)
	{
		size_t const to = text.find(at, from);

		out.push_back(text.substr(from, to == std::string::npos ? std::string::npos : to - from));

		if (to == std::string::npos)
		{
			return out;
		}

		from = to + 1;
	}
}

static bool Value(SpecialObject const &o, std::string const &field, long long *out, std::string *text)
{
	if (field == "wav") { *text = o.wav; return true; }
	if (field == "volume") { *out = o.volume; return true; }
	if (field == "pitch") { *out = o.pitch; return true; }
	if (field == "soundflags") { *out = o.soundFlags; return true; }
	if (field == "probability") { *out = o.probability; return true; }
	if (field == "type") { *out = o.type; return true; }
	if (field == "subtype") { *out = o.subType; return true; }
	if (field == "texture") { *out = o.texture; return true; }
	if (field == "extra1") { *out = o.extra1; return true; }
	if (field == "extra2") { *out = o.extra2; return true; }
	if (field == "rate") { *out = o.genRate; return true; }
	if (field == "increase") { *out = o.genIncrease; return true; }
	if (field == "limit") { *out = o.genLimit; return true; }
	if (field == "ptype") { *out = o.particleType; return true; }
	if (field == "time") { *out = o.time; return true; }
	if (field == "pprob") { *out = o.particleProbability; return true; }
	if (field == "speed") { *out = o.speed; return true; }
	if (field == "quantity") { *out = o.quantity; return true; }
	// a particle generator's sound, its INDSOUND (OP-135, added 11-17 by Drake): psound 1 when it has one
	if (field == "psound") { *out = o.hasSound ? 1 : 0; return true; }
	if (field == "psoundwav") { *text = o.particleSoundWav; return true; }
	if (field == "psoundindex") { *out = o.particleSoundIndex; return true; }
	if (field == "psoundinner") { *out = o.particleSoundInner; return true; }
	if (field == "psoundouter") { *out = o.particleSoundOuter; return true; }
	if (field == "psoundvolume") { *out = o.particleSoundVolume; return true; }
	if (field == "psoundpitch") { *out = o.particleSoundPitch; return true; }
	if (field == "psoundflags") { *out = o.particleSoundFlags; return true; }

	if (field.compare(0, 6, "weight") == 0)
	{
		int const n = atoi(field.c_str() + 6);

		*out = n >= 0 && n < 15 ? o.weights[n] : -1;
		return true;
	}

	return false;
}

// The level's own sound folder, REBENVDT > SOUNDDIR, or "" when it has none (as track_edit.cpp's TrackLevelSoundDirectory reads it)
static std::string LevelSoundDirectory(OpenLevelFile *file)
{
	File_Chunk *const level = file ? file->Level() : nullptr;
	Chunk_With_Children *const env = level ? dynamic_cast<Chunk_With_Children *>(level->lookup_single_child("REBENVDT")) : nullptr;
	Sound_Directory_Chunk *const directory = env ? dynamic_cast<Sound_Directory_Chunk *>(env->lookup_single_child("SOUNDDIR")) : nullptr;

	return directory && directory->directory ? std::string(directory->directory) : std::string();
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	if (argc < 3)
	{
		printf("usage: verify_special <saved copy> <check> [<check> ...]\n");
		return 2;
	}

	OpenLevelFile file;

	if (file.Open(argv[1]) != LevelOpenResult::Opened)
	{
		Check(false, std::string("the saved copy is taken by the game's own loader: ") + argv[1]);
		return 1;
	}

	Check(true, "the saved copy is taken by the game's own loader");

	std::vector<SpecialObject> all;

	Check(SpecialObjects_Read(&file, &all) == SpecialResult::Done, "its special objects are read: " + std::to_string((unsigned long long)all.size()));

	for (int i = 2; i < argc; i++)
	{
		std::vector<std::string> const parts = Split(argv[i], '|');
		SpecialKind kind = SpecialKind::Generator;

		if (parts.size() < 2)
		{
			Check(false, std::string("a check needs a kind and a name: ") + argv[i]);
			continue;
		}

		// sounddir|<folder>: the level's own sound folder (REBENVDT > SOUNDDIR, read as track_edit.cpp reads it) is exactly <folder>, capitals and all. (Added
		// 2026-11-27 by Melanikus (Dietrich): the sound import gives a level with no folder its own, Custom\<level>.)
		if (parts[0] == "sounddir")
		{
			std::string const holds = LevelSoundDirectory(&file);

			Check(holds == parts[1], "the level's sound folder (SOUNDDIR) is \"" + parts[1] + "\" (the copy holds \"" + holds + "\")");
			continue;
		}

		if (parts[0] == "generator") kind = SpecialKind::Generator;
		else if (parts[0] == "badguy") kind = SpecialKind::BadGuy;
		else if (parts[0] == "start") kind = SpecialKind::PlayerStart;
		else if (parts[0] == "sound") kind = SpecialKind::Sound;
		else if (parts[0] == "particle") kind = SpecialKind::ParticleGenerator;
		else
		{
			Check(false, "an unknown kind: " + parts[0]);
			continue;
		}

		SpecialObject const *found = nullptr;

		for (SpecialObject const &each : all)
		{
			if (each.kind == kind && (parts[1] == "*" || each.name == parts[1]))
			{
				found = &each;
			}
		}

		Check(found != nullptr, parts[0] + " \"" + parts[1] + "\" is in the copy");

		if (!found)
		{
			continue;
		}

		for (size_t f = 2; f < parts.size(); f++)
		{
			size_t const eq = parts[f].find('=');

			if (eq == std::string::npos)
			{
				Check(false, "a field check is name=value: " + parts[f]);
				continue;
			}

			std::string const field = parts[f].substr(0, eq);
			std::string const want = parts[f].substr(eq + 1);

			if (field == "flagson" || field == "flagsoff" || field == "pflagson" || field == "pflagsoff")
			{
				uint32_t const bits = (uint32_t)strtoul(want.c_str(), nullptr, 16);
				uint32_t const have = field[0] == 'p' ? found->particleFlags : found->flags;
				bool const on = field.find("on") != std::string::npos;

				Check(on ? (have & bits) == bits : (have & bits) == 0, parts[0] + " " + parts[1] + ": " + field + " " + want + " (flags are " + std::to_string((unsigned long long)have) + ")");
				continue;
			}

			long long number = 0;
			std::string text;

			if (!Value(*found, field, &number, &text))
			{
				Check(false, "an unknown field: " + field);
				continue;
			}

			if (field == "wav" || field == "psoundwav")
			{
				Check(text == want, parts[0] + " " + parts[1] + ": " + field + " is " + text + ", wanted " + want);
			}
			else
			{
				Check(number == atoll(want.c_str()), parts[0] + " " + parts[1] + ": " + field + " is " + std::to_string(number) + ", wanted " + want);
			}
		}
	}

	return failed ? 1 : 0;
}
