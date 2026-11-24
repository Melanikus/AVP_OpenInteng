// See shape_identity.h. Written 2026-11-23 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// The table itself is shape_identity_data.cpp, made by tests/scenarios/verify_shape_identities.

#include "shape_identity.h"

#include <string.h>

#include "shape_extract.h"   // ShapeExtract_KindWords: the words the table's kinds were made with

namespace host
{
	namespace
	{
		std::string Lower(std::string s)
		{
			for (char &c : s)
			{
				c = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;
			}

			return s;
		}
	}

	ShapeIdentityRow const *ShapeIdentity_Find(std::string const &sha256)
	{
		for (size_t i = 0; i < kShapeIdentityRowCount; i++)
		{
			if (sha256 == kShapeIdentityRows[i].sha256)
			{
				return &kShapeIdentityRows[i];
			}
		}

		return nullptr;
	}

	ShapeIdentityStockFile const *ShapeIdentity_StockFile(std::string const &folder, std::string const &fileName)
	{
		std::string const f = Lower(folder), n = Lower(fileName);

		for (size_t i = 0; i < kShapeIdentityStockFileCount; i++)
		{
			if (f == kShapeIdentityStockFiles[i].folder && n == kShapeIdentityStockFiles[i].fileName)
			{
				return &kShapeIdentityStockFiles[i];
			}
		}

		return nullptr;
	}

	bool ShapeIdentity_PlacedAs(std::string const &kind, ShapePlacedAs *as)
	{
		// the closed list (shape_identity.h): each pair's words are made by the same function the table's kinds were made by, and compared whole
		static struct { int32_t type, extra; } const placed[] = {
			{ 4, 0 }, { 5, 0 }, { 15, 0 },
			{ 2, 0 }, { 2, 2 }, { 2, 3 }, { 2, 5 }, { 2, 6 }, { 2, 7 }, { 2, 22 }, { 2, 23 },
			{ 3, 0 }, { 3, 2 }, { 3, 3 }, { 3, 5 }, { 3, 6 }, { 3, 7 }, { 3, 8 },
		};

		*as = ShapePlacedAs();

		for (auto const &one : placed)
		{
			if (kind == ShapeExtract_KindWords(one.type, one.extra))
			{
				as->type = one.type;
				as->extra = one.extra;
				return true;
			}
		}

		return false;
	}

	bool ShapeIdentity_PlacedWith(ShapeIdentityRow const *row, ShapePlacedWith *with)
	{
		*with = ShapePlacedWith();

		if (!row)
		{
			return false;
		}

		// a pickup of the closed list: its Type and ExtraData, the maker's own fields (every stock placement of those kinds has them)
		ShapePlacedAs as;

		if (ShapeIdentity_PlacedAs(row->kind, &as))
		{
			with->type = as.type;
			with->extra = as.extra;
			return true;
		}

		// FURNITURE: its own commonest stock placement, each field within what the game and the old editor allow (shape_identity.h)
		ShapeIdentityPlacement const &p = row->placement;
		bool const furniture = strcmp(row->kind, "furniture") == 0 && p.known && p.type == 1 && p.extra == 0;
		bool const fieldsAllowed = p.mass >= 0 && p.mass <= 65535 && (p.integrity & 0x80000000u) != 0 && (p.integrity & 0x7FFFFF00u) == 0 && (p.flags & ~0x33u) == 0;

		if (!furniture || !fieldsAllowed)
		{
			return false;
		}

		with->type = 1;
		with->extra = 0;
		with->mass = p.mass;
		with->integrity = p.integrity;
		with->flags = p.flags;
		return true;
	}

	std::string ShapeIdentity_PlacedAsWords(ShapeIdentityRow const *row)
	{
		ShapePlacedWith with;

		if (!row)
		{
			return "placed as static (Type 0): its shape is not one the editor knows";
		}

		if (ShapeIdentity_PlacedWith(row, &with))
		{
			std::string const words = std::string("placed as ") + row->kind + " (the game's Type " + std::to_string(with.type) + ", ExtraData " + std::to_string(with.extra);

			// (kept short: the picker's line under the grid is one line, and at a 1008-pixel window the longer words lost their end, measured 2026-11-24)
			return with.type != 1 ? words + ")" : words + "; stock mass " + std::to_string(with.mass) + ", integrity " + std::to_string(with.integrity & 0xFFu) + ", " +
				std::to_string(row->placement.same) + " of " + std::to_string(row->placement.of) + " placements)";
		}

		if (strcmp(row->kind, "furniture") == 0)
		{
			return "placed as static (Type 0): its stock kind is furniture, but no stock placement of it gives fields the editor takes";
		}

		if (!row->kind[0])
		{
			return "placed as static (Type 0): no stock object places it";
		}

		return strcmp(row->kind, "static") == 0 ? std::string("placed as static (Type 0), as stock places it") :
			std::string("placed as static (Type 0): its stock kind is ") + row->kind + ", which placement doesn't set";
	}
}
