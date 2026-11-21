// verify_objects: the check of a scenario's saved copy for the objects it must have taken out or kept, read back through the game's own loader (as verify_strategy). Written 2026-11-17
// by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: verify_objects.exe <the copy it saved> gone|kept|track|notrack <an object's name> [gone|kept|track|notrack <name>] ...
//      ... tracksound <name>|<index>|<wav>|<inner>|<outer>|<volume>|<pitch>|<flags>   ... notracksound <name>|<index>   ... trackpivot <name>|<section>|<sx>|<sy>|<sz>|<ex>|<ey>|<ez>   ... at <name>|<x>|<y>|<z>   ... near <name>|<x>|<y>|<z>|<within>   ... up <name>|<ux>|<uy>|<uz>|<within>
//      ... touching <name>|<px>|<py>|<pz>|<nx>|<ny>|<nz>|<within> (the object's box meets the surface through that point: see CheckTouching)
// CHECKS: the copy is read by the game's loader; for "gone <name>" no object of the copy (module or placed object) has that exact name; for "kept <name>" one has; for
// "track <name>" the object of that name holds a track of its own (OBJTRAK2), for "notrack <name>" it holds none (added 2026-11-17 by Frost, OP-20 / OP-21).
// For "alt <name>=<count>,<group>" the object's alternate locations (its ALTLOCAT, read by alt_locations_edit.cpp's loader from the copy the game's loader took) are that many, in that group
// (added 2026-11-17 by Drake, OP-34..38).
// For "tracksound" the object of that name holds the track sound with that index (TRAKSOUN: 0 the sound, 1 the start sound, 2 the end sound) and it has exactly that wav name
// and those numbers, as the file holds them (the ranges in the file's units). Added 2026-11-17 by Vasquez (Track Editing T-1: the Sound Edit window's OK reaches the save).
// For "notracksound <name>|<index>" the object of that name exists and holds no track sound with that index (TB-T1: Cancel on a sound Edit made adds none).
// For "trackpivot <name>|<section>|<sx>|<sy>|<sz>|<ex>|<ey>|<ez>" the object's track has that section, its pivot from (sx sy sz) to (ex ey ez) (T-2: Shift+T moves a track onto its object).
// For "at <name>|<x>|<y>|<z>" the object's place is exactly (x y z).
// For "near <name>|<x>|<y>|<z>|<within>" each of the object's three numbers is at most <within> from (x y z), for a place the engine's dynamics chose: they step by the frame's own
// time (NormalFrameTime), so where a dropped object lands varies a little from run to run (added 2026-11-17 by Drake, OP-47: Gravity's landing place reaches the save).
// For "nostrategy <name>" the copy has a holder of that exact name (a module or placed object, a virtual object, an enemy or generator, or a placed hierarchy: the first in that order, as the
// strategy lookup finds them) and it holds no strategy: Delete Strategy took its AVPSTRAT and kept the holder (added 2026-11-17 by Bishop, S-39 on virtual objects and placed hierarchies).
// For "starts <count>" the copy holds that many player starts; for "startnear <n>|<x>|<y>|<z>|<within>" the n-th (from 1, the file's order) is within <within> of (x y z) (added 2026-11-18 by
// Drake, OP-29).
// For "modflags <name>|<mask>|<word>" (two hex numbers, 1 to 8 digits) the module of that exact name has a MODFLAGS (in its MODULEDT) whose flag word, ANDed with the mask, is the word: what Strategy
// Data's flag page sets (Airduct 80000000, Stair 40000000, Sky Visible 20000000, Fogged 10000000) reached the save (added 2026-11-18 by Bishop, strategy-data.md S-28 / S-29 / S-30).
// Gives back 0 only if every check passed.

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "alt_locations_edit.h"
#include "special_alt_edit.h"
#include "strategy_lookup.h"

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// what the game's chunk code calls
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
	printf("  the game's code says: %s\n", message ? message : "");
}

void avp_exit(int code)
{
	printf("  FAILED  the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

// The level's first object with exactly this name, or null
static Object_Chunk *ObjectNamed(OpenLevelFile &level, char const *name)
{
	List<Object_Chunk *> objects;

	level.Level()->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		Object_Chunk *const object = each();

		if (object && object->object_data.o_name && strcmp(object->object_data.o_name, name) == 0)
		{
			return object;
		}
	}

	return nullptr;
}

// Whether an object of the level has exactly this name
static bool HasObjectNamed(OpenLevelFile &level, char const *name)
{
	return ObjectNamed(level, name) != nullptr;
}

// A whole number of the check's words, and only that: false for an empty word, one with anything after the number, or one outside [least, most]
static bool WholeNumber(std::string const &word, long long least, long long most, long long *out)
{
	if (word.empty() || word.size() > 20)
	{
		return false;
	}

	char *end = nullptr;
	long long const value = strtoll(word.c_str(), &end, 10);

	if (!end || *end != '\0' || value < least || value > most)
	{
		return false;
	}

	*out = value;
	return true;
}

// The check's words split at each '|', letter by letter; it stops once there are more fields than `most` (so a check with too many is refused by its count)
static std::vector<std::string> BarFields(std::string const &asked, size_t most)
{
	std::vector<std::string> fields(1);

	for (char const letter : asked)
	{
		if (letter != '|')
		{
			fields.back() += letter;
		}
		else if (fields.size() > most)
		{
			break;
		}
		else
		{
			fields.emplace_back();
		}
	}

	return fields;
}

// The object's track sound (TRAKSOUN) with that index, or null (no object, or no such sound)
static Object_Track_Sound_Chunk *TrackSoundOf(Object_Chunk *object, int index)
{
	if (!object)
	{
		return nullptr;
	}

	List<Chunk *> sounds = object->lookup_child("TRAKSOUN");

	for (LIF<Chunk *> each(&sounds); !each.done(); each.next())
	{
		Object_Track_Sound_Chunk *const sound = dynamic_cast<Object_Track_Sound_Chunk *>(each());

		if (sound && sound->index == index)
		{
			return sound;
		}
	}

	return nullptr;
}

// "at <name>|<x>|<y>|<z>": the object's place (OBJHEAD1, as the game's Object_Chunk holds it) is exactly that
static void CheckAt(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 4);
	long long at[3] = {};
	bool good = fields.size() == 4 && !fields[0].empty();

	for (int i = 0; good && i < 3; i++)
	{
		good = WholeNumber(fields[(size_t)i + 1], INT_MIN, INT_MAX, &at[i]);
	}

	if (!good)
	{
		Check(false, "an at check as <name>|<x>|<y>|<z>, not \"" + asked + "\"");
		return;
	}

	Object_Chunk *const object = ObjectNamed(level, fields[0].c_str());
	std::string const held = object ? std::to_string(object->object_data.location.x) + " " + std::to_string(object->object_data.location.y) + " " +
		std::to_string(object->object_data.location.z) : std::string("no object of that name");

	Check(object && object->object_data.location.x == at[0] && object->object_data.location.y == at[1] && object->object_data.location.z == at[2],
		"the copy's object \"" + fields[0] + "\" is at " + fields[1] + " " + fields[2] + " " + fields[3] + " (it is at: " + held + ")");
}

// "up <name>|<ux>|<uy>|<uz>|<within>": the object's UP as the game draws it is (ux uy uz) thousandths, each within <within> thousandths: its local (0, -1, 0) (the world is y down)
// turned by its OBJHEAD1 turn (x y z w) with the turn's own matrix R, which is what the game's loader (QuatToMat of (-x, -y, -z, w)) and _RotateVector give (src/host/surface_place.h).
// For placement mode's turn (P3: the base flat on a wall or floor). Added 2026-11-18 by Vasquez.
static void CheckUp(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 5);
	long long wanted[3] = {};
	long long within = 0;
	bool good = fields.size() == 5 && !fields[0].empty() && WholeNumber(fields[4], 0, 2000, &within);

	for (int i = 0; good && i < 3; i++)
	{
		good = WholeNumber(fields[(size_t)i + 1], -1000, 1000, &wanted[i]);
	}

	if (!good)
	{
		Check(false, "an up check as <name>|<ux>|<uy>|<uz>|<within>, thousandths from -1000 to 1000, not \"" + asked + "\"");
		return;
	}

	Object_Chunk *const object = ObjectNamed(level, fields[0].c_str());

	if (!object)
	{
		Check(false, "the copy's object \"" + fields[0] + "\" has its up along " + fields[1] + " " + fields[2] + " " + fields[3] + " (no object of that name)");
		return;
	}

	double x = object->object_data.orientation.x, y = object->object_data.orientation.y, z = object->object_data.orientation.z, w = object->object_data.orientation.w;
	double const length = sqrt(x * x + y * y + z * z + w * w);
	bool close = length > 1e-6 && length < 1e6;   // (a turn of no length, or not a number, is not a turn: the comparisons fail)
	long long up[3] = { 0, 0, 0 };

	if (close)
	{
		x /= length;
		y /= length;
		z /= length;
		w /= length;

		double const turned[3] = { -2.0 * (x * y - z * w), -(1.0 - 2.0 * (x * x + z * z)), -2.0 * (y * z + x * w) };

		for (int i = 0; i < 3; i++)
		{
			up[i] = (long long)floor(turned[i] * 1000.0 + 0.5);
			close = close && up[i] >= wanted[i] - within && up[i] <= wanted[i] + within;
		}
	}

	Check(close, "the copy's object \"" + fields[0] + "\" has its up within " + fields[4] + " thousandths of " + fields[1] + " " + fields[2] + " " + fields[3] + " (it is: " +
		std::to_string(up[0]) + " " + std::to_string(up[1]) + " " + std::to_string(up[2]) + ")");
}

// "touching <name>|<px>|<py>|<pz>|<nx>|<ny>|<nz>|<within>": the object's box TOUCHES the surface through (px py pz) whose normal, pointing out of it, is (nx ny nz) (65536 = length 1, any
// length but 0): of the eight corners of its shape's box (its SHAPE chunk's min and max, the shape's own frame), turned by its OBJHEAD1 turn with the turn's own matrix R (as "up" above) and
// put at its place, the one deepest along the normal is on that plane, within <within> (the file's units) either way: not floating off it, not sunk into it. For placement mode's place
// (Frost 11-19, Bishop's order: the push-out off the surface is the box's extent on the side that meets it).
static void CheckTouching(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 8);
	long long point[3] = {}, normal[3] = {};
	long long within = 0;
	bool good = fields.size() == 8 && !fields[0].empty() && WholeNumber(fields[7], 0, 100000, &within);

	for (int i = 0; good && i < 3; i++)
	{
		good = WholeNumber(fields[(size_t)i + 1], INT_MIN, INT_MAX, &point[i]) && WholeNumber(fields[(size_t)i + 4], -1000000, 1000000, &normal[i]);
	}

	double const normalLength = good ? sqrt((double)(normal[0] * normal[0] + normal[1] * normal[1] + normal[2] * normal[2])) : 0.0;

	if (!good || !(normalLength > 0.5))
	{
		Check(false, "a touching check as <name>|<px>|<py>|<pz>|<nx>|<ny>|<nz>|<within>, a normal of some length, not \"" + asked + "\"");
		return;
	}

	Object_Chunk *const object = ObjectNamed(level, fields[0].c_str());
	Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

	if (!object || !shape)
	{
		Check(false, "the copy's object \"" + fields[0] + "\" touches the surface (" + (object ? "it has no shape" : "no object of that name") + ")");
		return;
	}

	double x = object->object_data.orientation.x, y = object->object_data.orientation.y, z = object->object_data.orientation.z, w = object->object_data.orientation.w;
	double const length = sqrt(x * x + y * y + z * z + w * w);

	if (!(length > 1e-6 && length < 1e6))
	{
		Check(false, "the copy's object \"" + fields[0] + "\" touches the surface (its turn has no length)");
		return;
	}

	x /= length;
	y /= length;
	z /= length;
	w /= length;

	double const turn[3][3] = {
		{ 1.0 - 2.0 * (y * y + z * z), 2.0 * (x * y - z * w), 2.0 * (x * z + y * w) },
		{ 2.0 * (x * y + z * w), 1.0 - 2.0 * (x * x + z * z), 2.0 * (y * z - x * w) },
		{ 2.0 * (x * z - y * w), 2.0 * (y * z + x * w), 1.0 - 2.0 * (x * x + y * y) },
	};
	double const n[3] = { normal[0] / normalLength, normal[1] / normalLength, normal[2] / normalLength };
	double const place[3] = { (double)object->object_data.location.x, (double)object->object_data.location.y, (double)object->object_data.location.z };
	double const least[3] = { (double)shape->shape_data.min.x, (double)shape->shape_data.min.y, (double)shape->shape_data.min.z };
	double const most[3] = { (double)shape->shape_data.max.x, (double)shape->shape_data.max.y, (double)shape->shape_data.max.z };
	double deepest = 0.0;

	for (int corner = 0; corner < 8; corner++)
	{
		double const local[3] = { (corner & 1) ? most[0] : least[0], (corner & 2) ? most[1] : least[1], (corner & 4) ? most[2] : least[2] };
		double along = 0.0;

		for (int i = 0; i < 3; i++)
		{
			double const world = place[i] + turn[i][0] * local[0] + turn[i][1] * local[1] + turn[i][2] * local[2];

			along += n[i] * (world - (double)point[i]);
		}

		deepest = corner == 0 || along < deepest ? along : deepest;
	}

	char words[320];

	_snprintf_s(words, sizeof(words), _TRUNCATE, "%.1f off the surface (its deepest corner; + floating, - sunk), its box %d %d %d .. %d %d %d, its place %d %d %d", deepest,
		shape->shape_data.min.x, shape->shape_data.min.y, shape->shape_data.min.z, shape->shape_data.max.x, shape->shape_data.max.y, shape->shape_data.max.z, object->object_data.location.x,
		object->object_data.location.y, object->object_data.location.z);
	Check(fabs(deepest) <= (double)within, "the copy's object \"" + fields[0] + "\" touches the surface through " + fields[1] + " " + fields[2] + " " + fields[3] + ", normal " + fields[4] + " " +
		fields[5] + " " + fields[6] + ", within " + fields[7] + ": " + words);
}

// "near <name>|<x>|<y>|<z>|<within>": the object's place (OBJHEAD1) is within <within> of (x y z) on each of the three
static void CheckNear(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 5);
	long long at[3] = {};
	long long within = 0;
	bool good = fields.size() == 5 && !fields[0].empty() && WholeNumber(fields[4], 0, INT_MAX, &within);

	for (int i = 0; good && i < 3; i++)
	{
		good = WholeNumber(fields[(size_t)i + 1], INT_MIN, INT_MAX, &at[i]);
	}

	if (!good)
	{
		Check(false, "a near check as <name>|<x>|<y>|<z>|<within>, not \"" + asked + "\"");
		return;
	}

	Object_Chunk *const object = ObjectNamed(level, fields[0].c_str());
	bool close = object != nullptr;

	if (object)
	{
		long long const held[3] = { object->object_data.location.x, object->object_data.location.y, object->object_data.location.z };

		for (int i = 0; i < 3; i++)
		{
			close = close && held[i] >= at[i] - within && held[i] <= at[i] + within;   // long long: no sum here can overflow (each part is an int)
		}
	}

	std::string const held = object ? std::to_string(object->object_data.location.x) + " " + std::to_string(object->object_data.location.y) + " " +
		std::to_string(object->object_data.location.z) : std::string("no object of that name");

	Check(close, "the copy's object \"" + fields[0] + "\" is within " + fields[4] + " of " + fields[1] + " " + fields[2] + " " + fields[3] + " (it is at: " + held + ")");
}

// "trackpivot <name>|<section>|<sx>|<sy>|<sz>|<ex>|<ey>|<ez>": the object's track (OBJTRAK2) has that section, and the section's pivot starts and ends at those places (the file's units)
static void CheckTrackPivot(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 8);
	long long section = 0, at[6] = {};
	bool good = fields.size() == 8 && !fields[0].empty() && WholeNumber(fields[1], 0, INT_MAX, &section);

	for (int i = 0; good && i < 6; i++)
	{
		good = WholeNumber(fields[(size_t)i + 2], INT_MIN, INT_MAX, &at[i]);
	}

	if (!good)
	{
		Check(false, "a trackpivot check as <name>|<section>|<sx>|<sy>|<sz>|<ex>|<ey>|<ez>, not \"" + asked + "\"");
		return;
	}

	Object_Chunk *const object = ObjectNamed(level, fields[0].c_str());
	Object_Track_Chunk2 *const track = object ? dynamic_cast<Object_Track_Chunk2 *>(object->lookup_single_child("OBJTRAK2")) : nullptr;
	std::string held = object ? (track ? "no such section" : "no track") : "no object of that name";
	bool same = false;

	if (track && track->sections && section < (long long)track->num_sections)
	{
		ChunkTrackSection const &s = track->sections[section];

		held = std::to_string(s.pivot_start.x) + "|" + std::to_string(s.pivot_start.y) + "|" + std::to_string(s.pivot_start.z) + "|" + std::to_string(s.pivot_end.x) + "|" +
			std::to_string(s.pivot_end.y) + "|" + std::to_string(s.pivot_end.z);
		same = s.pivot_start.x == at[0] && s.pivot_start.y == at[1] && s.pivot_start.z == at[2] && s.pivot_end.x == at[3] && s.pivot_end.y == at[4] && s.pivot_end.z == at[5];
	}

	Check(same, "the copy's object \"" + fields[0] + "\" has track section " + fields[1] + " from " + fields[2] + " " + fields[3] + " " + fields[4] + " to " + fields[5] + " " + fields[6] + " " +
		fields[7] + " (it holds: " + held + ")");
}

// "trackendmoved <name>|<section>|<x>|<y>|<z>": the object's track has that section, and the section's pivot no longer ENDS at (x y z) (T-3b: a snap moved the end marker, wherever it landed)
static void CheckTrackEndMoved(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 5);
	long long section = 0, at[3] = {};
	bool good = fields.size() == 5 && !fields[0].empty() && WholeNumber(fields[1], 0, INT_MAX, &section);

	for (int i = 0; good && i < 3; i++)
	{
		good = WholeNumber(fields[(size_t)i + 2], INT_MIN, INT_MAX, &at[i]);
	}

	if (!good)
	{
		Check(false, "a trackendmoved check as <name>|<section>|<x>|<y>|<z>, not \"" + asked + "\"");
		return;
	}

	Object_Chunk *const object = ObjectNamed(level, fields[0].c_str());
	Object_Track_Chunk2 *const track = object ? dynamic_cast<Object_Track_Chunk2 *>(object->lookup_single_child("OBJTRAK2")) : nullptr;
	std::string held = object ? (track ? "no such section" : "no track") : "no object of that name";
	bool moved = false;

	if (track && track->sections && section < (long long)track->num_sections)
	{
		ChunkTrackSection const &t = track->sections[section];

		held = std::to_string(t.pivot_end.x) + " " + std::to_string(t.pivot_end.y) + " " + std::to_string(t.pivot_end.z);
		moved = t.pivot_end.x != at[0] || t.pivot_end.y != at[1] || t.pivot_end.z != at[2];
	}

	Check(moved, "the copy's object \"" + fields[0] + "\" has track section " + fields[1] + " ending somewhere else than " + fields[2] + " " + fields[3] + " " + fields[4] + " (it ends at: " + held + ")");
}

// "notracksound <name>|<index>": the object is there and holds no track sound with that index
static void CheckNoTrackSound(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 2);
	long long index = 0;

	if (fields.size() != 2 || fields[0].empty() || !WholeNumber(fields[1], 0, 2, &index))
	{
		Check(false, "a notracksound check as <name>|<index 0-2>, not \"" + asked + "\"");
		return;
	}

	std::string const &name = fields[0];
	Object_Chunk *const object = ObjectNamed(level, name.c_str());
	Object_Track_Sound_Chunk *const found = TrackSoundOf(object, (int)index);

	Check(object && !found, "the copy's object \"" + name + "\" holds no track sound " + std::to_string(index) + (object ? (found ? std::string(" (it holds one: ") +
		(found->wav_name ? found->wav_name : "(no name)") + ")" : std::string()) : std::string(" (no object of that name)")));
}

// "tracksound <name>|<index>|<wav>|<inner>|<outer>|<volume>|<pitch>|<flags>": the object's TRAKSOUN with that index holds exactly these (a chunk under 24 bytes is not read as a track
// sound by the game's loader: RIF_IMPLEMENT_DYNCREATE_HOLDING_AT_LEAST, so it is not found here either)
static void CheckTrackSound(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 8);
	long long index = 0, inner = 0, outer = 0, volume = 0, pitch = 0, flags = 0;

	if (fields.size() != 8 || fields[0].empty() || !WholeNumber(fields[1], 0, 2, &index) || !WholeNumber(fields[3], 0, 0xffffffffLL, &inner) ||
		!WholeNumber(fields[4], 0, 0xffffffffLL, &outer) || !WholeNumber(fields[5], INT_MIN, INT_MAX, &volume) || !WholeNumber(fields[6], INT_MIN, INT_MAX, &pitch) ||
		!WholeNumber(fields[7], INT_MIN, INT_MAX, &flags))
	{
		Check(false, "a tracksound check as <name>|<index 0-2>|<wav>|<inner>|<outer>|<volume>|<pitch>|<flags>, not \"" + asked + "\"");
		return;
	}

	Object_Chunk *const object = ObjectNamed(level, fields[0].c_str());
	Object_Track_Sound_Chunk *const found = TrackSoundOf(object, (int)index);
	std::string held = "none";

	if (found)
	{
		held = std::string(found->wav_name ? found->wav_name : "(no name)") + "|" + std::to_string(found->inner_range) + "|" + std::to_string(found->outer_range) + "|" +
			std::to_string(found->max_volume) + "|" + std::to_string(found->pitch) + "|" + std::to_string(found->flags);
	}

	bool const same = found && found->wav_name && fields[2] == found->wav_name && (long long)found->inner_range == inner && (long long)found->outer_range == outer &&
		found->max_volume == (int)volume && found->pitch == (int)pitch && found->flags == (int)flags;

	Check(same, "the copy's object \"" + fields[0] + "\" holds track sound " + fields[1] + " as " + fields[2] + "|" + fields[3] + "|" + fields[4] + "|" + fields[5] + "|" + fields[6] + "|" +
		fields[7] + (object ? " (it holds: " + held + ")" : " (no object of that name)"));
}

// "nostrategy <name>": the first holder of that exact name, in the strategy lookup's order (the level's objects, its virtual objects, its enemies and generators, its placed hierarchies), is in the
// copy and holds no strategy (FindObjectStrategy answers NoStrategy: no AVPSTRAT where its strategy would be)
static void CheckNoStrategy(OpenLevelFile &copy, char const *name)
{
	uint8_t id[8] = { 0 };
	bool found = false;
	std::string what;
	Object_Chunk *const object = ObjectNamed(copy, name);

	if (object)
	{
		memcpy(id, &object->object_data.ID, 8);
		found = true;
		what = "object";
	}

	std::vector<host::VirtualObjectRef> virtuals;
	std::vector<host::GeneratorRef> generators;
	std::vector<host::PlacedHierarchyRef> hierarchies;

	host::ListVirtualObjects(copy.Level(), &virtuals);
	host::ListGenerators(copy.Level(), &generators);
	host::ListPlacedHierarchies(copy.Level(), &hierarchies);

	for (size_t i = 0; !found && i < virtuals.size(); i++)
	{
		if (virtuals[i].name == name)
		{
			memcpy(id, virtuals[i].id, 8);
			found = true;
			what = "virtual object";
		}
	}

	for (size_t i = 0; !found && i < generators.size(); i++)
	{
		if (generators[i].name == name)
		{
			memcpy(id, generators[i].id, 8);
			found = true;
			what = "enemy or generator";
		}
	}

	for (size_t i = 0; !found && i < hierarchies.size(); i++)
	{
		if (hierarchies[i].name == name)
		{
			memcpy(id, hierarchies[i].id, 8);
			found = true;
			what = "placed hierarchy";
		}
	}

	host::ObjectStrategy held;
	host::StrategyFound const answer = found ? host::FindObjectStrategy(copy.Level(), id, &held) : host::StrategyFound::NoSuchObject;

	Check(found && answer == host::StrategyFound::NoStrategy, std::string("the copy has the ") + (found ? what : std::string("holder")) + " \"" + name + "\" and it holds no strategy (" +
		(found ? host::StrategyFoundWords(answer) : "no holder of that name") + ")");
}

// A hex number of the check's words, and only that: 1 to 8 hex digits, nothing else
static bool HexWord(std::string const &word, uint32_t *out)
{
	if (word.empty() || word.size() > 8)
	{
		return false;
	}

	uint32_t value = 0;

	for (char const letter : word)
	{
		int const digit = letter >= '0' && letter <= '9' ? letter - '0' : (letter >= 'a' && letter <= 'f' ? letter - 'a' + 10 : (letter >= 'A' && letter <= 'F' ? letter - 'A' + 10 : -1));

		if (digit < 0)
		{
			return false;
		}

		value = (value << 4) | (uint32_t)digit;
	}

	*out = value;
	return true;
}

// "modflags <name>|<mask>|<word>": the first object of that exact name is a module (it holds a MODULEDT) with a MODFLAGS whose flag word, ANDed with the mask, is the word (the chunks as the game's
// loader made them: a chunk it could not read is a "miscellaneous" one and fails the check)
static void CheckModFlags(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 3);
	uint32_t mask = 0, want = 0;

	if (fields.size() != 3 || fields[0].empty() || !HexWord(fields[1], &mask) || !HexWord(fields[2], &want))
	{
		Check(false, "a modflags check as <name>|<mask hex>|<word hex>, not \"" + asked + "\"");
		return;
	}

	Object_Chunk *const object = ObjectNamed(level, fields[0].c_str());
	Chunk *const moduleChunk = object ? object->lookup_single_child("MODULEDT") : nullptr;
	Chunk_With_Children *const module = moduleChunk && !moduleChunk->r_u_miscellaneous() ? dynamic_cast<Chunk_With_Children *>(moduleChunk) : nullptr;
	Chunk *const flagsChunk = module ? module->lookup_single_child("MODFLAGS") : nullptr;
	Module_Flag_Chunk *const flags = flagsChunk && !flagsChunk->r_u_miscellaneous() ? dynamic_cast<Module_Flag_Chunk *>(flagsChunk) : nullptr;
	char held[64];

	if (flags)
	{
		_snprintf_s(held, sizeof(held), _TRUNCATE, "its flag word is %08x", (unsigned)(uint32_t)flags->Flags);
	}
	else
	{
		_snprintf_s(held, sizeof(held), _TRUNCATE, "%s", !object ? "no object of that name" : (!module ? "it is not a module" : "it has no MODFLAGS"));
	}

	Check(flags && ((uint32_t)flags->Flags & mask) == want, "the copy's module \"" + fields[0] + "\" has the flag word " + fields[2] + " under the mask " + fields[1] + " (" + held + ")");
}

// "gameflags <name>|<mask>|<word>": the module's flag bits AS THE GAME'S LOADER TAKES THEM, ANDed with the mask, are the word. The loader ORs a MODFLAGS's word into
// the module's flags only when it has one (projload.cpp 2232-2234, "if(mfc) ... m_flags|=mfc->Flags"), so a module with no MODFLAGS has none of these bits: for a
// check that a bit is CLEAR (Drake 11-21: an auto-plot with the Airduct box unticked writes no MODFLAGS, and stock PROC11 and med16 have none). modflags, above,
// still wants the chunk itself
static void CheckGameFlags(OpenLevelFile &level, std::string const &asked)
{
	std::vector<std::string> const fields = BarFields(asked, 3);
	uint32_t mask = 0, want = 0;

	if (fields.size() != 3 || fields[0].empty() || !HexWord(fields[1], &mask) || !HexWord(fields[2], &want))
	{
		Check(false, "a gameflags check as <name>|<mask hex>|<word hex>, not \"" + asked + "\"");
		return;
	}

	Object_Chunk *const object = ObjectNamed(level, fields[0].c_str());
	Chunk *const moduleChunk = object ? object->lookup_single_child("MODULEDT") : nullptr;
	Chunk_With_Children *const module = moduleChunk && !moduleChunk->r_u_miscellaneous() ? dynamic_cast<Chunk_With_Children *>(moduleChunk) : nullptr;
	Chunk *const flagsChunk = module ? module->lookup_single_child("MODFLAGS") : nullptr;
	Module_Flag_Chunk *const flags = flagsChunk && !flagsChunk->r_u_miscellaneous() ? dynamic_cast<Module_Flag_Chunk *>(flagsChunk) : nullptr;
	bool const unreadFlags = flagsChunk && !flags;   // a MODFLAGS the loader could not read: the check can't say what the game takes
	uint32_t const word = flags ? (uint32_t)flags->Flags : 0u;
	char held[64];

	_snprintf_s(held, sizeof(held), _TRUNCATE, "%s", !object ? "no object of that name" : (!module ? "it is not a module" : (unreadFlags ? "its MODFLAGS is unreadable" : "")));

	if (module && !unreadFlags && flags)
	{
		_snprintf_s(held, sizeof(held), _TRUNCATE, "its MODFLAGS word is %08x", (unsigned)word);
	}
	else if (module && !unreadFlags)
	{
		_snprintf_s(held, sizeof(held), _TRUNCATE, "%s", "no MODFLAGS: the game takes no bits");
	}

	Check(module && !unreadFlags && (word & mask) == want, "the copy's module \"" + fields[0] + "\" has, as the game takes it, the flag word " + fields[2] + " under the mask " +
		fields[1] + " (" + held + ")");
}

int main(int argc, char **argv)
{
	if (argc < 4 || (argc - 2) % 2 != 0)
	{
		printf("verify_objects: <copy> gone|kept|track|notrack <name> [gone|kept|track|notrack <name>] ... [tracksound <name>|<index>|<wav>|<inner>|<outer>|<volume>|<pitch>|<flags>] [notracksound <name>|<index>] [trackpivot <name>|<section>|<sx>|<sy>|<sz>|<ex>|<ey>|<ez>] [at <name>|<x>|<y>|<z>] [trackendmoved <name>|<section>|<x>|<y>|<z>] [alt <name>=<count>,<group>] [virtualgone|virtualkept <name>] [nostrategy <name>] [modflags <name>|<mask hex>|<word hex>] [gameflags <name>|<mask hex>|<word hex>]\n");
		return 2;
	}

	OpenLevelFile copy;

	Check(copy.Open(argv[1]) == LevelOpenResult::Opened && copy.Level() != nullptr, std::string("the game's loader reads the copy: ") + argv[1]);

	if (checksFailed)
	{
		printf("FAILED verify_objects\n");
		return 1;
	}

	for (int a = 2; a + 1 < argc; a += 2)
	{
		std::string const what = argv[a];
		bool const there = HasObjectNamed(copy, argv[a + 1]);

		if (what == "gone")
		{
			Check(!there, std::string("the copy has no object named \"") + argv[a + 1] + "\"");
		}
		else if (what == "kept")
		{
			Check(there, std::string("the copy has an object named \"") + argv[a + 1] + "\"");
		}
		else if (what == "track" || what == "notrack")
		{
			// the object holds a track of its own (an OBJTRAK2 chunk: Object Placement's Copy / Paste Track, OP-20 / OP-21), or holds none
			Object_Chunk *const object = ObjectNamed(copy, argv[a + 1]);
			bool const hasTrack = object && object->lookup_single_child("OBJTRAK2") != nullptr;

			Check(object && hasTrack == (what == "track"), std::string("the copy's object \"") + argv[a + 1] + "\" " + (what == "track" ? "holds a track (OBJTRAK2)" : "holds no track") +
				(object ? "" : " (no object of that name)"));
		}
		else if (what == "alt")
		{
			// <name>=<count>,<group>: the name up to the last =, then two whole numbers
			std::string const words = argv[a + 1];
			size_t const equals = words.rfind('=');
			int count = -1, group = -1;
			bool const read = equals != std::string::npos && sscanf_s(words.c_str() + equals + 1, "%d,%d", &count, &group) == 2;
			std::string const name = read ? words.substr(0, equals) : words;
			Object_Chunk *const object = read ? ObjectNamed(copy, name.c_str()) : nullptr;
			model::AltLocations locations;
			uint8_t id[8];

			if (object)
			{
				memcpy(id, &object->object_data.ID, sizeof(id));
			}

			bool const loaded = object && host::AlternateLocations_Load(&copy, id, &locations) == host::AltLocationsResult::Done;

			Check(loaded && (int)locations.list.size() == count && locations.group == group, "the copy's object \"" + name + "\" has " + std::to_string(count) + " alternate locations in group " +
				std::to_string(group) + (!read ? " (the words are not <name>=<count>,<group>)" : !object ? " (no object of that name)" : !loaded ? " (its ALTLOCAT does not load)" : " (it has " +
				std::to_string((unsigned long long)locations.list.size()) + " in group " + std::to_string(locations.group) + ")"));
		}
		else if (what == "specialalt")
		{
			// <name>=<count>,<group>: a SPECIAL object of that name (a generator, a bad guy, a sound, a particle generator), its ALTLOCAT where the game reads a special's (special_alt_edit.h;
			// added 2026-11-17 by Drake, OP-34..38 for specials)
			std::string const words = argv[a + 1];
			size_t const equals = words.rfind('=');
			int count = -1, group = -1;
			bool const read = equals != std::string::npos && sscanf_s(words.c_str() + equals + 1, "%d,%d", &count, &group) == 2;
			std::string const name = read ? words.substr(0, equals) : words;
			std::vector<host::SpecialObject> specials;
			host::SpecialObject const *found = nullptr;

			if (read && host::SpecialObjects_Read(&copy, &specials) == host::SpecialResult::Done)
			{
				for (host::SpecialObject const &one : specials)
				{
					found = !found && one.name == name && one.kind != host::SpecialKind::PlayerStart ? &one : found;
				}
			}

			model::AltLocations locations;
			bool const loaded = found && host::SpecialAlt_Load(&copy, found->kind, found->number, &locations) == host::SpecialResult::Done;

			Check(loaded && (int)locations.list.size() == count && locations.group == group, "the copy's special object \"" + name + "\" has " + std::to_string(count) + " alternate locations in group " +
				std::to_string(group) + (!read ? " (the words are not <name>=<count>,<group>)" : !found ? " (no special object of that name)" : !loaded ? " (its ALTLOCAT does not load)" : " (it has " +
				std::to_string((unsigned long long)locations.list.size()) + " in group " + std::to_string(locations.group) + ")"));
		}
		else if (what == "starts" || what == "startnear")
		{
			// "starts <count>": the copy holds that many player starts (AVPSTART); "startnear <n>|<x>|<y>|<z>|<within>": the n-th of them (from 1, in the file's order, the order inteng's
			// FUN_00422110 numbers them "Player Start %d" in) is within <within> of (x y z) on each axis, the file's units (added 2026-11-18 by Drake, OP-29)
			std::vector<host::SpecialObject> specials;
			std::vector<host::SpecialObject> starts;
			bool const read = host::SpecialObjects_Read(&copy, &specials) == host::SpecialResult::Done;

			for (host::SpecialObject const &one : specials)
			{
				if (one.kind == host::SpecialKind::PlayerStart)
				{
					starts.push_back(one);
				}
			}

			if (what == "starts")
			{
				long long want = -1;
				bool const asked = WholeNumber(argv[a + 1], 0, INT_MAX, &want);

				Check(read && asked && (long long)starts.size() == want, std::string("the copy holds ") + argv[a + 1] + " player start(s) (it holds " + std::to_string((unsigned long long)starts.size()) +
					(read ? "" : "; its special objects could not be read") + ")");
			}
			else
			{
				std::vector<std::string> const fields = BarFields(argv[a + 1], 5);
				long long n = 0, at[3] = {}, within = 0;
				bool good = fields.size() == 5 && WholeNumber(fields[0], 1, INT_MAX, &n) && WholeNumber(fields[4], 0, INT_MAX, &within);

				for (int i = 0; good && i < 3; i++)
				{
					good = WholeNumber(fields[(size_t)i + 1], INT_MIN, INT_MAX, &at[i]);
				}

				bool close = good && read && (size_t)n <= starts.size();
				std::string held = "no such start";

				if (close)
				{
					host::SpecialObject const &one = starts[(size_t)n - 1];

					for (int i = 0; i < 3; i++)
					{
						close = close && (long long)one.place[i] >= at[i] - within && (long long)one.place[i] <= at[i] + within;
					}

					held = std::to_string(one.place[0]) + " " + std::to_string(one.place[1]) + " " + std::to_string(one.place[2]);
				}

				Check(close, std::string("the copy's player start ") + (good ? std::to_string(n) : "(a startnear check as <n>|<x>|<y>|<z>|<within>)") + " is within " + (good ? fields[4] : "?") + " of " +
					(good ? fields[1] + " " + fields[2] + " " + fields[3] : std::string("?")) + " (it is at: " + held + "; " + std::to_string((unsigned long long)starts.size()) + " starts)");
			}
		}
		else if (what == "tracksound")
		{
			CheckTrackSound(copy, argv[a + 1]);
		}
		else if (what == "notracksound")
		{
			CheckNoTrackSound(copy, argv[a + 1]);
		}
		else if (what == "trackendmoved")
		{
			CheckTrackEndMoved(copy, argv[a + 1]);
		}
		else if (what == "trackpivot")
		{
			CheckTrackPivot(copy, argv[a + 1]);
		}
		else if (what == "virtualgone" || what == "virtualkept")
		{
			// a VIRTUAL object (VIOBJECT in SPECLOBJ, strategy_lookup.h ListVirtualObjects) of that exact name is not there / is (S-87: Delete Multiple's Obsolete Switches)
			std::vector<host::VirtualObjectRef> virtuals;
			bool there = false;

			host::ListVirtualObjects(copy.Level(), &virtuals);

			for (host::VirtualObjectRef const &v : virtuals)
			{
				there = there || v.name == argv[a + 1];
			}

			Check(there == (what == "virtualkept"), std::string("the copy ") + (what == "virtualkept" ? "has" : "has no") + " virtual object named \"" + argv[a + 1] + "\"");
		}
		else if (what == "nostrategy")
		{
			CheckNoStrategy(copy, argv[a + 1]);
		}
		else if (what == "at")
		{
			CheckAt(copy, argv[a + 1]);
		}
		else if (what == "near")
		{
			CheckNear(copy, argv[a + 1]);
		}
		else if (what == "up")
		{
			CheckUp(copy, argv[a + 1]);
		}
		else if (what == "touching")
		{
			CheckTouching(copy, argv[a + 1]);
		}
		else if (what == "modflags")
		{
			CheckModFlags(copy, argv[a + 1]);
		}
		else if (what == "gameflags")
		{
			CheckGameFlags(copy, argv[a + 1]);
		}
		else
		{
			Check(false, "a word the check knows (gone, kept, track, notrack, tracksound, notracksound, trackpivot, trackendmoved, at, near, up, touching, alt, specialalt, virtualgone, virtualkept, nostrategy, modflags or gameflags), not \"" + what + "\"");
		}
	}

	printf("%s verify_objects\n", checksFailed ? "FAILED" : "PASSED");
	return checksFailed ? 1 : 0;
}
