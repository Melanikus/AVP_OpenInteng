// The test for the sound import's WAV check (src/model/wav_check.h) and its sound folder adder (src/model/sound_dir_add.h). Written 2026-11-27 by Melanikus
// (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code". Every file here is made by hand, so it needs none of
// the game's files and GitHub's runner runs it.
//   (1) a good WAV (PCM, 16 bits, mono, 22,050 a second) is taken, with what it holds; 8 bits stereo too; one with an even chunk between the format and the data
//       (as 5 stock sounds have: fact, smpl) too
//   (2) every refusal by the game's own words (Drake's game PR 2, wav_chunks.h), each file one change from a good one: no RIFF, no WAVE, another chunk first, a format of 14 or 20 bytes, an 18-byte
//       format with cbSize set, format 2, 3 channels, 24 bits, a rate of 0, numbers that don't agree, no data, data past the end, empty data, an odd chunk before
//       the data, a chunk past the end before the data, data of 64 MB + 1; and the game's own rules (Drake's game PR 2): the RIFF's size past the file, the
//       data past the RIFF's size, data that isn't a whole number of frames
//   (3) the 250,000-byte line: 250,000 taken without the flag, 250,001 taken WITH it (Steam AvP's limit is a warning)
//   (4) every shorter copy of a good file is refused (nothing is read past the bytes given), and no bytes at all
//   (5) the sound folder adder: a level made by hand gets SOUNDDIR at the end of REBENVDT, the sizes of REBENVDT and the file made right, every other byte the
//       same; the name's zeros to a multiple of 4; refused: a level with one already, no REBENVDT, bytes that can't be walked, a '/' in the name, a name the
//       game's rule refuses, an empty one, no room for the answer
// RED FIRST: on stand-ins that refuse everything, (1), (3) and (5)'s adding fail.

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "sound_dir_add.h"
#include "wav_check.h"

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

static void Add32(Bytes *b, uint32_t v)
{
	for (int k = 0; k < 4; k++)
	{
		b->push_back((uint8_t)(v >> (8 * k)));
	}
}

static void Add16(Bytes *b, uint16_t v)
{
	b->push_back((uint8_t)v);
	b->push_back((uint8_t)(v >> 8));
}

static void AddText(Bytes *b, char const *text)
{
	b->insert(b->end(), text, text + strlen(text));
}

// One chunk: its four letters, its length, its bytes (no pad added: a test that wants one adds it)
static Bytes Chunk(char const *id, Bytes const &body)
{
	Bytes b;

	AddText(&b, id);
	Add32(&b, (uint32_t)body.size());
	b.insert(b.end(), body.begin(), body.end());
	return b;
}

static Bytes Format(uint16_t tag, uint16_t channels, uint32_t rate, uint16_t bits, size_t length = 16)
{
	Bytes b;
	uint16_t const frame = (uint16_t)(channels * (bits / 8));

	Add16(&b, tag);
	Add16(&b, channels);
	Add32(&b, rate);
	Add32(&b, rate * frame);
	Add16(&b, frame);
	Add16(&b, bits);
	b.resize(length, 0);
	return b;
}

static Bytes Riff(std::vector<Bytes> const &chunks)
{
	Bytes all;

	for (Bytes const &c : chunks)
	{
		all.insert(all.end(), c.begin(), c.end());
	}

	Bytes b;

	AddText(&b, "RIFF");
	Add32(&b, (uint32_t)(all.size() + 4));
	AddText(&b, "WAVE");
	b.insert(b.end(), all.begin(), all.end());
	return b;
}

static Bytes Good(size_t dataBytes = 400, uint16_t channels = 1, uint16_t bits = 16)
{
	return Riff({ Chunk("fmt ", Format(1, channels, 22050, bits)), Chunk("data", Bytes(dataBytes, 0x40)) });
}

static void Refused(Bytes const &b, char const *words, char const *what)
{
	WavFacts facts;
	std::string why;
	bool const taken = WavCheck(b.data(), b.size(), &facts, &why);

	Check(!taken && why.find(words) != std::string::npos && facts.dataBytes == 0, std::string(what) + ": refused, \"" + why + "\"");
}

static void TestTaken()
{
	WavFacts facts;
	std::string why;
	Bytes const good = Good();

	Check(WavCheck(good.data(), good.size(), &facts, &why), "a good WAV is taken");
	Check(facts.channels == 1 && facts.bits == 16 && facts.rate == 22050 && facts.dataBytes == 400 && !facts.overSteamLimit, "what it holds: mono, 16 bits, 22050, 400 bytes");

	Bytes const stereo8 = Good(300, 2, 8);

	Check(WavCheck(stereo8.data(), stereo8.size(), &facts, &why) && facts.channels == 2 && facts.bits == 8, "8 bits stereo is taken");

	Bytes const between = Riff({ Chunk("fmt ", Format(1, 1, 22050, 16)), Chunk("fact", Bytes(4, 0)), Chunk("smpl", Bytes(60, 0)), Chunk("data", Bytes(100, 1)) });

	Check(WavCheck(between.data(), between.size(), &facts, &why) && facts.dataBytes == 100, "even chunks between the format and the data (fact 4, smpl 60) are skipped");

	Bytes const format18 = Riff({ Chunk("fmt ", Format(1, 1, 22050, 16, 18)), Chunk("data", Bytes(10, 1)) });

	Check(WavCheck(format18.data(), format18.size(), &facts, &why), "an 18-byte format with cbSize 0 is taken");
}

static void TestRefused()
{
	Bytes b = Good();

	b[0] = 'X';
	Refused(b, "it doesn't start with RIFF", "no RIFF");
	b = Good();
	b[8] = 'X';
	Refused(b, "it isn't a WAVE file", "no WAVE");
	Refused(Riff({ Chunk("LIST", Bytes(16, 0)), Chunk("fmt ", Format(1, 1, 22050, 16)), Chunk("data", Bytes(10, 1)) }), "its first chunk isn't its format (it is \"LIST\")", "a chunk before the format");
	Refused(Riff({ Chunk("fmt ", Format(1, 1, 22050, 16, 14)), Chunk("data", Bytes(10, 1)) }), "its format isn't the 16 bytes of plain PCM (this one: 14 bytes)", "a 14-byte format");
	Refused(Riff({ Chunk("fmt ", Format(1, 1, 22050, 16, 20)), Chunk("data", Bytes(10, 1)) }), "its format isn't the 16 bytes of plain PCM (this one: 20 bytes)", "a 20-byte format");

	Bytes cb = Format(1, 1, 22050, 16, 18);

	cb[16] = 22;
	Refused(Riff({ Chunk("fmt ", cb), Chunk("data", Bytes(10, 1)) }), "its format has extra bytes, which plain PCM never has (22 of them)", "an 18-byte format with cbSize set");
	Refused(Riff({ Chunk("fmt ", Format(2, 1, 22050, 16)), Chunk("data", Bytes(10, 1)) }), "it isn't plain PCM sound (its format tag isn't 1) (this one: format 2)", "format 2 (ADPCM)");
	Refused(Riff({ Chunk("fmt ", Format(1, 3, 22050, 16)), Chunk("data", Bytes(12, 1)) }), "it has neither 1 channel nor 2 (this one: 3)", "3 channels");
	Refused(Riff({ Chunk("fmt ", Format(1, 1, 22050, 24)), Chunk("data", Bytes(12, 1)) }), "its samples are neither 8 nor 16 bits (this one: 24 bits)", "24 bits");
	Refused(Riff({ Chunk("fmt ", Format(1, 1, 0, 16)), Chunk("data", Bytes(12, 1)) }), "its rate isn't from 100 to 100,000 samples a second (this one: 0)", "a rate of 0");

	Bytes disagree = Format(1, 1, 22050, 16);

	disagree[8] = 1;   // the bytes a second, made wrong
	Refused(Riff({ Chunk("fmt ", disagree), Chunk("data", Bytes(12, 1)) }), "its bytes a second aren't its rate times its block size", "a format whose bytes a second don't agree");
	Refused(Riff({ Chunk("fmt ", Format(1, 1, 22050, 16)) }), "it has no sound (no data chunk)", "no data chunk");

	Bytes past = Good();

	past[past.size() - 400 - 4] = 0x99;   // the data chunk's length, made 400 + 0x99... past the end
	past[past.size() - 400 - 3] = 0x01;
	Refused(past, "it says it holds more sound than the file has", "data past the end");
	Refused(Riff({ Chunk("fmt ", Format(1, 1, 22050, 16)), Chunk("data", Bytes()) }), "it holds no sound", "empty data");

	Bytes odd = Chunk("junk", Bytes(3, 0));

	odd.push_back(0);   // the pad byte a WAV has after an odd chunk
	// both games step a chunk by its length alone, never its pad: an odd chunk WITH its pad hides the data from both (rule 9), one without is found by both
	Refused(Riff({ Chunk("fmt ", Format(1, 1, 22050, 16)), odd, Chunk("data", Bytes(10, 1)) }), "it has no sound (no data chunk)", "an odd chunk with its pad before the data");

	{
		WavFacts facts;
		std::string why;
		Bytes const unpadded = Riff({ Chunk("fmt ", Format(1, 1, 22050, 16)), Chunk("junk", Bytes(3, 0)), Chunk("data", Bytes(10, 1)) });

		Check(WavCheck(unpadded.data(), unpadded.size(), &facts, &why) && facts.dataBytes == 10, "an odd chunk without its pad: the data found, as both games find it");
	}

	Bytes runs = Riff({ Chunk("fmt ", Format(1, 1, 22050, 16)), Chunk("junk", Bytes(8, 0)) });

	runs[runs.size() - 8 - 4] = 0xFE;   // junk's length made past the end (254: even, so it is the length that's refused, not its oddness)
	Refused(runs, "it has no sound (no data chunk)", "a chunk past the end before the data");

	// DRAKE'S EDGES (his wav_chunks_test pins the same): tag 0xFFFE, 0 channels, 4 and 0 bits, the rates 99 and 100,001 refused, 100 and 100,000 taken,
	// bytes a second of 0
	Refused(Riff({ Chunk("fmt ", Format(0xFFFE, 1, 22050, 16)), Chunk("data", Bytes(10, 1)) }), "its format tag isn't 1", "tag 0xFFFE (extensible)");
	Refused(Riff({ Chunk("fmt ", Format(1, 0, 22050, 16)), Chunk("data", Bytes(10, 1)) }), "it has neither 1 channel nor 2 (this one: 0)", "0 channels");
	Refused(Riff({ Chunk("fmt ", Format(1, 1, 22050, 4)), Chunk("data", Bytes(10, 1)) }), "neither 8 nor 16 bits (this one: 4 bits)", "4 bits");
	Refused(Riff({ Chunk("fmt ", Format(1, 1, 22050, 0)), Chunk("data", Bytes(10, 1)) }), "neither 8 nor 16 bits (this one: 0 bits)", "0 bits");
	Refused(Riff({ Chunk("fmt ", Format(1, 1, 99, 16)), Chunk("data", Bytes(10, 1)) }), "(this one: 99)", "a rate of 99");
	Refused(Riff({ Chunk("fmt ", Format(1, 1, 100001, 16)), Chunk("data", Bytes(10, 1)) }), "(this one: 100001)", "a rate of 100,001");

	for (uint32_t rate : { 100u, 100000u })
	{
		WavFacts facts;
		std::string why;
		Bytes const at = Riff({ Chunk("fmt ", Format(1, 1, rate, 16)), Chunk("data", Bytes(10, 1)) });

		Check(WavCheck(at.data(), at.size(), &facts, &why) && facts.rate == rate, "a rate of " + std::to_string(rate) + " is taken");
	}

	Bytes avg0 = Format(1, 1, 22050, 16);

	memset(&avg0[8], 0, 4);
	Refused(Riff({ Chunk("fmt ", avg0), Chunk("data", Bytes(10, 1)) }), "its bytes a second aren't its rate times its block size", "bytes a second of 0");

	// THE GAME'S OWN RULES (Drake's game PR 2): the RIFF's size inside the file; the data inside the RIFF's size; the data a whole number of frames
	Bytes riffPast = Good();

	riffPast[4] = (uint8_t)(riffPast[4] + 2);   // the RIFF says 2 bytes more than the file holds
	Refused(riffPast, "its RIFF chunk says it runs past the end of the file", "the RIFF's size past the file");

	Bytes riffShort = Good();

	riffShort[4] = (uint8_t)(riffShort[4] - 4);   // the RIFF says 4 bytes less: the data runs past it, though it is inside the file
	Refused(riffShort, "its sound runs past the end of its RIFF chunk", "the data past the RIFF's size");
	Refused(Riff({ Chunk("fmt ", Format(1, 1, 22050, 16)), Chunk("data", Bytes(401, 1)), Bytes(1, 0) }), "its sound isn't a whole number of blocks (401 bytes, in blocks of 2)", "401 bytes of 16-bit sound");

	// 64 MB + 1 of data, said by the chunk and there
	Bytes huge = Riff({ Chunk("fmt ", Format(1, 1, 22050, 16)), Chunk("data", Bytes(kWavMostDataBytes + 1, 0)) });

	Refused(huge, "it holds more sound than the game takes from one WAV", "data of 64 MB + 1");
}

static void TestSteamLine()
{
	WavFacts facts;
	std::string why;
	Bytes const at = Good(kWavSteamMostDataBytes);
	Bytes const over = Good(kWavSteamMostDataBytes + 2);

	Check(WavCheck(at.data(), at.size(), &facts, &why) && !facts.overSteamLimit, "250,000 bytes of sound: taken, no warning");
	Check(WavCheck(over.data(), over.size(), &facts, &why) && facts.overSteamLimit && facts.dataBytes == kWavSteamMostDataBytes + 2,
		"250,002 bytes: taken, WITH the warning (Steam AvP's SOUND_MAXSIZE)");
}

static void TestShorter()
{
	Bytes const good = Good(40);
	size_t taken = 0;

	for (size_t n = 0; n < good.size(); n++)
	{
		WavFacts facts;
		std::string why;

		taken += WavCheck(good.data(), n, &facts, &why) ? 1 : 0;
	}

	Check(taken == 0, "every shorter copy of a good file is refused (" + std::to_string((unsigned long long)taken) + " taken)");

	WavFacts facts;
	std::string why;

	Check(!WavCheck(nullptr, 0, &facts, &why), "no bytes at all: refused");
}

// A level made by hand: REBINFF2, then a chunk before, REBENVDT (holding `inside`), a chunk after
typedef std::vector<char> Level;

static Level Heading(char const *name, size_t total)
{
	Level b(name, name + 8);
	int32_t const size = (int32_t)total;
	char raw[4];

	memcpy(raw, &size, 4);
	b.insert(b.end(), raw, raw + 4);
	return b;
}

static Level Plain(char const *name, size_t bodyBytes)
{
	Level b = Heading(name, 12 + bodyBytes);

	b.resize(12 + bodyBytes, 'x');
	return b;
}

static Level Wrap(char const *name, std::vector<Level> const &children)
{
	Level body;

	for (Level const &c : children)
	{
		body.insert(body.end(), c.begin(), c.end());
	}

	Level b = Heading(name, 12 + body.size());

	b.insert(b.end(), body.begin(), body.end());
	return b;
}

static Level SoundDirChunk(std::string const &folder)
{
	size_t const payload = (folder.size() + 4) & ~(size_t)3;
	Level b = Heading("SOUNDDIR", 12 + payload);

	b.insert(b.end(), folder.begin(), folder.end());
	b.resize(12 + payload, 0);
	return b;
}

static void TestSoundDir()
{
	Level const before = Wrap("REBINFF2", { Plain("ENVPALET", 8), Wrap("REBENVDT", { Plain("BMPNAMES", 20), Plain("LIGHTSET", 4) }), Plain("OBJCHUNK", 16) });
	Level const want = Wrap("REBINFF2", { Plain("ENVPALET", 8), Wrap("REBENVDT", { Plain("BMPNAMES", 20), Plain("LIGHTSET", 4), SoundDirChunk("Custom\\Base") }),
		Plain("OBJCHUNK", 16) });
	Level out;
	std::string why;

	Check(SoundDirAdd_ToImage(before, "Custom\\Base", &out, &why) == SoundDirAddResult::Added && out == want,
		"SOUNDDIR Custom\\Base added at REBENVDT's end, the sizes made right, every other byte the same (" + why + ")");

	// the zeros: "Custom\Base" is 11 letters, so 12 bytes (one zero); one of 12 letters takes 16 (four zeros)
	Check(SoundDirChunk("Custom\\Base").size() == 24 && SoundDirChunk("Custom\\Basex").size() == 28, "the name's zeros to a multiple of 4 (the made-by-hand chunk)");

	Level const twelve = Wrap("REBINFF2", { Wrap("REBENVDT", { SoundDirChunk("Custom\\Basex") }) });
	Level const bare = Wrap("REBINFF2", { Wrap("REBENVDT", {}) });

	Check(SoundDirAdd_ToImage(bare, "Custom\\Basex", &out, &why) == SoundDirAddResult::Added && out == twelve, "a 12-letter name: its four zeros");

	Check(SoundDirAdd_ToImage(want, "Custom\\Base", &out, &why) == SoundDirAddResult::HasOne && out.empty(), "a level with a SOUNDDIR already: refused (" + why + ")");
	Check(SoundDirAdd_ToImage(Wrap("REBINFF2", { Plain("ENVPALET", 8) }), "Custom\\Base", &out, &why) == SoundDirAddResult::NoEnvironment && out.empty(),
		"no REBENVDT: refused (" + why + ")");

	Level broken = before;

	broken[12 + 8] = 0x7f;   // the first chunk's size, made past the end
	Check(SoundDirAdd_ToImage(broken, "Custom\\Base", &out, &why) == SoundDirAddResult::CantWalk && out.empty(), "a size past the end: refused (" + why + ")");

	Level wrongFile = before;

	wrongFile[8] = 1;   // the file's own size, made wrong
	Check(SoundDirAdd_ToImage(wrongFile, "Custom\\Base", &out, &why) == SoundDirAddResult::CantWalk, "the file's own size wrong: refused");
	Check(SoundDirAdd_ToImage(before, "Custom/Base", &out, &why) == SoundDirAddResult::BadCall && out.empty(), "a '/' in the name: refused (" + why + ")");
	Check(SoundDirAdd_ToImage(before, "Custom\\..\\Base", &out, &why) == SoundDirAddResult::BadCall, "a name going up a folder: refused (" + why + ")");
	Check(SoundDirAdd_ToImage(before, "", &out, &why) == SoundDirAddResult::BadCall, "an empty name: refused");
	Check(SoundDirAdd_ToImage(before, "Custom\\Base", nullptr, &why) == SoundDirAddResult::BadCall, "no room for the answer: refused");
	Check(SoundDirAdd_ToImage(Level(), "Custom\\Base", &out, &why) == SoundDirAddResult::CantWalk, "no bytes: refused");
}

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);
	TestTaken();
	TestRefused();
	TestSteamLine();
	TestShorter();
	TestSoundDir();
	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed ? 1 : 0;
}
