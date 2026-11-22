// video_mode_screen.cpp: the owner's Video mode, its screen (video_mode_screen.h says what and why; video_mode.h is the engine it shows).
// Written 2026-11-22 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "3dc.h"
#include "module.h"         // MODULE, which d3d_render.h needs
#include "inline.h"
#include "renderer.h"
#include "renderStates.h"   // TRANSLUCENCY_OFF
#include "d3_func.h"
#include "d3d_render.h"     // DrawQuad, DrawQuadSharp
#include "TextureManager.h" // Tex_Create / Tex_Lock: the source's picture

#include <windows.h>
#include <shellapi.h>       // DragFinish

#include <string.h>

#include <string>
#include <vector>

#include "editor_shared.h"  // statusWords
#include "panel_layout.h"   // ui::Box
#include "panel_paint.h"    // PaintBox, PaintText, PanelPaint_SafeText
#include "panel_pass.h"     // PanelPass_MenuWidth, PanelPass_MenuFontHeight, kPanelPassLineHeight
#include "state_line.h"
#include "text_scale.h"     // the editor's own pixels to the window's
#include "video_drop.h"
#include "video_mode.h"
#include "video_mode_layout.h"
#include "video_mode_screen.h"

namespace
{
	// inteng's dialog look (dialogs.cpp, reassess/dialog-look-drake.md): white, black and blue only
	uint32_t const kWhite = 0xFFFFFFFFu;
	uint32_t const kBlack = 0xFF000000u;
	uint32_t const kBlue = 0xFF0000FFu;

	int32_t const kBigScale = 2;            // the big message's letters: each font pixel a 2 x 2 block (item 1's, PaintText::scale), in inteng's own letters only
	int32_t const kBigMargin = 8;           // the black round them
	int32_t const kRectangleLine = 2;       // the rectangle's white line, with a black pixel on each side of it so it shows on any picture
	size_t const kMostDropped = 16;         // the paths a real drop hands over at most (the rest are only counted, video_drop.h)
	size_t const kMostDropText = 64 * 1024; // the harness's list, at most

	bool onScreen = false;
	ui::Box viewBox = {};
	host::VideoModeLayout plan;              // this frame's places (VideoModeScreen_Panels), read by the draw and the press
	std::vector<std::string> shownControls;  // the controls shown this frame, in plan.controls' order

	// The source's picture as a texture, made again when the engine's picture changes
	texID_t picture = MISSING_TEXTURE;
	std::vector<uint8_t> pictureBytes;
	uint32_t pictureWidth = 0, pictureHeight = 0;

	int32_t FontHeight()
	{
		return PanelPass_MenuFontHeight() > 0 ? PanelPass_MenuFontHeight() : kPanelPassLineHeight;
	}

	// How wide the words are drawn, in the editor's pixels, as the panel pass will draw them (made safe first: a file's name in them is hostile)
	int32_t WidthOf(std::string const &words)
	{
		int32_t width = 0;

		for (char const letter : host::PanelPaint_SafeText(words))
		{
			width += PanelPass_MenuWidth((unsigned char)letter);
		}

		return width;
	}

	void ReleasePicture()
	{
		if (picture != MISSING_TEXTURE)
		{
			Tex_Release(picture);
			picture = MISSING_TEXTURE;
		}

		pictureBytes.clear();
		pictureWidth = pictureHeight = 0;
	}

	// The engine's picture (R G B a pixel, the top row first) into the texture, as the game's own frames go in (B, G, R, 255 a pixel: fmv_preview.cpp FillPicture).
	// True when the texture holds it; a picture whose bytes don't match its size, or bigger than the engine's own limit, is not taken
	bool HoldPicture(host::VideoModeView const &view)
	{
		uint32_t const w = view.stillWidth, h = view.stillHeight;

		if (w == 0 || h == 0 || w > (uint32_t)host::kVideoModeMostStillSide || h > (uint32_t)host::kVideoModeMostStillSide || view.still.size() != (size_t)w * h * 3)
		{
			ReleasePicture();
			return false;
		}

		if (picture != MISSING_TEXTURE && w == pictureWidth && h == pictureHeight && view.still == pictureBytes)
		{
			return true;
		}

		ReleasePicture();
		picture = Tex_Create("editor_video_still", w, h, 32, TextureUsage_Normal);

		if (picture == MISSING_TEXTURE)
		{
			return false;
		}

		uint8_t *data = nullptr;
		uint32_t pitch = 0;

		if (!Tex_Lock(picture, &data, &pitch) || !data || pitch < w * 4)
		{
			ReleasePicture();
			return false;
		}

		for (uint32_t y = 0; y < h; y++)
		{
			uint8_t *to = data + (size_t)y * pitch;
			uint8_t const *from = view.still.data() + (size_t)y * w * 3;

			for (uint32_t x = 0; x < w; x++)
			{
				*to++ = from[2];
				*to++ = from[1];
				*to++ = from[0];
				*to++ = 255;
				from += 3;
			}
		}

		Tex_Unlock(picture);
		pictureBytes = view.still;
		pictureWidth = w;
		pictureHeight = h;
		return true;
	}

	// A box of one colour, in the editor's pixels, drawn in the window's
	void SolidBox(int32_t x, int32_t y, int32_t width, int32_t height, uint32_t colour)
	{
		if (width <= 0 || height <= 0)
		{
			return;
		}

		int32_t wx = 0, wy = 0, ww = 0, wh = 0;

		host::TextScale_BoxToWindow(x, y, width, height, &wx, &wy, &ww, &wh);
		DrawQuad((uint32_t)wx, (uint32_t)wy, (uint32_t)ww, (uint32_t)wh, NO_TEXTURE, colour, TRANSLUCENCY_OFF);
	}

	// A frame `thick` pixels wide just inside the box
	void FrameBox(int32_t x, int32_t y, int32_t width, int32_t height, int32_t thick, uint32_t colour)
	{
		SolidBox(x, y, width, thick, colour);
		SolidBox(x, y + height - thick, width, thick, colour);
		SolidBox(x, y + thick, thick, height - 2 * thick, colour);
		SolidBox(x + width - thick, y + thick, thick, height - 2 * thick, colour);
	}

	bool AddBox(host::PaintBox *boxes, size_t *count, size_t most, int32_t x, int32_t y, int32_t width, int32_t height, uint32_t colour)
	{
		if (*count >= most)
		{
			return false;
		}

		boxes[(*count)++] = host::PaintBox{ x, y, width, height, colour, false };
		return true;
	}

	bool AddLine(host::PaintText *lines, size_t *count, size_t most, std::string const &words, int32_t x, int32_t y, int32_t scale)
	{
		if (*count >= most)
		{
			return false;
		}

		host::PaintText line;

		line.text = words;
		line.x = x;
		line.y = y;
		line.colour = kWhite;
		line.scale = scale;
		lines[(*count)++] = line;
		return true;
	}
}

void host::VideoModeScreen_Panels(bool on, ui::Box const &view, PaintBox *boxes, size_t *boxCount, PaintText *lines, size_t *lineCount, size_t most)
{
	onScreen = on;
	shownControls.clear();
	plan = VideoModeLayout();

	if (!on)
	{
		StateLine_Add("videowords", "");
		StateLine_Add("videocontrols", "");
		StateLine_Add("videostate", VideoMode_StateWords());
		StateLine_Add("videofit", VideoMode_View().fitWords);
		return;
	}

	VideoMode_Frame();   // the worker's progress and its end taken in, before anything is drawn from the view

	VideoModeView const &now = VideoMode_View();
	bool const busy = now.phase == VideoModePhase::Loading || now.phase == VideoModePhase::Making;
	bool const held = now.phase == VideoModePhase::Loaded || now.phase == VideoModePhase::Making;
	int32_t const fontHeight = FontHeight();

	viewBox = view;

	// The controls this phase shows: none with no video; Cancel alone while a video is read or a film made (the others wait then); every other one with a video
	for (size_t c = 0; c < kVideoModeControlCount; c++)
	{
		bool const cancel = strcmp(kVideoModeControls[c], "Cancel") == 0;

		if ((busy && cancel) || (now.phase == VideoModePhase::Loaded && !cancel))
		{
			shownControls.push_back(kVideoModeControls[c]);
		}
	}

	std::vector<int32_t> widths;

	for (std::string const &name : shownControls)
	{
		widths.push_back(WidthOf(name));
	}

	// The video's words at the top: its name and size, the fit, the cut, the volume
	std::vector<std::string> said;

	if (held)
	{
		said = { now.sourceWords, now.fitWords, now.cutWords, "Volume: " + now.volumeWords };
	}

	VideoLayoutBox const room = { view.x, view.y, view.width, view.height };

	plan = VideoModeLayout_Plan(room, widths, fontHeight, said.size(), held ? now.sourceWidth : 0, held ? now.sourceHeight : 0, now.sourceRect);

	for (size_t i = 0; i < said.size() && i < plan.lineTops.size(); i++)
	{
		AddLine(lines, lineCount, most, said[i], view.x + kVideoLayoutMargin, plan.lineTops[i], 1);
	}

	// The controls' frame (a window: black, a white frame) and its buttons (a white frame filled blue, the words white, in the middle)
	std::string controlWords;

	if (plan.frame.width > 0)
	{
		AddBox(boxes, boxCount, most, plan.frame.x, plan.frame.y, plan.frame.width, plan.frame.height, kWhite);
		AddBox(boxes, boxCount, most, plan.frame.x + 1, plan.frame.y + 1, plan.frame.width - 2, plan.frame.height - 2, kBlack);
	}

	for (size_t i = 0; i < shownControls.size() && i < plan.controls.size(); i++)
	{
		VideoLayoutBox const &button = plan.controls[i];

		if (button.width <= 0)
		{
			continue;
		}

		AddBox(boxes, boxCount, most, button.x, button.y, button.width, button.height, kWhite);
		AddBox(boxes, boxCount, most, button.x + 1, button.y + 1, button.width - 2, button.height - 2, kBlue);
		AddLine(lines, lineCount, most, shownControls[i], button.x + (button.width - widths[i]) / 2, button.y + kVideoLayoutButtonPadY, 1);
		controlWords += (controlWords.empty() ? "" : "|") + shownControls[i] + " " + std::to_string(button.x) + " " + std::to_string(button.y) + " " +
			std::to_string(button.width) + " " + std::to_string(button.height);
	}

	// With no video: the owner's words in the middle of the black, big when they fit the view's width, else inteng's own size
	std::string emptyWords;

	// Big letters only in inteng's own font: the game's small font, drawn when inteng's isn't found, has one size and takes no scale (panel_paint.h PaintText::scale),
	// so measuring it as big would put its words off the middle
	int32_t const big = PanelPass_HasMenuFont() ? kBigScale : 1;

	if (now.phase == VideoModePhase::Empty)
	{
		emptyWords = kVideoModeEmptyWords;

		int32_t const width = WidthOf(emptyWords);
		int32_t const scale = width * big + 2 * kVideoLayoutMargin <= view.width ? big : 1;

		AddLine(lines, lineCount, most, emptyWords, view.x + (view.width - width * scale) / 2, view.y + (view.height - fontHeight * scale) / 2, scale);
	}

	// While a video is read or a film made: "Loading N%" / "Making N%", big, on a black box as wide as its longest form (100%), so it stays still as the % moves
	if (busy)
	{
		std::string const doing = now.phase == VideoModePhase::Loading ? "Loading" : "Making";
		std::string const words = doing + " " + std::to_string(now.percent < 0 ? 0 : (now.percent > 100 ? 100 : now.percent)) + "%";
		int32_t const widest = WidthOf(doing + " 100%");
		int32_t const boxWidth = widest * big + 2 * kBigMargin;
		int32_t const boxHeight = fontHeight * big + 2 * kBigMargin;
		int32_t const boxX = view.x + (view.width - boxWidth) / 2;
		int32_t const boxY = view.y + (view.height - boxHeight) / 2;

		if (AddBox(boxes, boxCount, most, boxX, boxY, boxWidth, boxHeight, kBlack) &&
			AddLine(lines, lineCount, most, words, boxX + (boxWidth - WidthOf(words) * big) / 2, boxY + kBigMargin, big) && now.phase == VideoModePhase::Loading)
		{
			VideoMode_LoadingDrawn();
		}
	}

	StateLine_Add("videowords", emptyWords);
	StateLine_Add("videocontrols", controlWords);
	StateLine_Add("videostate", VideoMode_StateWords());
	StateLine_Add("videofit", now.fitWords);
}

void host::VideoModeScreen_DrawUnder()
{
	if (!onScreen || viewBox.width <= 0 || viewBox.height <= 0)
	{
		return;
	}

	CheckWireFrameMode(0);   // the view may be drawn as lines (the View menu's Wireframe): the screen never is
	SolidBox(viewBox.x, viewBox.y, viewBox.width, viewBox.height, kBlack);

	VideoModeView const &now = VideoMode_View();

	if (plan.picture.width <= 0 || !HoldPicture(now))
	{
		return;
	}

	int32_t wx = 0, wy = 0, ww = 0, wh = 0;

	TextScale_BoxToWindow(plan.picture.x, plan.picture.y, plan.picture.width, plan.picture.height, &wx, &wy, &ww, &wh);
	DrawQuadSharp((uint32_t)wx, (uint32_t)wy, (uint32_t)ww, (uint32_t)wh, picture, 0xFFFFFFFFu, TRANSLUCENCY_OFF);

	// THE RECTANGLE: a white line with black on each side, over the picture (Fit's reaches past it, over the black where its bars go)
	VideoLayoutBox const &r = plan.rectangle;

	if (r.width > 2 * (kRectangleLine + 2) && r.height > 2 * (kRectangleLine + 2))
	{
		FrameBox(r.x - 1, r.y - 1, r.width + 2, r.height + 2, kRectangleLine + 2, kBlack);
		FrameBox(r.x, r.y, r.width, r.height, kRectangleLine, kWhite);
	}
}

bool host::VideoModeScreen_Press(int32_t x, int32_t y)
{
	if (!onScreen)
	{
		return false;
	}

	int32_t const at = VideoModeLayout_At(plan.controls, x, y);

	if (at < 0 || (size_t)at >= shownControls.size())
	{
		return false;
	}

	VideoMode_Press(shownControls[(size_t)at].c_str());
	return true;
}

void host::VideoModeScreen_ScreenChanged(size_t was, size_t now)
{
	if (VideoModeScreen_Is(now) && !VideoModeScreen_Is(was))
	{
		// The preview plays IN THE RECTANGLE: the film is that part of the picture made 128 x 96 (Fit's black bars in it), so it lands exactly where the rectangle showed
		// it would; with no rectangle, over the picture. The engine asks each frame; false (off the screen, or nothing to play over) stops it
		VideoMode_SetPreviewPlace([](int32_t *x, int32_t *y, int32_t *width, int32_t *height)
		{
			VideoLayoutBox const &place = plan.rectangle.width > 0 && plan.rectangle.height > 0 ? plan.rectangle : plan.picture;

			if (!onScreen || place.width <= 0 || place.height <= 0)
			{
				return false;
			}

			*x = place.x;
			*y = place.y;
			*width = place.width;
			*height = place.height;
			return true;
		});
	}
	else if (VideoModeScreen_Is(was) && !VideoModeScreen_Is(now))
	{
		VideoMode_Leave();
		ReleasePicture();
		onScreen = false;
		plan = VideoModeLayout();
		shownControls.clear();
	}
}

void host::VideoModeScreen_DropHandle(void *drop, bool onTheScreen)
{
	if (!drop)
	{
		return;
	}

	size_t more = 0;
	std::vector<std::wstring> const paths = onTheScreen ? VideoDrop_FromHdrop((HDROP)drop, kMostDropped, &more) : std::vector<std::wstring>();

	DragFinish((HDROP)drop);

	if (!onTheScreen)
	{
		statusWords = "Drop a video in Video mode (Mode > Video)";
		return;
	}

	VideoMode_Dropped(paths, paths.size() + more);
}

void host::VideoModeScreen_DropText(char const *text, size_t bytes, bool onTheScreen)
{
	if (!onTheScreen)
	{
		statusWords = "Drop a video in Video mode (Mode > Video)";
		return;
	}

	std::vector<std::wstring> const paths = text && bytes <= kMostDropText ? VideoDrop_FromHarness(text, bytes) : std::vector<std::wstring>();

	VideoMode_Dropped(paths, paths.size());
}

void host::VideoModeScreen_Close()
{
	VideoMode_Leave();
	ReleasePicture();
	onScreen = false;
}
