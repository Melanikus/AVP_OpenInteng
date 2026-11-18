// See placement_special_alt.h. Written 2026-11-17 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "placement_special_alt.h"

#include <map>

#include "level_file.h"
#include "level_objects.h"       // LevelObjects_OpenForEdit
#include "special_alt_edit.h"
#include "special_mode.h"

#include "editor_shared.h"       // statusWords, specialMode, LoadTheObjectListAgain (self-contained since bishop-hdr 2bbf15a0)

namespace
{
	// The lists read so far, by "kind number", with the editor's current one (inteng's +0x34, not in the file); read again when the level's generation moves
	std::map<std::string, model::AltLocations> lists;
	unsigned listsFor = 0;
	std::string words;

	std::string Key(host::SpecialKind kind, size_t number)
	{
		return std::to_string((int)kind) + " " + std::to_string((unsigned long long)number);
	}

	void Say(std::string const &what)
	{
		words = what;
		statusWords = words.c_str();
	}

	void SayList(char const *what, model::AltLocations const &list)
	{
		Say(std::string(what) + ": " + std::to_string((unsigned long long)list.list.size()) + " location" + (list.list.size() == 1 ? "" : "s") +
			(list.list.empty() ? std::string() : ", the shown one is " + std::to_string((unsigned long long)list.current + 1)));
	}

	std::map<std::string, model::AltLocations> before;   // the lists of the generation before, for their current ones

	// Two lists that say the same: the group, the spare and every entry
	bool Same(model::AltLocations const &a, model::AltLocations const &b)
	{
		if (a.group != b.group || a.spare != b.spare || a.list.size() != b.list.size())
		{
			return false;
		}

		for (size_t i = 0; i < a.list.size(); i++)
		{
			model::AltLocation const &x = a.list[i], &y = b.list[i];

			for (int k = 0; k < 3; k++)
			{
				if (x.place[k] != y.place[k])
				{
					return false;
				}
			}

			for (int k = 0; k < 4; k++)
			{
				if (x.turn[k] != y.turn[k])
				{
					return false;
				}
			}

			if (x.spare[0] != y.spare[0] || x.spare[1] != y.spare[1])
			{
				return false;
			}
		}

		return true;
	}

	// The list of that special, read once per level generation; null (with words) when it cannot be read
	model::AltLocations *ListOf(OpenLevelFile *open, host::SpecialKind kind, size_t number, bool quiet = false)
	{
		if (!open)
		{
			return nullptr;
		}

		if (listsFor != open->Generation())
		{
			// the level changed by another edit (a flight's end writes the object's place, a delete moves the numbers after it): every list is read again, the ones read before kept aside
			// so a list that reads back the same keeps its current one (inteng's +0x34 lives in the object and goes on through moves; only a level load starts it at the first)
			before.clear();
			before.swap(lists);
			listsFor = open->Generation();
		}

		std::string const key = Key(kind, number);
		auto found = lists.find(key);

		if (found == lists.end())
		{
			model::AltLocations loaded;
			host::SpecialResult const read = host::SpecialAlt_Load(open, kind, number, &loaded);

			if (read != host::SpecialResult::Done)
			{
				if (!quiet)
				{
					Say(std::string("Alt Locations: ") + host::SpecialWords(read));
				}

				return nullptr;
			}

			auto const old = before.find(key);

			if (old != before.end() && Same(old->second, loaded) && old->second.current < loaded.list.size())
			{
				loaded.current = old->second.current;
			}

			found = lists.insert(std::make_pair(key, loaded)).first;
		}

		return &found->second;
	}

	bool Save(OpenLevelFile *open, host::SpecialKind kind, size_t number, model::AltLocations const &list)
	{
		host::SpecialResult const written = host::SpecialAlt_Save(open, kind, number, list);

		if (written != host::SpecialResult::Done)
		{
			Say(std::string("Alt Locations: ") + host::SpecialWords(written));
			lists.erase(Key(kind, number));
			return false;
		}

		listsFor = open->Generation();   // our own write: the kept list is the file's
		LoadTheObjectListAgain();
		return true;
	}

	bool PutOn(OpenLevelFile *open, host::SpecialKind kind, size_t number, model::AltLocation const &location)
	{
		host::SpecialResult const put = host::SpecialAlt_Put(open, kind, number, location);

		if (put != host::SpecialResult::Done)
		{
			Say(std::string("Alt Locations: ") + host::SpecialWords(put));
			return false;
		}

		return true;
	}

	// The current special object, when there is one and it can have locations; a player start answers with words
	bool Current(host::SpecialKind *kind, size_t *number)
	{
		if (!specialMode.haveCurrent)
		{
			return false;
		}

		*kind = specialMode.kind;
		*number = specialMode.number;
		return true;
	}
}

bool SpecialAlt_Command(int32_t command)
{
	host::SpecialKind kind;
	size_t number = 0;

	if (!Current(&kind, &number))
	{
		return false;
	}

	if (kind == host::SpecialKind::PlayerStart)
	{
		return true;   // inteng's kinds 1, 2, 3 and 7 only (FUN_004df820): nothing
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	model::AltLocations *const list = ListOf(open, kind, number);
	model::AltLocation now;

	if (!list)
	{
		return true;
	}

	host::SpecialResult const here = host::SpecialAlt_Now(open, kind, number, &now);

	if (here != host::SpecialResult::Done)
	{
		Say(std::string("Alt Locations: ") + host::SpecialWords(here));
		return true;
	}

	if (command == 0x64)   // Add new
	{
		if (!model::AltLocations_Add(list, now))
		{
			Say("Alt Locations: there are as many locations as a chunk can say");
			return true;
		}

		if (Save(open, kind, number, *list))
		{
			SayList("Location added", *list);
		}
	}
	else if (command == 0x65)   // Delete Current: FUN_004df6b0 puts the NEXT location (or the first, after the last) on the object before it empties a list that is left with one
	{
		if (list->list.empty())
		{
			Say("Alt Locations: this object has none");
			return true;
		}

		bool const haveNext = list->list.size() >= 2;
		size_t const nextAt = list->current + 1 < list->list.size() ? list->current + 1 : 0;
		model::AltLocation const next = haveNext ? list->list[nextAt] : model::AltLocation();

		model::AltLocations_DeleteCurrent(list);

		if (haveNext && !PutOn(open, kind, number, next))
		{
			return true;
		}

		if (Save(open, kind, number, *list))
		{
			SayList("Location deleted", *list);
		}
	}
	else if (command == 0x67 || command == 0x68)   // Next, Prev
	{
		if (list->list.empty())
		{
			Say("Alt Locations: this object has none");
			return true;
		}

		if (command == 0x67)
		{
			model::AltLocations_Next(list, now);
		}
		else
		{
			model::AltLocations_Prev(list, now);
		}

		if (PutOn(open, kind, number, list->list[list->current]) && Save(open, kind, number, *list))
		{
			SayList(command == 0x67 ? "Next location" : "Previous location", *list);
		}
	}

	return true;
}

bool SpecialAlt_CurrentGroup(int64_t *group)
{
	host::SpecialKind kind;
	size_t number = 0;

	if (!group || !Current(&kind, &number) || kind == host::SpecialKind::PlayerStart)
	{
		return false;
	}

	model::AltLocations const *const list = ListOf(LevelObjects_OpenForEdit(), kind, number);

	*group = list ? list->group : 0;
	return true;
}

bool SpecialAlt_AssignGroup(int64_t typed)
{
	host::SpecialKind kind;
	size_t number = 0;

	if (!Current(&kind, &number))
	{
		return false;
	}

	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	model::AltLocations *const list = kind == host::SpecialKind::PlayerStart ? nullptr : ListOf(open, kind, number);

	if (!list)
	{
		return true;
	}

	if (!model::AltLocations_SetGroup(list, true, typed))
	{
		Say("Alt Locations: the group number has to be 0 or more (0 means no group)");
		return true;
	}

	if (list->list.empty())
	{
		// an empty list has no chunk (FUN_004dfc70): the number has nowhere to be kept in the file
		Say("Alt Locations: group " + std::to_string((long long)typed) + " set; this object has no locations to write it with");
		return true;
	}

	if (Save(open, kind, number, *list))
	{
		Say("Alt Locations: the group number is " + std::to_string((long long)typed));
	}

	return true;
}

size_t SpecialAlt_ChangeShown(OpenLevelFile *open, int64_t typed)
{
	std::vector<host::SpecialObject> all;

	if (!open || typed < 0 || host::SpecialObjects_Read(open, &all) != host::SpecialResult::Done)
	{
		return 0;
	}

	size_t changed = 0;

	for (host::SpecialObject const &one : all)
	{
		if (one.kind == host::SpecialKind::PlayerStart)
		{
			continue;
		}

		model::AltLocations *const list = ListOf(open, one.kind, one.number, true);
		model::AltLocation now;

		if (!list || list->list.empty() || host::SpecialAlt_Now(open, one.kind, one.number, &now) != host::SpecialResult::Done)
		{
			continue;
		}

		model::AltLocations_JumpTo(list, now, typed);

		if (PutOn(open, one.kind, one.number, list->list[list->current]) && Save(open, one.kind, one.number, *list))
		{
			changed++;
		}
	}

	return changed;
}

std::vector<std::string> SpecialAlt_ShownLines(OpenLevelFile *open, int kind, size_t number)
{
	std::vector<std::string> lines;

	if ((host::SpecialKind)kind == host::SpecialKind::PlayerStart)
	{
		return lines;
	}

	model::AltLocations const *const list = ListOf(open, (host::SpecialKind)kind, number, true);

	if (list && list->list.size() > 1)
	{
		lines.push_back("Location " + std::to_string((int)list->current) + "/" + std::to_string((int)list->list.size()));

		if (list->group != 0)
		{
			lines.push_back("Location group " + std::to_string((int)list->group));
		}
	}

	return lines;
}

void SpecialAlt_Forget()
{
	lists.clear();
	before.clear();
	listsFor = 0;
}
