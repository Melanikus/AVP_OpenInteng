// The test for where the editor's camera starts in a level just opened (camera_start.h), and that level_view.cpp uses it. Written 2026-11-14
// by Melanikus (Dietrich) for AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: the camera started at the player's display block, which the editor never moves from 0,0,0; Genshd1's view was black. The camera now
// starts at the player's body, at the level's own start, and a missing piece on the way there is refused, never read. The choice is tried
// here in every case. Then level_view.cpp's own text is read, as the source checks of the game's tests read theirs (draw_chain_test), to
// prove it hands the body's place to the choice and never starts the camera at the display block again. Run from the worktree's top
// folder, where src\host\level_view.cpp is.
//
// Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdio.h>

#include <string>

#include "camera_start.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static bool Same(const CameraPlace &a, const CameraPlace &b)
{
	return a.x == b.x && a.y == b.y && a.z == b.z;
}

static void TestTheChoice()
{
	puts("Where the camera starts:");

	CameraPlace const body = { 5100, -1200, 830 };
	CameraPlace const levelStart = { 4000, -900, 700 };
	const char *why = "not set";

	Check(Same(CameraStart_Choose(true, true, true, body, levelStart, &why), body) && why == 0, "with a player, a strategy block and a body: at the body, and no reason");

	CameraPlace const origin = { 0, 0, 0 };
	Check(Same(CameraStart_Choose(true, true, true, origin, levelStart, &why), origin) && why == 0, "a body at 0,0,0 is still the body: the level put it there");

	Check(Same(CameraStart_Choose(true, true, false, body, levelStart, &why), levelStart) && why && std::string(why).find("no body") != std::string::npos,
		"no dynamics block: at the level's start, and the reason says the player has no body");
	Check(Same(CameraStart_Choose(true, false, false, body, levelStart, &why), levelStart) && why && std::string(why).find("strategy block") != std::string::npos,
		"no strategy block: at the level's start, and the reason says so");
	Check(Same(CameraStart_Choose(false, false, false, body, levelStart, &why), levelStart) && why && std::string(why).find("no player") != std::string::npos,
		"no player: at the level's start, and the reason says so");
	Check(Same(CameraStart_Choose(false, true, true, body, levelStart, &why), levelStart) && why != 0,
		"no player, whatever else is claimed: the body is never used");
	Check(Same(CameraStart_Choose(true, false, true, body, levelStart, &why), levelStart) && why != 0,
		"no strategy block, whatever else is claimed: the body is never used");
}

// A source file's text, or empty if it can't be read
static std::string ReadSource(const char *name)
{
	std::string text;
	FILE *file = fopen(name, "rb");

	if (file)
	{
		char buffer[4096];
		size_t got = 0;

		while ((got = fread(buffer, 1, sizeof(buffer), file)) > 0)
		{
			text.append(buffer, got);
		}

		fclose(file);
	}

	return text;
}

static void TestLevelViewUsesIt()
{
	puts("level_view.cpp starts the camera by the choice:");

	std::string const source = ReadSource("src/host/level_view.cpp");

	Check(!source.empty(), "(level_view.cpp is read)");
	Check(source.find("CameraStart_Choose(hasPlayer, hasStrategy, hasDynamics, body, levelStart, &missing)") != std::string::npos,
		"it asks camera_start.h where the camera starts");
	Check(source.find("VECTORCH const &position = Player->ObStrategyBlock->DynPtr->Position;") != std::string::npos,
		"the body it hands over is the player's dynamics block's Position, where the game puts the player");
	Check(source.find("bool const hasDynamics = hasStrategy && Player->ObStrategyBlock->DynPtr != 0;") != std::string::npos
		&& source.find("bool const hasStrategy = hasPlayer && Player->ObStrategyBlock != 0;") != std::string::npos,
		"each piece is checked only once the one before it is there");
	Check(source.find("startHere->x = Player->ObWorld") == std::string::npos && source.find("startHere->x = start.x;") != std::string::npos,
		"the camera never starts at the player's display block");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestTheChoice();
	TestLevelViewUsesIt();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
