// The test of sha256_hex.h (Sha256Hex: the SHA-256 the editor names files by, with the game's own SHA-256): the standard's own examples (FIPS 180-4), the four that
// tests/host_inteng_exe_test checked for IntengExe_Sha256 before the inteng.exe reader went (2026-11-24). Needs nothing; runs everywhere.
// Written 2026-11-24 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>
#include <string.h>

#include <string>

#include "sha256_hex.h"

static int checks = 0, checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	checks++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);

	Check(host::Sha256Hex((uint8_t const *)"", 0) == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "sha256 of nothing (FIPS 180-4)");
	Check(host::Sha256Hex((uint8_t const *)"abc", 3) == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256 of abc");

	char const *const two = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

	Check(host::Sha256Hex((uint8_t const *)two, strlen(two)) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1", "sha256 of the two-block example (56 bytes)");

	std::string const million(1000000, 'a');

	Check(host::Sha256Hex((uint8_t const *)million.data(), million.size()) == "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0", "sha256 of a million a's");

	printf("%d checks, %d failed\n", checks, checksFailed);

	if (checksFailed == 0)
	{
		printf("All checks passed\n");
		return 0;
	}

	printf("SOME CHECKS FAILED\n");
	return 1;
}
