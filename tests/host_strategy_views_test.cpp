// The test of the strategy views' clicks (src/host/strategy_views.cpp, StrategyView_Click): what a left click on a line of a kind's Edit view does, for the rows of
// AvP-docs\engine\inteng\reassess\strategy-data-kinds.md a picture can't show (a door with both kind bits only a hand-made file has, the bounds of a row, a number's limits). Written 2026-11-16 by Melanikus (Drake)
// for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// It reads no file: each strategy is made by the model's own creation (model::MakeNewStrategy, a new strategy as the old editor made it) and a field set in its bytes.
// The program gives back a non-zero result if any check failed. Each check's words say what it proves.

#include <stdio.h>
#include <string.h>

#include <string>
#include <vector>

#include "field_model.h"
#include "strategy_views.h"

using host::StrategyViewClick;

static int checksFailed = 0;

static void Check(bool good, std::string const &what)
{
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

// A new strategy of the kind, as the payload the views read (the chunk's bytes after its 12-byte heading)
static std::vector<uint8_t> New(int32_t kind)
{
	uint8_t bytes[1024];
	size_t length = 0;

	if (!model::MakeNewStrategy(kind, bytes, sizeof(bytes), &length))
	{
		return std::vector<uint8_t>();
	}

	return std::vector<uint8_t>(bytes, bytes + length);
}

static void Put32(std::vector<uint8_t> *payload, size_t at, int32_t value)
{
	if (payload->size() >= at + 4)
	{
		memcpy(payload->data() + at, &value, 4);
	}
}

static StrategyViewClick Click(int32_t kind, std::vector<uint8_t> const &payload, int32_t y)
{
	StrategyViewClick out;

	host::StrategyView_Click(kind, payload, 300, y, &out);
	return out;
}

int main()
{
	puts("The Door's first line (S-K50-6: 0x52cdf5-0x52ce0a, bit 8 is tested first):");

	std::vector<uint8_t> door = New(50);
	int32_t flagsAt = -1;

	// the flags word's place: found by setting it through the model's own reader
	for (size_t at = 0; at + 4 <= door.size() && flagsAt < 0; at += 4)
	{
		std::vector<uint8_t> probe = door;
		int32_t read = 0;

		Put32(&probe, at, 0x5a5a);

		if (host::StrategyView_FieldNumber(50, probe, "DoorFlags", &read) && read == 0x5a5a)
		{
			flagsAt = (int32_t)at;
		}
	}

	Check(!door.empty() && flagsAt >= 0, "a new Door is made and its flags word is found");

	auto doorWith = [&](int32_t flags) { std::vector<uint8_t> d = door; Put32(&d, (size_t)flagsAt, flags); return d; };

	StrategyViewClick c = Click(50, doorWith(8 | 2), 24);
	Check(c.action == StrategyViewClick::Set && c.field == "DoorFlags" && c.value == 2, "a door with BOTH bits (8 and 2) becomes a proximity door with bit 2 only, as inteng's");
	c = Click(50, doorWith(8), 24);
	Check(c.action == StrategyViewClick::Set && c.value == 2, "a switch door (bit 8) becomes a proximity door (bit 2)");
	c = Click(50, doorWith(2), 24);
	Check(c.action == StrategyViewClick::Set && c.value == 8, "a proximity door (bit 2) becomes a switch door (bit 8)");
	c = Click(50, doorWith(2 | 1 | 16), 24);
	Check(c.action == StrategyViewClick::Set && c.value == (8 | 1 | 16), "the other bits stay");

	puts("The Message's rows (S-K73-2, -3):");

	std::vector<uint8_t> message = New(73);
	int32_t stringAt = -1;

	for (size_t at = 0; at + 4 <= message.size() && stringAt < 0; at += 4)
	{
		std::vector<uint8_t> probe = message;
		int32_t read = 0;

		Put32(&probe, at, 4321);

		if (host::StrategyView_FieldNumber(73, probe, "message_string", &read) && read == 4321)
		{
			stringAt = (int32_t)at;
		}
	}

	Check(!message.empty() && stringAt >= 0, "a new Message is made and its string number is found");

	auto messageWith = [&](int32_t n) { std::vector<uint8_t> m = message; Put32(&m, (size_t)stringAt, n); return m; };

	c = Click(73, messageWith(0), 24);
	Check(c.action == StrategyViewClick::Toggle, "line 1 flips the start flag");
	c = Click(73, messageWith(0), 40);
	Check(c.action == StrategyViewClick::Window && c.pickRow == host::kWindowMessageText, "the String row opens the 500 x 400 window \"Message\"");
	c = Click(73, messageWith(0), 56);
	Check(c.action == StrategyViewClick::None, "with no string number the rows end at 48: a click at 56 is on nothing");
	c = Click(73, messageWith(5), 56);
	Check(c.action == StrategyViewClick::Window && c.pickRow == host::kWindowMessageText, "with a number from 1 to 450 the text row is drawn at 48, so the rows reach 63");
	c = Click(73, messageWith(5), 64);
	Check(c.action == StrategyViewClick::None, "and end there");
	c = Click(73, messageWith(451), 56);
	Check(c.action == StrategyViewClick::None, "a number above 450 has no text row");
	c = Click(73, messageWith(-1), 56);
	Check(c.action == StrategyViewClick::None, "nor has a negative one");

	// SKD-70-4 (reassess/strategy-kinds-dietrich.md; FUN_00532820 0x532b69): a new Mission Objective has its alterations label at 112 and "None" at 128, so its block ends at 144:
	// a click inside starts the target editor, a click below (by the P / V lines at the foot) is on nothing
	puts("The Mission Objective's alterations block (SKD-70-4):");

	std::vector<uint8_t> const objective = New(70);

	c = Click(70, objective, 120);
	Check(c.action == StrategyViewClick::Collect, "the block's label starts the target editor");
	c = Click(70, objective, 136);
	Check(c.action == StrategyViewClick::Collect, "so does its \"None\" row");
	c = Click(70, objective, 144);
	Check(c.action == StrategyViewClick::None, "the block ends after its last row: a click at 144 is on nothing");
	c = Click(70, objective, 400);
	Check(c.action == StrategyViewClick::None, "a click far below (by the P / V lines) is on nothing");

	// INTENG BUG, FIXED (section 15; S-K70-1, 0x5328c4): the Description's text row is there for a Description of 1 to 450, whatever the Completed number (inteng tests the bound on the
	// Completed number). A click at y 40 is on the Description's rows (window 0) when its text row is there, on the Completed line (window 1) when not
	puts("The Mission Objective's Description row (S-K70-1, INTENG BUG FIXED at 0x5328c4):");

	std::vector<uint8_t> described = New(70);

	Put32(&described, 20, 5);     // Description 5
	Put32(&described, 24, 500);   // Completed 500
	c = Click(70, described, 40);
	Check(c.action == StrategyViewClick::Window && c.index == 0, "Description 5, Completed 500: the Description's text row is there (y 32 to 47 is the Description's)");
	Put32(&described, 20, 500);   // Description 500
	Put32(&described, 24, 0);
	c = Click(70, described, 40);
	Check(c.action == StrategyViewClick::Window && c.index == 1, "Description 500 (past the table), Completed 0: no text row, y 40 is the Completed line");

	puts("The Self Destruct's time (S-K78-2):");

	c = Click(78, New(78), 24);
	Check(c.action == StrategyViewClick::Number && c.field == "timer" && c.title == "Time until destruction (seconds)" && c.clampBelow && c.lowest == 0,
		"line 1 asks \"Time until destruction (seconds)\"; a negative time is kept as 0");
	Check(c.highest == 32767, "NAMED: at most 32767, as the game shifts the seconds into 16.16 (object_setup.cpp add_selfdestruct: more would overflow a signed int)");

	puts("The Enemy's view (S-K69-1..5): its lines and clicks by species and mission:");

	std::vector<uint8_t> enemy = New(69);
	uint8_t const enemyId[8] = { 9, 9, 9, 9, 9, 9, 9, 0 };
	int32_t species = 7;
	host::StrategyViewWorld world;

	world.species = [&species](uint8_t const *) { return species; };
	world.path = [](int32_t number, std::string *name) { if (number == 3) { *name = "Upstairs"; return true; } if (number == 4) { name->clear(); return true; } return false; };

	auto linesOf = [&](int32_t kind, std::vector<uint8_t> const &payload) {
		std::vector<host::StrategyViewLine> lines;
		std::string all;

		host::StrategyView_Lines(kind, payload, "Bob", 729, [](uint8_t const *, std::string *) { return false; }, enemyId, &lines, &world);

		for (host::StrategyViewLine const &one : lines)
		{
			all += std::to_string(one.x) + "@" + std::to_string(one.y) + ":" + one.text + "|";
		}

		return all;
	};

	Check(!enemy.empty(), "a new Enemy is made");
	Check(linesOf(69, enemy) == "200@0:Bob : Enemy|100@709:E : Edit extra message data for target|16@16:Mission : Wait then wander (default)|16@32:Turn Death Target On|16@48:Death Target : None|",
		"a new marine: its title, the E line, \"Wait then wander (default)\", the death target lines at rows 2 and 3 (the new request word is 1)");

	std::vector<uint8_t> follower = enemy;

	Put32(&follower, 20, 5);
	Put32(&follower, 36, 0x00070003);
	Check(linesOf(69, follower).find("16@16:Mission : Path follower|16@32:Path - 3  Upstairs|16@48:Turn Death Target On|16@64:Death Target : None|") != std::string::npos,
		"a marine path follower: the path line (the low 16 bits: 3, its name) and the death target lines a row lower");
	Put32(&follower, 36, 4);
	Check(linesOf(69, follower).find("16@32:Path - 4|") != std::string::npos, "a path with no name is \"Path - %d\"");
	Put32(&follower, 36, 9);
	Check(linesOf(69, follower).find("16@32:Path - 9  Doesn't exist|") != std::string::npos, "a path the level hasn't: \"Path - %d  Doesn't exist\"");

	species = 1;
	Check(linesOf(69, follower).find("16@16:Mission : Wait then wander (Only option)|16@32:Turn Death Target On|") != std::string::npos, "an alien has one mission only, and no path line whatever its Mission");
	species = 2;
	Put32(&follower, 20, 1);
	Check(linesOf(69, follower).find("16@16:Mission : Path follower|16@32:Path - 9") != std::string::npos, "a predator's Mission 1 is Path follower");
	Put32(&follower, 20, 5);
	Check(linesOf(69, follower).find("16@16:Mission : Unknown Mission Type|16@32:Turn Death") != std::string::npos, "and its 5 is unknown");

	auto enemyClick = [&](std::vector<uint8_t> const &payload, int32_t y, int32_t who) { StrategyViewClick out; host::StrategyView_Click(69, payload, 300, y, &out, -1, who); return out; };

	Put32(&follower, 20, 5);
	c = enemyClick(enemy, 24, 7);
	Check(c.action == StrategyViewClick::Window && c.pickRow == host::kWindowEnemyMission, "a marine's Mission line opens Select Mission Type");
	c = enemyClick(enemy, 24, 1);
	Check(c.action == StrategyViewClick::None, "an alien's does nothing");
	c = enemyClick(follower, 40, 7);
	Check(c.action == StrategyViewClick::Window && c.pickRow == host::kWindowSelectPath, "a marine path follower's path line opens Select Path");
	c = enemyClick(follower, 56, 7);
	Check(c.action == StrategyViewClick::Toggle && c.field == "request" && c.mask == 1, "its \"Turn Death Target\" line (row 3) flips the request's bit 1");
	c = enemyClick(follower, 72, 7);
	Check(c.action == StrategyViewClick::Pick && c.field == "DeathTarget" && c.pickRow == host::kPickTarget && c.requestField == "request", "its death target line picks the target");
	c = enemyClick(enemy, 40, 7);
	Check(c.action == StrategyViewClick::Toggle, "with no path line, row 2 is the request line");

	puts("The Generator's view (S-K79-1..3):");

	std::vector<uint8_t> generator = New(79);

	Check(linesOf(79, generator) == "200@0:Bob : Generator|16@16:Mission : Hunt (default)|", "a new generator: Hunt, no E line, no target");
	Put32(&generator, 20, 1);
	Put32(&generator, 24, 3);
	Check(linesOf(79, generator) == "200@0:Bob : Generator|16@16:Mission : Path follower|16@32:Path - 3  Upstairs|", "a path follower shows its path");
	c = Click(79, generator, 24);
	Check(c.action == StrategyViewClick::Window && c.pickRow == host::kWindowGeneratorMission, "its Mission line opens Select Mission Type");
	c = Click(79, generator, 40);
	Check(c.action == StrategyViewClick::Window && c.pickRow == host::kWindowSelectPath, "its path line opens Select Path");
	Put32(&generator, 20, 0);
	c = Click(79, generator, 40);
	Check(c.action == StrategyViewClick::None, "a hunter has no path line");

	puts("The Placed Object's flags (S-K62-1, W-2): font16's glyph 1 (a tick) when set, glyph 3 (an X) when clear, as inteng draws them (0x530fd6-0x531036):");

	std::vector<uint8_t> placed = New(62);
	std::string const placedLines = linesOf(62, placed);

	Check(!placed.empty() && placedLines.find("|16@80:\x01|32@80:Object destroyed|16@96:\x03|32@96:Object picked up|") != std::string::npos,
		"a new Placed Object (flags 1): \"Object destroyed\" has the tick (code 1), \"Object picked up\" the X (code 3)");

	puts("76 Hierarchy Object (S-K76-1..4, FUN_00533a40): kind 72's effects, the pages one row higher (Vasquez 11-16):");
	{
		std::vector<uint8_t> hierarchy;
		std::vector<host::TrackEffect> flagEffects(1);

		flagEffects[0].point = 0;
		flagEffects[0].pad = 0;
		flagEffects[0].targets.resize(1);
		flagEffects[0].targets[0].id = { 1, 2, 3, 4, 5, 6, 7, 8 };
		flagEffects[0].targets[0].request = 1;
		flagEffects[0].targets[0].direction = 3;
		Check(host::StrategyView_TrackWith(New(76), flagEffects, &hierarchy), "a Hierarchy Object with flag 0's effect: one target, on, both directions");
		Check(linesOf(76, New(76)).find("200@0:Bob : Hierarchy|100@689:E : Edit extra message data for target|100@709:P : Add new key frame flag effect|16@16:Key Frame Flag Effects :|32@32:None|") == 0,
			"a new one: the title, the E and P lines, the label at 16, \"None\" at 32");
		Check(linesOf(76, hierarchy).find("|16@16:Key Frame Flag Effects :|32@32:Flag 0 : 1 targets|") != std::string::npos, "its effect's line at 32");

		world.page = 0;

		std::string const flagPage = linesOf(76, hierarchy);

		world.page = -1;
		Check(flagPage.find("|16@16:Flag 0 targets :|0@32:(on)|50@32:(Both directions)|190@32:") != std::string::npos, "flag 0's page: its target's row at 32");

		host::StrategyViewClick click;

		Check(host::StrategyView_Click(76, hierarchy, 0, 16, &click) && click.action == host::StrategyViewClick::Window && click.pickRow == host::kWindowTrackPoint,
			"the label (P presses y 16): \"Which flag?\"");
		Check(host::StrategyView_Click(76, hierarchy, 0, 40, &click) && click.pickRow == host::kOpenTrackPage && click.index == 0, "the effect's line: its flag page");
		Check(!host::StrategyView_Click(76, hierarchy, 0, 48, &click) && !host::StrategyView_Click(76, hierarchy, 0, 10, &click), "below the last line (16 x (2 + 1)) or above the label: nothing");
		Check(host::StrategyView_Click(76, hierarchy, 20, 40, &click, 0) && click.pickRow == host::kRunTrackRequest && click.index == 0, "the flag page's target row, x < 50: its on / off");
		Check(host::StrategyView_Click(76, hierarchy, 100, 40, &click, 0) && click.pickRow == host::kRunTrackDirection, "x 50 .. 189: its direction");
		Check(host::StrategyView_Click(76, hierarchy, 300, 40, &click, 0) && click.action == host::StrategyViewClick::Collect && click.index == 0, "x >= 190: the camera to it, the targets editor");
		Check(host::StrategyView_Click(76, hierarchy, 300, 20, &click, 0) && click.action == host::StrategyViewClick::Collect && click.index == -1, "the label line: the targets editor");
		Check(!host::StrategyView_Click(76, hierarchy, 300, 48, &click, 0), "at the end row: nothing");
	}

	puts("52 Binary Switch's presses (FUN_0052d700 0x52d9ee / 0x52da56; S-K52-3, S-K52-4):");
	{
		host::StrategyViewClick click;

		Check(host::StrategyView_Click(52, New(52), 300, 0x20, &click) && click.action == host::StrategyViewClick::Window && click.pickRow == host::kWindowSwitchProperties &&
		      host::StrategyView_Click(52, New(52), 300, 0x4f, &click) && click.pickRow == host::kWindowSwitchProperties, "y 0x20 .. 0x4f (Rest State, Mode, Switch off): the Switch Properties form (P presses 0x20)");
		Check(host::StrategyView_Click(52, New(52), 300, 0x50, &click) && click.pickRow == host::kPickSwitchTarget && host::StrategyView_Click(52, New(52), 300, 0x5f, &click) &&
		      click.pickRow == host::kPickSwitchTarget, "y 0x50 .. 0x5f (the target): the target pick (T presses 0x50)");
		Check(!host::StrategyView_Click(52, New(52), 300, 0x1f, &click) && !host::StrategyView_Click(52, New(52), 300, 0x60, &click), "above 0x20 or below 0x5f: nothing");
	}

	puts("61 Linked Switch's view (code 0x52dfd0 -> FUN_0052d700 0x52d8f0-0x52d989; S-K61-1):");
	{
		std::vector<std::array<uint8_t, 8>> links;
		host::StrategyViewWorld linked;
		host::StrategyViewClick linkedClick;
		auto const linkedLines = [&]() {
			std::vector<host::StrategyViewLine> lines;
			std::string all;

			host::StrategyView_Lines(61, New(61), "Sam", 729, [](uint8_t const *id, std::string *name) { *name = id[0] == 1 ? "Ann" : "Ben"; return true; }, enemyId, &lines, &linked);

			for (host::StrategyViewLine const &one : lines)
			{
				all += std::to_string(one.x) + "@" + std::to_string(one.y) + ":" + one.text + "|";
			}

			return all;
		};

		linked.links = &links;
		linked.strategyKind = [](uint8_t const *id) { return id[0] == 1 ? 52 : 66; };

		std::string const none = linkedLines();

		Check(none.find("200@0:Sam : Linked Switch|100@669:E : Edit extra message data for target|100@689:P : Change Properties|") == 0 &&
		      none.find("|16@120:Links:|32@136:None|") != std::string::npos, "no links: 52's lines with \" : Linked Switch\", then \"Links:\" at (0x10, 0x78) and \"None\" at (0x20, 0x88)");

		links.push_back({ 1, 0, 0, 0, 0, 0, 0, 0 });
		links.push_back({ 2, 0, 0, 0, 0, 0, 0, 0 });

		std::string const two = linkedLines();

		Check(two.find("|16@120:Links:|20@136:bs|64@136:Ann|20@152:ls|64@152:Ben|") != std::string::npos && two.find("32@136:None") == std::string::npos,
			"two links: \"bs\" for a Binary Switch, \"ls\" for any other, at (0x14, 0x88 + 16 i), the name after the tab at 64; no \"None\"");
		Check(host::StrategyView_Click(61, New(61), 300, 0x30, &linkedClick) && linkedClick.pickRow == host::kWindowSwitchProperties &&
		      host::StrategyView_Click(61, New(61), 300, 0x50, &linkedClick) && linkedClick.pickRow == host::kPickSwitchTarget, "its presses are 52's: the form, the target pick");
	}

	puts("E: which page a target opens (S-KC-8; FUN_00529b30 -> FUN_00529570 -> the wrapper's +0x10):");

	using host::RequestPage;

	Check(host::StrategyView_RequestPageOf(false, false, -1, false, 0) == RequestPage::Nothing, "an ID no object has (or the view's own): nothing");
	Check(host::StrategyView_RequestPageOf(true, false, -1, false, 0) == RequestPage::Nothing, "a target with no strategy: nothing (FUN_005393a0 0x5393a3)");
	Check(host::StrategyView_RequestPageOf(true, true, 7, false, 0) == RequestPage::Enemy && host::StrategyView_RequestPageOf(true, true, 7, true, 69) == RequestPage::Enemy,
		"a marine: its own page, with or without a strategy (the enemy wrapper's FUN_0053a470)");
	Check(host::StrategyView_RequestPageOf(true, true, 3, true, 69) == RequestPage::NotMarine, "a facehugger: \"Only available for marines/civilians\"");
	Check(host::StrategyView_RequestPageOf(true, false, -1, true, 62) == RequestPage::PlacedObject && host::StrategyView_RequestPageOf(true, false, -1, true, 66) == RequestPage::Switch &&
	      host::StrategyView_RequestPageOf(true, false, -1, true, 68) == RequestPage::Switch && host::StrategyView_RequestPageOf(true, false, -1, true, 70) == RequestPage::Mission &&
	      host::StrategyView_RequestPageOf(true, false, -1, true, 72) == RequestPage::Track && host::StrategyView_RequestPageOf(true, false, -1, true, 80) == RequestPage::Track &&
	      host::StrategyView_RequestPageOf(true, false, -1, true, 73) == RequestPage::Message, "62, 66 / 68, 70, 72 / 80 and 73 have pages of their own (their +0x18)");
	Check(host::StrategyView_RequestPageOf(true, false, -1, true, 50) == RequestPage::NotUsed && host::StrategyView_RequestPageOf(true, false, 0, true, 79) == RequestPage::NotUsed,
		"a door, or a generator's 79: FUN_0052b220, \"doesn't use extended request data\"");
	Check(host::StrategyView_RequestPageOf(true, false, -1, true, 77) == RequestPage::NotUsed && host::StrategyView_RequestPageOf(true, false, -1, true, 78) == RequestPage::NotUsed,
		"S-K77-E / S-K78-E: a Volume of Death's and a Self Destruct's +0x18 is the default FUN_0052b220 (\"doesn't use extended request data\")");

	host::RequestPageState page;
	std::vector<host::StrategyViewLine> pageLines;
	auto pageText = [&](int32_t height) {
		std::string all;

		host::StrategyView_RequestLines(&page, height, [](int32_t number, std::string *name) { if (number == 3) { *name = "Upstairs"; return true; } return false; }, &pageLines);

		for (host::StrategyViewLine const &one : pageLines)
		{
			all += std::to_string(one.x) + "@" + std::to_string(one.y) + ":" + one.text + "|";
		}

		return all;
	};

	puts("E on a Placed Object target (S-K62-E, FUN_00531240):");
	Check(!host::StrategyView_RequestOpen(RequestPage::NotUsed, 5, "x", 0, 0, &page), "NotUsed opens no page (the caller says the message)");
	Check(host::StrategyView_RequestOpen(RequestPage::PlacedObject, 0x81 * 2 + 1, "lamp", 16, 300, &page) && page.low == 1 && page.data == 0x81, "the word cut to bit 0 and the data = word >> 1");
	Check(pageText(729) == "100@0:Target  -  lamp : Placed object|32@16:Set integrity to 1|16@16:\x01|32@32:Change type to standard|16@32:\x03|32@48:Change type to flicker|16@48:\x03|"
	      "32@64:Change type to strobe|16@64:\x03|32@80:Swap 'up' and 'down' colours|16@80:\x03|", "a light target with integrity 1 set: its lines and boxes (glyph 1 set, 3 clear)");
	Check(host::StrategyView_RequestClick(&page, 24) == host::RequestAsk::None && page.data == 0, "a click on the set integrity clears it and its bit (data & 0xffff807e)");
	Check(host::StrategyView_RequestClick(&page, 24) == host::RequestAsk::Integrity, "a click on the unset integrity asks \"New integrity value\"");
	host::StrategyView_RequestIntegrity(&page, 300);
	Check(page.data == ((255u << 7) | 1u), "an answer above 254 is 255, bit 1 set");
	host::StrategyView_RequestIntegrity(&page, -5);
	Check(page.data == 1u, "below 1 is 0");
	host::StrategyView_RequestClick(&page, 40);
	host::StrategyView_RequestClick(&page, 56);
	Check((page.data & 0xeu) == 8u, "standard, then flicker: one of the three at most (bits 2..8 cleared, the new one set)");
	host::StrategyView_RequestClick(&page, 56);
	Check((page.data & 0xeu) == 0u, "flicker again: none");
	host::StrategyView_RequestClick(&page, 88);
	Check((page.data & 0x10u) != 0 && host::StrategyView_RequestWord(page) == (page.low | (page.data << 1)), "swap flips; the word put back is bit 0 | data << 1");
	host::StrategyView_RequestOpen(RequestPage::PlacedObject, 0, "box", 0, 2, &page);
	Check(pageText(729) == "100@0:Target  -  box : Placed object|32@16:Set integrity|16@16:\x03|", "a target that is no light: only the integrity line");
	host::StrategyView_RequestClick(&page, 40);
	Check(page.data == 0, "and its light rows are not there to click");

	puts("E on a Mission Objective target (S-K70-E, 0x5333a0): the third box the other way round; each pass draws only its own lines (INTENG BUG FIXED: inteng's list kept the old ones):");
	host::StrategyView_RequestOpen(RequestPage::Mission, 0, "obj", 0, 0, &page);
	Check(pageText(729).find("16@48:\x03|") != std::string::npos && pageText(729).find("16@80:\x01|") != std::string::npos, "data 0: the first box an X, the third a TICK (0x5334cf: the JZ takes \"\\x01\" when bit 4 is clear)");
	host::StrategyView_RequestClick(&page, 56);
	Check(page.data == 1u && pageText(729).find("16@48:\x03|") == std::string::npos && pageText(729).find("16@48:\x01|") != std::string::npos,
		"a click flips bit 1: the new mark only (INTENG BUG, FIXED: inteng never empties its list between passes, 0x5333b1, so the old mark stayed drawn under the new one)");
	Check(!host::StrategyView_RequestEscapeEndsView(RequestPage::Mission) && !host::StrategyView_RequestEscapeEndsView(RequestPage::Track) && !host::StrategyView_RequestEscapeEndsView(RequestPage::Message) &&
	      !host::StrategyView_RequestEscapeEndsView(RequestPage::PlacedObject) && !host::StrategyView_RequestEscapeEndsView(RequestPage::Switch) && !host::StrategyView_RequestEscapeEndsView(RequestPage::Enemy),
		"Escape on any page ends that page alone (INTENG BUG, FIXED: the Mission, Track and Message pages left the Escape slot set, 0x5335d3 / 0x53535c / 0x5339f4, and ended the view under them too)");

	puts("E on a Track target (S-K72-E, 0x535210):");
	host::StrategyView_RequestOpen(RequestPage::Track, 2 * 1 + 0, "trk", 0, 0, &page);
	Check(pageText(729) == "100@0:Target  -  trk : Track object|32@16:Start track|16@32:\x02|32@32:Start track forward|32@48:Start track backward|", "data 1: the mark beside \"Start track forward\"");
	host::StrategyView_RequestClick(&page, 50);
	Check(page.data == 2u && host::StrategyView_RequestWord(page) == 4u, "a click on the third line: data 2, the word 4");

	puts("E on a Message target (S-K73-E, FUN_00533880):");
	host::StrategyView_RequestOpen(RequestPage::Message, 2 * 5 + 1, "msg", 0, 0, &page);
	Check(page.data == 0u && pageText(729) == "100@0:Target  -  msg : Message|100@709:Left button : Cycle through options|200@32:Current effect of on/off message :|0@48:On :|64@48:Display message|0@64:Off :|64@64:Ignored|",
		"data 5 is made 0; the words after the tab at 64");
	host::StrategyView_RequestClick(&page, 300);
	host::StrategyView_RequestClick(&page, 300);
	Check(page.data == 2u && pageText(729).find("64@48:Activate and display message|") != std::string::npos, "two clicks anywhere: data 2");
	host::StrategyView_RequestClick(&page, 300);
	Check(page.data == 0u && host::StrategyView_RequestWord(page) == 1u, "a third: back to 0, bit 0 kept");

	puts("E on a placed hierarchy target (S-K76-E, FUN_0053b830): its own page, the word NOT cut (Vasquez 11-16):");
	Check(host::StrategyView_RequestPageOf(true, false, -1, false, 0, true) == RequestPage::PlacedHierarchy && host::StrategyView_RequestPageOf(true, false, -1, true, 76, true) == RequestPage::PlacedHierarchy &&
	      host::StrategyView_RequestPageOf(false, false, -1, false, 0, true) == RequestPage::Nothing, "a placed hierarchy: its page with or without a strategy (the wrapper's +0x10); none found: nothing");
	host::StrategyView_RequestOpen(RequestPage::PlacedHierarchy, 0x00010301u, "dropship ken", 0, 0, &page);
	Check(pageText(729) == "100@0:Target  -  dropship ken : Placed hierarchy|16@16:Sequence index 3|", "the title and \"Sequence index %d\" = the word's byte 1 (0x00010301: 3)");
	Check(host::StrategyView_RequestClick(&page, 10) == host::RequestAsk::None && host::StrategyView_RequestClick(&page, 32) == host::RequestAsk::None &&
	      host::StrategyView_RequestClick(&page, 16) == host::RequestAsk::SelectSequence && host::StrategyView_RequestClick(&page, 31) == host::RequestAsk::SelectSequence,
		"only a press at 16 <= y < 32 asks for a sequence");
	host::StrategyView_RequestSequence(&page, 5);
	Check(host::StrategyView_RequestWord(page) == 0x00010501u && pageText(729).find("16@16:Sequence index 5|") != std::string::npos, "sequence 5 chosen: bits 8-15 only, every other bit of the word kept");
	Check(!host::StrategyView_RequestEscapeEndsView(RequestPage::PlacedHierarchy), "Escape on it does not end the view under it (0x53bc02 clears the slot)");

	puts("E on a marine target (S-K69-E, FUN_0053a470):");
	host::StrategyView_RequestOpen(RequestPage::Enemy, 0, "Bob", 0, 0, &page);
	Check(pageText(729).find("|0@16:\x02|") != std::string::npos, "no change: the mark at the first line");
	Check(host::StrategyView_RequestClick(&page, 100) == host::RequestAsk::SelectPath && page.data == ((4u << 7) | 1u), "Change to Path follower: m 4, bit 1, the path asked");
	host::StrategyView_RequestPath(&page, 3);
	Check(pageText(729).find("|0@96:\x02|16@112:Path - 3  Upstairs|") != std::string::npos, "the mark at Path follower and \"Path - 3  Upstairs\"");
	host::StrategyView_RequestClick(&page, 40);
	Check(page.data == ((3u << 15) | 1u), "Change to Wander keeps the path bits (data & 0xffff807f)");
	host::StrategyView_RequestClick(&page, 20);
	Check(page.data == (3u << 15), "No change clears bit 1 only");
	host::StrategyView_RequestClick(&page, 120);
	Check(page.data == (3u << 15), "below the last line: nothing");

	puts("E on a Multi / Area Switch target (S-84, FUN_00530510): the word's bit 2:");
	host::StrategyView_RequestOpen(RequestPage::Switch, 1, "sw", 0, 0, &page);
	Check(pageText(729).find("16@48:\x03|") != std::string::npos, "the page drawn once: bit 2 clear, its box an X");
	host::StrategyView_RequestClick(&page, 50);
	Check(host::StrategyView_RequestWord(page) == 3u && pageText(729).find("16@48:\x01|") != std::string::npos, "a click on \"Part of link switch set\" sets bit 2 (word 1 -> 3)");
	Check(pageText(729).find("16@48:\x03|") == std::string::npos, "and only the new mark is drawn (INTENG BUG, FIXED: inteng's list, emptied once at 0x53051c, kept the old X under it)");

	puts("66 Multiple Target Switch (S-K66-2..7, FUN_0052f700): its targets written back, the row under a y, its presses:");
	{
		std::vector<uint8_t> const empty = New(66);
		std::vector<host::MultiTarget> three(3);

		for (size_t i = 0; i < three.size(); i++)
		{
			three[i].id.fill((uint8_t)(0x11 * (i + 1)));
			three[i].request = (uint32_t)i;
		}

		std::vector<uint8_t> multi;
		std::vector<host::MultiTarget> back;
		int32_t flagsWas = 0, flagsNow = 0, mode = -1;

		Check(host::StrategyView_MultiWith(empty, three, &multi) && multi.size() == empty.size() + 36, "three targets written into a new 66: 36 bytes more");
		Check(host::StrategyView_MultiTargets(multi, &back) && back.size() == 3 && back[2].id[0] == 0x33 && back[2].request == 2u, "and read back in order, with their requests");
		host::StrategyView_FieldNumber(66, empty, "flags", &flagsWas);
		host::StrategyView_FieldNumber(66, multi, "flags", &flagsNow);
		host::StrategyView_FieldNumber(66, multi, "Mode", &mode);
		Check(flagsWas == flagsNow && mode == 1, "the flags after the list kept (" + std::to_string(flagsNow) + "), the mode read (Wait: 1, so the Targets line is at 80)");

		size_t index = 99;

		Check(!host::StrategyView_MultiTargetAt(multi, 0, 88, &index), "y 88, the \"Targets :\" line itself: no row (INTENG BUG, FIXED: inteng's S flipped the first target there)");
		Check(host::StrategyView_MultiTargetAt(multi, 0, 96, &index) && index == 0, "y 96: row 0");
		Check(host::StrategyView_MultiTargetAt(multi, 0, 127, &index) && index == 1, "y 127: row 1");
		Check(host::StrategyView_MultiTargetAt(multi, 0, 128, &index) && index == 2, "y 128: row 2");
		Check(!host::StrategyView_MultiTargetAt(multi, 0, 144, &index), "y 144: past the last target, no row");
		Check(host::StrategyView_MultiTargetAt(multi, 5, 96, &index) && index == 0, "a page past the last shows the first: y 96 is row 0");

		StrategyViewClick press;

		host::StrategyView_Click(66, multi, 20, 112, &press);
		Check(press.action == StrategyViewClick::Run && press.pickRow == host::kRunMultiToggle && press.index == 1, "a press at x 20 on row 1: its request's bit flips (S)");
		host::StrategyView_Click(66, multi, 300, 96, &press);
		Check(press.action == StrategyViewClick::Collect && press.pickRow == host::kCollectMultiTargets && press.index == 0, "a press at x 300 on row 0: to its target, then the collector (T)");
		host::StrategyView_Click(66, multi, 300, 84, &press);
		Check(press.action == StrategyViewClick::Collect && press.index == -1, "a press at x 300 on the Targets line: the collector, no row");
		host::StrategyView_Click(66, multi, 300, 0x30, &press);
		Check(press.action == StrategyViewClick::Window && press.pickRow == host::kWindowSwitchProperties, "a press on y 0x30, above the Targets line: the form (P)");

		std::vector<uint8_t> hostile = multi;

		Put32(&hostile, 36, 1000000);
		Check(!host::StrategyView_MultiTargets(hostile, &back) && back.empty() && !host::StrategyView_MultiTargetAt(hostile, 0, 96, &index) && !host::StrategyView_MultiWith(hostile, three, &multi),
		      "a count the bytes do not hold (hostile): no list, no row, nothing written");
	}

	if (checksFailed)
	{
		printf("FAILED host_strategy_views_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_strategy_views_test\n");
	return 0;
}
