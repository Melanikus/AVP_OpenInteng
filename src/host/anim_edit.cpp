// anim_edit.cpp: see anim_edit.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "anim_edit.h"

#include <string.h>

#include <map>

#include "chunk_object.hpp"      // Object_Chunk, Shape_Chunk
#include "chunk_shape.hpp"       // lookup_single_child
#include "mishchnk.hpp"          // File_Chunk::list_objects
#include "animation_chunk.hpp"   // Animation_Chunk, TEXANIM, FrameList
#include "level_file.h"
#include "level_image.h"         // host::image: the level's bytes
#include "shape_edit.h"          // ShapeEdit_Read (the shape's number)
#include "shape_model.h"

namespace host
{
	namespace
	{
		size_t const kShapeNumberAt = 20;   // in SHPHEAD1's payload (shape_model.h)

		Object_Chunk *ObjectWithId(File_Chunk *level, uint8_t const *id)
		{
			List<Object_Chunk *> found;

			level->list_objects(&found);

			for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (object && memcmp(&object->object_data.ID, id, 8) == 0)
				{
					return object;
				}
			}

			return nullptr;
		}
	}

	AnimEditResult AnimEdit_Read(OpenLevelFile *file, uint8_t const object[8], int32_t *shapeNumber, model::TextureAnim *anim)
	{
		if (!file || !file->Level() || !object || !shapeNumber || !anim)
		{
			return AnimEditResult::BadCall;
		}

		Object_Chunk *const chunk = ObjectWithId(file->Level(), object);

		if (!chunk)
		{
			return AnimEditResult::NoObject;
		}

		Shape_Chunk *const shape = chunk->get_assoc_shape();
		model::ShapeModel model;
		std::string why;

		if (!shape || ShapeEdit_Read(file->Level(), file, object, &model, nullptr, &why) != ShapeEditResult::Done)
		{
			return AnimEditResult::NoShape;
		}

		*shapeNumber = model.number;
		anim->polygons.clear();

		Animation_Chunk const *const animation = (Animation_Chunk const *)shape->lookup_single_child("TEXTANIM");

		for (int i = 0; animation && i < animation->NumPolys; i++)
		{
			TEXANIM const *const from = animation->AnimList[i];
			model::AnimPolygon polygon;

			polygon.polygon = from->poly;
			polygon.group = from->ID;
			polygon.numVerts = from->NumVerts;
			polygon.flags = from->AnimFlags;
			polygon.identifier = from->Identifier;

			for (int j = 0; j < from->NumSeq; j++)
			{
				FrameList const *const list = from->Seq[j];
				model::AnimSequence sequence;

				sequence.speed = list->Speed;
				sequence.flags = list->Flags;
				sequence.spare1 = list->spare1;
				sequence.spare2 = list->spare2;

				for (int k = 0; k < list->NumFrames; k++)
				{
					sequence.textures.push_back(list->Textures[k]);
				}

				for (int k = 0; k < list->NumFrames * 2 * from->NumVerts; k++)
				{
					sequence.uv.push_back(list->UVCoords[k]);
				}

				polygon.sequences.push_back(sequence);
			}

			anim->polygons.push_back(polygon);
		}

		return AnimEditResult::Done;
	}

	AnimEditResult AnimEdit_Write(OpenLevelFile *file, std::vector<AnimWrite> const &writes, std::string *why)
	{
		using namespace image;

		if (!file || !file->Level() || writes.empty())
		{
			return AnimEditResult::BadCall;
		}

		// the new chunks first: nothing is touched when one doesn't fit
		std::map<int32_t, Bytes> fresh;   // a shape's number -> its new TEXTANIM chunk (empty: none)

		for (AnimWrite const &one : writes)
		{
			std::vector<unsigned char> payload;

			if (one.anim.polygons.empty())
			{
				fresh[one.shapeNumber] = Bytes();
				continue;
			}

			model::TextureAnimResult const made = model::WriteTextureAnim(one.anim, &payload);

			if (made != model::TextureAnimResult::Ok)
			{
				if (why)
				{
					*why = model::TextureAnimWords(made);
				}

				return AnimEditResult::NotFit;
			}

			fresh[one.shapeNumber] = MakeChunk("TEXTANIM", Bytes(payload.begin(), payload.end()));
		}

		Bytes imageBytes;
		std::vector<Span> top;

		if (!Walk(file, &imageBytes, &top))
		{
			return AnimEditResult::CantBuild;
		}

		Bytes out;
		size_t done = 0;

		AddCopy(&out, imageBytes, Span{ 0, kHeadingBytes });

		for (Span const &chunk : top)
		{
			std::vector<Span> inside;
			int32_t number = 0;
			bool mine = false;

			if (NameIs(imageBytes, chunk.at, "REBSHAPE") && Children(imageBytes, chunk.at + kHeadingBytes, chunk.at + chunk.size, &inside))
			{
				for (Span const &child : inside)
				{
					if (NameIs(imageBytes, child.at, "SHPHEAD1") && child.size >= kHeadingBytes + kShapeNumberAt + 4)
					{
						number = IntAt(imageBytes, child.at + kHeadingBytes + kShapeNumberAt);
						mine = fresh.count(number) != 0;
						break;
					}
				}
			}

			if (!mine)
			{
				AddCopy(&out, imageBytes, chunk);
				continue;
			}

			// the shape again: every child as it was, the TEXTANIM made again (or put at the end, or left out)
			Bytes const &replacement = fresh[number];
			size_t const start = out.size();
			bool placed = false;

			AddCopy(&out, imageBytes, Span{ chunk.at, kHeadingBytes });

			for (Span const &child : inside)
			{
				if (NameIs(imageBytes, child.at, "TEXTANIM") && !placed)
				{
					placed = true;

					if (!replacement.empty())
					{
						out.insert(out.end(), replacement.begin(), replacement.end());
					}
				}
				else
				{
					AddCopy(&out, imageBytes, child);
				}
			}

			if (!placed && !replacement.empty())
			{
				out.insert(out.end(), replacement.begin(), replacement.end());
			}

			PutIntAt(&out, start + 8, (int32_t)(out.size() - start));
			fresh.erase(number);
			done++;
		}

		if (done != writes.size() && !fresh.empty())
		{
			return AnimEditResult::CantBuild;   // a shape number the level doesn't hold
		}

		if (out.size() > kLargest)
		{
			return AnimEditResult::CantBuild;
		}

		PutIntAt(&out, 8, (int32_t)out.size());

		switch (file->ReplaceLevel(out))
		{
		case LevelOpenResult::Opened: return AnimEditResult::Done;
		case LevelOpenResult::CantRead: return AnimEditResult::CantBuild;
		default: return AnimEditResult::LoaderRefused;
		}
	}
}
