// The test of the switches' Links (strategy_views.h StrategyView_SwitchLinks / StrategyView_StoredLinks, strategy_lookup.h ListStrategyRecords): a Linked, Multi or Area switch's Links made again
// as inteng's FUN_0052dc90 makes them (strategy-data.md S-K61-1, S-K61-3, S-K66-2). Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng.
// AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: host_switch_links_test.exe <level> [<level> ...] (tests/host_switch_links_test.runargs). The levels are only read. inteng's Save makes every switch's Links again before it writes
// (FUN_005282d0 -> FUN_0052e000), so in a level inteng saved the Links each switch holds are the ones made again: on stock levels every one must come out the same, in the same order.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <array>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "level_file.h"
#include "strategy_lookup.h"
#include "strategy_views.h"

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

static void Word(std::vector<uint8_t> *p, int32_t value)
{
	p->insert(p->end(), (uint8_t const *)&value, (uint8_t const *)&value + 4);
}

static std::array<uint8_t, 8> Id(uint8_t n)
{
	std::array<uint8_t, 8> id = { n, 0, 0, 0, 0, 0, 0, 1 };

	return id;
}

// a payload as the lookup gives it: two words, the kind, then the strategy up to the given size
static std::vector<uint8_t> Payload(int32_t kind, size_t size)
{
	std::vector<uint8_t> p;

	Word(&p, 0);
	Word(&p, 0);
	Word(&p, kind);
	p.resize(size, 0);
	return p;
}

// a Binary / Linked switch targeting target with this request (Target at 36, request at 44); a 61 with no links
static StrategyRecordRef OneTarget(uint8_t n, int32_t kind, std::array<uint8_t, 8> const &target, int32_t request)
{
	StrategyRecordRef r;

	r.id = Id(n);
	r.kind = kind;
	r.payload = Payload(kind, 48);
	memcpy(r.payload.data() + 36, target.data(), 8);
	memcpy(r.payload.data() + 44, &request, 4);

	if (kind == 61)
	{
		Word(&r.payload, 0);
	}

	return r;
}

// a Multi / Area switch with these targets (and requests) and these links
static StrategyRecordRef Multi(uint8_t n, int32_t kind, std::vector<std::pair<std::array<uint8_t, 8>, int32_t>> const &targets, std::vector<std::array<uint8_t, 8>> const &links)
{
	StrategyRecordRef r;

	r.id = Id(n);
	r.kind = kind;
	r.payload = Payload(kind, 36);
	Word(&r.payload, (int32_t)targets.size());

	for (auto const &t : targets)
	{
		r.payload.insert(r.payload.end(), t.first.begin(), t.first.end());
		Word(&r.payload, t.second);
	}

	Word(&r.payload, (int32_t)links.size());

	for (auto const &l : links)
	{
		r.payload.insert(r.payload.end(), l.begin(), l.end());
	}

	Word(&r.payload, 0);                    // pad1
	Word(&r.payload, (int32_t)0x80000000);   // flags (End)
	return r;
}

static void TestRules()
{
	puts("the rules (FUN_0052dc90, FUN_005218b0, FUN_005218e0):");

	std::array<uint8_t, 8> const me = Id(9);
	std::vector<StrategyRecordRef> records;

	records.push_back(OneTarget(1, 52, me, 0));                                  // a Binary Switch targeting me, request 0
	records.push_back(OneTarget(2, 50, me, 2));                                  // a Door's bytes: no switch (FUN_005218b0)
	records.push_back(Multi(3, 66, { { Id(7), 2 }, { me, 0 }, { me, 2 } }, {})); // a Multi Switch: its first target with my ID has request 0
	records.push_back(OneTarget(4, 61, me, 3));                                  // a Linked Switch targeting me, request bit 1
	records.push_back(Multi(5, 68, { { me, 2 } }, {}));                          // an Area Switch targeting me, request bit 1
	records.push_back(OneTarget(6, 52, Id(8), 2));                               // a Binary Switch targeting someone else

	std::vector<std::array<uint8_t, 8>> links = StrategyView_SwitchLinks(61, me.data(), records);

	Check(links.size() == 4 && links[0] == Id(1) && links[1] == Id(3) && links[2] == Id(4) && links[3] == Id(5),
		"a Linked Switch (61): every switch that targets it, whatever the request, in the records' order: 1, 3, 4, 5 (the Door and the switch targeting another object are none)");

	links = StrategyView_SwitchLinks(66, me.data(), records);
	Check(links.size() == 2 && links[0] == Id(4) && links[1] == Id(5),
		"a Multi Switch (66): only where the request has bit 1 (& 2, 0x52193d / 0x5219c4): 4 and 5; the Multi Switch's FIRST target with the ID decides (request 0: not a link)");
	Check(StrategyView_SwitchLinks(68, me.data(), records) == links, "an Area Switch (68) as a Multi Switch");
	Check(StrategyView_SwitchLinks(52, me.data(), records).empty() && StrategyView_SwitchLinks(50, me.data(), records).empty(), "any other kind: no links (0x52dcc7)");

	std::vector<std::array<uint8_t, 8>> stored;

	Check(StrategyView_StoredLinks(66, Multi(3, 66, { { me, 1 } }, { Id(1), Id(2) }).payload, &stored) && stored.size() == 2 && stored[0] == Id(1) && stored[1] == Id(2),
		"a Multi Switch's stored Links: after its targets");

	StrategyRecordRef linked = OneTarget(4, 61, me, 0);

	linked.payload.resize(48);
	Word(&linked.payload, 1);
	linked.payload.insert(linked.payload.end(), me.begin(), me.end());
	Check(StrategyView_StoredLinks(61, linked.payload, &stored) && stored.size() == 1 && stored[0] == me, "a Linked Switch's stored Links: at 48");

	// hostile counts: a target or link count larger than the bytes, or below 0, is refused and nothing is read past the end
	StrategyRecordRef bad = Multi(3, 66, { { me, 2 } }, {});
	int32_t const huge = 0x7fffffff;

	memcpy(bad.payload.data() + 36, &huge, 4);
	Check(!StrategyView_StoredLinks(66, bad.payload, &stored) && StrategyView_SwitchLinks(66, me.data(), { bad }).empty(), "a target count past the bytes: refused, no link");

	int32_t const below = -1;

	memcpy(bad.payload.data() + 36, &below, 4);
	Check(!StrategyView_StoredLinks(66, bad.payload, &stored), "a target count below 0: refused");
	linked.payload.resize(52);
	Check(!StrategyView_StoredLinks(61, linked.payload, &stored), "a link count past the bytes: refused");
	Check(!StrategyView_StoredLinks(61, std::vector<uint8_t>(20, 0), &stored) && StrategyView_SwitchLinks(61, me.data(), { OneTarget(1, 52, me, 0) }).size() == 1, "a short payload: refused");
}

// S-K61-3: Strategy Data's Save writes each switch's Links made again (FUN_0052e000 -> FUN_0052dc90): StrategyView_WithLinks puts a new list in, the bytes around it kept
static void TestWithLinks()
{
	puts("the Links written (S-K61-3, StrategyView_WithLinks):");

	std::array<uint8_t, 8> const me = Id(9);
	std::vector<uint8_t> out;
	std::vector<std::array<uint8_t, 8>> stored;

	// a Linked Switch with two links and four bytes after them: one link written
	StrategyRecordRef linked = OneTarget(4, 61, me, 0);

	linked.payload.resize(48);
	Word(&linked.payload, 2);
	linked.payload.insert(linked.payload.end(), me.begin(), me.end());
	linked.payload.insert(linked.payload.end(), me.begin(), me.end());
	Word(&linked.payload, 0x12345678);

	Check(StrategyView_WithLinks(61, linked.payload, { Id(5) }, &out) && out.size() == linked.payload.size() - 8 && memcmp(out.data(), linked.payload.data(), 48) == 0 &&
		StrategyView_StoredLinks(61, out, &stored) && stored.size() == 1 && stored[0] == Id(5) && memcmp(out.data() + out.size() - 4, linked.payload.data() + linked.payload.size() - 4, 4) == 0,
		"a Linked Switch (61): NumLinks 2 -> 1 at 48, the new ID after it, the 48 bytes before and the 4 after kept");

	// a Multi Switch with one target and no links: two links written after its targets, its pad and flags kept
	StrategyRecordRef multi = Multi(3, 66, { { Id(7), 2 } }, {});

	Check(StrategyView_WithLinks(66, multi.payload, { Id(1), Id(2) }, &out) && out.size() == multi.payload.size() + 16 && StrategyView_StoredLinks(66, out, &stored) && stored.size() == 2 &&
		stored[0] == Id(1) && stored[1] == Id(2) && memcmp(out.data(), multi.payload.data(), 36 + 4 + 12) == 0 && memcmp(out.data() + out.size() - 8, multi.payload.data() + multi.payload.size() - 8, 8) == 0,
		"a Multi Switch (66): the Links after its targets (0 -> 2), the targets before and the pad / flags after kept");

	Check(StrategyView_WithLinks(68, Multi(5, 68, { { me, 2 } }, { Id(1) }).payload, {}, &out) && StrategyView_StoredLinks(68, out, &stored) && stored.empty(), "an Area Switch (68): its Links emptied");

	// hostile: a payload whose old list can't be read is refused and out is left as it was
	out.assign(3, 0xab);
	linked.payload.resize(52);
	Check(!StrategyView_WithLinks(61, linked.payload, { Id(1) }, &out) && out.size() == 3 && !StrategyView_WithLinks(52, OneTarget(1, 52, me, 0).payload, {}, &out),
		"a link count past the bytes, or a kind with no Links (52): refused, nothing written");
}

static void TestLevel(char const *path)
{
	OpenLevelFile level;

	if (level.Open(path) != LevelOpenResult::Opened || !level.Level())
	{
		Check(false, std::string("the game's loader takes ") + path);
		return;
	}

	std::vector<StrategyRecordRef> records;

	ListStrategyRecords(level.Level(), &records);

	size_t switches = 0, withLinks = 0, same = 0;

	for (StrategyRecordRef const &one : records)
	{
		if (one.kind != 61 && one.kind != 66 && one.kind != 68)
		{
			continue;
		}

		std::vector<std::array<uint8_t, 8>> stored;

		switches++;

		if (StrategyView_StoredLinks(one.kind, one.payload, &stored) && stored == StrategyView_SwitchLinks(one.kind, one.id.data(), records))
		{
			same++;
			withLinks += stored.empty() ? 0 : 1;
		}
	}

	Check(switches > 0 && same == switches, std::string(path) + ": " + std::to_string(switches) + " Linked / Multi / Area switches (" + std::to_string(withLinks) + " with links): the Links made again are the saved ones, in order, for " +
		std::to_string(same));
}

int main(int argc, char **argv)
{
	setvbuf(stdout, 0, _IONBF, 0);
	TestRules();
	TestWithLinks();

	if (argc < 2)
	{
		printf("  FAILED  give stock levels (tests/host_switch_links_test.runargs)\n");
		return 1;
	}

	for (int a = 1; a < argc; a++)
	{
		TestLevel(argv[a]);
	}

	if (checksFailed)
	{
		printf("FAILED host_switch_links_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_switch_links_test\n");
	return 0;
}
