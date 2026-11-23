// The test for a new picture named in a level's BMPNAMES list (src/model/bitmap_add.h: the RIM import's step 7). Written 2026-11-23 by Melanikus (Vasquez) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
// Each level here is made by hand, chunk by chunk, and what comes back is compared with the level made by hand WITH the entry, byte for byte: so every size,
// the entry's every number, its name's zeros, and every byte that mustn't move, are checked at once.
//   (1) a list of two (indices 1 and 5) and BMNAMVER 7 after it: index 6, the entry flags 0x0100010C, version 7 << 8, priority 6, size word 64 << 16 | 128,
//       BMNAMVER 8; the chunks before and after REBENVDT and inside it as they were
//   (2) BMNAMVER before the list: the same, BMNAMVER in its own place
//   (3) no BMNAMVER: version 0, and a BMNAMVER holding 1 made at REBENVDT's end
//   (4) the name's zeros: a name of 16 letters takes 20 bytes (four zeros), one of 15 takes 16 (one zero)
//   (5) added to what it gave back: the next index (7) and BMNAMVER 9
//   (6) the edges taken: the largest index 4094 gives 4095; BMNAMVER 4095 gives version 4095 << 8
//   (7) refused, nothing given back (index -1, the words saying why): the name there already in other capitals; no REBENVDT; no BMPNAMES; two BMPNAMES;
//       two BMNAMVER; a count word's top bits set; the largest index 4095; BMNAMVER 4096; a size past the end; an entry name with no end; a count of three
//       with two entries; the file's own size wrong; no bytes; a name going up a folder; a side of 0, or of 65536; no room for the answer
// RED FIRST: on a stand-in that refuses everything, (1) to (6) fail.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "bitmap_add.h"

using namespace model;

typedef std::vector<char> Bytes;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static void AddInt(Bytes *bytes, int32_t value)
{
	char raw[4];

	memcpy(raw, &value, 4);
	bytes->insert(bytes->end(), raw, raw + 4);
}

static Bytes Join(std::vector<Bytes> const &parts)
{
	Bytes all;

	for (Bytes const &part : parts)
	{
		all.insert(all.end(), part.begin(), part.end());
	}

	return all;
}

// A whole chunk: its 8-letter name, its size (these 12 bytes too), then the payload
static Bytes Chunk(char const *name, Bytes const &payload)
{
	Bytes chunk(name, name + 8);

	AddInt(&chunk, (int32_t)(12 + payload.size()));
	chunk.insert(chunk.end(), payload.begin(), payload.end());
	return chunk;
}

static Bytes Number(int32_t value)
{
	Bytes bytes;

	AddInt(&bytes, value);
	return bytes;
}

// One entry as the game writes it: five numbers, the name, its zero and zeros to a multiple of 4 (written out here on its own, not taken from the code tested)
static Bytes Entry(int32_t flags, int32_t index, int32_t d1, int32_t priority, int32_t size, std::string const &name)
{
	Bytes entry;

	AddInt(&entry, flags);
	AddInt(&entry, index);
	AddInt(&entry, d1);
	AddInt(&entry, priority);
	AddInt(&entry, size);
	entry.insert(entry.end(), name.begin(), name.end());

	size_t const zeros = name.size() % 4 == 0 ? 4 : 4 - name.size() % 4;

	entry.insert(entry.end(), zeros, 0);
	return entry;
}

static Bytes List(int32_t word, std::vector<Bytes> const &entries)
{
	return Chunk("BMPNAMES", Join({ Number(word), Join(entries) }));
}

static Bytes Level(std::vector<Bytes> const &top)
{
	return Chunk("REBINFF2", Join(top));
}

int main()
{
	std::string const name = "Custom\\Test\\Wall01.RIM";   // 22 letters: two zeros
	Bytes const stock1 = Entry(0x0100010C, 1, (3 << 8) | 1, 6, (128 << 16) | 128, "Envrnmts\\Test\\Floor.RIM");
	Bytes const stock5 = Entry(0x0100010C, 5, 6 << 8, 6, (121 << 16) | 128, "Envrnmts\\Test\\Frame & Panels.RIM");
	Bytes const before = Chunk("OTHERCHK", { 1, 2, 3, 4 });
	Bytes const acoustics = Chunk("ENVACOUS", { 9, 9, 9, 9, 9, 9, 9, 9 });
	Bytes const after = Chunk("RBOBJECT", Chunk("OBJHEAD1", Bytes(64, 'o')));
	Bytes const wanted = Entry(0x0100010C, 6, 7 << 8, 6, (64 << 16) | 128, name);
	Bytes out;
	int32_t index = 0;
	std::string why;

	// ---- (1) the list, then BMNAMVER
	puts("Added:");
	Bytes const level = Level({ before, Chunk("REBENVDT", Join({ List(2, { stock1, stock5 }), Chunk("BMNAMVER", Number(7)), acoustics })), after });
	Bytes const levelWith = Level({ before, Chunk("REBENVDT", Join({ List(3, { stock1, stock5, wanted }), Chunk("BMNAMVER", Number(8)), acoustics })), after });
	BitmapAddResult const added = BitmapAdd_ToImage(level, name, 128, 64, &out, &index, &why);

	Check(added == BitmapAddResult::Added && index == 6, std::string("(1) added, index 6 (") + BitmapAddResultWords(added) + ", " + std::to_string(index) + " " + why + ")");
	Check(out == levelWith, "(1) the level's bytes are the hand-made level with the entry, byte for byte (" + std::to_string(out.size()) + " of " +
		std::to_string(levelWith.size()) + ")");

	// ---- (2) BMNAMVER before the list
	Bytes const versionFirst = Level({ before, Chunk("REBENVDT", Join({ Chunk("BMNAMVER", Number(7)), List(2, { stock1, stock5 }), acoustics })), after });
	Bytes const versionFirstWith = Level({ before, Chunk("REBENVDT", Join({ Chunk("BMNAMVER", Number(8)), List(3, { stock1, stock5, wanted }), acoustics })), after });
	BitmapAddResult const second = BitmapAdd_ToImage(versionFirst, name, 128, 64, &out, &index, &why);

	Check(second == BitmapAddResult::Added && out == versionFirstWith, "(2) BMNAMVER before the list: the same, byte for byte " + why);

	// ---- (3) no BMNAMVER: version 0, one made at REBENVDT's end holding 1
	Bytes const noVersion = Level({ before, Chunk("REBENVDT", Join({ List(2, { stock1, stock5 }), acoustics })), after });
	Bytes const noVersionWith = Level({ before, Chunk("REBENVDT", Join({ List(3, { stock1, stock5, Entry(0x0100010C, 6, 0, 6, (64 << 16) | 128, name) }), acoustics,
		Chunk("BMNAMVER", Number(1)) })), after });
	BitmapAddResult const third = BitmapAdd_ToImage(noVersion, name, 128, 64, &out, &index, &why);

	Check(third == BitmapAddResult::Added && out == noVersionWith, "(3) no BMNAMVER: version 0, and BMNAMVER 1 made at REBENVDT's end, byte for byte " + why);

	// ---- (4) the name's zeros
	BitmapAdd_ToImage(level, "Custom\\Ab\\Cdefg.RIM", 128, 128, &out, &index, &why);   // 19 letters
	size_t const grew19 = out.size() - level.size();
	BitmapAdd_ToImage(level, "Custom\\Ab\\Cdefgh.RIM", 128, 128, &out, &index, &why);  // 20 letters
	size_t const grew20 = out.size() - level.size();

	Check(grew19 == 20 + 20 && grew20 == 20 + 24, "(4) 19 letters take 20 bytes (one zero), 20 letters take 24 (four zeros): grew " + std::to_string(grew19) + " and " +
		std::to_string(grew20));

	// ---- (5) added to what it gave back
	BitmapAdd_ToImage(level, name, 128, 64, &out, &index, &why);
	Bytes const once = out;
	std::string const other = "Custom\\Test\\Wall02.RIM";
	BitmapAddResult const again = BitmapAdd_ToImage(once, other, 256, 256, &out, &index, &why);
	Bytes const twiceWith = Level({ before, Chunk("REBENVDT", Join({ List(4, { stock1, stock5, wanted, Entry(0x0100010C, 7, 8 << 8, 6, (256 << 16) | 256, other) }),
		Chunk("BMNAMVER", Number(9)), acoustics })), after });

	Check(again == BitmapAddResult::Added && index == 7 && out == twiceWith, "(5) a second one: index 7, version 8 << 8, BMNAMVER 9, byte for byte " + why);

	// ---- (6) the edges taken
	Bytes const nearFull = Level({ Chunk("REBENVDT", Join({ List(1, { Entry(0x0100010C, 4094, 0, 6, 0, "a.RIM") }), Chunk("BMNAMVER", Number(4095)) })) });
	BitmapAddResult const edge = BitmapAdd_ToImage(nearFull, name, 128, 128, &out, &index, &why);
	Bytes const nearFullWith = Level({ Chunk("REBENVDT", Join({ List(2, { Entry(0x0100010C, 4094, 0, 6, 0, "a.RIM"), Entry(0x0100010C, 4095, 4095 << 8, 6,
		(128 << 16) | 128, name) }), Chunk("BMNAMVER", Number(4096)) })) });

	Check(edge == BitmapAddResult::Added && index == 4095 && out == nearFullWith, "(6) the largest index 4094 gives 4095, BMNAMVER 4095 gives version 4095 << 8 " + why);

	// ---- (7) refused
	puts("Refused:");
	auto refused = [&](Bytes const &image, std::string const &picture, uint32_t width, uint32_t height, BitmapAddResult want, std::string const &what)
	{
		out.assign(3, 'x');
		index = 99;
		BitmapAddResult const got = BitmapAdd_ToImage(image, picture, width, height, &out, &index, &why);

		Check(got == want && out.empty() && index == -1 && !why.empty(), "(7) " + what + " (" + BitmapAddResultWords(got) + ": " + why + ")");
	};

	auto env = [&](std::vector<Bytes> const &inside)
	{
		return Level({ before, Chunk("REBENVDT", Join(inside)), after });
	};

	Bytes badSize = level;
	int32_t const tooBig = 100000;

	memcpy(badSize.data() + 12 + before.size() + 8, &tooBig, 4);   // REBENVDT's size, past the end

	Bytes noEnd = env({ Chunk("BMPNAMES", Join({ Number(1), Number(0x0100010C), Number(1), Number(0), Number(6), Number(0), Bytes(8, 'n') })) });
	Bytes wrongTotal = level;

	wrongTotal[8] = (char)(wrongTotal[8] + 4);

	refused(level, "envrnmts\\TEST\\floor.rim", 128, 128, BitmapAddResult::NameInList, "the name there already (Envrnmts\\Test\\Floor.RIM), in other capitals");
	refused(Level({ before, after }), name, 128, 128, BitmapAddResult::NoPictureList, "no REBENVDT");
	refused(env({ acoustics }), name, 128, 128, BitmapAddResult::NoPictureList, "no BMPNAMES");
	refused(env({ List(1, { stock1 }), List(1, { stock5 }) }), name, 128, 128, BitmapAddResult::Unusual, "two BMPNAMES");
	refused(env({ List(1, { stock1 }), Chunk("BMNAMVER", Number(3)), Chunk("BMNAMVER", Number(4)) }), name, 128, 128, BitmapAddResult::Unusual, "two BMNAMVER");
	refused(env({ List(0x00010001, { stock1 }) }), name, 128, 128, BitmapAddResult::Unusual, "a count word's top bits set");
	refused(env({ List(1, { Entry(0x0100010C, 4095, 0, 6, 0, "a.RIM") }) }), name, 128, 128, BitmapAddResult::ListFull, "the largest index 4095");
	refused(env({ List(1, { stock1 }), Chunk("BMNAMVER", Number(4096)) }), name, 128, 128, BitmapAddResult::ListFull, "BMNAMVER 4096");
	refused(badSize, name, 128, 128, BitmapAddResult::CantWalk, "a size past the end");
	refused(noEnd, name, 128, 128, BitmapAddResult::CantWalk, "an entry's name with no end inside the list");
	refused(env({ List(3, { stock1, stock5 }) }), name, 128, 128, BitmapAddResult::CantWalk, "a count of three with two entries");
	refused(wrongTotal, name, 128, 128, BitmapAddResult::CantWalk, "the file's own size wrong");
	refused(Bytes(), name, 128, 128, BitmapAddResult::CantWalk, "no bytes");
	refused(level, "Custom\\..\\x.RIM", 128, 128, BitmapAddResult::BadCall, "a name going up a folder");
	refused(level, name, 0, 128, BitmapAddResult::BadCall, "a side of 0");
	refused(level, name, 128, 65536, BitmapAddResult::BadCall, "a side of 65536");

	BitmapAddResult const noRoom = BitmapAdd_ToImage(level, name, 128, 128, nullptr, &index, &why);

	Check(noRoom == BitmapAddResult::BadCall && index == -1, "(7) no room for the answer");

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
