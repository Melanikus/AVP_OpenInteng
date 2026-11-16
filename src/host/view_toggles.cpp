// See view_toggles.h. Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code". (Modified 2026-11-14 by Melanikus (Frost): the draw modes and the headlight; then reshaped to the old
// inteng's own, view-state-table.md.)

#include <wchar.h>

#include "view_toggles.h"

namespace host
{
	ViewToggles ViewToggles_AsBuilt()
	{
		ViewToggles const toggles = { DrawMode::Polygon, Gouraud::On, false, false, kBrightnessAsBuilt, true };
		return toggles;
	}

	int32_t ViewToggles_HeldBrightness(int64_t asked)
	{
		return asked < kBrightnessLeast ? kBrightnessLeast : asked > kBrightnessMost ? kBrightnessMost : (int32_t)asked;
	}

	ViewToggles ViewToggles_Use(const ViewToggles &now, ViewToggle which, const GameCanShow &can)
	{
		ViewToggles after = now;

		switch (which)
		{
			case ViewToggle::Polygon:
				after.drawMode = DrawMode::Polygon;
				break;
			case ViewToggle::Polyline:
				if (can.outlines)
				{
					after.drawMode = DrawMode::Polyline;
				}
				break;
			case ViewToggle::FilledPolyline:
				if (can.outlines)
				{
					after.drawMode = DrawMode::FilledPolyline;
				}
				break;
			case ViewToggle::WireTextured:
				after.drawMode = DrawMode::WireTextured;
				break;
			case ViewToggle::GouraudSwitch:
				after.gouraud = now.gouraud == Gouraud::Off ? Gouraud::On : Gouraud::Off;
				break;
			case ViewToggle::BackFaces:
				after.backFaces = can.backFaces && !now.backFaces;
				break;
			case ViewToggle::Headlight:
				after.headlight = !now.headlight;
				break;
			case ViewToggle::Brighter:
				after.brightness = ViewToggles_HeldBrightness((int64_t)ViewToggles_HeldBrightness(now.brightness) + kBrightnessStep);
				break;
			case ViewToggle::Darker:
				after.brightness = ViewToggles_HeldBrightness((int64_t)ViewToggles_HeldBrightness(now.brightness) - kBrightnessStep);
				break;
			case ViewToggle::Textures:
				after.textured = !now.textured;
				break;
		}

		return after;
	}

	GameViewSwitches ViewToggles_GameSwitches(const ViewToggles &toggles, const GameCanShow &can)
	{
		// Each mode is asked for by name, so a draw mode that's none of the four is drawn as Polygon, which sets none of the mode switches;
		// and only Off is unlit, so a Gouraud value that isn't Off is drawn On
		DrawMode const mode = toggles.drawMode;
		bool const unlit = toggles.gouraud == Gouraud::Off;
		bool const lines = mode == DrawMode::Polyline || mode == DrawMode::WireTextured;
		bool const whiteLines = can.outlines && (mode == DrawMode::Polyline || mode == DrawMode::FilledPolyline);

		GameViewSwitches switches;
		switches.wireFrameMode = lines ? kWireFrameEverything : kWireFrameOff;
		// Filled polyline with the textures off is the hidden-line look (kHiddenLineFill, view_toggles.h): black faces, hard white edges, whatever the Gouraud switch says.
		// The game is told fully lit too, so none of its light passes can touch the black.
		bool const hiddenLine = whiteLines && mode == DrawMode::FilledPolyline && !toggles.textured;
		switches.drawFullBright = unlit || hiddenLine ? 1 : 0;
		switches.filledOutlines = whiteLines ? 1 : 0;
		switches.outlineColour = kWireWhite;
		// The white lines of Polyline AND of Filled polyline follow the switch: On (no B), shaded by the level's own light; Off (B pressed, fully
		// lit), hard white. (Owner, 2026-11-15, through Bishop: "I want shaded poly line if no B", "9 shaded wires too, right now they are hard
		// white with B off"; B pressed is the fully lit state, owner-intent.md item 3.)
		switches.outlinesLit = whiteLines && !unlit && !hiddenLine ? 1 : 0;
		switches.drawBackFaces = can.backFaces && toggles.backFaces ? 1 : 0;
		switches.headlight = toggles.headlight ? 1 : 0;
		switches.gamma = ViewToggles_HeldBrightness(toggles.brightness);
		switches.untextured = toggles.textured ? 0 : 1;
		switches.untexturedColour = hiddenLine ? kHiddenLineFill : kUntexturedColour;
		switches.outlineDepthBias = hiddenLine ? kHiddenLineDepthBias : kOutlineDepthBias;
		return switches;
	}

	const char *ViewToggles_Words(const ViewToggles &after, ViewToggle which, const GameCanShow &can)
	{
		switch (which)
		{
			case ViewToggle::Polygon:
				return "Draw mode: polygon, the polygons solid";
			case ViewToggle::Polyline:
				if (!can.outlines)
				{
					return "Polyline can't be shown yet: this editor's copy of the game doesn't have its white lines";
				}

				return "Draw mode: polyline, every polygon a white line";
			case ViewToggle::FilledPolyline:
				if (!can.outlines)
				{
					return "Filled polyline can't be shown yet: this editor's copy of the game doesn't have its white lines";
				}

				return "Draw mode: filled polyline, the polygons with white lines";
			case ViewToggle::WireTextured:
				return "Draw mode: wireframe, textured, each line in its texture's colour";
			case ViewToggle::GouraudSwitch:
				return after.gouraud == Gouraud::Off ? "Gouraud: off, textures at 100%, unshaded" : "Gouraud: on, shaded in the level's own light";
			case ViewToggle::BackFaces:
				if (!can.backFaces)
				{
					return "Back faces can't be shown yet: this editor's copy of the game doesn't have them";
				}

				return after.backFaces ? "Back faces: shown" : "Back faces: hidden";
			case ViewToggle::Headlight:
				if (!after.headlight)
				{
					return "Headlight: off";
				}

				return after.gouraud == Gouraud::Off ? "Headlight: on, and it shows once Gouraud is on" : "Headlight: on";
			case ViewToggle::Brighter:
				return ViewToggles_HeldBrightness(after.brightness) >= kBrightnessMost ? "Brightness: as bright as it goes" : "Brightness: brighter";
			case ViewToggle::Darker:
				return ViewToggles_HeldBrightness(after.brightness) <= kBrightnessLeast ? "Brightness: as dark as it goes" : "Brightness: darker";
			case ViewToggle::Textures:
				return after.textured ? "Textures: on" : "Textures: off, the polygons plain";
		}

		return "";
	}
}

namespace
{
	host::ViewToggles theToggles = host::ViewToggles_AsBuilt();
}

host::GameCanShow ViewToggles_GameCanShow()
{
	// R-DRAWMODES-1 is in the editor's avpx (the integration's own pin; DrawFilledOutlines, DrawFilledOutlinesColour and
	// DrawFilledOutlinesLit, d3d_render.h), so its outlines can be shown and ViewToggles_Apply sets them. R-BACKFACES-1 isn't yet: when it
	// is, its answer here says true too.
	host::GameCanShow const can = { false, true };
	return can;
}

bool ViewToggles_BackFacesCanBeShown()
{
	return ViewToggles_GameCanShow().backFaces;
}

bool ViewToggles_OutlinesCanBeShown()
{
	return ViewToggles_GameCanShow().outlines;
}

host::ViewToggles ViewToggles_Now()
{
	return theToggles;
}

void ViewToggles_SetTextured(bool textured)
{
	theToggles.textured = textured;
}

const char *ViewToggles_UseNow(host::ViewToggle which)
{
	host::GameCanShow const can = ViewToggles_GameCanShow();
	theToggles = host::ViewToggles_Use(theToggles, which, can);
	return host::ViewToggles_Words(theToggles, which, can);
}

bool ViewToggles_FromCommandWord(const wchar_t *word, host::ViewToggle *toggles, size_t room, size_t *count)
{
	struct Choice { const wchar_t *word; size_t count; host::ViewToggle which[kCommandWordMostToggles]; };
	static Choice const choices[] = {
		{ L"normal", 0, { host::ViewToggle::Polygon, host::ViewToggle::Polygon } },
		{ L"polygon", 1, { host::ViewToggle::Polygon, host::ViewToggle::Polygon } },
		{ L"polyline", 1, { host::ViewToggle::Polyline, host::ViewToggle::Polygon } },
		{ L"filledpolyline", 1, { host::ViewToggle::FilledPolyline, host::ViewToggle::Polygon } },
		{ L"wire-textured", 1, { host::ViewToggle::WireTextured, host::ViewToggle::Polygon } },
		{ L"unlit", 1, { host::ViewToggle::GouraudSwitch, host::ViewToggle::Polygon } },
		{ L"headlight", 1, { host::ViewToggle::Headlight, host::ViewToggle::Polygon } },
		{ L"polyline+unlit", 2, { host::ViewToggle::Polyline, host::ViewToggle::GouraudSwitch } },
		{ L"wire-textured+unlit", 2, { host::ViewToggle::WireTextured, host::ViewToggle::GouraudSwitch } },
		{ L"normal+headlight", 1, { host::ViewToggle::Headlight, host::ViewToggle::Polygon } },
	};

	if (!word || !toggles || !count || room < kCommandWordMostToggles)
	{
		return false;
	}

	for (const Choice &choice : choices)
	{
		if (wcscmp(word, choice.word) == 0)
		{
			for (size_t t = 0; t < choice.count; t++)
			{
				toggles[t] = choice.which[t];
			}

			*count = choice.count;
			return true;
		}
	}

	return false;
}
