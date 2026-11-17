// make_strategy_level: MAKES a level for an acceptance whose strategy no stock level holds (reassess/strategy-closeout-drake.md E-5; Bishop 11-16: "a copy of a stock level with the strategy
// bytes written by a TEST TOOL, never by hand"). Written 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
//
// USE: make_strategy_level.exe <a stock level> <the level to make, ending ".made.rif"> <an object's name> <step> ... [then <another object's name> <step> ...] ...
// The stock level is only read. Each object's strategy is changed by its steps, in order, starting from the strategy it holds:
//   kind <n>                    a new strategy of kind n in place of its own: model::MakeNewStrategy (the old editor's creation values) for a kind the old editor offered; for one it never
//                               offered (63 Console, 67 Teleport), the kind's payload with every list empty and every byte 0 but the kind number
//   <field> <number>            the field, by the model's name, set to the number (model::SetField: the value is checked as the editor checks it)
//   <field> id <name>           an object field set to the ID of the object (or virtual object, or enemy / generator) with that name
//   <field> noid <hex> <hex>    an object field set to an ID no object of the level has (the two halves as the file stores them): the record a Find Errors repair is for
//   size <bytes>                the payload made that long (zeros added at its end, or its end cut off)
//   insert <at> <bytes>         that many zero bytes put in at <at> (both multiples of 4): room for a list's item before the words after it (a Track's point effect)
//   word <at> <number>          the 4 bytes at <at> (a multiple of 4, inside the payload) set to the number
//   idat <at> <name>            the 8 bytes at <at> set to that object's ID
//   noidat <at> <hex> <hex>     the 8 bytes at <at> set to an ID no object of the level has
// A step that can't be done stops the tool before anything is written, and says which and why. Each strategy must then be one the game's loader takes (model::CheckStrategySize); it is
// written as the editor writes one (host::SetStrategyBytes, or host::VirtualObject_SetStrategyBytes for a virtual object), the level is saved to the new file, and the new file is read back by
// the game's own loader: each object must hold exactly the bytes made for it. A file already at the new name is replaced only when its name ends ".made.rif" (only this tool makes those).
// Gives back 0 only when the level was made and read back.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <map>
#include <string>
#include <vector>

#include <windows.h>

#include "chunk.hpp"
#include "chunk_object.hpp"
#include "mishchnk.hpp"
#include "field_model.h"
#include "level_file.h"
#include "object_edit.h"
#include "strategy_lookup.h"
#include "virtual_object.h"

// what the game's chunk code calls
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
	printf("FAILED make_strategy_level: the game's code stopped the program (code %d)\n", code);
	exit(code ? code : 1);
}

void LogErrorStringC(const char *)
{
}

void Sound_ForgetAddressesInside(const void *, size_t)
{
}

static size_t const kMostBytes = 65536;   // SetStrategyBytes and VirtualObject_SetStrategyBytes refuse more

struct Named
{
	uint8_t id[8];
	bool isVirtual;
	bool isGenerator;
};

// every object with an ID, by its exact name: objects (modules and placed objects), virtual objects, enemies and generators; a name two of them have is refused when it is used
static void NamesOf(File_Chunk *level, std::map<std::string, std::vector<Named>> *out)
{
	out->clear();

	List<Object_Chunk *> objects;
	level->list_objects(&objects);

	for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
	{
		if (each() && each()->object_data.o_name)
		{
			Named one = {};

			memcpy(one.id, &each()->object_data.ID, 8);
			(*out)[each()->object_data.o_name].push_back(one);
		}
	}

	std::vector<host::VirtualObjectRef> virtuals;
	host::ListVirtualObjects(level, &virtuals);

	for (host::VirtualObjectRef const &v : virtuals)
	{
		Named one = {};

		memcpy(one.id, v.id, 8);
		one.isVirtual = true;
		(*out)[v.name].push_back(one);
	}

	std::vector<host::GeneratorRef> generators;
	host::ListGenerators(level, &generators);

	for (host::GeneratorRef const &g : generators)
	{
		Named one = {};

		memcpy(one.id, g.id, 8);
		one.isGenerator = true;
		(*out)[g.name].push_back(one);
	}
}

static bool Fail(std::string const &why)
{
	printf("FAILED make_strategy_level: %s\n", why.c_str());
	return false;
}

static bool Number(char const *text, int64_t lowest, int64_t highest, int64_t *value)
{
	char *end = nullptr;
	long long const read = strtoll(text, &end, 0);

	if (!text[0] || !end || *end || read < lowest || read > highest)
	{
		return false;
	}

	*value = read;
	return true;
}

static bool Half(char const *text, uint32_t *value)
{
	int64_t read = 0;

	if (!Number(text, 0, 0xffffffffll, &read))
	{
		return false;
	}

	*value = (uint32_t)read;
	return true;
}

struct Job
{
	std::string name;
	Named who = {};
	std::vector<uint8_t> payload;
};

struct Tool
{
	File_Chunk *level = nullptr;
	std::map<std::string, std::vector<Named>> names;

	bool Find(std::string const &name, Named *out) const
	{
		auto const found = names.find(name);

		if (found == names.end())
		{
			return Fail("no object of the level is named \"" + name + "\"");
		}

		if (found->second.size() != 1)
		{
			return Fail(std::to_string((long long)found->second.size()) + " objects of the level are named \"" + name + "\": the name doesn't say which");
		}

		*out = found->second[0];
		return true;
	}

	bool NoObjectHas(uint32_t first, uint32_t second) const
	{
		for (auto const &each : names)
		{
			for (Named const &one : each.second)
			{
				uint32_t a = 0, b = 0;

				memcpy(&a, one.id, 4);
				memcpy(&b, one.id + 4, 4);

				if (a == first && b == second)
				{
					return Fail("an object of the level has the ID given as one none has: \"" + each.first + "\"");
				}
			}
		}

		return true;
	}

	bool Put(std::vector<uint8_t> *payload, char const *atText, uint8_t const *bytes, size_t count) const
	{
		int64_t at = 0;

		if (!Number(atText, 0, (int64_t)kMostBytes, &at) || at % 4 != 0 || (size_t)at + count > payload->size())
		{
			return Fail(std::string("the place ") + atText + " is not a multiple of 4 inside the strategy's " + std::to_string((unsigned long long)payload->size()) + " bytes");
		}

		memcpy(payload->data() + at, bytes, count);
		return true;
	}

	static model::Field const *FieldOf(model::StrategyKind const &kind, char const *name)
	{
		for (size_t i = 0; i < kind.fieldCount; i++)
		{
			if (strcmp(kind.fields[i].name, name) == 0)
			{
				return &kind.fields[i];
			}
		}

		for (size_t i = 0; i < model::StrategyBaseFieldCount; i++)
		{
			if (strcmp(model::StrategyBaseFields[i].name, name) == 0)
			{
				return &model::StrategyBaseFields[i];
			}
		}

		return nullptr;
	}

	// one step of a job, from args[*a]; *a is moved past it
	bool Step(Job *job, std::vector<std::string> const &args, size_t *a) const
	{
		std::string const verb = args[*a];
		auto const need = [&](size_t more) { return *a + more < args.size() ? true : Fail("\"" + verb + "\" needs " + std::to_string((unsigned long long)more) + " more word(s)"); };

		if (verb == "kind")
		{
			int64_t number = 0;

			if (!need(1) || !Number(args[*a + 1].c_str(), 0, 0x7fffffff, &number))
			{
				return *a + 1 < args.size() ? Fail("the kind \"" + args[*a + 1] + "\" is not a number") : false;
			}

			model::StrategyKind const *const kind = model::FindStrategyKind((int32_t)number);

			if (!kind)
			{
				return Fail("the editor doesn't know a strategy of kind " + args[*a + 1]);
			}

			std::vector<uint8_t> made(kMostBytes, 0);
			size_t length = 0;

			if (model::MakeNewStrategy(kind->number, made.data(), made.size(), &length))
			{
				made.resize(length);
			}
			else
			{
				// a kind the old editor never offered: its payload with every list empty, every byte 0 but its number
				made.assign(kind->fixedBytes, 0);

				if (made.size() < 20)
				{
					return Fail("kind " + args[*a + 1] + "'s payload is shorter than a strategy's 20 bytes");
				}

				memcpy(made.data() + 8, &kind->number, 4);
			}

			job->payload = made;
			*a += 2;
			return true;
		}

		if (verb == "size")
		{
			int64_t bytes = 0;

			if (!need(1) || !Number(args[*a + 1].c_str(), 20, (int64_t)kMostBytes, &bytes))
			{
				return *a + 1 < args.size() ? Fail("the size \"" + args[*a + 1] + "\" is not a number from 20 to 65536") : false;
			}

			job->payload.resize((size_t)bytes, 0);
			*a += 2;
			return true;
		}

		if (verb == "insert")
		{
			int64_t at = 0, bytes = 0;

			if (!need(2))
			{
				return false;
			}

			if (!Number(args[*a + 1].c_str(), 0, (int64_t)job->payload.size(), &at) || at % 4 != 0 ||
			    !Number(args[*a + 2].c_str(), 4, (int64_t)kMostBytes, &bytes) || bytes % 4 != 0 || job->payload.size() + (size_t)bytes > kMostBytes)
			{
				return Fail("\"insert " + args[*a + 1] + " " + args[*a + 2] + "\": the place must be a multiple of 4 inside the strategy's " + std::to_string((unsigned long long)job->payload.size()) +
				            " bytes and the zeros a multiple of 4, the whole no more than 65536 bytes");
			}

			job->payload.insert(job->payload.begin() + (ptrdiff_t)at, (size_t)bytes, 0);
			*a += 3;
			return true;
		}

		if (verb == "word")
		{
			int64_t value = 0;

			if (!need(2) || !Number(args[*a + 2].c_str(), -0x80000000ll, 0xffffffffll, &value))
			{
				return *a + 2 < args.size() ? Fail("the word \"" + args[*a + 2] + "\" is not a 32-bit number") : false;
			}

			uint32_t const word = (uint32_t)value;

			if (!Put(&job->payload, args[*a + 1].c_str(), (uint8_t const *)&word, 4))
			{
				return false;
			}

			*a += 3;
			return true;
		}

		if (verb == "idat")
		{
			Named target = {};

			if (!need(2) || !Find(args[*a + 2], &target) || !Put(&job->payload, args[*a + 1].c_str(), target.id, 8))
			{
				return false;
			}

			*a += 3;
			return true;
		}

		if (verb == "noidat")
		{
			uint32_t halves[2] = {};

			if (!need(3) || !Half(args[*a + 2].c_str(), &halves[0]) || !Half(args[*a + 3].c_str(), &halves[1]))
			{
				return *a + 3 < args.size() ? Fail("\"" + args[*a + 2] + " " + args[*a + 3] + "\" are not two 32-bit numbers") : false;
			}

			if (!NoObjectHas(halves[0], halves[1]) || !Put(&job->payload, args[*a + 1].c_str(), (uint8_t const *)halves, 8))
			{
				return false;
			}

			*a += 4;
			return true;
		}

		// a field, by the model's name
		if (job->payload.size() < 20)
		{
			return Fail("\"" + job->name + "\" holds no strategy to set \"" + verb + "\" in (start with: kind <n>)");
		}

		int32_t kindNumber = 0;
		memcpy(&kindNumber, job->payload.data() + 8, 4);

		model::StrategyKind const *const kind = model::FindStrategyKind(kindNumber);
		model::Field const *const field = kind ? FieldOf(*kind, verb.c_str()) : nullptr;

		if (!field)
		{
			return Fail("\"" + verb + "\" is not a field of a strategy of kind " + std::to_string((long long)kindNumber));
		}

		if (!need(1))
		{
			return false;
		}

		if (args[*a + 1] == "id" || args[*a + 1] == "noid")
		{
			uint32_t halves[2] = {};

			if (args[*a + 1] == "id")
			{
				Named target = {};

				if (!need(2) || !Find(args[*a + 2], &target))
				{
					return false;
				}

				memcpy(halves, target.id, 8);
				*a += 3;
			}
			else
			{
				if (!need(3) || !Half(args[*a + 2].c_str(), &halves[0]) || !Half(args[*a + 3].c_str(), &halves[1]))
				{
					return *a + 3 < args.size() ? Fail("\"" + args[*a + 2] + " " + args[*a + 3] + "\" are not two 32-bit numbers") : false;
				}

				if (!NoObjectHas(halves[0], halves[1]))
				{
					return false;
				}

				*a += 4;
			}

			if (!model::SetFieldObject(*kind, *field, job->payload.data(), job->payload.size(), halves[0], halves[1]))
			{
				return Fail("\"" + verb + "\" can't hold an object's ID in this strategy");
			}

			return true;
		}

		char *end = nullptr;
		double const value = strtod(args[*a + 1].c_str(), &end);
		char const *why = nullptr;

		if (!end || *end || !model::SetField(*kind, *field, job->payload.data(), job->payload.size(), value, &why))
		{
			return Fail("\"" + verb + "\" can't be set to " + args[*a + 1] + (why ? std::string(": ") + why : std::string()));
		}

		*a += 2;
		return true;
	}
};

static bool EndsWith(std::string const &text, std::string const &end)
{
	if (text.size() < end.size())
	{
		return false;
	}

	for (size_t i = 0; i < end.size(); i++)
	{
		if (tolower((unsigned char)text[text.size() - end.size() + i]) != end[i])
		{
			return false;
		}
	}

	return true;
}

int main(int argc, char **argv)
{
	if (argc < 5)
	{
		printf("make_strategy_level: <stock level> <new level, ending .made.rif> <object name> <step> ... [then <object name> <step> ...] ...\n");
		return 2;
	}

	std::string const stock = argv[1];
	std::string const made = argv[2];

	if (!EndsWith(made, ".made.rif"))
	{
		Fail("the level to make, \"" + made + "\", must end \".made.rif\" (the only files this tool replaces)");
		return 1;
	}

	if (_stricmp(stock.c_str(), made.c_str()) == 0)
	{
		Fail("the stock level and the level to make are the same file");
		return 1;
	}

	OpenLevelFile level;

	if (level.Open(stock.c_str()) != LevelOpenResult::Opened || !level.Level())
	{
		Fail("the game's loader doesn't read the stock level " + stock);
		return 1;
	}

	Tool tool;
	tool.level = level.Level();
	NamesOf(tool.level, &tool.names);

	// the jobs, one per object, every step checked before anything is written
	std::vector<std::string> args(argv + 3, argv + argc);
	std::vector<Job> jobs;

	for (size_t a = 0; a < args.size();)
	{
		Job job;

		job.name = args[a];

		if (!tool.Find(job.name, &job.who))
		{
			return 1;
		}

		if (job.who.isGenerator)
		{
			Fail("\"" + job.name + "\" is an enemy or a generator: this tool writes the strategies of objects and virtual objects only");
			return 1;
		}

		host::ObjectStrategy found;

		if (host::FindObjectStrategy(tool.level, job.who.id, &found) == host::StrategyFound::Found)
		{
			job.payload = found.payload;
		}

		for (a++; a < args.size() && args[a] != "then";)
		{
			if (!tool.Step(&job, args, &a))
			{
				return 1;
			}
		}

		a += a < args.size() ? 1 : 0;   // past "then"

		size_t expected = 0;
		model::SizeCheck const size = job.payload.size() >= 20 ? model::CheckStrategySize(job.payload.data(), job.payload.size(), &expected) : model::SizeCheck::TooShort;

		if (size != model::SizeCheck::Right)
		{
			Fail("\"" + job.name + "\"'s strategy (" + std::to_string((unsigned long long)job.payload.size()) + " bytes) is one the game's loader refuses: " + model::SizeCheckWords(size));
			return 1;
		}

		jobs.push_back(job);
	}

	for (Job const &job : jobs)
	{
		if (job.who.isVirtual)
		{
			host::VirtualObjectResult const result = host::VirtualObject_SetStrategyBytes(&level, job.who.id, job.payload.data(), job.payload.size());

			if (result != host::VirtualObjectResult::Done)
			{
				Fail("\"" + job.name + "\"'s strategy wasn't written: " + host::VirtualObjectWords(result));
				return 1;
			}
		}
		else
		{
			host::ObjectEditResult const result = host::SetStrategyBytes(&level, job.who.id, job.payload.data(), job.payload.size());

			if (result != host::ObjectEditResult::Done)
			{
				Fail("\"" + job.name + "\"'s strategy wasn't written: " + host::ObjectEditWords(result));
				return 1;
			}
		}
	}

	if (GetFileAttributesA(made.c_str()) != INVALID_FILE_ATTRIBUTES && !DeleteFileA(made.c_str()))
	{
		Fail("the earlier " + made + " couldn't be deleted");
		return 1;
	}

	LevelSaveResult const saved = level.SaveAs(made.c_str());

	if (saved != LevelSaveResult::Saved)
	{
		Fail("the level wasn't saved to " + made + " (save result " + std::to_string((long long)saved) + ")");
		return 1;
	}

	// read back by the game's own loader: each object holds exactly the bytes made for it
	OpenLevelFile check;

	if (check.Open(made.c_str()) != LevelOpenResult::Opened || !check.Level())
	{
		Fail("the game's loader doesn't read the made level " + made);
		return 1;
	}

	for (Job const &job : jobs)
	{
		host::ObjectStrategy found;

		if (host::FindObjectStrategy(check.Level(), job.who.id, &found) != host::StrategyFound::Found || found.payload != job.payload)
		{
			Fail("in the made level \"" + job.name + "\" doesn't hold the strategy made for it");
			return 1;
		}

		int32_t kind = 0;
		memcpy(&kind, job.payload.data() + 8, 4);
		printf("  made: \"%s\" holds a strategy of kind %d, %u bytes\n", job.name.c_str(), kind, (unsigned)job.payload.size());
	}

	printf("made %s from %s\n", made.c_str(), stock.c_str());
	return 0;
}
