// host_texture_loop_waits_test.cpp: CAM-9's rule (src/host/texture_loop_waits.h): which of Texture Map's waits keep Home and Insert out, as inteng runs them as loops of their
// own. Each wait alone keeps them out, none lets them in, and both plane modes count. Written 2026-11-19 by Melanikus (Drake) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>

#include <string>

#include "texture_loop_waits.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

int main()
{
	host::TextureLoopWaitState none;

	Check(!host::TextureLoopWaits(none), "no wait up: Home and Insert act in Texture Map (WinMain 0x58cfe8 / 0x58cff6 after its frame)");

	struct One
	{
		char const *name;
		void (*set)(host::TextureLoopWaitState *);
	};

	One const each[] = {
		{ "the texture chooser or the repeats grid (0x2a FUN_005575a0, 0x10f FUN_00557c00)", [](host::TextureLoopWaitState *s) { s->chooser = true; } },
		{ "Wrap's plane polygon (0x2e FUN_0055e400)", [](host::TextureLoopWaitState *s) { s->planeMode = 1; } },
		{ "Tile's plane polygon (0x2f FUN_0055e5d0)", [](host::TextureLoopWaitState *s) { s->planeMode = 2; } },
		{ "Get UV coords from which polygon (0x31 FUN_0055c210)", [](host::TextureLoopWaitState *s) { s->getUv = true; } },
		{ "Get Anim from which polygon (0xb3 FUN_0055c210)", [](host::TextureLoopWaitState *s) { s->getAnim = true; } },
		{ "Anim's Select (0x9d FUN_005406a0)", [](host::TextureLoopWaitState *s) { s->animSelect = true; } },
		{ "the base polygon (Shift+B, 0xe7 FUN_00556ff0)", [](host::TextureLoopWaitState *s) { s->basePolygon = true; } },
		{ "Invert's object (0x101, its own wait)", [](host::TextureLoopWaitState *s) { s->invertObject = true; } },
	};

	for (One const &one : each)
	{
		host::TextureLoopWaitState s;

		one.set(&s);
		Check(host::TextureLoopWaits(s), std::string("keeps them out alone: ") + one.name);
	}

	host::TextureLoopWaitState all;

	for (One const &one : each)
	{
		one.set(&all);
	}

	Check(host::TextureLoopWaits(all), "every wait at once: out");

	host::TextureLoopWaitState odd;

	odd.planeMode = -1;   // not a mode the editor sets; any not-0 counts, as the editor's own test of it does
	Check(host::TextureLoopWaits(odd), "a plane mode that is not 0, whatever its number: out");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
