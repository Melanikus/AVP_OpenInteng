// smk_header_test.cpp: a Smacker header read as hostile input, the words about it, and the game's message FMV names (src/model/smk_header.h; facts:
// AvP-docs/engine/inteng/reassess/fmv-trace-dietrich.md). Written 2026-11-18 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "smk_header.h"

static int checks = 0, checksFailed = 0;

static void Check(bool ok, std::string const &what)
{
	checks++;

	if (!ok)
	{
		checksFailed++;
	}

	printf("  %s  %s\n", ok ? "ok" : "FAILED", what.c_str());
}

static void Put(std::vector<uint8_t> *bytes, size_t at, uint32_t value)
{
	for (int i = 0; i < 4; i++)
	{
		(*bytes)[at + (size_t)i] = (uint8_t)(value >> (8 * i));
	}
}

// A header as the game's message FMVs have it (message100.smk's numbers: rate -6666, track 0 0xc0005622), then each test changes what it needs
static std::vector<uint8_t> AvpMessage()
{
	std::vector<uint8_t> bytes(model::kSmkHeaderBytes, 0);

	memcpy(bytes.data(), "SMK2", 4);
	Put(&bytes, 4, 128);
	Put(&bytes, 8, 96);
	Put(&bytes, 12, 228);
	Put(&bytes, 16, (uint32_t)-6666);
	Put(&bytes, 72, 0xc0005622u);
	return bytes;
}

int main()
{
	printf("THE SMACKER HEADER\n");

	model::SmkHeader header;
	std::vector<uint8_t> bytes = AvpMessage();

	Check(model::SmkHeader_Read(bytes.data(), bytes.size(), &header) == model::SmkRead::Read, "an AvP message header is read");
	Check(header.width == 128 && header.height == 96 && header.frames == 228, "its size and frames");
	Check(header.tracks[0].present && header.tracks[0].compressed && header.tracks[0].rate == 22050 && header.tracks[0].bits == 8 && header.tracks[0].channels == 1, "its track 0: 22050 Hz 8-bit mono, compressed");
	Check(!header.tracks[1].present, "no track 1");
	Check(model::SmkHeader_DiffersFromAvp(header).empty(), "nothing differs from AvP's format (rate -6666 is 15.0015 fps)");
	Check(model::SmkHeader_Words(header) == "SMK2 128 x 96, 228 frames at 15 fps (15.2 s), 22050 Hz 8-bit mono", "its words: " + model::SmkHeader_Words(header));

	// HOSTILE: too short, not Smacker, impossible numbers; *out is left alone each time
	model::SmkHeader untouched;

	untouched.width = 77;
	Check(model::SmkHeader_Read(bytes.data(), model::kSmkHeaderBytes - 1, &untouched) == model::SmkRead::TooShort && untouched.width == 77, "103 bytes: too short, nothing written");
	Check(model::SmkHeader_Read(nullptr, 1000, &untouched) == model::SmkRead::TooShort, "no bytes: too short");

	std::vector<uint8_t> bik = bytes;

	memcpy(bik.data(), "BIKi", 4);
	Check(model::SmkHeader_Read(bik.data(), bik.size(), &untouched) == model::SmkRead::NotSmacker && untouched.width == 77, "a Bink file: not Smacker");

	std::vector<uint8_t> smk3 = bytes;

	memcpy(smk3.data(), "SMK3", 4);
	Check(model::SmkHeader_Read(smk3.data(), smk3.size(), nullptr) == model::SmkRead::NotSmacker, "SMK3: not Smacker");

	std::vector<uint8_t> smk4 = bytes;

	memcpy(smk4.data(), "SMK4", 4);
	Check(model::SmkHeader_Read(smk4.data(), smk4.size(), &header) == model::SmkRead::Read && model::SmkHeader_DiffersFromAvp(header) == "SMK4, not SMK2", "SMK4 is read and differs");

	struct Bad { size_t at; uint32_t value; char const *what; };
	Bad const bad[] = { { 4, 0, "width 0" }, { 8, 0, "height 0" }, { 4, 4097, "width 4097" }, { 8, 0xffffffffu, "height 4294967295" }, { 12, 0, "0 frames" }, { 12, 1000001, "1,000,001 frames" } };

	for (Bad const &one : bad)
	{
		std::vector<uint8_t> b = bytes;

		Put(&b, one.at, one.value);
		Check(model::SmkHeader_Read(b.data(), b.size(), &untouched) == model::SmkRead::Impossible && untouched.width == 77, std::string(one.what) + ": impossible");
	}

	std::vector<uint8_t> big = bytes;

	Put(&big, 4, 4096);
	Put(&big, 8, 4096);
	Put(&big, 12, 1000000);
	Check(model::SmkHeader_Read(big.data(), big.size(), &header) == model::SmkRead::Read, "4096 x 4096 and 1,000,000 frames: the most, read");

	// THE RATE: above 0 ms a frame, below 0 hundredths of a ms, 0 is 100 ms; the most negative has no positive in 32 bits
	struct Rate { int32_t field; double fps; };
	Rate const rates[] = { { 100, 10.0 }, { 0, 10.0 }, { -10000, 10.0 }, { 40, 25.0 }, { INT_MIN, 1000.0 / 21474836.48 } };

	for (Rate const &one : rates)
	{
		std::vector<uint8_t> b = bytes;

		Put(&b, 16, (uint32_t)one.field);
		model::SmkHeader_Read(b.data(), b.size(), &header);

		double const fps = model::SmkHeader_Fps(header);

		Check(fps > one.fps * 0.9999 && fps < one.fps * 1.0001, "rate field " + std::to_string(one.field) + ": " + std::to_string(fps) + " fps");
	}

	// WHAT DIFFERS, each part named
	std::vector<uint8_t> other = bytes;

	Put(&other, 4, 320);
	Put(&other, 8, 240);
	Put(&other, 16, 100);
	Put(&other, 72, 0xf0002b11u);   // 11025 Hz, 16-bit, stereo
	model::SmkHeader_Read(other.data(), other.size(), &header);
	Check(model::SmkHeader_DiffersFromAvp(header) == "320 x 240, not 128 x 96; 10 fps, not 15; sound 11025 Hz 16-bit stereo, not 22050 Hz 8-bit mono", "what differs: " + model::SmkHeader_DiffersFromAvp(header));

	std::vector<uint8_t> silent = bytes;

	Put(&silent, 72, 0);
	model::SmkHeader_Read(silent.data(), silent.size(), &header);
	Check(model::SmkHeader_DiffersFromAvp(header) == "no sound, not 22050 Hz 8-bit mono", "no track 0: differs");
	Check(model::SmkHeader_Words(header).find("no sound") != std::string::npos, "and its words say no sound");

	printf("THE MESSAGE FMV NAMES\n");

	struct Name { char const *name; bool is; int32_t number; };
	Name const names[] = {
		{ "message100.smk", true, 100 }, { "Message154.smk", true, 154 }, { "MESSAGE7.SMK", true, 7 }, { "message0.smk", true, 0 }, { "message450.smk", true, 450 },
		{ "message451.smk", false, 0 }, { "message07.smk", false, 0 }, { "message00.smk", false, 0 }, { "message.smk", false, 0 }, { "message-5.smk", false, 0 },
		{ "message1000.smk", false, 0 }, { "message12a.smk", false, 0 }, { "xmessage1.smk", false, 0 }, { "message1.smk.bak", false, 0 }, { "message1.bik", false, 0 },
		{ "message 1.smk", false, 0 }, { "", false, 0 } };

	for (Name const &one : names)
	{
		int32_t number = -1;
		bool const is = model::MessageFmvNumber(one.name, &number);

		Check(is == one.is && (!is || number == one.number), std::string("\"") + one.name + "\": " + (one.is ? "message " + std::to_string(one.number) : std::string("no message FMV")));
	}

	Check(model::MessageFmvName(47) == "message47.smk", "message 47's file is message47.smk");

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed != 0)
	{
		printf("FAILED smk_header_test\n");
		return 1;
	}

	printf("passed smk_header_test\n");
	return 0;
}
