// The test of src/host/generator_settings: edobtype's "Generator Settings" window as numbers (resource 133; reassess/object-placement.md OP-131, edobtype-dialogs.md section 2). Written 2026-11-16 by
// Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_generator_settings_test.exe (no files read)

#include <stdio.h>
#include <string.h>

#include <string>

#include "generator_settings.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static SpecialObject AGenerator()
{
	SpecialObject g;

	g.kind = SpecialKind::Generator;
	g.flags = 0x800 | 0x2000 | 0x200000 | 0x4000000;   // Marine, Predator, NOT Medium, own rate (use global times off)

	for (int i = 0; i < 15; i++)
	{
		g.weights[i] = i * 10;
	}

	g.genRate = 29;       // 0.29 a minute
	g.genIncrease = 150;  // 1.5
	g.genLimit = 12;
	return g;
}

int main()
{
	puts("shown (the constructor 0x10001ed0, DDX's show direction):");

	SpecialObject g = AGenerator();
	GeneratorSettingsTyped shown = GeneratorSettings_Shown(g);

	Check(shown.weights[0] == "0" && shown.weights[11] == "110" && shown.weights[14] == "140", "the weights as \"%d\"");
	Check(shown.rate == "0.29" && shown.increase == "1.5", "the rates x 0.01 as MFC's float text (\"%.*g\", 6 digits): 0.29, 1.5");
	Check(shown.maxNpcs == "12", "Max NPCs \"%u\"");
	Check(shown.marine && !shown.alien && shown.predator && shown.easy && !shown.medium && shown.hard, "Marine / Alien / Predator by 0x800 / 0x1000 / 0x2000; Easy / Medium / Hard ticked when their NOT bit is clear");
	Check(!shown.useGlobalTimes && !shown.useOwnMax && !shown.startsInactive && !shown.multiplayerStart, "Use global generator times: 0x4000000 clear; Use own max npc setting: 0x2000000 set");

	puts("OK (DoDataExchange 0x100020f0's order, OK 0x10002410):");

	SpecialObject out = g;
	std::string message;
	int32_t control = 0;

	Check(GeneratorSettings_Ok(shown, &out, &message, &control) && out.flags == (g.flags | 0x8000000) && out.hasSettings && out.genRate == 29 && out.genIncrease == 150 && out.genLimit == 12,
		"OK on what is shown: nothing changes but 0x8000000 (the constructor's) and the settings kept (a GENEXSET written)");

	GeneratorSettingsTyped t = shown;

	t.weights[0] = "abc";       // Pulse Rifle 1027 (10th read)
	t.weights[11] = "2000";     // Alien 1039 (1st read)
	out = g;
	Check(!GeneratorSettings_Ok(t, &out, &message, &control) && control == 1039 && message == "Please enter an integer between 0 and 1000." && out.weights[11] == 110,
		"the FIRST refused in the DLL's order says MFC's words: Alien 1039 before Pulse Rifle 1027; nothing kept");
	t.weights[11] = " 7 ";
	Check(!GeneratorSettings_Ok(t, &out, &message, &control) && control == 1027 && message == "Please enter an integer.", "spaces round a number are taken; \"abc\": \"Please enter an integer.\"");
	t.weights[0] = "5";
	t.rate = "50.5";
	Check(!GeneratorSettings_Ok(t, &out, &message, &control) && control == 1042 && message == "Please enter a number between 0 and 50.", "a rate above 50: \"Please enter a number between 0 and 50.\"");
	t.rate = "x";
	Check(!GeneratorSettings_Ok(t, &out, &message, &control) && control == 1042 && message == "Please enter a number.", "a rate that is no number: \"Please enter a number.\"");
	t.rate = "0.29";
	t.maxNpcs = "256";
	Check(!GeneratorSettings_Ok(t, &out, &message, &control) && control == 1051 && message == "Please enter an integer between 0 and 255.", "Max NPCs 256: \"Please enter an integer between 0 and 255.\"");
	t.maxNpcs = "-1";
	Check(!GeneratorSettings_Ok(t, &out, &message, &control) && control == 1051, "Max NPCs -1: refused (\"%u\" takes no '-')");
	t.maxNpcs = "200";
	t.useGlobalTimes = true;
	t.useOwnMax = true;
	t.alien = true;
	t.medium = true;
	t.startsInactive = true;
	t.multiplayerStart = true;
	out = g;
	Check(GeneratorSettings_Ok(t, &out, &message, &control) && out.weights[11] == 7 && out.weights[0] == 5 && out.genLimit == 200 && out.genRate == 29, "the values kept; 0.29 stays 29 (INTENG BUG, FIXED: ftol would make 28)");
	Check((out.flags & 0x4000000) == 0 && (out.flags & 0x2000000) != 0, "Use global generator times ticked clears 0x4000000 and Use own max npc setting sets 0x2000000, each on the bit it was read from (INTENG BUG, FIXED: the DLL swaps them)");
	Check((out.flags & 0x1000) != 0 && (out.flags & 0x200000) == 0 && (out.flags & 0x80000000u) != 0 && (out.flags & 0x40000000) != 0, "Alien, Medium (its NOT bit cleared), Starts inactive, Multiplayer start");

	puts("MFC's readers:");

	long n = 0;
	double d = 0;

	Check(GeneratorSettings_ScanInt("\t12 ", false, &n) && n == 12 && !GeneratorSettings_ScanInt("12 3", false, &n) && !GeneratorSettings_ScanInt("", false, &n) && !GeneratorSettings_ScanInt("-0", false, &n),
		"\"%d\": spaces round one number; two numbers, nothing, or a 0 not written as 0: no number");
	Check(GeneratorSettings_ScanInt(std::string(70, ' ') + "5", false, &n) == false, "only the first 63 letters are read (szT[64]): a number after 70 spaces is not seen");
	Check(GeneratorSettings_ScanFloat("1e1", &d) && d == 10.0 && !GeneratorSettings_ScanFloat("1,5", &d), "floats: strtod's whole text");

	if (checksFailed)
	{
		printf("FAILED host_generator_settings_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_generator_settings_test\n");
	return 0;
}
