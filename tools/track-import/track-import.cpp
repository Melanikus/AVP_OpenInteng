// track-import.cpp: animated track objects from a baked animation (the owner, 11-19: "a walking mech ... exported from Blender as an animated robot as a few dozen tracked objects in
// sync"; Bishop's proof order: a small Blender-baked arm, written as track objects into a stock level, played by the game's own track code). Written 2026-11-19 by Melanikus
// (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// A command-line tool built from the editor's own code: the level is read, changed and checked by the game's loader through OpenLevelFile, as every editor change is. No window.
//
//   track-import probe <level.rif> <x> <y> <z>
//       the floor below and the ceiling above that point (the file's units), from the level's modules' polygons (AutoLinkScene, the editor's own module scene)
//   track-import parts <level.rif> <out.rif> <source> <scale> <name> [<source> <scale> <name> ...]
//       for each: a NEW placed object <name> around a COPY of <source>'s shape (PolyEdit_CopyShapeAsObject), sized by <scale> with Rescale (ShapeRescale_ByFactor, the
//       editor's '='/'-' work): the level's own objects stay as they were. Says each new shape's box.
//   track-import import <level.rif> <anim.json> <out.rif> [linear|smooth]
//       each part of the animation a TRACK OBJECT: its strategy Track (kind 72, AssignStrategy), its place and turn the first key's (TrackSetObjectPose), its OBJTRAK2 one section
//       per pair of keys (TrackStore), looping, playing at the level's start, timer 0. LINEAR (the default): each section's pivot is the part's ROOT JOINT, its object offset the
//       part's origin seen from that joint (track.cpp:352-376: the place is the pivot plus the offset turned by the section's turn). SMOOTH: the smoothing flag, the pivots the
//       part's ORIGIN (with smoothing the game's place IS the spline through the pivots and the offset is never added: track.cpp:333-338).
//   track-import check <level.rif> <anim.json> [loops: 1, 10, 100, 1000 or 10000]
//       every part's track played TOGETHER by the game's own code (game_track_check.h), every frame the same NormalFrameTime as the game gives all of them, for that many loops
//       (default 100) at 60 frames a second, then again at an uneven frame rate: the gap at every joint the parts share, each part's distance from the baked pose, the seam, and
//       how near any part comes to the player's collision shape at the start.
//   track-import posecopy <level.rif> <anim.json> <seconds> <copy.rif>
//       a copy whose parts' own place and turn are the game's track pose at that moment (for the editor's pictures: it draws an object at its place)
//
// THE ANIMATION (anim.json, made by AvP-docs\tools\frost\mech\mech_arm.py in Blender): "fps", "seconds", "dense_fps", "player" { "place", "radius" }, "parts": [ { "name",
// "joint_root", "joint_end", "box_min", "box_max" (the part's own frame, the file's units), "keys" (fps x seconds + 1 of [x, y, z, qx, qy, qz, qw]: the part's origin in the
// level and its turn as the file holds one), "dense" (dense_fps x seconds of the same, the truth the check compares with) } ].
//
// THE GAME'S LIMITS (Bishop's reading): a track's sections are one PoolAllocateMem of 180 bytes each and a pool block is 1 MiB (mempool.cpp:20), so at most 5825 sections a track;
// the level's pool is 40 blocks, so the import keeps all its tracks under 4 MiB. The loader trusts the file's count: these are enforced HERE.
//
// THE INPUT IS HOSTILE: the JSON reader has bounds on size, depth, strings and counts; every number is checked against its bound before it is used; the level is only ever changed
// through the game's own loader.

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <new>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "environment_chunk.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "autolink_scene.h"
#include "object_edit.h"
#include "poly_edit.h"
#include "shape_edit.h"
#include "shape_rescale_edit.h"
#include "track_edit.h"
#include "game_track_check.h"

HANDLE avp_CreateFile(const char *name, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES security, DWORD creation, DWORD flags, HANDLE model)
{
	return CreateFileA(name, access, share, security, creation, flags, model);
}

DWORD avp_GetFileAttributes(const char *name)
{
	return GetFileAttributesA(name);
}

void avp_MessageBox(const char *message, int)
{
	printf("track-import: the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("track-import: FAILED: the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

namespace
{
	size_t const kMostSectionsPerTrack = 5825;            // 1 MiB / 180 bytes (mempool.cpp MEMORY_BLOCK_SIZE, sizeof(TRACK_SECTION_DATA))
	size_t const kMostImportedBytes = 4u * 1024u * 1024u;  // all the import's tracks, of the level's 40 MiB pool
	size_t const kSectionBytes = 180;
	double const kMostPlace = 16777216.0;                 // 2^24, the game's place bound

	// THE READER'S OWN BOUNDS (the tool is 32-bit: a value held costs about 64 bytes, and 20 million of them ran it out of memory with no word said). The largest import the game takes
	// is 4 MiB of sections, about 23,300 keys of 8 values, and a check's dense poses a few times that: 4 million values (about 256 MB held) is well above any of it.
	size_t const kMostValues = 4000000;
	size_t const kMostTextBytes = 1024u * 1024u;          // every name together

	// ---------------------------------------------------------------- a small strict JSON reader

	struct Json
	{
		enum Kind { Null, Bool, Number, Text, Array, Object } kind = Null;
		double number = 0.0;
		bool flag = false;
		std::string text;
		std::vector<Json> items;
		std::vector<std::pair<std::string, Json> > members;

		Json const *Get(char const *key) const
		{
			for (auto const &m : members)
			{
				if (m.first == key)
				{
					return &m.second;
				}
			}

			return nullptr;
		}
	};

	class JsonReader
	{
	public:
		JsonReader(char const *text, size_t length) : at(text), end(text + length) {}

		bool Read(Json *out)
		{
			if (!Value(out, 0))
			{
				return false;
			}

			Space();
			return at == end;
		}

	private:
		char const *at;
		char const *end;
		size_t values = 0;
		size_t textBytes = 0;

		void Space()
		{
			while (at < end && (*at == ' ' || *at == '\t' || *at == '\r' || *at == '\n'))
			{
				at++;
			}
		}

		bool Word(char const *word)
		{
			size_t const n = strlen(word);

			if ((size_t)(end - at) < n || memcmp(at, word, n) != 0)
			{
				return false;
			}

			at += n;
			return true;
		}

		bool Text(std::string *out)
		{
			if (at >= end || *at != '"')
			{
				return false;
			}

			at++;

			while (at < end && *at != '"')
			{
				// plain printable letters only: no escapes are needed by the animation's names
				if ((unsigned char)*at < 0x20 || (unsigned char)*at > 0x7e || *at == '\\' || out->size() >= 256 || ++textBytes > kMostTextBytes)
				{
					return false;
				}

				out->push_back(*at++);
			}

			if (at >= end)
			{
				return false;
			}

			at++;
			return true;
		}

		bool Number(double *out)
		{
			char buffer[64];
			size_t n = 0;

			while (at < end && n < sizeof(buffer) - 1 && strchr("+-0123456789.eE", *at))
			{
				buffer[n++] = *at++;
			}

			buffer[n] = 0;

			if (n == 0 || n >= sizeof(buffer) - 1)
			{
				return false;
			}

			char *stop = nullptr;
			double const value = strtod(buffer, &stop);

			if (!stop || *stop || !(value > -1e12 && value < 1e12))
			{
				return false;
			}

			*out = value;
			return true;
		}

		bool Value(Json *out, int depth)
		{
			if (depth > 16 || ++values > kMostValues)
			{
				return false;
			}

			Space();

			if (at >= end)
			{
				return false;
			}

			if (*at == '{')
			{
				out->kind = Json::Object;
				at++;
				Space();

				if (at < end && *at == '}')
				{
					at++;
					return true;
				}

				for (;;)
				{
					std::string key;
					Json value;

					Space();

					if (!Text(&key))
					{
						return false;
					}

					Space();

					if (at >= end || *at != ':')
					{
						return false;
					}

					at++;

					if (!Value(&value, depth + 1))
					{
						return false;
					}

					out->members.emplace_back(key, std::move(value));
					Space();

					if (at < end && *at == ',')
					{
						at++;
						continue;
					}

					if (at < end && *at == '}')
					{
						at++;
						return true;
					}

					return false;
				}
			}

			if (*at == '[')
			{
				out->kind = Json::Array;
				at++;
				Space();

				if (at < end && *at == ']')
				{
					at++;
					return true;
				}

				for (;;)
				{
					Json value;

					if (!Value(&value, depth + 1))
					{
						return false;
					}

					out->items.push_back(std::move(value));
					Space();

					if (at < end && *at == ',')
					{
						at++;
						continue;
					}

					if (at < end && *at == ']')
					{
						at++;
						return true;
					}

					return false;
				}
			}

			if (*at == '"')
			{
				out->kind = Json::Text;
				return Text(&out->text);
			}

			if (Word("true") || Word("false"))
			{
				out->kind = Json::Bool;
				out->flag = at[-1] == 'e' && at[-2] == 'u';
				return true;
			}

			if (Word("null"))
			{
				out->kind = Json::Null;
				return true;
			}

			out->kind = Json::Number;
			return Number(&out->number);
		}
	};

	// ---------------------------------------------------------------- the animation

	struct Key
	{
		double place[3];   // the part's origin in the level, the file's units
		double turn[4];    // its turn, x y z w, as the file holds one
	};

	struct Part
	{
		std::string name;
		double jointRoot[3], jointEnd[3], boxMin[3], boxMax[3];
		std::vector<Key> keys, dense;
	};

	struct Anim
	{
		int fps = 0, denseFps = 0;
		double seconds = 0.0;
		double player[3] = { 0, 0, 0 };
		double playerRadius = 0.0;
		std::vector<Part> parts;
	};

	bool Numbers(Json const *json, size_t count, double *out, double least, double most)
	{
		if (!json || json->kind != Json::Array || json->items.size() != count)
		{
			return false;
		}

		for (size_t i = 0; i < count; i++)
		{
			if (json->items[i].kind != Json::Number || !(json->items[i].number >= least && json->items[i].number <= most))
			{
				return false;
			}

			out[i] = json->items[i].number;
		}

		return true;
	}

	bool Keys(Json const *json, size_t count, std::vector<Key> *out)
	{
		if (!json || json->kind != Json::Array || json->items.size() != count)
		{
			return false;
		}

		for (Json const &item : json->items)
		{
			double v[7];
			Key key;

			if (!Numbers(&item, 7, v, -kMostPlace, kMostPlace))
			{
				return false;
			}

			double const length = sqrt(v[3] * v[3] + v[4] * v[4] + v[5] * v[5] + v[6] * v[6]);

			if (!(length > 0.99 && length < 1.01))
			{
				return false;   // a turn must be of length 1 (to the float's rounding)
			}

			for (int i = 0; i < 3; i++)
			{
				key.place[i] = v[i];
			}

			for (int i = 0; i < 4; i++)
			{
				key.turn[i] = v[3 + i] / length;
			}

			out->push_back(key);
		}

		return true;
	}

	bool ReadAnim(char const *path, Anim *anim, std::string *why)
	{
		HANDLE const file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		LARGE_INTEGER size;

		if (file == INVALID_HANDLE_VALUE)
		{
			*why = "the animation file can't be opened";
			return false;
		}

		std::vector<char> bytes;
		DWORD got = 0;
		bool const read = GetFileSizeEx(file, &size) && size.QuadPart > 0 && size.QuadPart <= 64 * 1024 * 1024 &&
			(bytes.resize((size_t)size.QuadPart), ReadFile(file, bytes.data(), (DWORD)bytes.size(), &got, NULL)) && got == bytes.size();

		CloseHandle(file);

		if (!read)
		{
			*why = "the animation file can't be read whole (or is over 64 MiB)";
			return false;
		}

		Json root;

		if (!JsonReader(bytes.data(), bytes.size()).Read(&root) || root.kind != Json::Object)
		{
			*why = "the animation file is not JSON this reader takes (plain names, numbers, depth 16, at most 4 million values and 1 MiB of names)";
			return false;
		}

		double fps = 0, seconds = 0, denseFps = 0, radius = 0;

		if (!root.Get("fps") || root.Get("fps")->kind != Json::Number || !root.Get("seconds") || root.Get("seconds")->kind != Json::Number || !root.Get("dense_fps") ||
			root.Get("dense_fps")->kind != Json::Number)
		{
			*why = "fps, seconds or dense_fps is missing";
			return false;
		}

		fps = root.Get("fps")->number;
		seconds = root.Get("seconds")->number;
		denseFps = root.Get("dense_fps")->number;

		if (fps != floor(fps) || fps < 1 || fps > 120 || denseFps != floor(denseFps) || denseFps < 1 || denseFps > 240 || !(seconds >= 0.1 && seconds <= 3600.0) ||
			fps * seconds != floor(fps * seconds) || denseFps * seconds != floor(denseFps * seconds))
		{
			*why = "fps (1-120) and dense_fps (1-240) must be whole, seconds 0.1-3600, and each times seconds a whole number of keys";
			return false;
		}

		Json const *const player = root.Get("player");

		if (!player || player->kind != Json::Object || !Numbers(player->Get("place"), 3, anim->player, -kMostPlace, kMostPlace) || !player->Get("radius") ||
			player->Get("radius")->kind != Json::Number || !((radius = player->Get("radius")->number) > 0 && radius < 100000))
		{
			*why = "player { place [x y z], radius } is missing or out of bounds";
			return false;
		}

		anim->fps = (int)fps;
		anim->denseFps = (int)denseFps;
		anim->seconds = seconds;
		anim->playerRadius = radius;

		Json const *const parts = root.Get("parts");

		if (!parts || parts->kind != Json::Array || parts->items.empty() || parts->items.size() > 1000)
		{
			*why = "parts must be a list of 1 to 1000";
			return false;
		}

		size_t const keyCount = (size_t)(fps * seconds) + 1, denseCount = (size_t)(denseFps * seconds);

		for (Json const &item : parts->items)
		{
			Part part;
			Json const *const name = item.Get("name");

			if (!name || name->kind != Json::Text || name->text.empty() || name->text.size() > 63 || !Numbers(item.Get("joint_root"), 3, part.jointRoot, -100000, 100000) ||
				!Numbers(item.Get("joint_end"), 3, part.jointEnd, -100000, 100000) || !Numbers(item.Get("box_min"), 3, part.boxMin, -100000, 100000) ||
				!Numbers(item.Get("box_max"), 3, part.boxMax, -100000, 100000) || !Keys(item.Get("keys"), keyCount, &part.keys) || !Keys(item.Get("dense"), denseCount, &part.dense))
			{
				*why = "a part needs name (1-63 letters), joint_root, joint_end, box_min, box_max and " + std::to_string(keyCount) + " keys and " + std::to_string(denseCount) +
					" dense poses of 7 numbers (a turn of length 1)";
				return false;
			}

			part.name = name->text;
			anim->parts.push_back(part);
		}

		return true;
	}

	// ---------------------------------------------------------------- the level

	bool PlaceNumber(char const *text, int32_t *out)
	{
		char *end = nullptr;
		long const value = strtol(text, &end, 10);

		if (!text[0] || !end || *end || value <= -16777216L || value >= 16777216L)
		{
			return false;
		}

		*out = (int32_t)value;
		return true;
	}

	// The level's scale: its environment's ENVSDSCL, 1 when it has none (projload.cpp:363-369: GlobalScale times that chunk's scale)
	double LevelScale(OpenLevelFile &open)
	{
		Chunk_With_Children *const envd = open.Level() ? (Chunk_With_Children *)open.Level()->lookup_single_child("REBENVDT") : nullptr;
		Environment_Scale_Chunk const *const scaleChunk = envd ? dynamic_cast<Environment_Scale_Chunk const *>(envd->lookup_single_child("ENVSDSCL")) : nullptr;
		double const scale = scaleChunk ? scaleChunk->scale : 1.0;

		return scale > 0.001 && scale < 1000.0 ? scale : 1.0;
	}

	// The 8-byte ID of the first object of exactly that name (modules and placed objects, File_Chunk::list_objects's order); false for none
	bool IdOf(OpenLevelFile &open, char const *name, uint8_t id[8])
	{
		List<Object_Chunk *> objects;

		open.Level()->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (object && object->object_data.o_name && strcmp(object->object_data.o_name, name) == 0)
			{
				uint8_t const *const bytes = (uint8_t const *)&object->object_data.ID;

				for (int k = 0; k < 8; k++)
				{
					id[k] = bytes[k];
				}

				return true;
			}
		}

		return false;
	}

	bool OpenLevel(OpenLevelFile &open, char const *path)
	{
		if (open.Open(path) != LevelOpenResult::Opened || !open.Level())
		{
			printf("track-import: FAILED: the game's loader does not take %s\n", path);
			return false;
		}

		return true;
	}

	bool Save(OpenLevelFile &open, char const *path)
	{
		if (open.SaveAs(path) != LevelSaveResult::Saved)
		{
			printf("track-import: FAILED: %s could not be written\n", path);
			return false;
		}

		OpenLevelFile back;

		if (back.Open(path) != LevelOpenResult::Opened || !back.Level())
		{
			printf("track-import: FAILED: the game's loader does not read back %s\n", path);
			return false;
		}

		printf("track-import: wrote %s; the game's loader reads it back\n", path);
		return true;
	}

	int32_t Whole(double value)
	{
		return (int32_t)(value >= 0.0 ? floor(value + 0.5) : -floor(-value + 0.5));
	}

	// the file's turn as a rotation (R p: how a shape's point is drawn, surface_place.h)
	void TurnPoint(double const q[4], double const p[3], double out[3])
	{
		double const x = q[0], y = q[1], z = q[2], w = q[3];
		double const r[3][3] = {
			{ 1 - 2 * (y * y + z * z), 2 * (x * y - z * w), 2 * (x * z + y * w) },
			{ 2 * (x * y + z * w), 1 - 2 * (x * x + z * z), 2 * (y * z - x * w) },
			{ 2 * (x * z - y * w), 2 * (y * z + x * w), 1 - 2 * (x * x + y * y) },
		};

		for (int i = 0; i < 3; i++)
		{
			out[i] = r[i][0] * p[0] + r[i][1] * p[1] + r[i][2] * p[2];
		}
	}

	// ---------------------------------------------------------------- probe

	int Probe(int argc, char **argv)
	{
		int32_t at[3];

		if (argc != 6 || !PlaceNumber(argv[3], &at[0]) || !PlaceNumber(argv[4], &at[1]) || !PlaceNumber(argv[5], &at[2]))
		{
			printf("track-import: usage: track-import probe <level.rif> <x> <y> <z> (whole numbers of the file's units)\n");
			return 2;
		}

		OpenLevelFile open;

		if (!OpenLevel(open, argv[2]))
		{
			return 1;
		}

		double const scale = LevelScale(open);
		double const units = 100.0 * scale;   // inteng's memory units per file unit (autolink_scene.h)
		host::AutoLinkScene scene;

		host::AutoLinkScene_Build(&open, scale, &scene);

		double const x = at[0] * units, y = at[1] * units, z = at[2] * units;
		double floorY = 1e300, ceilingY = -1e300;
		std::string floorName = "none", ceilingName = "none";

		for (host::AutoLinkModule const &one : scene.list)
		{
			host::PortalShape const *const shape = one.module.shape;

			if (!shape)
			{
				continue;
			}

			for (size_t p = 0; p < shape->polygons.size(); p++)
			{
				std::vector<int32_t> const &corners = shape->polygons[p];

				if (corners.size() < 3 || shape->normals.size() < 3 * (p + 1))
				{
					continue;
				}

				// the corners in the world (memory units): the module's place plus its shape's points (modules are not turned)
				std::vector<double> cx, cy, cz;
				bool good = true;

				for (int32_t const c : corners)
				{
					if (c < 0 || 3 * (size_t)c + 2 >= shape->points.size())
					{
						good = false;
						break;
					}

					cx.push_back((double)one.module.place[0] + shape->points[3 * (size_t)c]);
					cy.push_back((double)one.module.place[1] + shape->points[3 * (size_t)c + 1]);
					cz.push_back((double)one.module.place[2] + shape->points[3 * (size_t)c + 2]);
				}

				if (!good)
				{
					continue;
				}

				// (x, z) inside the polygon seen from above: the crossings of a line along +x (even-odd, so either winding)
				bool inside = false;

				for (size_t i = 0, j = cx.size() - 1; i < cx.size(); j = i++)
				{
					if ((cz[i] > z) != (cz[j] > z) && x < (cx[j] - cx[i]) * (z - cz[i]) / (cz[j] - cz[i]) + cx[i])
					{
						inside = !inside;
					}
				}

				double const nx = shape->normals[3 * p] / 65536.0, ny = shape->normals[3 * p + 1] / 65536.0, nz = shape->normals[3 * p + 2] / 65536.0;

				if (!inside || fabs(ny) < 0.05)
				{
					continue;   // not over it, or a wall
				}

				double const hitY = cy[0] - (nx * (x - cx[0]) + nz * (z - cz[0])) / ny;
				std::string const name = scene.names.count(one.number) ? scene.names.at(one.number) : std::to_string(one.number);

				if (hitY > y && hitY < floorY)
				{
					floorY = hitY;
					floorName = name;
				}

				if (hitY < y && hitY > ceilingY)
				{
					ceilingY = hitY;
					ceilingName = name;
				}
			}
		}

		printf("track-import: probe at %d %d %d (the level's scale %.4f): floor %s (module %s), ceiling %s (module %s)\n", at[0], at[1], at[2], scale,
			floorY < 1e299 ? std::to_string((long long)floor(floorY / units + 0.5)).c_str() : "none", floorName.c_str(),
			ceilingY > -1e299 ? std::to_string((long long)floor(ceilingY / units + 0.5)).c_str() : "none", ceilingName.c_str());
		return floorY < 1e299 ? 0 : 1;
	}

	// ---------------------------------------------------------------- parts

	int Parts(int argc, char **argv)
	{
		if (argc < 7 || (argc - 4) % 3 != 0)
		{
			printf("track-import: usage: track-import parts <level.rif> <out.rif> <source> <scale> <name> [...]\n");
			return 2;
		}

		OpenLevelFile open;

		if (!OpenLevel(open, argv[2]))
		{
			return 1;
		}

		for (int a = 4; a + 2 < argc; a += 3)
		{
			char const *const source = argv[a];
			char const *const name = argv[a + 2];
			char *stop = nullptr;
			double const scale = strtod(argv[a + 1], &stop);
			uint8_t sourceId[8], newId[8], already[8];
			int32_t place[3];
			float turn[4];
			std::string made, why;

			if (!stop || *stop || !(scale > 0.01 && scale <= 100.0))
			{
				printf("track-import: FAILED: the scale %s is not a number from 0.01 to 100\n", argv[a + 1]);
				return 2;
			}

			if (!IdOf(open, source, sourceId) || !host::TrackObjectPlace(&open, sourceId, place, turn))
			{
				printf("track-import: FAILED: the level has no object named %s\n", source);
				return 1;
			}

			if (IdOf(open, name, already))
			{
				printf("track-import: FAILED: the level already has an object named %s\n", name);
				return 1;
			}

			host::PolySplitResult const copied = host::PolyEdit_CopyShapeAsObject(&open, sourceId, name, place, turn, newId, &made, &why);

			if (copied != host::PolySplitResult::Done)
			{
				printf("track-import: FAILED: %s's shape could not be copied for %s: %s (%s)\n", source, name, host::PolySplitWords(copied), why.c_str());
				return 1;
			}

			host::ShapeRescaleResult const sized = host::ShapeRescale_ByFactor(&open, open.Level(), newId, scale, &why);

			if (sized != host::ShapeRescaleResult::Done)
			{
				printf("track-import: FAILED: %s could not be sized by %g: %s (%s)\n", name, scale, host::ShapeRescaleWords(sized), why.c_str());
				return 1;
			}

			model::ShapeModel shape;

			if (host::ShapeEdit_Read(open.Level(), &open, newId, &shape, nullptr, &why) != host::ShapeEditResult::Done)
			{
				printf("track-import: FAILED: %s's new shape can't be read back: %s\n", name, why.c_str());
				return 1;
			}

			printf("track-import: part %s, a copy of %s's shape (number %d) times %g: box %d %d %d .. %d %d %d\n", made.c_str(), source, shape.number, scale, shape.extents.minX,
				shape.extents.minY, shape.extents.minZ, shape.extents.maxX, shape.extents.maxY, shape.extents.maxZ);
		}

		return Save(open, argv[3]) ? 0 : 1;
	}

	// ---------------------------------------------------------------- import

	int Import(int argc, char **argv)
	{
		bool const smooth = argc == 6 && strcmp(argv[5], "smooth") == 0;

		if ((argc != 5 && argc != 6) || (argc == 6 && !smooth && strcmp(argv[5], "linear") != 0))
		{
			printf("track-import: usage: track-import import <level.rif> <anim.json> <out.rif> [linear|smooth]\n");
			return 2;
		}

		Anim anim;
		std::string why;

		if (!ReadAnim(argv[3], &anim, &why))
		{
			printf("track-import: FAILED: %s: %s\n", argv[3], why.c_str());
			return 1;
		}

		size_t const sections = anim.parts[0].keys.size() - 1;

		if (sections > kMostSectionsPerTrack || sections * kSectionBytes * anim.parts.size() > kMostImportedBytes || sections < 1)
		{
			printf("track-import: FAILED: %zu sections a track and %zu tracks: the game takes at most %zu sections a track (one 1 MiB pool block) and this import keeps all its tracks "
				"under %zu bytes of the level's pool\n", sections, anim.parts.size(), kMostSectionsPerTrack, kMostImportedBytes);
			return 1;
		}

		OpenLevelFile open;

		if (!OpenLevel(open, argv[2]))
		{
			return 1;
		}

		double const scale = LevelScale(open);

		// the sections' times: the loop is exactly seconds x 65536, each key at the whole number nearest its own time (no drift, loop after loop)
		std::vector<int32_t> times(sections);
		double const loop = anim.seconds * 65536.0;

		for (size_t k = 0; k < sections; k++)
		{
			times[k] = Whole(loop * (double)(k + 1) / (double)sections) - Whole(loop * (double)k / (double)sections);
		}

		for (Part const &part : anim.parts)
		{
			uint8_t id[8];

			if (!IdOf(open, part.name.c_str(), id))
			{
				printf("track-import: FAILED: the level has no object named %.*s\n", (int)part.name.size(), part.name.c_str());
				return 1;
			}

			host::ObjectEditResult const assigned = host::AssignStrategy(&open, id, 72);

			if (assigned != host::ObjectEditResult::Done)
			{
				printf("track-import: FAILED: %.*s could not be given the Track strategy (kind 72)\n", (int)part.name.size(), part.name.c_str());
				return 1;
			}

			host::TrackLoaded loaded;

			loaded.track.flags = 1u | 2u | (smooth ? 8u : 0u);   // Loop, Playing at start (all start on the level's first frame: one clock), Smoothing
			loaded.track.timerStart = 0;

			int32_t const offset[3] = { -Whole(part.jointRoot[0]), -Whole(part.jointRoot[1]), -Whole(part.jointRoot[2]) };

			for (size_t k = 0; k < sections; k++)
			{
				host::TrackSection section = {};
				Key const &a = part.keys[k];
				Key const &b = part.keys[k + 1];

				for (int i = 0; i < 4; i++)
				{
					section.quatStart[i] = (float)a.turn[i];
					section.quatEnd[i] = (float)b.turn[i];
				}

				for (int i = 0; i < 3; i++)
				{
					if (smooth)
					{
						// the spline's points are the origin's places; the offset is not used by the game then
						section.pivotStart[i] = Whole(a.place[i]);
						section.pivotEnd[i] = Whole(b.place[i]);
						section.offset[i] = 0;
					}
					else
					{
						section.offset[i] = offset[i];
					}
				}

				if (!smooth)
				{
					// the pivot at each end is the ROOT JOINT, worked out with the GAME's own turn of the offset (the place the game gives is pivot + RotateVector(offset)),
					// so the place it gives at each key is the key's origin exactly as the game turns it
					for (int end = 0; end < 2; end++)
					{
						Key const &key = end ? b : a;
						float turn[4] = { (float)key.turn[0], (float)key.turn[1], (float)key.turn[2], (float)key.turn[3] };
						int32_t matrix[9], turned[3];
						int32_t const scaledOffset[3] = { (int32_t)(offset[0] * scale), (int32_t)(offset[1] * scale), (int32_t)(offset[2] * scale) };

						GameTrack_MatrixOfFileTurn(turn, matrix);
						GameTrack_Rotate(matrix, scaledOffset, turned);

						for (int i = 0; i < 3; i++)
						{
							(end ? section.pivotEnd : section.pivotStart)[i] = Whole(key.place[i] - turned[i] / scale);
						}
					}
				}

				section.time = times[k];
				loaded.track.sections.push_back(section);
			}

			int32_t place[3] = { Whole(part.keys[0].place[0]), Whole(part.keys[0].place[1]), Whole(part.keys[0].place[2]) };
			float turn[4] = { (float)part.keys[0].turn[0], (float)part.keys[0].turn[1], (float)part.keys[0].turn[2], (float)part.keys[0].turn[3] };

			if (host::TrackSetObjectPose(&open, id, place, turn) != host::TrackEditResult::Done || host::TrackStore(&open, id, loaded) != host::TrackEditResult::Done)
			{
				printf("track-import: FAILED: %.*s's track was refused by the game's loader\n", (int)part.name.size(), part.name.c_str());
				return 1;
			}

			printf("track-import: %.*s: a track object, %zu sections (%s), looping, playing at the start\n", (int)part.name.size(), part.name.c_str(), sections, smooth ? "smooth: pivots on its origin" :
				"linear: pivots on its root joint");
		}

		return Save(open, argv[4]) ? 0 : 1;
	}

	// ---------------------------------------------------------------- check

	struct Played
	{
		std::vector<GameTrack *> tracks;
		std::vector<Part const *> parts;

		~Played()
		{
			for (GameTrack *t : tracks)
			{
				GameTrack_Free(t);
			}
		}
	};

	bool PlayAll(OpenLevelFile &open, Anim const &anim, double scale, Played *played)
	{
		for (Part const &part : anim.parts)
		{
			uint8_t id[8];
			host::TrackLoaded loaded;

			if (!IdOf(open, part.name.c_str(), id) || host::TrackLoad(&open, id, &loaded) != host::TrackEditResult::Done || !loaded.had)
			{
				printf("track-import: FAILED: %.*s is not in the level with a track\n", (int)part.name.size(), part.name.c_str());
				return false;
			}

			GameTrack *const track = GameTrack_Make(loaded.track, scale);

			if (!track)
			{
				printf("track-import: FAILED: the game makes no controller of %.*s's track\n", (int)part.name.size(), part.name.c_str());
				return false;
			}

			played->tracks.push_back(track);
			played->parts.push_back(&part);
		}

		return true;
	}

	// a point of the part's own frame where the game holds it now (the file's units)
	void GamePoint(GameTrackPose const &pose, double const local[3], double scale, double out[3])
	{
		int32_t const in[3] = { Whole(local[0] * scale), Whole(local[1] * scale), Whole(local[2] * scale) };
		int32_t turned[3];

		GameTrack_Rotate(pose.matrix, in, turned);

		for (int i = 0; i < 3; i++)
		{
			out[i] = (pose.position[i] + turned[i]) / scale;
		}
	}

	// the same point where the baked pose holds it (the file's turn: R p)
	void TruthPoint(Key const &key, double const local[3], double out[3])
	{
		double turned[3];

		TurnPoint(key.turn, local, turned);

		for (int i = 0; i < 3; i++)
		{
			out[i] = key.place[i] + turned[i];
		}
	}

	double Distance(double const a[3], double const b[3])
	{
		return sqrt((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2]));
	}

	struct Measures
	{
		double jointGap = 0, jointGapAt = 0;          // the most two parts' shared joint is apart
		double fromTruth = 0, fromTruthAt = 0;        // the most a part's joint is from the baked pose
		double seamJump = 0, elsewhereJump = 0;       // the hand's step against the baked step, at the seam and elsewhere
		double nearestPlayer = 1e300;                 // the nearest any corner of any part comes to the player's upright line
		double lastLoopFromTruth = 0;
	};

	// Plays every part together for `loops` loops at the dense rate (each frame's time the whole number nearest its own, as the game's 16.16 clock), measuring as it goes
	bool Measure(OpenLevelFile &open, Anim const &anim, double scale, int loops, Measures *m)
	{
		Played played;

		if (!PlayAll(open, anim, scale, &played))
		{
			return false;
		}

		size_t const perLoop = anim.parts[0].dense.size();
		size_t const frames = perLoop * (size_t)loops;
		double const loopTime = anim.seconds * 65536.0;
		std::vector<GameTrackPose> poses(played.tracks.size());
		std::vector<double> lastHand(3, 0.0), lastTruthHand(3, 0.0);
		int64_t clock = 0;

		for (size_t f = 1; f <= frames; f++)
		{
			int64_t const next = (int64_t)floor(loopTime * (double)f / (double)perLoop + 0.5);
			int32_t const step = (int32_t)(next - clock);

			clock = next;

			for (size_t i = 0; i < played.tracks.size(); i++)
			{
				GameTrack_Step(played.tracks[i], step, &poses[i]);
			}

			size_t const d = f % perLoop;   // the baked pose at this moment of the loop
			double hand[3] = { 0, 0, 0 }, truthHand[3] = { 0, 0, 0 };

			for (size_t i = 0; i < played.tracks.size(); i++)
			{
				Part const &part = *played.parts[i];
				double root[3], end[3], truthRoot[3], truthEnd[3];

				GamePoint(poses[i], part.jointRoot, scale, root);
				GamePoint(poses[i], part.jointEnd, scale, end);
				TruthPoint(part.dense[d], part.jointRoot, truthRoot);
				TruthPoint(part.dense[d], part.jointEnd, truthEnd);

				double const off = Distance(root, truthRoot) > Distance(end, truthEnd) ? Distance(root, truthRoot) : Distance(end, truthEnd);

				if (off > m->fromTruth)
				{
					m->fromTruth = off;
					m->fromTruthAt = (double)f / anim.denseFps;
				}

				if (f > frames - perLoop && off > m->lastLoopFromTruth)
				{
					m->lastLoopFromTruth = off;
				}

				if (i + 1 < played.tracks.size())
				{
					double nextRoot[3];

					GamePoint(poses[i + 1], played.parts[i + 1]->jointRoot, scale, nextRoot);

					double const gap = Distance(end, nextRoot);

					if (gap > m->jointGap)
					{
						m->jointGap = gap;
						m->jointGapAt = (double)f / anim.denseFps;
					}
				}
				else
				{
					memcpy(hand, end, sizeof(hand));
					memcpy(truthHand, truthEnd, sizeof(truthHand));
				}

				// the part's box corners against the player's upright line (the collision shape is a cylinder about it: CE_MARINE's radius)
				for (int c = 0; c < 8; c++)
				{
					double const local[3] = { (c & 1) ? part.boxMax[0] : part.boxMin[0], (c & 2) ? part.boxMax[1] : part.boxMin[1], (c & 4) ? part.boxMax[2] : part.boxMin[2] };
					double corner[3];

					GamePoint(poses[i], local, scale, corner);

					double const across = sqrt((corner[0] - anim.player[0]) * (corner[0] - anim.player[0]) + (corner[2] - anim.player[2]) * (corner[2] - anim.player[2]));

					m->nearestPlayer = across < m->nearestPlayer ? across : m->nearestPlayer;
				}
			}

			if (f > 1)
			{
				double step3[3], truthStep[3];

				for (int i = 0; i < 3; i++)
				{
					step3[i] = hand[i] - lastHand[i];
					truthStep[i] = truthHand[i] - lastTruthHand[i];
				}

				double const zero[3] = { 0, 0, 0 };
				double diff[3] = { step3[0] - truthStep[0], step3[1] - truthStep[1], step3[2] - truthStep[2] };
				double const jump = Distance(diff, zero);
				size_t const inLoop = f % perLoop;
				bool const atSeam = inLoop <= 2 || inLoop >= perLoop - 2;

				if (atSeam)
				{
					m->seamJump = jump > m->seamJump ? jump : m->seamJump;
				}
				else
				{
					m->elsewhereJump = jump > m->elsewhereJump ? jump : m->elsewhereJump;
				}
			}

			memcpy(lastHand.data(), hand, sizeof(hand));
			memcpy(lastTruthHand.data(), truthHand, sizeof(truthHand));
		}

		return true;
	}

	int Check(int argc, char **argv)
	{
		int loops = 100;

		if (argc != 4 && argc != 5)
		{
			printf("track-import: usage: track-import check <level.rif> <anim.json> [loops: 1, 10, 100, 1000 or 10000]\n");
			return 2;
		}

		if (argc == 5)
		{
			// the count is one of these, matched as text: no number typed on the command line ever reaches the check's arithmetic
			static char const *const kLoopWords[] = { "1", "10", "100", "1000", "10000" };
			static int const kLoopCounts[] = { 1, 10, 100, 1000, 10000 };
			bool known = false;

			for (size_t k = 0; k < sizeof(kLoopCounts) / sizeof(kLoopCounts[0]); k++)
			{
				if (strcmp(argv[4], kLoopWords[k]) == 0)
				{
					loops = kLoopCounts[k];
					known = true;
				}
			}

			if (!known)
			{
				printf("track-import: FAILED: loops must be 1, 10, 100, 1000 or 10000\n");
				return 2;
			}
		}

		Anim anim;
		std::string why;

		if (!ReadAnim(argv[3], &anim, &why))
		{
			printf("track-import: FAILED: %s: %s\n", argv[3], why.c_str());
			return 1;
		}

		OpenLevelFile open;

		if (!OpenLevel(open, argv[2]))
		{
			return 1;
		}

		double const scale = LevelScale(open);

		// the game's turn of a vector against the file's (R p): the same, or the track's offsets and the drawn shapes disagree
		{
			Key const &key = anim.parts[0].keys[1];
			float turn[4] = { (float)key.turn[0], (float)key.turn[1], (float)key.turn[2], (float)key.turn[3] };
			int32_t matrix[9], turned[3];
			int32_t const v[3] = { 10000, 20000, 30000 };
			double const vd[3] = { 10000, 20000, 30000 };
			double expect[3];

			GameTrack_MatrixOfFileTurn(turn, matrix);
			GameTrack_Rotate(matrix, v, turned);
			TurnPoint(key.turn, vd, expect);

			double const got[3] = { (double)turned[0], (double)turned[1], (double)turned[2] };

			printf("track-import: the game's RotateVector by the loader's matrix against the file's turn R p: %.1f apart on a vector of 37417 (game %d %d %d, file %.0f %.0f %.0f)\n",
				Distance(got, expect), turned[0], turned[1], turned[2], expect[0], expect[1], expect[2]);
		}

		Measures m;

		if (!Measure(open, anim, scale, loops, &m))
		{
			return 1;
		}

		// AN UNEVEN FRAME RATE (the game's NormalFrameTime is whatever the last frame took): 24 and 90 frames a second taken in turn for as long, every part stepped by the same
		// time each frame: the shared joints must hold as well as at a steady rate (the parts keep one clock)
		double unevenGap = 0.0;
		{
			Played played;

			if (!PlayAll(open, anim, scale, &played))
			{
				return 1;
			}

			std::vector<GameTrackPose> poses(played.tracks.size());
			int64_t const total = (int64_t)floor(anim.seconds * 65536.0 * loops + 0.5);
			int64_t clock = 0;

			for (int f = 0; clock < total; f++)
			{
				int32_t const step = (f % 2) ? 65536 / 90 : 65536 / 24;

				clock += step;

				for (size_t i = 0; i < played.tracks.size(); i++)
				{
					GameTrack_Step(played.tracks[i], step, &poses[i]);
				}

				for (size_t i = 0; i + 1 < played.tracks.size(); i++)
				{
					double end[3], nextRoot[3];

					GamePoint(poses[i], played.parts[i]->jointEnd, scale, end);
					GamePoint(poses[i + 1], played.parts[i + 1]->jointRoot, scale, nextRoot);
					unevenGap = Distance(end, nextRoot) > unevenGap ? Distance(end, nextRoot) : unevenGap;
				}
			}
		}

		printf("track-import: %d loops at %d frames a second, every part played by the game's own track code, all stepped by the same NormalFrameTime:\n", loops, anim.denseFps);
		printf("track-import:   the most two parts' shared joint comes apart: %.2f (at %.3f s); at 24 and 90 frames a second in turn: %.2f\n", m.jointGap, m.jointGapAt, unevenGap);
		printf("track-import:   the most a part's joint is from the baked pose: %.2f (at %.3f s); in the last loop: %.2f\n", m.fromTruth, m.fromTruthAt, m.lastLoopFromTruth);
		printf("track-import:   the hand's step against the baked step: %.2f at the loop's seam, %.2f anywhere else\n", m.seamJump, m.elsewhereJump);
		printf("track-import:   the nearest any part's box comes to the player's upright line at the start: %.0f (the marine's collision radius %.0f)\n", m.nearestPlayer, anim.playerRadius);
		return 0;
	}

	// ---------------------------------------------------------------- posecopy

	// the file's turn (x y z w) of a game OrientMat: the quaternion whose loader matrix (QuatToMat of (-x, -y, -z, w)) is that matrix, tried both ways round and kept when it gives it back
	bool TurnOfMatrix(int32_t const m[9], float out[4])
	{
		double const a[3][3] = { { m[0] / 65536.0, m[1] / 65536.0, m[2] / 65536.0 }, { m[3] / 65536.0, m[4] / 65536.0, m[5] / 65536.0 }, { m[6] / 65536.0, m[7] / 65536.0, m[8] / 65536.0 } };

		for (int transposed = 0; transposed < 2; transposed++)
		{
			double r[3][3];

			for (int i = 0; i < 3; i++)
			{
				for (int j = 0; j < 3; j++)
				{
					r[i][j] = transposed ? a[j][i] : a[i][j];
				}
			}

			// the standard matrix-to-quaternion (r = R(q), q = x y z w)
			double q[4];
			double const trace = r[0][0] + r[1][1] + r[2][2];

			if (trace > 0)
			{
				double const s = sqrt(trace + 1.0) * 2;
				q[3] = 0.25 * s;
				q[0] = (r[2][1] - r[1][2]) / s;
				q[1] = (r[0][2] - r[2][0]) / s;
				q[2] = (r[1][0] - r[0][1]) / s;
			}
			else if (r[0][0] > r[1][1] && r[0][0] > r[2][2])
			{
				double const s = sqrt(1.0 + r[0][0] - r[1][1] - r[2][2]) * 2;
				q[3] = (r[2][1] - r[1][2]) / s;
				q[0] = 0.25 * s;
				q[1] = (r[0][1] + r[1][0]) / s;
				q[2] = (r[0][2] + r[2][0]) / s;
			}
			else if (r[1][1] > r[2][2])
			{
				double const s = sqrt(1.0 + r[1][1] - r[0][0] - r[2][2]) * 2;
				q[3] = (r[0][2] - r[2][0]) / s;
				q[0] = (r[0][1] + r[1][0]) / s;
				q[1] = 0.25 * s;
				q[2] = (r[1][2] + r[2][1]) / s;
			}
			else
			{
				double const s = sqrt(1.0 + r[2][2] - r[0][0] - r[1][1]) * 2;
				q[3] = (r[1][0] - r[0][1]) / s;
				q[0] = (r[0][2] + r[2][0]) / s;
				q[1] = (r[1][2] + r[2][1]) / s;
				q[2] = 0.25 * s;
			}

			float const turn[4] = { (float)q[0], (float)q[1], (float)q[2], (float)q[3] };
			int32_t back[9];
			int32_t worst = 0;

			GameTrack_MatrixOfFileTurn(turn, back);

			for (int i = 0; i < 9; i++)
			{
				int32_t const d = back[i] > m[i] ? back[i] - m[i] : m[i] - back[i];
				worst = d > worst ? d : worst;
			}

			if (worst < 200)
			{
				memcpy(out, turn, sizeof(turn));
				return true;
			}
		}

		return false;
	}

	int PoseCopy(int argc, char **argv)
	{
		char *stop = nullptr;
		double const at = argc == 6 ? strtod(argv[4], &stop) : -1.0;

		if (argc != 6 || !stop || *stop || !(at >= 0.0 && at <= 3600.0))
		{
			printf("track-import: usage: track-import posecopy <level.rif> <anim.json> <seconds, 0-3600> <copy.rif>\n");
			return 2;
		}

		Anim anim;
		std::string why;

		if (!ReadAnim(argv[3], &anim, &why))
		{
			printf("track-import: FAILED: %s: %s\n", argv[3], why.c_str());
			return 1;
		}

		OpenLevelFile open;

		if (!OpenLevel(open, argv[2]))
		{
			return 1;
		}

		double const scale = LevelScale(open);
		Played played;

		if (!PlayAll(open, anim, scale, &played))
		{
			return 1;
		}

		// to that moment at the dense rate (the game's clock in whole 16.16 steps)
		std::vector<GameTrackPose> poses(played.tracks.size());
		int64_t const target = (int64_t)floor(at * 65536.0 + 0.5);
		int64_t const perStep = 65536 / anim.denseFps;
		int64_t clock = 0;

		for (size_t i = 0; i < played.tracks.size(); i++)
		{
			GameTrack_Pose(played.tracks[i], &poses[i]);
		}

		while (clock < target)
		{
			int32_t const step = (int32_t)(target - clock < perStep ? target - clock : perStep);

			clock += step;

			for (size_t i = 0; i < played.tracks.size(); i++)
			{
				GameTrack_Step(played.tracks[i], step, &poses[i]);
			}
		}

		for (size_t i = 0; i < played.tracks.size(); i++)
		{
			uint8_t id[8];
			int32_t place[3];
			float turn[4];

			for (int k = 0; k < 3; k++)
			{
				place[k] = Whole(poses[i].position[k] / scale);
			}

			if (!IdOf(open, played.parts[i]->name.c_str(), id) || !TurnOfMatrix(poses[i].matrix, turn) || host::TrackSetObjectPose(&open, id, place, turn) != host::TrackEditResult::Done)
			{
				printf("track-import: FAILED: %.*s's pose at %.3f s could not be written\n", (int)played.parts[i]->name.size(), played.parts[i]->name.c_str(), at);
				return 1;
			}

			// and its track's timer starts there: the editor (as the game) puts a track object where its track's timer start puts it, whatever its own place says
			host::TrackLoaded loaded;
			int64_t const loopTime = (int64_t)floor(anim.seconds * 65536.0 + 0.5);

			if (host::TrackLoad(&open, id, &loaded) != host::TrackEditResult::Done || !loaded.had)
			{
				printf("track-import: FAILED: %.*s's track can't be read\n", (int)played.parts[i]->name.size(), played.parts[i]->name.c_str());
				return 1;
			}

			loaded.track.timerStart = (int32_t)(target % loopTime);

			if (host::TrackStore(&open, id, loaded) != host::TrackEditResult::Done)
			{
				printf("track-import: FAILED: %.*s's track with its timer at %.3f s was refused by the game's loader\n", (int)played.parts[i]->name.size(), played.parts[i]->name.c_str(), at);
				return 1;
			}

			printf("track-import: %.*s at %.3f s: place %d %d %d, turn %.4f %.4f %.4f %.4f\n", (int)played.parts[i]->name.size(), played.parts[i]->name.c_str(), at, place[0], place[1], place[2], turn[0], turn[1], turn[2],
				turn[3]);
		}

		return Save(open, argv[5]) ? 0 : 1;
	}
}

static int Run(int argc, char **argv);

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);

	// a refusal, never a silent stop: whatever the reader's bounds miss, running out of memory is said and the files are left as they were (each command writes only at its end)
	try
	{
		return Run(argc, argv);
	}
	catch (std::bad_alloc const &)
	{
		printf("track-import: FAILED: out of memory (the input is too big for this 32-bit tool); nothing was written\n");
		return 1;
	}
}

static int Run(int argc, char **argv)
{
	if (argc >= 2)
	{
		if (strcmp(argv[1], "probe") == 0)
		{
			return Probe(argc, argv);
		}

		// resave <in.rif> <out.rif>: the level as the editor's save writes it, nothing changed (a stock level is packed, REBCRIF1; the save writes REBINFF2): the gate's "before"
		if (strcmp(argv[1], "resave") == 0 && argc == 4)
		{
			OpenLevelFile open;

			return OpenLevel(open, argv[2]) && Save(open, argv[3]) ? 0 : 1;
		}

		// image <in.rif> <out.bin>: the level's unpacked bytes (REBINFF2), as the editor builds them before its save packs them: what chunk-diff.cjs walks
		if (strcmp(argv[1], "image") == 0 && argc == 4)
		{
			OpenLevelFile open;
			std::vector<char> image;

			if (!OpenLevel(open, argv[2]) || !open.BuildLevel(image) || image.empty())
			{
				return 1;
			}

			FILE *out = nullptr;

			if (fopen_s(&out, argv[3], "wb") != 0 || !out)
			{
				printf("track-import: FAILED: %s could not be written\n", argv[3]);
				return 1;
			}

			bool const whole = fwrite(image.data(), 1, image.size(), out) == image.size();

			fclose(out);
			printf("track-import: %s's image, %zu bytes, written to %s\n", argv[2], image.size(), argv[3]);
			return whole ? 0 : 1;
		}

		if (strcmp(argv[1], "parts") == 0)
		{
			return Parts(argc, argv);
		}

		if (strcmp(argv[1], "import") == 0)
		{
			return Import(argc, argv);
		}

		if (strcmp(argv[1], "check") == 0)
		{
			return Check(argc, argv);
		}

		if (strcmp(argv[1], "posecopy") == 0)
		{
			return PoseCopy(argc, argv);
		}
	}

	printf("track-import: usage: track-import probe|parts|import|check|posecopy ... (see the top of track-import.cpp)\n");
	return 2;
}
