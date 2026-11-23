// See shape_update.h. Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "shape_update.h"
#include "shape_import.h"   // the chunk helpers (Frost's, one hardened walker for hostile bytes)

#include <math.h>
#include <string.h>

#include <algorithm>
#include <string>
#include <tuple>
#include <vector>

namespace host
{
	namespace
	{
		using namespace shape_chunks;

		size_t const kHeadPayload = 68;            // SHPHEAD1 up to the count of the objects that use the shape (its names follow)
		size_t const kSubHeadPayload = 44;         // SUBSHPHD: flags, number, points, polygons, radius, the box
		int32_t const kFarthest = 1 << 24;         // the game's CheckPlace (chunk.hpp)
		double const kMostRescale = 1000000.0;     // Rescale's own rule (OP-13: DAT_005ae590)
		int32_t const kMostChunkBytes = 0x7fffffff;

		bool Fail(std::string *why, char const *words)
		{
			if (why)
			{
				*why = words;
			}

			return false;
		}

		float FloatAt(Bytes const &bytes, size_t at)
		{
			float value;

			memcpy(&value, bytes.data() + at, sizeof(value));
			return value;
		}

		double DoubleAt(Bytes const &bytes, size_t at)
		{
			double value;

			memcpy(&value, bytes.data() + at, sizeof(value));
			return value;
		}

		void PutFloatAt(Bytes *bytes, size_t at, float value)
		{
			memcpy(bytes->data() + at, &value, sizeof(value));
		}

		void PutDoubleAt(Bytes *bytes, size_t at, double value)
		{
			memcpy(bytes->data() + at, &value, sizeof(value));
		}

		// A real number (not NaN, not endless), told from its bits: the editor is built with fast arithmetic, which isn't promised to compare NaN properly (as avpx real_numbers.h)
		bool IsReal(double value)
		{
			uint64_t bits;

			memcpy(&bits, &value, sizeof(bits));
			return ((bits >> 52) & 0x7ff) != 0x7ff;
		}

		// A whole number times the rescale, cut toward 0 as inteng's __ftol does (FUN_00430210); false when it would be farther than 2^24
		bool Scaled(int32_t value, double rescale, int32_t *out)
		{
			double const r = (double)value * rescale;

			if (!IsReal(r) || r < -(double)kFarthest || r > (double)kFarthest)
			{
				return false;
			}

			*out = (int32_t)r;
			return true;
		}

		// A zero-ended text inside [from, to): false when it has no end there
		bool TextIn(Bytes const &bytes, size_t from, size_t to, std::string *text)
		{
			if (from >= to || to > bytes.size())
			{
				return false;
			}

			void const *const zero = memchr(bytes.data() + from, 0, to - from);

			if (!zero)
			{
				return false;
			}

			text->assign(bytes.data() + from, (char const *)zero);
			return true;
		}

		bool Payload(Span const &span, size_t least)
		{
			return span.size >= kHeadingBytes + least;
		}

		// The level's shape, as the update needs it
		struct LevelShape
		{
			std::vector<Span> parts;
			size_t head = 0, extfl = 0;
			std::vector<Span> extParts;
			size_t extfn = 0, extobjnm = 0;
			double rescale = 1.0;
			std::string fileName, objectName;
			int32_t objectCount = 0;
		};

		ShapeUpdateResult ReadLevelShape(Bytes const &s, LevelShape *ls, std::string *why)
		{
			size_t size = 0;

			if (s.size() < kHeadingBytes || !NameIs(s, 0, "REBSHAPE") || !ChunkSizeAt(s, 0, s.size(), &size) || size != s.size() || !Children(s, kHeadingBytes, size, &ls->parts))
			{
				Fail(why, "the level's shape can't be walked");
				return ShapeUpdateResult::BadShape;
			}

			ls->head = FirstNamed(s, ls->parts, "SHPHEAD1");
			ls->extfl = FirstNamed(s, ls->parts, "SHPEXTFL");

			if (ls->extfl == ls->parts.size())
			{
				return ShapeUpdateResult::NotOutside;
			}

			if (ls->head == ls->parts.size() || !Payload(ls->parts[ls->head], kHeadPayload))
			{
				Fail(why, "the level's shape has no header, or one too short");
				return ShapeUpdateResult::BadShape;
			}

			ls->objectCount = IntAt(s, ls->parts[ls->head].at + kHeadingBytes + 64);

			Span const ext = ls->parts[ls->extfl];

			if (!Children(s, ext.at + kHeadingBytes, ext.at + ext.size, &ls->extParts))
			{
				Fail(why, "the level shape's SHPEXTFL can't be walked");
				return ShapeUpdateResult::BadShape;
			}

			ls->extfn = FirstNamed(s, ls->extParts, "SHPEXTFN");
			ls->extobjnm = FirstNamed(s, ls->extParts, "EXTOBJNM");

			if (ls->extfn == ls->extParts.size())
			{
				return ShapeUpdateResult::NotOutside;
			}

			Span const fn = ls->extParts[ls->extfn];

			// SHPEXTFN: the rescale (a double), the version, the file's name ended by a 0
			if (!Payload(fn, 13) || !TextIn(s, fn.at + kHeadingBytes + 12, fn.at + fn.size, &ls->fileName))
			{
				Fail(why, "the level shape's SHPEXTFN is too short or its name has no end");
				return ShapeUpdateResult::BadShape;
			}

			ls->rescale = DoubleAt(s, fn.at + kHeadingBytes);

			if (ls->extobjnm != ls->extParts.size())
			{
				Span const on = ls->extParts[ls->extobjnm];

				// EXTOBJNM: a pad, then the object's name ended by a 0
				if (!Payload(on, 5) || !TextIn(s, on.at + kHeadingBytes + 4, on.at + on.size, &ls->objectName))
				{
					Fail(why, "the level shape's EXTOBJNM is too short or its name has no end");
					return ShapeUpdateResult::BadShape;
				}
			}

			return ShapeUpdateResult::Done;
		}

		// The names of the objects a shape's header lists (the count at +64, then that many names ended by a 0), each measured against the header
		bool HeaderNames(Bytes const &b, Span const &head, std::vector<std::string> *names)
		{
			names->clear();

			size_t const payload = head.at + kHeadingBytes;
			int32_t const count = IntAt(b, payload + 64);
			size_t at = payload + kHeadPayload;
			size_t const end = head.at + head.size;

			if (count < 0 || (size_t)count > end - at)
			{
				return false;
			}

			for (int32_t i = 0; i < count; i++)
			{
				std::string one;

				if (!TextIn(b, at, end, &one))
				{
					return false;
				}

				at += one.size() + 1;
				names->push_back(one);
			}

			return true;
		}

		// The new SHPHEAD1 / SUBSHPHD payload: the given one with the scaled radius and box of the outside header. The points' and polygons' counts are NOT the outside header's: the caller
		// puts in the counts of the chunks the new shape holds (ChunkCounts), as the game's loader takes them and inteng's save writes them back (hostile-input-2 / -3, the 11-17 review)
		bool ScaledBox(Bytes const &o, size_t oFrom, double rescale, Bytes *into, size_t intoFrom)
		{
			float const radius = FloatAt(o, oFrom + 8);
			double const scaledRadius = (double)radius * rescale;

			if (!IsReal(scaledRadius) || scaledRadius < 0.0 || scaledRadius > (double)kFarthest)
			{
				return false;
			}

			PutFloatAt(into, intoFrom + 8, (float)scaledRadius);

			for (int k = 0; k < 6; k++)
			{
				int32_t v = 0;

				if (!Scaled(IntAt(o, oFrom + 12 + (size_t)k * 4), rescale, &v))
				{
					return false;
				}

				PutIntAt(into, intoFrom + 12 + (size_t)k * 4, v);
			}

			return true;
		}

		// The point and polygon counts of the shape (or sub-shape) whose parts these are, from the chunks themselves, as the game's loader takes them (chunk_shape.cpp: SHPRAWVT / 12,
		// SHPPOLYS / 36; none: 0). false when a chunk isn't whole records, or a count wouldn't fit an int.
		bool ChunkCounts(Bytes const &b, std::vector<Span> const &parts, int32_t *points, int32_t *polygons)
		{
			size_t const raw = FirstNamed(b, parts, "SHPRAWVT");
			size_t const polys = FirstNamed(b, parts, "SHPPOLYS");
			size_t const rawBytes = raw == parts.size() ? 0 : parts[raw].size - kHeadingBytes;
			size_t const polyBytes = polys == parts.size() ? 0 : parts[polys].size - kHeadingBytes;

			if (rawBytes % 12 != 0 || polyBytes % 36 != 0 || rawBytes / 12 > 0x7fffffff || polyBytes / 36 > 0x7fffffff)
			{
				return false;
			}

			*points = (int32_t)(rawBytes / 12);
			*polygons = (int32_t)(polyBytes / 36);
			return true;
		}

		// SHPRAWVT's points x the rescale; false when the size isn't whole points or a place would be too far
		bool ScaledPoints(Bytes const &o, Span const &raw, double rescale, Bytes *payload, std::vector<int32_t> *points)
		{
			size_t const bytes = raw.size - kHeadingBytes;

			if (bytes % 12 != 0)
			{
				return false;
			}

			payload->clear();
			points->clear();

			for (size_t i = 0; i < bytes; i += 4)
			{
				int32_t v = 0;

				if (!Scaled(IntAt(o, raw.at + kHeadingBytes + i), rescale, &v))
				{
					return false;
				}

				AddInt(payload, v);
				points->push_back(v);
			}

			return true;
		}

		// (A texture list's names, BitmapEntry and ReadBitmaps: shape_import.h's shape_chunks, where they moved 2026-11-23 for the placement picker's use too.)

		bool IsOneOf(Bytes const &b, size_t at, char const *const *names, size_t count)
		{
			for (size_t i = 0; i < count; i++)
			{
				if (NameIs(b, at, names[i]))
				{
					return true;
				}
			}

			return false;
		}

		// FUN_0050f570's six data chunks (made again from the outside shape), FUN_0050eb20's twelve extra kinds (deleted), and the eleven copied back in inteng's order (0x5136a6-0x513c25)
		char const *const kDataChunks[] = { "SHPRAWVT", "SHPVNORM", "SHPPNORM", "SHPPOLYS", "SHPUVCRD", "SHPTEXFN" };
		char const *const kDeletedExtras[] = { "SHPZSPDT", "SHPMRGDT", "SHPMORPH", "TEXTANIM", "SHPPCINF", "SHPFRAGS", "ANIMSEQU", "PNOTINBB", "ANSHCEN2", "ASALTTEX", "CONSHAPE", "SHPPRPRO" };
		char const *const kCopiedExtras[] = { "SHPZSPDT", "SHPMRGDT", "SHPMORPH", "TEXTANIM", "SHPPCINF", "SHPFRAGS", "ANIMSEQU", "PNOTINBB", "ANSHCEN2", "ASALTTEX", "CONSHAPE" };
		size_t const kDataCount = sizeof(kDataChunks) / sizeof(kDataChunks[0]);
		size_t const kDeletedCount = sizeof(kDeletedExtras) / sizeof(kDeletedExtras[0]);
	}

	// THE FIDELITY REWRITE (Vasquez 11-17 ~23:00; the 11-17 review's fidelity-1..12, traced again in reassess/op138-fidelity-traces-vasquez.json): everything below follows inteng's FUN_00512fc0
	// step by step on the bytes. Each refusal that inteng does not have is NAMED where it is made (a duplicate chunk inteng's lookup_single_child stops the program on, a file a CONSHAPE makes
	// inteng's loader stop on, a NULL shape inteng dereferences).

	bool ShapeUpdate_LevelUsers(std::vector<char> const &level, std::vector<int32_t> *users)
	{
		users->clear();

		std::vector<Span> top;
		size_t total = 0;

		if (level.size() < kHeadingBytes || !NameIs(level, 0, "REBINFF2") || !ChunkSizeAt(level, 0, level.size(), &total) || !Children(level, kHeadingBytes, total, &top))
		{
			return false;
		}

		users->assign(top.size(), 0);

		// FUN_004bf1d0 (0x4bf48c-0x4bf4b8): each RBOBJECT with an OBJHEAD1 is tied to the FIRST REBSHAPE, in the file's order, whose SHPHEAD1 number (+20) is its OBJHEAD1 shape number (+56)
		std::vector<std::pair<int32_t, size_t> > firstOfNumber;

		for (size_t i = 0; i < top.size(); i++)
		{
			std::vector<Span> parts;

			if (!NameIs(level, top[i].at, "REBSHAPE") || !Children(level, top[i].at + kHeadingBytes, top[i].at + top[i].size, &parts))
			{
				continue;
			}

			size_t const head = FirstNamed(level, parts, "SHPHEAD1");

			if (head == parts.size() || !Payload(parts[head], 24))
			{
				continue;
			}

			int32_t const number = IntAt(level, parts[head].at + kHeadingBytes + 20);
			bool seen = false;

			for (std::pair<int32_t, size_t> const &one : firstOfNumber)
			{
				seen = seen || one.first == number;
			}

			if (!seen)
			{
				firstOfNumber.push_back(std::make_pair(number, i));
			}
		}

		std::sort(firstOfNumber.begin(), firstOfNumber.end());

		for (size_t i = 0; i < top.size(); i++)
		{
			std::vector<Span> parts;

			if (!NameIs(level, top[i].at, "RBOBJECT") || !Children(level, top[i].at + kHeadingBytes, top[i].at + top[i].size, &parts))
			{
				continue;
			}

			size_t const head = FirstNamed(level, parts, "OBJHEAD1");

			if (head == parts.size() || !Payload(parts[head], 60))
			{
				continue;
			}

			int32_t const number = IntAt(level, parts[head].at + kHeadingBytes + 56);
			auto const found = std::lower_bound(firstOfNumber.begin(), firstOfNumber.end(), std::make_pair(number, (size_t)0));

			if (found != firstOfNumber.end() && found->first == number)
			{
				(*users)[found->second]++;
			}
		}

		return true;
	}

	namespace
	{
		size_t CountNamed(Bytes const &b, std::vector<Span> const &parts, char const *name)
		{
			size_t n = 0;

			for (Span const &part : parts)
			{
				n += NameIs(b, part.at, name) ? 1 : 0;
			}

			return n;
		}

		// FUN_0042f860 / FUN_0042f9b0 / FUN_0042fa00 (0x516c24-0x516cc5 for a SUBSHAPE, 0x50fc36-0x50fcd4 for the shape): the centre is the BOX's, (max + min) added as 32-bit numbers (the add
		// wraps) then halved as a double and cut toward 0 (__ftol); the radius about it is the farthest (scaled) point, each distance sqrt((dx*dx + dy*dy) + dz*dz) of the 32-bit difference,
		// kept as a float whenever it is farther than the float kept so far (FCOM, strict)
		void CentreAndRadius(int32_t const box[6], std::vector<int32_t> const &points, int32_t centre[3], float *radius)
		{
			for (int k = 0; k < 3; k++)
			{
				int32_t const sum = (int32_t)((uint32_t)box[k * 2] + (uint32_t)box[k * 2 + 1]);

				centre[k] = (int32_t)((double)sum / 2.0);
			}

			float r = 0.0f;

			for (size_t i = 0; i + 2 < points.size(); i += 3)
			{
				int32_t const dx = (int32_t)((uint32_t)points[i] - (uint32_t)centre[0]);
				int32_t const dy = (int32_t)((uint32_t)points[i + 1] - (uint32_t)centre[1]);
				int32_t const dz = (int32_t)((uint32_t)points[i + 2] - (uint32_t)centre[2]);
				double const len = sqrt(((double)dx * dx + (double)dy * dy) + (double)dz * dz);

				if (len > (double)r)
				{
					r = (float)len;
				}
			}

			*radius = r;
		}

		Bytes CentreChunk(int32_t const centre[3], float radius)
		{
			Bytes payload;

			AddInt(&payload, centre[0]);
			AddInt(&payload, centre[1]);
			AddInt(&payload, centre[2]);
			payload.resize(16, 0);
			PutFloatAt(&payload, 12, radius);
			return MakeChunk("SHPCENTR", payload);
		}

		// FUN_00516570 on a SUBSHAPE of the copied SHPFRAGS (0x51393c-0x5139b2), AT EVERY RESCALE (1.0 too): its own children in their order without the six data kinds (SUBSHPHD with the counts
		// of the chunks it now holds, its radius and box x the rescale; FRAGLOCN's place x the rescale, its pads as they are; an existing SHPCENTR worked out again from the scaled box and points),
		// then the six data kinds (SHPRAWVT x the rescale, the rest as they are), then a new SHPCENTR when it had none. NAMED: a second SUBSHPHD, FRAGLOCN, SHPCENTR or data chunk, which
		// inteng's lookup_single_child stops the program on, is refused.
		bool UpdatedSubShape(Bytes const &o, Span const &sub, double rescale, Bytes *out)
		{
			std::vector<Span> parts;

			if (!Children(o, sub.at + kHeadingBytes, sub.at + sub.size, &parts) || CountNamed(o, parts, "SUBSHPHD") != 1 || CountNamed(o, parts, "FRAGLOCN") > 1 ||
				CountNamed(o, parts, "SHPCENTR") > 1)
			{
				return false;
			}

			for (char const *const kind : kDataChunks)
			{
				if (CountNamed(o, parts, kind) > 1)
				{
					return false;
				}
			}

			size_t const hd = FirstNamed(o, parts, "SUBSHPHD");
			size_t const raw = FirstNamed(o, parts, "SHPRAWVT");
			Bytes pointsPayload;
			std::vector<int32_t> points;
			int32_t subPoints = 0, subPolygons = 0;

			if (!Payload(parts[hd], kSubHeadPayload) || (raw != parts.size() && !ScaledPoints(o, parts[raw], rescale, &pointsPayload, &points)) ||
				!ChunkCounts(o, parts, &subPoints, &subPolygons))
			{
				return false;
			}

			Bytes head(o.begin() + (std::ptrdiff_t)parts[hd].at, o.begin() + (std::ptrdiff_t)(parts[hd].at + parts[hd].size));

			if (!ScaledBox(o, parts[hd].at + kHeadingBytes + 8, rescale, &head, kHeadingBytes + 8))
			{
				return false;
			}

			PutIntAt(&head, kHeadingBytes + 8, subPoints);
			PutIntAt(&head, kHeadingBytes + 12, subPolygons);

			int32_t box[6], centre[3];
			float radius = 0.0f;

			for (int k = 0; k < 6; k++)
			{
				box[k] = IntAt(head, kHeadingBytes + 20 + (size_t)k * 4);
			}

			CentreAndRadius(box, points, centre, &radius);

			Bytes const centreChunk = CentreChunk(centre, radius);
			bool hadCentre = false;

			out->assign(o.begin() + (std::ptrdiff_t)sub.at, o.begin() + (std::ptrdiff_t)(sub.at + kHeadingBytes));

			for (Span const &part : parts)
			{
				if (IsOneOf(o, part.at, kDataChunks, kDataCount))
				{
					continue;
				}

				if (NameIs(o, part.at, "SUBSHPHD"))
				{
					out->insert(out->end(), head.begin(), head.end());
				}
				else if (NameIs(o, part.at, "SHPCENTR"))
				{
					out->insert(out->end(), centreChunk.begin(), centreChunk.end());
					hadCentre = true;
				}
				else if (NameIs(o, part.at, "FRAGLOCN"))
				{
					if (!Payload(part, 12))
					{
						return false;
					}

					Bytes chunk(o.begin() + (std::ptrdiff_t)part.at, o.begin() + (std::ptrdiff_t)(part.at + part.size));

					for (int k = 0; k < 3; k++)
					{
						int32_t v = 0;

						if (!Scaled(IntAt(o, part.at + kHeadingBytes + (size_t)k * 4), rescale, &v))
						{
							return false;
						}

						PutIntAt(&chunk, kHeadingBytes + (size_t)k * 4, v);
					}

					out->insert(out->end(), chunk.begin(), chunk.end());
				}
				else
				{
					AddCopy(out, o, part);
				}
			}

			for (char const *const kind : kDataChunks)
			{
				size_t const at = FirstNamed(o, parts, kind);

				if (at == parts.size())
				{
					continue;
				}

				if (strcmp(kind, "SHPRAWVT") == 0)
				{
					Bytes const made = MakeChunk("SHPRAWVT", pointsPayload);

					out->insert(out->end(), made.begin(), made.end());
				}
				else
				{
					AddCopy(out, o, parts[at]);
				}
			}

			if (!hadCentre)
			{
				out->insert(out->end(), centreChunk.begin(), centreChunk.end());
			}

			if (out->size() > (size_t)kMostChunkBytes)
			{
				return false;
			}

			PutIntAt(out, 8, (int32_t)out->size());
			return true;
		}

		// VerDedup (inteng 0x419a20 / FUN_00419ce0, avpx bitmap_names.cpp get_version_num / set_version_num): while two or more are left, the LAST goes when the first is higher than the
		// second, else the FIRST goes. Gives the index (into versions) of the one left, or versions.size() when there is none; *gone gets the indexes taken out.
		size_t VerDedup(std::vector<int32_t> const &versions, std::vector<size_t> *gone)
		{
			std::vector<size_t> left;

			for (size_t i = 0; i < versions.size(); i++)
			{
				left.push_back(i);
			}

			while (left.size() > 1)
			{
				if (versions[left[0]] > versions[left[1]])
				{
					gone->push_back(left.back());
					left.pop_back();
				}
				else
				{
					gone->push_back(left.front());
					left.erase(left.begin());
				}
			}

			return left.empty() ? versions.size() : left[0];
		}

		std::string Folded(std::string const &name)
		{
			std::string folded = name;

			for (char &c : folded)
			{
				c = (c >= 'A' && c <= 'Z') ? (char)(c - 'A' + 'a') : c;   // the CRT's _stricmp with no locale set (FUN_005909e0): A-Z only
			}

			return folded;
		}

		Bytes ListPayload(std::vector<BitmapEntry> const &entries)
		{
			Bytes payload;

			AddInt(&payload, (int32_t)entries.size());   // FUN_00419760 writes the count only (0x419786)

			for (BitmapEntry const &e : entries)
			{
				AddInt(&payload, e.flags);
				AddInt(&payload, e.index);
				AddInt(&payload, e.version);   // d1: the enum id above the 20-bit version, as read
				AddInt(&payload, e.priority);
				AddInt(&payload, e.transparency);
				payload.insert(payload.end(), e.name.begin(), e.name.end());
				payload.insert(payload.end(), 4 - e.name.size() % 4, 0);
			}

			return payload;
		}

		Bytes NameChunk(std::string const &name)
		{
			Bytes payload(name.begin(), name.end());

			payload.insert(payload.end(), 4 - name.size() % 4, 0);   // FUN_004beda0 / avpx RIF_Name_Chunk: the name, its 0, padded (the pad written as zeros)
			return MakeChunk("RIFFNAME", payload);
		}

		// The outside environment's RIFFNAME name (its first, FUN_004318c0 0x5147e3), when it has one: its payload up to its 0
		bool OutsideRiffName(Bytes const &o, std::vector<Span> const &env, std::string *name, bool *bad)
		{
			size_t const at = FirstNamed(o, env, "RIFFNAME");

			if (at == env.size())
			{
				return false;
			}

			if (!TextIn(o, env[at].at + kHeadingBytes, env[at].at + env[at].size, name))
			{
				*bad = true;
				return false;
			}

			return true;
		}

		// THE SHPEXTFL AFTER THE UPDATE (0x513c8e-0x514850; op138-fidelity-traces-vasquez.json part A, its "chunkOrder"): the level SHPEXTFL's children (the SHPEXTFN with the rescale and
		// version already in) with the texture list, its extended data and version, and the RIFFNAME done as inteng does them, then the new chunks in inteng's order: BMPLSTST, BMNAMEXT, BMNAMVER,
		// RIFFNAME. NAMED: the level's fixed palette (FUN_00432160 on its REBENVDT's PRSETPAL) is taken as none, so a list's palette bit 0x40 is never set and BMNAMEXT's bit 0 is cleared, and
		// PAL() (the level's ENVPALET / PRSETPAL "up to date" bits, FUN_00431df0 / FUN_00431f50) is not done: the 8-bit tools' palette bookkeeping.
		ShapeUpdateResult NewExternal(Bytes const &s, Span const &extfl, std::vector<Span> const &extParts, size_t extfn, double rescale, int32_t version, Bytes const &o,
			std::vector<Span> const *env, Bytes *newExt, std::string *why)
		{
			struct Item
			{
				Bytes chunk;
				bool kept = true;
			};

			std::vector<Item> items;

			for (size_t i = 0; i < extParts.size(); i++)
			{
				Item item;

				item.chunk.assign(s.begin() + (std::ptrdiff_t)extParts[i].at, s.begin() + (std::ptrdiff_t)(extParts[i].at + extParts[i].size));

				if (i == extfn)
				{
					PutDoubleAt(&item.chunk, kHeadingBytes, rescale);   // OP-138e: the rescale and the file shape's version
					PutIntAt(&item.chunk, kHeadingBytes + 8, version);
				}

				items.push_back(item);
			}

			auto const named = [&](size_t i, char const *name) { return items[i].kept && memcmp(items[i].chunk.data(), name, 8) == 0; };
			Bytes newList, newExtData, newVersion, newName;

			if (env)
			{
				// A1: the outside BMPNAMES (one at most: inteng's lookup_single_child stops the program on two: NAMED, refused); one with no names counts as none
				std::vector<BitmapEntry> theirs;
				int32_t theirHead = 0;
				size_t const n = FirstNamed(o, *env, "BMPNAMES");

				if (CountNamed(o, *env, "BMPNAMES") > 1)
				{
					Fail(why, "the shape file's environment holds two texture lists (BMPNAMES)");
					return ShapeUpdateResult::BadFile;
				}

				if (n != env->size() && !ReadBitmaps(o, (*env)[n], &theirHead, &theirs))
				{
					Fail(why, "the file's texture list (BMPNAMES) doesn't fit its chunk, or a texture's number is outside 0..32767");
					return ShapeUpdateResult::BadFile;
				}

				bool const haveTheirs = !theirs.empty();

				// A2: the level's lists
				std::vector<size_t> lists;

				for (size_t i = 0; i < items.size(); i++)
				{
					if (named(i, "BMPLSTST"))
					{
						lists.push_back(i);
					}
				}

				size_t current = items.size();   // the list kept (k == 1)
				bool listIsNew = false;

				if (lists.size() == 1)
				{
					current = lists[0];
				}
				else
				{
					for (size_t const i : lists)
					{
						items[i].kept = false;   // two or more: every one goes
					}

					listIsNew = haveTheirs;
				}

				// A3: the extended data, when there is a list: the first BMNAMEXT, or a new one; its bit 0 is the fixed palette's (none: NAMED above)
				size_t extData = items.size();
				bool extIsNew = false;
				int32_t extFlags = 0;

				if (current != items.size() || listIsNew)
				{
					for (size_t i = 0; i < items.size() && extData == items.size(); i++)
					{
						if (named(i, "BMNAMEXT"))
						{
							extData = i;
						}
					}

					if (extData != items.size())
					{
						if (items[extData].chunk.size() < kHeadingBytes + 4)
						{
							Fail(why, "the shape's BMNAMEXT is too short");
							return ShapeUpdateResult::BadShape;
						}

						extFlags = IntAt(items[extData].chunk, kHeadingBytes);
					}
					else
					{
						extIsNew = true;
					}

					extFlags &= ~1;
				}

				// the kept list's COUNT (its head word's low 16 bits, as inteng's loader reads it) is all the paths but the merge need; its entries are read for the merge only
				std::vector<BitmapEntry> mine;
				int32_t mineHead = 0;
				int32_t mineCount = 0;

				if (current != items.size())
				{
					if (items[current].chunk.size() < kHeadingBytes + 4)
					{
						Fail(why, "the shape's texture list (BMPLSTST) is too short");
						return ShapeUpdateResult::BadShape;
					}

					mineCount = IntAt(items[current].chunk, kHeadingBytes) & 0xffff;
				}

				if (haveTheirs && mineCount != 0)
				{
					Span const whole = { 0, items[current].chunk.size() };

					if (!ReadBitmaps(items[current].chunk, whole, &mineHead, &mine))
					{
						Fail(why, "the shape's texture list (BMPLSTST) can't be read");
						return ShapeUpdateResult::BadShape;
					}
				}

				if (!haveTheirs)
				{
					// A4: no outside names: the kept list goes (its extended data told when it had names)
					if (current != items.size())
					{
						if (mineCount != 0)
						{
							extFlags &= ~4;
						}

						items[current].kept = false;
					}
				}
				else
				{
					std::vector<BitmapEntry> result;

					if (mineCount == 0)
					{
						// A5a: an empty (or new) list takes the outside names, each one's flags masked to 0x10ffc3f (0x5144cc)
						result = theirs;

						for (BitmapEntry &e : result)
						{
							e.flags &= 0x10ffc3f;
						}

						extFlags &= ~4;
					}
					else
					{
						// A5b: THE MERGE (0x513f39-0x5141c6): the outside entries in the outside order, every field theirs but the flags, which take the level entry's of the same name
						std::vector<std::pair<std::string, size_t> > mineByName;

						for (size_t i = 0; i < mine.size(); i++)
						{
							mineByName.push_back(std::make_pair(Folded(mine[i].name), i));
						}

						std::stable_sort(mineByName.begin(), mineByName.end(), [](std::pair<std::string, size_t> const &a, std::pair<std::string, size_t> const &b) { return a.first < b.first; });

						std::vector<std::string> theirNames;

						for (BitmapEntry t : theirs)
						{
							t.flags &= 0xff3ffd3f;

							std::string const key = Folded(t.name);
							auto const at = std::lower_bound(mineByName.begin(), mineByName.end(), key, [](std::pair<std::string, size_t> const &a, std::string const &k) { return a.first < k; });

							if (at != mineByName.end() && at->first == key)
							{
								BitmapEntry const &m = mine[at->second];   // the FIRST level entry of that name: stable_sort kept the list's order
								int32_t mf = m.flags;

								if (((mf ^ t.flags) & 0xf0042) != 0 || ((t.flags & 2) && !(t.flags & 0x1000000) && !(mf & 0x1000000) && m.transparency != t.transparency))
								{
									mf &= 0xff3ffd7f;
								}

								t.flags = ((mf ^ t.flags) & 0x10ffc7f) ^ mf;

								if ((m.version & 0xfffff) != (t.version & 0xfffff))
								{
									t.flags &= 0xffefffff;
									extFlags &= ~4;
								}
							}
							else
							{
								t.flags &= 0xff2ffd7f;
								extFlags &= ~4;
							}

							theirNames.push_back(key);
							result.push_back(t);
						}

						std::sort(theirNames.begin(), theirNames.end());

						for (BitmapEntry const &m : mine)
						{
							if (!std::binary_search(theirNames.begin(), theirNames.end(), Folded(m.name)))
							{
								extFlags &= ~4;
							}
						}
					}

					Bytes const list = MakeChunk("BMPLSTST", ListPayload(result));

					if (current != items.size())
					{
						items[current].chunk = list;   // rewritten in its place, even when nothing changed
					}
					else
					{
						newList = list;
					}

					// A6: the version: the outside BMPNAMES's (its environment's BMNAMVERs as its load left them: a list whose head word has a version above its count sets it as it loads,
					// FUN_00419480 0x41966a), set on the SHPEXTFL's one BMNAMVER, or a new one
					std::vector<int32_t> theirVersions;

					for (size_t i = 0; i < env->size(); i++)
					{
						if (NameIs(o, (*env)[i].at, "BMNAMVER"))
						{
							if (!Payload((*env)[i], 4))
							{
								Fail(why, "the shape file's BMNAMVER is too short");
								return ShapeUpdateResult::BadFile;
							}

							theirVersions.push_back(IntAt(o, (*env)[i].at + kHeadingBytes));
						}
						else if (i == n && ((theirHead >> 16) & 0xffff) != 0)
						{
							std::vector<size_t> gone;
							size_t const left = VerDedup(theirVersions, &gone);
							std::vector<int32_t> after;

							for (size_t k = 0; k < theirVersions.size(); k++)
							{
								if (std::find(gone.begin(), gone.end(), k) == gone.end())
								{
									after.push_back(theirVersions[k]);
								}
							}

							(void)left;

							if (after.empty())
							{
								after.push_back((theirHead >> 16) & 0xffff);
							}
							else
							{
								after[0] = (theirHead >> 16) & 0xffff;
							}

							theirVersions = after;
						}
					}

					std::vector<size_t> theirGone;
					size_t const theirLeft = VerDedup(theirVersions, &theirGone);
					int32_t const v = theirLeft == theirVersions.size() ? 0 : theirVersions[theirLeft];
					std::vector<size_t> versionItems;
					std::vector<int32_t> mineVersions;

					for (size_t i = 0; i < items.size(); i++)
					{
						if (named(i, "BMNAMVER"))
						{
							if (items[i].chunk.size() < kHeadingBytes + 4)
							{
								Fail(why, "the shape's BMNAMVER is too short");
								return ShapeUpdateResult::BadShape;
							}

							versionItems.push_back(i);
							mineVersions.push_back(IntAt(items[i].chunk, kHeadingBytes));
						}
					}

					std::vector<size_t> mineGone;
					size_t const mineLeft = VerDedup(mineVersions, &mineGone);

					for (size_t const g : mineGone)
					{
						items[versionItems[g]].kept = false;
					}

					if (mineLeft != mineVersions.size())
					{
						PutIntAt(&items[versionItems[mineLeft]].chunk, kHeadingBytes, v);
					}
					else
					{
						Bytes payload;

						AddInt(&payload, v);
						newVersion = MakeChunk("BMNAMVER", payload);
					}
				}

				if (extData != items.size())
				{
					PutIntAt(&items[extData].chunk, kHeadingBytes, extFlags);
				}
				else if (extIsNew)
				{
					Bytes payload(52, 0);

					PutIntAt(&payload, 0, extFlags);
					newExtData = MakeChunk("BMNAMEXT", payload);
				}
			}

			// R: every RIFFNAME goes; with an outside environment, one is made from its first RIFFNAME's name, at the end (0x5146f0-0x514850)
			if (env)
			{
				std::string name;
				bool bad = false;

				if (OutsideRiffName(o, *env, &name, &bad))
				{
					newName = NameChunk(name);
				}
				else if (bad)
				{
					Fail(why, "the shape file's RIFFNAME has no end");
					return ShapeUpdateResult::BadFile;
				}
			}

			// DECIDED (an inteng bug, fixed as a named difference): inteng deletes the RIFFNAMEs even when it makes none, which leaves a shape with no EXTOBJNM NAMELESS: the game finds an outside
			// shape by that name, and its loader refuses a nameless one (avpx chunk_shape.cpp, Shape_External_File_Chunk::post_input_processing). Then they stay as they were.
			bool hasObjectName = false;

			for (size_t i = 0; i < items.size(); i++)
			{
				hasObjectName = hasObjectName || named(i, "EXTOBJNM");
			}

			if (!newName.empty() || hasObjectName)
			{
				for (size_t i = 0; i < items.size(); i++)
				{
					if (named(i, "RIFFNAME"))
					{
						items[i].kept = false;
					}
				}
			}

			newExt->assign(s.begin() + (std::ptrdiff_t)extfl.at, s.begin() + (std::ptrdiff_t)(extfl.at + kHeadingBytes));

			for (Item const &item : items)
			{
				if (item.kept)
				{
					newExt->insert(newExt->end(), item.chunk.begin(), item.chunk.end());
				}
			}

			for (Bytes const *const add : { &newList, &newExtData, &newVersion, &newName })
			{
				newExt->insert(newExt->end(), add->begin(), add->end());
			}

			PutIntAt(newExt, 8, (int32_t)newExt->size());
			return ShapeUpdateResult::Done;
		}
	}

	char const *ShapeUpdateWords(ShapeUpdateResult result)
	{
		switch (result)
		{
		case ShapeUpdateResult::Done: return "the shape was brought up to date from its own file";
		case ShapeUpdateResult::NameOnly: return "the shape's file holds no single shape: only its file name was taken";
		case ShapeUpdateResult::NotOutside: return "the shape is not an outside shape";
		case ShapeUpdateResult::BadShape: return "the level's shape can't be read";
		case ShapeUpdateResult::BadFile: return "the shape's file isn't one this can read";
		case ShapeUpdateResult::NoShape: return "the shape's file holds no shape of that name";
		case ShapeUpdateResult::BadScale: return "the scale is not a real number above 0 and at most 1,000,000, or a point would be too far out";
		}

		return "unknown answer";
	}

	bool ShapeUpdate_UsedBy(std::vector<char> const &shapeChunk, std::string const &objectName)
	{
		std::vector<Span> parts;
		size_t size = 0;
		std::vector<std::string> names;

		if (objectName.empty() || shapeChunk.size() < kHeadingBytes || !NameIs(shapeChunk, 0, "REBSHAPE") || !ChunkSizeAt(shapeChunk, 0, shapeChunk.size(), &size) ||
			!Children(shapeChunk, kHeadingBytes, size, &parts))
		{
			return false;
		}

		size_t const head = FirstNamed(shapeChunk, parts, "SHPHEAD1");

		if (head == parts.size() || !Payload(parts[head], kHeadPayload) || !HeaderNames(shapeChunk, parts[head], &names))
		{
			return false;
		}

		for (std::string const &one : names)
		{
			if (one == objectName)
			{
				return true;
			}
		}

		return false;
	}

	bool ShapeUpdate_FileName(std::vector<char> const &shapeChunk, std::string *name)
	{
		LevelShape ls;

		if (ReadLevelShape(shapeChunk, &ls, nullptr) != ShapeUpdateResult::Done)
		{
			return false;
		}

		// FUN_00567e40: the part after the last ':', '/' or '\'
		std::string last = ls.fileName;
		size_t const cut = last.find_last_of(":/\\");

		if (cut != std::string::npos)
		{
			last = last.substr(cut + 1);
		}

		if (last.size() > 4 && _stricmp(last.c_str() + last.size() - 4, ".rif") == 0)
		{
			last.resize(last.size() - 4);
		}

		if (last.empty())
		{
			return false;
		}

		*name = last;
		return true;
	}

	ShapeUpdateResult ShapeUpdate_Build(std::vector<char> const &shapeChunk, std::vector<char> const &outside, bool forced, int32_t levelUsers, std::vector<char> *newShape, std::string *why,
		double *before, double *after)
	{
		newShape->clear();

		Bytes const &s = shapeChunk;
		Bytes const &o = outside;
		LevelShape ls;
		ShapeUpdateResult const read = ReadLevelShape(s, &ls, why);

		if (read != ShapeUpdateResult::Done)
		{
			return read;
		}

		// the level shape's own SHPCENTR: one at most (NAMED: inteng's lookup_single_child stops the program on two, 0x50fce0; refused here)
		if (CountNamed(s, ls.parts, "SHPCENTR") > 1)
		{
			Fail(why, "the level's shape holds two SHPCENTR");
			return ShapeUpdateResult::BadShape;
		}

		// THE OUTSIDE FILE: a whole REBINFF2, its environment (one at most: NAMED, two stop inteng's lookup_single_child), its objects and its shapes
		std::vector<Span> top;
		size_t total = 0;

		if (o.size() < kHeadingBytes || !NameIs(o, 0, "REBINFF2") || !ChunkSizeAt(o, 0, o.size(), &total) || !Children(o, kHeadingBytes, total, &top))
		{
			Fail(why, "the shape's file is not a rif file whose chunks can be walked");
			return ShapeUpdateResult::BadFile;
		}

		if (CountNamed(o, top, "REBENVDT") > 1)
		{
			Fail(why, "the shape's file holds two environments (REBENVDT)");
			return ShapeUpdateResult::BadFile;
		}

		size_t const envAt = FirstNamed(o, top, "REBENVDT");
		std::vector<Span> env;

		if (envAt != top.size() && !Children(o, top[envAt].at + kHeadingBytes, top[envAt].at + top[envAt].size, &env))
		{
			Fail(why, "the shape file's environment data can't be walked");
			return ShapeUpdateResult::BadFile;
		}

		std::vector<Span> const *const envOrNone = envAt != top.size() ? &env : nullptr;

		// THE SHAPE (0x513151-0x51346c)
		size_t chosen = top.size();

		if (ls.extobjnm != ls.extParts.size())
		{
			// An EXTOBJNM (the chunk being there decides, 0x513166: an empty name takes this path too): the FIRST RBOBJECT whose OBJHEAD1 name (+60) is its name, compared with strcmp
			// (0x5131ae: capitals count), then the shape tied to that object as FUN_004bf1d0 ties them: the first REBSHAPE whose SHPHEAD1 number (+20) is the object's shape number (+56).
			// DECIDED fidelity-6: the shape was found by its header's list of object names until 11-17.
			bool found = false;
			int32_t shapeNumber = 0;

			for (Span const &t : top)
			{
				std::vector<Span> parts;
				std::string objectName;

				if (!NameIs(o, t.at, "RBOBJECT") || !Children(o, t.at + kHeadingBytes, t.at + t.size, &parts))
				{
					continue;
				}

				size_t const head = FirstNamed(o, parts, "OBJHEAD1");

				if (head == parts.size())
				{
					continue;
				}

				if (CountNamed(o, parts, "OBJHEAD1") > 1 || !Payload(parts[head], 61) || !TextIn(o, parts[head].at + kHeadingBytes + 60, parts[head].at + parts[head].size, &objectName))
				{
					Fail(why, "an object of the shape's file has a header that can't be read");
					return ShapeUpdateResult::BadFile;
				}

				if (objectName == ls.objectName)
				{
					shapeNumber = IntAt(o, parts[head].at + kHeadingBytes + 56);
					found = true;
					break;
				}
			}

			if (!found)
			{
				Fail(why, "no object of the shape's file has the name the level's shape gives (EXTOBJNM)");
				return ShapeUpdateResult::NoShape;   // nothing changes, as in inteng (0x513212)
			}

			for (size_t i = 0; i < top.size() && chosen == top.size(); i++)
			{
				std::vector<Span> parts;

				if (NameIs(o, top[i].at, "REBSHAPE") && Children(o, top[i].at + kHeadingBytes, top[i].at + top[i].size, &parts))
				{
					size_t const head = FirstNamed(o, parts, "SHPHEAD1");

					if (head != parts.size() && Payload(parts[head], 24) && IntAt(o, parts[head].at + kHeadingBytes + 20) == shapeNumber)
					{
						chosen = i;
					}
				}
			}

			if (chosen == top.size())
			{
				// NAMED: inteng takes the object's shape with no test and reads through a null one (0x51347f); ours changes nothing
				Fail(why, "the object the level's shape names has no shape in its file");
				return ShapeUpdateResult::NoShape;
			}
		}
		else
		{
			std::vector<size_t> shapes;

			for (size_t i = 0; i < top.size(); i++)
			{
				if (NameIs(o, top[i].at, "REBSHAPE"))
				{
					shapes.push_back(i);
				}
			}

			if (shapes.size() == 1)
			{
				chosen = shapes[0];
			}
			else
			{
				// 0x51328e-0x5133f1: only the SHPEXTFL's RIFFNAME. Every one goes, and one is MADE from the file environment's first RIFFNAME's name (none when it has none); with no environment
				// nothing changes. The rescale is untouched: Take scale's new / old is 1.0.
				if (envAt == top.size())
				{
					Fail(why, "the file holds no single shape and no environment");
					return ShapeUpdateResult::NoShape;
				}

				std::string name;
				bool bad = false;
				bool const haveName = OutsideRiffName(o, env, &name, &bad);

				if (bad)
				{
					Fail(why, "the shape file's RIFFNAME has no end");
					return ShapeUpdateResult::BadFile;
				}

				Span const ext = ls.parts[ls.extfl];
				Bytes newExt(s.begin() + (std::ptrdiff_t)ext.at, s.begin() + (std::ptrdiff_t)(ext.at + kHeadingBytes));

				for (Span const &part : ls.extParts)
				{
					// DECIDED (the named fix above, NewExternal): with no RIFFNAME to make, the old ones stay, or the shape would be nameless
					if (!haveName || !NameIs(s, part.at, "RIFFNAME"))
					{
						AddCopy(&newExt, s, part);
					}
				}

				if (haveName)
				{
					Bytes const chunk = NameChunk(name);

					newExt.insert(newExt.end(), chunk.begin(), chunk.end());
				}

				PutIntAt(&newExt, 8, (int32_t)newExt.size());

				Bytes shape(s.begin(), s.begin() + (std::ptrdiff_t)kHeadingBytes);

				for (size_t i = 0; i < ls.parts.size(); i++)
				{
					if (i == ls.extfl)
					{
						shape.insert(shape.end(), newExt.begin(), newExt.end());
					}
					else
					{
						AddCopy(&shape, s, ls.parts[i]);
					}
				}

				PutIntAt(&shape, 8, (int32_t)shape.size());
				*newShape = shape;

				if (before)
				{
					*before = ls.rescale;
				}

				if (after)
				{
					*after = ls.rescale;
				}

				return ShapeUpdateResult::NameOnly;
			}
		}

		// THE SCALE (0x51346c-0x51364b): forced, 1.0, then the file's ENVSDSCL; not forced, the file's ENVSDSCL when no object of the level is tied to the shape (its header's associated
		// objects, FUN_0050e9f0, as FUN_004bf1d0 ties them: ShapeUpdate_LevelUsers; DECIDED fidelity-7, it was the header's stored name count); else the shape's own. NAMED (OP-141): the
		// rescale must be a real number above 0 and at most 1,000,000.
		double rescale = forced ? 1.0 : ls.rescale;
		size_t const scaleAt = FirstNamed(o, env, "ENVSDSCL");

		if ((forced || levelUsers == 0) && envAt != top.size() && scaleAt != env.size())
		{
			if (!Payload(env[scaleAt], 8))
			{
				Fail(why, "the file's ENVSDSCL is too short");
				return ShapeUpdateResult::BadFile;
			}

			rescale = DoubleAt(o, env[scaleAt].at + kHeadingBytes);
		}

		if (!IsReal(rescale) || rescale <= 0.0 || rescale > kMostRescale)
		{
			Fail(why, "the scale is not a real number above 0 and at most 1,000,000");
			return ShapeUpdateResult::BadScale;
		}

		// THE OUTSIDE SHAPE'S PARTS: one header, each data kind and each single extra kind at most once, no CONSHAPE (NAMED: inteng's lookup_single_child stops the program on a second, and its
		// loader on any CONSHAPE under a shape, 0x50e34f; such a file is refused here)
		std::vector<Span> oParts;
		char const *const kSingleExtrasFirst[] = { "SHPZSPDT", "SHPMRGDT", "SHPMORPH", "TEXTANIM", "SHPPCINF" };
		char const *const kSingleExtrasLast[] = { "PNOTINBB", "ANSHCEN2", "ASALTTEX" };

		if (!Children(o, top[chosen].at + kHeadingBytes, top[chosen].at + top[chosen].size, &oParts) || CountNamed(o, oParts, "SHPHEAD1") != 1 || CountNamed(o, oParts, "SHPFRAGS") > 1 ||
			CountNamed(o, oParts, "CONSHAPE") > 0)
		{
			Fail(why, "the file's shape can't be walked, or holds no single header, two SHPFRAGS or a CONSHAPE");
			return ShapeUpdateResult::BadFile;
		}

		for (char const *const kind : kDataChunks)
		{
			if (CountNamed(o, oParts, kind) > 1)
			{
				Fail(why, "the file's shape holds one of its data chunks twice");
				return ShapeUpdateResult::BadFile;
			}
		}

		for (char const *const *kinds : { kSingleExtrasFirst, kSingleExtrasLast })
		{
			size_t const count = kinds == kSingleExtrasFirst ? sizeof(kSingleExtrasFirst) / sizeof(kSingleExtrasFirst[0]) : sizeof(kSingleExtrasLast) / sizeof(kSingleExtrasLast[0]);

			for (size_t k = 0; k < count; k++)
			{
				if (CountNamed(o, oParts, kinds[k]) > 1)
				{
					Fail(why, "the file's shape holds one of its extra chunks twice");
					return ShapeUpdateResult::BadFile;
				}
			}
		}

		size_t const oHead = FirstNamed(o, oParts, "SHPHEAD1");

		if (!Payload(oParts[oHead], kHeadPayload))
		{
			Fail(why, "the file's shape header is too short");
			return ShapeUpdateResult::BadFile;
		}

		size_t const oHeadPayload = oParts[oHead].at + kHeadingBytes;
		size_t const oRaw = FirstNamed(o, oParts, "SHPRAWVT");
		int32_t chunkPoints = 0, chunkPolygons = 0;

		// the counts the new header gets: the copied chunks' (as the game's loader takes them and inteng's save writes them), never the hostile header's own (hostile-input-2 / -3)
		if (!ChunkCounts(o, oParts, &chunkPoints, &chunkPolygons))
		{
			Fail(why, "the file shape's SHPRAWVT or SHPPOLYS isn't whole records");
			return ShapeUpdateResult::BadFile;
		}

		// OP-138a: the points, the header's box and radius x the rescale (FUN_00430210: no 1.0 short-cut)
		Bytes pointsPayload;
		std::vector<int32_t> points;

		if (oRaw != oParts.size() && !ScaledPoints(o, oParts[oRaw], rescale, &pointsPayload, &points))
		{
			Fail(why, "a point of the file's shape would be farther than 2^24, or its SHPRAWVT isn't whole points");
			return ShapeUpdateResult::BadScale;
		}

		Bytes head(s.begin() + (std::ptrdiff_t)ls.parts[ls.head].at, s.begin() + (std::ptrdiff_t)(ls.parts[ls.head].at + ls.parts[ls.head].size));

		if (!ScaledBox(o, oHeadPayload + 24, rescale, &head, kHeadingBytes + 24))
		{
			Fail(why, "the file shape's radius or box would be too far out");
			return ShapeUpdateResult::BadScale;
		}

		PutIntAt(&head, kHeadingBytes + 24, chunkPoints);
		PutIntAt(&head, kHeadingBytes + 28, chunkPolygons);

		int32_t box[6], centre[3];
		float centreRadius = 0.0f;

		for (int k = 0; k < 6; k++)
		{
			box[k] = IntAt(head, kHeadingBytes + 36 + (size_t)k * 4);
		}

		CentreAndRadius(box, points, centre, &centreRadius);

		Bytes const centreChunk = CentreChunk(centre, centreRadius);

		// THE SHPEXTFL (OP-138c..e and the RIFFNAME, 0x513c8e-0x514850): NewExternal. DECIDED fidelity-1 / -2 / -3 / -4 / -11: until 11-17 the RIFFNAME was kept, a list was kept when the outside
		// file gave none or the same names, a changed list took the outside flags masked to 0x10ffc3f only, and BMNAMVER / BMNAMEXT were never touched.
		Bytes newExt;
		ShapeUpdateResult const external = NewExternal(s, ls.parts[ls.extfl], ls.extParts, ls.extfn, rescale, IntAt(o, oHeadPayload + 60), o, envOrNone, &newExt, why);

		if (external != ShapeUpdateResult::Done)
		{
			return external;
		}

		// THE NEW SHAPE (FUN_0050f570, FUN_0050eb20, 0x5136a6-0x513c25): the level's own parts in their order without the six data kinds and the twelve extra kinds (the header, SHPEXTFL and
		// SHPCENTR in their places); then the file's data chunks, each one it has (an empty one too: inteng makes the chunk whenever the list is there); a new SHPCENTR when the level shape had
		// none; then the file's extras in inteng's order: one each of SHPZSPDT, SHPMRGDT, SHPMORPH, TEXTANIM, SHPPCINF and SHPFRAGS (each SUBSHAPE through FUN_00516570, at every rescale:
		// DECIDED fidelity-5), every ANIMSEQU in the file's order (not rescaled), one each of PNOTINBB, ANSHCEN2 and ASALTTEX. SHPPRPRO is never copied back.
		Bytes shape(s.begin(), s.begin() + (std::ptrdiff_t)kHeadingBytes);
		bool centrePut = false;

		for (size_t i = 0; i < ls.parts.size(); i++)
		{
			Span const &part = ls.parts[i];

			if (i == ls.head)
			{
				shape.insert(shape.end(), head.begin(), head.end());
			}
			else if (i == ls.extfl)
			{
				shape.insert(shape.end(), newExt.begin(), newExt.end());
			}
			else if (NameIs(s, part.at, "SHPCENTR"))
			{
				shape.insert(shape.end(), centreChunk.begin(), centreChunk.end());
				centrePut = true;
			}
			else if (!IsOneOf(s, part.at, kDataChunks, kDataCount) && !IsOneOf(s, part.at, kDeletedExtras, kDeletedCount))
			{
				AddCopy(&shape, s, part);
			}
		}

		for (char const *const wanted : kDataChunks)
		{
			size_t const at = FirstNamed(o, oParts, wanted);

			if (at == oParts.size())
			{
				continue;
			}

			if (strcmp(wanted, "SHPRAWVT") == 0)
			{
				Bytes const raw = MakeChunk("SHPRAWVT", pointsPayload);

				shape.insert(shape.end(), raw.begin(), raw.end());
			}
			else
			{
				AddCopy(&shape, o, oParts[at]);
			}
		}

		if (!centrePut)
		{
			shape.insert(shape.end(), centreChunk.begin(), centreChunk.end());
		}

		for (char const *const kind : kSingleExtrasFirst)
		{
			size_t const at = FirstNamed(o, oParts, kind);

			if (at != oParts.size())
			{
				AddCopy(&shape, o, oParts[at]);
			}
		}

		size_t const fragsAt = FirstNamed(o, oParts, "SHPFRAGS");

		if (fragsAt != oParts.size())
		{
			Span const frag = oParts[fragsAt];
			std::vector<Span> inside;

			if (!Children(o, frag.at + kHeadingBytes, frag.at + frag.size, &inside))
			{
				Fail(why, "the file shape's SHPFRAGS can't be walked");
				return ShapeUpdateResult::BadFile;
			}

			Bytes frags(o.begin() + (std::ptrdiff_t)frag.at, o.begin() + (std::ptrdiff_t)(frag.at + kHeadingBytes));

			for (Span const &one : inside)
			{
				if (NameIs(o, one.at, "SUBSHAPE"))
				{
					Bytes sub;

					if (!UpdatedSubShape(o, one, rescale, &sub))
					{
						Fail(why, "a fragment of the file's shape can't be updated (too far out, its chunks don't fit, or one of them is there twice)");
						return ShapeUpdateResult::BadScale;
					}

					frags.insert(frags.end(), sub.begin(), sub.end());
				}
				else
				{
					AddCopy(&frags, o, one);
				}
			}

			if (frags.size() > (size_t)kMostChunkBytes)
			{
				Fail(why, "the shape's fragments would be too big");
				return ShapeUpdateResult::BadFile;
			}

			PutIntAt(&frags, 8, (int32_t)frags.size());
			shape.insert(shape.end(), frags.begin(), frags.end());
		}

		for (Span const &part : oParts)
		{
			if (NameIs(o, part.at, "ANIMSEQU"))
			{
				AddCopy(&shape, o, part);
			}
		}

		for (char const *const kind : kSingleExtrasLast)
		{
			size_t const at = FirstNamed(o, oParts, kind);

			if (at != oParts.size())
			{
				AddCopy(&shape, o, oParts[at]);
			}
		}

		if (shape.size() > (size_t)kMostChunkBytes)
		{
			Fail(why, "the shape would be too big");
			return ShapeUpdateResult::BadFile;
		}

		PutIntAt(&shape, 8, (int32_t)shape.size());

		if (before)
		{
			*before = ls.rescale;
		}

		if (after)
		{
			*after = rescale;
		}

		*newShape = shape;
		return ShapeUpdateResult::Done;
	}

	ShapeUpdateResult ShapeUpdate_Build(std::vector<char> const &shapeChunk, std::vector<char> const &outside, bool forced, std::vector<char> *newShape, std::string *why,
		double *before, double *after)
	{
		// the shape alone, its level not at hand: its header's object count (+64) stands for the objects of the level tied to it
		LevelShape ls;
		int32_t const users = ReadLevelShape(shapeChunk, &ls, nullptr) == ShapeUpdateResult::Done ? ls.objectCount : 0;

		return ShapeUpdate_Build(shapeChunk, outside, forced, users, newShape, why, before, after);
	}
}
