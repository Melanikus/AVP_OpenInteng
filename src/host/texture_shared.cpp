// texture_shared.cpp: the Texture Map's shared calls (see texture_shared.h): the undo of the polygon screen's edits. TextureWrapPolygon is in texture_map_screen.cpp,
// beside the pairing it reads. Written 2026-11-17 by Melanikus (Frost) for AVP_OpenInteng; the edits ring 2026-11-18. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#include "texture_shared.h"

#include <string.h>

#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "editor_shared.h"
#include "level_file.h"
#include "level_objects.h"
#include "poly_edit.h"
#include "polygon_items.h"
#include "shape_edit.h"
#include "undo_ring.h"
#include "uv_edit.h"

namespace
{
	// One polygon as the edits ring keeps it (FUN_00567ba0: shape, polygon, partner, then the polygon's texture record)
	struct PolygonState
	{
		uint8_t id[8];
		int32_t polygon;
		bool isModule;
		int32_t moduleIndex;
		int32_t partner;
		int32_t engineType;
		int32_t flags;
		int32_t colour;
		model::UvList uv;   // cornerCount 0 for a polygon with no texture
	};

	// [0] polygon (never -1 or -2: a record's first word must not be the ring's own), [1..2] the object's ID, [3] module, [4] module number, [5] partner, [6] type,
	// [7] flags, [8] colour, [9] corner count, [10..17] u, v of the four corners as their bits
	int32_t const kRecordInts = 18;

	model::UndoRing editsRing(25000, kRecordInts * 4);   // FUN_0054be10: FUN_0057e5c0(25000, 0x38)

	void Pack(PolygonState const &state, int32_t *record)
	{
		record[0] = state.polygon;
		memcpy(&record[1], state.id, 8);
		record[3] = state.isModule ? 1 : 0;
		record[4] = state.moduleIndex;
		record[5] = state.partner;
		record[6] = state.engineType;
		record[7] = state.flags;
		record[8] = state.colour;
		record[9] = state.uv.cornerCount;

		for (int c = 0; c < model::kMostUvCorners; c++)
		{
			memcpy(&record[10 + c * 2], &state.uv.corners[c].u, 4);
			memcpy(&record[11 + c * 2], &state.uv.corners[c].v, 4);
		}
	}

	void Unpack(int32_t const *record, PolygonState *state)
	{
		state->polygon = record[0];
		memcpy(state->id, &record[1], 8);
		state->isModule = record[3] != 0;
		state->moduleIndex = record[4];
		state->partner = record[5];
		state->engineType = record[6];
		state->flags = record[7];
		state->colour = record[8];
		state->uv = {};
		state->uv.cornerCount = record[9] >= 0 && record[9] <= model::kMostUvCorners ? record[9] : 0;

		for (int c = 0; c < model::kMostUvCorners; c++)
		{
			memcpy(&state->uv.corners[c].u, &record[10 + c * 2], 4);
			memcpy(&state->uv.corners[c].v, &record[11 + c * 2], 4);
		}
	}

	// The polygon as the open level holds it now; shapes read once per call through `shapes`
	bool ReadState(OpenLevelFile *open, uint8_t const id[8], bool isModule, int32_t moduleIndex, int32_t polygon, std::map<std::string, model::ShapeModel> *shapes,
		PolygonState *out)
	{
		std::string const key((char const *)id, 8);
		auto found = shapes->find(key);

		if (found == shapes->end())
		{
			model::ShapeModel shape;
			std::string why;

			if (host::ShapeEdit_Read(open->Level(), open, id, &shape, nullptr, &why) != host::ShapeEditResult::Done)
			{
				return false;
			}

			found = shapes->emplace(key, std::move(shape)).first;
		}

		if (polygon < 0 || (size_t)polygon >= found->second.polygons.size())
		{
			return false;
		}

		model::ShapePolygon const &now = found->second.polygons[(size_t)polygon];
		model::PolygonUv uv;
		std::string why;

		memcpy(out->id, id, 8);
		out->polygon = polygon;
		out->isModule = isModule;
		out->moduleIndex = moduleIndex;
		out->partner = TexturePartnerOf(open, id, polygon);
		out->engineType = now.engineType;
		out->flags = now.flags;
		out->colour = now.colour;
		out->uv = {};

		if (host::UvEdit_ReadPolygon(open, open->Level(), id, polygon, &uv, &why) == host::UvEditResult::Done && uv.textured)
		{
			out->uv = uv.uv;
		}

		return true;
	}

	bool SameUv(model::UvList const &a, model::UvList const &b)
	{
		if (a.cornerCount != b.cornerCount)
		{
			return false;
		}

		for (int c = 0; c < a.cornerCount; c++)
		{
			if (memcmp(&a.corners[c], &b.corners[c], sizeof(a.corners[c])) != 0)
			{
				return false;
			}
		}

		return true;
	}

	// What one step's walk gives each polygon: the state to put in (`target`), keyed by object and polygon, in the order first met
	struct Pending
	{
		std::map<std::pair<std::string, int32_t>, PolygonState> target;
		std::vector<std::pair<std::string, int32_t>> order;
	};

	// The polygons of the step written back, shape by shape: the type, flags and colour (SHPPOLYS), then the texture coordinates, then the pairing (FUN_00567c10
	// -> FUN_00566d60 for every shape whose pairing changed). Gives back how many polygons came back and whether one couldn't be written.
	void WriteBack(OpenLevelFile *open, Pending const &pending, size_t *changed, size_t *refused, bool *showAgain)
	{
		std::vector<std::string> ids;

		for (auto const &key : pending.order)
		{
			bool seen = false;

			for (std::string const &id : ids)
			{
				seen = seen || id == key.first;
			}

			if (!seen)
			{
				ids.push_back(key.first);
			}
		}

		for (std::string const &idText : ids)
		{
			uint8_t const *const id = (uint8_t const *)idText.data();
			model::ShapeModel shape;
			std::string why;

			if (host::ShapeEdit_Read(open->Level(), open, id, &shape, nullptr, &why) != host::ShapeEditResult::Done)
			{
				for (auto const &key : pending.order)
				{
					*refused += key.first == idText ? 1 : 0;
				}

				continue;
			}

			bool polygonsChanged = false;
			std::vector<PolygonState const *> mine;

			for (auto const &key : pending.order)
			{
				if (key.first != idText)
				{
					continue;
				}

				PolygonState const &want = pending.target.at(key);

				if (want.polygon < 0 || (size_t)want.polygon >= shape.polygons.size())
				{
					(*refused)++;
					continue;
				}

				model::ShapePolygon &polygon = shape.polygons[(size_t)want.polygon];

				if (polygon.engineType != want.engineType || polygon.flags != want.flags || polygon.colour != want.colour)
				{
					*showAgain = true;   // FUN_0055e110 makes the game's polygon again: here the game's items are made again from the working bytes
					polygon.engineType = want.engineType;
					polygon.flags = want.flags;
					polygon.colour = want.colour;
					polygonsChanged = true;
				}

				mine.push_back(&want);
			}

			if (polygonsChanged && host::ShapeEdit_WriteSameCounts(open, open->Level(), id, shape, &why) != host::ShapeEditResult::Done)
			{
				*refused += mine.size();
				continue;
			}

			// the texture coordinates of those whose list changed
			std::vector<model::PolygonUv> lists;
			TexturePolygon lead = {};

			for (PolygonState const *want : mine)
			{
				model::PolygonUv now;
				std::string readWhy;

				if (want->uv.cornerCount == 0 || host::UvEdit_ReadPolygon(open, open->Level(), id, want->polygon, &now, &readWhy) != host::UvEditResult::Done || !now.textured)
				{
					continue;
				}

				if (!SameUv(now.uv, want->uv))
				{
					now.uv = want->uv;
					lists.push_back(now);
					memcpy(lead.id, id, 8);
					lead.polygon = want->polygon;
					lead.isModule = want->isModule;
					lead.moduleIndex = want->moduleIndex;
				}
			}

			if (!lists.empty())
			{
				if (host::UvEdit_WritePolygons(open, open->Level(), id, lists, &why) == host::UvEditResult::Done)
				{
					ShowTheNewUv(open, lead, lists);
				}
				else
				{
					*refused += lists.size();
				}
			}

			// the pairing: the fake copy's while that is on, else the file's merge data written again
			std::vector<int32_t> numbers;
			bool hasMerge = false;
			bool pairingChanged = false;

			if (!TextureFakeMergeOn())
			{
				host::PolygonItems_ReadMergeData(open, open->Level(), id, &numbers, &hasMerge);

				if (!hasMerge || numbers.size() != shape.polygons.size())
				{
					numbers.assign(shape.polygons.size(), -1);   // a shape with no SHPMRGDT yet: none paired (SetMergeData gives it the chunk)
				}
			}

			for (PolygonState const *want : mine)
			{
				int32_t const now = TexturePartnerOf(open, id, want->polygon);

				if (now == want->partner)
				{
					continue;
				}

				if (TextureFakeMergeOn())
				{
					TextureFakePairSet(open, id, want->polygon, want->partner);
				}
				else if ((size_t)want->polygon < numbers.size())
				{
					numbers[(size_t)want->polygon] = want->partner;
					pairingChanged = true;
				}
			}

			if (pairingChanged && host::PolyEdit_SetMergeData(open, id, numbers, &why) != host::PolyMergeResult::Done)
			{
				(*refused)++;
			}

			*changed += mine.size();
		}
	}

	// FUN_0057e6d0 / FUN_0057e770 with FUN_00567ac0 for each record and FUN_00567c10 after
	bool Replay(bool undo)
	{
		OpenLevelFile *const open = LevelObjects_OpenForEdit();

		if (!open || !open->Level())
		{
			return false;
		}

		Pending pending;
		std::map<std::string, model::ShapeModel> shapes;
		size_t unreadable = 0;

		auto const each = [&](int32_t *record)
		{
			PolygonState recorded;
			PolygonState live;

			Unpack(record, &recorded);

			std::pair<std::string, int32_t> const key(std::string((char const *)recorded.id, 8), recorded.polygon);
			auto const already = pending.target.find(key);

			// the same polygon twice in a step: what it holds now is what the later record gave it
			if (already != pending.target.end())
			{
				live = already->second;
			}
			else if (!ReadState(open, recorded.id, recorded.isModule, recorded.moduleIndex, recorded.polygon, &shapes, &live))
			{
				unreadable++;
				return;
			}
			else
			{
				pending.order.push_back(key);
			}

			pending.target[key] = recorded;
			Pack(live, record);   // FUN_00565b90: the record takes what the polygon had, so the same record does it again
		};

		size_t changed = 0, refused = 0;
		bool showAgain = false;
		bool const ran = undo ? editsRing.Undo(each, [] {}) : editsRing.Redo(each, [] {});

		if (!ran)
		{
			textureWords = undo ? "Texture Map: nothing to undo" : "Texture Map: nothing to redo";
			statusWords = textureWords.c_str();
			return false;
		}

		WriteBack(open, pending, &changed, &refused, &showAgain);

		if (showAgain)
		{
			ShowTheWorkingBytes();   // a picture or a type came back: the game's items are made again from the working bytes
		}

		textureWords = std::string("Texture Map: ") + (undo ? "undone" : "done again") + " on " + std::to_string(changed) + " polygon" + (changed == 1 ? "" : "s") +
			(refused + unreadable ? ", " + std::to_string(refused + unreadable) + " could not be put back" : "");
		statusWords = textureWords.c_str();
		return true;
	}
}

void TextureUndo_Reset()
{
	editsRing.Reset();   // FUN_0057e630
}

// ----- the selection's ring -----
namespace
{
	// [0] 0 = it was selected, 1 = it was deselected (FUN_00567a80 / FUN_00567aa0), [1..2] the object's ID, [3] polygon, [4] module, [5] module number
	int32_t const kSelectionRecordInts = 6;

	model::UndoRing selectionRing(100000, kSelectionRecordInts * 4);   // FUN_0054bf80: FUN_0057e5c0(100000, 0xc)

	bool ReplaySelection(bool undo)
	{
		size_t selected = 0, deselected = 0;

		// FUN_00567a30: 0 -> deselect and 1; 1 -> select and 0
		auto const each = [&](int32_t *record)
		{
			TexturePolygon one = {};

			memcpy(one.id, &record[1], 8);
			one.polygon = record[3];
			one.isModule = record[4] != 0;
			one.moduleIndex = record[5];

			size_t const at = TextureSelectedIndex(one.id, one.polygon);

			if (record[0] == 0)
			{
				if (at < texturePolygons.size())
				{
					texturePolygons.erase(texturePolygons.begin() + (std::ptrdiff_t)at);
					deselected++;
				}

				record[0] = 1;
			}
			else
			{
				if (at >= texturePolygons.size() && texturePolygons.size() < kTexturePolygonsMost)
				{
					texturePolygons.push_back(one);
					selected++;
				}

				record[0] = 0;
			}
		};

		bool const ran = undo ? selectionRing.Undo(each, [] {}) : selectionRing.Redo(each, [] {});

		if (!ran)
		{
			textureWords = undo ? "Texture Map: no selection to undo" : "Texture Map: no selection to redo";
			statusWords = textureWords.c_str();
			return false;
		}

		ShowTheTexturePolygons();
		textureWords = std::string("Texture Map: selection ") + (undo ? "undone" : "done again") + ": " + std::to_string(selected) + " selected, " + std::to_string(deselected) +
			" deselected; " + std::to_string(texturePolygons.size()) + " selected";
		statusWords = textureWords.c_str();
		return true;
	}
}

void TextureSelectionUndo_BeginStep()
{
	selectionRing.BeginStep();   // FUN_00543fa0 0x543fa5
}

void TextureSelectionUndo_Record(TexturePolygon const &one, bool selected)
{
	if (!selectionRing.HasSlots())
	{
		selectionRing.Reset();
	}

	int32_t *const record = selectionRing.NewRecord();   // null while an undo or redo of the selection runs

	if (!record)
	{
		return;
	}

	record[0] = selected ? 0 : 1;
	memcpy(&record[1], one.id, 8);
	record[3] = one.polygon;
	record[4] = one.isModule ? 1 : 0;
	record[5] = one.moduleIndex;
}

bool TextureSelectionUndo_Undo()
{
	return ReplaySelection(true);
}

bool TextureSelectionUndo_Redo()
{
	return ReplaySelection(false);
}

void TextureSelectionUndo_Reset()
{
	selectionRing.Reset();   // 0x54bcc8
}

void TextureUndo_ForgetAll()
{
	editsRing.Reset();
	selectionRing.Reset();
}

void TextureUndo_BeginStep()
{
	editsRing.BeginStep();
}

void TextureUndo_RecordSelection()
{
	// FUN_00573570: only when something is selected (+0xc); FUN_0057e940, then FUN_00567ba0 for every selected polygon
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || texturePolygons.empty())
	{
		return;
	}

	if (!editsRing.HasSlots())
	{
		editsRing.Reset();
	}

	editsRing.BeginStep();

	std::map<std::string, model::ShapeModel> shapes;

	for (TexturePolygon const &one : texturePolygons)
	{
		PolygonState state;

		if (!ReadState(open, one.id, one.isModule, one.moduleIndex, one.polygon, &shapes, &state))
		{
			continue;
		}

		int32_t *const record = editsRing.NewRecord();

		if (!record)
		{
			return;   // while an undo runs, or past the ring's limit (undo_ring.h)
		}

		Pack(state, record);
	}
}

void TextureUndo_RecordPolygon(uint8_t const id[8], int32_t polygon)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();

	if (!open || !open->Level() || !id)
	{
		return;
	}

	if (!editsRing.HasSlots())
	{
		editsRing.Reset();
	}

	// the module the selection knows it by, for the view's preview (an object of the level when it isn't selected)
	bool isModule = false;
	int32_t moduleIndex = -1;

	for (TexturePolygon const &one : texturePolygons)
	{
		if (memcmp(one.id, id, 8) == 0)
		{
			isModule = one.isModule;
			moduleIndex = one.moduleIndex;
			break;
		}
	}

	std::map<std::string, model::ShapeModel> shapes;
	PolygonState state;

	if (ReadState(open, id, isModule, moduleIndex, polygon, &shapes, &state))
	{
		int32_t *const record = editsRing.NewRecord();

		if (record)
		{
			Pack(state, record);
		}
	}
}

bool TextureUndo_Undo()
{
	return Replay(true);
}

bool TextureUndo_Redo()
{
	return Replay(false);
}
