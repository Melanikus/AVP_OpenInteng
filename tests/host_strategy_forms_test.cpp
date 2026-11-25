// The test of strategy_forms.h (Edit Strategy as a form of drop-downs, number boxes and buttons): which kinds open a form and the rows each one has. A kind not yet
// switched to its form must keep inteng's page (no rows), so the editor can be released after any merge. Needs nothing; runs everywhere.
// Written 2026-11-24 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "field_model.h"
#include "strategy_forms.h"

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

int main()
{
	setvbuf(stdout, nullptr, _IONBF, 0);

	// The kinds that have a form; every other kind keeps its page. Each group adds one line per kind UNDER ITS OWN LINE as it switches it on
	// (hasForm[61] = true;), so the four of us never edit the same lines.
	bool hasForm[128] = {};
	// switches (Vasquez): 61, 66, 68
	hasForm[61] = true;
	// modules (Drake): 51, 50, 57, 59, 54, 75, 64
	// objects (Dietrich): 62, 77, 78, 73, 70
	// paths (Frost): 72, 80, 76, 69, 79

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

	Check(host::StrategyForm_KindWords(61) == " : Linked Switch" && host::StrategyForm_KindWords(66) == " : Multiple Target Switch" && host::StrategyForm_KindWords(1).empty(),
	      "the title words are the pages' own");

	printf("%d checks, %d failed\n", checks, checksFailed);
	return checksFailed == 0 ? 0 : 1;
}
