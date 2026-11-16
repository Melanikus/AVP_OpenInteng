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

	if (checksFailed)
	{
		printf("FAILED host_strategy_views_test (%d checks failed)\n", checksFailed);
		return 1;
	}

	printf("passed host_strategy_views_test\n");
	return 0;
}
