// texture_import_screen.cpp: THE RIM IMPORT PAGE (texture_import_screen.h says what and why). Written 2026-11-23 by Melanikus (Dietrich) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "texture_import_screen.h"

#include <windows.h>
#include <commdlg.h>

#include "TextureManager.h"   // Tex_CreateFromRIM: the new RIM loaded into the view
#include "dialogs.h"
#include "editor_shared.h"    // statusWords, editorWindow, editedLevel, openLevelName, theGameFolder
#include "level_view.h"       // LevelView_AddImportedPicture
#include "panel_pass.h"       // PanelPass_MenuWidth: a letter's width in the font in use (inteng's menu font, else the small one)
#include "rim_save.h"         // RimSave_SuggestedName
#include "texture_import.h"   // Vasquez's engine: TextureImport_Make
#include "video_drop.h"       // the path rules a dropped film is checked by

namespace
{
	std::wstring pending;   // the picture the open form is for (whole path, already checked)

	std::string LastPart(std::string const &path)
	{
		size_t const slash = path.find_last_of("\\/");

		return slash == std::string::npos ? path : path.substr(slash + 1);
	}
}

void host::TextureImportScreen_Dropped(std::vector<std::wstring> const &paths, size_t listed)
{
	std::wstring path;
	std::string why;

	if (!VideoDrop_CheckPath(paths, listed, "picture", &path, &why))
	{
		statusWords = why.c_str();
		return;
	}

	if (!TextureImportScreen_IsPicture(path))
	{
		std::string const words = "Not taken: " + LastPart(VideoDrop_Words(path)) + " is not a picture the editor takes (.png, .bmp, .jpg, .jpeg); a video goes in Video mode (Mode > Video)";

		statusWords = words.c_str();
		return;
	}

	TextureImportScreen_Open(path);
}

void host::TextureImportScreen_Open(std::wstring const &picture)
{
	if (!editedLevel || openLevelName.empty())
	{
		statusWords = "Import texture: open a level first (the texture is the open level's)";
		return;
	}

	pending = picture;

	std::string const shown = LastPart(VideoDrop_Words(picture));

	// the form as wide as its words in the font in use, as dialogs.cpp measures them (PastTheWindow: each letter's PanelPass_MenuWidth)
	auto const lineWidth = [](std::string const &line)
	{
		int32_t width = 0;

		for (char const letter : line)
		{
			width += PanelPass_MenuWidth((unsigned char)letter);
		}

		return width;
	};

	Dialogs_Form(TextureImportScreen_Form(shown, RimSave_SuggestedName(shown), lineWidth), [](int32_t answer, std::vector<DialogWidget> const &widgets)
	{
		TextureImportAnswers answers;

		if (answer != kDialogOk)
		{
			statusWords = "Import texture: nothing imported";
			return;
		}

		if (!TextureImportScreen_Read(widgets, &answers))
		{
			statusWords = "Import texture: nothing imported (the form lost its parts)";
			return;
		}

		if (!editedLevel)
		{
			statusWords = "Import texture: nothing imported: the level was closed";
			return;
		}

		TextureImportChoice choice;
		TextureImportDone done;
		std::string why;

		choice.picture = pending;
		choice.name = answers.name;
		choice.side = answers.side;
		choice.mode = answers.mode == 1 ? model::ImageFitMode::Crop : answers.mode == 2 ? model::ImageFitMode::Stretch : model::ImageFitMode::Fit;
		choice.transparentBars = answers.transparentBars;

		TextureImportResult const result = TextureImport_Make(editedLevel, openLevelName, theGameFolder, choice, &done, &why);

		if (result != TextureImportResult::Imported)
		{
			std::string const words = std::string("Not imported: ") + TextureImportResultWords(result) + (why.empty() ? std::string() : ": " + why);

			// SAID WHERE IT CAN'T BE MISSED (the owner on alpha.6, 2026-11-23: "a silent message on the bottom"): inteng's message box, its words and Ok, as well
			// as the status line. (The form is closed before this answer runs, dialogs.cpp, so the box opens in its own right)
			statusWords = words.c_str();
			Dialogs_Message(words);
			return;
		}

		// OFFERED IN TEXTURE MAP AT ONCE (plan step 8): the RIM just saved, loaded into the view by its game name ("graphics/" + the list's entry, read from
		// the data folder as every level picture is), and given to the view with its index; a picture the view can't load is offered once the level is
		// opened again (the game's loader then loads it from the list)
		std::string rim = "graphics/" + done.entry;

		for (char &c : rim)
		{
			c = c == '\\' ? '/' : c;
		}

		texID_t const texture = Tex_CreateFromRIM(rim);
		bool const offered = texture != MISSING_TEXTURE && LevelView_AddImportedPicture((int32_t)texture, done.index);
		std::string const words = TextureImport_DoneWords(done) + (offered ? "; in Texture Map now (Next / Prev Texture, or the chooser)" :
			"; Texture Map offers it once the level is saved and opened again");

		statusWords = words.c_str();
	});
}

void host::TextureImportScreen_Browse()
{
	wchar_t picked[32768] = {};
	OPENFILENAMEW open = {};
	wchar_t const filter[] = L"Pictures (*.png;*.bmp;*.jpg;*.jpeg)\0*.png;*.bmp;*.jpg;*.jpeg\0\0";

	open.lStructSize = sizeof(open);
	open.hwndOwner = editorWindow;
	open.lpstrFilter = filter;
	open.lpstrFile = picked;
	open.nMaxFile = (DWORD)(sizeof(picked) / sizeof(picked[0]));
	open.lpstrTitle = L"Import texture";
	// as Video mode's Load: a shortcut comes back as itself (never its target), and no network button
	open.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_DONTADDTORECENT | OFN_NONETWORKBUTTON | OFN_HIDEREADONLY | OFN_EXPLORER | OFN_NODEREFERENCELINKS;

	if (!GetOpenFileNameW(&open))
	{
		statusWords = "Import texture: nothing picked";
		return;
	}

	// the picked file is hostile all the same (a name can be typed in that box): the drop's own check
	TextureImportScreen_Dropped({ std::wstring(picked) }, 1);
}
