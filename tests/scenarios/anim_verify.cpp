// anim_verify.cpp: the Texture Map Anim menu scenario's check of one step. Usage: anim_verify.exe <the level before> <the level after (a saved copy)> <added> <sequences> <frames per sequence, comma separated, or -> <anim number or -> [removed <n>]
// Both files are opened with the game's own chunk library. It checks: the polygons that have a TEXTANIM record in the second and none in the first are exactly <added> (all in one group, each holding <sequences> sequences with the given frame counts and
// the anim number); <removed> polygons (default 0) lost their record; every other record of every shape is exactly as it was; and every frame of an added record holds the picture and the coordinates the polygon itself has in the second file (the
// scenario makes no other edit, so the frames and the polygon agree). Exit 0 only when every check holds. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include <math.h>
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

static int failed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());
	failed += good ? 0 : 1;
}

struct Record
{
	std::string object;   // the object's 8-byte ID
	int poly, id, numSeq, numVerts, flags, identifier;
	std::vector<int> frames, speeds, seqFlags, textures, uv;   // per sequence: frame count, speed, flags; then every frame's picture and six numbers, in order
};

static void Collect(OpenLevelFile &file, std::map<std::string, std::map<int, Record>> *out)
{
	List<Object_Chunk *> list;

	file.Level()->list_objects(&list);

	std::set<Shape_Chunk *> seen;

	for (LIF<Object_Chunk *> each(&list); !each.done(); each.next())
	{
		Object_Chunk *const object = each();
		Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

		if (!shape || seen.count(shape))
		{
			continue;
		}

		seen.insert(shape);

		Animation_Chunk *const animation = (Animation_Chunk *)shape->lookup_single_child("TEXTANIM");
		std::string const key((char const *)&object->object_data.ID, 8);

		(*out)[key];   // every shape has an entry, with or without records

		for (int i = 0; animation && i < animation->NumPolys; i++)
		{
			TEXANIM const *t = animation->AnimList[i];
			Record r;

			r.object = key;
			r.poly = t->poly;
			r.id = t->ID;
			r.numSeq = t->NumSeq;
			r.numVerts = t->NumVerts;
			r.flags = t->AnimFlags;
			r.identifier = t->Identifier;

			for (int j = 0; j < t->NumSeq; j++)
			{
				FrameList const *f = t->Seq[j];

				r.frames.push_back(f->NumFrames);
				r.speeds.push_back(f->Speed);
				r.seqFlags.push_back(f->Flags);

				for (int k = 0; k < f->NumFrames; k++)
				{
					r.textures.push_back(f->Textures[k]);
				}

				for (int k = 0; k < f->NumFrames * 2 * t->NumVerts; k++)
				{
					r.uv.push_back(f->UVCoords[k]);
				}
			}

			(*out)[key][t->poly] = r;
		}
	}
}

static bool Same(Record const &a, Record const &b)
{
	return a.poly == b.poly && a.id == b.id && a.numSeq == b.numSeq && a.numVerts == b.numVerts && a.flags == b.flags && a.identifier == b.identifier && a.frames == b.frames && a.speeds == b.speeds && a.seqFlags == b.seqFlags && a.textures == b.textures && a.uv == b.uv;
}

int main(int argc, char **argv)
{
	if (argc < 7)
	{
		printf("usage: anim_verify <before> <after> <added> <sequences> <frames,comma,separated|-> <anim number|-> [removed <n>]\n");
		return 2;
	}

	OpenLevelFile before, after;

	Check(before.Open(argv[1]) == LevelOpenResult::Opened, "the level before the step opens");
	Check(after.Open(argv[2]) == LevelOpenResult::Opened, "the saved copy opens (the game's loader takes it)");

	if (failed)
	{
		return 1;
	}

	int const wantAdded = atoi(argv[3]);
	int const wantSeq = atoi(argv[4]);
	std::vector<int> wantFrames;

	if (strcmp(argv[5], "-") != 0)
	{
		char const *at = argv[5];

		while (*at)
		{
			char *end = nullptr;
			long const value = strtol(at, &end, 10);

			if (end == at)
			{
				break;
			}

			wantFrames.push_back((int)value);
			at = *end == ',' ? end + 1 : end;
		}
	}

	bool const checkNumber = strcmp(argv[6], "-") != 0;
	int const wantNumber = checkNumber ? atoi(argv[6]) : 0;
	int wantRemoved = 0;

	if (argc >= 9 && strcmp(argv[7], "removed") == 0)
	{
		wantRemoved = atoi(argv[8]);
	}

	// props <sequence index> <speed> <sequence flags> <the polygon's flags>: what Change Properties made of one sequence of the new record
	bool const checkProps = argc >= 11 && strcmp(argv[7], "props") == 0;
	int const propSeq = checkProps ? atoi(argv[8]) : 0, propSpeed = checkProps ? atoi(argv[9]) : 0, propFlags = checkProps ? atoi(argv[10]) : 0, propRecordFlags = checkProps && argc >= 12 ? atoi(argv[11]) : 0;

	std::map<std::string, std::map<int, Record>> was, now;

	Collect(before, &was);
	Collect(after, &now);

	Check(was.size() == now.size(), "the same number of shapes: " + std::to_string(was.size()) + " and " + std::to_string(now.size()));

	size_t added = 0, removed = 0, changedOthers = 0;
	std::set<int> groups;

	for (auto const &shape : now)
	{
		std::map<int, Record> const &oldRecords = was[shape.first];

		for (auto const &entry : shape.second)
		{
			auto const old = oldRecords.find(entry.first);

			if (old == oldRecords.end())
			{
				added++;
				groups.insert(entry.second.id);

				Record const &r = entry.second;

				printf("  added: polygon %d, group %d, %d sequence(s), frames", r.poly, r.id, r.numSeq);

				for (int f : r.frames)
				{
					printf(" %d", f);
				}

				printf(", anim number %d, flags %d\n", r.identifier, r.flags);
				Check(r.numSeq == wantSeq, "the new record has " + std::to_string(wantSeq) + " sequence(s)");
				Check(r.numVerts == 3, "the new record has 3 corners");

				if (!wantFrames.empty())
				{
					Check(r.frames == wantFrames, "its sequences have the wanted frame counts");
				}

				if (checkProps)
				{
					bool const there = propSeq >= 0 && (size_t)propSeq < r.speeds.size();

					Check(there && r.speeds[(size_t)propSeq] == propSpeed && r.seqFlags[(size_t)propSeq] == propFlags && r.flags == propRecordFlags, "sequence " + std::to_string(propSeq) + " has speed " + std::to_string(propSpeed) + ", flags " + std::to_string(propFlags) + " and the polygon's flags are " + std::to_string(propRecordFlags) +
						(there ? " (it has " + std::to_string(r.speeds[(size_t)propSeq]) + ", " + std::to_string(r.seqFlags[(size_t)propSeq]) + ", " + std::to_string(r.flags) + ")" : ""));
				}

				if (checkNumber)
				{
					Check(r.identifier == wantNumber, "its anim number is " + std::to_string(wantNumber));
				}

				// every frame holds what the polygon itself holds in the saved copy
				uint8_t id[8];
				memcpy(id, shape.first.data(), 8);

				model::PolygonUv polygon;
				std::string why;

				if (host::UvEdit_ReadPolygon(&after, after.Level(), id, r.poly, &polygon, &why) == host::UvEditResult::Done && polygon.textured && polygon.uv.cornerCount >= 3)
				{
					bool agree = true;
					size_t frames = 0;

					for (size_t f = 0; f < r.textures.size(); f++)
					{
						agree = agree && r.textures[f] == (polygon.colour & 0xfff);
					}

					for (size_t at = 0; at + 6 <= r.uv.size(); at += 6, frames++)
					{
						for (int c = 0; c < 3; c++)
						{
							agree = agree && r.uv[at + (size_t)c * 2] == (int)floor(polygon.uv.corners[c].u + 0.5f) && r.uv[at + (size_t)c * 2 + 1] == (int)floor(polygon.uv.corners[c].v + 0.5f);
						}
					}

					Check(agree && frames == r.textures.size(), "every frame holds the polygon's own picture and coordinates (" + std::to_string(frames) + " frames)");
				}
				else
				{
					Check(false, "the polygon of the new record is readable in the copy");
				}
			}
			else if (!Same(old->second, entry.second))
			{
				changedOthers++;
				printf("  a record of polygon %d changed: group %d -> %d, anim number %d -> %d\n", entry.first, old->second.id, entry.second.id, old->second.identifier, entry.second.identifier);
			}
		}

		for (auto const &entry : oldRecords)
		{
			removed += shape.second.count(entry.first) ? 0 : 1;
		}
	}

	Check((int)added == wantAdded, std::to_string(added) + " polygons got a record (wanted " + std::to_string(wantAdded) + ")");
	// a trailing "groups <n>" says how many groups the new records are in (without it: at most one)
	if (argc >= 9 && strcmp(argv[argc - 2], "groups") == 0)
	{
		Check((int)groups.size() == atoi(argv[argc - 1]), std::to_string(groups.size()) + " groups among the new records (wanted " + argv[argc - 1] + ")");
	}
	else
	{
		Check(groups.size() <= 1, "the new records are all of one group");
	}
	Check((int)removed == wantRemoved, std::to_string(removed) + " polygons lost their record (wanted " + std::to_string(wantRemoved) + ")");
	Check(changedOthers == 0, std::to_string(changedOthers) + " other records changed");
	printf("%s\n", failed ? "SOME CHECKS FAILED" : "All checks passed");
	return failed ? 1 : 0;
}
