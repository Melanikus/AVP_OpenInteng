// See shape_picker_screen.h and shape_picker.h. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#include "3dc.h"
#include "inline.h"
#include "gameplat.h"
#include "platform.h"
#include "renderer.h"
#include "d3_func.h"
#include "TextureManager.h"   // Tex_Create / Tex_Lock: a texture of the game's for each tile's drawing
#include <windows.h>

#include <stdio.h>
#include <string.h>

#include <map>
#include <new>
#include <string>
#include <vector>

#include "chunk.hpp"
#include "chunk_object.hpp"         // Shape_Chunk (as shape_chooser.cpp)
#include "chunk_shape.hpp"          // Chunk_With_Children::lookup_single_child
#include "mishchnk.hpp"             // File_Chunk::list_shapes
#include "logString.h"
#include "level_file.h"
#include "level_view.h"            // LevelView_SetPanes: the live panes drawn in the level's frame
#include "editor_shared.h"         // statusWords, Answer, shapeFolderSetting, theGameFolder, the create's hand-over
#include "level_objects.h"         // LevelObjects_Level, LevelObjects_OpenForEdit
#include "shape_chooser.h"         // the level's outside shapes, by file
#include "shape_extract.h"         // ShapeExtract_Sha256
#include "shape_import.h"          // ShapeImport_Read: one handle
#include "shape_pane_gpu.h"        // the live panes' GPU side (the interface agreed with Frost)
#include "shape_picker.h"
#include "shape_picker_screen.h"
#include "shape_search.h"
#include "shape_thumb.h"
#include "shape_thumb_render.h"
#include "state_line.h"
#include "text_scale.h"
#include "texture_window_paint.h"  // ShapePickerPaint_Draw

namespace
{
	float const kFieldOfView = 0.7f;              // about 40 degrees across
	uint32_t const kGround = 0x14181C;            // the texture chooser's ground (texture_window_paint.cpp), behind each drawing too

	struct Kept   // one drawing, by its file's SHA-256
	{
		std::vector<uint8_t> bgra;                // kShapeThumbSide x kShapeThumbSide, empty when it has no picture
		host::ShapePickerState state;
		std::string noPicture;                    // why it has no picture although its shape was read ("" when it has one)
		int32_t texture = -1;                     // the game's texture of it, made when first shown
		uint64_t used = 0;                        // the last frame it was shown (the least recent goes first)
		host::ShapePaneModel paneModel;           // what its LIVE pane is made of (shape_pane_gpu.h): put on the card for the sitting
		int pane = -1;                            // its live pane's handle, -1 while it has none (then the still picture above is shown)
		bool paneTried = false;                   // the card was asked for it this visit (the picker's close lets every pane go: asked again next visit)
		std::string paneWhy;                      // why it has no live pane
	};

	bool open = false;
	uint64_t openedGeneration = 0;                // the level's generation when the picker opened (level_objects.h): another one now, and it closes
	host::ShapePickerList list;
	std::vector<host::ShapePickerState> states;   // each tile's, as far as it went
	std::vector<std::string> shaOf;               // each tile's file's SHA-256, once read
	std::vector<std::string> noPictureOf;         // each tile's: why its drawing has no picture
	std::vector<int32_t> levelNumberOf;           // an "in this level" tile's: the level's first shape of its file, by number (drawn and placed); -1 for the others
	std::vector<size_t> levelPartsOf;             // and how many of the level's shapes are of that file
	host::ShapePickerView view;
	host::ShapePickerLayout layout;
	int32_t ownWide = 0;                          // the editor's own width at the last frame: where the scroll bar stands
	int32_t ownHigh = 0;                          // and its own height (the paint lays out as the frame did)
	int32_t shownColumns = -1, shownRows = -1;    // the grid's shape and the list's length when the selection was last brought into view
	size_t shownCount = (size_t)-1;
	bool dragging = false;                        // the scroll bar's thumb is held (pressed on it, not yet let go)
	uint64_t openedAtMs = 0;                      // THE TURNING's clock (the owner: "rotating in 3d"): the picker's opening, from which every pane turns,
	uint64_t selectedAtMs = 0;                    // and the selection's last change, from which the selected one turns
	size_t lastSelected = (size_t)-1;
	std::string paneTurns;                        // the panes drawn live the last frame, "<name>~<degrees>|...": the state line's pickerturns
	int32_t grabbedAt = 0;                        // where on the thumb it was taken, from its top
	bool selectedClicked = false;                 // the selected tile was clicked (so a second click on it places it)
	std::map<std::string, Kept> kept;
	uint64_t frameNumber = 0;
	host::ShapeThumbRenderer renderer;
	bool rendererTried = false;
	std::string rendererWhy;
	int textureCount = 0;                         // to name each game texture apart

	std::string FolderOf(host::ShapePickerTile const &tile)
	{
		return tile.path.size() > tile.fileName.size() ? tile.path.substr(0, tile.path.size() - tile.fileName.size() - 1) : std::string();
	}

	host::ShapePickerThinned thinned;   // what the opening left out and folded (ShapePicker_Thin), for the state line

	// THE OPENING'S LOOK at each tile drawn from its own file (shape_picker.h ShapePicker_Thin; the owner, 2026-11-23: "No picture means no geometry? if yes
	// don't show!", and copies that look the same folded into one tile): read with the drawing's own read (ShapeImport_Read: one handle, a plain file, at most
	// kShapeImportMostBytes; then ShapeThumb_Read, every count checked), at most kShapePickerMostLookMs in all. A tile "in this level" is drawn from the level's
	// own shape, not its file, so it isn't looked at; a file that can't be read, or no room to read it, keeps its tile and says why when it is drawn.
	std::vector<host::ShapePickerLook> LookAtTheFiles(size_t *pastTime, DWORD *tookMs)
	{
		std::vector<host::ShapePickerLook> looks(list.tiles.size());
		DWORD const start = GetTickCount();

		*pastTime = 0;
		*tookMs = 0;

		for (size_t i = 0; i < list.tiles.size(); i++)
		{
			host::ShapePickerTile const &tile = list.tiles[i];

			if (tile.inLevel || tile.levelOnly)
			{
				continue;
			}

			if (GetTickCount() - start > host::kShapePickerMostLookMs)
			{
				(*pastTime)++;
				continue;
			}

			try
			{
				std::vector<char> bytes;
				std::string fileName, why;
				host::ShapeThumbModel model;
				bool noGeometry = false;

				if (host::ShapeImport_Read(FolderOf(tile).c_str(), tile.name, &bytes, &fileName) != host::ShapeImportResult::Done)
				{
					continue;   // (said when it is drawn, by the same read)
				}

				if (!host::ShapeThumb_Read(bytes, &model, &why, &noGeometry))
				{
					looks[i].noGeometry = noGeometry;   // false for a broken or too big file: it keeps its tile, with why
					continue;
				}

				std::string const look = host::ShapeThumb_LookBytes(model);

				looks[i].fingerprint = look.empty() ? std::string() : host::ShapeExtract_Sha256(std::vector<char>(look.begin(), look.end()));
			}
			catch (std::bad_alloc const &)
			{
				looks[i] = host::ShapePickerLook();   // no room to look at it: it keeps its tile (the drawing says so in its turn)
			}
		}

		*tookMs = GetTickCount() - start;
		return looks;
	}

	// THE LEVEL'S OWN SHAPE of that number, as the bytes of a shape file (Frost's SHOULD 2 on piece 1: an "in this level" tile is placed by its number, so it is
	// DRAWN from the level's shape too, and the picture is what gets placed): a REBINFF2 holding a REBSHAPE of the shape's own header, points, polygons,
	// normals and texture coordinates, and a REBENVDT of its outside file's texture list (SHPEXTFL's BMPLSTST, BMPNAMES's very form: shape_import.h) and
	// RIFFNAME, each chunk's bytes as the editor holds them (OpenLevelFile::PayloadOf). shape_thumb reads it as any file: hostile, every count checked.
	bool LevelShapeAsFile(int32_t number, std::vector<char> *file)
	{
		using namespace host::shape_chunks;

		OpenLevelFile *const store = LevelObjects_OpenForEdit();
		File_Chunk *const level = LevelObjects_Level();

		if (!store || !level || number < 0)
		{
			return false;
		}

		List<Shape_Chunk *> shapes;

		level->list_shapes(&shapes);

		for (LIF<Shape_Chunk *> each(&shapes); !each.done(); each.next())
		{
			Shape_Chunk *const shape = each();
			Chunk *const header = shape ? shape->lookup_single_child("SHPHEAD1") : nullptr;
			std::vector<unsigned char> bytes;

			if (!header || !store->PayloadOf(header, &bytes) || bytes.size() < 24 || (int32_t)((uint32_t)bytes[20] | (uint32_t)bytes[21] << 8 | (uint32_t)bytes[22] << 16 |
				(uint32_t)bytes[23] << 24) != number)
			{
				continue;
			}

			auto const chunkOf = [store](Chunk_With_Children *holder, char const *name, Bytes *out) -> bool
			{
				Chunk *const child = holder ? holder->lookup_single_child(name) : nullptr;
				std::vector<unsigned char> payload;

				if (!child || !store->PayloadOf(child, &payload))
				{
					return false;
				}

				*out = MakeChunk(name, Bytes(payload.begin(), payload.end()));
				return true;
			};

			Bytes body, env, part;

			for (char const *name : { "SHPHEAD1", "SHPRAWVT", "SHPPOLYS", "SHPPNORM", "SHPVNORM", "SHPUVCRD" })
			{
				if (chunkOf(shape, name, &part))
				{
					body.insert(body.end(), part.begin(), part.end());
				}
			}

			// (as shape_chooser.cpp ExternalName holds it: a SHPEXTFL the library couldn't take as one is no holder of chunks)
			Chunk *const outsideChunk = shape->lookup_single_child("SHPEXTFL");
			Chunk_With_Children *const outside = outsideChunk && !outsideChunk->r_u_miscellaneous() ? dynamic_cast<Chunk_With_Children *>(outsideChunk) : nullptr;

			if (outside && chunkOf(outside, "BMPLSTST", &part))
			{
				part = MakeChunk("BMPNAMES", Bytes(part.begin() + (std::ptrdiff_t)kHeadingBytes, part.end()));   // the same form under the file's name for it
				env.insert(env.end(), part.begin(), part.end());
			}

			if (outside && chunkOf(outside, "RIFFNAME", &part))
			{
				env.insert(env.end(), part.begin(), part.end());
			}

			Bytes whole = MakeChunk("REBSHAPE", body), environment = MakeChunk("REBENVDT", env);

			whole.insert(whole.end(), environment.begin(), environment.end());
			*file = MakeChunk("REBINFF2", whole);
			return true;
		}

		return false;
	}

	// The folder's .rif names at its top, as the lookup order would look in it (shape_search.h): each name converted without a best fit, so one name can't stand
	// for another (a name that doesn't convert is left out and counted), at most kShapePickerMostFilesAFolder
	void ListFolder(host::ShapePickerFolder *f)
	{
		WIN32_FIND_DATAW found;
		std::string const pattern = f->folder.path + "\\*.rif";
		int const wide = MultiByteToWideChar(CP_ACP, 0, pattern.c_str(), -1, nullptr, 0);
		std::wstring w(wide > 0 ? (size_t)wide : 1, L'\0');

		if (wide <= 0 || !MultiByteToWideChar(CP_ACP, 0, pattern.c_str(), -1, &w[0], wide))
		{
			f->refused = "its path can't be read";
			return;
		}

		HANDLE const search = FindFirstFileExW(w.c_str(), FindExInfoBasic, &found, FindExSearchNameMatch, nullptr, 0);

		if (search == INVALID_HANDLE_VALUE)
		{
			return;   // nothing of that kind there (a missing folder was found by the lookup's own judging)
		}

		do
		{
			if (found.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY | FILE_ATTRIBUTE_REPARSE_POINT))
			{
				continue;   // a folder or a link named so: the lookup's reader would refuse it (shape_import.h NotPlain), so it isn't a tile
			}

			BOOL unsaid = FALSE;
			int const length = WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, found.cFileName, -1, nullptr, 0, nullptr, &unsaid);
			std::string name(length > 1 ? (size_t)length - 1 : 0, '\0');

			if (length > 1)
			{
				WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, found.cFileName, -1, &name[0], length, nullptr, &unsaid);
			}

			if (unsaid || name.empty())
			{
				f->more++;   // (said in the folder's note with the names past the cap)
				continue;
			}

			if (f->names.size() < host::kShapePickerMostFilesAFolder)
			{
				f->names.push_back(name);
			}
			else
			{
				f->more++;
			}
		} while (FindNextFileW(search, &found));

		FindClose(search);
	}

	// The drawing of tile i, as far as this frame allows: read on one handle, kept drawings reused by the file's SHA-256, else made ready and drawn
	void DrawTile(size_t i)
	{
		host::ShapePickerTile const &tile = list.tiles[i];
		host::ShapePickerState &state = states[i];
		std::vector<char> bytes;
		std::string fileName;

		// an "in this level" tile: the level's own shape, the one placed (by its number); any other: its file, read on one handle
		if (tile.inLevel && levelNumberOf[i] >= 0)
		{
			if (!LevelShapeAsFile(levelNumberOf[i], &bytes))
			{
				state.drawing = host::ShapePickerDrawing::Failed;
				state.why = "the level's own shape of that file can't be read";
				return;
			}
		}
		else
		{
			host::ShapeImportResult const read = host::ShapeImport_Read(FolderOf(tile).c_str(), tile.name, &bytes, &fileName);

			if (read != host::ShapeImportResult::Done)
			{
				state.drawing = host::ShapePickerDrawing::Failed;
				state.why = host::ShapeImportWords(read);
				return;
			}
		}

		std::string const sha = host::ShapeExtract_Sha256(bytes);
		auto const known = kept.find(sha);

		shaOf[i] = sha;

		if (known != kept.end())
		{
			state = known->second.state;
			noPictureOf[i] = known->second.noPicture;
			return;
		}

		Kept made;
		host::ShapeThumbModel model;
		std::string why;

		if (!host::ShapeThumb_Read(bytes, &model, &why))
		{
			made.state.drawing = host::ShapePickerDrawing::Failed;
			made.state.why = why;
		}
		else
		{
			made.state.drawing = host::ShapePickerDrawing::Drawn;
			made.state.parts = tile.inLevel ? levelPartsOf[i] : model.parts;   // an "in this level" tile's: the level's shapes of that file
			made.state.textures = model.pictures.size();

			// its pictures, read as the level's own are: one that can't be is named, and its polygons drawn shaded. A refused path (shape_thumb.h: the
			// level open's own rule for a texture's name) is never read, nor one with no path at all (Frost on ec8a4d0e: not even "graphics/" alone)
			std::vector<host::ShapeThumbTexture> textures(model.pictures.size());
			std::vector<std::pair<uint32_t, uint32_t> > sizes(model.pictures.size(), std::make_pair(0u, 0u));
			uint64_t tileBytes = 0;   // this tile's pictures together (kShapePickerMostTileBytes at most)

			for (size_t p = 0; p < model.pictures.size(); p++)
			{
				std::string pictureWhy;
				host::ShapeThumbPicture const &picture = model.pictures[p];

				if (!picture.refused.empty() || picture.path.empty())
				{
					pictureWhy = picture.refused.empty() ? "it names no file" : "its name is refused: " + picture.refused;
				}
				else if (ShapePickerScreen_ReadPicture(picture, &textures[p], &pictureWhy, &tileBytes))
				{
					sizes[p] = std::make_pair(textures[p].width, textures[p].height);
					made.state.texturesFound++;
					continue;
				}

				if (made.state.textureWhy.empty())
				{
					made.state.textureWhy = host::ShapePicker_SafeName(picture.name) + ": " + pictureWhy;
				}
			}

			if (!rendererTried)
			{
				rendererTried = true;

				if (!renderer.Open(&rendererWhy))
				{
					LogString("Shape picker: " + rendererWhy);
				}
			}

			float eye[3], at[3];

			host::ShapeThumb_Camera(model, kFieldOfView, eye, at);

			// THE LIVE PANE's model (shape_pane_gpu.h): put on the card by UploadPane, drawn turning each frame; the still picture below is shown till then
			made.paneModel.batches = host::ShapeThumb_Triangles(model, sizes);
			made.paneModel.pictures = model.pictures;

			for (int a = 0; a < 3; a++)
			{
				made.paneModel.eye[a] = eye[a];
				made.paneModel.at[a] = at[a];
			}

			std::string drawWhy = rendererWhy;
			bool tryLater = false;

			if (!renderer.IsOpen() || !renderer.Draw(host::ShapeThumb_Triangles(model, sizes), textures, eye, at, kFieldOfView, kGround, &made.bgra, &drawWhy, &tryLater))
			{
				made.bgra.clear();
				made.noPicture = drawWhy;

				if (tryLater)
				{
					return;   // the card is busy (Draw says so in its result): this tile stays waiting, and is tried again in a later frame
				}
			}
		}

		made.used = frameNumber;
		state = made.state;
		noPictureOf[i] = made.noPicture;
		kept[sha] = made;

		// the least recently shown drawings go first, past the most kept (never one shown this frame)
		while (kept.size() > kShapePickerMostKept)
		{
			auto oldest = kept.end();

			for (auto it = kept.begin(); it != kept.end(); ++it)
			{
				oldest = it->second.used < frameNumber && (oldest == kept.end() || it->second.used < oldest->second.used) ? it : oldest;
			}

			if (oldest == kept.end())
			{
				break;
			}

			if (oldest->second.texture >= 0)
			{
				Tex_Release((texID_t)oldest->second.texture);
			}

			// its live pane let go on the card too (shape_pane_gpu.h ShapePaneGpu_Forget), so drawings let go and uploaded again never pile up on it
			if (oldest->second.pane >= 0)
			{
				host::ShapePaneGpu_Forget(oldest->second.pane);
			}

			kept.erase(oldest);
		}
	}

	// The game's texture of a kept drawing, made the first time it is shown
	int32_t TextureOf(Kept *k)
	{
		if (k->texture >= 0 || k->bgra.size() != (size_t)host::kShapeThumbSide * host::kShapeThumbSide * 4)
		{
			return k->texture;
		}

		texID_t const made = Tex_Create("editor_picker_" + std::to_string((long long)++textureCount), (uint32_t)host::kShapeThumbSide, (uint32_t)host::kShapeThumbSide, 32,
			TextureUsage_Normal);
		uint8_t *data = nullptr;
		uint32_t pitch = 0;

		if (made == MISSING_TEXTURE)
		{
			return -1;
		}

		if (!Tex_Lock(made, &data, &pitch) || !data || pitch < (uint32_t)host::kShapeThumbSide * 4)
		{
			Tex_Release(made);
			return -1;
		}

		for (int32_t y = 0; y < host::kShapeThumbSide; y++)
		{
			memcpy(data + (size_t)y * pitch, k->bgra.data() + (size_t)y * host::kShapeThumbSide * 4, (size_t)host::kShapeThumbSide * 4);
		}

		Tex_Unlock(made);
		k->texture = (int32_t)made;
		return k->texture;
	}

	// A drawing's LIVE PANE asked of the card, once a visit (shape_pane_gpu.h): its handle, or the card's words why not (its still picture stays shown)
	void UploadPane(Kept *k)
	{
		if (k->paneTried || k->paneModel.batches.empty())
		{
			return;
		}

		k->paneTried = true;
		k->paneWhy.clear();
		k->pane = host::ShapePaneGpu_Upload(k->paneModel, &k->paneWhy);
	}

	// THE LIVE PANES, called by the level's frame between the level and the panels (level_view.h LevelView_SetPanes): each tile on the screen whose drawing is
	// on the card, drawn turning in its box (the selected one from when it was selected, the others from the picker's opening: once round every 8 s,
	// shape_picker.h ShapePicker_TurnAt), then the frame's queue drawn (ShapePaneGpu_FrameEnd). What was drawn, and at what turn, goes on the state line
	void DrawPanes(void *)
	{
		paneTurns.clear();

		if (!open)
		{
			return;
		}

		uint64_t const now = GetTickCount64();
		size_t const shown = (size_t)layout.columns * (size_t)layout.rows;

		for (size_t i = view.first; i < list.tiles.size() && i < view.first + shown; i++)
		{
			int32_t box[4], window[4];
			auto const k = shaOf[i].empty() ? kept.end() : kept.find(shaOf[i]);

			if (k == kept.end() || k->second.pane < 0 || !host::ShapePicker_TileBox(layout, view.first, i, box))
			{
				continue;
			}

			float const turn = host::ShapePicker_TurnAt(now - (i == view.selected ? selectedAtMs : openedAtMs));

			host::TextScale_BoxToWindow(box[0], box[1], box[2], box[3], &window[0], &window[1], &window[2], &window[3]);   // the window's pixels, as pickerboxes
			host::ShapePaneGpu_Draw(k->second.pane, window, turn);
			paneTurns += (paneTurns.empty() ? "" : "|") + host::ShapePicker_SafeName(list.tiles[i].name) + "~" + std::to_string(host::ShapePicker_TurnDegrees(turn));
		}

		host::ShapePaneGpu_FrameEnd();
	}

	// The selected tile placed the way Quick-create places a shape: the level's own shape of that file by its NUMBER, else the file brought in; one that can't be,
	// said in words and the picker kept open
	void PlaceSelected()
	{
		if (list.tiles.empty() || view.selected >= list.tiles.size())
		{
			return;
		}

		host::ShapePickerTile const tile = list.tiles[view.selected];
		std::string const whyNot = host::ShapePicker_WhyNotPlaced(tile, states[view.selected]);

		if (states[view.selected].drawing == host::ShapePickerDrawing::Waiting && !tile.inLevel)
		{
			Answer(tile.name + ": wait for its picture first (the file is read as it is drawn)");
			return;
		}

		if (!whyNot.empty())
		{
			Answer(tile.name + " can't be placed: " + whyNot);
			return;
		}

		// the very shape the tile was drawn from: the level's first of that file, by its number (inteng's own match for a plain name: shape_chooser.h), or -1:
		// the file brought in
		int32_t const number = tile.inLevel ? levelNumberOf[view.selected] : -1;

		ShapePickerScreen_Close(false);
		ShapeChooserCreateFromPicker(host::ShapePicker_NameBase(tile), number);   // the name is asked, then the object made (construction_modes_screen.cpp); a part's base is its object
		                                                                            // name ("Game" of "Game@offbits"), so no '@' goes into an object's name
	}
}

bool ShapePickerScreen_IsOpen()
{
	return open;
}

// THE LEVEL CHANGED while the picker is open (level_objects.h LevelObjects_Generation: opened again, or another one): its list and its "in this level" tiles'
// shape numbers are the old level's, and the drawing and the placing look a number up in the level open now, so a tile could draw and place another shape
// than the one it names (Frost's lead, Bishop's order, 2026-11-23). It closes, saying so; opened again, it lists the level open now. CloseTheLevel closes it
// before a level goes (editor_main.cpp); this catches any other way a level changes. True when it closed.
static bool ClosedForALevelChange()
{
	if (!open || LevelObjects_Generation() == openedGeneration)
	{
		return false;
	}

	ShapePickerScreen_Close(false);
	Answer("Create: the level changed, so the placement picker closed (open it again for the shapes of the level open now)");
	return true;
}

bool ShapePickerScreen_Open()
{
	OpenLevelFile *const levelFile = LevelObjects_OpenForEdit();

	if (!levelFile || !LevelObjects_Level())
	{
		Answer("Create: no level is held to work on");
		return false;
	}

	// the folders as the lookup order judges them, each as it is looked in
	std::vector<host::ShapePickerFolder> folders;

	for (host::ShapeSearchFolder const &one : host::ShapeSearch_Order(shapeFolderSetting, theGameFolder))
	{
		host::ShapePickerFolder f;
		bool missing = false;

		f.folder = one;
		f.refused = host::ShapeSearch_FolderWhyNot(one, &missing);
		f.missing = missing;

		if (!f.missing && f.refused.empty())
		{
			ListFolder(&f);
		}

		folders.push_back(f);
	}

	std::vector<host::ShapeChoice> shapes;
	std::vector<std::string> levelFiles;

	host::ShapeChooser_List(LevelObjects_Level(), levelFile, &shapes);

	for (host::ShapeChoice const &one : shapes)
	{
		levelFiles.push_back(one.name);
	}

	list = host::ShapePicker_List(folders, levelFiles);

	// NO GEOMETRY, NO TILE; COPIES FOLDED (the owner 11-23, through Bishop 2): each folder tile's file looked at, then the empty ones left out and the lookalikes
	// folded, before the level's own shapes are added (those are drawn from the level, by number)
	size_t pastTime = 0;
	DWORD lookMs = 0;
	std::vector<host::ShapePickerLook> const looks = LookAtTheFiles(&pastTime, &lookMs);

	host::ShapePicker_Thin(&list, looks, pastTime, &thinned);

	// A SLOW LOOK SAID (Dietrich's NIT on 7ad5b295): the look runs before the window can draw again, so a word during it would never be seen; when it took a
	// second or more (a stranger's large folder; the owner's 352 files take about 70 ms), the status line says afterwards why the window waited
	std::string const slowWords = lookMs >= 1000 ? " (looking at the files took " + std::to_string((unsigned long)(lookMs / 1000)) + "." +
		std::to_string((unsigned long)((lookMs % 1000) / 100)) + " s: each one is read once, to leave out the empty ones and fold the copies)" : std::string();

	if (!slowWords.empty())
	{
		list.notes.push_back("looking at the files took " + std::to_string((unsigned long)lookMs) + " ms as the picker opened");
	}

	// THE LEVEL'S OWN SHAPES as tiles too (shape_picker.h ShapePicker_AddLevelShapes; Bishop 2's decision on the owner's "items from shape_rifs + level"): a
	// shape the level alone holds, and each part of a fragment file, drawn and placed by number
	std::vector<host::ShapePickerLevelShape> levelShapes;

	for (host::ShapeChoice const &one : shapes)
	{
		host::ShapePickerLevelShape s;

		s.file = one.name;
		s.gameName = one.gameName;
		s.number = one.number;
		levelShapes.push_back(s);
	}

	host::ShapePicker_AddLevelShapes(&list, levelShapes);
	states.assign(list.tiles.size(), host::ShapePickerState());
	shaOf.assign(list.tiles.size(), std::string());
	noPictureOf.assign(list.tiles.size(), std::string());
	levelNumberOf.assign(list.tiles.size(), -1);
	levelPartsOf.assign(list.tiles.size(), 0);

	// each "in this level" tile's shape: the level's FIRST of that file (inteng's own match for a plain name, shape_chooser.h), drawn and placed; and how many
	// of the level's shapes are of it (a fragment file's parts: "N parts: the level's first is placed")
	for (size_t i = 0; i < list.tiles.size(); i++)
	{
		if (list.tiles[i].levelOnly)
		{
			levelNumberOf[i] = list.tiles[i].levelNumber;   // the level's own shape: that one, by its number (one part of a fragment file is one shape)
			levelPartsOf[i] = 1;
			continue;
		}

		for (host::ShapeChoice const &one : shapes)
		{
			if (list.tiles[i].inLevel && _stricmp(one.name.c_str(), list.tiles[i].name.c_str()) == 0)
			{
				levelNumberOf[i] = levelNumberOf[i] < 0 ? one.number : levelNumberOf[i];
				levelPartsOf[i]++;
			}
		}
	}
	view = host::ShapePickerView();
	selectedClicked = false;
	dragging = false;
	shownColumns = shownRows = -1;   // (the first frame brings the selection into view)
	openedAtMs = selectedAtMs = GetTickCount64();   // the turning starts now
	lastSelected = view.selected;
	paneTurns.clear();
	open = true;
	openedGeneration = LevelObjects_Generation();   // the level its list was built from
	ShapePickerPictures_Begin();   // the visit's one allowance for its pictures (shape_picker_pictures.cpp)
	LevelView_SetPanes(DrawPanes, nullptr);   // the live panes drawn in the level's frame while it is open

	for (std::string const &note : list.notes)
	{
		LogString("Shape picker: " + note);
	}

	Answer(list.tiles.empty() ? "Create: no shape file in " + host::ShapeSearch_Labels(host::ShapeSearch_Order(shapeFolderSetting, theGameFolder)) + " (Escape leaves)" :
		"Create: " + std::to_string((unsigned long long)list.tiles.size()) + " shapes from " + host::ShapeSearch_Labels(host::ShapeSearch_Order(shapeFolderSetting, theGameFolder)) +
		": a click selects one, a second click (or Return) places it, Escape leaves" + slowWords);
	return true;
}

void ShapePickerScreen_Close(bool sayNothingChosen)
{
	if (open && sayNothingChosen)
	{
		Answer("Create: no shape was chosen");
	}

	ShapePickerPictures_End();   // the open level's own count put back (RimLoaderToolAllowance)

	// a drawing that lacked a picture (the visit's allowance used up, a file not there) isn't kept for the next visit: that one tries again with its own allowance
	for (auto it = kept.begin(); it != kept.end();)
	{
		bool const lacking = it->second.state.texturesFound < it->second.state.textures;

		if (lacking && it->second.texture >= 0)
		{
			Tex_Release((texID_t)it->second.texture);
		}

		it = lacking ? kept.erase(it) : std::next(it);
	}

	// the live panes let go (as agreed with Frost: the card's drawings are the visit's); each kept drawing asks again next visit
	LevelView_SetPanes(nullptr, nullptr);
	host::ShapePaneGpu_ForgetAll();

	for (auto &k : kept)
	{
		k.second.pane = -1;
		k.second.paneTried = false;
	}

	paneTurns.clear();
	open = false;
	dragging = false;
	list = host::ShapePickerList();
	states.clear();
	shaOf.clear();
	noPictureOf.clear();
	levelNumberOf.clear();
	levelPartsOf.clear();
}

void ShapePickerScreen_Frame(int32_t ownWidth, int32_t ownHeight)
{
	if (ClosedForALevelChange() || !open)
	{
		return;
	}

	frameNumber++;
	ownWide = ownWidth;
	layout = host::ShapePicker_Layout(ownWidth, ownHeight);

	// the selection brought into view ONLY when the grid's shape or the list changed (ShapePicker_Show's own rule): done every frame, it undid the wheel's and the
	// scroll bar's moves, which leave the selection where it is (found 2026-11-23 building the bar: the rows went straight back to the selection)
	if (layout.columns != shownColumns || layout.rows != shownRows || list.tiles.size() != shownCount)
	{
		view = host::ShapePicker_Show(view, layout, list.tiles.size());
		shownColumns = layout.columns;
		shownRows = layout.rows;
		shownCount = list.tiles.size();
	}

	// a newly selected tile turns from its start (the selected one first: Bishop 2's order)
	if (view.selected != lastSelected)
	{
		lastSelected = view.selected;
		selectedAtMs = GetTickCount64();
	}

	// a few of the tiles on the screen drawn, the selected first, within the frame's budget
	size_t const shown = (size_t)layout.columns * (size_t)layout.rows;
	DWORD const start = GetTickCount();
	int drawn = 0;

	// THE TILES ON THE SCREEN, before anything is drawn (Frost, 2026-11-23: the owner's "poluted with a bunch of "no picture" entries"):
	// - each one's kept drawing is marked shown THIS frame. DrawTile lets go only drawings not shown this frame, and the paint marks them only after the
	//   frame's drawing, so with more tiles on the screen than the picker keeps (kShapePickerMostKept; a 3840 x 2160 screen holds 540) a visible drawing would
	//   be let go, drawn again, and another let go, every frame (Vasquez's review of 578f59d9). Marked here, a visible one is never let go: the kept ones grow
	//   past the most only by what the screen holds.
	// - one whose drawing was let go while it was off the screen goes back to waiting, so it is drawn again (its file read again) instead of showing "no
	//   picture" for the rest of the sitting. A tile whose file couldn't be read has no SHA-256, so it stays as it failed and isn't tried every frame.
	for (size_t i = view.first; i < list.tiles.size() && i < view.first + shown; i++)
	{
		auto const k = shaOf[i].empty() ? kept.end() : kept.find(shaOf[i]);

		if (k != kept.end())
		{
			k->second.used = frameNumber;
		}
		else if (!shaOf[i].empty() && states[i].drawing != host::ShapePickerDrawing::Waiting)
		{
			states[i] = host::ShapePickerState();
			noPictureOf[i].clear();
		}
	}

	for (int pass = 0; pass < 2 && drawn < kShapePickerDrawsAFrame; pass++)
	{
		for (size_t i = view.first; i < list.tiles.size() && i < view.first + shown && drawn < kShapePickerDrawsAFrame; i++)
		{
			if ((pass == 0) != (i == view.selected) || states[i].drawing != host::ShapePickerDrawing::Waiting || GetTickCount() - start > kShapePickerFrameMs)
			{
				continue;
			}

			// OUT OF MEMORY in one tile's drawing (a stranger's shape can name many large pictures; the editor is 32-bit) fails THAT TILE, in words, never the
			// editor, as the level's open does (level_file.cpp)
			try
			{
				DrawTile(i);
			}
			catch (std::bad_alloc const &)
			{
				states[i] = host::ShapePickerState();
				states[i].drawing = host::ShapePickerDrawing::Failed;
				states[i].why = "not enough memory to draw it";
			}

			drawn++;
		}
	}

	// the drawings on the screen put on the card as live panes, a few a frame (the selected first), as the drawings themselves are paced
	int uploads = 0;

	for (int pass = 0; pass < 2 && uploads < kShapePickerDrawsAFrame; pass++)
	{
		for (size_t i = view.first; i < list.tiles.size() && i < view.first + shown && uploads < kShapePickerDrawsAFrame; i++)
		{
			auto const k = (pass == 0) != (i == view.selected) || shaOf[i].empty() ? kept.end() : kept.find(shaOf[i]);

			if (k != kept.end() && !k->second.paneTried && !k->second.paneModel.batches.empty())
			{
				UploadPane(&k->second);
				uploads++;
			}
		}
	}

	ownHigh = ownHeight;   // (painted by ShapePickerScreen_Paint, in the panels' pass)
}

void ShapePickerScreen_Paint(int32_t, int32_t)
{
	if (ClosedForALevelChange() || !open)
	{
		return;
	}

	// THE LIVE PANES first, so the names, the frame and the bar go over them: the picture DrawPanes made of them this frame (in the level's frame, into the GPU
	// side's own texture) added to the panels here, over the object card, under the bar (Frost and Vasquez, 2026-11-23: shape_pane_gpu.h ShapePaneGpu_Composite)
	host::ShapePaneGpu_Composite();

	// the frame's own size as ShapePickerScreen_Frame laid the grid out this frame (so the paint and the layout always agree)
	int32_t const ownWidth = ownWide, ownHeight = ownHigh;
	size_t const shown = (size_t)layout.columns * (size_t)layout.rows;
	std::vector<ShapePickerPaintTile> tiles;

	for (size_t i = view.first; i < list.tiles.size() && i < view.first + shown; i++)
	{
		int32_t box[4];

		if (!host::ShapePicker_TileBox(layout, view.first, i, box))
		{
			continue;
		}

		ShapePickerPaintTile t;
		auto const k = shaOf[i].empty() ? kept.end() : kept.find(shaOf[i]);

		t.texture = -1;
		t.live = k != kept.end() && k->second.pane >= 0;   // drawn LIVE in the level's frame (DrawPanes): no still picture over it

		if (k != kept.end())
		{
			k->second.used = frameNumber;
			t.texture = t.live ? -1 : TextureOf(&k->second);
		}

		t.x = box[0];
		t.y = box[1];
		t.side = box[2];
		t.nameHeight = host::kShapePickerNameHeight;
		t.selected = i == view.selected;
		t.name = host::ShapePicker_SafeName(list.tiles[i].name);   // drawn in inteng's font: the safe rule's letters only
		t.words = states[i].drawing == host::ShapePickerDrawing::Waiting ? "drawing..." : states[i].drawing == host::ShapePickerDrawing::Failed ? "can't be drawn" :
			"no picture";
		tiles.push_back(t);
	}

	std::vector<std::string> lines;

	if (!list.tiles.empty() && view.selected < list.tiles.size())
	{
		lines.push_back(host::ShapePicker_TileWords(list.tiles[view.selected], states[view.selected]));
	}

	lines.push_back(list.tiles.empty() ? "No shape file in the shape folders. Escape leaves." : "A click selects a shape, a second click (or Return) places it. Arrows, Page Up / Down move. Escape leaves.");

	for (std::string const &note : list.notes)
	{
		lines.push_back(note);
	}

	ShapePickerPaint_Draw(ownWidth, ownHeight, tiles, host::ShapePicker_Bar(layout, view, list.tiles.size(), ownWidth), lines, 16, ownHeight - host::kShapePickerBottomRoom + 4);
}

bool ShapePickerScreen_Key(WPARAM key)
{
	if (ClosedForALevelChange() || !open)   // (closed for a level change: the key is the editor's)
	{
		return false;
	}

	size_t const before = view.selected;

	switch (key)
	{
	case VK_ESCAPE: ShapePickerScreen_Close(true); return true;
	case VK_RETURN: PlaceSelected(); return true;
	case VK_LEFT: view = host::ShapePicker_Key(view, layout, list.tiles.size(), host::ShapePickerKey::Left); break;
	case VK_RIGHT: view = host::ShapePicker_Key(view, layout, list.tiles.size(), host::ShapePickerKey::Right); break;
	case VK_UP: view = host::ShapePicker_Key(view, layout, list.tiles.size(), host::ShapePickerKey::Up); break;
	case VK_DOWN: view = host::ShapePicker_Key(view, layout, list.tiles.size(), host::ShapePickerKey::Down); break;
	case VK_PRIOR: view = host::ShapePicker_Key(view, layout, list.tiles.size(), host::ShapePickerKey::PageUp); break;
	case VK_NEXT: view = host::ShapePicker_Key(view, layout, list.tiles.size(), host::ShapePickerKey::PageDown); break;
	case VK_HOME: view = host::ShapePicker_Key(view, layout, list.tiles.size(), host::ShapePickerKey::Home); break;
	case VK_END: view = host::ShapePicker_Key(view, layout, list.tiles.size(), host::ShapePickerKey::End); break;
	default: break;
	}

	selectedClicked = selectedClicked && view.selected == before;
	return true;   // every key is the picker's while it is open
}

void ShapePickerScreen_Click(int32_t x, int32_t y)
{
	if (ClosedForALevelChange() || !open)   // (closed for a level change: nothing is placed)
	{
		return;
	}

	// a thumb still held is let go by any press (Frost's review of b4521891, SHOULD 1: let go outside the window, no WM_LBUTTONUP reached us, and the next press's
	// moves scrolled the rows)
	dragging = false;

	// THE SCROLL BAR first (the owner: "a dragable scroll box on the top right"): an arrow a row, the track a page, the thumb taken to be dragged
	host::ShapePickerBar const bar = host::ShapePicker_Bar(layout, view, list.tiles.size(), ownWide);
	host::ShapePickerBarPart const part = host::ShapePicker_BarPartAt(bar, x, y);

	if (part == host::ShapePickerBarPart::Thumb)
	{
		dragging = true;
		grabbedAt = y - bar.thumb[1];
		return;
	}

	if (part != host::ShapePickerBarPart::None)
	{
		view = host::ShapePicker_BarPress(view, layout, list.tiles.size(), part);
		return;
	}

	int const tile = host::ShapePicker_TileAt(layout, view.first, list.tiles.size(), x, y);

	if (tile < 0)
	{
		return;
	}

	if ((size_t)tile == view.selected && selectedClicked)
	{
		PlaceSelected();
		return;
	}

	view.selected = (size_t)tile;
	selectedClicked = true;
}

bool ShapePickerScreen_Drag(int32_t y)
{
	if (ClosedForALevelChange() || !open || !dragging)
	{
		return false;
	}

	host::ShapePickerBar const bar = host::ShapePicker_Bar(layout, view, list.tiles.size(), ownWide);

	view = host::ShapePicker_ThumbTo(view, layout, list.tiles.size(), bar, y - grabbedAt);
	return true;
}

bool ShapePickerScreen_Release()
{
	bool const was = open && dragging;

	dragging = false;
	return was;
}

void ShapePickerScreen_Wheel(int delta)
{
	if (!ClosedForALevelChange() && open && delta != 0)
	{
		view = host::ShapePicker_Scroll(view, layout, list.tiles.size(), delta > 0 ? -(delta / WHEEL_DELTA > 0 ? delta / WHEEL_DELTA : 1) : (-delta / WHEEL_DELTA > 0 ? -delta / WHEEL_DELTA : 1));
	}
}

void ShapePickerScreen_StateFields()
{
	if (!open)
	{
		StateLine_Add("picker", "closed");
		host::ShapePaneGpu_StateFields();   // the draw path's fields closed too: no pane drawn (Dietrich's reopen proof reads "drawn 0")
		return;
	}

	size_t drawnCount = 0, failedCount = 0;

	for (host::ShapePickerState const &s : states)
	{
		drawnCount += s.drawing == host::ShapePickerDrawing::Drawn ? 1 : 0;
		failedCount += s.drawing == host::ShapePickerDrawing::Failed ? 1 : 0;
	}

	std::string boxes, words, notes;
	size_t const shown = (size_t)layout.columns * (size_t)layout.rows;

	for (size_t i = view.first; i < list.tiles.size() && i < view.first + shown; i++)
	{
		int32_t box[4], wx = 0, wy = 0, ww = 0, wh = 0;

		if (!host::ShapePicker_TileBox(layout, view.first, i, box))
		{
			continue;
		}

		host::TextScale_BoxToWindow(box[0], box[1], box[2], box[3], &wx, &wy, &ww, &wh);   // the window's pixels: the harness clicks there
		boxes += (boxes.empty() ? "" : "|") + host::ShapePicker_SafeName(list.tiles[i].name) + "~" + std::to_string(wx) + " " + std::to_string(wy) + " " + std::to_string(ww) + " " +
			std::to_string(wh);   // (the name made safe: a folder's file name may hold anything Windows lets a name hold)
		words += (words.empty() ? "" : "|") + host::ShapePicker_TileWords(list.tiles[i], states[i]) + (noPictureOf[i].empty() ? "" : "; no picture: " + host::ShapePicker_SafeWords(noPictureOf[i]));
	}

	for (std::string const &note : list.notes)
	{
		notes += (notes.empty() ? "" : "|") + note;
	}

	StateLine_Add("picker", host::ShapePicker_OpenWords(layout, view, list.tiles, drawnCount, failedCount));   // the selected name made safe (shape_picker.h)
	StateLine_Add("pickerboxes", boxes);
	StateLine_Add("pickertiles", words);
	StateLine_Add("pickernotes", notes);

	// the scroll bar's parts, in the window's pixels as pickerboxes are (the harness presses and drags there)
	host::ShapePickerBar bar = host::ShapePicker_Bar(layout, view, list.tiles.size(), ownWide);

	for (int32_t *box : { bar.column, bar.up, bar.down, bar.thumb, bar.above, bar.below })
	{
		int32_t wx = 0, wy = 0, ww = 0, wh = 0;

		host::TextScale_BoxToWindow(box[0], box[1], box[2], box[3], &wx, &wy, &ww, &wh);
		box[0] = wx;
		box[1] = wy;
		box[2] = box[2] > 0 ? ww : 0;
		box[3] = box[3] > 0 ? wh : 0;
	}

	StateLine_Add("pickerscroll", host::ShapePicker_BarWords(bar));
	StateLine_Add("pickerdrag", dragging ? "held" : "free");

	// THE LIVE PANES (shape_pane_gpu.h): those drawn the last frame and their turn, "<name>~<degrees>|..."; the draw path's own fields (pickerpanetime,
	// pickerpanedraw: ShapePaneGpu_StateFields); and how many drawings the card holds or refused, with the first refusal's words
	size_t onCard = 0, refused = 0;
	std::string firstWhy;

	host::ShapePaneGpu_StateFields();

	for (auto const &k : kept)
	{
		onCard += k.second.pane >= 0 ? 1 : 0;
		refused += k.second.paneTried && k.second.pane < 0 ? 1 : 0;
		firstWhy = firstWhy.empty() && k.second.paneTried && k.second.pane < 0 ? k.second.paneWhy : firstWhy;
	}

	StateLine_Add("pickerturns", paneTurns);
	StateLine_Add("pickerpanes", "on card " + std::to_string((unsigned long long)onCard) + " refused " + std::to_string((unsigned long long)refused) +
		(firstWhy.empty() ? std::string() : ": " + host::ShapePicker_SafeWords(firstWhy)));

	// the sitting's kept pictures: how many were decoded whole, how many given from the kept ones, what is kept against its most, and how many were let go
	ShapePickerPictureCounts const pictures = ShapePickerPictures_Counts();
	auto const count = [](uint64_t n) { return std::to_string((unsigned long long)n); };

	StateLine_Add("pickerpictures", "decoded " + count(pictures.decoded) + " fromkept " + count(pictures.fromKept) + " kept " + count(pictures.kept) + " bytes " +
		count(pictures.keptBytes) + " most " + count(kShapePickerMostKeptPictureBytes) + " letgo " + count(pictures.letGo));

	// what the opening left out (no geometry) and folded (each copy under the tile it looks the same as, "<tile>~<copy>"), '|' between, every name made safe
	std::string leftOut, folded;

	for (std::string const &name : thinned.leftOut)
	{
		leftOut += (leftOut.empty() ? "" : "|") + host::ShapePicker_SafeName(name);
	}

	for (host::ShapePickerTile const &tile : list.tiles)
	{
		for (std::string const &copy : tile.copies)
		{
			folded += (folded.empty() ? "" : "|") + host::ShapePicker_SafeName(tile.name) + "~" + host::ShapePicker_SafeName(copy);
		}
	}

	StateLine_Add("pickerleftout", leftOut);
	StateLine_Add("pickerfolded", folded);

	// the tiles on the screen as the paint shows them, each counted once: with their drawing (the paint draws a kept one whatever the tile's state), else still
	// "drawing...", else words only (can't be drawn, no picture, or a drawing let go)
	size_t onScreen = 0, pictured = 0, waiting = 0, wordsOnly = 0;

	for (size_t i = view.first; i < list.tiles.size() && i < view.first + shown; i++)
	{
		auto const k = shaOf[i].empty() ? kept.end() : kept.find(shaOf[i]);

		onScreen++;

		if (k != kept.end() && k->second.bgra.size() == (size_t)host::kShapeThumbSide * host::kShapeThumbSide * 4)
		{
			pictured++;
		}
		else if (states[i].drawing == host::ShapePickerDrawing::Waiting)
		{
			waiting++;
		}
		else
		{
			wordsOnly++;
		}
	}

	auto const number = [](size_t n) { return std::to_string((unsigned long long)n); };

	StateLine_Add("pickershown", "shown " + number(onScreen) + " pictured " + number(pictured) + " waiting " + number(waiting) + " words " + number(wordsOnly));
}
