// See generator_settings.h. Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "generator_settings.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

namespace host
{
	namespace
	{
		uint32_t const kMarine = 0x800, kAlien = 0x1000, kPredator = 0x2000, kNotEasy = 0x100000, kNotMedium = 0x200000, kNotHard = 0x400000, kUseOwnLimit = 0x2000000, kUseOwnRate = 0x4000000,
		               kAdvanced = 0x8000000, kMultiplayerStart = 0x40000000, kInactive = 0x80000000u;

		// what the reader sees of an edit: MFC reads the window text into a buffer (GetWindowText): DDX_TextWithFormat's szT[64] for a whole number, _AfxTextFloatFormat's szBuffer[32] for a float, so
		// only the first 63 / 31 letters are read
		size_t const kIntLetters = 63, kFloatLetters = 31;

		// MFC's float text: _stprintf("%.*g", FLT_DIG = 6)
		std::string FloatText(float value)
		{
			char text[64];

			_snprintf_s(text, sizeof(text), _TRUNCATE, "%.*g", 6, (double)value);
			return text;
		}

		bool Spaces(char c)
		{
			return c == ' ' || c == '\t';
		}

		// one weight (DDX_Text int, DDV_MinMaxInt 0 .. 1000)
		bool ReadWeight(std::string const &text, int32_t *value, std::string *message)
		{
			long read = 0;

			if (!GeneratorSettings_ScanInt(text, false, &read))
			{
				*message = "Please enter an integer.";   // AFX_IDP_PARSE_INT (61712)
				return false;
			}

			if (read < 0 || read > 1000)
			{
				*message = "Please enter an integer between 0 and 1000.";   // AFX_IDP_PARSE_INT_RANGE (61714) with "%ld" of 0 and 1000
				return false;
			}

			*value = (int32_t)read;
			return true;
		}

		// one rate (DDX_Text float, DDV_MinMaxFloat 0 .. 50)
		bool ReadRate(std::string const &text, float *value, std::string *message)
		{
			double read = 0;

			if (!GeneratorSettings_ScanFloat(text, &read))
			{
				*message = "Please enter a number.";   // AFX_IDP_PARSE_REAL (61713)
				return false;
			}

			float const kept = (float)read;

			if (kept < 0.0f || kept > 50.0f)
			{
				*message = "Please enter a number between " + FloatText(0.0f) + " and " + FloatText(50.0f) + ".";   // AFX_IDP_PARSE_REAL_RANGE (61715), "%.*g" of the limits
				return false;
			}

			*value = kept;
			return true;
		}

		uint32_t Bit(uint32_t flags, uint32_t bit, bool set)
		{
			return set ? (flags | bit) : (flags & ~bit);
		}
	}

	bool GeneratorSettings_ScanInt(std::string const &whole, bool unsignedForm, long *value)
	{
		std::string const text = whole.substr(0, kIntLetters);
		size_t at = 0;

		while (at < text.size() && Spaces(text[at]))
		{
			at++;
		}

		std::string const rest = text.substr(at);
		char const first = rest.empty() ? '\0' : rest[0];
		char *end = nullptr;
		long read = 0;

		if (unsignedForm)
		{
			if (first == '-')
			{
				return false;
			}

			read = (long)strtoul(rest.c_str(), &end, 10);
		}
		else
		{
			read = strtol(rest.c_str(), &end, 10);
		}

		if (!end || (read == 0 && first != '0'))
		{
			return false;   // nothing read
		}

		while (Spaces(*end))
		{
			end++;
		}

		if (*end != '\0')
		{
			return false;   // more than one number
		}

		*value = read;
		return true;
	}

	bool GeneratorSettings_ScanFloat(std::string const &whole, double *value)
	{
		std::string const text = whole.substr(0, kFloatLetters);
		size_t at = 0;

		while (at < text.size() && Spaces(text[at]))
		{
			at++;
		}

		std::string const rest = text.substr(at);
		char const first = rest.empty() ? '\0' : rest[0];
		char *end = nullptr;
		double const read = strtod(rest.c_str(), &end);

		if (!end || (read == 0.0 && first != '0'))
		{
			return false;
		}

		while (Spaces(*end))
		{
			end++;
		}

		if (*end != '\0' || !(read == read))
		{
			return false;
		}

		*value = read;
		return true;
	}

	GeneratorSettingsTyped GeneratorSettings_Shown(SpecialObject const &generator)
	{
		GeneratorSettingsTyped t;
		uint32_t const flags = generator.flags | kAdvanced;   // 0x10001f83: the advanced generator, at once

		for (int i = 0; i < 15; i++)
		{
			t.weights[i] = std::to_string((long long)generator.weights[i]);
		}

		// 0x1000200f / 0x10002024: (float)(int)rate * 0.01 (a double product kept in a float)
		t.rate = FloatText((float)((double)generator.genRate * 0.01));
		t.increase = FloatText((float)((double)generator.genIncrease * 0.01));
		t.maxNpcs = std::to_string((unsigned)generator.genLimit);
		t.alien = (flags & kAlien) != 0;
		t.marine = (flags & kMarine) != 0;
		t.predator = (flags & kPredator) != 0;
		t.useGlobalTimes = (flags & kUseOwnRate) == 0;   // 0x10002054
		t.useOwnMax = (flags & kUseOwnLimit) != 0;      // 0x10002060
		t.startsInactive = (flags & kInactive) != 0;
		t.multiplayerStart = (flags & kMultiplayerStart) != 0;
		t.easy = (flags & kNotEasy) == 0;
		t.medium = (flags & kNotMedium) == 0;
		t.hard = (flags & kNotHard) == 0;
		return t;
	}

	bool GeneratorSettings_Ok(GeneratorSettingsTyped const &typed, SpecialObject *generator, std::string *message, int32_t *control)
	{
		if (!generator)
		{
			return false;
		}

		std::string words;
		int32_t weights[15];
		float rate = 0.0f, increase = 0.0f;

		for (int i = 0; i < 15; i++)
		{
			weights[i] = generator->weights[i];
		}

		// DoDataExchange 0x100020f0's order: Alien, Civ Flamer, Civ Molotov, Civ Pistol, Civ Shotgun, Civ Unarmed, Marine Flamer, Grenade, Minigun, Pulse Rifle, Sadar, Smartgun, (three boxes), Praetorian,
		// Pred Alien, (two boxes), the two rates, Marine Pistol, Max NPCs
		struct Weight { int index; int32_t id; };
		static Weight const first[12] = { { 11, 1039 }, { 8, 1035 }, { 10, 1038 }, { 7, 1034 }, { 6, 1033 }, { 9, 1036 }, { 1, 1028 }, { 4, 1031 }, { 5, 1032 }, { 0, 1027 }, { 3, 1030 }, { 2, 1029 } };
		static Weight const second[2] = { { 13, 1041 }, { 12, 1040 } };

		auto const refuse = [&](int32_t id)
		{
			if (message)
			{
				*message = words;
			}

			if (control)
			{
				*control = id;
			}

			return false;
		};

		for (Weight const &w : first)
		{
			if (!ReadWeight(typed.weights[w.index], &weights[w.index], &words))
			{
				return refuse(w.id);
			}
		}

		for (Weight const &w : second)
		{
			if (!ReadWeight(typed.weights[w.index], &weights[w.index], &words))
			{
				return refuse(w.id);
			}
		}

		if (!ReadRate(typed.rate, &rate, &words))
		{
			return refuse(1042);
		}

		if (!ReadRate(typed.increase, &increase, &words))
		{
			return refuse(1043);
		}

		if (!ReadWeight(typed.weights[14], &weights[14], &words))
		{
			return refuse(1049);
		}

		// Max NPCs: DDX_Text on a BYTE ("%u", AFX_IDP_PARSE_BYTE when not read or above 255), then DDV_MinMaxByte 0 .. 255
		long npcs = 0;

		if (!GeneratorSettings_ScanInt(typed.maxNpcs, true, &npcs) || (int)npcs > 255)
		{
			words = "Please enter an integer between 0 and 255.";   // AFX_IDP_PARSE_BYTE (61718)
			return refuse(1051);
		}

		// OK 0x10002410: the members back
		for (int i = 0; i < 15; i++)
		{
			generator->weights[i] = weights[i];
		}

		// INTENG BUG, FIXED: ftol(rate x 100.0) truncates (0.29 -> 28); the nearest hundredth is kept
		generator->genRate = (int32_t)floor((double)rate * 100.0 + 0.5);
		generator->genIncrease = (int32_t)floor((double)increase * 100.0 + 0.5);
		generator->genLimit = (uint8_t)npcs;

		uint32_t flags = generator->flags | kAdvanced;

		flags = Bit(flags, kAlien, typed.alien);
		flags = Bit(flags, kMarine, typed.marine);
		flags = Bit(flags, kPredator, typed.predator);
		flags = Bit(flags, kUseOwnRate, !typed.useGlobalTimes);   // INTENG BUG, FIXED: the DLL writes this one on 0x2000000 (0x1000257d)
		flags = Bit(flags, kUseOwnLimit, typed.useOwnMax);         // INTENG BUG, FIXED: the DLL writes this one on 0x4000000 (0x1000259f)
		flags = Bit(flags, kInactive, typed.startsInactive);
		flags = Bit(flags, kMultiplayerStart, typed.multiplayerStart);
		flags = Bit(flags, kNotEasy, !typed.easy);
		flags = Bit(flags, kNotMedium, !typed.medium);
		flags = Bit(flags, kNotHard, !typed.hard);
		generator->flags = flags;
		generator->hasSettings = true;   // the save (FUN_004e52a0) writes the GENEXSET when 0x8000000 is set
		return true;
	}
}
