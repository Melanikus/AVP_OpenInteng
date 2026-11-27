// The test for the .FFL pack writer and its strict checker (src/model/fastfile_pack.h). Written 2026-11-27 by Melanikus (Frost) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Made by hand here, so it needs none of the game's files and GitHub's runner runs it;
// the 108 stock packs are model_fastfile_pack_stock_test's.
//   (1) three files given out of order make the pack written out here by hand, byte for byte: _stricmp's order ("snd\_c.wav" before "snd\A.wav": '_' is
//       below the small letters, above the capitals), each entry 8 + ((letters + 4) & ~3), each start rounded up to 4, zeros between and at the end
//   (2) the checker passes it and lists the three as the entries say; it passes it with leftover bytes after a name's zero too (as 2,256 stock bytes are)
//   (3) the edges: a name of 8 letters takes 12 bytes (its zero and three more), a file of no bytes, 199 letters and 1,000 files are taken
//   (4) whatever Build makes, the checker passes, and lists what went in (50 files, names of 1 to 50 letters, 0 to 49 bytes)
//   (5) every refusal of Build, with its place in the caller's list and the pack left empty; every refusal of the name rule; their words
//   (6) every refusal of the checker on bytes made wrong one way at a time, each by its own words; every shorter copy of a pack and every header byte
//       turned over refused (the bytes come from anyone: nothing may be walked past them)
// RED FIRST: on a stand-in that refuses everything (2026-11-27), 48 of the 53 checks failed. The 5 that passed ask only for a refusal, which a stand-in gives
// too: no files; the name rule's three refusals; no bytes; every shorter copy; every header byte turned over.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "fastfile_pack.h"

using namespace model;

typedef std::vector<uint8_t> Bytes;

static int checks = 0;
static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checks++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static void AddU32(Bytes *bytes, uint32_t value)
{
	for (int k = 0; k < 4; k++)
	{
		bytes->push_back((uint8_t)(value >> (8 * k)));
	}
}

static void PutU32(Bytes *bytes, size_t at, uint32_t value)
{
	for (int k = 0; k < 4; k++)
	{
		(*bytes)[at + k] = (uint8_t)(value >> (8 * k));
	}
}

static void AddText(Bytes *bytes, char const *text, size_t zeros)
{
	bytes->insert(bytes->end(), text, text + strlen(text));
	bytes->insert(bytes->end(), zeros, 0);
}

static PackFile File(std::string const &name, Bytes const &bytes)
{
	PackFile file;

	file.name = name;
	file.bytes = bytes;
	return file;
}

// THE PACK OF (1), written out by hand: 20 bytes of header, three entries of 20 bytes, 20 bytes of data
//   at 20: "snd\_c.wav" (10 letters: its zero and 1 more), data at 0, 2 bytes      data 80..81, then 82..83 zeros to 4
//   at 40: "snd\A.wav" (9 letters: its zero and 2 more), data at 4, 8 bytes        data 84..91
//   at 60: "snd\b.wav" (9 letters), data at 12, 5 bytes                            data 92..96, then 97..99 zeros to 4
static Bytes ThreeByHand()
{
	Bytes b;

	AddText(&b, "RFFL", 0);
	AddU32(&b, 0);
	AddU32(&b, 3);
	AddU32(&b, 60);
	AddU32(&b, 20);
	AddU32(&b, 0);
	AddU32(&b, 2);
	AddText(&b, "snd\\_c.wav", 2);
	AddU32(&b, 4);
	AddU32(&b, 8);
	AddText(&b, "snd\\A.wav", 3);
	AddU32(&b, 12);
	AddU32(&b, 5);
	AddText(&b, "snd\\b.wav", 3);
	b.insert(b.end(), { 6, 7, 0, 0, 8, 9, 10, 11, 12, 13, 14, 15, 1, 2, 3, 4, 5, 0, 0, 0 });
	return b;
}

static std::vector<PackFile> ThreeFiles()
{
	return { File("snd\\b.wav", { 1, 2, 3, 4, 5 }), File("snd\\_c.wav", { 6, 7 }), File("snd\\A.wav", { 8, 9, 10, 11, 12, 13, 14, 15 }) };
}

static bool Passes(Bytes const &bytes, std::vector<PackListed> *listed = nullptr)
{
	std::vector<PackListed> got;
	std::string why;
	bool const passed = FastFilePack_Check(bytes.data(), bytes.size(), &got, &why);

	if (listed)
	{
		*listed = got;
	}

	return passed && why.empty();
}

// The checker refuses these bytes, says so in words holding `words`, and lists nothing
static void Refused(Bytes const &bytes, std::string const &words, std::string const &what)
{
	std::vector<PackListed> listed(1);
	std::string why;
	bool const passed = FastFilePack_Check(bytes.data(), bytes.size(), &listed, &why);

	Check(!passed && listed.empty() && why.find(words) != std::string::npos, "(6) refused: " + what + " (\"" + why + "\")");
}

// Build refuses these files for `expected`, at `whichExpected`, and leaves the pack empty
static void BuildRefused(std::vector<PackFile> const &files, PackRefused expected, size_t whichExpected, std::string const &what)
{
	Bytes pack(7, 0xab);
	PackRefused refused = PackRefused::None;
	size_t which = 12345;
	bool const built = FastFilePack_Build(files, &pack, &refused, &which);

	Check(!built && refused == expected && which == whichExpected && pack.empty(), "(5) Build refuses " + what + " (at " + std::to_string(which) + ")");
}

static bool Built(std::vector<PackFile> const &files, Bytes *pack)
{
	PackRefused refused = PackRefused::NoFiles;
	size_t which = 12345;

	return FastFilePack_Build(files, pack, &refused, &which) && refused == PackRefused::None;
}

int main()
{
	// (1) the pack, byte for byte
	Bytes three;
	Check(Built(ThreeFiles(), &three) && three == ThreeByHand(), "(1) three files out of order make the pack written out by hand, byte for byte (" + std::to_string(three.size()) + " of 100 bytes)");

	// (2) the checker passes it, and lists it
	std::vector<PackListed> listed;
	Check(Passes(ThreeByHand(), &listed) && listed.size() == 3 && listed[0].name == "snd\\_c.wav" && listed[0].offset == 0 && listed[0].length == 2 &&
		listed[1].name == "snd\\A.wav" && listed[1].offset == 4 && listed[1].length == 8 && listed[2].name == "snd\\b.wav" && listed[2].offset == 12 &&
		listed[2].length == 5, "(2) the checker passes it and lists the three in the pack's order, each where its entry says");

	Bytes leftovers = ThreeByHand();
	leftovers[39] = 0xab;   // after "snd\_c.wav"'s zero (at 38)
	leftovers[58] = 'x';    // after "snd\A.wav"'s zero (at 57)
	leftovers[59] = 0xff;
	Check(Passes(leftovers, &listed) && listed.size() == 3 && listed[0].name == "snd\\_c.wav" && listed[1].name == "snd\\A.wav",
		"(2) and with leftover bytes after a name's zero, as Rebellion's packer left in stock's (neither reader reads them)");

	// (3) the edges
	Bytes eight;
	Bytes eightByHand;
	AddText(&eightByHand, "RFFL", 0);
	AddU32(&eightByHand, 0);
	AddU32(&eightByHand, 1);
	AddU32(&eightByHand, 20);
	AddU32(&eightByHand, 4);
	AddU32(&eightByHand, 0);
	AddU32(&eightByHand, 4);
	AddText(&eightByHand, "ab\\c.wav", 4);
	eightByHand.insert(eightByHand.end(), { 9, 9, 9, 9 });
	Check(Built({ File("ab\\c.wav", { 9, 9, 9, 9 }) }, &eight) && eight == eightByHand && Passes(eight),
		"(3) a name of 8 letters takes 12 bytes: its zero and three more (44 bytes in all)");

	Bytes empty;
	Check(Built({ File("z\\b.wav", { 1 }), File("z\\a.wav", {}) }, &empty) && Passes(empty, &listed) && listed.size() == 2 && listed[0].name == "z\\a.wav" &&
		listed[0].offset == 0 && listed[0].length == 0 && listed[1].offset == 0 && listed[1].length == 1 && empty.size() == 20 + 32 + 4,
		"(3) a file of no bytes: it starts at 0 and so does the next");

	std::string const longest = "s\\" + std::string(kPackMostNameLetters - 6, 'a') + ".wav";
	Bytes longPack;
	Check(longest.size() == kPackMostNameLetters && Built({ File(longest, { 1, 2, 3 }) }, &longPack) && Passes(longPack, &listed) && listed.size() == 1 &&
		listed[0].name == longest, "(3) a name of 199 letters is taken");

	std::vector<PackFile> thousand;
	for (size_t i = 0; i < kPackMostFiles; i++)
	{
		thousand.push_back(File("f\\" + std::to_string(10000 + i) + ".wav", { (uint8_t)i }));
	}
	Bytes thousandPack;
	Check(Built(thousand, &thousandPack) && Passes(thousandPack, &listed) && listed.size() == kPackMostFiles, "(3) 1,000 files are taken");

	// (4) whatever Build makes, the checker passes
	std::vector<PackFile> fifty;
	for (size_t i = 0; i < 50; i++)
	{
		std::string name = "d\\" + std::string(i % 47, (char)('a' + i % 26)) + std::to_string(i);
		fifty.push_back(File(name, Bytes(i, (uint8_t)(i + 1))));
	}
	Bytes fiftyPack;
	bool same = Built(fifty, &fiftyPack) && Passes(fiftyPack, &listed) && listed.size() == 50;
	for (size_t i = 0; same && i < 50; i++)
	{
		bool found = false;
		for (PackListed const &l : listed)
		{
			found = found || (l.name == fifty[i].name && l.length == fifty[i].bytes.size() && (l.length == 0 ||
				memcmp(fiftyPack.data() + 20 + (fiftyPack[12] | fiftyPack[13] << 8 | fiftyPack[14] << 16) + l.offset, fifty[i].bytes.data(), l.length) == 0));
		}
		same = found;
	}
	Check(same, "(4) 50 files of every name length and size: the checker passes the pack and lists each with its own bytes");

	// (5) Build's refusals, each at its place in the caller's list, the pack left empty
	BuildRefused({}, PackRefused::NoFiles, 0, "no files");
	std::vector<PackFile> tooMany = thousand;
	tooMany.push_back(File("f\\extra.wav", { 1 }));
	BuildRefused(tooMany, PackRefused::TooManyFiles, kPackMostFiles, "1,001 files");
	BuildRefused({ File("s\\a.wav", { 1 }), File("", { 1 }) }, PackRefused::NameRefused, 1, "an empty name");
	BuildRefused({ File("s\\..\\a.wav", { 1 }) }, PackRefused::NameRefused, 0, "a name going up a folder");
	BuildRefused({ File("s\\con.wav", { 1 }) }, PackRefused::NameRefused, 0, "a name that is a device's");
	BuildRefused({ File("c:\\a.wav", { 1 }) }, PackRefused::NameRefused, 0, "a drive's colon");
	BuildRefused({ File("\\s\\a.wav", { 1 }) }, PackRefused::NameRefused, 0, "a name starting with a slash");
	BuildRefused({ File("s/a.wav", { 1 }) }, PackRefused::NameHasSlash, 0, "a '/' (the game asks with '\\')");
	BuildRefused({ File(longest + "x", { 1 }) }, PackRefused::NameTooLong, 0, "a name of 200 letters");
	BuildRefused({ File(std::string("s\\a\0b.wav", 9), { 1 }) }, PackRefused::NameHasNul, 0, "a zero inside a name");
	BuildRefused({ File("snd\\A.wav", { 1 }), File("x\\b.wav", { 1 }), File("SND\\a.WAV", { 2 }) }, PackRefused::NameTwice, 2, "the same name twice in other capitals");
	{
		// made in place: a list in braces would copy the 64 MB more than once
		std::vector<PackFile> big(2);
		big[0].name = "s\\a.wav";
		big[0].bytes.assign(1, 1);
		big[1].name = "s\\big.wav";
		big[1].bytes.assign(kPackMostFileBytes + 1, 0);
		BuildRefused(big, PackRefused::FileTooBig, 1, "a file of 64 MB and a byte");
	}
	{
		// in the pack's order s\a.wav (the caller's 1) comes first, so s\b.wav (the caller's 0) is the one that takes it over
		std::vector<PackFile> big(2);
		big[0].name = "s\\b.wav";
		big[0].bytes.assign(33u * 1024u * 1024u, 0);
		big[1].name = "s\\a.wav";
		big[1].bytes.assign(33u * 1024u * 1024u, 0);
		BuildRefused(big, PackRefused::PackTooBig, 0, "two files of 33 MB (a pack over 64 MB)");
	}

	Check(FastFilePack_WhyNameRefused("sound\\mission pack env\\Gen_1 & 2@x+y-z.wav") == PackRefused::None,
		"(5) the name rule takes every character stock's names have: letters, digits, space and & + - . @ _ \\");
	Check(FastFilePack_WhyNameRefused("s\\a.wav.") == PackRefused::NameRefused && FastFilePack_WhyNameRefused("s\\a.wav ") == PackRefused::NameRefused &&
		FastFilePack_WhyNameRefused("s\\\\a.wav") == PackRefused::NameRefused && FastFilePack_WhyNameRefused("s\\\xe9.wav") == PackRefused::NameRefused,
		"(5) and refuses a part ending in a dot or a space, an empty part, and a letter outside its list");

	bool wordsAll = true;
	PackRefused const all[] = { PackRefused::None, PackRefused::NoFiles, PackRefused::TooManyFiles, PackRefused::NameRefused, PackRefused::NameHasSlash,
		PackRefused::NameTooLong, PackRefused::NameHasNul, PackRefused::NameTwice, PackRefused::FileTooBig, PackRefused::PackTooBig };
	for (PackRefused a : all)
	{
		wordsAll = wordsAll && FastFilePack_RefusedWords(a) && FastFilePack_RefusedWords(a)[0];
		for (PackRefused b : all)
		{
			wordsAll = wordsAll && (a == b || strcmp(FastFilePack_RefusedWords(a), FastFilePack_RefusedWords(b)) != 0);
		}
	}
	Check(wordsAll, "(5) every refusal has words of its own");

	// (6) the checker's refusals, one thing wrong at a time, on the pack of (1)
	Bytes const good = ThreeByHand();
	Bytes b;

	Refused(Bytes(), "shorter than", "no bytes");
	b = good; b.resize(19); Refused(b, "shorter than", "19 bytes");
	b = good; b[3] = 'X'; Refused(b, "RFFL", "a wrong signature");
	b = good; PutU32(&b, 4, 1); Refused(b, "version", "version 1");
	b = good; PutU32(&b, 8, 0); Refused(b, "no files", "a count of 0");
	b = good; PutU32(&b, 8, (uint32_t)kPackMostFiles + 1); Refused(b, "more files", "a count of 1,001");
	b = good; PutU32(&b, 12, 61); Refused(b, "don't add up", "entry bytes one more than there are");
	b = good; b.push_back(0); Refused(b, "don't add up", "a byte more than the sizes say");
	b = good; PutU32(&b, 12, 0xfffffff0u); Refused(b, "don't add up", "entry bytes that would wrap a 32-bit sum");
	b = good; PutU32(&b, 12, 59); PutU32(&b, 16, 21); Refused(b, "multiple of 4", "data bytes not a multiple of 4 (the sum still right)");
	b = good; PutU32(&b, 8, 4); Refused(b, "past the entries", "a count of 4 with three entries");
	b = good; PutU32(&b, 8, 2); Refused(b, "walk over the entries", "a count of 2 with three entries");
	b = good; b[77] = 'x'; b[78] = 'x'; b[79] = 'x'; Refused(b, "no zero", "the last name with no zero inside the entries");
	b = good; b[71] = '/'; Refused(b, "'/'", "a name with '/'");
	b = good; b[72] = '.'; Refused(b, "refused", "a name going up a folder (\"snd\\..wav\")");
	b = good; b[73] = '\n'; Refused(b, "(snd\\b?wav)", "a name holding a line break, shown with '?' in the words (a refused name can hold anything)");
	b = good; b[72] = 'a'; Refused(b, "twice", "the same name twice in other capitals");
	b = good; b[52] = 'b'; b[72] = 'A'; Refused(b, "order", "two names out of _stricmp's order");
	b = good; PutU32(&b, 64, (uint32_t)kPackMostFileBytes + 1); Refused(b, "longer than", "a file of 64 MB and a byte");
	b = good; PutU32(&b, 40, 5); Refused(b, "multiple of 4", "a file starting at 5");
	b = good; PutU32(&b, 40, 8); Refused(b, "where the last one ended", "a file starting past where the last one ended");
	b = good; PutU32(&b, 64, 9); Refused(b, "past the data", "a file running past the data");
	b = good; PutU32(&b, 64, 4); Refused(b, "where the last file", "data bytes going on past the last file's round-up");
	b = good; b[82] = 1; Refused(b, "zeros", "a byte between two files that isn't 0");
	b = good; b[98] = 1; Refused(b, "zeros", "a byte after the last file that isn't 0");
	{
		Bytes over(kPackMostBytes + 4, 0);
		memcpy(over.data(), good.data(), 20);
		Refused(over, "bigger than", "a pack over 64 MB");
	}
	Refused(Bytes(1, 0), "shorter than", "one byte");
	{
		std::vector<PackListed> got(1);
		std::string why;
		Check(!FastFilePack_Check(nullptr, 100, &got, &why) && got.empty() && !why.empty(), "(6) refused: no bytes at all, though a size is given");
	}

	bool everyShorter = true;
	for (size_t n = 0; n < good.size(); n++)
	{
		Bytes cut(good.begin(), good.begin() + n);
		std::vector<PackListed> got(1);
		std::string why;
		everyShorter = everyShorter && !FastFilePack_Check(cut.data(), cut.size(), &got, &why) && got.empty() && !why.empty();
	}
	Check(everyShorter, "(6) every shorter copy of the pack (0 to 99 bytes) is refused");

	bool everyHeaderByte = true;
	for (size_t i = 0; i < 20; i++)
	{
		b = good;
		b[i] ^= 0xff;
		std::vector<PackListed> got(1);
		std::string why;
		everyHeaderByte = everyHeaderByte && !FastFilePack_Check(b.data(), b.size(), &got, &why) && got.empty() && !why.empty();
	}
	Check(everyHeaderByte, "(6) every header byte turned over is refused");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
