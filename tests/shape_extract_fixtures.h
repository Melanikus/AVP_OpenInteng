// shape_extract_fixtures.h: the maps the Option C tests build (host_shape_extract_test, host_shape_extract_write_test): a shape file laid out as the old
// tools' package lays one out, a level holding shapes, and an object placing one. Moved here from host_shape_extract_test.cpp on 2026-11-23 by Melanikus
// (Frost) for AVP_OpenInteng, so both tests build the very same bytes. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#pragma once

#include <stdint.h>
#include <string.h>

#include <vector>

#include "shape_import.h"

using namespace host::shape_chunks;

static Bytes Filled(size_t n, int seed)
{
	Bytes b(n);

	for (size_t i = 0; i < n; i++)
	{
		b[i] = (char)((i * 7 + (size_t)seed * 13) & 0xff);
	}

	return b;
}

static void Add(Bytes *out, Bytes const &chunk)
{
	out->insert(out->end(), chunk.begin(), chunk.end());
}

// A shape file as the package lays one out: RIFVERIN, the shape (header with flags 0, a lock user, its own number, version 3 at +60, one object), the
// object placing it, the environment data (ENDTHEAD, RIFFNAME, BMPNAMES, BMNAMEXT, BMNAMVER)
static Bytes PackageFile()
{
	Bytes head;
	AddInt(&head, 0);                                  // flags
	Bytes lock(16, 0);
	memcpy(lock.data(), "someone", 7);                // the old tools' last saver
	head.insert(head.end(), lock.begin(), lock.end());
	AddInt(&head, 77);                                 // its own number
	Bytes counts = Filled(36, 1);                      // +24 to +59: counts, sizes
	head.insert(head.end(), counts.begin(), counts.end());
	AddInt(&head, 3);                                  // +60: its version
	AddInt(&head, 1);                                  // +64: one object uses it
	Bytes objectName(8, 0);
	memcpy(objectName.data(), "medbox", 6);
	head.insert(head.end(), objectName.begin(), objectName.end());

	Bytes shape;
	Add(&shape, MakeChunk("SHPHEAD1", head));
	Add(&shape, MakeChunk("SHPMRGDT", Filled(16, 2)));
	Add(&shape, MakeChunk("SHPCENTR", Filled(16, 3)));
	Add(&shape, MakeChunk("SHPPCINF", Filled(8, 4)));
	Add(&shape, MakeChunk("SHPRAWVT", Filled(48, 5)));
	Add(&shape, MakeChunk("SHPVNORM", Filled(48, 6)));
	Add(&shape, MakeChunk("SHPPNORM", Filled(24, 7)));
	Add(&shape, MakeChunk("SHPPOLYS", Filled(40, 8)));
	Add(&shape, MakeChunk("SHPUVCRD", Filled(32, 9)));

	// the name and the bitmap list as the package has them: each name ended by a 0, then 0s to a whole 4
	Bytes riffName(8, 0);
	memcpy(riffName.data(), "Pack", 4);
	Bytes bitmaps;
	AddInt(&bitmaps, 1);                               // one picture
	Bytes numbers = Filled(20, 12);
	PutIntAt(&numbers, 0, 0x10c);                      // its flags: the default (bitmap_names.hpp), "priority and transparency are valid" (0x100) among them
	bitmaps.insert(bitmaps.end(), numbers.begin(), numbers.end());
	Bytes picture(12, 0);
	memcpy(picture.data(), "medbox.bmp", 10);          // 10 letters, its end 0, a pad 0
	bitmaps.insert(bitmaps.end(), picture.begin(), picture.end());

	Bytes env;
	Add(&env, MakeChunk("ENDTHEAD", Filled(24, 10)));
	Add(&env, MakeChunk("RIFFNAME", riffName));
	Add(&env, MakeChunk("BMPNAMES", bitmaps));
	Add(&env, MakeChunk("BMNAMEXT", Filled(20, 13)));
	Add(&env, MakeChunk("BMNAMVER", Filled(8, 14)));

	Bytes whole, zero;
	AddInt(&zero, 0);
	Add(&whole, MakeChunk("RIFVERIN", zero));
	Add(&whole, MakeChunk("REBSHAPE", shape));
	Add(&whole, MakeChunk("RBOBJECT", MakeChunk("OBJHEAD1", Filled(32, 15))));
	Add(&whole, MakeChunk("REBENVDT", env));
	return MakeChunk("REBINFF2", whole);
}

// A level holding the given shape chunks, each with a SHPFRAGS of the level's own added after its header (as stock levels have)
static Bytes LevelOf(std::vector<Bytes> const &shapes)
{
	Bytes whole;
	Add(&whole, MakeChunk("ENVRDATA", Filled(12, 20)));

	for (Bytes const &s : shapes)
	{
		Add(&whole, s);
	}

	return MakeChunk("REBINFF2", whole);
}

// An object placing the shape numbered `shapeNumber` (OBJHEAD1 as chunk_object.cpp reads it: flags, the lock user, the place as three whole numbers
// at +20, the turn as four floats at +32, the object's number, its version, the shape's number at +56, then the name), moved to `place` and turned by
// the quaternion `turn` (x, y, z, w), as a stock level's objects are
static Bytes PlacingObject(int32_t shapeNumber, int32_t const place[3], float const turn[4])
{
	Bytes head;
	AddInt(&head, 0xFC00);                             // flags: placed, the three game modes, the platform and PC load
	Bytes lock(16, 0);
	memcpy(lock.data(), "Player", 6);
	head.insert(head.end(), lock.begin(), lock.end());

	for (int i = 0; i < 3; i++)
	{
		AddInt(&head, place[i]);
	}

	for (int i = 0; i < 4; i++)
	{
		int32_t asInt;
		memcpy(&asInt, &turn[i], 4);
		AddInt(&head, asInt);
	}

	AddInt(&head, 5);                                  // the object's number
	AddInt(&head, 1);                                  // its version
	AddInt(&head, shapeNumber);
	Bytes objectName(12, 0);
	memcpy(objectName.data(), "medbox010", 9);
	head.insert(head.end(), objectName.begin(), objectName.end());
	return MakeChunk("RBOBJECT", MakeChunk("OBJHEAD1", head));
}

// The same level shape as a PART of a file holds it: its SHPEXTFL with an EXTOBJNM after the SHPEXTFN (chunk_shape.cpp: a number, then the object's
// name in that file, ended by a 0 and padded to 4), as 277 of the 1,160 stock outside shapes are held (Dietrich's measure, 11-23)
static Bytes AsAPart(Bytes const &shape, char const *objectName)
{
	std::vector<Span> parts;
	Children(shape, kHeadingBytes, shape.size(), &parts);
	Bytes out(shape.begin(), shape.begin() + (std::ptrdiff_t)kHeadingBytes);

	for (Span const &part : parts)
	{
		if (!NameIs(shape, part.at, "SHPEXTFL"))
		{
			AddCopy(&out, shape, part);
			continue;
		}

		std::vector<Span> in;
		Children(shape, part.at + kHeadingBytes, part.at + part.size, &in);
		Bytes inside;

		for (Span const &one : in)
		{
			AddCopy(&inside, shape, one);

			if (NameIs(shape, one.at, "SHPEXTFN"))
			{
				Bytes name;
				AddInt(&name, 0);
				name.insert(name.end(), objectName, objectName + strlen(objectName));
				name.push_back(0);
				PadToFour(&name);
				Add(&inside, MakeChunk("EXTOBJNM", name));
			}
		}

		Add(&out, MakeChunk("SHPEXTFL", inside));
	}

	PutIntAt(&out, 8, (int32_t)out.size());
	return out;
}
