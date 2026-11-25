// texture_import_screen.h: THE RIM IMPORT PAGE (the owner, 2026-11-23, in alpha.6: "like it can hold drag and drop and create safe .smk videos we want it to drag and
// drop and create .rims"; AvP-docs director/plan-rim-import.md, the UI, Video mode's twin). Written 2026-11-23 by Melanikus (Dietrich) for AVP_OpenInteng, on
// Vasquez's engine (texture_import.h). AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// A PICTURE DROPPED on the editor's window anywhere but the Video screen (where a film goes: video_mode_screen.h), or picked by Texture Map's "Import texture...",
// is checked by the same path rules as a dropped film (video_drop.h VideoDrop_CheckPath: another computer, a device, a stream, a name not on a drive of this
// computer are refused from the letters, before anything opens it), then by its ending (.png, .bmp, .jpg or .jpeg: the engine's own). Then ONE FORM in inteng's look:
// the texture's name (the engine's suggestion first), its size (128, 256 or 512 square), how the picture is made that square (Fit: the whole picture, bars round
// it; Crop: the middle that fills it; Stretch: all of it, out of shape), and whether Fit's bars are see-through. Ok runs the engine's one call
// (TextureImport_Make: the picture read as hostile input, made a square, given a palette, written as a RIM under the data folder's graphics\Custom\<level>\, and
// named in the open level's list); its words go on the status line. Cancel imports nothing.
// THEN IT IS OFFERED IN TEXTURE MAP AT ONCE (plan step 8): the new RIM is loaded into the view (Tex_CreateFromRIM) and given to it with its list index
// (level_view.h LevelView_AddImportedPicture), so Next / Prev Texture and the chooser offer it before the level is saved; a polygon given it is written with
// that index. Once the level is saved and opened again, the game's own loader loads it from the list like any of the level's pictures.
// The level isn't saved here: the editor's own Save writes it, as every other edit.
#pragma once

#include <stddef.h>
#include <stdint.h>

#include <functional>
#include <string>
#include <vector>

#include "dialog_box.h"

namespace host
{
	// The picture endings the page takes (the engine's Stage 1: image_import.h), any capitals
	bool TextureImportScreen_IsPicture(std::wstring const &path);

	// What a drop handed over (VideoDrop_FromHdrop's list, `listed` in all): checked, then the form opened for the one picture; else why not, on the status line
	void TextureImportScreen_Dropped(std::vector<std::wstring> const &paths, size_t listed);

	// The form for that picture (its whole path), over whatever screen is shown; refused in words when no level is open
	void TextureImportScreen_Open(std::wstring const &picture);

	// Texture Map's "Import texture...": Windows' own Open dialog for one .png, .bmp, .jpg or .jpeg, then the form
	void TextureImportScreen_Browse();

	// THE FORM, pure (no Windows, no editor state; texture_import_screen_test checks it): pictureWords, the picture's name as shown; name, the texture's name
	// it starts with. Its parts, in this order: the words, the name's label and edit box, the size's three boxes (128 ticked), the way's three (Fit ticked), the
	// see-through box (unticked), Ok, Cancel. lineWidth, when given, is how wide the font in use draws one line of words (the editor's: texture_import_screen.cpp):
	// the form is then as wide as its longest line of words needs, and never narrower than inteng's 460 (Drake 2026-11-24: in inteng's own letters the
	// second line ran 32 pixels past the window, the state line's dialogpast "label 1: right 32", acceptance-texture-import and -jpeg)
	DialogFormSpec TextureImportScreen_Form(std::string const &pictureWords, std::string const &name, std::function<int32_t(std::string const &)> const &lineWidth = nullptr);

	// The form's answers read back from its parts as Dialogs_Form gives them: false when they aren't the form's (then nothing is imported)
	struct TextureImportAnswers
	{
		std::string name;
		uint32_t side = 128;
		int mode = 0;               // 0 Fit, 1 Crop, 2 Stretch
		bool transparentBars = false;
	};

	bool TextureImportScreen_Read(std::vector<DialogWidget> const &widgets, TextureImportAnswers *answers);
}
