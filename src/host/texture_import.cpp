// texture_import.cpp: a picture made a texture of the open level, end to end (texture_import.h says the steps and their order). Written 2026-11-23 by
// Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "texture_import.h"

#include <windows.h>

#include <new>   // std::bad_alloc: out of memory refused, never the editor's end

#include <vector>

#include "bitmap_add.h"
#include "image_import.h"
#include "level_file.h"
#include "palette_quantize.h"
#include "rim_save.h"
#include "rim_write.h"

namespace host
{
	namespace
	{
		TextureImportResult Refuse(TextureImportResult result, std::string const &words, std::string *why)
		{
			*why = words;
			return result;
		}
	}

	namespace
	{
		// The steps, as TextureImport_Make says; *saved: the RIM's path once step 7 has made it (so the guard below can take it away again)
		TextureImportResult MakeUnguarded(OpenLevelFile *level, std::string const &levelFile, std::string const &dataFolder, TextureImportChoice const &choice,
			TextureImportDone *done, std::string *why, std::string *saved);
	}

	// OUT OF MEMORY anywhere in the import (Dietrich's review of e20ccef6, MUST 1: the editor is 32-bit, 2 GB, and nothing caught it) is a refusal in words, never
	// the editor's end, as the level open's is (level_file.cpp OpenLevelFile::Open); a RIM this import had made by then is deleted again, and the level is as it
	// was (it is swapped only in the last step, by ReplaceLevel, which keeps the old level when it can't take the new)
	TextureImportResult TextureImport_Make(OpenLevelFile *level, std::string const &levelFile, std::string const &dataFolder, TextureImportChoice const &choice,
		TextureImportDone *done, std::string *why)
	{
		std::string notSaid, saved;

		why = why ? why : &notSaid;

		try
		{
			return MakeUnguarded(level, levelFile, dataFolder, choice, done, why, &saved);
		}
		catch (std::bad_alloc const &)
		{
			bool const deleted = saved.empty() || DeleteFileA(saved.c_str()) != 0;

			try
			{
				*why = std::string("Import texture: there is not the memory to make it, so nothing was imported") + (deleted ? "" : "; the file made for it couldn't be deleted");
			}
			catch (std::bad_alloc const &)
			{
				why->clear();   // (no memory even for the words: the result says it)
			}

			return TextureImportResult::BadPicture;
		}
	}

	namespace
	{
	TextureImportResult MakeUnguarded(OpenLevelFile *level, std::string const &levelFile, std::string const &dataFolder, TextureImportChoice const &choice,
		TextureImportDone *done, std::string *why, std::string *saved)
	{
		TextureImportDone result;

		why->clear();

		if (done)
		{
			*done = result;
		}

		if (!level || !level->Level())
		{
			return Refuse(TextureImportResult::NoLevel, "Import texture: no level is open", why);
		}

		// 1. THE NAME
		std::string nameWhy;

		if (!RimSave_EntryName(levelFile, choice.name, &result.entry, &nameWhy))
		{
			return Refuse(TextureImportResult::BadName, "Import texture: " + nameWhy, why);
		}

		// 2. THE PICTURE (its own words start with "Import texture: "), decoded to at most twice the texture's side squared (Dietrich's review of e20ccef6, MUST 1,
		// Bishop 2's ruling: the work bounded from the picture's own size before a pixel is decoded): a larger one is scaled down by WIC's own scaler first, so the
		// fit's planes are at most (2 x 512)^2 pixels, 4 MB of BGRA and 32 MB of doubles, whatever the picture's size
		ImagePixels pixels;

		if (choice.side == 0 || ImageImport_Read(choice.picture, &pixels, why, 4ull * choice.side * choice.side) != ImageImportResult::Done)
		{
			return TextureImportResult::BadPicture;
		}

		result.pictureWidth = pixels.fileWidth;   // the picture's own size, for the words (the fit took it scaled when it was larger than it needed)
		result.pictureHeight = pixels.fileHeight;
		result.side = choice.side;

		// 3. THE SQUARE
		std::vector<uint8_t> square;
		std::string stepWhy;

		if (!model::ImageFit_Square(pixels.bgra.data(), pixels.width, pixels.height, choice.side, choice.mode, choice.transparentBars, &square, &stepWhy))
		{
			return Refuse(TextureImportResult::BadPicture, "Import texture: " + stepWhy, why);
		}

		// 4. THE PALETTE
		model::QuantizedImage quantized;

		if (!model::PaletteQuantize(square.data(), choice.side, choice.side, &quantized, &stepWhy))
		{
			return Refuse(TextureImportResult::BadPicture, "Import texture: " + stepWhy, why);
		}

		result.coloursIn = quantized.coloursIn;
		result.coloursOut = quantized.palette.size();
		result.reduced = quantized.reduced;
		result.transparent = quantized.transparent >= 0;
		result.partialAlpha = quantized.partialAlpha;

		// 5. THE RIM
		model::RimPicture picture;
		std::vector<uint8_t> rim;

		picture.width = choice.side;
		picture.height = choice.side;
		picture.indices = quantized.indices;
		picture.transparent = quantized.transparent;

		for (model::PaletteRgb const &colour : quantized.palette)
		{
			picture.palette.push_back(model::RimRgb{ colour.r, colour.g, colour.b });
		}

		if (model::RimWrite_Stock(picture, &rim, &stepWhy) != model::RimWriteResult::Written)
		{
			return Refuse(TextureImportResult::BadPicture, "Import texture: " + stepWhy, why);
		}

		// 6. THE LEVEL WITH THE ENTRY, before anything is saved
		std::vector<char> image, withEntry;

		if (!level->BuildLevel(image))
		{
			return Refuse(TextureImportResult::ListRefused, "Import texture: the level's bytes couldn't be built to add the texture to", why);
		}

		if (model::BitmapAdd_ToImage(image, result.entry, choice.side, choice.side, &withEntry, &result.index, &stepWhy) != model::BitmapAddResult::Added)
		{
			return Refuse(TextureImportResult::ListRefused, "Import texture: " + stepWhy + "; nothing was saved", why);
		}

		// 7. THE FILE (its own words start with "Import texture: ")
		if (RimSave_New(dataFolder, result.entry, rim, &result.savedPath, why) != RimSaveResult::Saved)
		{
			return TextureImportResult::CantSave;
		}

		*saved = result.savedPath;   // (from here on, a throw takes this file away again: TextureImport_Make's guard)

		// 8. THE LEVEL SWAPPED, the game's own loader first
		if (level->ReplaceLevel(withEntry) != LevelOpenResult::Opened)
		{
			// this import's own file, made a moment before by CREATE_NEW: deleted again, so nothing is left half done
			bool const deleted = DeleteFileA(result.savedPath.c_str()) != 0;

			return Refuse(TextureImportResult::LevelRefused, "Import texture: the game's own loader wouldn't take the level with the new texture, so the level is as it was" +
				std::string(deleted ? ", and the file made for it is deleted again" : "; the file made for it couldn't be deleted: graphics\\" + result.entry), why);
		}

		if (done)
		{
			*done = result;
		}

		return TextureImportResult::Imported;
	}
	}

	std::string TextureImport_DoneWords(TextureImportDone const &done)
	{
		std::string words = "Imported " + done.entry + " as texture " + std::to_string(done.index) + ": " + std::to_string(done.pictureWidth) + " x " +
			std::to_string(done.pictureHeight) + " made " + std::to_string(done.side) + " x " + std::to_string(done.side) + ", ";

		words += done.reduced ? std::to_string(done.coloursIn) + " colours made " + std::to_string(done.coloursOut) : std::to_string(done.coloursIn) + " colours kept exactly";

		if (done.transparent)
		{
			words += ", one see-through";
		}

		if (done.partialAlpha)
		{
			words += "; " + std::to_string(done.partialAlpha) + " half see-through pixels made wholly one or the other";
		}

		return words + ". Save the level to keep it";
	}

	char const *TextureImportResultWords(TextureImportResult result)
	{
		switch (result)
		{
		case TextureImportResult::Imported:
			return "imported";
		case TextureImportResult::NoLevel:
			return "no level open";
		case TextureImportResult::BadName:
			return "the name is refused";
		case TextureImportResult::BadPicture:
			return "the picture is refused";
		case TextureImportResult::ListRefused:
			return "the level's list can't take it";
		case TextureImportResult::CantSave:
			return "the file couldn't be saved";
		case TextureImportResult::LevelRefused:
			return "the game's loader refused the level";
		}

		return "unknown";
	}
}
