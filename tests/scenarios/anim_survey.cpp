// anim_survey.cpp: a survey of the TEXTANIM chunks of a level (Anim items of the Texture Map mode): how many shapes hold one, the polygons, groups (the ID field), sequences, frames, speeds, flags and whether the shape has merge data.
// Usage: anim_survey.exe <level>. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <set>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "animation_chunk.hpp"
#include "level_file.h"
#include "shape_edit.h"
#include "uv_edit.h"

HANDLE avp_CreateFile(const char *n, DWORD a, DWORD s, LPSECURITY_ATTRIBUTES sa, DWORD c, DWORD f, HANDLE m) { return CreateFileA(n, a, s, sa, c, f, m); }
DWORD avp_GetFileAttributes(const char *n) { return GetFileAttributesA(n); }
void avp_MessageBox(const char *m, int) { printf("  the game's code says: %s\n", m ? m : ""); }
void avp_exit(int c) { printf("  FAILED  the game's code stopped the program (code %d)\n", c); exit(c ? c : 1); }
void LogErrorStringC(const char *) {}
void Sound_ForgetAddressesInside(const void *, size_t) {}

int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("usage: anim_survey <level>\n");
		return 2;
	}

	OpenLevelFile file;

	if (file.Open(argv[1]) != LevelOpenResult::Opened)
	{
		printf("the level does not open\n");
		return 1;
	}

	List<Object_Chunk *> list;
	file.Level()->list_objects(&list);

	std::set<Shape_Chunk *> seen;
	size_t objects = 0, shapes = 0, animated = 0, withMerge = 0, records = 0, sequences = 0, frames = 0, shown = 0;
	std::map<int, size_t> groupSizes, perSeq, frameCounts, flagValues, speedValues, identifiers, animFlags;
	size_t groupsAcrossShapes = 0;
	std::map<int, std::set<Shape_Chunk *>> groupShapes;
	size_t lastQuadShapes = 0;

	for (LIF<Object_Chunk *> each(&list); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		objects++;

		Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

		if (!shape || seen.count(shape))
		{
			continue;
		}

		seen.insert(shape);
		shapes++;

		Animation_Chunk *const animation = (Animation_Chunk *)shape->lookup_single_child("TEXTANIM");

		if (!animation)
		{
			continue;
		}

		animated++;

		bool const merge = shape->lookup_single_child("SHPMRGDT") != nullptr;

		withMerge += merge ? 1 : 0;

		if (shown < 4)
		{
			shown++;
			printf("shape %zu (object %s): TEXTANIM with %d polygons, merge data %s\n", shapes, object->object_data.o_name, animation->NumPolys, merge ? "yes" : "no");

			for (int i = 0; i < animation->NumPolys && i < 4; i++)
			{
				TEXANIM const *t = animation->AnimList[i];

				printf("  poly %d ID %d seqs %d verts %d flags %d identifier %d\n", t->poly, t->ID, t->NumSeq, t->NumVerts, t->AnimFlags, t->Identifier);

				model::PolygonUv now;
				std::string why;

				if (host::UvEdit_ReadPolygon(&file, file.Level(), (uint8_t const *)&object->object_data.ID, t->poly, &now, &why) == host::UvEditResult::Done)
				{
					printf("    polygon now: type %d corners %d picture %d colour %08x uv:", now.engineType, now.cornerCount, now.pictureNumber, (unsigned)now.colour);

					for (int c = 0; c < now.uv.cornerCount; c++)
					{
						printf(" %g %g", now.uv.corners[c].u, now.uv.corners[c].v);
					}

					printf("\n");
				}

				for (int j = 0; j < t->NumSeq; j++)
				{
					FrameList const *f = t->Seq[j];

					printf("    seq %d: speed %d flags %d frames %d spare %d %d; textures", j, f->Speed, f->Flags, f->NumFrames, f->spare1, f->spare2);

					for (int k = 0; k < f->NumFrames && k < 6; k++)
					{
						printf(" %d", f->Textures[k]);
					}

					printf("; uv of frame 0:");

					for (int k = 0; k < 2 * t->NumVerts; k++)
					{
						printf(" %d", f->UVCoords[k]);
					}

					printf("\n");
				}
			}
		}

		for (int i = 0; i < animation->NumPolys; i++)
		{
			TEXANIM const *t = animation->AnimList[i];

			records++;
			groupSizes[t->ID]++;
			groupShapes[t->ID].insert(shape);
			perSeq[t->NumSeq]++;
			identifiers[t->Identifier]++;
			animFlags[t->AnimFlags]++;

			for (int j = 0; j < t->NumSeq; j++)
			{
				sequences++;
				frames += (size_t)t->Seq[j]->NumFrames;
				frameCounts[t->Seq[j]->NumFrames]++;
				flagValues[t->Seq[j]->Flags]++;
				speedValues[t->Seq[j]->Speed]++;
			}
		}
	}

	for (auto const &g : groupShapes)
	{
		groupsAcrossShapes += g.second.size() > 1 ? 1 : 0;
	}

	(void)lastQuadShapes;
	printf("%zu objects, %zu shapes, %zu with TEXTANIM (%zu of those with merge data), %zu polygon records, %zu groups (%zu across more than one shape), %zu sequences, %zu frames\n", objects, shapes, animated, withMerge, records, groupSizes.size(), groupsAcrossShapes, sequences, frames);

	auto dump = [](char const *name, std::map<int, size_t> const &m)
	{
		printf("%s:", name);

		int n = 0;

		for (auto const &e : m)
		{
			if (n++ < 14)
			{
				printf(" %d x%zu", e.first, e.second);
			}
		}

		printf("%s\n", m.size() > 14 ? " ..." : "");
	};

	dump("records per group", groupSizes);
	dump("sequences per record", perSeq);
	dump("frames per sequence", frameCounts);
	dump("sequence flags", flagValues);
	dump("sequence speeds", speedValues);
	dump("identifiers", identifiers);
	dump("anim flags", animFlags);
	return 0;
}
