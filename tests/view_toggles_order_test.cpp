// The order test of the View menu's draw modes and switches (src/host/view_toggles.h). Written 2026-11-14 by Melanikus (Apone) for
// AVP_OpenInteng (F3). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// WHY: Melanikus, 2026-11-14 21:35: "clicking polyline first time didn't change anything, i had to siwtch modes and come back". Each mode
// had been tried alone, from a clean start, and nothing tried the moves between them. Bishop's rule since then: every order is tested,
// from a table of what each item does. The table is AvP-docs\engine\inteng\view-state-table.md, and since Melanikus's 21:44 ("I want a
// copy right, it just needs to work now as it did then") it's the old inteng's own: one draw mode (7 Polygon, 8 Polyline, 9 Filled
// polyline, and the textured wireframe he asked for on top) and a Gouraud switch kept across every mode (inteng's DAT_00914d6c), whose
// Off is the textures at 100% with no light. It's written as code below (TableNext and TableSwitches), and the test walks every state
// there is (each mode, each Gouraud value, every mix of the toggles, the brightness at its ends and in the middle, and each of what the
// game can and can't show yet) through every item, and checks that the editor's model gives exactly the table's next state and exactly
// the table's switches for the game. A clean start is a state of its own: every item that's on from there has to change something.
//
// When view-state-table.md changes, the table here changes with it, never the other way round.
//
// It runs none of the game. The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "view_toggles.h"

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

static const DrawMode kModes[] = { DrawMode::Polygon, DrawMode::Polyline, DrawMode::FilledPolyline, DrawMode::WireTextured };
static const Gouraud kGouraud[] = { Gouraud::Off, Gouraud::On };
static const ViewToggle kItems[] = {
	ViewToggle::Polygon, ViewToggle::Polyline, ViewToggle::FilledPolyline, ViewToggle::WireTextured, ViewToggle::GouraudSwitch,
	ViewToggle::BackFaces, ViewToggle::Headlight, ViewToggle::Brighter, ViewToggle::Darker,
};
static const int32_t kBrightnesses[] = { kBrightnessLeast, kBrightnessLeast + 1, kBrightnessAsBuilt, kBrightnessMost - 1, kBrightnessMost };

static const char *ModeName(DrawMode mode)
{
	switch (mode)
	{
		case DrawMode::Polygon: return "polygon";
		case DrawMode::Polyline: return "polyline";
		case DrawMode::FilledPolyline: return "filled polyline";
		case DrawMode::WireTextured: return "wireframe, textured";
	}
	return "?";
}

static const char *ItemName(ViewToggle item)
{
	switch (item)
	{
		case ViewToggle::Polygon: return "Polygon (7)";
		case ViewToggle::Polyline: return "Polyline (8)";
		case ViewToggle::FilledPolyline: return "Filled polyline (9)";
		case ViewToggle::WireTextured: return "Wireframe, textured";
		case ViewToggle::GouraudSwitch: return "Gouraud (B)";
		case ViewToggle::BackFaces: return "Back faces";
		case ViewToggle::Headlight: return "Headlight (Ctrl+B)";
		case ViewToggle::Brighter: return "Brighter";
		case ViewToggle::Darker: return "Darker";
	}
	return "?";
}

static std::string Describe(const ViewToggles &t)
{
	char words[160];
	snprintf(words, sizeof(words), "%s, gouraud %s, back faces %s, headlight %s, brightness %d", ModeName(t.drawMode),
		t.gouraud == Gouraud::On ? "on" : "off", t.backFaces ? "on" : "off", t.headlight ? "on" : "off", (int)t.brightness);
	return words;
}

static bool Same(const ViewToggles &a, const ViewToggles &b)
{
	return a.drawMode == b.drawMode && a.gouraud == b.gouraud && a.backFaces == b.backFaces && a.headlight == b.headlight
		&& a.brightness == b.brightness;
}

static bool Same(const GameViewSwitches &a, const GameViewSwitches &b)
{
	return a.wireFrameMode == b.wireFrameMode && a.drawFullBright == b.drawFullBright && a.drawBackFaces == b.drawBackFaces
		&& a.filledOutlines == b.filledOutlines && a.outlineColour == b.outlineColour && a.outlinesLit == b.outlinesLit
		&& a.headlight == b.headlight && a.gamma == b.gamma && a.untexturedColour == b.untexturedColour;
}

// THE TABLE (view-state-table.md, sections 1, 2 and 6a): what each item does to the state.
// - A draw mode item makes that the draw mode and leaves the Gouraud switch and every toggle as they are. Choosing the mode already on
//   changes nothing. Polyline and Filled polyline need the game's outline pass, so while it can't show outlines they change nothing.
// - The Gouraud switch goes Off or On, and nothing else touches it, so it's kept across every mode change.
// - Back faces goes on or off once the game can show them; until then it stays off. The headlight goes on or off.
// - Brighter and Darker move the brightness by one step, held to its ends.
static ViewToggles TableNext(const ViewToggles &now, ViewToggle item, const GameCanShow &can)
{
	ViewToggles next = now;
	int32_t const held = now.brightness < kBrightnessLeast ? kBrightnessLeast : (now.brightness > kBrightnessMost ? kBrightnessMost : now.brightness);

	switch (item)
	{
		case ViewToggle::Polygon: next.drawMode = DrawMode::Polygon; break;
		case ViewToggle::Polyline: if (can.outlines) next.drawMode = DrawMode::Polyline; break;
		case ViewToggle::FilledPolyline: if (can.outlines) next.drawMode = DrawMode::FilledPolyline; break;
		case ViewToggle::WireTextured: next.drawMode = DrawMode::WireTextured; break;
		case ViewToggle::GouraudSwitch: next.gouraud = now.gouraud == Gouraud::On ? Gouraud::Off : Gouraud::On; break;
		case ViewToggle::BackFaces: next.backFaces = can.backFaces && !now.backFaces; break;
		case ViewToggle::Headlight: next.headlight = !now.headlight; break;
		case ViewToggle::Brighter: next.brightness = held + kBrightnessStep > kBrightnessMost ? kBrightnessMost : held + kBrightnessStep; break;
		case ViewToggle::Darker: next.brightness = held - kBrightnessStep < kBrightnessLeast ? kBrightnessLeast : held - kBrightnessStep; break;
	}

	return next;
}

// THE TABLE: what the game is given for a state. The wire for Polyline and the textured wireframe; white outlines for Polyline and
// Filled polyline, in the level's light only for Polyline with the switch On (inteng's own lines are hard); fully lit whenever the switch
// is Off, in every mode. Nothing depends on the state before.
static GameViewSwitches TableSwitches(const ViewToggles &t, const GameCanShow &can)
{
	bool const off = t.gouraud == Gouraud::Off;
	GameViewSwitches s;
	s.wireFrameMode = (t.drawMode == DrawMode::Polyline || t.drawMode == DrawMode::WireTextured) ? kWireFrameEverything : kWireFrameOff;
	s.filledOutlines = can.outlines && (t.drawMode == DrawMode::Polyline || t.drawMode == DrawMode::FilledPolyline) ? 1 : 0;
	s.outlineColour = kWireWhite;
	s.outlinesLit = (t.drawMode == DrawMode::Polyline || t.drawMode == DrawMode::FilledPolyline) && !off ? 1 : 0;
	s.drawFullBright = off ? 1 : 0;
	s.drawBackFaces = can.backFaces && t.backFaces ? 1 : 0;
	s.headlight = t.headlight ? 1 : 0;
	s.untexturedColour = kUntexturedColour;   // every state here has its textures on: the plain colour is the white one
	s.gamma = t.brightness < kBrightnessLeast ? kBrightnessLeast : (t.brightness > kBrightnessMost ? kBrightnessMost : t.brightness);
	return s;
}

static const GameCanShow kCans[] = { { false, false }, { true, false }, { false, true }, { true, true } };

// Every state: each draw mode the game can show, with each Gouraud value and every mix of the toggles it can show, at each brightness
template <typename Visit> static void EveryState(const GameCanShow &can, Visit visit)
{
	for (DrawMode mode : kModes)
	{
		if ((mode == DrawMode::Polyline || mode == DrawMode::FilledPolyline) && !can.outlines)
		{
			continue;
		}
		for (Gouraud g : kGouraud)
		{
			for (int back = 0; back < (can.backFaces ? 2 : 1); back++)
			{
				for (int head = 0; head < 2; head++)
				{
					for (int32_t bright : kBrightnesses)
					{
						ViewToggles const t = { mode, g, back != 0, head != 0, bright, true };
						visit(t);
					}
				}
			}
		}
	}
}

static void TestEveryMove()
{
	puts("Every move from every state:");

	for (const GameCanShow &can : kCans)
	{
		size_t moves = 0, nextWrong = 0, switchesWrong = 0;
		std::string first;

		EveryState(can, [&](const ViewToggles &now) {
			for (ViewToggle item : kItems)
			{
				ViewToggles const after = ViewToggles_Use(now, item, can);
				ViewToggles const want = TableNext(now, item, can);
				moves++;
				if (!Same(after, want))
				{
					nextWrong++;
					if (first.empty()) first = "from " + Describe(now) + ", " + ItemName(item) + " gave " + Describe(after) + ", not " + Describe(want);
				}
				if (!Same(ViewToggles_GameSwitches(after, can), TableSwitches(want, can)))
				{
					switchesWrong++;
					if (first.empty()) first = "from " + Describe(now) + ", " + ItemName(item) + ": the game's switches aren't the table's";
				}
			}
		});

		char words[200];
		snprintf(words, sizeof(words), "with the game %s back faces and %s outlines: %u moves, each to the table's next state and the table's switches",
			can.backFaces ? "showing" : "not showing", can.outlines ? "showing" : "not showing", (unsigned)moves);
		Check(nextWrong == 0 && switchesWrong == 0 && moves > 0, words);
		if (!first.empty())
		{
			printf("          the first that wasn't: %s\n", first.c_str());
		}
	}
}

static void TestTheCleanStart()
{
	puts("From a clean start:");

	GameCanShow const can = { true, true };
	ViewToggles const clean = ViewToggles_AsBuilt();
	ViewToggles const want = { DrawMode::Polygon, Gouraud::On, false, false, kBrightnessAsBuilt };
	Check(Same(clean, want), "a clean start is Polygon with the Gouraud switch on, every toggle off and the brightness as built: " + Describe(clean));
	Check(Same(ViewToggles_GameSwitches(clean, can), TableSwitches(want, can)), "and the game is given the table's switches for it");

	// Every item that's on from a clean start changes something; the mode already on (Polygon) is the table's one exception
	for (ViewToggle item : kItems)
	{
		ViewToggles const after = ViewToggles_Use(clean, item, can);
		bool const changed = !Same(after, clean);
		if (item == ViewToggle::Polygon)
		{
			Check(!changed, std::string("from a clean start, ") + ItemName(item) + ", the mode already on, changes nothing (inteng sets it again)");
		}
		else
		{
			Check(changed && Same(after, TableNext(clean, item, can)), std::string("from a clean start, the first choice of ") + ItemName(item)
				+ " changes the state, to the table's: " + Describe(after));
		}
	}

	// Melanikus's report: Polyline first, then other items, then Polyline again. Polyline gives the game the wire and the white lines
	// every time, whatever came before, and the same switches as from a clean start but for the switch and toggles still as they were left.
	ViewToggles const firstPolyline = ViewToggles_Use(clean, ViewToggle::Polyline, can);
	GameViewSwitches const firstSwitches = ViewToggles_GameSwitches(firstPolyline, can);
	Check(firstSwitches.wireFrameMode == kWireFrameEverything && firstSwitches.filledOutlines == 1 && firstSwitches.outlineColour == kWireWhite,
		"Polyline as the very first choice gives the game the wire and white lines");
	bool always = true;
	std::string first;
	for (ViewToggle between : kItems)
	{
		ViewToggles const again = ViewToggles_Use(ViewToggles_Use(firstPolyline, between, can), ViewToggle::Polyline, can);
		ViewToggles cleanThenSame = firstPolyline;
		cleanThenSame.gouraud = again.gouraud;
		cleanThenSame.backFaces = again.backFaces;
		cleanThenSame.headlight = again.headlight;
		cleanThenSame.brightness = again.brightness;
		GameViewSwitches const got = ViewToggles_GameSwitches(again, can);
		bool const good = got.wireFrameMode == kWireFrameEverything && Same(got, ViewToggles_GameSwitches(cleanThenSame, can));
		if (!good && first.empty()) first = ItemName(between);
		always = always && good;
	}
	Check(always, "Polyline, then any one item, then Polyline again gives the wire, and the same as from a clean start but for the switch and toggles left as they were"
		+ (first.empty() ? std::string() : " (it didn't after " + first + ")"));
}

// The Gouraud switch is kept across every mode change, and every mode is lit or unlit by it alone: Melanikus's four wireframe kinds are
// the two wire modes times the switch
static void TestTheSwitchAcrossModes()
{
	puts("The Gouraud switch across modes:");

	GameCanShow const can = { true, true };
	bool kept = true;
	for (DrawMode from : kModes)
	{
		for (Gouraud g : kGouraud)
		{
			ViewToggles const now = { from, g, false, false, kBrightnessAsBuilt };
			for (ViewToggle item : { ViewToggle::Polygon, ViewToggle::Polyline, ViewToggle::FilledPolyline, ViewToggle::WireTextured })
			{
				kept = kept && ViewToggles_Use(now, item, can).gouraud == g;
			}
		}
	}
	Check(kept, "from every mode, with the switch either way, choosing any mode leaves the switch as it was");

	ViewToggles texturedLit = { DrawMode::WireTextured, Gouraud::On, false, false, kBrightnessAsBuilt, true };
	ViewToggles texturedUnlit = texturedLit;
	texturedUnlit.gouraud = Gouraud::Off;
	ViewToggles whiteLit = texturedLit;
	whiteLit.drawMode = DrawMode::Polyline;
	ViewToggles whiteUnlit = whiteLit;
	whiteUnlit.gouraud = Gouraud::Off;
	GameViewSwitches const a = ViewToggles_GameSwitches(texturedLit, can), b = ViewToggles_GameSwitches(texturedUnlit, can);
	GameViewSwitches const c = ViewToggles_GameSwitches(whiteLit, can), d = ViewToggles_GameSwitches(whiteUnlit, can);
	Check(a.wireFrameMode == kWireFrameEverything && a.drawFullBright == 0 && a.filledOutlines == 0, "textured wireframe, lit: the game's lines in the level's light");
	Check(b.wireFrameMode == kWireFrameEverything && b.drawFullBright == 1 && b.filledOutlines == 0, "textured wireframe, unlit: the same lines, the textures at 100%");
	Check(c.filledOutlines == 1 && c.outlineColour == kWireWhite && c.outlinesLit == 1, "white wireframe, lit: white lines in the level's light (ours, his request)");
	Check(d.filledOutlines == 1 && d.outlineColour == kWireWhite && d.outlinesLit == 0, "white wireframe, unlit: hard white lines, inteng's own 8");
}

// The game is given the same switches for the same state, whatever came before
static void TestNoHistory()
{
	puts("No history:");

	GameCanShow const can = { true, true };
	bool same = true;
	EveryState(can, [&](const ViewToggles &now) {
		for (ViewToggle a : kItems)
		{
			for (ViewToggle b : kItems)
			{
				ViewToggles const viaA = ViewToggles_Use(ViewToggles_Use(now, a, can), b, can);
				same = same && Same(ViewToggles_GameSwitches(viaA, can), TableSwitches(viaA, can));
			}
		}
	});
	Check(same, "after every two moves from every state, the game's switches are the table's for where it ended");
}

int main()
{
	TestEveryMove();
	TestTheCleanStart();
	TestTheSwitchAcrossModes();
	TestNoHistory();

	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
