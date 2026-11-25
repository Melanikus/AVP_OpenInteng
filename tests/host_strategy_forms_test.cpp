// The test of strategy_forms.h (Edit Strategy as a form of drop-downs, number boxes and buttons): which kinds open a form and the rows each one has. A kind not yet
// switched to its form must keep inteng's page (no rows), so the editor can be released after any merge. Needs nothing; runs everywhere.
// Written 2026-11-24 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "field_model.h"
#include "strategy_forms.h"
#include "strategy_forms_paths.h"   // paths (Frost): the editor's answers, stood in below

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

// A new strategy of the kind, as the editor makes one (the model's own maker)
static std::vector<uint8_t> New(int32_t kind)
{
	uint8_t bytes[1024];
	size_t length = 0;

	return model::MakeNewStrategy(kind, bytes, sizeof(bytes), &length) ? std::vector<uint8_t>(bytes, bytes + length) : std::vector<uint8_t>();
}

// The payload with a field set, its place found through the model's own reader (as tests/host_strategy_views_test finds a field's place)
static std::vector<uint8_t> With(int32_t kind, std::vector<uint8_t> const &payload, char const *field, int32_t value)
{
	for (size_t at = 0; at + 4 <= payload.size(); at += 4)
	{
		std::vector<uint8_t> probe = payload;
		int32_t read = 0;

		memcpy(probe.data() + at, &value, 4);

		if (host::StrategyView_FieldNumber(kind, probe, field, &read) && read == value)
		{
			return probe;
		}
	}

	return payload;
}

static bool ShownAt(host::StrategyFormRow const &row, int32_t kind, std::vector<uint8_t> const &payload)
{
	return !row.shown || row.shown(kind, payload);
}

// paths (Frost): the editor's answers (strategy_forms_paths.h), stood in: no species, no track sections and no paths, as with nothing selected and no level open
int32_t host::StrategyFormsPaths_SelectedSpecies()
{
	return -1;
}

int32_t host::StrategyFormsPaths_SelectedTrackSections()
{
	return 0;
}

std::vector<std::pair<int32_t, std::string>> host::StrategyFormsPaths_LevelPaths()
{
	return {};
}

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);

	// The kinds that have a form; every other kind keeps its page. Each group adds one line per kind UNDER ITS OWN LINE as it switches it on
	// (hasForm[61] = true;), so the four of us never edit the same lines.
	bool hasForm[128] = {};
	// switches (Vasquez): 61, 66, 68
	hasForm[61] = true;
	hasForm[66] = true;
	hasForm[68] = true;
	// modules (Drake): 51, 50, 57, 59, 54, 75, 64
	hasForm[75] = true;   // Fan (Drake 2026-11-24)
	hasForm[54] = true;   // Platform Lift (Drake 2026-11-24)
	hasForm[50] = true;   // Door (Drake 2026-11-24)
	// objects (Dietrich): 62, 77, 78, 73, 70
	// paths (Frost): 72, 80, 76, 69, 79
	hasForm[69] = true;
	hasForm[79] = true;
	hasForm[72] = true;
	hasForm[80] = true;
	hasForm[76] = true;

	for (int32_t kind = -1; kind < 128; kind++)
	{
		bool const expected = kind >= 0 && hasForm[kind];
		bool const has = host::StrategyForm_Has(kind);

		Check(has == expected, "kind " + std::to_string(kind) + (expected ? " opens its form" : " keeps inteng's page"));
		Check(host::StrategyForm_Rows(kind).empty() == !expected, "kind " + std::to_string(kind) + (expected ? " has rows" : " has no rows"));
	}

	// 61 LINKED SWITCH: the rows of its page and its P form (strategy_forms_switches.cpp), in their order, with their fields and rules
	std::vector<host::StrategyFormRow> const linked = host::StrategyForm_Rows(61);

	Check(linked.size() == 7, "61 has seven rows: Rest State, Mode, Seconds, Security, Switch off message, Target, Links");

	if (linked.size() == 7)
	{
		using K = host::StrategyFormRowKind;
		host::StrategyFormRow const &rest = linked[0], &mode = linked[1], &seconds = linked[2], &security = linked[3], &off = linked[4], &target = linked[5];

		Check(linked[6].kind == K::Links && !linked[6].shown, "Links: the switches that target it, read only, always shown");

		Check(rest.kind == K::Bits && rest.field == "flags" && rest.mask == 1 && rest.choices.size() == 2 && rest.choices[1].value == 1, "Rest State: bit 0 of the flags, Off / On");
		Check(mode.kind == K::Choice && mode.field == "Mode" && mode.choices.size() == 4 && mode.choices[0].value == 0 && mode.choices[1].value == 1 && mode.choices[2].value == 2 &&
		      mode.choices[3].value == 5, "Mode: Timer 0, Wait 1, Toggle 2, Time Delay (Autoexec) 5; no Time Delay 4 (inteng's form makes it 0 for a Linked Switch)");
		Check(seconds.kind == K::Number && seconds.field == "Time" && seconds.factor == 65536.0 && seconds.isFloat && seconds.lowest == 0.0 && seconds.highest == 30000.0,
		      "Seconds: the Time as 16.16, a real number 0 to 30000 (the P form's rule)");
		Check(security.kind == K::Number && security.field == "Security" && !security.isFloat && security.lowest == 0.0, "Security: a whole number, 0 or more");
		Check(off.kind == K::Bits && off.field == "flags" && off.mask == 6 && off.choices.size() == 3 && off.choices[0].value == 0 && off.choices[1].value == 2 && off.choices[2].value == 4,
		      "Switch off message: bits 1 and 2 of the flags, Reversed 0, Same 2, None 4");
		Check(target.kind == K::Target && target.field == "Target" && target.pickRow == host::kPickSwitchTarget && target.request == "request" && target.extra,
		      "Target: the switch target pick (a new target makes the request 0), with Extra data...");

		std::vector<uint8_t> const fresh = New(61);

		Check(!fresh.empty(), "a new Linked Switch is made");

		for (int32_t const now : { 0, 1, 2, 4, 5 })
		{
			std::vector<uint8_t> const payload = With(61, fresh, "Mode", now);
			int32_t read = -1;
			bool const timed = now == 0 || now == 4 || now == 5;

			Check(host::StrategyView_FieldNumber(61, payload, "Mode", &read) && read == now, "mode " + std::to_string(now) + " set in the record");
			Check(ShownAt(seconds, 61, payload) == timed, "mode " + std::to_string(now) + ": Seconds " + (timed ? "shown" : "hidden") + " (the page shows the time for 0, 4 and 5)");
			Check(ShownAt(off, 61, payload) == (now <= 2), "mode " + std::to_string(now) + ": Switch off message " + (now <= 2 ? "shown" : "hidden") + " (the page shows it for 0 to 2)");
			Check(ShownAt(rest, 61, payload) && ShownAt(mode, 61, payload) && ShownAt(security, 61, payload) && ShownAt(target, 61, payload),
			      "mode " + std::to_string(now) + ": the other rows always shown");
		}
	}

	// 66 MULTIPLE TARGET / 68 AREA SWITCH: the P form's rows, the targets as a List (read and written through the record's own layout), 68's trigger volume, the Links
	for (int32_t const kind : { 66, 68 })
	{
		std::string const k = std::to_string(kind);
		std::vector<host::StrategyFormRow> const rows = host::StrategyForm_Rows(kind);
		size_t const want = kind == 68 ? 8 : 7;

		Check(rows.size() == want, k + " has " + std::to_string(want) + " rows: Rest State, Mode, Seconds, Security, Switch off message, Targets," + (kind == 68 ? " Trigger Volume," : "") + " Links");

		if (rows.size() != want)
		{
			continue;
		}

		using K = host::StrategyFormRowKind;
		host::StrategyFormRow const &rest = rows[0], &mode = rows[1], &targets = rows[5];

		Check(rest.kind == K::Choice && rest.field == "RestState" && rest.choices.size() == 2, k + ": Rest State is its own word (RestState), Off / On");
		Check(mode.kind == K::Choice && mode.choices.size() == 5 && mode.choices[3].value == 4, k + ": Mode has all five, Time Delay 4 too");
		Check(rows[2].field == "Time" && rows[3].field == "Security" && rows[4].kind == K::Bits && rows[4].field == "flags" && rows[4].mask == 6, k + ": Seconds, Security, Switch off message");
		Check(targets.kind == K::List && targets.readList && targets.writeList && targets.entryChecks.size() == 1 && targets.entryChecks[0].mask == 1 && targets.newWord == 1 &&
		      targets.entryExtra && targets.entryDuplicate, k + ": Targets, a List: On (request bit 0), Extra data..., Duplicate; a new target is on (request 1)");
		Check(kind != 68 || (rows[6].kind == K::Action && rows[6].key == 'V'), k + ": the Area Switch's Trigger Volume runs V");
		Check(rows.back().kind == K::Links, k + ": the Links last");

		// the list round trip on a new record: none at first; two written (the second a copy of the first, off) and read back the same, the record's other bytes kept
		std::vector<uint8_t> const fresh = New(kind);
		std::vector<host::StrategyFormEntry> entries;

		Check(!fresh.empty() && targets.readList(fresh, &entries) && entries.empty(), k + ": a new one has no targets");

		host::StrategyFormEntry one;

		one.id = { 1, 2, 3, 4, 5, 6, 7, 8 };
		one.word = 1;
		entries = { one, one };
		entries[1].word = 0;

		std::vector<uint8_t> written;
		std::vector<host::StrategyFormEntry> back;

		Check(targets.writeList(fresh, entries, &written) && targets.readList(written, &back) && back.size() == 2 && back[0].id == one.id && back[0].word == 1 && back[1].word == 0,
		      k + ": two targets written and read back as they were");

		int32_t restBefore = -1, restAfter = -2;

		Check(host::StrategyView_FieldNumber(kind, fresh, "RestState", &restBefore) && host::StrategyView_FieldNumber(kind, written, "RestState", &restAfter) && restBefore == restAfter,
		      k + ": the fields before the list kept");
	}

	Check(host::StrategyForm_KindWords(61) == " : Linked Switch" && host::StrategyForm_KindWords(66) == " : Multiple Target Switch" && host::StrategyForm_KindWords(1).empty(),
	      "the title words are the pages' own");

	// paths (Frost): 72 / 80 / 76's EFFECTS, a list of lists read and written whole: the list kept sorted by the number, a number once (the first kept, with its targets),
	// each target's request and direction as written; 80 adds Integrity, the destruct target's on / off and the Destruct Target
	for (int32_t const kind : { 72, 80, 76 })
	{
		std::string const k = std::to_string(kind);
		std::vector<host::StrategyFormRow> const rows = host::StrategyForm_Rows(kind);
		size_t const want = kind == 80 ? 4 : 1;

		Check(rows.size() == want, k + " has " + std::to_string(want) + " rows");

		if (rows.size() != want)
		{
			continue;
		}

		host::StrategyFormRow const &effects = rows[0];

		Check(effects.kind == host::StrategyFormRowKind::List && effects.readList && effects.writeList && effects.entryWords && effects.newNumberRefused && effects.subRow &&
		      effects.subRow->entryChoices.size() == 4 && effects.subRow->newWord == 1 && effects.subRow->newChoice == 3, k + ": the effects, a list of lists; a new target on, both directions");
		Check(kind != 80 || (rows[1].field == "integrity" && rows[2].field == "target_request" && rows[3].field == "targetID"), k + ": Integrity, the destruct target's on / off, the Destruct Target");
		Check(effects.newNumberRefused(New(kind), 0).empty() && !effects.newNumberRefused(New(kind), -1).empty() && (kind != 76 || (effects.newNumberRefused(New(kind), 31).empty() &&
		      !effects.newNumberRefused(New(kind), 32).empty())), k + ": the number asked: 0 taken, -1 refused" + (kind == 76 ? std::string(", a flag 0 to 31") : std::string()));

		std::vector<uint8_t> const fresh = New(kind);
		std::vector<host::StrategyFormEntry> entries, back;

		Check(!fresh.empty() && effects.readList(fresh, &entries) && entries.empty(), k + ": a new one has no effects");

		host::StrategyFormEntry two, one, twoAgain, target;

		target.id = { 1, 2, 3, 4, 5, 6, 7, 8 };
		target.word = 1;
		target.choice = 7;
		two.word = 2;
		two.sub = { target };
		one.word = 1;
		twoAgain.word = 2;
		entries = { two, one, twoAgain };

		std::vector<uint8_t> written;

		Check(effects.writeList(fresh, entries, &written) && effects.readList(written, &back) && back.size() == 2 && back[0].word == 1 && back[0].sub.empty() && back[1].word == 2 &&
		      back[1].sub.size() == 1 && back[1].sub[0].id == target.id && back[1].sub[0].word == 1 && back[1].sub[0].choice == 7,
		      k + ": written sorted (1, 2), 2 once with its target (on, Opposite back 7), and read back so");
		Check(effects.entryWords(back[1]) == (kind == 76 ? "Flag 2" : "Point 2"), k + ": a line's words (" + effects.entryWords(back[1]) + ")");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
