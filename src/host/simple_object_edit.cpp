// See simple_object_edit.h. Written 2026-11-16 by Vasquez (Sonnet 5.5) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "simple_object_edit.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "chunk_object.hpp"   // Object_Chunk, Object_Header_Chunk
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "level_file.h"       // OpenLevelFile: PayloadOf and SetPayload
#include "object_edit.h"      // AssignStrategy
#include "shape_chooser.h"    // the object's shape and the shapes' outside file names
#include "strategy_lookup.h"  // the object's first strategy

namespace host
{
	namespace
	{
		// the strategy kinds, and where the simple object's numbers lie in the AVPSTRAT chunk's bytes after its heading (strategy_fields.cpp: NewSimpleObject)
		int32_t const kKindSimple = 62;
		int32_t const kKindR6Simple = 1003;
		int32_t const kKindLegacySimple = 53;
		size_t const kKindAt = 8;
		size_t const kTypeAt = 20, kExtraAt = 24, kMassAt = 28, kIntegrityAt = 32, kFlagsAt = 36;
		size_t const kSimpleBytes = 40;   // the fields above end at 40

		// the header (OBJHEAD1) flag bits the window and the save touch (chunk_object.hpp)
		uint32_t const kMarine = 0x800, kAlien = 0x1000, kPredator = 0x2000;
		uint32_t const kPlatformSet = 0x4000, kPc = 0x8000, kPsx = 0x10000, kSaturn = 0x20000;
		uint32_t const kNotEasy = 0x100000, kNotMedium = 0x200000, kNotHard = 0x400000;
		uint32_t const kPlatformBits = kPc | kPsx | kSaturn;
		uint32_t const kDifficultyBits = kNotEasy | kNotMedium | kNotHard;
		uint32_t const kWriteBits = kMarine | kAlien | kPredator | kPlatformBits | kDifficultyBits;

		// the window's lists, in the DLL's order (edobtype.dll tables 0x1001f918 and the ones after)
		SimpleChoice const typeChoices[] =
		{
			{ "Static", 0 }, { "Furniture", 1 }, { "Weapon", 2 }, { "Ammo", 3 }, { "Medikit", 4 }, { "Armour", 5 }, { "Placed light", 0x10 }, { "Key", 6 },
			{ "Boxed Sentry Gun", 7 }, { "IR Goggles", 8 }, { "Video Screen", 9 }, { "Data Tape", 10 }, { "Motion Tracker Upgrade", 11 }, { "Pheromone Pod", 12 },
			{ "Special Game Pickup Object", 13 }, { "Hit me and I'll destruct base", 14 }, { "Predator field charge", 15 },
		};

		SimpleChoice const weaponChoices[] =
		{
			{ "Pulse Rifle", 0 }, { "Smart Gun", 2 }, { "Flame Thrower", 3 }, { "Sadar", 5 }, { "Grenade Launcher", 6 }, { "Minigun", 7 }, { "Pistol", 0x16 }, { "Skeeter", 0x17 },
		};

		SimpleChoice const ammoChoices[] =
		{
			{ "10MM_CULW", 0 }, { "Shotgun", 1 }, { "Smart Gun", 2 }, { "Flame Thrower", 3 }, { "Plasma", 4 }, { "Sadar_tow", 5 }, { "Grenade", 6 }, { "Minigun", 7 }, { "Pulse Grenade", 8 },
			{ "Speargun", 9 },
		};

		SimpleChoice const medikitChoices[] = { { "Stimpack", 0 }, { "Medkit", 1 }, { "Booster", 2 } };
		SimpleChoice const armourChoices[] = { { "Small", 0 }, { "Big", 1 } };

		SimpleChoice const keyChoices[] =
		{
			{ "Data log", 0 }, { "Pass 1", 1 }, { "Pass 2", 2 }, { "Pass 3", 3 }, { "Pass 4", 4 }, { "Pass 5", 5 }, { "Pass 6", 6 }, { "Pass 7", 7 }, { "Pass 8", 8 }, { "Pass 9", 9 },
			{ "Pass 10", 10 }, { "Pass 11", 11 }, { "Pass 12", 12 }, { "Pass 13", 13 }, { "Pass 14", 14 }, { "Pass 15", 15 }, { "Pass 16", 16 }, { "Pass 17", 17 }, { "Pass 18", 18 },
			{ "Pass 19", 19 }, { "Pass 20", 20 }, { "Pass 30 (Special)", 30 }, { "Pass 31 (Never used)", 31 },   // the last row was missing (edobtype 0x1001fb68, read 11-17)
		};

		SimpleChoice const explosionChoices[] = { { "None", 0 }, { "Small", 1 }, { "Big", 2 }, { "Molotov", 3 } };

		// inteng's shape-name table (0x5e7b58), as read from his inteng.exe at run time (SimpleObject_SetShapeNames; Bishop's ruling V-31 11-16): empty while it was not read, and then no type is guessed
		std::vector<SimpleShapeName> &ShapeNames()
		{
			static std::vector<SimpleShapeName> names;

			return names;
		}


		// which type shows which list, and which shows the Mass and Integrity boxes (FUN_10004520's switch)
		bool ShowsMass(int32_t type)
		{
			return type == 0 || type == 1 || type == 9 || type == 13 || type == 16;
		}

		int32_t Read32(unsigned char const *at)
		{
			int32_t value;

			memcpy(&value, at, sizeof(value));
			return value;
		}

		void Write32(unsigned char *at, int32_t value)
		{
			memcpy(at, &value, sizeof(value));
		}

		Object_Chunk *ObjectWithId(File_Chunk *level, uint8_t const *id)
		{
			if (!level || !id)
			{
				return nullptr;
			}

			List<Object_Chunk *> found;

			level->list_objects(&found);

			for (LIF<Object_Chunk *> each(&found); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (object && memcmp(&object->object_data.ID, id, 8) == 0)
				{
					return object;
				}
			}

			return nullptr;
		}

		std::string Lower(std::string text)
		{
			for (char &c : text)
			{
				c = (char)tolower((unsigned char)c);
			}

			return text;
		}

		// the name of the object's shape's outside file as FUN_004e44c0 makes it: lower case, the extension and folder taken off. Empty when the object's shape has no outside file.
		std::string ShapeFileName(OpenLevelFile *store, File_Chunk *level, uint8_t const *id)
		{
			int32_t number = 0;

			if (!ShapeChooser_ShapeOfObject(level, store, id, &number))
			{
				return std::string();
			}

			std::vector<ShapeChoice> shapes;

			ShapeChooser_List(level, store, &shapes);

			for (ShapeChoice const &one : shapes)
			{
				if (one.number == number)
				{
					return Lower(one.name);
				}
			}

			return std::string();
		}

		void Guess(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, SimpleObjectDraft *draft)
		{
			std::string const name = ShapeFileName(store, level, id);

			if (name.empty())
			{
				return;
			}

			int32_t type = 0, extra = 0;

			if (SimpleShapeGuess(name, &type, &extra))
			{
				draft->type = type;
				draft->extra = extra;
			}

			draft->flags |= kPlatformBits;
		}

		// the chunk's bytes now: the store's when it has them (an edit may have changed them), else none
		bool BytesOf(OpenLevelFile *store, Chunk *chunk, std::vector<unsigned char> *bytes)
		{
			return store && chunk && store->PayloadOf(chunk, bytes);
		}

		// FUN_10005ae0's sscanf "%d": the leading number of the text, else the fallback (the function's own parameter, the type)
		int32_t NumberOr(std::string const &text, int32_t fallback)
		{
			int value = 0;

			if (sscanf(text.c_str(), "%d", &value) == 1)
			{
				return (int32_t)value;
			}

			return fallback;
		}

		SimpleChoice const *FindChoice(SimpleChoice const *list, size_t count, std::string const &typed, bool *numberOnly)
		{
			std::string const lower = Lower(typed);

			for (size_t i = 0; i < count; i++)
			{
				if (Lower(list[i].name) == lower)
				{
					return &list[i];
				}
			}

			// the number, as the list's value
			char *end = nullptr;
			long const n = strtol(typed.c_str(), &end, 10);

			if (end && end != typed.c_str() && *end == 0)
			{
				for (size_t i = 0; i < count; i++)
				{
					if (list[i].value == n)
					{
						if (numberOnly)
						{
							*numberOnly = true;
						}

						return &list[i];
					}
				}
			}

			return nullptr;
		}

		std::string NameOfValue(SimpleChoice const *list, size_t count, int32_t value)
		{
			for (size_t i = 0; i < count; i++)
			{
				if (list[i].value == value)
				{
					return list[i].name;
				}
			}

			return std::string();
		}
	}

	size_t SimpleTypeChoices(SimpleChoice const **list)
	{
		if (list)
		{
			*list = typeChoices;
		}

		return sizeof(typeChoices) / sizeof(typeChoices[0]);
	}

	size_t SimpleExtraChoices(int32_t type, SimpleChoice const **list)
	{
		SimpleChoice const *found = nullptr;
		size_t count = 0;

		switch (type)
		{
		case 2: found = weaponChoices; count = sizeof(weaponChoices) / sizeof(weaponChoices[0]); break;
		case 3: found = ammoChoices; count = sizeof(ammoChoices) / sizeof(ammoChoices[0]); break;
		case 4: found = medikitChoices; count = sizeof(medikitChoices) / sizeof(medikitChoices[0]); break;
		case 5: found = armourChoices; count = sizeof(armourChoices) / sizeof(armourChoices[0]); break;
		case 6: found = keyChoices; count = sizeof(keyChoices) / sizeof(keyChoices[0]); break;
		default: break;
		}

		if (list)
		{
			*list = found;
		}

		return count;
	}

	void SimpleObject_SetShapeNames(std::vector<SimpleShapeName> const &names)
	{
		ShapeNames() = names;
	}

	bool SimpleObject_ShapeNamesKnown()
	{
		return !ShapeNames().empty();
	}

	bool SimpleShapeGuess(std::string const &lowerName, int32_t *type, int32_t *extra)
	{
		// FUN_004f6a20: the first row whose name is the same (strcmp: the caller gives the lower-cased name)
		for (SimpleShapeName const &row : ShapeNames())
		{
			if (lowerName == row.name)
			{
				if (type)
				{
					*type = row.type;
				}

				if (extra)
				{
					*extra = row.extra;
				}

				return true;
			}
		}

		return false;
	}

	char const *SimpleObjectWords(SimpleObjectResult result)
	{
		switch (result)
		{
		case SimpleObjectResult::Done: return "done";
		case SimpleObjectResult::BadCall: return "no level or no object";
		case SimpleObjectResult::NoObject: return "the level has no object with that name";
		case SimpleObjectResult::NoHeader: return "the object has no header the editor can change";
		case SimpleObjectResult::NotPlaced: return "a module has no simple object data, so none is made on it";
		case SimpleObjectResult::CantBuild: return "the object's strategy could not be put together";
		case SimpleObjectResult::LoaderRefused: return "the game would not take the level that way, so nothing was changed";
		}

		return "the object could not be changed";
	}

	SimpleObjectResult SimpleObject_Read(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, SimpleObjectDraft *out)
	{
		if (!out)
		{
			return SimpleObjectResult::BadCall;
		}

		*out = SimpleObjectDraft();

		if (!store || !level || !id)
		{
			return SimpleObjectResult::BadCall;
		}

		Object_Chunk *const object = ObjectWithId(level, id);

		if (!object)
		{
			return SimpleObjectResult::NoObject;
		}

		Object_Header_Chunk *const header = object->get_header();
		std::vector<unsigned char> head;

		if (!header || !BytesOf(store, header, &head) || head.size() < 4)
		{
			return SimpleObjectResult::NoHeader;
		}

		// the flags (FUN_004e0ed0 0x4e073f-0x4e087a)
		uint32_t const h = (uint32_t)Read32(head.data());
		uint32_t flags = (h & (kMarine | kAlien | kPredator));

		flags |= (h & kPlatformSet) == 0 ? kPlatformBits : (h & kPlatformBits);
		flags |= h & kDifficultyBits;
		out->flags = flags;
		out->mass = 50;
		out->integrity = 2;

		ObjectStrategy strategy;
		bool guess = false;

		if (FindObjectStrategy(level, id, &strategy) == StrategyFound::Found)
		{
			std::vector<unsigned char> bytes;

			if (!BytesOf(store, strategy.chunk, &bytes))
			{
				bytes.assign(strategy.payload.begin(), strategy.payload.end());
			}

			int32_t const kind = bytes.size() >= kKindAt + 4 ? Read32(bytes.data() + kKindAt) : 0;

			out->strategyKind = kind;

			if ((kind == kKindSimple || kind == kKindR6Simple) && bytes.size() >= kSimpleBytes)
			{
				out->type = Read32(bytes.data() + kTypeAt);
				out->extra = Read32(bytes.data() + kExtraAt);
				out->mass = Read32(bytes.data() + kMassAt) & 0xffff;
				out->integrity = Read32(bytes.data() + kIntegrityAt) & 0xffff;
				out->explosion = (Read32(bytes.data() + kFlagsAt) >> 4) & 3;
				guess = out->type == 0 && out->extra == 0;
			}
			else if (kind != kKindLegacySimple)
			{
				// any other strategy: the defaults (inteng does not read it)
			}
		}
		else
		{
			out->flags = kMarine | kAlien | kPredator | kPc;   // 0xb800 (FUN_004e0ed0 0x4e0c9a)
			guess = true;
		}

		if (guess)
		{
			Guess(store, level, id, out);
		}

		return SimpleObjectResult::Done;
	}

	SimpleObjectResult SimpleObject_Write(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, SimpleObjectDraft const &draft)
	{
		if (!store || !level || !id)
		{
			return SimpleObjectResult::BadCall;
		}

		Object_Chunk *const object = ObjectWithId(level, id);

		if (!object)
		{
			return SimpleObjectResult::NoObject;
		}

		Object_Header_Chunk *const header = object->get_header();
		std::vector<unsigned char> head;

		if (!header || !BytesOf(store, header, &head) || head.size() < 4)
		{
			return SimpleObjectResult::NoHeader;
		}

		// the strategy first (it may rebuild the level), so a refusal leaves everything as it was
		ObjectStrategy strategy;
		bool const have = FindObjectStrategy(level, id, &strategy) == StrategyFound::Found;
		int32_t kind = 0;

		if (have)
		{
			std::vector<unsigned char> bytes;

			if (!BytesOf(store, strategy.chunk, &bytes))
			{
				bytes.assign(strategy.payload.begin(), strategy.payload.end());
			}

			kind = bytes.size() >= kKindAt + 4 ? Read32(bytes.data() + kKindAt) : 0;
		}

		// WHICH OBJECTS' STRATEGY THE EDIT WRITES. inteng's load (FUN_004e0ed0 0x4e0aaf-0x4e0bc0) sets the object's +0x5c, and its save (FUN_004e1f90 0x4e2521 / 0x4e2808) writes the
		// strategy only when it is set: no strategy, a legacy 53, a simple object (62, 1003) and the other game's 2000 / 2001 / 2002 are given (or keep) a simple object; ANY OTHER strategy (a
		// door, a switch, a lift ...) is left as it is, and only the header's flags are written. OUR BUG, FIXED 2026-11-17 (Drake): it was replaced by a simple object, so Edit and OK on a door
		// made it stop being a door.
		bool const writesStrategy = !have || kind == kKindSimple || kind == kKindR6Simple || kind == kKindLegacySimple || kind == 2000 || kind == 2001 || kind == 2002;

		if (writesStrategy)
		{
			if (!have || (kind != kKindSimple && kind != kKindR6Simple))
			{
				ObjectEditResult const made = AssignStrategy(store, id, kKindSimple);

				if (made != ObjectEditResult::Done)
				{
					return made == ObjectEditResult::LoaderRefused ? SimpleObjectResult::LoaderRefused : made == ObjectEditResult::NotPlaced ? SimpleObjectResult::NotPlaced : SimpleObjectResult::CantBuild;
				}

				level = store->Level();
			}

			// the simple object's numbers, in place (a 1003 too: inteng's save changes both in place, 0x4e2914)
			if (FindObjectStrategy(level, id, &strategy) != StrategyFound::Found)
			{
				return SimpleObjectResult::CantBuild;
			}

			std::vector<unsigned char> bytes;

			if (!BytesOf(store, strategy.chunk, &bytes))
			{
				bytes.assign(strategy.payload.begin(), strategy.payload.end());
			}

			int32_t const nowKind = bytes.size() >= kKindAt + 4 ? Read32(bytes.data() + kKindAt) : 0;

			if (bytes.size() < kSimpleBytes || (nowKind != kKindSimple && nowKind != kKindR6Simple))
			{
				return SimpleObjectResult::CantBuild;
			}

			Write32(bytes.data() + kTypeAt, draft.type);
			Write32(bytes.data() + kExtraAt, draft.extra);
			Write32(bytes.data() + kMassAt, draft.mass & 0xffff);
			Write32(bytes.data() + kIntegrityAt, (int32_t)(((uint32_t)draft.integrity & 0xffff) | 0x80000000u));

			int32_t flagsWord = Read32(bytes.data() + kFlagsAt) & ~0x30;

			if (draft.explosion > 0 && draft.explosion < 4)
			{
				flagsWord |= draft.explosion << 4;
			}

			Write32(bytes.data() + kFlagsAt, flagsWord);

			if (!store->SetPayload(strategy.chunk, bytes))
			{
				return SimpleObjectResult::CantBuild;
			}
		}

		// the header's flags: the window's six bits and the three platform bits, and "platform load set" on
		Object_Chunk *const again = ObjectWithId(level, id);
		Object_Header_Chunk *const header2 = again ? again->get_header() : nullptr;

		if (!header2 || !BytesOf(store, header2, &head) || head.size() < 4)
		{
			return SimpleObjectResult::NoHeader;
		}

		uint32_t h = (uint32_t)Read32(head.data());

		h = (h & ~kWriteBits) | (draft.flags & kWriteBits) | kPlatformSet;
		Write32(head.data(), (int32_t)h);

		if (!store->SetPayload(header2, head))
		{
			return SimpleObjectResult::NoHeader;
		}

		return SimpleObjectResult::Done;
	}

	bool SimpleEdit_Begin(OpenLevelFile *store, File_Chunk *level, uint8_t const *id, SimpleObjectEdit *edit)
	{
		if (!edit)
		{
			return false;
		}

		*edit = SimpleObjectEdit();

		if (SimpleObject_Read(store, level, id, &edit->draft) != SimpleObjectResult::Done)
		{
			return false;
		}

		memcpy(edit->id, id, 8);
		edit->massText = std::to_string(edit->draft.mass);
		edit->integrityText = std::to_string(edit->draft.integrity);
		edit->open = true;
		return true;
	}

	SimpleLayout SimpleEdit_LayoutFor(int32_t type)
	{
		SimpleLayout layout;
		SimpleChoice const *list = nullptr;
		size_t const n = SimpleExtraChoices(type, &list);

		layout.extra = n > 0;

		for (size_t i = 0; i < n; i++)
		{
			layout.extraChoices.push_back(list[i].name);
		}

		layout.mass = ShowsMass(type);
		return layout;
	}

	bool SimpleEdit_TypeOfWord(std::string const &word, int32_t *type)
	{
		SimpleChoice const *list = nullptr;
		size_t const types = SimpleTypeChoices(&list);
		SimpleChoice const *const found = word.empty() ? nullptr : FindChoice(list, types, word, nullptr);

		if (found && type)
		{
			*type = found->value;
		}

		return found != nullptr;
	}

	int SimpleEdit_ExtraRow(int32_t type, int32_t extra)
	{
		SimpleChoice const *list = nullptr;
		size_t const n = SimpleExtraChoices(type, &list);

		for (size_t i = 0; i < n; i++)
		{
			if (list[i].value == extra)
			{
				return (int)i;
			}
		}

		return -1;
	}

	void SimpleEdit_Items(SimpleObjectEdit const &edit, std::string *title, std::vector<SimpleFormItem> *items)
	{
		if (title)
		{
			*title = "Edit object properties";
		}

		if (!items)
		{
			return;
		}

		items->clear();

		SimpleFormItem item;
		SimpleChoice const *list = nullptr;
		size_t const types = SimpleTypeChoices(&list);

		item.label = "Type of Object";
		item.value = NameOfValue(list, types, edit.draft.type);
		item.kind = 2;

		for (size_t i = 0; i < types; i++)
		{
			item.choices.push_back(list[i].name);
		}

		items->push_back(item);

		SimpleLayout const layout = SimpleEdit_LayoutFor(edit.draft.type);

		if (layout.extra)
		{
			size_t const extras = SimpleExtraChoices(edit.draft.type, &list);

			item = SimpleFormItem();
			item.label = "Enter type";   // the DLL sets this caption on the list's label (FUN_10004520 0x100045f1)
			item.value = NameOfValue(list, extras, edit.draft.extra);
			item.kind = 2;
			item.choices = layout.extraChoices;
			items->push_back(item);
		}

		if (layout.mass)
		{
			item = SimpleFormItem();
			item.label = "Mass (x100g)";
			item.value = edit.massText;
			item.kind = 0;
			items->push_back(item);
			item.label = "Integrity";
			item.value = edit.integrityText;
			items->push_back(item);
		}

		struct Check { char const *label; uint32_t bit; bool notBit; };
		Check const checks[] =
		{
			{ "Marine", kMarine, false }, { "Alien", kAlien, false }, { "Predator", kPredator, false },
			{ "Easy", kNotEasy, true }, { "Medium", kNotMedium, true }, { "Hard", kNotHard, true },
		};

		for (Check const &one : checks)
		{
			bool const on = one.notBit ? (edit.draft.flags & one.bit) == 0 : (edit.draft.flags & one.bit) != 0;

			item = SimpleFormItem();
			item.label = one.label;
			item.value = on ? "y" : "n";
			item.kind = 1;
			items->push_back(item);
		}

		item = SimpleFormItem();
		item.label = "Explosion";
		item.value = NameOfValue(explosionChoices, 4, edit.draft.explosion);
		item.kind = 3;

		for (SimpleChoice const &one : explosionChoices)
		{
			item.choices.push_back(one.name);
		}

		items->push_back(item);
	}

	SimpleAnswer SimpleEdit_Answer(OpenLevelFile *store, File_Chunk *level, SimpleObjectEdit *edit, std::vector<std::string> const &values, std::string *words)
	{
		if (words)
		{
			words->clear();
		}

		if (!edit || !edit->open)
		{
			return SimpleAnswer::Failed;
		}

		SimpleObjectDraft next = edit->draft;
		size_t at = 0;

		auto take = [&](std::string *text)
		{
			*text = at < values.size() ? values[at] : std::string();
			at++;

			if (text->size() > 63)
			{
				text->resize(63);
			}
		};

		std::string typed;

		take(&typed);

		int32_t pickedType = 0;

		if (SimpleEdit_TypeOfWord(typed, &pickedType))
		{
			next.type = pickedType;
		}

		// what the window shows is what the picked type shows: the list headed "Enter type" and the number boxes
		SimpleLayout const layout = SimpleEdit_LayoutFor(next.type);
		SimpleChoice const *list = nullptr;

		if (layout.extra)
		{
			size_t const extras = SimpleExtraChoices(next.type, &list);

			take(&typed);

			SimpleChoice const *const picked = typed.empty() ? nullptr : FindChoice(list, extras, typed, nullptr);

			if (picked)
			{
				next.extra = picked->value;
			}
		}

		std::string mass = edit->massText, integrity = edit->integrityText;

		if (layout.mass)
		{
			take(&mass);
			take(&integrity);
		}

		uint32_t const bits[6] = { kMarine, kAlien, kPredator, kNotEasy, kNotMedium, kNotHard };

		for (int i = 0; i < 6; i++)
		{
			std::string one;

			take(&one);

			bool const on = !one.empty() && (one[0] == 'y' || one[0] == 'Y');
			bool const notBit = i >= 3;

			if (notBit ? !on : on)
			{
				next.flags |= bits[i];
			}
			else
			{
				next.flags &= ~bits[i];
			}
		}

		take(&typed);

		SimpleChoice const *const picked = typed.empty() ? nullptr : FindChoice(explosionChoices, 4, typed, nullptr);

		if (picked)
		{
			next.explosion = picked->value;
		}

		edit->massText = mass;
		edit->integrityText = integrity;
		edit->draft = next;

		// OK: the numbers (FUN_10005ae0). A text that is no number leaves the type's number (the function's own parameter).
		if (layout.mass)
		{
			int32_t const m = NumberOr(mass, next.type);

			if ((uint32_t)m != ((uint32_t)m & 0xffff))
			{
				if (words)
				{
					*words = "Mass value too big (>65535)\n";
				}

				return SimpleAnswer::Message;
			}

			int32_t const g = NumberOr(integrity, next.type);

			if ((uint32_t)g != ((uint32_t)g & 0xffff))
			{
				if (words)
				{
					*words = "Integrity value too big (>65535)\n";
				}

				return SimpleAnswer::Message;
			}

			next.mass = m;
			next.integrity = g;
			edit->draft = next;
		}

		SimpleObjectResult const written = SimpleObject_Write(store, level, edit->id, next);

		if (written != SimpleObjectResult::Done)
		{
			if (words)
			{
				*words = SimpleObjectWords(written);
			}

			return SimpleAnswer::Failed;
		}

		edit->open = false;
		return SimpleAnswer::Done;
	}
}
