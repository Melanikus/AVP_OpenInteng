// See shape_picker.h. Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "shape_picker.h"

#include <algorithm>
#include <map>
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

		// a box set (x, y, w, h), never less than 0 high
		void Set(int32_t box[4], int32_t x, int32_t y, int32_t w, int32_t h)
		{
			box[0] = x;
			box[1] = y;
			box[2] = w;
			box[3] = h > 0 ? h : 0;
		}

		bool Inside(int32_t const box[4], int32_t x, int32_t y)
		{
			return box[2] > 0 && box[3] > 0 && x >= box[0] && x < box[0] + box[2] && y >= box[1] && y < box[1] + box[3];
		}

		// the rows past one page: how far down the first row in view can go
		size_t MostFirstRow(ShapePickerLayout const &layout, size_t count)
		{
			size_t const shownRows = (size_t)(std::max)(1, layout.rows), allRows = RowsOf(count, layout.columns);

			return allRows > shownRows ? allRows - shownRows : 0;
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

	void ShapePicker_AddLevelShapes(ShapePickerList *list, std::vector<ShapePickerLevelShape> const &shapes)
	{
		if (!list)
		{
			return;
		}

		// how many of the level's shapes each file gives: more than one is a fragment file, whose parts get a tile each
		std::map<std::string, size_t> parts;

		for (ShapePickerLevelShape const &s : shapes)
		{
			parts[Lower(s.file)]++;
		}

		std::set<std::string> named;

		for (ShapePickerTile const &t : list->tiles)
		{
			named.insert(Lower(t.name));
		}

		size_t added = 0, past = 0;

		for (ShapePickerLevelShape const &s : shapes)
		{
			bool const fragment = parts[Lower(s.file)] > 1;
			std::string const name = !fragment ? s.file : (!s.gameName.empty() ? s.gameName : s.file + "#" + std::to_string((long long)s.number));

			if (name.empty() || s.number < 0 || named.count(Lower(name)) != 0)
			{
				continue;   // a folder's tile of that name (the same shape, placed by number already), or one added already
			}

			if (added >= kShapePickerMostLevelShapes)
			{
				past++;
				continue;
			}

			ShapePickerTile t;

			t.name = name;
			t.where = "the level's own shape";
			t.plain = true;          // (placed by number: the file's name isn't looked up)
			t.inLevel = true;
			t.levelOnly = true;
			t.levelNumber = s.number;
			list->tiles.push_back(t);
			named.insert(Lower(name));
			added++;
		}

		if (past > 0)
		{
			list->notes.push_back("the level holds " + std::to_string((unsigned long long)past) + " more shapes than the " + std::to_string((unsigned long long)kShapePickerMostLevelShapes) +
				" the grid shows: they aren't shown");
		}

		std::stable_sort(list->tiles.begin(), list->tiles.end(), [](ShapePickerTile const &a, ShapePickerTile const &b) { return Lower(a.name) < Lower(b.name); });
	}

	std::string ShapePicker_NameBase(ShapePickerTile const &tile)
	{
		size_t const at = tile.levelOnly ? tile.name.find('@') : std::string::npos;

		return at != std::string::npos && at > 0 ? tile.name.substr(0, at) : tile.name;
	}

	void ShapePicker_Thin(ShapePickerList *list, std::vector<ShapePickerLook> const &looks, size_t pastTime, ShapePickerThinned *out)
	{
		*out = ShapePickerThinned();

		std::vector<ShapePickerTile> kept;
		std::map<std::string, size_t> byLook;   // a fingerprint: the kept tile (in `kept`) it belongs to, the first by name (the tiles are in name order)

		kept.reserve(list->tiles.size());

		for (size_t i = 0; i < list->tiles.size(); i++)
		{
			ShapePickerLook const look = i < looks.size() ? looks[i] : ShapePickerLook();

			if (look.noGeometry)
			{
				out->leftOut.push_back(list->tiles[i].name);
				continue;
			}

			if (!look.fingerprint.empty())
			{
				auto const same = byLook.find(look.fingerprint);

				if (same != byLook.end())
				{
					ShapePickerTile &into = kept[same->second];

					into.copies.push_back(list->tiles[i].name);
					out->folded.push_back(into.name + "~" + list->tiles[i].name);
					continue;
				}

				byLook[look.fingerprint] = kept.size();
			}

			kept.push_back(list->tiles[i]);
		}

		list->tiles.swap(kept);

		auto count = [](size_t n, char const *one, char const *many) { return std::to_string((unsigned long long)n) + " " + (n == 1 ? one : many); };

		if (!out->leftOut.empty())
		{
			list->notes.push_back(count(out->leftOut.size(), "file", "files") + " left out: no shape in them to draw (the names on the state line, pickerleftout)");
		}

		if (!out->folded.empty())
		{
			list->notes.push_back(count(out->folded.size(), "copy", "copies") + " folded into the tile they look the same as (the same points and textures)");
		}

		if (pastTime > 0)
		{
			list->notes.push_back(count(pastTime, "file", "files") + (pastTime == 1 ? " wasn't" : " weren't") + " looked at as the picker opened (it took longer than " +
				std::to_string((unsigned long long)kShapePickerMostLookMs) + " ms): one with no shape says so when drawn");
		}
	}

	ShapePickerLayout ShapePicker_Layout(int32_t windowWidth, int32_t windowHeight)
	{
		ShapePickerLayout layout;
		int32_t const barRoom = kShapePickerBarWidth + kShapePickerGap;   // the scroll bar at the right, and a gap before it
		int32_t const width = windowWidth - 2 * kShapePickerGap - barRoom, height = windowHeight - kShapePickerTop - kShapePickerBottomRoom;

		layout.stepX = kShapePickerPicture + kShapePickerGap;
		layout.stepY = kShapePickerPicture + kShapePickerNameHeight + kShapePickerGap;
		layout.columns = width >= layout.stepX ? width / layout.stepX : 1;
		layout.rows = height >= layout.stepY ? height / layout.stepY : 1;
		layout.left = (std::max)((int32_t)kShapePickerGap, (windowWidth - barRoom - layout.columns * layout.stepX + kShapePickerGap) / 2);
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

	ShapePickerBar ShapePicker_Bar(ShapePickerLayout const &layout, ShapePickerView const &view, size_t count, int32_t windowWidth)
	{
		ShapePickerBar bar;
		int32_t const arrow = kShapePickerBarWidth;   // each arrow a square, as a dialog list's
		int32_t const height = (std::max)(1, layout.rows) * layout.stepY - kShapePickerGap;   // from the grid's top to its last row's foot, the names included
		int32_t const x = windowWidth - kShapePickerGap - kShapePickerBarWidth;

		if (layout.stepY <= 0 || x < 0 || height < 2 * arrow + kShapePickerLeastThumb)
		{
			return bar;   // no room for one: none (every box 0 wide)
		}

		Set(bar.column, x, layout.top, kShapePickerBarWidth, height);
		Set(bar.up, x, layout.top, kShapePickerBarWidth, arrow);
		Set(bar.down, x, layout.top + height - arrow, kShapePickerBarWidth, arrow);

		// the thumb: as long as the share of the rows in view, as far down the track as the first row is down the rows past one page
		size_t const columns = (size_t)(std::max)(1, layout.columns), shownRows = (size_t)(std::max)(1, layout.rows), allRows = RowsOf(count, layout.columns);
		size_t const most = MostFirstRow(layout, count), firstRow = (std::min)(view.first / columns, most);
		int32_t const trackTop = layout.top + arrow, track = height - 2 * arrow;
		int32_t length = most == 0 ? track : (int32_t)((int64_t)track * (int64_t)shownRows / (int64_t)allRows);

		length = (std::max)(kShapePickerLeastThumb, (std::min)(length, track));

		int32_t const thumbTop = trackTop + (most == 0 ? 0 : (int32_t)((int64_t)(track - length) * (int64_t)firstRow / (int64_t)most));

		Set(bar.thumb, x, thumbTop, kShapePickerBarWidth, length);
		Set(bar.above, x, trackTop, kShapePickerBarWidth, thumbTop - trackTop);
		Set(bar.below, x, thumbTop + length, kShapePickerBarWidth, trackTop + track - (thumbTop + length));
		return bar;
	}

	ShapePickerBarPart ShapePicker_BarPartAt(ShapePickerBar const &bar, int32_t x, int32_t y)
	{
		return Inside(bar.up, x, y) ? ShapePickerBarPart::Up : Inside(bar.down, x, y) ? ShapePickerBarPart::Down : Inside(bar.thumb, x, y) ? ShapePickerBarPart::Thumb :
			Inside(bar.above, x, y) ? ShapePickerBarPart::Above : Inside(bar.below, x, y) ? ShapePickerBarPart::Below : ShapePickerBarPart::None;
	}

	ShapePickerView ShapePicker_BarPress(ShapePickerView view, ShapePickerLayout const &layout, size_t count, ShapePickerBarPart part)
	{
		int const page = (std::max)(1, layout.rows);

		switch (part)
		{
		case ShapePickerBarPart::Up: return ShapePicker_Scroll(view, layout, count, -1);
		case ShapePickerBarPart::Down: return ShapePicker_Scroll(view, layout, count, 1);
		case ShapePickerBarPart::Above: return ShapePicker_Scroll(view, layout, count, -page);
		case ShapePickerBarPart::Below: return ShapePicker_Scroll(view, layout, count, page);
		default: return view;
		}
	}

	ShapePickerView ShapePicker_ThumbTo(ShapePickerView view, ShapePickerLayout const &layout, size_t count, ShapePickerBar const &bar, int32_t thumbTop)
	{
		size_t const columns = (size_t)(std::max)(1, layout.columns), most = MostFirstRow(layout, count);
		int32_t const trackTop = bar.up[1] + bar.up[3], room = bar.down[1] - trackTop - bar.thumb[3];   // how far the thumb's top can go down the track

		if (bar.column[2] <= 0)
		{
			return view;
		}

		if (most == 0 || room <= 0)
		{
			view.first = 0;   // every row in view (or no track to move along): the first row
			return view;
		}

		int64_t const offset = (std::max)((int64_t)0, (std::min)((int64_t)thumbTop - trackTop, (int64_t)room));

		view.first = (size_t)((offset * (int64_t)most + room / 2) / room) * columns;   // the nearest row to that share of the track
		return view;
	}

	std::string ShapePicker_BarWords(ShapePickerBar const &bar)
	{
		struct Part
		{
			char const *name;
			int32_t const *box;
		};

		Part const parts[5] = { { "up", bar.up }, { "down", bar.down }, { "above", bar.above }, { "below", bar.below }, { "thumb", bar.thumb } };
		std::string words;

		for (Part const &p : parts)
		{
			if (p.box[2] > 0 && p.box[3] > 0)
			{
				words += (words.empty() ? "" : "|") + std::string(p.name) + "~" + std::to_string(p.box[0]) + " " + std::to_string(p.box[1]) + " " + std::to_string(p.box[2]) +
					" " + std::to_string(p.box[3]);
			}
		}

		return words;
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

	std::string ShapePicker_SafeName(std::string const &name)
	{
		std::string safe = ShapePicker_SafeWords(name);

		for (char &c : safe)
		{
			c = c == ';' ? '?' : c;
		}

		return safe;
	}

	std::vector<std::string> ShapePicker_TileLines(std::string const &words, size_t perLine, size_t mostLines)
	{
		std::vector<std::string> lines;
		size_t at = 0;

		perLine = perLine > 0 ? perLine : 1;

		while (lines.size() < mostLines)
		{
			while (at < words.size() && words[at] == ' ')   // a line never starts with a space
			{
				at++;
			}

			if (at >= words.size())
			{
				break;
			}

			size_t take = words.size() - at <= perLine ? words.size() - at : perLine;   // all that is left, or a line's worth

			if (at + take < words.size() && words[at + take] != ' ')
			{
				// the line would end inside a word: end it at the last space in it instead, unless the word is longer than a whole line (then it is cut)
				size_t const space = words.rfind(' ', at + take - 1);

				take = space != std::string::npos && space > at ? space - at : take;
			}

			lines.push_back(words.substr(at, take));
			at += take;
		}

		return lines;
	}

	std::string ShapePicker_OpenWords(ShapePickerLayout const &layout, ShapePickerView const &view, std::vector<ShapePickerTile> const &tiles, size_t drawn,
		size_t failed)
	{
		std::string const selected = view.selected < tiles.size() ? ShapePicker_SafeName(tiles[view.selected].name) : std::string("none");

		return "open cols " + std::to_string(layout.columns) + " rows " + std::to_string(layout.rows) + " count " + std::to_string((unsigned long long)tiles.size()) +
			" drawn " + std::to_string((unsigned long long)drawn) + " failed " + std::to_string((unsigned long long)failed) + " first " +
			std::to_string((unsigned long long)view.first) + " selected " + selected;
	}

	std::string ShapePicker_TileWords(ShapePickerTile const &tile, ShapePickerState const &state)
	{
		std::string words = ShapePicker_SafeName(tile.name) + ": " + ShapePicker_SafeWords(tile.where);

		words += tile.inLevel ? "; in this level" : "";
		words += state.parts > 1 ? "; " + std::to_string((unsigned long long)state.parts) + " parts" + (tile.inLevel ? ": the level's first is placed" : "") : "";

		// the copies folded into it (ShapePicker_Thin): their names, made safe, at most four and then how many more
		if (!tile.copies.empty())
		{
			size_t const most = 4;

			words += "; looks the same as ";

			for (size_t c = 0; c < tile.copies.size() && c < most; c++)
			{
				words += (c ? ", " : "") + ShapePicker_SafeName(tile.copies[c]);
			}

			words += tile.copies.size() > most ? " and " + std::to_string((unsigned long long)(tile.copies.size() - most)) + " more" : "";
		}

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
