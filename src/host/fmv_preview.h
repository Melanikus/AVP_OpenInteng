// fmv_preview.h: the FMV PREVIEW (the owner's addition 11-18, the video picker of alpha.2): an FMV of the data folder played in the editor, over the window that asked for it, through
// Barry's decoder (smacker_dll.h: the pinned libsmackerdec.dll). Picture only (no sound yet). Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code:
// see License.txt, "AVP_OpenInteng's own code".
//
// HOSTILE INPUT: the file's header passes our own checks first (fmv_files.h, model::SmkHeader_Read), and the decoder must then agree on its size and frames; a picture wider or taller
// than kFmvPreviewMostSide is not previewed. Frames are shown on the clock (frame k at k times the header's milliseconds a frame), at most kFmvPreviewMostDecodesAFrame decoded in one
// editor frame (a slow editor shows fewer, never stalls); the last frame stays until the preview stops. Each frame's 256 colours and pixels go into one texture of the video's size.
#pragma once

#include <stdint.h>

#include <functional>
#include <string>

namespace host
{
	int32_t const kFmvPreviewMostSide = 1024;
	int32_t const kFmvPreviewMostDecodesAFrame = 4;

	// The frame that is due after `elapsedMs` of a video of `frames` frames at `msPerFrame`: 0 at the start, held at the last one (frames - 1); 0 for no frames or no rate
	uint32_t FmvPreview_DueFrame(double elapsedMs, double msPerFrame, uint32_t frames);

	// The place to play in, asked each frame in the editor's own pixels (left, top, width, height); false when it is gone (the window closed), which stops the preview
	typedef std::function<bool(int32_t *x, int32_t *y, int32_t *width, int32_t *height)> FmvPreviewPlace;

	// Where in the frame the picture is drawn. OverItsWindow: over the window that plays it, under that window's open list (Strategy's Message window, an inteng dialog: the
	// film is part of the dialog). UnderThePanels: on the screen itself, under every panel, the status line, the bar and its drop-downs, the pop-up and every dialog (the Video
	// mode's preview; the owner 11-22: Save pressed during a preview opened its form under the film, which hid it and took every click)
	enum class FmvPreviewLayer
	{
		OverItsWindow,
		UnderThePanels,
	};

	// Starts folder\name (stopping one that played), its picture fitted into the place with its shape kept, drawn on `layer`. False with *words (for the status line) when it
	// can't; true with *words saying what plays ("Playing FMVs\message47.smk: SMK2 128 x 96, ... (libsmackerdec.dll, the one this editor was built with)").
	bool FmvPreview_Start(std::string const &folder, std::string const &name, char const *settingsFile, FmvPreviewPlace const &place, std::string *words,
		FmvPreviewLayer layer = FmvPreviewLayer::OverItsWindow);

	void FmvPreview_Stop();
	bool FmvPreview_On();

	// True once a preview that is on has played all of its frames for their time and its sound has ended (or it has none). Its last frame stays on until it is stopped
	bool FmvPreview_Ended();

	// The editor's frame: the due frames decoded, the picture drawn; and the harness's state field fmvpreview ("<name> frame <k> of <n>", "" when off). FmvPreview_Draw is called
	// between the overlay and the top (PanelPass_AddToFrame's underTop) and draws an OverItsWindow preview; FmvPreview_DrawUnderThePanels is called before every panel
	// (underPanels) and draws an UnderThePanels one. Each frame exactly one of them does the work and says the field
	void FmvPreview_Draw(int32_t viewWidth, int32_t viewHeight);
	void FmvPreview_DrawUnderThePanels(int32_t viewWidth, int32_t viewHeight);

	// THE VIDEO SCREENS AT LEVEL OPEN (3b, the owner's addition): the level just loaded, judged as the game's ScanImagesForFMVs will (fmv_files.h FmvFiles_JudgeScreens, over the game's
	// own picture names and sizes, the .smk looked for in the game folder): each picture meant as a screen and its verdict in the log; when one stays still, the words for the status line
	// (with `otherwise` after them when it has any), else `otherwise`. Here beside the preview because both read the game's texture manager.
	char const *FmvScreens_SayAtOpen(char const *gameFolder, char const *otherwise);
}
