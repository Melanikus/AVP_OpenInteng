// See shape_rescale_edit.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng (P11). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "shape_rescale_edit.h"

#include <math.h>
#include <string.h>

#include <exception>
#include <vector>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk, Shape_Chunk
#include "chunk_shape.hpp"    // Shape_Chunk's children
#include "mishchnk.hpp"       // File_Chunk::list_objects

#include "level_file.h"
#include "shape_edit.h"
#include "shape_import.h"   // shape_chunks: the chunk walk (V-30a)
#include "shape_maths.h"

namespace host
{
	namespace
	{
		// V-30a (object-placement.md; Vasquez 11-17): inteng's rebuild after a scale (FUN_0050f570, 0x50fce0-0x50fd05) makes a SHPCENTR when the shape has none (operator_new(0x28) +
		// FUN_00510b40). The level is put together again with the shape numbered shapeNumber given that chunk at the end of its children, and handed to the game's loader
		// (ReplaceLevel), which takes it or refuses it. True when the shape holds a SHPCENTR afterwards.
		bool AddCentreChunk(OpenLevelFile *store, int32_t shapeNumber, std::vector<unsigned char> const &payload)
		{
			using namespace shape_chunks;

			Bytes image;
			size_t total = 0;
			std::vector<Span> top;

			if (!store || payload.size() != 16 || !store->BuildLevel(image) || image.size() < kHeadingBytes || !NameIs(image, 0, "REBINFF2") || !ChunkSizeAt(image, 0, image.size(), &total) ||
				!Children(image, kHeadingBytes, total, &top))
			{
				return false;
			}

			size_t const kNumberAt = 20;   // the shape's number in SHPHEAD1's payload (flags 4, the lock user 16)
			Bytes out(image.begin(), image.begin() + (std::ptrdiff_t)kHeadingBytes);
			bool found = false;

			for (Span const &t : top)
			{
				std::vector<Span> inside;
				bool ours = false, hasCentre = false;

				if (!found && NameIs(image, t.at, "REBSHAPE") && Children(image, t.at + kHeadingBytes, t.at + t.size, &inside))
				{
					for (Span const &c : inside)
					{
						ours = ours || (NameIs(image, c.at, "SHPHEAD1") && c.size >= kHeadingBytes + kNumberAt + 4 && IntAt(image, c.at + kHeadingBytes + kNumberAt) == shapeNumber);
						hasCentre = hasCentre || NameIs(image, c.at, "SHPCENTR");
					}
				}

				if (!ours)
				{
					out.insert(out.end(), image.begin() + (std::ptrdiff_t)t.at, image.begin() + (std::ptrdiff_t)(t.at + t.size));
					continue;
				}

				found = true;

				if (hasCentre)
				{
					return true;   // nothing to make
				}

				size_t const start = out.size();
				Bytes const centre = MakeChunk("SHPCENTR", Bytes(payload.begin(), payload.end()));

				out.insert(out.end(), image.begin() + (std::ptrdiff_t)t.at, image.begin() + (std::ptrdiff_t)(t.at + t.size));
				out.insert(out.end(), centre.begin(), centre.end());

				if (out.size() - start > 0x7fffffff)
				{
					return false;
				}

				PutIntAt(&out, start + 8, (int32_t)(out.size() - start));
			}

			if (!found || out.size() > 0x7fffffff)
			{
				return false;
			}

			PutIntAt(&out, 8, (int32_t)out.size());
			return store->ReplaceLevel(out) == LevelOpenResult::Opened;
		}

		// The first SHPEXTFN inside the first SHPEXTFL of the object's shape chunk, or null
		Chunk *FirstExternalName(Object_Chunk *object)
		{
			Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

			if (!shape)
			{
				return nullptr;
			}

			Chunk *const file = shape->lookup_single_child("SHPEXTFL");
			Chunk_With_Children *const holder = file && !file->r_u_miscellaneous() ? dynamic_cast<Chunk_With_Children *>(file) : nullptr;

			if (!holder)
			{
				return nullptr;
			}

			Chunk *const name = holder->lookup_single_child("SHPEXTFN");

			return name && !name->r_u_miscellaneous() ? name : nullptr;
		}
	}

	char const *ShapeRescaleWords(ShapeRescaleResult result)
	{
		switch (result)
		{
		case ShapeRescaleResult::Done: return "done";
		case ShapeRescaleResult::BadCall: return "no level is open, or no object is named";
		case ShapeRescaleResult::NoObject: return "the level has no object with that ID";
		case ShapeRescaleResult::BadValue: return "the size is a number above 0 and up to 1000000";
		case ShapeRescaleResult::NoExternal: return "the shape has no outside-file name to scale from (inteng does nothing then)";
		case ShapeRescaleResult::SharedShape: return "other objects use this shape too (inteng would rescale it once for each: not copied), so nothing was changed";
		case ShapeRescaleResult::BadShape: return "the shape's numbers could not be read";
		case ShapeRescaleResult::Overflow: return "a number would be too big for the level";
		case ShapeRescaleResult::Refused: return "the bytes would not take the change, so nothing was changed";
		}

		return "the shape could not be rescaled";
	}

	// THE WORK of FUN_004e1340 (the placed shape's +0x14): factorGiven false: value is the typed scale and the factor is worked out as FUN_004e1a80 does (Rescale, OP-13);
	// factorGiven true: value IS the factor (the '=' and '-' keys, OP-14: 10/7 and 0.7)
	static ShapeRescaleResult Rescale(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, bool factorGiven, double value, double levelScale, double *factorOut,
		std::string *why)
	{
		double const typed = value;

		if (factorOut)
		{
			*factorOut = 0.0;
		}

		if (!store || !level || !id)
		{
			return ShapeRescaleResult::BadCall;
		}

		// inteng's compare is 0.0 <= value <= 1,000,000.0 (0x4e1b10); a typed 0 is refused here. A given factor must be above 0 and at most the same 1,000,000 (inteng's keys
		// push only 10/7 and 0.7; the bound is ours, as plain compares, because isfinite is not to be trusted under the build's /fp:fast: an infinity got past it)
		if (factorGiven ? !(value > 0.0 && value <= kRescaleMost) : (!(typed > 0.0) || !(typed <= kRescaleMost) || !isfinite(levelScale) || !(levelScale > 0.0)))
		{
			return ShapeRescaleResult::BadValue;
		}

		try
		{
			List<Object_Chunk *> objects;
			Object_Chunk *object = nullptr;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
				{
					object = each();
					break;
				}
			}

			if (!object)
			{
				return ShapeRescaleResult::NoObject;
			}

			Chunk *const name = FirstExternalName(object);
			std::vector<unsigned char> nameBytes;

			if (!name)
			{
				return ShapeRescaleResult::NoExternal;
			}

			if (!store->PayloadOf(name, &nameBytes) || nameBytes.size() < 12)
			{
				return ShapeRescaleResult::BadShape;
			}

			double rescale = 0.0;

			memcpy(&rescale, nameBytes.data(), 8);

			if (!isfinite(rescale) || !(rescale > 0.0))
			{
				return ShapeRescaleResult::BadShape;
			}

			// FUN_004e1a80 0x4e1c69: typed / (_DAT_00607bd8 x _DAT_005ac438 x rescale), _DAT_00607bd8 = 100.0 x the level's scale
			double const denominator = ((100.0 * levelScale) * kRescaleHundredth) * rescale;
			double const factor = factorGiven ? value : typed / denominator;

			if (!isfinite(factor) || !(factor > 0.0))
			{
				return ShapeRescaleResult::Overflow;
			}

			model::ShapeModel shape;
			size_t users = 0;
			std::string words;
			ShapeEditResult const read = ShapeEdit_Read(level, store, id, &shape, &users, &words);

			if (read != ShapeEditResult::Done)
			{
				if (why)
				{
					*why = words.empty() ? ShapeEditWords(read) : words;
				}

				return ShapeRescaleResult::BadShape;
			}

			if (users != 1)
			{
				return ShapeRescaleResult::SharedShape;
			}

			if (model::RescaleShape(&shape, factor) != model::ShapeMathsResult::Ok)
			{
				return ShapeRescaleResult::Overflow;
			}

			// the rescale double, made ready before anything is put in
			double const rescaled = rescale * factor;

			if (!isfinite(rescaled))
			{
				return ShapeRescaleResult::Overflow;
			}

			memcpy(nameBytes.data(), &rescaled, 8);

			// FUN_0050f570 (the rebuild after the scaling) works the centre out again from the new extents ((max + min) / 2, each truncated toward
			// zero) and the radius about it (model::ShapeRadiusAboutCentre): SHPCENTR is 3 ints and a float. Made ready now; written into the chunk, or
			// MADE when the shape has none, as inteng's rebuild makes one (V-30a, Vasquez 11-17; before, ours wrote it only when there was one: a named gap).
			Shape_Chunk *const owned = object->get_assoc_shape();
			Chunk *const centreChunk = owned ? owned->lookup_single_child("SHPCENTR") : nullptr;
			std::vector<unsigned char> centreBytes;
			bool writeCentre = false;
			bool const makeCentre = owned && !centreChunk;   // V-30a: none, so it is made (FUN_0050f570 0x50fce0-0x50fd05)

			if (makeCentre)
			{
				centreBytes.assign(16, 0);
			}

			if (makeCentre || (centreChunk && !centreChunk->r_u_miscellaneous() && store->PayloadOf(centreChunk, &centreBytes) && centreBytes.size() == 16))
			{
				int32_t const mid[3] =
				{
					(int32_t)(((int64_t)shape.extents.maxX + (int64_t)shape.extents.minX) / 2),
					(int32_t)(((int64_t)shape.extents.maxY + (int64_t)shape.extents.minY) / 2),
					(int32_t)(((int64_t)shape.extents.maxZ + (int64_t)shape.extents.minZ) / 2),
				};
				float const aboutCentre = model::ShapeRadiusAboutCentre(shape);

				memcpy(centreBytes.data(), mid, 12);
				memcpy(centreBytes.data() + 12, &aboutCentre, 4);
				writeCentre = true;
			}

			if (ShapeEdit_WriteSameCounts(store, level, id, shape, &words) != ShapeEditResult::Done)
			{
				if (why)
				{
					*why = words;
				}

				return ShapeRescaleResult::Refused;
			}

			if (!store->SetPayload(name, nameBytes) || (writeCentre && centreChunk && !store->SetPayload(centreChunk, centreBytes)))
			{
				return ShapeRescaleResult::Refused;
			}

			// V-30a: the SHPCENTR the shape had none of, put in last (the level is put together again: no chunk of the old tree is used after this)
			if (makeCentre && !AddCentreChunk(store, shape.number, centreBytes))
			{
				if (why)
				{
					*why = "the shape's new SHPCENTR could not be put in (the level could not be put together, or the game's loader refused it)";
				}

				return ShapeRescaleResult::Refused;
			}

			if (factorOut)
			{
				*factorOut = factor;
			}

			return ShapeRescaleResult::Done;
		}
		catch (std::exception const &)
		{
			return ShapeRescaleResult::BadShape;
		}
	}

	ShapeRescaleResult ShapeRescale_ByValue(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, double typed, double levelScale, double *factorOut,
		std::string *why)
	{
		return Rescale(store, level, id, false, typed, levelScale, factorOut, why);
	}

	ShapeRescaleResult ShapeRescale_ByFactor(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, double factor, std::string *why)
	{
		return Rescale(store, level, id, true, factor, 1.0, nullptr, why);
	}

	bool ShapeRescale_Current(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, double *rescale)
	{
		if (!store || !level || !id || !rescale)
		{
			return false;
		}

		try
		{
			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				if (!each() || memcmp(&each()->object_data.ID, id, 8) != 0)
				{
					continue;
				}

				// the same chunk Rescale reads and writes (FirstExternalName), and the same 8 bytes: the SHPEXTFN's data starts with the rescale double (FUN_00514bb0 copies it to +0x30)
				Chunk *const name = FirstExternalName(each());
				std::vector<unsigned char> nameBytes;

				if (!name || !store->PayloadOf(name, &nameBytes) || nameBytes.size() < 8)
				{
					return false;
				}

				memcpy(rescale, nameBytes.data(), 8);
				return true;
			}
		}
		catch (std::exception const &)
		{
		}

		return false;
	}
}
