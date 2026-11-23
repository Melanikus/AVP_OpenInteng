// See shape_picker.h. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "shape_picker.h"

#include <algorithm>
#include <set>

#include "shape_import.h"   // ShapeImport_NameIsPlain: the names Quick-create takes

namespace host
{
	namespace
	{
		std::string Lower(std::string text)
		{
			for (char &c : text)
			{
				c = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
			}

			return text;
		}

		// "pulam.rif" -> "pulam" (any capitals in the ending); "" for a name that doesn't end in ".rif" or is only that
		std::string StemOf(std::string const &fileName)
		{
			return fileName.size() > 4 && Lower(fileName.substr(fileName.size() - 4)) == ".rif" ? fileName.substr(0, fileName.size() - 4) : std::string();
		}

		// whole rows of tiles: how many rows `count` tiles take (at least 1)
		size_t RowsOf(size_t count, int32_t columns)
		{
			size_t const c = columns > 0 ? (size_t)columns : 1;
			return count == 0 ? 1 : (count + c - 1) / c;
		}
	}

	ShapePickerList ShapePicker_List(std::vector<ShapePickerFolder> const &folders, std::vector<std::string> const &levelFiles)
	{
		ShapePickerList list;
		std::set<std::string> seen, inLevel;

		for (std::string const &name : levelFiles)
		{
			inLevel.insert(Lower(name));
		}

		for (ShapePickerFolder const &one : folders)
		{
			if (one.missing)
			{
				continue;
			}

			if (!one.refused.empty())
			{
				list.notes.push_back(one.folder.label + " isn't used: " + one.refused + " (no folder after it is looked in, as Quick-create's lookup stops there)");
				break;
			}

			for (size_t i = 0; i < one.names.size() && i < kShapePickerMostFilesAFolder; i++)
			{
				std::string const stem = StemOf(one.names[i]);

				if (stem.empty() || !seen.insert(Lower(stem)).second)
				{
					continue;   // not a .rif, or a name an earlier folder holds already (that one is the file Quick-create takes)
				}

				ShapePickerTile tile;
				tile.name = stem;
				tile.fileName = one.names[i];
				tile.where = one.folder.label;
				tile.path = one.folder.path + "\\" + one.names[i];
				tile.plain = ShapeImport_NameIsPlain(stem);
				tile.inLevel = inLevel.count(Lower(stem)) != 0;
				list.tiles.push_back(tile);
			}

			size_t const past = one.more + (one.names.size() > kShapePickerMostFilesAFolder ? one.names.size() - kShapePickerMostFilesAFolder : 0);

			if (past > 0)
			{
				list.notes.push_back(one.folder.label + " holds " + std::to_string((unsigned long long)past) + " more .rif files than the " +
					std::to_string((unsigned long long)kShapePickerMostFilesAFolder) + " a folder shows: they aren't shown");
			}
		}

		std::stable_sort(list.tiles.begin(), list.tiles.end(), [](ShapePickerTile const &a, ShapePickerTile const &b) { return Lower(a.name) < Lower(b.name); });
		return list;
	}

	ShapePickerLayout ShapePicker_Layout(int32_t windowWidth, int32_t windowHeight)
	{
		ShapePickerLayout layout;
		int32_t const width = windowWidth - 2 * kShapePickerGap, height = windowHeight - kShapePickerTop - kShapePickerBottomRoom;

		layout.stepX = kShapePickerPicture + kShapePickerGap;
		layout.stepY = kShapePickerPicture + kShapePickerNameHeight + kShapePickerGap;
		layout.columns = width >= layout.stepX ? width / layout.stepX : 1;
		layout.rows = height >= layout.stepY ? height / layout.stepY : 1;
		layout.left = (std::max)((int32_t)kShapePickerGap, (windowWidth - layout.columns * layout.stepX + kShapePickerGap) / 2);
		layout.top = kShapePickerTop + kShapePickerGap / 2;
		return layout;
	}

	bool ShapePicker_TileBox(ShapePickerLayout const &layout, size_t first, size_t tile, int32_t box[4])
	{
		size_t const columns = (size_t)(std::max)(1, layout.columns), shown = columns * (size_t)(std::max)(1, layout.rows);

		if (tile < first || tile - first >= shown)
		{
			return false;
		}

		size_t const at = tile - first;

		box[0] = layout.left + (int32_t)(at % columns) * layout.stepX;
		box[1] = layout.top + (int32_t)(at / columns) * layout.stepY;
		box[2] = kShapePickerPicture;
		box[3] = kShapePickerPicture;
		return true;
	}

	int ShapePicker_TileAt(ShapePickerLayout const &layout, size_t first, size_t count, int32_t x, int32_t y)
	{
		if (layout.stepX <= 0 || layout.stepY <= 0 || x < layout.left || y < layout.top)
		{
			return -1;
		}

		int32_t const column = (x - layout.left) / layout.stepX, row = (y - layout.top) / layout.stepY;
		int32_t const inX = (x - layout.left) % layout.stepX, inY = (y - layout.top) % layout.stepY;

		if (column >= layout.columns || row >= layout.rows || inX >= kShapePickerPicture || inY >= kShapePickerPicture + kShapePickerNameHeight)
		{
			return -1;   // past the grid, or in a gap
		}

		size_t const tile = first + (size_t)row * (size_t)layout.columns + (size_t)column;
		return tile < count ? (int)tile : -1;
	}

	ShapePickerView ShapePicker_Show(ShapePickerView view, ShapePickerLayout const &layout, size_t count)
	{
		size_t const columns = (size_t)(std::max)(1, layout.columns), rows = (size_t)(std::max)(1, layout.rows);
		size_t const lastRow = RowsOf(count, layout.columns) - 1;

		view.selected = count == 0 ? 0 : (std::min)(view.selected, count - 1);

		size_t firstRow = view.first / columns, selectedRow = view.selected / columns;

		firstRow = selectedRow < firstRow ? selectedRow : (selectedRow >= firstRow + rows ? selectedRow - rows + 1 : firstRow);
		firstRow = (std::min)(firstRow, lastRow + 1 > rows ? lastRow + 1 - rows : 0);
		view.first = firstRow * columns;
		return view;
	}

	ShapePickerView ShapePicker_Key(ShapePickerView view, ShapePickerLayout const &layout, size_t count, ShapePickerKey key)
	{
		if (count == 0)
		{
			return ShapePickerView();
		}

		size_t const columns = (size_t)(std::max)(1, layout.columns), page = columns * (size_t)(std::max)(1, layout.rows);
		size_t &s = view.selected;

		s = (std::min)(s, count - 1);

		switch (key)
		{
		case ShapePickerKey::Left: s = s > 0 ? s - 1 : 0; break;
		case ShapePickerKey::Right: s = s + 1 < count ? s + 1 : s; break;
		case ShapePickerKey::Up: s = s >= columns ? s - columns : s; break;
		case ShapePickerKey::Down: s = s + columns < count ? s + columns : s; break;
		case ShapePickerKey::PageUp: s = s >= page ? s - page : s % columns; break;
		case ShapePickerKey::PageDown: s = s + page < count ? s + page : count - 1; break;
		case ShapePickerKey::Home: s = 0; break;
		case ShapePickerKey::End: s = count - 1; break;
		}

		return ShapePicker_Show(view, layout, count);
	}

	ShapePickerView ShapePicker_Scroll(ShapePickerView view, ShapePickerLayout const &layout, size_t count, int rows)
	{
		size_t const columns = (size_t)(std::max)(1, layout.columns), shownRows = (size_t)(std::max)(1, layout.rows);
		size_t const allRows = RowsOf(count, layout.columns), most = allRows > shownRows ? allRows - shownRows : 0;
		size_t firstRow = view.first / columns;

		if (rows < 0)
		{
			firstRow = (size_t)(-(long long)rows) > firstRow ? 0 : firstRow - (size_t)(-(long long)rows);
		}
		else
		{
			firstRow = (std::min)(firstRow + (size_t)rows, most);
		}

		view.first = firstRow * columns;
		return view;
	}

	std::string ShapePicker_WhyNotPlaced(ShapePickerTile const &tile, ShapePickerState const &state)
	{
		if (tile.inLevel)
		{
			return std::string();   // the level's own shape, by its number: the file itself isn't read to place it
		}

		if (!tile.plain)
		{
			return "its name has a letter that isn't a letter, digit, _ or -, or is a device's (Quick-create takes plain names only)";
		}

		if (state.drawing == ShapePickerDrawing::Failed)
		{
			return "it can't be read as a shape";
		}

		if (state.parts > 1)
		{
			return "the file holds " + std::to_string((unsigned long long)state.parts) + " shapes, and a shape is brought in from a file of one; its parts can be placed once the '@' import is in";
		}

		return std::string();
	}

	std::string ShapePicker_SafeWords(std::string const &text)
	{
		std::string safe = text.size() > kShapePickerMostWordLetters ? text.substr(0, kShapePickerMostWordLetters) : text;

		for (char &c : safe)
		{
			unsigned char const u = (unsigned char)c;
			c = (u < 0x20 || u > 0x7e || c == '|' || c == '~') ? '?' : c;
		}

		return safe;
	}

	std::string ShapePicker_TileWords(ShapePickerTile const &tile, ShapePickerState const &state)
	{
		std::string words = ShapePicker_SafeWords(tile.name) + ": " + ShapePicker_SafeWords(tile.where);

		words += tile.inLevel ? "; in this level" : "";
		words += state.parts > 1 ? "; " + std::to_string((unsigned long long)state.parts) + " parts" : "";

		if (state.drawing == ShapePickerDrawing::Waiting)
		{
			words += "; drawing";
		}
		else if (state.drawing == ShapePickerDrawing::Failed)
		{
			words += "; can't be drawn: " + ShapePicker_SafeWords(state.why);
		}
		else if (state.textures > 0)
		{
			words += "; textures " + std::to_string((unsigned long long)state.texturesFound) + " of " + std::to_string((unsigned long long)state.textures);
			words += state.textureWhy.empty() ? "" : "; texture " + ShapePicker_SafeWords(state.textureWhy);
		}

		std::string const notPlaced = ShapePicker_WhyNotPlaced(tile, state);

		words += notPlaced.empty() ? "" : "; can't be placed: " + ShapePicker_SafeWords(notPlaced);
		return words;
	}
}
