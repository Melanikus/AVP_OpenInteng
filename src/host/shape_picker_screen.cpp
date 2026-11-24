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

#include <algorithm>
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
#include "shape_identity.h"        // typed placement: what a tile's shape is, by its file's SHA-256 (ShapeIdentity_Find, ShapeIdentity_PlacedAs)
#include "shape_import.h"          // ShapeImport_Read: one handle
#include "shape_pane_gpu.h"        // the live panes' GPU side (the interface agreed with Frost)
#include "shape_resized.h"         // a resized copy of a stock pickup: the same pickup (ShapeResized_Compare)
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
		host::ShapeIdentityRow const *resizedRow = nullptr;   // the stock pickup its shape is a resized copy of (FindResized), null: none
		host::ShapeResizedAnswer resized;                     // and how (its scale)
	};

	// A STOCK PICKUP on the player's machine (ReadStockPickups): a file of the lookup order's folders whose bytes ARE a typed row's (its SHA-256 the row's), so
	// what a resized copy is compared with is Steam's own shape, never a file that only has its name
	struct StockPickup
	{
		host::ShapeIdentityRow const *row = nullptr;
		host::ShapeThumbModel model;
	};

	bool open = false;
	uint64_t openedGeneration = 0;                // the level's generation when the picker opened (level_objects.h): another one now, and it closes
	host::ShapePickerList list;
	std::vector<host::ShapePickerState> states;   // each tile's, as far as it went
	std::vector<std::string> shaOf;               // each tile's file's SHA-256, once read
	std::vector<std::string> noPictureOf;         // each tile's: why its drawing has no picture
	std::vector<int32_t> levelNumberOf;           // an "in this level" tile's: the level's first shape of its file, by number (drawn and placed); -1 for the others
	std::vector<size_t> levelPartsOf;             // and how many of the level's shapes are of that file
	// TYPED PLACEMENT (Dietrich 2026-11-23; the owner: "Those must have a green border around their render pane and when moused over explain that"): each tile's
	// row in the identity table, null while unknown. Known by the SHA-256 of the very shape the tile places. A folder's file: its own bytes, read as it is
	// drawn. An "in this level" tile: the file the level's own shape of that number makes (shape_extract.h ShapeExtract_FileSums), worked out as the picker opens.
	std::vector<host::ShapeIdentityRow const *> knownOf;
	// A RESIZED COPY (Vasquez 2026-11-24, Bishop's card; the owner: "Yes, same pickup"): a tile with no row by its SHA-256 whose shape is a stock pickup's at one
	// uniform scale (shape_resized.h) takes that pickup's row in knownOf, and its answer here (same: it came that way, with its scale). Compared only with the
	// stock pickups read from the folders as the picker opened, so only after the first start's search made them (before it, nothing is typed by its size)
	std::vector<host::ShapeResizedAnswer> resizedOf;
	std::vector<StockPickup> stockPickups;
	std::string stockPickupsWords;                // how they were read as the picker opened ("5 read in 12 ms", or why not)
	std::vector<int> kindGroupOf;                 // each tile's group in the order by kind (shape_picker.h ShapePicker_KindGroup), as the picker opened
	std::string levelSumsWords;                   // how the level's own shapes were looked up as the picker opened ("<n> of <m> known in <ms> ms", or why not)
	bool pointerSeen = false;                     // the pointer has moved over the window since the picker opened (no button needed) ...
	int32_t pointerX = 0, pointerY = 0;           // ... and where it was last, in the editor's own pixels: the tile under it is found again each frame, as the rows scroll
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
	// kShapeImportMostBytes; then ShapeThumb_Read, every count checked), at most kShapePickerMostLookMs in all. A tile of the LEVEL'S OWN shape ("in this
	// level", or the level's alone) is looked at as it is drawn and placed: the level's shape of its number made a file (LevelShapeAsFile; Dietrich 2026-11-24,
	// so the level's own tiles fold too); a file that can't be read, or no room to read it, keeps its tile and says why when it is drawn.
	// THE SAME SURFACE IN ANOTHER POLYGON LIST, FOLDED (Vasquez 2026-11-24; the owner: "dedupe those", Bishop's approval): the files looked at below are grouped
	// by what one surface's files share (shape_resized.h ShapeResized_SurfaceKey); in each group the first by name (the plain one: "pulse" before "pulse_als-dm")
	// is kept, and every other one that is ITS SURFACE (ShapeResized_SameSurface, either way round: every point exactly equal, its quads two of the other's
	// triangles) is given its look, so ShapePicker_Thin folds it into that tile, named in its copies, as a lookalike is. The plain one is what is drawn and
	// placed, the file Quick-create would take. The models kept for it: at most kFoldMostCoordinates coordinates in all (past that, the rest keep their tiles)
	size_t const kFoldMostCoordinates = 4 * 1024 * 1024;

	struct SurfaceTile
	{
		size_t tile = 0;
		host::ShapeThumbModel model;
	};

	void FoldSameSurfaces(std::map<std::string, std::vector<SurfaceTile>> const &surfaces, std::vector<host::ShapePickerLook> *looks)
	{
		for (auto const &group : surfaces)
		{
			if (group.second.size() < 2)
			{
				continue;
			}

			// the first by name, as ShapePicker_Thin keeps: the list is in the order of the names in small letters as the files are looked at (the order by
			// kind comes later), so the first tile of the group
			std::vector<SurfaceTile const *> byName;

			for (SurfaceTile const &one : group.second)
			{
				byName.push_back(&one);
			}

			std::sort(byName.begin(), byName.end(), [](SurfaceTile const *a, SurfaceTile const *b) { return a->tile < b->tile; });

			// THE LEVEL'S OWN TILE KEPT (Dietrich 2026-11-24, Bishop's default): when the group holds a tile of the level's own shape, the first such is kept
			// (placed by its number, nothing brought in), as ShapePicker_Thin keeps it
			size_t keep = 0;

			for (size_t k = 0; k < byName.size(); k++)
			{
				host::ShapePickerTile const &tile = list.tiles[byName[k]->tile];

				if (tile.inLevel || tile.levelOnly)
				{
					keep = k;
					break;
				}
			}

			SurfaceTile const &kept = *byName[keep];
			std::string const &look = (*looks)[kept.tile].fingerprint;

			for (size_t k = 0; k < byName.size() && !look.empty(); k++)
			{
				if (k == keep)
				{
					continue;
				}

				SurfaceTile const &other = *byName[k];

				if (host::ShapeResized_SameSurface(other.model, kept.model, nullptr) || host::ShapeResized_SameSurface(kept.model, other.model, nullptr))
				{
					(*looks)[other.tile].fingerprint = look;   // folded into the kept tile by ShapePicker_Thin
				}
			}
		}
	}

	// levelNumbers: each tile's level shape by its number (-1: a folder's file only), as Open works them out before the look
	bool LevelShapeAsFile(int32_t number, std::vector<char> *file);

	std::vector<host::ShapePickerLook> LookAtTheFiles(std::vector<int32_t> const &levelNumbers, size_t *pastTime, DWORD *tookMs)
	{
		std::vector<host::ShapePickerLook> looks(list.tiles.size());
		DWORD const start = GetTickCount();
		std::map<std::string, std::vector<SurfaceTile>> surfaces;   // by the SHA-256 of their surface key (the fold, above)
		size_t keptCoordinates = 0;

		*pastTime = 0;
		*tookMs = 0;

		for (size_t i = 0; i < list.tiles.size(); i++)
		{
			host::ShapePickerTile const &tile = list.tiles[i];
			int32_t const number = i < levelNumbers.size() ? levelNumbers[i] : -1;
			bool const fromLevel = (tile.inLevel || tile.levelOnly) && number >= 0;

			if ((tile.inLevel || tile.levelOnly) && number < 0)
			{
				continue;   // (a level tile with no shape of its own here: drawn from its file, as before, and not looked at)
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

				if (fromLevel ? !LevelShapeAsFile(number, &bytes) : host::ShapeImport_Read(FolderOf(tile).c_str(), tile.name, &bytes, &fileName) != host::ShapeImportResult::Done)
				{
					continue;   // (said when it is drawn, by the same read)
				}

				// what the identity table knows it by: these very bytes' SHA-256 (the order by kind, ShapePickerScreen_Open; Frost 2026-11-24); a level tile is known
				// by the file its shape makes (ShapeExtract_FileSums, later), not by these bytes
				looks[i].fileSha = fromLevel ? std::string() : host::ShapeExtract_Sha256(bytes);

				if (!host::ShapeThumb_Read(bytes, &model, &why, &noGeometry))
				{
					looks[i].noGeometry = noGeometry;   // false for a broken or too big file: it keeps its tile, with why
					continue;
				}

				std::string const look = host::ShapeThumb_LookBytes(model);

				looks[i].fingerprint = look.empty() ? std::string() : host::ShapeExtract_Sha256(std::vector<char>(look.begin(), look.end()));

				// kept for the fold by its surface (above), while there is room
				std::string const surface = looks[i].fingerprint.empty() ? std::string() : host::ShapeResized_SurfaceKey(model);

				if (!surface.empty() && keptCoordinates + model.points.size() <= kFoldMostCoordinates)
				{
					keptCoordinates += model.points.size();
					surfaces[host::ShapeExtract_Sha256(std::vector<char>(surface.begin(), surface.end()))].push_back(SurfaceTile{ i, std::move(model) });
				}
			}
			catch (std::bad_alloc const &)
			{
				looks[i] = host::ShapePickerLook();   // no room to look at it: it keeps its tile (the drawing says so in its turn)
			}
		}

		try
		{
			FoldSameSurfaces(surfaces, &looks);
		}
		catch (std::bad_alloc const &)
		{
			// no room to compare them: each keeps its own tile
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

	// THE STOCK PICKUPS ON THIS MACHINE, read as the picker opens: for each row of the identity table whose kind is one placement types (shape_identity.h, the
	// closed list), the first file of that name in the lookup order's folders whose bytes ARE that row's (their SHA-256 the row's: a file with only the name is
	// passed over), read as a tile is (ShapeImport_Read: one handle, a plain file, its size capped; then ShapeThumb_Read, every count checked). At most
	// kShapePickerMostLookMs in all; out of memory, none. The table holds no shapes, only their SHA-256s, so a pickup the folders don't hold can't be compared:
	// before the first start's search makes them, no resized copy is typed (the safe side: it stays unknown, so static)
	void ReadStockPickups(std::vector<host::ShapePickerFolder> const &folders)
	{
		DWORD const start = GetTickCount();
		size_t wanted = 0, pastTime = 0;

		stockPickups.clear();

		try
		{
			for (size_t r = 0; r < host::kShapeIdentityRowCount; r++)
			{
				host::ShapeIdentityRow const &row = host::kShapeIdentityRows[r];
				host::ShapePlacedAs as;

				if (!host::ShapeIdentity_PlacedAs(row.kind, &as))
				{
					continue;   // not a pickup: a resized copy of it would be static anyway
				}

				wanted++;

				if (GetTickCount() - start > host::kShapePickerMostLookMs)
				{
					pastTime++;
					continue;
				}

				for (host::ShapePickerFolder const &f : folders)
				{
					std::string held;   // the name as the folder holds it (any capitals), "" when it holds none

					for (std::string const &name : f.names)
					{
						held = _stricmp(name.c_str(), row.fileName) == 0 ? name : held;
					}

					if (f.missing || !f.refused.empty() || held.size() <= 4)
					{
						continue;
					}

					std::vector<char> bytes;
					std::string fileName, why;
					StockPickup stock;

					if (host::ShapeImport_Read(f.folder.path.c_str(), held.substr(0, held.size() - 4), &bytes, &fileName) != host::ShapeImportResult::Done ||
						host::ShapeExtract_Sha256(bytes) != row.sha256 || !host::ShapeThumb_Read(bytes, &stock.model, &why))
					{
						continue;   // not Steam's bytes of it (or not readable): a later folder may hold them
					}

					stock.row = &row;
					stockPickups.push_back(stock);
					break;
				}
			}
		}
		catch (std::bad_alloc const &)
		{
			stockPickups.clear();
			stockPickupsWords = "not enough memory to read them";
			return;
		}

		stockPickupsWords = std::to_string((unsigned long long)stockPickups.size()) + " of " + std::to_string((unsigned long long)wanted) + " read in " +
			std::to_string((unsigned long long)(GetTickCount() - start)) + " ms" + (pastTime ? ", " + std::to_string((unsigned long long)pastTime) + " past the time" : "");
	}

	// A RESIZED COPY (shape_resized.h): tile i has no row by its SHA-256, and its shape is a stock pickup's at one uniform scale, so it takes that pickup's row
	// (typed, its green border). Only the stock pickups with the same counts of points and polygons are compared, so the work stays small. When two pickups of
	// OTHER kinds both fit, neither is taken (it stays unknown): no shape is ever given a kind it might not be
	void FindResized(size_t i, host::ShapeThumbModel const &model, Kept *made)
	{
		if (i >= knownOf.size() || i >= resizedOf.size() || knownOf[i])
		{
			return;
		}

		host::ShapeIdentityRow const *found = nullptr;
		host::ShapeResizedAnswer foundAnswer;

		for (StockPickup const &stock : stockPickups)
		{
			if (stock.model.points.size() != model.points.size() || stock.model.polygons.size() != model.polygons.size())
			{
				continue;
			}

			host::ShapeResizedAnswer const answer = host::ShapeResized_Compare(model, stock.model);

			if (!answer.same)
			{
				continue;
			}

			if (found && std::string(found->kind) != stock.row->kind)
			{
				return;   // two of other kinds: neither
			}

			if (!found)
			{
				found = stock.row;
				foundAnswer = answer;
			}
		}

		if (found)
		{
			made->resizedRow = found;
			made->resized = foundAnswer;
			knownOf[i] = found;
			resizedOf[i] = foundAnswer;
			return;
		}

		// NOT ONE, though a stock pickup has its NAME: why not, in words for the line under the grid and the state line (a mapper sees why his medpack stays
		// static; one compare, with that pickup only)
		std::string const &tileName = list.tiles[i].name;

		for (StockPickup const &stock : stockPickups)
		{
			std::string const stockName = stock.row->fileName;

			if (stockName.size() == tileName.size() + 4 && _strnicmp(stockName.c_str(), tileName.c_str(), tileName.size()) == 0)
			{
				host::ShapeResizedAnswer answer = host::ShapeResized_Compare(model, stock.model);

				answer.same = false;   // (it can't be: it was compared above)
				answer.why = "not a resized copy of " + host::ShapePicker_SafeName(stockName) + ": " + answer.why;
				made->resized = answer;
				resizedOf[i] = answer;
				return;
			}
		}
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

		// a folder's file is known by these very bytes, the ones placed (an "in this level" tile's shape was looked up as the picker opened: its bytes here are
		// the drawing's own form of the level's shape, not a file the table lists)
		if (!(tile.inLevel && levelNumberOf[i] >= 0))
		{
			knownOf[i] = host::ShapeIdentity_Find(sha);
		}

		if (known != kept.end())
		{
			state = known->second.state;
			noPictureOf[i] = known->second.noPicture;

			if (!knownOf[i] && i < resizedOf.size())   // the same bytes, so the same answer: a resized copy of that stock pickup, or why not
			{
				knownOf[i] = known->second.resizedRow;
				resizedOf[i] = known->second.resized;
			}

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
			FindResized(i, model, &made);
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

	// TYPED PLACEMENT: tile i's shape is known and its object is made as a game object (a pickup of the closed list, or furniture with its own stock placement:
	// shape_identity.h ShapeIdentity_PlacedWith), so it has a green border
	bool TypedTile(size_t i)
	{
		host::ShapePlacedWith with;

		return i < knownOf.size() && host::ShapeIdentity_PlacedWith(knownOf[i], &with);
	}

	// A resized copy, in words: "a resized copy of medbox01.rif (scale 0.800, every point within 0.4 mm of it; 0.9 allowed)"
	std::string ResizedWords(host::ShapeIdentityRow const *row, host::ShapeResizedAnswer const &answer)
	{
		if (answer.sameSize)   // at the stock's own size, every point exactly its: another FILE of that shape (the old tools' package's, Bishop's (a))
		{
			return "the same shape as " + host::ShapePicker_SafeName(row->fileName) + " (another file of it)";
		}

		char numbers[128];

		snprintf(numbers, sizeof numbers, " (scale %.3f, every point within %.1f mm of it; %.1f allowed)", answer.scale, answer.worstMm, answer.allowedMm);
		return "a resized copy of " + host::ShapePicker_SafeName(row->fileName) + numbers;
	}

	// The tile under the pointer now (ShapePickerScreen_Hover), or -1: the pointer hasn't moved over the window, or isn't over a tile's picture or name
	int HoveredTile()
	{
		return pointerSeen ? host::ShapePicker_TileAt(layout, view.first, list.tiles.size(), pointerX, pointerY) : -1;
	}

	// The words for the tile under the pointer: why it has a green border, or what its object would be made as instead
	std::string HoverWords(size_t i)
	{
		host::ShapePickerTile const &tile = list.tiles[i];
		std::string const name = host::ShapePicker_SafeName(tile.name);
		bool const notRead = !knownOf[i] && !(tile.inLevel && levelNumberOf[i] >= 0) && shaOf[i].empty();

		if (notRead)
		{
			return name + ": its file isn't read yet, so what it is isn't known yet";
		}

		if (TypedTile(i) && i < resizedOf.size() && resizedOf[i].same)
		{
			return name + ": green border: " + ResizedWords(knownOf[i], resizedOf[i]) + ", " + host::ShapeIdentity_PlacedAsWords(knownOf[i]);
		}

		if (TypedTile(i))
		{
			return name + ": green border: a stock shape known by its file's SHA-256 (" + host::ShapePicker_SafeName(knownOf[i]->fileName) + "), " +
				host::ShapeIdentity_PlacedAsWords(knownOf[i]);
		}

		bool const nearMiss = !knownOf[i] && i < resizedOf.size() && !resizedOf[i].why.empty();   // a stock pickup's name, not its shape at a size

		return name + ": " + host::ShapeIdentity_PlacedAsWords(knownOf[i]) + (nearMiss ? "; " + resizedOf[i].why : std::string());
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

	// THE ORDER BY KIND (Frost 2026-11-24; the owner, via Bishop: "1 weapons & pickups, lights, screens, furniture, props, switches, tracks, fans and rest,
	// fragment, debirs and unkonw last"; AvP-docs tools/frost/picker-sort/PLAN.md). As the picker opens, before anything is drawn: a folder's tile is known by
	// its file's bytes as the opening's look read them (tile.fileSha), an "in this level" tile by the file the level's own shape makes (knownOf, worked out just
	// before); then each tile gets its group (shape_picker.h ShapePicker_KindGroup) and every list kept per tile is put in the same new order
	// (ShapePicker_KindOrder), so a tile's number, parts, row and state stay its own. A file not read in the look's time is unknown here (last), even if its
	// drawing later knows it. Lists of another length than the tiles (never, as Open makes them) leave the order as it was.
	template <typename T> void Reorder(std::vector<T> *items, std::vector<size_t> const &order)
	{
		std::vector<T> moved;

		moved.reserve(order.size());

		for (size_t k : order)
		{
			moved.push_back((*items)[k]);
		}

		items->swap(moved);
	}

	void SortByKind()
	{
		size_t const count = list.tiles.size();

		kindGroupOf.assign(count, host::kShapePickerKindGroups);

		if (states.size() != count || shaOf.size() != count || noPictureOf.size() != count || levelNumberOf.size() != count || levelPartsOf.size() != count ||
			knownOf.size() != count || resizedOf.size() != count)   // (resizedOf: Vasquez, a resized copy's answer moves with its tile, Frost's note)
		{
			return;
		}

		std::vector<std::pair<int, std::string>> keys;

		keys.reserve(count);

		for (size_t i = 0; i < count; i++)
		{
			host::ShapePickerTile const &tile = list.tiles[i];

			if (!(tile.inLevel && levelNumberOf[i] >= 0) && !tile.fileSha.empty())
			{
				knownOf[i] = host::ShapeIdentity_Find(tile.fileSha);   // a folder's file: by its own bytes, the ones placed
			}

			host::ShapeIdentityRow const *const row = knownOf[i];

			kindGroupOf[i] = host::ShapePicker_KindGroup(row != nullptr, row ? row->kind : "", row ? row->levels : "");
			keys.push_back(std::make_pair(kindGroupOf[i], tile.name));
		}

		std::vector<size_t> const order = host::ShapePicker_KindOrder(keys);

		if (order.size() != count)
		{
			return;
		}

		Reorder(&list.tiles, order);
		Reorder(&states, order);
		Reorder(&shaOf, order);
		Reorder(&noPictureOf, order);
		Reorder(&levelNumberOf, order);
		Reorder(&levelPartsOf, order);
		Reorder(&knownOf, order);
		Reorder(&resizedOf, order);   // (so a tile never takes another tile's resized answer once it is filled at the open)
		Reorder(&kindGroupOf, order);
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
		host::ShapeIdentityRow const *const known = knownOf[view.selected];   // (a row of the built-in table: it outlives the picker)
		std::string const resizedWords = known && view.selected < resizedOf.size() && resizedOf[view.selected].same ?
			ResizedWords(known, resizedOf[view.selected]) : std::string();   // how it is known, when by its size (said on the status line once made)

		ShapePickerScreen_Close(false);
		ShapeChooserCreateFromPicker(host::ShapePicker_NameBase(tile), number, known, resizedWords);   // the object made, with its automatic name, and following the mouse (construction_modes_screen.cpp); a part's base is its object
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

	// THE LEVEL'S OWN SHAPES as tiles too (shape_picker.h ShapePicker_AddLevelShapes; Bishop 2's decision on the owner's "items from shape_rifs + level"): a
	// shape the level alone holds, and each part of a fragment file, drawn and placed by number. ADDED BEFORE THE LOOK (Dietrich 2026-11-24, Bishop's card:
	// "fold the LEVEL's own tiles too"), each with its shape's number, so the look reads the level's shape and the thinning folds it like any other tile
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

	std::vector<int32_t> lookNumbers(list.tiles.size(), -1);   // each tile's level shape for the look: the level's alone by its own number, an "in this level" file's
	                                                          // by the level's FIRST of that file (as it is drawn and placed, below)

	for (size_t i = 0; i < list.tiles.size(); i++)
	{
		if (list.tiles[i].levelOnly)
		{
			lookNumbers[i] = list.tiles[i].levelNumber;
			continue;
		}

		for (host::ShapeChoice const &one : shapes)
		{
			if (list.tiles[i].inLevel && lookNumbers[i] < 0 && _stricmp(one.name.c_str(), list.tiles[i].name.c_str()) == 0)
			{
				lookNumbers[i] = one.number;
			}
		}
	}

	// NO GEOMETRY, NO TILE; COPIES FOLDED (the owner 11-23, through Bishop 2): each tile looked at (a folder's file, or the level's own shape), then the empty
	// ones left out and the lookalikes folded, the level's own tile kept when it is one of them
	size_t pastTime = 0;
	DWORD lookMs = 0;
	std::vector<host::ShapePickerLook> const looks = LookAtTheFiles(lookNumbers, &pastTime, &lookMs);

	for (size_t i = 0; i < list.tiles.size() && i < looks.size(); i++)
	{
		list.tiles[i].fileSha = looks[i].fileSha;   // each tile keeps what its file is known by, through the folding and the level's tiles (the order by kind)
	}

	host::ShapePicker_Thin(&list, looks, pastTime, &thinned);

	// A SLOW LOOK SAID (Dietrich's NIT on 7ad5b295): the look runs before the window can draw again, so a word during it would never be seen; when it took a
	// second or more (a stranger's large folder; the owner's 352 files take about 70 ms), the status line says afterwards why the window waited
	std::string const slowWords = lookMs >= 1000 ? " (looking at the files took " + std::to_string((unsigned long)(lookMs / 1000)) + "." +
		std::to_string((unsigned long)((lookMs % 1000) / 100)) + " s: each one is read once, to leave out the empty ones and fold the copies)" : std::string();

	if (!slowWords.empty())
	{
		list.notes.push_back("looking at the files took " + std::to_string((unsigned long)lookMs) + " ms as the picker opened");
	}

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

	// TYPED PLACEMENT: the level's own shapes known by the files they make (shape_extract.h ShapeExtract_FileSums), on the level as the next save would write it.
	// Its bytes are the level's own, so hostile: the extractor checks every size as it walks them. Out of memory, or bytes it can't walk, leave those tiles
	// unknown (their objects are made static), said on the state line.
	knownOf.assign(list.tiles.size(), nullptr);
	resizedOf.assign(list.tiles.size(), host::ShapeResizedAnswer());
	pointerSeen = false;
	levelSumsWords.clear();
	ReadStockPickups(folders);   // what a resized copy is compared with as its tile is drawn: Steam's own pickups, from the folders the list was made of

	bool anyFromLevel = false;

	for (size_t i = 0; i < list.tiles.size(); i++)
	{
		anyFromLevel = anyFromLevel || (list.tiles[i].inLevel && levelNumberOf[i] >= 0);
	}

	if (anyFromLevel)
	{
		DWORD const sumsStart = GetTickCount();
		size_t fromLevel = 0, knownFromLevel = 0;

		try
		{
			std::vector<char> image;
			std::map<int32_t, std::string> sums;

			if (!levelFile->BuildLevel(image) || !host::ShapeExtract_FileSums(image, &sums))
			{
				levelSumsWords = "the level's bytes couldn't be built or walked";
			}
			else
			{
				for (size_t i = 0; i < list.tiles.size(); i++)
				{
					if (list.tiles[i].inLevel && levelNumberOf[i] >= 0)
					{
						auto const sum = sums.find(levelNumberOf[i]);

						knownOf[i] = sum == sums.end() ? nullptr : host::ShapeIdentity_Find(sum->second);
						fromLevel++;
						knownFromLevel += knownOf[i] ? 1 : 0;
					}
				}

				levelSumsWords = std::to_string((unsigned long long)knownFromLevel) + " of " + std::to_string((unsigned long long)fromLevel) + " known in " +
					std::to_string((unsigned long long)(GetTickCount() - sumsStart)) + " ms";
			}
		}
		catch (std::bad_alloc const &)
		{
			knownOf.assign(list.tiles.size(), nullptr);
			levelSumsWords = "not enough memory to look them up";
		}
	}

	// THE LEVEL'S RESIZED COPIES KNOWN BEFORE THE ORDER (Dietrich 2026-11-24, Bishop's card; Frost's find: a resized copy was known only as its tile was DRAWN,
	// after the order by kind, so Invasion's own rescaled pickups sorted as unknown, last, though they got their green border). Each level tile still unknown
	// here is read as its drawing reads it (LevelShapeAsFile, then ShapeThumb_Read: every count checked, the level's bytes being hostile) and compared with
	// the stock pickups (FindResized, shape_resized.h), so it sorts into its kind's group. At most kShapePickerMostLookMs in all; a tile past it, or out of
	// memory, stays unknown at the open, and its drawing still finds it later, as before.
	if (anyFromLevel && !stockPickups.empty())
	{
		DWORD const resizedStart = GetTickCount();
		size_t compared = 0, resizedFound = 0, pastTime = 0;

		try
		{
			for (size_t i = 0; i < list.tiles.size(); i++)
			{
				if (knownOf[i] || !(list.tiles[i].inLevel && levelNumberOf[i] >= 0))
				{
					continue;
				}

				if (GetTickCount() - resizedStart > host::kShapePickerMostLookMs)
				{
					pastTime++;
					continue;
				}

				std::vector<char> bytes;
				host::ShapeThumbModel model;
				std::string why;
				Kept unused;   // (FindResized also notes its answer on a drawing; at the open there is none yet)

				if (!LevelShapeAsFile(levelNumberOf[i], &bytes) || !host::ShapeThumb_Read(bytes, &model, &why))
				{
					continue;   // (its drawing says why, as before)
				}

				compared++;
				FindResized(i, model, &unused);
				resizedFound += knownOf[i] ? 1 : 0;
			}

			levelSumsWords += "; resized at the open " + std::to_string((unsigned long long)resizedFound) + " of " + std::to_string((unsigned long long)compared) +
				" in " + std::to_string((unsigned long long)(GetTickCount() - resizedStart)) + " ms" +
				(pastTime ? ", " + std::to_string((unsigned long long)pastTime) + " past the time" : "");
		}
		catch (std::bad_alloc const &)
		{
			levelSumsWords += "; resized at the open: not enough memory";   // (those found so far keep their row: each is a whole answer)
		}
	}

	SortByKind();

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
	knownOf.clear();
	resizedOf.clear();
	stockPickups.clear();
	kindGroupOf.clear();
	pointerSeen = false;
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
		t.typed = TypedTile(i);   // its object is made as the game object its kind says: a green border round its picture
		t.name = host::ShapePicker_SafeName(list.tiles[i].name);   // drawn in inteng's font: the safe rule's letters only
		t.words = states[i].drawing == host::ShapePickerDrawing::Waiting ? "drawing..." : states[i].drawing == host::ShapePickerDrawing::Failed ? "can't be drawn" :
			"no picture";
		tiles.push_back(t);
	}

	// IN THIS ORDER (the game's 2D list draws things in the order they are added):
	// 1. each tile's BLACK BOX at half opacity, under everything of the tile (the owner, 2026-11-24: "I want a 50% oppacity background in black behind the
	//    spinning images, bit hard to see"; Frost);
	// 2. THE LIVE PANES over the boxes, so the names, the frame and the bar go over them: the picture DrawPanes made of them this frame (in the level's frame,
	//    into the GPU side's own texture) added to the panels here, over the object card, under the bar (Frost and Vasquez, 2026-11-23: shape_pane_gpu.h
	//    ShapePaneGpu_Composite);
	// 3. the still pictures, the words, the names, the frames, the bar and the lines (ShapePickerPaint_Draw, below).
	ShapePickerPaint_Backdrops(ownWidth, ownHeight, tiles);
	host::ShapePaneGpu_Composite();

	std::vector<std::string> lines;

	if (!list.tiles.empty() && view.selected < list.tiles.size())
	{
		lines.push_back(host::ShapePicker_TileWords(list.tiles[view.selected], states[view.selected]));
	}

	// the tile under the pointer says what it is in place of the how-to line, so the lines still fit over the status line (the owner: "when moused over explain that")
	int const under = HoveredTile();

	lines.push_back(under >= 0 ? HoverWords((size_t)under) : list.tiles.empty() ? "No shape file in the shape folders. Escape leaves." :
		"A click selects a shape, a second click (or Return) places it. Arrows, Page Up / Down move. Escape leaves.");

	for (std::string const &note : list.notes)
	{
		lines.push_back(note);
	}

	ShapePickerPaint_Draw(ownWidth, ownHeight, tiles, host::ShapePicker_Bar(layout, view, list.tiles.size(), ownWidth), lines, layout.wordsX, layout.wordsY);   // (the words inside the box's foot)
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

void ShapePickerScreen_Hover(int32_t x, int32_t y)
{
	if (open)
	{
		pointerSeen = true;
		pointerX = x;
		pointerY = y;
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

	// TYPED PLACEMENT: the tiles on the screen with a green border, each "<name>~<kind>" ('|' between); then the tile under the pointer and its words, as the
	// line under the grid says them ("none" when the pointer is over no tile); and how the level's own shapes were looked up as the picker opened
	{
		std::string typedTiles, resizedTiles, nearMisses;
		size_t const onScreen = (size_t)layout.columns * (size_t)layout.rows;

		for (size_t i = view.first; i < list.tiles.size() && i < view.first + onScreen; i++)
		{
			if (TypedTile(i))
			{
				typedTiles += (typedTiles.empty() ? "" : "|") + host::ShapePicker_SafeName(list.tiles[i].name) + "~" + knownOf[i]->kind;
			}

			if (TypedTile(i) && i < resizedOf.size() && resizedOf[i].same)   // A RESIZED COPY: "<name>~<stock file>~<scale>~<worst mm>"
			{
				char numbers[64];

				snprintf(numbers, sizeof numbers, "~%.3f~%.2f", resizedOf[i].scale, resizedOf[i].worstMm);
				resizedTiles += (resizedTiles.empty() ? "" : "|") + host::ShapePicker_SafeName(list.tiles[i].name) + "~" + host::ShapePicker_SafeName(knownOf[i]->fileName) + numbers;
			}

			if (!knownOf[i] && i < resizedOf.size() && !resizedOf[i].why.empty())   // A STOCK PICKUP'S NAME, NOT ITS SHAPE AT A SIZE: "<name>~<why>"
			{
				nearMisses += (nearMisses.empty() ? "" : "|") + host::ShapePicker_SafeName(list.tiles[i].name) + "~" + resizedOf[i].why;
			}
		}

		int const under = HoveredTile();

		StateLine_Add("pickertyped", typedTiles);
		StateLine_Add("pickerresized", resizedTiles);
		StateLine_Add("pickerresizednot", nearMisses);
		StateLine_Add("pickerstockpickups", stockPickupsWords.empty() ? std::string("none read") : stockPickupsWords);
		StateLine_Add("pickerhover", under >= 0 ? host::ShapePicker_SafeName(list.tiles[(size_t)under].name) + "~" + HoverWords((size_t)under) : std::string("none"));
		StateLine_Add("pickerlevelkinds", levelSumsWords.empty() ? std::string("none from the level") : levelSumsWords);
	}

	// THE ORDER BY KIND: the tiles on the screen in the order shown, each "<group>:<name>" ('|' between), its group as shape_picker.h ShapePicker_KindGroup
	// numbers them (1 pickups ... 9 fragments & debris, 10 unknown)
	{
		std::string order;
		size_t const onScreen = (size_t)layout.columns * (size_t)layout.rows;

		for (size_t i = view.first; i < list.tiles.size() && i < view.first + onScreen; i++)
		{
			int const group = i < kindGroupOf.size() ? kindGroupOf[i] : host::kShapePickerKindGroups;

			order += (order.empty() ? "" : "|") + std::to_string(group) + ":" + host::ShapePicker_SafeName(list.tiles[i].name);
		}

		StateLine_Add("pickerorder", order);
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

	// THE PICKER'S BOX (the owner's 10% padding, shape_picker.h ShapePickerLayout): "<x> <y> <w> <h>" in the window's pixels as pickerboxes are, and the
	// editor's own frame "of <w> <h>", so a check can see it is 10% in from each side
	{
		int32_t wx = 0, wy = 0, ww = 0, wh = 0, fw = 0, fh = 0, fx = 0, fy = 0;

		host::TextScale_BoxToWindow(layout.box[0], layout.box[1], layout.box[2], layout.box[3], &wx, &wy, &ww, &wh);
		host::TextScale_BoxToWindow(0, 0, ownWide, ownHigh, &fx, &fy, &fw, &fh);
		StateLine_Add("pickerbox", std::to_string(wx) + " " + std::to_string(wy) + " " + std::to_string(ww) + " " + std::to_string(wh) + " of " + std::to_string(fw) + " " +
			std::to_string(fh));
	}

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
