// The test for the search of new maps for object shapes, one map a frame (src/host/shapes_trigger.h): maps on the list skipped, new ones searched and put on
// it (a failed search too), a map that can't be read named and not listed, two copies of one map searched once, the list's most, the status line's words,
// hostile file names. The reading and the search are made-up here (a table of maps), so no file is touched. Written 2026-11-23 by Melanikus (Drake) for
// AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// The program gives back a non-zero result if any check failed.

#include <stdint.h>
#include <stdio.h>

#include <map>
#include <string>
#include <vector>

#include "shapes_trigger.h"

using namespace host;

static int checksFailed = 0;
static int checksRun = 0;

static void Check(bool good, std::string const &what)
{
	checksRun++;
	printf("  %s  %s\n", good ? "ok" : "FAILED", what.c_str());

	if (!good)
	{
		checksFailed++;
	}
}

static std::string Sha(int k)
{
	std::string text(64, '0');

	for (int i = 0; i < 64; i++)
	{
		text[i] = "0123456789abcdef"[(k * 5 + i * 3 + (k >> (i % 12))) & 15];
	}

	return text;
}

// The made-up maps: path -> (its SHA-256 or why it can't be read, what its search gives)
struct FakeMap
{
	std::string sha256, unreadable;
	ShapesMapSearch search;
};

struct Fakes
{
	std::map<std::string, FakeMap> maps;
	std::vector<std::string> read, searched;

	ShapesTriggerHooks Hooks()
	{
		ShapesTriggerHooks hooks;

		hooks.read = [this](std::string const &path, std::string *sha256)
		{
			read.push_back(path);
			FakeMap const &map = maps.at(path);
			*sha256 = map.sha256;
			return map.unreadable;
		};
		hooks.search = [this](std::string const &path)
		{
			searched.push_back(path);
			return maps.at(path).search;
		};
		return hooks;
	}
};

static ShapesScannedList ListOf(std::vector<ShapesScannedEntry> const &entries)
{
	ShapesScannedList list;

	list.present = true;
	list.entries = entries;
	return list;
}

static void TheNewMaps()
{
	Fakes fakes;
	std::string const rifs = "C:\\Games\\AvP\\avp_rifs\\";

	fakes.maps[rifs + "Genshd1.RIF"] = { Sha(1), "", { 0, false, "" } };
	fakes.maps[rifs + "Derelict.RIF"] = { Sha(2), "", { 0, false, "" } };
	fakes.maps[rifs + "Custom\\Mine.rif"] = { Sha(3), "", { 4, false, "" } };
	fakes.maps[rifs + "Custom\\Copy of Mine.rif"] = { Sha(3), "", { 4, false, "" } };            // the same map twice
	fakes.maps[rifs + "Custom\\Broken.rif"] = { Sha(4), "", { 0, true, "a chunk runs past its parent" } };
	fakes.maps[rifs + "Custom\\Link.rif"] = { "", "a link (a junction or a symbolic link)", { 0, false, "" } };
	fakes.maps["C:\\Games\\AvP\\avp_huds\\Marine.rif"] = { std::string(64, 'E'), "", { 3, false, "" } };   // upper case from the reader

	std::vector<std::string> const paths = { rifs + "Genshd1.RIF", rifs + "Derelict.RIF", rifs + "Custom\\Mine.rif", rifs + "Custom\\Copy of Mine.rif",
		rifs + "Custom\\Broken.rif", rifs + "Custom\\Link.rif", "C:\\Games\\AvP\\avp_huds\\Marine.rif" };

	ShapesTrigger trigger;
	trigger.Begin(ListOf({ { Sha(1), "Genshd1.RIF" }, { Sha(2), "Derelict.RIF" } }), paths);
	ShapesTriggerHooks const hooks = fakes.Hooks();

	bool const first = trigger.Step(hooks);
	Check(first && !trigger.Done() && fakes.read.size() == 1 && fakes.searched.empty(), "one Step takes one map (Genshd1, on the list: read, not searched)");

	size_t steps = 1;
	while (trigger.Step(hooks))
	{
		steps++;
	}

	Check(steps == 7 && trigger.Done() && !trigger.Step(hooks) && fakes.read.size() == 7, "seven maps, seven Steps, then nothing more is read");
	Check(fakes.searched == std::vector<std::string>({ rifs + "Custom\\Mine.rif", rifs + "Custom\\Broken.rif", "C:\\Games\\AvP\\avp_huds\\Marine.rif" }),
		"searched: only the new maps, a map's second copy not again, the unreadable one not at all (" + std::to_string(fakes.searched.size()) + ")");

	std::vector<ShapesScannedEntry> const &list = trigger.List();
	Check(trigger.Changed() && list.size() == 5 && list[0].sha256 == Sha(1) && list[2].sha256 == Sha(3) && list[2].name == "Mine.rif" && list[3].sha256 == Sha(4) &&
		list[3].name == "Broken.rif" && list[4].sha256 == std::string(64, 'e') && list[4].name == "Marine.rif",
		"the list: the old two, then the new three in order, the failed search too, the SHA-256 in small letters, the file's name without its folders");
	Check(trigger.Scanned() == 3 && trigger.Written() == 7, "3 maps scanned, 7 shapes written");
	Check(trigger.Words() == "Shapes: 3 new maps scanned, 7 new shapes in shape_rifs; Broken.rif: a chunk runs past its parent, Link.rif: a link (a junction or a "
		"symbolic link)", "the status line: \"" + trigger.Words() + "\"");
}

static void NothingNew()
{
	Fakes fakes;
	fakes.maps["a.rif"] = { Sha(1), "", { 0, false, "" } };
	ShapesTrigger trigger;
	trigger.Begin(ListOf({ { Sha(1), "a.rif" } }), { "a.rif" });
	ShapesTriggerHooks const hooks = fakes.Hooks();

	while (trigger.Step(hooks)) {}

	Check(!trigger.Changed() && trigger.Words().empty() && fakes.searched.empty(), "nothing new: no list to write, nothing said");

	ShapesTrigger empty;
	empty.Begin(ListOf({}), {});
	Check(empty.Done() && !empty.Step(hooks) && !empty.Changed(), "no maps at all: done at once");
}

static void TheListsMost()
{
	std::vector<ShapesScannedEntry> full;
	for (int k = 0; k < (int)kShapesScannedMost; k++)
	{
		full.push_back({ Sha(10000 + k), "m.rif" });
	}

	Fakes fakes;
	fakes.maps["new.rif"] = { Sha(1), "", { 2, false, "" } };
	ShapesTrigger trigger;
	trigger.Begin(ListOf(full), { "new.rif" });
	ShapesTriggerHooks const hooks = fakes.Hooks();

	while (trigger.Step(hooks)) {}

	Check(fakes.searched.empty() && !trigger.Changed() && trigger.List().size() == kShapesScannedMost &&
		trigger.Words().find("new.rif: the list of maps searched is full (4096)") != std::string::npos,
		"a full list (4,096): a new map named, not searched, the list not grown");
}

static void TheWordsAndNames()
{
	Fakes fakes;
	std::vector<std::string> paths;

	for (int k = 0; k < 5; k++)
	{
		std::string const path = "D:\\x\\bad" + std::to_string(k) + ".rif";
		fakes.maps[path] = { "", "missing", { 0, false, "" } };
		paths.push_back(path);
	}

	fakes.maps["D:\\x\\one.rif"] = { Sha(7), "", { 1, false, "" } };
	paths.push_back("D:\\x\\one.rif");
	fakes.maps["D:\\x\\evil\x1b[2J\n%s.rif"] = { Sha(8), "", { 0, true, "bad\r\nchunk" } };
	paths.push_back("D:\\x\\evil\x1b[2J\n%s.rif");
	fakes.maps["D:\\x\\short.rif"] = { "abc", "", { 0, false, "" } };   // a reader giving no proper SHA-256
	paths.push_back("D:\\x\\short.rif");

	ShapesTrigger trigger;
	trigger.Begin(ListOf({}), paths);
	ShapesTriggerHooks const hooks = fakes.Hooks();

	while (trigger.Step(hooks)) {}

	std::string const words = trigger.Words();
	Check(words == "Shapes: 2 new maps scanned, 1 new shape in shape_rifs; bad0.rif: missing, bad1.rif: missing, bad2.rif: missing and 4 more",
		"one shape, two maps, the first three named and the rest counted: \"" + words + "\"");
	Check(trigger.List().size() == 2 && trigger.List()[1].name == "evil?[2J?%s.rif", "a hostile file name kept to the safe text rule on the list (\"" +
		(trigger.List().size() == 2 ? trigger.List()[1].name : std::string()) + "\")");
	Check(fakes.searched.size() == 2, "a reader giving a SHA-256 that isn't 64 hex letters: not searched, not listed");
}

int main()
{
	TheNewMaps();
	NothingNew();
	TheListsMost();
	TheWordsAndNames();

	printf("checks %d; failed %d\n", checksRun, checksFailed);
	return checksFailed ? 1 : 0;
}
