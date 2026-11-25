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

// THE OBJECTS' ROWS (Dietrich, strategy_forms_objects.cpp): each kind's rows in their order, with the fields, bits, ranges and page actions of its page (strategy_views.cpp), and
// every field one the model knows for that kind (so a misspelt name can't make a row that writes nothing)
static void ObjectsRows()
{
	using K = host::StrategyFormRowKind;

	auto const knows = [](int32_t kind, std::string const &field)
	{
		int32_t read = 0;
		std::vector<uint8_t> const fresh = New(kind);

		return !fresh.empty() && host::StrategyView_FieldNumber(kind, fresh, field.c_str(), &read);
	};

	std::vector<host::StrategyFormRow> const destruct = host::StrategyForm_Rows(78);
	Check(destruct.size() == 1 && destruct[0].kind == K::Number && destruct[0].field == "timer" && !destruct[0].isFloat && destruct[0].lowest == 0.0 &&
	      destruct[0].highest == 32767.0 && destruct[0].factor == 1.0 && knows(78, "timer"), "78 Self Destruct: its Time, a whole number of seconds, 0 to 32767");

	std::vector<host::StrategyFormRow> const death = host::StrategyForm_Rows(77);
	Check(death.size() == 4, "77 Volume of Death has four rows: Starts, Collision, Damage per second, Volume");

	if (death.size() == 4)
	{
		Check(death[0].kind == K::Bits && death[0].field == "flags" && death[0].mask == 1 && death[0].choices.size() == 2 && death[0].choices[0].value == 0 &&
		      death[0].choices[1].value == 1 && knows(77, "flags"), "77 Starts: bit 0 of the flags, Inactive 0 / Active 1");
		Check(death[1].kind == K::Bits && death[1].field == "flags" && death[1].mask == 2 && death[1].choices.size() == 2 && death[1].choices[0].value == 0 &&
		      death[1].choices[1].value == 2, "77 Collision: bit 1 of the flags, only harmful on collision 0 / not required 2");
		Check(death[2].kind == K::Number && death[2].field == "damage" && !death[2].isFloat && death[2].lowest == 0.0 && death[2].highest == 2147483647.0 && knows(77, "damage"),
		      "77 Damage per second: a whole number, 0 or more");
		Check(death[3].kind == K::Action && death[3].key == 'V' && death[3].pageY < 0 && death[3].button == "Edit Volume", "77 Volume: the page's V, Edit Volume");
	}

	std::vector<host::StrategyFormRow> const placed = host::StrategyForm_Rows(62);
	Check(placed.size() == 4, "62 Placed Object has four rows: Target request, Event Target, Object destroyed, Object picked up");

	if (placed.size() == 4)
	{
		Check(placed[0].kind == K::Bits && placed[0].field == "target_request" && placed[0].mask == 1 && placed[0].choices.size() == 2 && placed[0].choices[0].value == 0 &&
		      placed[0].choices[1].value == 1 && knows(62, "target_request"), "62 Target request: bit 0 only (Turn Target Off 0 / On 1), its higher bits kept");
		Check(placed[1].kind == K::Target && placed[1].field == "targetID" && placed[1].pickRow == host::kPickTarget && placed[1].request == "target_request" && placed[1].extra,
		      "62 Event Target: the page's pick rule, its request word, and Extra data... (the page's E)");
		Check(placed[2].kind == K::Check && placed[2].field == "flags" && placed[2].mask == 1 && placed[3].kind == K::Check && placed[3].field == "flags" && placed[3].mask == 2 &&
		      knows(62, "flags"), "62 Object destroyed: bit 0 of the flags; Object picked up: bit 1");
	}

	std::vector<host::StrategyFormRow> const message = host::StrategyForm_Rows(73);
	Check(message.size() == 3, "73 Message has three rows: Starts, String, Video");

	if (message.size() == 3)
	{
		Check(message[0].kind == K::Bits && message[0].field == "flags" && message[0].mask == 1 && message[0].choices.size() == 2 && message[0].choices[0].value == 0 &&
		      message[0].choices[1].value == 1 && knows(73, "flags"), "73 Starts: bit 0 of the flags, Activated 0 / Deactivated 1");
		Check(message[1].kind == K::Number && message[1].field == "message_string" && !message[1].isFloat && message[1].lowest == 0.0 && message[1].highest == 450.0 &&
		      knows(73, "message_string"), "73 String: a whole number, 0 to 450 (the loader's range)");

		// the Video button is the page's press on its String line: that press must open the Message window, as the page's click does (strategy_views.cpp MessageClick)
		host::StrategyViewClick press;

		Check(message[2].kind == K::Action && message[2].pageY >= 0 && host::StrategyView_Click(73, New(73), message[2].pageX, message[2].pageY, &press) &&
		      press.action == host::StrategyViewClick::Window && press.pickRow == host::kWindowMessageText, "73 Video: the press on the page's String line, which opens the Message window (Play)");
	}

	std::vector<host::StrategyFormRow> const mission = host::StrategyForm_Rows(70);
	Check(mission.size() == 7, "70 Mission Objective has seven rows: Description, Completed Message, Order Number, Shown, Possible, Special, Alterations");

	if (mission.size() == 7)
	{
		Check(mission[0].kind == K::Number && mission[0].field == "mission_description_string" && mission[0].lowest == 0.0 && mission[0].highest == 450.0 &&
		      knows(70, "mission_description_string"), "70 Description: a whole number, 0 to 450 (the loader's range)");
		Check(mission[1].kind == K::Number && mission[1].field == "mission_complete_string" && mission[1].lowest == 0.0 && mission[1].highest == 450.0 &&
		      knows(70, "mission_complete_string"), "70 Completed Message: a whole number, 0 to 450");
		Check(mission[2].kind == K::Number && mission[2].field == "mission_number" && !mission[2].isFloat && mission[2].lowest == -2147483648.0 &&
		      mission[2].highest == 2147483647.0 && knows(70, "mission_number"), "70 Order Number: any whole number (inteng made no check)");
		Check(mission[3].kind == K::Bits && mission[3].field == "flags" && mission[3].mask == 1 && mission[3].choices.size() == 2 && mission[3].choices[1].value == 1 &&
		      mission[4].kind == K::Bits && mission[4].field == "flags" && mission[4].mask == 2 && mission[4].choices.size() == 2 && mission[4].choices[1].value == 2 &&
		      knows(70, "flags"), "70 Shown: bit 0 of the flags; Possible: bit 1");
		Check(mission[5].kind == K::Choice && mission[5].field == "mission_completion_effect" && mission[5].choices.size() == 2 && mission[5].choices[0].value == 0 &&
		      mission[5].choices[1].value == 1 && knows(70, "mission_completion_effect"), "70 Special: Nothing 0, Complete level 1");

		host::StrategyFormRow const &list = mission[6];

		Check(list.kind == K::List && list.entryChecks.size() == 2 && list.entryChecks[0].mask == 1u && list.entryChecks[1].mask == 2u && list.newWord == 3u &&
		      list.targetKinds == std::vector<int32_t>({ 70, 71 }) && !list.entryExtra && !list.entryDuplicate,
		      "70 Alterations: a list of objectives only (70, 71), Visible bit 0 and Possible bit 1 of each word, a new one with both");

		// the list's read and write go through the page's own reader and writer (strategy_views StrategyView_MissionTargets / _WithMissionTargets)
		std::vector<uint8_t> const fresh = New(70);
		std::vector<host::StrategyFormEntry> entries, back;
		std::vector<uint8_t> written;

		Check(list.readList && list.writeList && list.readList(fresh, &entries) && entries.empty(), "a new 70 reads an empty list");

		entries.resize(2);
		entries[0].id.fill(0x11);
		entries[0].word = 3u;
		entries[1].id.fill(0x22);
		entries[1].word = 1u;
		Check(list.writeList(fresh, entries, &written) && written.size() == fresh.size() + 24 && list.readList(written, &back) && back.size() == 2 && back[0].word == 3u &&
		      back[1].id[0] == 0x22 && back[1].word == 1u, "two entries written and read back in order, with their words");

		std::vector<uint8_t> hostile = written;
		int32_t const huge = 1000000;

		if (hostile.size() >= 44)
		{
			memcpy(hostile.data() + 40, &huge, 4);
		}

		back.clear();
		Check(hostile.size() >= 44 && !list.readList(hostile, &back) && back.empty(), "a count the bytes don't hold (hostile): no list");
	}
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

std::vector<int32_t> host::StrategyFormsPaths_HierarchySequences(uint8_t const id[8])
{
	// a placed hierarchy with two sequences, 3 and 300 (300: past the 8 bits a word holds, not offered), for the ID 1..8; none for any other
	uint8_t const one[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };

	return memcmp(id, one, 8) == 0 ? std::vector<int32_t>{ 3, 300 } : std::vector<int32_t>();
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
	hasForm[59] = true;   // Switch Door (Drake 2026-11-25)
	// objects (Dietrich): 62, 77, 78, 73, 70
	hasForm[78] = true;
	hasForm[77] = true;
	hasForm[62] = true;
	hasForm[73] = true;
	hasForm[70] = true;
	ObjectsRows();
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

	// paths (Frost): THE E PAGES' rows, each page's (StrategyFormRows_Extra), on the page's data; no form for NotUsed / NotMarine / Nothing, nor for a placed hierarchy with no sequence
	{
		using P = host::RequestPage;
		using K = host::StrategyFormRowKind;
		host::RequestPageState state;
		std::vector<host::StrategyFormRow> rows;

		Check(host::StrategyFormRows_Extra(P::Switch, state, &rows) && rows.size() == 1 && rows[0].kind == K::Check && rows[0].mask == 1 && rows[0].field == "request.data",
		      "E, a switch: one box, \"Part of link switch set\", data bit 0");
		Check(host::StrategyFormRows_Extra(P::Mission, state, &rows) && rows.size() == 3 && rows[0].mask == 1 && rows[1].mask == 2 && rows[2].mask == 4, "E, a mission: three boxes, bits 0, 1, 2");
		Check(host::StrategyFormRows_Extra(P::Track, state, &rows) && rows.size() == 1 && rows[0].kind == K::Choice && rows[0].choices.size() == 3 && rows[0].choices[2].value == 2,
		      "E, a track: start, forward, backward (0, 1, 2)");
		Check(host::StrategyFormRows_Extra(P::Message, state, &rows) && rows.size() == 1 && rows[0].choices.size() == 3, "E, a message: its three options");
		Check(host::StrategyFormRows_Extra(P::Enemy, state, &rows) && rows.size() == 2 && rows[0].mask == 0x7f81u && rows[0].choices.size() == 6 && rows[0].choices[5].value == 0x201 &&
		      rows[1].mask == 0x7f8000u && rows[1].shown && !rows[1].shown(69, { 0x81, 0, 0, 0 }) && rows[1].shown(69, { 0x01, 0x02, 0, 0 }),
		      "E, a marine: the change (bit 0, bits 7 up), the path only for a path follower (change 4)");

		state.targetType = 0;

		Check(host::StrategyFormRows_Extra(P::PlacedObject, state, &rows) && rows.size() == 2 && rows[0].clearAlso == 0x7f80u && rows[1].kind == K::Number && rows[1].mask == 0x7f80u &&
		      rows[1].shift == 7 && rows[1].alsoBits == 1u && rows[1].highest == 255.0, "E, a placed object: Set integrity (unticked clears it), the integrity in bits 7 to 14, setting bit 0");

		state.targetType = 16;

		Check(host::StrategyFormRows_Extra(P::PlacedObject, state, &rows) && rows.size() == 4 && rows[2].mask == 0xEu && rows[3].mask == 0x10u, "E, a placed light: its type and the swap too");

		state.targetId = { 1, 2, 3, 4, 5, 6, 7, 8 };

		Check(host::StrategyFormRows_Extra(P::PlacedHierarchy, state, &rows) && rows.size() == 1 && rows[0].mask == 0xff00u && rows[0].choices.size() == 1 && rows[0].choices[0].value == 3 << 8,
		      "E, a placed hierarchy: its sequences (3; not 300, past 8 bits) in bits 8 to 15");

		state.targetId = { 9, 9, 9, 9, 9, 9, 9, 9 };

		Check(!host::StrategyFormRows_Extra(P::PlacedHierarchy, state, &rows) && !host::StrategyFormRows_Extra(P::NotUsed, state, &rows) && !host::StrategyFormRows_Extra(P::NotMarine, state, &rows) &&
		      !host::StrategyFormRows_Extra(P::Nothing, state, &rows), "E: no form for a placed hierarchy with no sequence, NotUsed, NotMarine, Nothing (their message, as before)");
	}

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
