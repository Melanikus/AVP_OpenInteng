// The test of the screenshot's context in its PNG (src/io/png_meta.cpp; GitHub #78). Written 2026-11-17 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// THE CASES: a known 1x1 PNG (its CRCs checked by this test's own bitwise CRC, not the code's table) holds no context; a round trip (UTF-8 text kept byte for byte, the chunk before IEND, the
// picture's own chunks untouched); and the refusals, each named: not a PNG, a corrupt CRC, a file cut short at every length, a second chunk, an over-long text (on writing and on reading),
// compressed, a language tag, not UTF-8, a zero byte, not JSON, JSON with more after it, nested past 64, bytes after IEND, IHDR not first, a length past 2^31, a type that is not letters,
// no IEND; Write refuses a picture that has one already. The file helpers: added in place, read back, a second add refused with the file unchanged, a folder refused.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include <windows.h>

#include "png_meta.h"

using namespace host;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

typedef std::vector<uint8_t> Bytes;

// The PNG standard's CRC, bit by bit (the code under test uses a table: two ways to one answer)
static uint32_t BitCrc(uint8_t const *bytes, size_t length)
{
	uint32_t crc = 0xffffffffu;

	for (size_t i = 0; i < length; i++)
	{
		crc ^= bytes[i];

		for (int k = 0; k < 8; k++)
		{
			crc = (crc & 1) ? (crc >> 1) ^ 0xedb88320u : crc >> 1;
		}
	}

	return crc ^ 0xffffffffu;
}

static void Put32(Bytes *out, uint32_t value)
{
	out->push_back((uint8_t)(value >> 24));
	out->push_back((uint8_t)(value >> 16));
	out->push_back((uint8_t)(value >> 8));
	out->push_back((uint8_t)value);
}

// A chunk of that type and data, its CRC right
static Bytes MakeChunk(char const *type, Bytes const &data)
{
	Bytes body(type, type + 4);
	Bytes chunk;

	body.insert(body.end(), data.begin(), data.end());
	Put32(&chunk, (uint32_t)data.size());
	chunk.insert(chunk.end(), body.begin(), body.end());
	Put32(&chunk, BitCrc(body.data(), body.size()));
	return chunk;
}

// Our iTXt's data: the keyword, its zero, the two flags, the language tag and the translated keyword (each ended by a zero), the text
static Bytes OurData(std::string const &text, uint8_t flag = 0, uint8_t method = 0, std::string const &language = "")
{
	Bytes data(kPngMetaKeyword, kPngMetaKeyword + strlen(kPngMetaKeyword));

	data.push_back(0);
	data.push_back(flag);
	data.push_back(method);
	data.insert(data.end(), language.begin(), language.end());
	data.push_back(0);
	data.push_back(0);
	data.insert(data.end(), text.begin(), text.end());
	return data;
}

// The PNG with these bytes put just before its IEND (its last 12)
static Bytes BeforeEnd(Bytes const &png, Bytes const &chunk)
{
	Bytes out(png.begin(), png.end() - 12);

	out.insert(out.end(), chunk.begin(), chunk.end());
	out.insert(out.end(), png.end() - 12, png.end());
	return out;
}

static PngMetaResult Read(Bytes const &png, std::string *json = nullptr, std::string *why = nullptr)
{
	std::string ignored, words;
	PngMetaResult const result = PngMeta_Read(png, json ? json : &ignored, &words);

	if (why)
	{
		*why = words;
	}

	return result;
}

static std::string Named(PngMetaResult result, std::string const &why)
{
	return std::string(" (") + PngMetaWords(result) + ": " + why + ")";
}

int main()
{
	// ---- a known picture: the 1x1 transparent PNG (67 bytes) found in many references
	Bytes const png = {
		0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a,
		0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89,
		0x00, 0x00, 0x00, 0x0a, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0x00, 0x01, 0x00, 0x00, 0x05, 0x00, 0x01, 0x0d, 0x0a, 0x2d, 0xb4,
		0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82 };

	Check(BitCrc(png.data() + 12, 17) == 0x1f15c489u && BitCrc(png.data() + 37, 14) == 0x0d0a2db4u && BitCrc(png.data() + 59, 4) == 0xae426082u,
		"the sample picture's three CRCs are right by this test's own bitwise CRC");

	std::string why, json;
	PngMetaResult result = Read(png, &json, &why);

	Check(result == PngMetaResult::NotFound && json.empty(), "a picture with no context: NotFound, its chunks and CRCs accepted" + Named(result, why));

	// ---- the round trip: UTF-8 kept byte for byte, the chunk just before IEND, the rest untouched
	std::string const context = "{ \"format\": 1, \"level\": \"Genshd1\", \"camera\": [ -27851, -14055, 85500 ], \"note\": \"caf\xc3\xa9 \\u00e9 \\\"quoted\\\"\", \"ok\": true, \"n\": null, \"x\": -1.5e3 }";
	Bytes written;

	result = PngMeta_Write(png, context, &written, &why);
	Check(result == PngMetaResult::Done, "the context is written" + Named(result, why));
	result = Read(written, &json, &why);
	Check(result == PngMetaResult::Done && json == context, "and read back the same, byte for byte" + Named(result, why));
	Check(written.size() == png.size() + 12 + OurData(context).size() && memcmp(written.data(), png.data(), png.size() - 12) == 0 &&
		memcmp(written.data() + written.size() - 12, png.data() + png.size() - 12, 12) == 0 && memcmp(written.data() + png.size() - 12 + 4, "iTXt", 4) == 0,
		"one iTXt chunk, just before IEND; every other byte of the picture as it was");
	Check(MakeChunk("iTXt", OurData(context)) == Bytes(written.begin() + (std::ptrdiff_t)(png.size() - 12), written.end() - 12), "its bytes are the standard's: keyword, 0, 0, 0, \"\", \"\", text; the CRC right");

	Bytes again;

	result = PngMeta_Write(written, context, &again, &why);
	Check(result == PngMetaResult::Duplicate && again.empty(), "writing a second context into it is refused" + Named(result, why));
	Check(PngMeta_Write(png, "{}", &again, &why) == PngMetaResult::Done && Read(again, &json) == PngMetaResult::Done && json == "{}", "an empty object is a context too");

	// ---- the refusals
	Check(Read(Bytes(), nullptr, &why) == PngMetaResult::NotPng, "no bytes: not a PNG" + Named(PngMetaResult::NotPng, why));
	Check(Read(Bytes(png.begin() + 1, png.end()), nullptr, &why) == PngMetaResult::NotPng, "a signature off by one byte: not a PNG");

	Bytes bad = written;

	bad[png.size() - 12 + 8 + 20] ^= 0x01;   // a byte of our text
	result = Read(bad, &json, &why);
	Check(result == PngMetaResult::BadCrc && json.empty(), "a corrupt CRC (one bit of the text changed): refused" + Named(result, why));

	bad = written;
	bad[16] ^= 0x01;   // IHDR's width
	Check(Read(bad, nullptr, &why) == PngMetaResult::BadCrc, "and a corrupt CRC in the picture's own IHDR too");

	size_t cuts = 0, refused = 0;

	for (size_t n = 0; n < written.size(); n++)
	{
		Bytes const cut(written.begin(), written.begin() + (std::ptrdiff_t)n);
		PngMetaResult const got = Read(cut);

		cuts++;
		refused += (got == PngMetaResult::Truncated || got == PngMetaResult::NoEnd || got == PngMetaResult::NotPng) ? 1 : 0;
	}

	Check(refused == cuts, "the file cut short at every one of its " + std::to_string(cuts) + " lengths: refused each time (" + std::to_string(refused) + ")");
	Check(Read(Bytes(written.begin(), written.end() - 12), nullptr, &why) == PngMetaResult::NoEnd, "cut just before IEND: no IEND" + Named(PngMetaResult::NoEnd, why));

	Bytes const twice = BeforeEnd(written, MakeChunk("iTXt", OurData("{\"format\": 1}")));

	result = Read(twice, &json, &why);
	Check(result == PngMetaResult::Duplicate && json.empty(), "a second context chunk: refused" + Named(result, why));

	std::string const longText = "{\"x\": \"" + std::string(kPngMetaMostTextBytes, 'a') + "\"}";

	result = PngMeta_Write(png, longText, &again, &why);
	Check(result == PngMetaResult::TooLarge, "an over-long text is not written" + Named(result, why));
	result = Read(BeforeEnd(png, MakeChunk("iTXt", OurData(longText))), &json, &why);
	Check(result == PngMetaResult::TooLarge && json.empty(), "and not read" + Named(result, why));

	std::string const atTheLimit = "{\"x\": \"" + std::string(kPngMetaMostTextBytes - 9, 'a') + "\"}";

	Check(atTheLimit.size() == kPngMetaMostTextBytes && PngMeta_Write(png, atTheLimit, &again, &why) == PngMetaResult::Done && Read(again, &json) == PngMetaResult::Done && json == atTheLimit,
		"a text of exactly the limit is written and read");

	struct Shape
	{
		char const *what;
		Bytes data;
	};

	Shape const shapes[] = {
		{ "compressed (flag 1)", OurData("{}", 1, 0) },
		{ "a compression method other than 0", OurData("{}", 0, 1) },
		{ "a language tag", OurData("{}", 0, 0, "en") },
		{ "text that is not UTF-8", OurData("{\"a\": \"\xff\"}") },
		{ "an overlong UTF-8 form", OurData("{\"a\": \"\xc0\xaf\"}") },
		{ "a UTF-8 surrogate", OurData("{\"a\": \"\xed\xa0\x80\"}") },
		{ "a zero byte in the text", OurData(std::string("{\"a\": \"x\0y\"}", 12)) },
		{ "text that is not JSON", OurData("hello") },
		{ "a JSON array, not an object", OurData("[1, 2]") },
		{ "JSON with more after it", OurData("{} {}") },
		{ "an unended string", OurData("{\"a\": \"x}") },
		{ "a raw control character in a string", OurData("{\"a\": \"x\ny\"}") },
		{ "a bad escape", OurData("{\"a\": \"\\x\"}") },
		{ "a number with a leading zero", OurData("{\"a\": 01}") },
		{ "a comma too many", OurData("{\"a\": 1,}") },
		{ "nested 65 deep", OurData("{\"a\": " + std::string(64, '[') + std::string(64, ']') + "}") },
	};

	for (Shape const &shape : shapes)
	{
		result = Read(BeforeEnd(png, MakeChunk("iTXt", shape.data)), &json, &why);
		Check(result == PngMetaResult::BadText && json.empty(), std::string("our chunk with ") + shape.what + ": refused" + Named(result, why));
	}

	Check(Read(BeforeEnd(png, MakeChunk("iTXt", OurData("{\"a\": " + std::string(63, '[') + std::string(63, ']') + "}"))), &json) == PngMetaResult::Done, "nested 64 deep: taken");
	Check(PngMeta_Write(png, "not json", &again, &why) == PngMetaResult::BadText && PngMeta_Write(png, std::string("{}\0", 3), &again, &why) == PngMetaResult::BadText,
		"Write refuses text that is not one JSON object, or holds a zero byte");

	Bytes after = written;

	after.push_back(0);
	result = Read(after, nullptr, &why);
	Check(result == PngMetaResult::BadChunk, "a byte after IEND: refused" + Named(result, why));

	Bytes noHeader(png.begin(), png.begin() + 8);
	Bytes const text = MakeChunk("tEXt", Bytes({ 'a', 0, 'b' }));

	noHeader.insert(noHeader.end(), text.begin(), text.end());
	noHeader.insert(noHeader.end(), png.begin() + 8, png.end());
	result = Read(noHeader, nullptr, &why);
	Check(result == PngMetaResult::BadChunk, "a chunk before IHDR: refused" + Named(result, why));

	Bytes huge = written;

	huge[png.size() - 12] = 0xff;   // our chunk's length: 0xff......
	result = Read(huge, nullptr, &why);
	Check(result == PngMetaResult::BadChunk || result == PngMetaResult::Truncated, "a length past 2^31: refused, nothing read past the end" + Named(result, why));

	Bytes digits = written;

	digits[png.size() - 12 + 4] = '1';   // "1TXt"
	result = Read(digits, nullptr, &why);
	Check(result == PngMetaResult::BadChunk, "a chunk type that is not four letters: refused" + Named(result, why));

	Check(Read(BeforeEnd(png, MakeChunk("iTXt", Bytes({ 'A', 'V', 'P', '_', 'O', 'p', 'e', 'n', 'I', 'n', 't', 'e', 'n', 'g', 0 }))), nullptr, &why) == PngMetaResult::BadText,
		"our keyword with nothing after it: refused");
	Check(Read(BeforeEnd(png, MakeChunk("iTXt", Bytes({ 'O', 't', 'h', 'e', 'r', 0, 0, 0, 0, 0, '{', '}' }))), nullptr, &why) == PngMetaResult::NotFound,
		"another program's iTXt is not ours: NotFound");

	// ---- the file helpers, on copies in a folder of the test's own
	std::string const folder = "png_meta_test_" + std::to_string(GetCurrentProcessId());
	std::string const file = folder + "\\shot.png";

	CreateDirectoryA(folder.c_str(), nullptr);

	FILE *out = fopen(file.c_str(), "wb");

	if (out)
	{
		fwrite(png.data(), 1, png.size(), out);
		fclose(out);
	}

	result = PngMeta_AddToFile(file.c_str(), context, &why);
	Check(result == PngMetaResult::Done, "the context added to a picture file" + Named(result, why));
	result = PngMeta_ReadFile(file.c_str(), &json, &why);
	Check(result == PngMetaResult::Done && json == context, "and read back from it" + Named(result, why));
	Check(GetFileAttributesA((file + ".meta").c_str()) == INVALID_FILE_ATTRIBUTES, "no temporary file is left");

	result = PngMeta_AddToFile(file.c_str(), "{\"other\": 2}", &why);
	Check(result == PngMetaResult::Duplicate && PngMeta_ReadFile(file.c_str(), &json, nullptr) == PngMetaResult::Done && json == context, "a second add is refused, the file as it was" + Named(result, why));
	result = PngMeta_ReadFile(folder.c_str(), &json, &why);
	Check(result == PngMetaResult::NotPng && json.empty(), "a folder is not a picture" + Named(result, why));
	Check(PngMeta_ReadFile((folder + "\\none.png").c_str(), &json, &why) == PngMetaResult::NotPng, "nor is a file that isn't there");

	DeleteFileA(file.c_str());
	RemoveDirectoryA(folder.c_str());

	// ---- a real picture, when there is one: the editor's own (D3DX's PNG, with its own chunks), put as host_png_meta_test_real.png in the folder
	// the test runs from. Its name is the test's own, so no path from the command line is opened (Snyk Code, Path Traversal, 2026-11-17).
	char const *const realName = "host_png_meta_test_real.png";
	FILE *in = fopen(realName, "rb");

	if (in)
	{
		Bytes real;

		uint8_t block[65536];
		size_t got;

		while ((got = fread(block, 1, sizeof(block), in)) > 0)
		{
			real.insert(real.end(), block, block + got);
		}

		fclose(in);

		result = Read(real, nullptr, &why);
		Check(result == PngMetaResult::NotFound, std::string("a picture the editor saved (") + realName + ") walks clean" + Named(result, why));
		result = PngMeta_Write(real, context, &written, &why);
		Check(result == PngMetaResult::Done && Read(written, &json) == PngMetaResult::Done && json == context, "and takes a context" + Named(result, why));
	}

	printf("%d checks failed\n", checksFailed);
	return checksFailed ? 1 : 0;
}
