// The test for src/host's view toggles, built to AvP-docs\engine\inteng\view-state-table.md (the old inteng's own draw modes and Gouraud
// switch): the clean start, each mode and the Gouraud switch, the game's switches they set, brightness, the View menu's lines that use
// them, and the proof runs' -viewmode words. Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see
// License.txt, "AVP_OpenInteng's own code". (Modified 2026-11-14 by Melanikus (Frost): reshaped to inteng's. Apone's
// view_toggles_order_test checks every move from every state against the table; this one checks each rule plainly, one at a time.)
//
// It needs no window and none of the game: the toggles are plain data, and the switches they'd give the game are plain numbers. Each
// check's words say what it proves. The program gives back a non-zero result if any check failed. tests/takeouts/view_toggles_test.json
// proves each check can fail.

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>

#include "editor_actions.h"
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

// What the editor's copy of the game can show: nothing extra; and both, as once its avpx carries R-BACKFACES-1 and R-DRAWMODES-1
static GameCanShow const kNone = { false, false };
static GameCanShow const kBoth = { true, true };

static ViewToggles As(DrawMode mode, Gouraud gouraud)
{
	ViewToggles t = ViewToggles_AsBuilt();
	t.drawMode = mode;
	t.gouraud = gouraud;
	return t;
}

static void TestModesAndTheSwitch()
{
	puts("The draw mode and the Gouraud switch:");

	ViewToggles const start = ViewToggles_AsBuilt();
	Check(start.drawMode == DrawMode::Polygon && start.gouraud == Gouraud::On && !start.backFaces && !start.headlight && start.brightness == 128,
		"the clean start is Polygon with the Gouraud switch on, no back faces, no headlight, at a new player's brightness");

	Check(ViewToggles_Use(start, ViewToggle::Polyline, kBoth).drawMode == DrawMode::Polyline
		&& ViewToggles_Use(start, ViewToggle::FilledPolyline, kBoth).drawMode == DrawMode::FilledPolyline
		&& ViewToggles_Use(start, ViewToggle::WireTextured, kBoth).drawMode == DrawMode::WireTextured
		&& ViewToggles_Use(As(DrawMode::Polyline, Gouraud::On), ViewToggle::Polygon, kBoth).drawMode == DrawMode::Polygon,
		"each mode's item picks that mode, and one replaces another");

	ViewToggles const lines = ViewToggles_Use(start, ViewToggle::Polyline, kBoth);
	ViewToggles const again = ViewToggles_Use(lines, ViewToggle::Polyline, kBoth);
	Check(again.drawMode == lines.drawMode && again.gouraud == lines.gouraud && again.backFaces == lines.backFaces && again.headlight == lines.headlight
		&& again.brightness == lines.brightness, "picking the mode already on changes nothing");

	ViewToggles const off = ViewToggles_Use(start, ViewToggle::GouraudSwitch, kBoth);
	Check(off.gouraud == Gouraud::Off && ViewToggles_Use(off, ViewToggle::GouraudSwitch, kBoth).gouraud == Gouraud::On && off.drawMode == DrawMode::Polygon,
		"the Gouraud item turns the switch off, and again on, and leaves the mode as it was");

	bool kept = true;
	ViewToggle const modes[] = { ViewToggle::Polygon, ViewToggle::Polyline, ViewToggle::FilledPolyline, ViewToggle::WireTextured };

	for (ViewToggle from : modes)
	{
		for (ViewToggle to : modes)
		{
			ViewToggles const there = ViewToggles_Use(ViewToggles_Use(off, from, kBoth), to, kBoth);
			kept = kept && there.gouraud == Gouraud::Off;
		}
	}

	Check(kept, "the Gouraud switch is kept across every mode change, from every mode to every other");

	Check(ViewToggles_Use(start, ViewToggle::Polyline, kNone).drawMode == DrawMode::Polygon
		&& ViewToggles_Use(start, ViewToggle::FilledPolyline, kNone).drawMode == DrawMode::Polygon
		&& ViewToggles_Use(start, ViewToggle::WireTextured, kNone).drawMode == DrawMode::WireTextured,
		"while the game can't show the white lines, Polyline and Filled polyline change nothing; the textured wireframe always works");

	Check(!ViewToggles_Use(start, ViewToggle::BackFaces, kNone).backFaces && ViewToggles_Use(start, ViewToggle::BackFaces, kBoth).backFaces,
		"back faces stay off while the game can't show them, and turn on once it can");

	ViewToggles shown = start;
	shown.backFaces = true;
	Check(!ViewToggles_Use(shown, ViewToggle::BackFaces, kNone).backFaces, "and turn off, never on, if they were somehow on while they can't be shown");

	ViewToggles const lit = ViewToggles_Use(start, ViewToggle::Headlight, kBoth);
	Check(lit.headlight && !ViewToggles_Use(lit, ViewToggle::Headlight, kBoth).headlight && ViewToggles_Use(lit, ViewToggle::Polyline, kBoth).headlight,
		"the headlight turns on and off, and is kept across a mode change");
}

// Textured / Untextured (inteng's DAT_005fd6b4, run 25): starts textured, turns over with its own item, and is separate from every draw mode and from Gouraud
static void TestTextures()
{
	puts("Textured and Untextured:");

	Check(ViewToggles_AsBuilt().textured, "the editor starts textured, as inteng does");

	ViewToggles now = ViewToggles_AsBuilt();
	now = ViewToggles_Use(now, ViewToggle::Textures, kBoth);
	Check(!now.textured && now.drawMode == DrawMode::Polygon && now.gouraud == Gouraud::On, "the item turns textures off and changes nothing else");
	now = ViewToggles_Use(now, ViewToggle::FilledPolyline, kBoth);
	Check(!now.textured && now.drawMode == DrawMode::FilledPolyline, "a draw mode picked later leaves it off: Filled polyline with the textures off");
	now = ViewToggles_Use(now, ViewToggle::Textures, kBoth);
	Check(now.textured, "and the item turns it on again");

	DrawMode const modes[] = { DrawMode::Polygon, DrawMode::Polyline, DrawMode::FilledPolyline, DrawMode::WireTextured };
	bool everyMode = true;

	for (DrawMode mode : modes)
	{
		ViewToggles plain = As(mode, Gouraud::On);
		ViewToggles dressed = plain;

		plain.textured = false;
		dressed.textured = true;
		everyMode = everyMode && ViewToggles_GameSwitches(plain, kBoth).untextured == 1 && ViewToggles_GameSwitches(dressed, kBoth).untextured == 0;
	}

	Check(everyMode, "the game's Untextured follows the switch in every draw mode");
	Check(std::string(ViewToggles_Words(now, ViewToggle::Textures, kBoth)) == "Textures: on", "the status line says what is on");
}

static void TestGameSwitches()
{
	puts("The game's switches, from where the toggles are:");

	GameViewSwitches const polygonOn = ViewToggles_GameSwitches(As(DrawMode::Polygon, Gouraud::On), kBoth);
	GameViewSwitches const polygonOff = ViewToggles_GameSwitches(As(DrawMode::Polygon, Gouraud::Off), kBoth);
	Check(polygonOn.wireFrameMode == 0 && polygonOn.filledOutlines == 0 && polygonOn.drawFullBright == 0 && polygonOff.drawFullBright == 1,
		"Polygon: solid, no lines; the Gouraud switch off is the game's fully lit, textures at 100%");

	GameViewSwitches const whiteLit = ViewToggles_GameSwitches(As(DrawMode::Polyline, Gouraud::On), kBoth);
	GameViewSwitches const whiteUnlit = ViewToggles_GameSwitches(As(DrawMode::Polyline, Gouraud::Off), kBoth);
	Check(whiteUnlit.wireFrameMode == 3 && whiteUnlit.filledOutlines == 1 && whiteUnlit.outlineColour == kWireWhite && whiteUnlit.outlinesLit == 0,
		"Polyline with the switch off: everything as lines, covered by hard white outlines, inteng's own 8");
	Check(whiteLit.wireFrameMode == 3 && whiteLit.filledOutlines == 1 && whiteLit.outlineColour == kWireWhite && whiteLit.outlinesLit == 1
		&& whiteLit.drawFullBright == 0, "and with it on: the white lines in the level's own light");

	GameViewSwitches const filledOn = ViewToggles_GameSwitches(As(DrawMode::FilledPolyline, Gouraud::On), kBoth);
	GameViewSwitches const filledOff = ViewToggles_GameSwitches(As(DrawMode::FilledPolyline, Gouraud::Off), kBoth);
	Check(filledOn.wireFrameMode == 0 && filledOn.filledOutlines == 1 && filledOn.outlineColour == kWireWhite && filledOn.outlinesLit == 1
		&& filledOff.outlinesLit == 0 && filledOff.drawFullBright == 1,
		"Filled polyline: the polygons solid with white lines, shaded by the level's light with the switch on, hard white with it off (his request)");

	// Filled polyline with the textures off is the hidden-line look: flat black faces, hard white edges, whatever the Gouraud switch says; nothing else changes
	{
		ViewToggles bare = As(DrawMode::FilledPolyline, Gouraud::On);
		bare.textured = false;

		GameViewSwitches const hiddenOn = ViewToggles_GameSwitches(bare, kBoth);
		bare.gouraud = Gouraud::Off;
		GameViewSwitches const hiddenOff = ViewToggles_GameSwitches(bare, kBoth);
		Check(hiddenOn.untextured == 1 && hiddenOn.untexturedColour == kHiddenLineFill && hiddenOn.filledOutlines == 1 && hiddenOn.outlineColour == kWireWhite
			&& hiddenOn.outlinesLit == 0 && hiddenOn.wireFrameMode == 0 && hiddenOn.drawFullBright == 1,
			"Filled polyline with the textures off, switch on: flat black faces, the white edges hard (not in the level's light)");
		Check(hiddenOn.outlineDepthBias == kHiddenLineDepthBias && kHiddenLineDepthBias < kOutlineDepthBias,
			"the hidden-line look pulls its edges toward the camera harder than the outlines always were (so no edge is cut into dashes by the faces)");
		Check(hiddenOff.untexturedColour == kHiddenLineFill && hiddenOff.outlinesLit == 0 && hiddenOff.filledOutlines == 1,
			"and with the Gouraud switch off the same: black faces, hard white edges");

		ViewToggles dressed = As(DrawMode::FilledPolyline, Gouraud::On);
		dressed.textured = true;

		GameViewSwitches const kept = ViewToggles_GameSwitches(dressed, kBoth);
		Check(kept.untextured == 0 && kept.untexturedColour == kUntexturedColour && kept.outlinesLit == 1 && kept.filledOutlines == 1,
			"Filled polyline with the textures ON is as it was: the level's own look, the edges in the level's light");

		Check(ViewToggles_GameSwitches(bare, kNone).untexturedColour == kUntexturedColour,
			"a game that can't show the white edges gets no black faces either: a black polygon with no lines would show nothing");

		DrawMode const everyMode[] = { DrawMode::Polygon, DrawMode::Polyline, DrawMode::FilledPolyline, DrawMode::WireTextured };
		bool otherwisePlain = true;

		for (DrawMode mode : everyMode)
		{
			for (Gouraud gouraud : { Gouraud::On, Gouraud::Off })
			{
				for (bool textured : { true, false })
				{
					ViewToggles state = As(mode, gouraud);
					state.textured = textured;

					bool const hidden = mode == DrawMode::FilledPolyline && !textured;
					otherwisePlain = otherwisePlain && ViewToggles_GameSwitches(state, kBoth).untexturedColour == (hidden ? kHiddenLineFill : kUntexturedColour);
				}
			}
		}

		bool outlinesAsAlways = true;

		for (DrawMode mode : everyMode)
		{
			for (bool textured : { true, false })
			{
				ViewToggles state = As(mode, Gouraud::On);
				state.textured = textured;

				GameViewSwitches const got = ViewToggles_GameSwitches(state, kBoth);
				bool const hidden = mode == DrawMode::FilledPolyline && !textured;

				outlinesAsAlways = outlinesAsAlways && got.outlineDepthBias == (hidden ? kHiddenLineDepthBias : kOutlineDepthBias);
			}
		}

		Check(otherwisePlain && outlinesAsAlways, "every other draw mode, Gouraud value and textures setting keeps the plain white colour and the outlines' own pull");
	}

	GameViewSwitches const texturedOn = ViewToggles_GameSwitches(As(DrawMode::WireTextured, Gouraud::On), kBoth);
	GameViewSwitches const texturedOff = ViewToggles_GameSwitches(As(DrawMode::WireTextured, Gouraud::Off), kBoth);
	Check(texturedOn.wireFrameMode == 3 && texturedOn.filledOutlines == 0 && texturedOn.drawFullBright == 0 && texturedOff.drawFullBright == 1,
		"the textured wireframe: the game's own lines, in the level's light with the switch on and at 100% with it off");

	Check(ViewToggles_GameSwitches(As(DrawMode::Polyline, Gouraud::On), kNone).filledOutlines == 0
		&& ViewToggles_GameSwitches(As(DrawMode::FilledPolyline, Gouraud::On), kNone).filledOutlines == 0,
		"no outlines are asked of a game that can't show them, whatever the mode says");

	ViewToggles wrongMode = ViewToggles_AsBuilt();
	wrongMode.drawMode = (DrawMode)77;
	wrongMode.gouraud = (Gouraud)9;
	GameViewSwitches const wrong = ViewToggles_GameSwitches(wrongMode, kBoth);
	Check(wrong.wireFrameMode == 0 && wrong.filledOutlines == 0 && wrong.drawFullBright == 0,
		"a mode or a switch value that isn't one of the table's is drawn as the clean start draws: solid, shaded");

	ViewToggles all = As(DrawMode::Polygon, Gouraud::On);
	all.backFaces = true;
	all.headlight = true;
	all.brightness = 200;
	GameViewSwitches const allOn = ViewToggles_GameSwitches(all, kBoth);
	Check(allOn.drawBackFaces == 1 && allOn.headlight == 1 && allOn.gamma == 200 && ViewToggles_GameSwitches(all, kNone).drawBackFaces == 0,
		"back faces, the headlight and the brightness reach the game; back faces never while it can't show them");
}

static void TestBrightness()
{
	puts("Brightness:");

	ViewToggles const start = ViewToggles_AsBuilt();
	Check(ViewToggles_Use(start, ViewToggle::Brighter, kNone).brightness == 128 + kBrightnessStep
		&& ViewToggles_Use(start, ViewToggle::Darker, kNone).brightness == 128 - kBrightnessStep, "Brighter and Darker move it a step of 16");

	ViewToggles nearTop = start;
	nearTop.brightness = 250;
	ViewToggles nearBottom = start;
	nearBottom.brightness = 10;
	Check(ViewToggles_Use(nearTop, ViewToggle::Brighter, kNone).brightness == 255 && ViewToggles_Use(nearBottom, ViewToggle::Darker, kNone).brightness == 0,
		"a step past the ends stops at them, 255 and 0, the game's own slider's range");

	int32_t const hostile[] = { -1000, 100000, INT32_MIN, INT32_MAX };
	bool held = true;

	for (int32_t value : hostile)
	{
		ViewToggles wrong = start;
		wrong.brightness = value;
		int32_t const up = ViewToggles_Use(wrong, ViewToggle::Brighter, kNone).brightness;
		int32_t const down = ViewToggles_Use(wrong, ViewToggle::Darker, kNone).brightness;
		held = held && up >= 0 && up <= 255 && down >= 0 && down <= 255 && ViewToggles_GameSwitches(wrong, kNone).gamma >= 0
			&& ViewToggles_GameSwitches(wrong, kNone).gamma <= 255;
	}

	Check(held, "a brightness out of range, even the largest and smallest numbers there are, never reaches the game out of 0 to 255");
	Check(ViewToggles_HeldBrightness(INT64_MIN) == 0 && ViewToggles_HeldBrightness(INT64_MAX) == 255 && ViewToggles_HeldBrightness(77) == 77,
		"and it's held to 0 to 255 whatever it's asked");
}

static void TestWords()
{
	puts("What the status line says:");

	ViewToggles const start = ViewToggles_AsBuilt();
	Check(strcmp(ViewToggles_Words(ViewToggles_Use(start, ViewToggle::Polyline, kBoth), ViewToggle::Polyline, kBoth),
		"Draw mode: polyline, every polygon a white line") == 0, "it says the draw mode is polyline now");
	Check(strstr(ViewToggles_Words(start, ViewToggle::Polyline, kNone), "can't be shown yet") != 0
		&& strstr(ViewToggles_Words(start, ViewToggle::BackFaces, kNone), "can't be shown yet") != 0,
		"and says why polyline or back faces can't be shown yet");
	Check(strcmp(ViewToggles_Words(ViewToggles_Use(start, ViewToggle::GouraudSwitch, kBoth), ViewToggle::GouraudSwitch, kBoth),
		"Gouraud: off, textures at 100%, unshaded") == 0, "and whether the Gouraud switch is off or on");
}

static void TestTheEditorsToggles()
{
	puts("The editor's toggles, and the View menu's lines:");

	Check(!ViewToggles_BackFacesCanBeShown(), "back faces can't be shown until the editor's game carries R-BACKFACES-1");
	Check(ViewToggles_OutlinesCanBeShown(), "and the editor's game carries R-DRAWMODES-1, so Polyline's and Filled polyline's white lines can be shown");

	ViewToggles const before = ViewToggles_Now();
	Check(before.drawMode == DrawMode::Polygon && before.gouraud == Gouraud::On, "the editor starts at the clean start");

	struct Line { int32_t action; const char *name; };
	Line const lines[] = { { kActionViewPolygon, "Polygon" }, { kActionViewPolyline, "Polyline" }, { kActionViewFilledPolyline, "Filled polyline" },
		{ kActionViewWireTextured, "Wireframe, textured" }, { kActionViewGouraud, "Gouraud" }, { kActionViewHeadlight, "Headlight" },
		{ kActionViewBackFaces, "Back faces" }, { kActionViewBrighter, "Brighter" }, { kActionViewDarker, "Darker" } };
	bool allThere = true;

	for (const Line &line : lines)
	{
		const EditorAction *const action = EditorActions_Find(line.action);
		allThere = allThere && action && action->menu == EditorMenu::View && strcmp(action->name, line.name) == 0 && action->run;
	}

	Check(allThere, "the View menu has its nine lines, each with its words and its run");
	Check(!EditorActions_Find(102) && !EditorActions_Find(112) && EditorActions_Find(111) && EditorActions_Find(111)->menu == EditorMenu::View,
		"and the retired 102 and 112 are gone, so nothing can name them, and 111 is the View menu's Object list (L-1)");

	const EditorAction *const headlight = EditorActions_Find(kActionViewHeadlight);
	char headlightWords[kEditorKeyWordsRoom];
	Check(headlight && headlight->classic.virtualKey == 'B' && headlight->classic.ctrl && !headlight->classic.shift
		&& headlight->modern.virtualKey == 'B' && headlight->modern.ctrl && !headlight->modern.shift
		&& EditorActions_KeyWords(headlight->classic, headlightWords, sizeof(headlightWords)) && strcmp(headlightWords, "Ctrl+B") == 0,
		"the Headlight line's key is Ctrl+B, inteng's, in both key layouts, and the menu says so");

	EditorHostState host = { true, 0 };
	EditorActions_Find(kActionViewPolyline)->run(&host);
	Check(ViewToggles_Now().drawMode == DrawMode::Polyline && host.running && host.statusWords
		&& strcmp(host.statusWords, "Draw mode: polyline, every polygon a white line") == 0, "its Polyline line picks polyline, and the status line says so");

	EditorActions_Find(kActionViewGouraud)->run(&host);
	Check(ViewToggles_Now().gouraud == Gouraud::Off && ViewToggles_Now().drawMode == DrawMode::Polyline, "its Gouraud line turns the switch off and keeps the mode");

	EditorActions_Find(kActionViewBrighter)->run(&host);
	Check(ViewToggles_Now().brightness == 144, "its Brighter line moves the editor's brightness a step");

	ui::Menu menus[(size_t)EditorMenu::Count];
	ui::MenuItem items[64];
	EditorKeyWords words[64];
	bool const made = EditorActions_MakeMenus(kEditorActions, kEditorActionCount, KeyLayout::Classic, menus, (size_t)EditorMenu::Count, items, 64,
		words, 64);
	Check(made && menus[1].count == 15 && menus[1].items[0].action == kActionViewPolygon && menus[1].items[2].action == kActionViewFilledPolyline
		&& menus[1].items[11].action == kActionViewTextures && menus[1].items[12].action == kActionViewGouraudLevelOnly && menus[1].items[13].action == kActionViewKeysClassic
		&& menus[1].items[14].action == kActionViewKeysModern,
		"the View menu made from the table starts with inteng's three modes, in order: 7, 8, 9, and ends with the Object list, Textures, Gouraud's third state (off) and the two key layouts");
}

// -viewmode's word, for the proof runs (ViewToggles_FromCommandWord): only the exact words
static void TestCommandWord()
{
	puts("The proof runs' -viewmode word:");

	struct Good { const wchar_t *word; size_t count; ViewToggle first; ViewToggle second; };
	Good const good[] = { { L"normal", 0, ViewToggle::Darker, ViewToggle::Darker }, { L"polygon", 1, ViewToggle::Polygon, ViewToggle::Darker },
		{ L"polyline", 1, ViewToggle::Polyline, ViewToggle::Darker }, { L"filledpolyline", 1, ViewToggle::FilledPolyline, ViewToggle::Darker },
		{ L"wire-textured", 1, ViewToggle::WireTextured, ViewToggle::Darker }, { L"unlit", 1, ViewToggle::GouraudSwitch, ViewToggle::Darker },
		{ L"headlight", 1, ViewToggle::Headlight, ViewToggle::Darker }, { L"polyline+unlit", 2, ViewToggle::Polyline, ViewToggle::GouraudSwitch },
		{ L"wire-textured+unlit", 2, ViewToggle::WireTextured, ViewToggle::GouraudSwitch },
		{ L"normal+headlight", 1, ViewToggle::Headlight, ViewToggle::Darker } };
	bool allTaken = true;

	for (const Good &g : good)
	{
		ViewToggle toggles[kCommandWordMostToggles] = { ViewToggle::Darker, ViewToggle::Darker };
		size_t count = 99;
		allTaken = allTaken && ViewToggles_FromCommandWord(g.word, toggles, kCommandWordMostToggles, &count) && count == g.count
			&& toggles[0] == g.first && toggles[1] == g.second;
	}

	Check(allTaken, "each of the ten words is taken, as the View menu's own items, in order");

	std::wstring const longWord(100000, L'p');
	const wchar_t *const bad[] = { L"", L"Polyline", L"polyline ", L" polyline", L"polylinex", L"poly", L"wireframe", L"flat", L"fullbright",
		L"normal\x00e9", longWord.c_str(), L"polyline+", L"+unlit", L"unlit+polyline", L"polyline+unlit+headlight", L"polyline unlit",
		L"polyline+Unlit", L"wire-green-lit" };
	bool allRefused = true;

	for (const wchar_t *word : bad)
	{
		ViewToggle toggles[kCommandWordMostToggles] = { ViewToggle::Darker, ViewToggle::Darker };
		size_t count = 99;
		allRefused = allRefused && !ViewToggles_FromCommandWord(word, toggles, kCommandWordMostToggles, &count) && count == 99
			&& toggles[0] == ViewToggle::Darker && toggles[1] == ViewToggle::Darker;
	}

	ViewToggle toggles[kCommandWordMostToggles] = { ViewToggle::Darker, ViewToggle::Darker };
	size_t count = 99;
	Check(allRefused && !ViewToggles_FromCommandWord(nullptr, toggles, kCommandWordMostToggles, &count)
		&& !ViewToggles_FromCommandWord(L"polyline", nullptr, kCommandWordMostToggles, &count)
		&& !ViewToggles_FromCommandWord(L"polyline", toggles, kCommandWordMostToggles, nullptr)
		&& !ViewToggles_FromCommandWord(L"polyline+unlit", toggles, 1, &count) && count == 99 && toggles[0] == ViewToggle::Darker,
		"anything else is refused, with nothing written: another case, a space, a longer or shorter word, a retired mode, an accent, 100,000 letters,"
		" the two in the other order, a third, no word, or too little room");
}

int main()
{
	setvbuf(stdout, NULL, _IONBF, 0);

	TestModesAndTheSwitch();
	TestTextures();
	TestGameSwitches();
	TestBrightness();
	TestWords();
	TestTheEditorsToggles();
	TestCommandWord();

	printf("%s: %d check(s) failed\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed", checksFailed);
	return checksFailed ? 1 : 0;
}
