// The test for src/host/module_link_avp: LINK WITH AVP's two files (Module Linking's Misc > Link with AVP, 0x190): module.aaa as inteng writes it when the program loses the focus
// (FUN_004c0f00) and the game's reader reads it (avpx projload.cpp LoadModuleData), and module.bbb, the game's camera, read as HOSTILE input when the program gets the focus back
// (FUN_004c19a0, with the size and number checks inteng never made). Written 2026-11-18 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
//
// Each check's words say what it proves. The program gives back a non-zero result if any check failed.

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "module_link_avp.h"

static int checksFailed = 0;
static int checksMade = 0;

static void Check(bool good, std::string const &what)
{
	checksMade++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static void PutInt(std::vector<uint8_t> *out, int32_t value)
{
	uint32_t const bits = (uint32_t)value;

	for (int i = 0; i < 4; i++)
	{
		out->push_back((uint8_t)((bits >> (8 * i)) & 0xff));
	}
}

// module.bbb's 48 bytes, written as the game writes them (projload.cpp:3208-3211: VDB_World, then VDB_Mat)
static std::vector<uint8_t> CameraBytes(int32_t x, int32_t y, int32_t z, std::vector<int32_t> const &matrix)
{
	std::vector<uint8_t> bytes;

	PutInt(&bytes, x);
	PutInt(&bytes, y);
	PutInt(&bytes, z);

	for (int32_t const one : matrix)
	{
		PutInt(&bytes, one);
	}

	return bytes;
}

static bool Refused(std::vector<uint8_t> const &bytes, std::string const &words, std::string *whyOut = nullptr)
{
	host::LinkAvpCamera camera;

	memset(&camera, 0x5a, sizeof(camera));

	host::LinkAvpCamera const before = camera;
	std::string why;
	bool const read = host::LinkAvp_ReadCamera(bytes.data(), bytes.size(), &camera, &why);
	bool const untouched = memcmp(&camera, &before, sizeof(camera)) == 0;

	Check(!read && !why.empty() && untouched, words + " (refused, the camera given back untouched: \"" + why + "\")");

	if (whyOut)
	{
		*whyOut = why;
	}

	return !read;
}

int main()
{
	// ---- module.aaa
	{
		std::vector<host::LinkAvpModule> modules(2);

		modules[0].index = 38;
		modules[0].items = { { 21, 0 }, { 76, 2 } };
		modules[1].index = -1;

		std::vector<uint8_t> bytes;
		std::string why;

		Check(host::LinkAvp_AaaBytes("avp_rifs/Invasion.rif", modules, &bytes, &why), "module.aaa is made for a level and two modules");

		std::vector<uint8_t> want(60, 0);

		memcpy(want.data(), "avp_rifs/Invasion.rif", strlen("avp_rifs/Invasion.rif"));
		PutInt(&want, 38);
		PutInt(&want, 2);
		PutInt(&want, 21);
		PutInt(&want, 0);
		PutInt(&want, 76);
		PutInt(&want, 2);
		PutInt(&want, -1);
		PutInt(&want, 0);
		Check(bytes == want, "the bytes are the game reader's: the 60-byte name with zeros after it, then per module its index and count and per item object_index THEN branch_no (FUN_004c0f00 writes +0xc then +0x0), "
			"a module with no items written with count 0, all 32-bit little-endian (" + std::to_string(bytes.size()) + " bytes, 92 wanted)");
	}

	{
		std::vector<uint8_t> bytes;
		std::string why;
		std::string const fits(59, 'a');

		Check(host::LinkAvp_AaaBytes(fits, {}, &bytes, &why) && bytes.size() == 60 && memcmp(bytes.data(), fits.data(), 59) == 0 && bytes[59] == 0,
			"a 59-letter name fits the field whole, with its zero (and no module: the name alone, 60 bytes)");

		std::string const longName = "avp_rifs/" + std::string(51, 'b') + "/Custom.rif";

		Check(host::LinkAvp_AaaBytes(longName, {}, &bytes, &why) && bytes.size() == 60 && memcmp(bytes.data(), "Custom.rif", 11) == 0,
			"INTENG BUG (1) FIXED: a name that does not fit (" + std::to_string(longName.size()) + " letters) is cut to its part after the last '/', all the game compares (inteng overran its stack)");

		Check(host::LinkAvp_AaaBytes("C:" + std::string(70, 'c') + ":Level.rif", {}, &bytes, &why) && memcmp(bytes.data(), "Level.rif", 10) == 0,
			"a ':' cuts the same as a '/', as the game's compare does");

		bytes.assign(3, 7);
		why.clear();
		Check(!host::LinkAvp_AaaBytes("avp_rifs/" + std::string(60, 'd') + ".rif", {}, &bytes, &why) && !why.empty() && bytes.size() == 3,
			"a name whose last part does not fit either writes nothing, with why (\"" + why + "\"), the output left as it was");
	}

	// ---- module.bbb
	// The game's matrix of a camera turned 45 degrees (512) and looking 22.5 degrees (256): P(look) x E(0, turn, 0) (level_view.cpp LevelView_MatrixOfAngles), each number x 65536
	std::vector<int32_t> const turned = { 46341, 0, 46341, 17734, 60547, -17734, -42813, 25080, 42813 };

	{
		std::vector<uint8_t> const bytes = CameraBytes(1000, -2000, 3000, turned);
		host::LinkAvpCamera camera;
		std::string why;

		Check(host::LinkAvp_ReadCamera(bytes.data(), bytes.size(), &camera, &why), "a 48-byte module.bbb with a place and a turn is taken (" + why + ")");
		Check(camera.place[0] == 1000 && camera.place[1] == -2000 && camera.place[2] == 3000, "its place is the game's, in mm (inteng's x 100 is its own unit)");

		bool same = true;

		for (int i = 0; i < 9; i++)
		{
			same = same && camera.matrix[i] == turned[i];
		}

		Check(same, "its matrix is the game's VDB_Mat in its own order, mat11 mat12 mat13 mat21 ...");
	}

	std::string why;

	Refused(std::vector<uint8_t>(), "INTENG BUG (3) FIXED: an empty module.bbb");
	{
		std::vector<uint8_t> shorter = CameraBytes(0, 0, 0, turned);

		shorter.pop_back();
		Refused(shorter, "a module.bbb of 47 bytes (inteng read the rest from its stack)", &why);
		Check(why.find("47 bytes") != std::string::npos, "the words say how many bytes it has: \"" + why + "\"");
	}

	{
		std::vector<uint8_t> longer = CameraBytes(0, 0, 0, turned);

		longer.push_back(0);
		Refused(longer, "a module.bbb of 49 bytes");
	}

	{
		host::LinkAvpCamera camera;
		std::string words;

		Check(!host::LinkAvp_ReadCamera(nullptr, 48, &camera, &words) && !words.empty(), "no bytes at all, said to be 48, are refused");
	}

	{
		std::vector<uint8_t> const edge = CameraBytes(host::kLinkAvpMostPlace, -host::kLinkAvpMostPlace, 0, turned);
		host::LinkAvpCamera camera;
		std::string words;

		Check(host::LinkAvp_ReadCamera(edge.data(), edge.size(), &camera, &words), "a place at the camera's limit, 2^24 mm, is taken");
	}

	Refused(CameraBytes(host::kLinkAvpMostPlace + 1, 0, 0, turned), "a place 1 mm past the camera's limit on x (inteng's x 100 would overflow from 21,474,837)");
	Refused(CameraBytes(0, -host::kLinkAvpMostPlace - 1, 0, turned), "a place past the limit on y, below");
	Refused(CameraBytes(0, 0, 0x7fffffff, turned), "the largest place on z");
	Refused(CameraBytes(0, 0, 0, { 65536, 65536, 65536, 65536, 65536, 65536, 65536, 65536, 65536 }), "a matrix whose rows are not of length 1");
	Refused(CameraBytes(0, 0, 0, { 65536, 0, 0, 0, 65536, 0, 0, 0, -65536 }), "a mirror (rows of length 1, square, but turned inside out)");
	Refused(CameraBytes(0, 0, 0, { 65536, 0, 0, 65536, 0, 0, 0, 0, 65536 }), "two rows the same (not square to each other)");
	Refused(CameraBytes(0, 0, 0, { 0x7fffffff, 0, 0, 0, 65536, 0, 0, 0, 65536 }), "a matrix with the largest number in it (nothing overflows)");
	Refused(CameraBytes(0, 0, 0, { 0, 0, 0, 0, 0, 0, 0, 0, 0 }), "a matrix of zeros");

	{
		std::vector<uint8_t> const near = CameraBytes(0, 0, 0, { 66191, 0, 0, 0, 66191, 0, 0, 0, 66191 });   // 1% long
		host::LinkAvpCamera camera;
		std::string words;

		Check(host::LinkAvp_ReadCamera(near.data(), near.size(), &camera, &words), "a turn 1% off (the game's 16.16 numbers are never exact) is taken");
	}

	Refused(CameraBytes(0, 0, 0, { 67502, 0, 0, 0, 67502, 0, 0, 0, 67502 }), "a matrix 3% long");

	printf("%d checks, %d failed\n", checksMade, checksFailed);
	printf("%s\n", checksFailed ? "SOME CHECKS FAILED" : "All checks passed");
	return checksFailed ? 1 : 0;
}
