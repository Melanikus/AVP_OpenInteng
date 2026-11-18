// verify_texmap.cpp: the Texture Map scenario's check of one step, written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// Usage: verify_texmap.exe <the level before the step> <the level after it (a saved copy)> <step> [numbers]
//   texture <picture file name>     Set Texture: the polygons whose picture changed (the selection) all hold ONE picture index, and that index is the BMPNAMES entry of the named file
//   flipx <W> <H> | flipy <W> <H>   X-Flip / Y-Flip: every polygon whose coordinates changed holds exactly FlipU / FlipV (uv_model.h) of what it held before, over the window 0,0,W,H (the lowest picture's size)
//   corners <W> <H> <1|-1> <0..3>   the texture window's corner commands: the one changed polygon's node n is on corner (start + n x direction) & 3 of the area 0,0 to W,H
//   wrap <W> <H> [scale] | tile <W> <H> [scale]
//                                   Wrap / Tile: the polygons whose coordinates changed (the selection) hold exactly what uv_project.h's ProjectCorners gives for the world points of their corners, for SOME chosen polygon's plane
//                                   (the one the scenario clicked is not known here: every polygon of the level is tried, and the one that matches is named); scale is the level's ENVSDSCL (1 when none)
// Every step also checks, with the game's chunk library reading both files: the copy opens, no shape's points or polygon corners changed, every other chunk of every object and shape is byte-identical (SHPPOLYS and
// SHPUVCRD apart), no polygon's flags, type, corner count or list number changed, and what the step must not change (pictures for a coordinate step, coordinates for the picture step) did not change.
// Exit 0 only when every check holds.
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "bitmap_names.hpp"
#include "level_file.h"
#include "level_chunks.h"
#include "shape_edit.h"
#include "file_pick.h"
#include "uv_edit.h"
#include "uv_project.h"

#include "arg_number.h"

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

static std::string Key(uint8_t const *id)
{
	return std::string((char const *)id, 8);
}

static std::string HexOf(std::string const &key)
{
	char text[24];

	_snprintf_s(text, sizeof(text), _TRUNCATE, "%02X%02X%02X%02X%02X%02X%02X%02X", key.size() > 0 ? (unsigned char)key[0] : 0, key.size() > 1 ? (unsigned char)key[1] : 0, key.size() > 2 ? (unsigned char)key[2] : 0, key.size() > 3 ? (unsigned char)key[3] : 0, key.size() > 4 ? (unsigned char)key[4] : 0, key.size() > 5 ? (unsigned char)key[5] : 0, key.size() > 6 ? (unsigned char)key[6] : 0, key.size() > 7 ? (unsigned char)key[7] : 0);
	return text;
}

// the same unit as the editor's (inteng's: the file's number times 100 times the level's scale, rounded to the nearest)
static int32_t InIntengUnits(int32_t fileNumber, double scale)
{
	double const value = (double)fileNumber * 100.0 * scale;

	return value >= 2147483647.0 ? INT32_MAX : (value <= -2147483648.0 ? INT32_MIN : (int32_t)(value < 0 ? value - 0.5 : value + 0.5));
}

struct PolygonRecord
{
	std::string object;
	int32_t polygon;
	model::PolygonUv uv;
};

static bool SameUv(model::PolygonUv const &a, model::PolygonUv const &b)
{
	if (a.uv.cornerCount != b.uv.cornerCount)
	{
		return false;
	}

	for (int c = 0; c < a.uv.cornerCount; c++)
	{
		if (a.uv.corners[c].u != b.uv.corners[c].u || a.uv.corners[c].v != b.uv.corners[c].v)
		{
			return false;
		}
	}

	return true;
}

// Two chunks hold the same bytes. A chunk with children (an outside shape's SHPEXTFL) has no payload of its own, so its children are compared one by one by their ids, to the depth the game's files use
static bool SameChunk(OpenLevelFile &a, Chunk *ca, OpenLevelFile &b, Chunk *cb, int depth)
{
	if (!ca || !cb)
	{
		return !ca && !cb;
	}

	std::vector<unsigned char> ba, bb;

	if (a.PayloadOf(ca, &ba) && b.PayloadOf(cb, &bb))
	{
		return ba == bb;
	}

	if (depth <= 0)
	{
		return false;
	}

	Chunk_With_Children *const pa = dynamic_cast<Chunk_With_Children *>(ca), *const pb = dynamic_cast<Chunk_With_Children *>(cb);

	if (!pa || !pb)
	{
		return false;
	}

	char const *const kids[] = { "SHPEXTFN", "EXTOBJNM", "RIFFNAME", "BMPLSTST", "SHPTEXFN", "SHPEXTFL" };

	for (char const *name : kids)
	{
		if (!SameChunk(a, pa->lookup_single_child(name), b, pb->lookup_single_child(name), depth - 1))
		{
			return false;
		}
	}

	return true;
}

// Where two chunks' payloads differ: the places of the bytes that are not the same (the size of the shorter counts as a place if the sizes differ). False when a payload can't be read (a chunk with children).
static bool DifferingPlaces(OpenLevelFile &a, Chunk *ca, OpenLevelFile &b, Chunk *cb, std::vector<size_t> *places)
{
	std::vector<unsigned char> pa, pb;

	places->clear();

	if (!ca || !cb || !a.PayloadOf(ca, &pa) || !b.PayloadOf(cb, &pb))
	{
		return false;
	}

	for (size_t i = 0; i < pa.size() && i < pb.size(); i++)
	{
		if (pa[i] != pb[i])
		{
			places->push_back(i);
		}
	}

	if (pa.size() != pb.size())
	{
		places->push_back(((size_t)1 << 32) + (pa.size() < pb.size() ? pa.size() : pb.size()));
	}

	return true;
}

int main(int argc, char **argv)
{
	// THE BASELINE: "--baseline <copy A> <copy B>" after the step's words: two saves of the same level with NO edit between them (made in the same run). inteng's own save runs prepare_for_output on the
	// whole tree (every object's version_no goes up, the shape headers rebuild their object-name lists, the shape headers inside SHPEXTFL with them), so every save differs from the one before it in those
	// chunks. With a baseline the check is: a chunk (and, for a chunk with bytes of its own, the places of the bytes) differs between before and after exactly where it differs between A and B.
	std::string baselineA, baselineB;

	for (int at = 1; at + 2 < argc; at++)
	{
		if (strcmp(argv[at], "--baseline") == 0)
		{
			baselineA = argv[at + 1];
			baselineB = argv[at + 2];
			argc = at;
			break;
		}
	}

	if (argc < 4)
	{
		printf("usage: verify_texmap <before> <after> <texture NAME | flipx W H | flipy W H | wrap W H [scale] | tile W H [scale]>\n");
		return 2;
	}

	std::string const step = argv[3];
	OpenLevelFile before, after;

	Check(before.Open(argv[1]) == LevelOpenResult::Opened, "the level before the step opens");
	Check(after.Open(argv[2]) == LevelOpenResult::Opened, "the saved copy opens (the game's loader takes it)");

	if (failed)
	{
		return 1;
	}

	OpenLevelFile baseA, baseB;
	std::map<std::string, Object_Chunk *> baseAObjects, baseBObjects;
	bool const haveBaseline = !baselineA.empty();

	if (haveBaseline)
	{
		Check(baseA.Open(baselineA.c_str()) == LevelOpenResult::Opened && baseB.Open(baselineB.c_str()) == LevelOpenResult::Opened, "the two baseline copies (saved with no edit between) open");

		if (failed)
		{
			return 1;
		}

		for (int pass = 0; pass < 2; pass++)
		{
			OpenLevelFile &file = pass == 0 ? baseA : baseB;
			List<Object_Chunk *> list;

			file.Level()->list_objects(&list);

			for (LIF<Object_Chunk *> each(&list); !each.done(); each.next())
			{
				if (each())
				{
					(pass == 0 ? baseAObjects : baseBObjects)[Key((uint8_t const *)&each()->object_data.ID)] = each();
				}
			}
		}

		printf("  (the baseline: %zu and %zu objects in the two saves made with no edit between them)\n", baseAObjects.size(), baseBObjects.size());
	}

	std::map<std::string, Object_Chunk *> beforeObjects, afterObjects;

	for (int pass = 0; pass < 2; pass++)
	{
		OpenLevelFile &file = pass == 0 ? before : after;
		List<Object_Chunk *> list;

		file.Level()->list_objects(&list);

		for (LIF<Object_Chunk *> each(&list); !each.done(); each.next())
		{
			if (each())
			{
				(pass == 0 ? beforeObjects : afterObjects)[Key((uint8_t const *)&each()->object_data.ID)] = each();
			}
		}
	}

	Check(beforeObjects.size() == afterObjects.size(), "the same number of objects: " + std::to_string(beforeObjects.size()) + " and " + std::to_string(afterObjects.size()));

	size_t shapes = 0, geometryDiffers = 0, otherChunks = 0, otherChunksDiffer = 0, flagsDiffer = 0, unreadable = 0;
	std::map<std::string, size_t> differByName;   // which kinds of chunk differ, for the report
	std::vector<PolygonRecord> was, now;   // every textured polygon of every object, before and after, in the same order

	for (auto const &entry : beforeObjects)
	{
		uint8_t id[8];

		memcpy(id, entry.first.data(), 8);

		model::ShapeModel a, b;
		std::string why;

		if (host::ShapeEdit_Read(before.Level(), &before, id, &a, nullptr, &why) != host::ShapeEditResult::Done || host::ShapeEdit_Read(after.Level(), &after, id, &b, nullptr, &why) != host::ShapeEditResult::Done)
		{
			continue;
		}

		shapes++;

		{
			Object_Chunk *const oa = beforeObjects[entry.first], *const ob = afterObjects[entry.first];
			Shape_Chunk *const sa = oa ? oa->get_assoc_shape() : nullptr, *const sb = ob ? ob->get_assoc_shape() : nullptr;
			char const *const names[] = { "SHPHEAD1", "SHPRAWVT", "SHPPNORM", "SHPVNORM", "SHPMRGDT", "SHPTEXFN", "SHPCENTR", "SHPEXTFL" };

			if (sa && sb)
			{
				for (char const *name : names)
				{
					Chunk *const ca = sa->lookup_single_child(name), *const cb = sb->lookup_single_child(name);
					std::vector<unsigned char> ba, bb;

					if (!ca && !cb)
					{
						continue;
					}

					otherChunks++;
					bool differs = !SameChunk(before, ca, after, cb, 3);

					if (haveBaseline)
					{
						Object_Chunk *const xa = baseAObjects.count(entry.first) ? baseAObjects[entry.first] : nullptr, *const xb = baseBObjects.count(entry.first) ? baseBObjects[entry.first] : nullptr;
						Shape_Chunk *const ta = xa ? xa->get_assoc_shape() : nullptr, *const tb = xb ? xb->get_assoc_shape() : nullptr;
						Chunk *const da = ta ? ta->lookup_single_child(name) : nullptr, *const db = tb ? tb->lookup_single_child(name) : nullptr;
						bool const baselineDiffers = !SameChunk(baseA, da, baseB, db, 3);
						std::vector<size_t> stepPlaces, baselinePlaces;
						bool const stepBytes = DifferingPlaces(before, ca, after, cb, &stepPlaces), baselineBytes = DifferingPlaces(baseA, da, baseB, db, &baselinePlaces);

						// the same chunk differs in the same places in a save made with no edit: it is the save's own, not the step's
						differs = differs != baselineDiffers || (stepBytes && baselineBytes && stepPlaces != baselinePlaces);

						// the header of a shape the step edited (its UV coordinates or its picture list changed): the editor rebuilt that shape, and the save numbers it as its own, so it is not held to the baseline
						if (differs && strcmp(name, "SHPHEAD1") == 0)
						{
							std::vector<unsigned char> ua, ub, fa, fb;
							Chunk *const uvA = sa->lookup_single_child("SHPUVCRD"), *const uvB = sb->lookup_single_child("SHPUVCRD");
							Chunk *const nameA = sa->lookup_single_child("SHPTEXFN"), *const nameB = sb->lookup_single_child("SHPTEXFN");
							bool const uvEdited = (uvA && uvB && before.PayloadOf(uvA, &ua) && after.PayloadOf(uvB, &ub) && ua != ub);
							bool const picturesEdited = (nameA && nameB && before.PayloadOf(nameA, &fa) && after.PayloadOf(nameB, &fb) && fa != fb);

							if (uvEdited || picturesEdited)
							{
								differs = false;
							}
						}

						if (differs)
						{
							std::string places;

							for (size_t k = 0; k < stepPlaces.size() && k < 12; k++)
							{
								places += " " + std::to_string(stepPlaces[k]);
							}

							printf("  (beyond the baseline: %s of object %s: %zu bytes differ from the copy before, at:%s; the baseline pair differs in %zu bytes)\n", name, HexOf(entry.first).c_str(), stepPlaces.size(), places.c_str(), baselinePlaces.size());
						}
					}

					otherChunksDiffer += differs ? 1 : 0;
					differByName[name] += differs ? 1 : 0;

					if (differs && differByName[name] == 1)
					{
						size_t firstAt = 0;

						while (firstAt < ba.size() && firstAt < bb.size() && ba[firstAt] == bb[firstAt])
						{
							firstAt++;
						}

						printf("  (first %s that differs: %zu bytes before, %zu after, first different byte at %zu)\n", name, ba.size(), bb.size(), firstAt);
					}
				}
			}

			std::vector<unsigned char> ha, hb;

			if (oa && ob && oa->get_header() && ob->get_header())
			{
				otherChunks++;
				bool differs = !before.PayloadOf(oa->get_header(), &ha) || !after.PayloadOf(ob->get_header(), &hb) || ha != hb;

				if (haveBaseline)
				{
					Object_Chunk *const xa = baseAObjects.count(entry.first) ? baseAObjects[entry.first] : nullptr, *const xb = baseBObjects.count(entry.first) ? baseBObjects[entry.first] : nullptr;
					std::vector<unsigned char> xha, xhb;
					bool const baselineDiffers = !xa || !xb || !xa->get_header() || !xb->get_header() || !baseA.PayloadOf(xa->get_header(), &xha) || !baseB.PayloadOf(xb->get_header(), &xhb) || xha != xhb;
					std::vector<size_t> stepPlaces, baselinePlaces;
					bool const stepBytes = DifferingPlaces(before, oa->get_header(), after, ob->get_header(), &stepPlaces), baselineBytes = xa && xb && DifferingPlaces(baseA, xa->get_header(), baseB, xb->get_header(), &baselinePlaces);

					// OBJHEAD1's version_no (the 32 bits at byte 52: Object_Header_Chunk::prepare adds 1 on every save) is compared by its VALUE, so a carry into the next byte is not a difference; every other byte is compared as it is
					{
						auto version = [](std::vector<unsigned char> const &bytes) -> int64_t { return bytes.size() >= 56 ? (int64_t)((uint32_t)bytes[52] | ((uint32_t)bytes[53] << 8) | ((uint32_t)bytes[54] << 16) | ((uint32_t)bytes[55] << 24)) : -1; };
						auto rest = [](std::vector<unsigned char> bytes) { for (size_t i = 52; i < 56 && i < bytes.size(); i++) { bytes[i] = 0; } return bytes; };
						bool const readable = !ha.empty() && !hb.empty() && !xha.empty() && !xhb.empty() && version(ha) >= 0 && version(hb) >= 0 && version(xha) >= 0 && version(xhb) >= 0;

						if (readable)
						{
							differs = rest(ha) != rest(hb) || (version(hb) - version(ha)) != (version(xhb) - version(xha));
						}
						else
						{
							differs = differs != baselineDiffers || (stepBytes && baselineBytes && stepPlaces != baselinePlaces);
						}
					}

					if (differs)
					{
						std::string places;

						for (size_t k = 0; k < stepPlaces.size() && k < 12; k++)
						{
							places += " " + std::to_string(stepPlaces[k]);
						}

						printf("  (beyond the baseline: OBJHEAD1 of object %s: %zu bytes differ from the copy before, at:%s; the baseline pair differs in %zu bytes)\n", HexOf(entry.first).c_str(), stepPlaces.size(), places.c_str(), baselinePlaces.size());
					}
				}

				otherChunksDiffer += differs ? 1 : 0;
				differByName["OBJHEAD1"] += differs ? 1 : 0;
			}
		}

		bool same = a.points.size() == b.points.size() && a.polygons.size() == b.polygons.size();

		for (size_t p = 0; same && p < a.points.size(); p++)
		{
			same = a.points[p].x == b.points[p].x && a.points[p].y == b.points[p].y && a.points[p].z == b.points[p].z;
		}

		for (size_t p = 0; same && p < a.polygons.size(); p++)
		{
			same = a.polygons[p].cornerCount == b.polygons[p].cornerCount && memcmp(a.polygons[p].corners, b.polygons[p].corners, sizeof(a.polygons[p].corners)) == 0 && a.polygons[p].engineType == b.polygons[p].engineType;
		}

		geometryDiffers += same ? 0 : 1;

		for (size_t p = 0; same && p < a.polygons.size(); p++)
		{
			PolygonRecord x, y;
			std::string whyA, whyB;

			x.object = y.object = entry.first;
			x.polygon = y.polygon = (int32_t)p;

			if (host::UvEdit_ReadPolygon(&before, before.Level(), id, (int32_t)p, &x.uv, &whyA) != host::UvEditResult::Done || host::UvEdit_ReadPolygon(&after, after.Level(), id, (int32_t)p, &y.uv, &whyB) != host::UvEditResult::Done)
			{
				unreadable++;
				continue;
			}

			if (!x.uv.textured || !y.uv.textured)
			{
				continue;
			}

			flagsDiffer += (x.uv.flags != y.uv.flags || x.uv.engineType != y.uv.engineType || x.uv.cornerCount != y.uv.cornerCount || x.uv.uvListNumber != y.uv.uvListNumber) ? 1 : 0;
			was.push_back(x);
			now.push_back(y);
		}
	}

	printf("  %zu shapes compared, %zu textured polygons (%zu unreadable)\n", shapes, was.size(), unreadable);
	Check(geometryDiffers == 0, "no shape's points or polygon corners changed (" + std::to_string(geometryDiffers) + " differ)");
	for (auto const &entry : differByName)
	{
		if (entry.second)
		{
			printf("  (%s differs in %zu objects)\n", entry.first.c_str(), entry.second);
		}
	}

	Check(otherChunksDiffer == 0, std::string(haveBaseline ? "the other chunks of every object and shape differ exactly where two saves made with no edit differ (the save's own prepare_for_output): " : "the other chunks of every object and shape are byte-identical (") + std::to_string(otherChunks) + " compared, " + std::to_string(otherChunksDiffer) + (haveBaseline ? " differ beyond the baseline)" : " differ)"));
	Check(flagsDiffer == 0, "no polygon's flags, type, corner count or list number changed (" + std::to_string(flagsDiffer) + " differ)");

	std::vector<size_t> pictureChanged, uvChanged;

	for (size_t i = 0; i < was.size(); i++)
	{
		if ((was[i].uv.colour & 0xfff) != (now[i].uv.colour & 0xfff))
		{
			pictureChanged.push_back(i);
		}

		if (!SameUv(was[i].uv, now[i].uv))
		{
			uvChanged.push_back(i);
		}
	}

	printf("  %zu polygons changed picture, %zu changed coordinates\n", pictureChanged.size(), uvChanged.size());

	if (step == "texture" && argc >= 5)
	{
		std::string const wanted = argv[4];

		Check(pictureChanged.size() >= 2, "the picture of the selection changed (" + std::to_string(pictureChanged.size()) + " polygons)");
		Check(uvChanged.empty(), "no polygon's coordinates changed");

		int32_t const index = pictureChanged.empty() ? -1 : (now[pictureChanged[0]].uv.colour & 0xfff);
		bool allOne = !pictureChanged.empty();
		bool upperBitsKept = true;

		for (size_t i : pictureChanged)
		{
			allOne = allOne && (now[i].uv.colour & 0xfff) == index;
			upperBitsKept = upperBitsKept && (now[i].uv.colour & ~0xfff) == (was[i].uv.colour & ~0xfff);
		}

		Check(allOne, "every changed polygon holds the one picture index " + std::to_string(index));
		Check(upperBitsKept, "the rest of each polygon's colour word (its list number) is kept");

		// the BMPNAMES entry with that index, by its file name
		Chunk *names = nullptr;
		std::string found = "(none)";

		if (host::FindEnvironmentChild(after.Level(), "BMPNAMES", &names) == host::LevelChunkFound::Found && names)
		{
			Chunk_With_BMPs *const holder = static_cast<Chunk_With_BMPs *>(names);

			for (LIF<BMP_Name> each(&holder->bmps); !each.done(); each.next())
			{
				if (each().index == index && each().filename)
				{
					found = each().filename;
				}
			}
		}

		auto const lowerBase = [](std::string text)
		{
			size_t const slash = text.find_last_of("\\/");

			text = slash == std::string::npos ? text : text.substr(slash + 1);

			size_t const dot = text.find_last_of('.');

			text = dot == std::string::npos ? text : text.substr(0, dot);
			std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return (char)tolower(c); });
			return text;
		};

		printf("  index %d is the BMPNAMES entry \"%s\"\n", index, found.c_str());
		Check(lowerBase(found) == lowerBase(wanted), "that picture is the one named \"" + wanted + "\"");
	}
	else if ((step == "flipx" || step == "flipy") && argc >= 6)
	{
		// the picture's size is bounded before it is used (arg_number.h)
		long width = 0, height = 0;

		if (!ArgNumber("the picture's width", argv[4], 1, 65536, &width) || !ArgNumber("the picture's height", argv[5], 1, 65536, &height))
		{
			return 2;
		}

		int32_t const w = (int32_t)width, h = (int32_t)height;
		model::TextureWindow const window = { 0, 0, w, h, w, h };
		size_t right = 0;

		Check(pictureChanged.empty(), "no polygon's picture changed");
		Check(uvChanged.size() >= 2, "the selection's coordinates changed (" + std::to_string(uvChanged.size()) + " polygons)");

		for (size_t i : uvChanged)
		{
			model::PolygonUv expected = was[i].uv;

			if ((step == "flipx" ? model::FlipU(&expected, window) : model::FlipV(&expected, window)) == model::UvModelResult::Ok && SameUv(expected, now[i].uv))
			{
				right++;
			}
		}

		Check(right == uvChanged.size(), std::string(step == "flipx" ? "every changed polygon holds FlipU" : "every changed polygon holds FlipV") + " of what it held, over 0,0," + std::to_string(w) + "," + std::to_string(h) + " (" + std::to_string(right) + " of " + std::to_string(uvChanged.size()) + ")");
	}
	else if (step == "nodemove" && argc >= 7)
	{
		// the texture window: one polygon was selected and one of its nodes (1 to 4) was moved by du, dv - that node's coordinates hold exactly the old ones plus (du, dv), every other corner is as it was, and no other polygon changed
		int const node = atoi(argv[4]) - 1;
		double const du = atof(argv[5]), dv = atof(argv[6]);

		Check(pictureChanged.empty(), "no polygon's picture changed");
		Check(uvChanged.size() == 1, "exactly one polygon's coordinates changed (" + std::to_string(uvChanged.size()) + ")");

		if (uvChanged.size() == 1)
		{
			model::PolygonUv const &before = was[uvChanged[0]].uv, &after = now[uvChanged[0]].uv;
			bool right = before.uv.cornerCount == after.uv.cornerCount && node >= 0 && node < before.uv.cornerCount;

			for (int k = 0; right && k < before.uv.cornerCount; k++)
			{
				double const wantU = before.uv.corners[k].u + (k == node ? du : 0.0), wantV = before.uv.corners[k].v + (k == node ? dv : 0.0);

				printf("  corner %d: %.2f %.2f -> %.2f %.2f (wanted %.2f %.2f)\n", k + 1, before.uv.corners[k].u, before.uv.corners[k].v, after.uv.corners[k].u, after.uv.corners[k].v, wantU, wantV);
				right = right && fabs(after.uv.corners[k].u - wantU) < 0.01 && fabs(after.uv.corners[k].v - wantV) < 0.01;
			}

			Check(right, "node " + std::to_string(node + 1) + " moved by " + std::to_string(du) + " " + std::to_string(dv) + " and every other corner stayed");
		}
	}
	else if (step == "corners" && argc >= 8)
	{
		// TM-B (Dietrich, 2026-11-18): the texture window's Clockwise / Anti-Clockwise corner commands (FUN_00556e80(direction, start)): one polygon was selected and its node n now holds the corner
		// (start + n x direction) & 3 of the texture area 0,0 to W,H: 0 (0, 0), 1 (W - 1, 0), 2 (W - 1, H - 1), 3 (0, H - 1). W and H are the area's (the picture's size, plus 1 with Add 1 on).
		long width = 0, height = 0, direction = 0, start = 0;

		if (!ArgNumber("the area's width", argv[4], 1, 65536, &width) || !ArgNumber("the area's height", argv[5], 1, 65536, &height) ||
			!ArgNumber("the direction", argv[6], -1, 1, &direction) || direction == 0 || !ArgNumber("the first corner", argv[7], 0, 3, &start))
		{
			return 2;
		}

		double const cornerU[4] = { 0.0, (double)width - 1, (double)width - 1, 0.0 };
		double const cornerV[4] = { 0.0, 0.0, (double)height - 1, (double)height - 1 };

		Check(pictureChanged.empty(), "no polygon's picture changed");
		Check(uvChanged.size() == 1, "exactly one polygon's coordinates changed (" + std::to_string(uvChanged.size()) + ")");

		if (uvChanged.size() == 1)
		{
			model::PolygonUv const &before = was[uvChanged[0]].uv, &after = now[uvChanged[0]].uv;
			bool right = before.uv.cornerCount == after.uv.cornerCount && after.uv.cornerCount >= 3;

			for (int k = 0; right && k < after.uv.cornerCount && k < 4; k++)
			{
				int const corner = (int)(((start + k * direction) % 4 + 4) % 4);

				printf("  node %d: %.2f %.2f (wanted corner %d: %.2f %.2f)\n", k + 1, after.uv.corners[k].u, after.uv.corners[k].v, corner, cornerU[corner], cornerV[corner]);
				right = right && fabs(after.uv.corners[k].u - cornerU[corner]) < 0.01 && fabs(after.uv.corners[k].v - cornerV[corner]) < 0.01;
			}

			Check(right, std::string("every node is on its corner of 0,0 to ") + std::to_string(width) + "," + std::to_string(height) + (direction > 0 ? ", clockwise" : ", anti-clockwise") +
				" from corner " + std::to_string(start));
		}
	}
	else if ((step == "wrap" || step == "tile") && argc >= 6)
	{
		// the picture's size is bounded before it is used (arg_number.h)
		long width = 0, height = 0;

		if (!ArgNumber("the picture's width", argv[4], 1, 65536, &width) || !ArgNumber("the picture's height", argv[5], 1, 65536, &height))
		{
			return 2;
		}

		int32_t const w = (int32_t)width, h = (int32_t)height;
		double const scale = argc >= 7 ? atof(argv[6]) : 1.0;
		// the texture window's four corner nodes, as the editor keeps them: the picture's corners at the start, then every X-Flip (x), Y-Flip (y), Turn A (a) and Turn B (b) done since, in order, which move the nodes as they move the polygons (inteng's FUN_00570f40 does the same to its corner points)
		std::string const flips = argc >= 8 ? argv[7] : "";
		bool const tile = step == "tile";

		Check(pictureChanged.empty(), "no polygon's picture changed");
		Check(uvChanged.size() >= 2, "the selection's coordinates changed (" + std::to_string(uvChanged.size()) + " polygons)");

		// the selection's corners in the world, in inteng's unit, in the order the editor makes them
		std::vector<model::Fix3> world;
		std::map<std::string, std::pair<model::ShapeModel, std::pair<std::vector<int32_t>, std::vector<int32_t>>>> cache;

		auto const worldOf = [&](std::string const &object) -> std::pair<model::ShapeModel, std::pair<std::vector<int32_t>, std::vector<int32_t>>> *
		{
			auto found = cache.find(object);

			if (found == cache.end())
			{
				uint8_t id[8];
				std::pair<model::ShapeModel, std::pair<std::vector<int32_t>, std::vector<int32_t>>> made;

				memcpy(id, object.data(), 8);

				if (host::FilePick_World(&after, after.Level(), id, &made.first, &made.second.first, &made.second.second) != host::FilePickResult::Done)
				{
					return nullptr;
				}

				found = cache.insert(std::make_pair(object, made)).first;
			}

			return &found->second;
		};

		bool good = true;

		for (size_t i : uvChanged)
		{
			auto *const shape = worldOf(now[i].object);

			if (!shape || (size_t)now[i].polygon >= shape->first.polygons.size())
			{
				good = false;
				break;
			}

			for (int c = 0; c < now[i].uv.uv.cornerCount; c++)
			{
				int32_t const point = shape->first.polygons[(size_t)now[i].polygon].corners[c];

				if (point < 0 || (size_t)point * 3 + 2 >= shape->second.first.size())
				{
					good = false;
					break;
				}

				world.push_back({ InIntengUnits(shape->second.first[(size_t)point * 3], scale), InIntengUnits(shape->second.first[(size_t)point * 3 + 1], scale), InIntengUnits(shape->second.first[(size_t)point * 3 + 2], scale) });
			}
		}

		Check(good, "the world points of the selection's corners are found");

		model::PolygonUv nodes = {};

		nodes.textured = true;
		nodes.engineType = 7;
		nodes.cornerCount = 4;
		nodes.uv.cornerCount = 4;
		nodes.uv.corners[0] = { 0.0f, 0.0f };
		nodes.uv.corners[1] = { (float)w, 0.0f };
		nodes.uv.corners[2] = { (float)w, (float)h };
		nodes.uv.corners[3] = { 0.0f, (float)h };

		for (char const letter : flips)
		{
			model::TextureWindow const flipWindow = { 0, 0, w, h, w, h };

			if (letter == 'x') model::FlipU(&nodes, flipWindow);
			else if (letter == 'y') model::FlipV(&nodes, flipWindow);
			else if (letter == 'a') model::TurnA(&nodes, flipWindow);
			else if (letter == 'b') model::TurnB(&nodes, flipWindow);
		}

		model::WindowCorners window = {};

		for (int k = 0; k < 4; k++)
		{
			window.u[k] = (int32_t)nodes.uv.corners[k].u;
			window.v[k] = (int32_t)nodes.uv.corners[k].v;
		}

		model::TextureWindow const rect = { 0, 0, w, h, w, h };
		model::TileSettings const tiles = { 1000, 1000, 0, 0 };
		std::vector<model::UvCorner> expectedCorners;
		size_t tried = 0;
		std::string planeAt;
		std::vector<model::UvCorner> wanted;

		for (size_t i : uvChanged)
		{
			for (int c = 0; c < now[i].uv.uv.cornerCount; c++)
			{
				wanted.push_back(now[i].uv.uv.corners[c]);
			}
		}

		for (auto const &object : beforeObjects)
		{
			if (!good || !planeAt.empty())
			{
				break;
			}

			auto *const plane = worldOf(object.first);

			if (!plane)
			{
				continue;
			}

			for (size_t p = 0; p < plane->first.polygons.size() && planeAt.empty(); p++)
			{
				model::ShapePolygon const &polygon = plane->first.polygons[p];
				model::Fix3 corners[3];
				bool usable = polygon.cornerCount >= 3;

				for (int c = 0; usable && c < 3; c++)
				{
					size_t const at = (size_t)polygon.corners[c];

					usable = polygon.corners[c] >= 0 && at * 3 + 2 < plane->second.first.size();

					if (usable)
					{
						corners[c] = { InIntengUnits(plane->second.first[at * 3], scale), InIntengUnits(plane->second.first[at * 3 + 1], scale), InIntengUnits(plane->second.first[at * 3 + 2], scale) };
					}
				}

				if (!usable)
				{
					continue;
				}

				model::Fix3 const normal = { plane->second.second[p * 3], plane->second.second[p * 3 + 1], plane->second.second[p * 3 + 2] };
				model::PlaneBasis basis;

				if (model::ProjectionBasis(corners, model::TriangleIsOriented(corners[0], corners[1], corners[2], normal), &basis) != model::UvModelResult::Ok)
				{
					continue;
				}

				tried++;

				if (model::ProjectCorners(tile, basis, window, rect, tiles, world, &expectedCorners) != model::UvModelResult::Ok || expectedCorners.size() != wanted.size())
				{
					continue;
				}

				bool equal = true;

				for (size_t k = 0; equal && k < wanted.size(); k++)
				{
					equal = expectedCorners[k].u == wanted[k].u && expectedCorners[k].v == wanted[k].v;
				}

				if (equal)
				{
					planeAt = "polygon " + std::to_string(p) + " of the object " + std::to_string(*(uint32_t const *)object.first.data()) + "/" + std::to_string(*(uint32_t const *)(object.first.data() + 4));
				}
			}
		}

		printf("  %zu candidate planes tried\n", tried);
		Check(!planeAt.empty(), std::string("the selection's coordinates are exactly what ProjectCorners gives (") + (tile ? "tile" : "wrap") + ") for the plane of " + (planeAt.empty() ? "no polygon of the level" : planeAt));
	}
	else
	{
		Check(false, "the step \"" + step + "\" is not one of texture NAME, flipx W H, flipy W H, wrap W H [scale], tile W H [scale]");
	}

	printf("%s\n", failed ? "SOME CHECKS FAILED" : "All checks passed");
	return failed ? 1 : 0;
}
