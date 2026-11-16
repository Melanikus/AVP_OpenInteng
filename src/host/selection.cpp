// See selection.h. Written 2026-11-14 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include <stdio.h>
#include <string.h>

#include "selection.h"

namespace host
{
	namespace
	{
		Selected selected = { SelectedKind::Nothing, 0, { 0 }, 0 };
		uint8_t members[kSelectionSetMost][kSelectionIdBytes];
		size_t memberCount = 0;
		uint32_t changes = 0;
		char pickWords[kSelectionMostWords] = { 0 };

		bool IdIsNull(const uint8_t *id)
		{
			for (size_t i = 0; i < kSelectionIdBytes; i++)
			{
				if (id[i] != 0)
				{
					return false;
				}
			}

			return true;
		}

		bool SameAsNow(const Selected &picked)
		{
			if (picked.kind != selected.kind)
			{
				return false;
			}

			switch (picked.kind)
			{
				case SelectedKind::Module:
					return picked.moduleIndex == selected.moduleIndex;
				case SelectedKind::Object:
					return memcmp(picked.objectId, selected.objectId, kSelectionIdBytes) == 0;
				case SelectedKind::Light:
					return picked.lightIndex == selected.lightIndex;
				default:
					return true;
			}
		}

		// A distance in millimetres as metres to a tenth, held to 0 to kSelectionMostDistance first
		void Metres(int32_t distance, char *out, size_t size)
		{
			int32_t const held = distance < 0 ? 0 : (distance > kSelectionMostDistance ? kSelectionMostDistance : distance);
			_snprintf(out, size - 1, "%d.%d m", (int)(held / 1000), (int)((held % 1000) / 100));
			out[size - 1] = '\0';
		}
	}

	bool Selection_Set(const Selected &picked)
	{
		Selected kept = { picked.kind, 0, { 0 }, 0 };

		switch (picked.kind)
		{
			case SelectedKind::Nothing:
				break;
			case SelectedKind::Module:
				if (picked.moduleIndex < 0)
				{
					return false;
				}
				kept.moduleIndex = picked.moduleIndex;
				break;
			case SelectedKind::Object:
				if (IdIsNull(picked.objectId))
				{
					return false;
				}
				memcpy(kept.objectId, picked.objectId, kSelectionIdBytes);
				break;
			case SelectedKind::Light:
				if (picked.lightIndex < 0)
				{
					return false;
				}
				kept.lightIndex = picked.lightIndex;
				break;
			default:
				return false;
		}

		// The set follows: one object for an object, none otherwise
		size_t const wasInSet = memberCount;
		bool const setIsAlready = kept.kind == SelectedKind::Object ? (wasInSet == 1 && memcmp(members[0], kept.objectId, kSelectionIdBytes) == 0)
			: wasInSet == 0;

		if (!SameAsNow(kept) || !setIsAlready)
		{
			selected = kept;
			memberCount = 0;

			if (kept.kind == SelectedKind::Object)
			{
				memcpy(members[0], kept.objectId, kSelectionIdBytes);
				memberCount = 1;
			}

			changes++;
		}

		return true;
	}

	bool Selection_Toggle(const Selected &picked)
	{
		if (picked.kind != SelectedKind::Object || IdIsNull(picked.objectId))
		{
			return false;
		}

		for (size_t i = 0; i < memberCount; i++)
		{
			if (memcmp(members[i], picked.objectId, kSelectionIdBytes) == 0)
			{
				// taken out: the later ones move up, and the last one left is what the panel shows
				for (size_t j = i + 1; j < memberCount; j++)
				{
					memcpy(members[j - 1], members[j], kSelectionIdBytes);
				}

				memberCount--;
				selected.kind = memberCount ? SelectedKind::Object : SelectedKind::Nothing;
				selected.moduleIndex = 0;
				memset(selected.objectId, 0, kSelectionIdBytes);

				if (memberCount)
				{
					memcpy(selected.objectId, members[memberCount - 1], kSelectionIdBytes);
				}

				changes++;
				return true;
			}
		}

		if (memberCount >= kSelectionSetMost)
		{
			return false;
		}

		// A module selected before isn't part of a set of objects: it goes
		if (selected.kind != SelectedKind::Object)
		{
			memberCount = 0;
		}

		memcpy(members[memberCount++], picked.objectId, kSelectionIdBytes);
		selected.kind = SelectedKind::Object;
		selected.moduleIndex = 0;
		memcpy(selected.objectId, picked.objectId, kSelectionIdBytes);
		changes++;
		return true;
	}

	size_t Selection_SetCount()
	{
		return memberCount;
	}

	bool Selection_SetMember(size_t index, uint8_t *id)
	{
		if (index >= memberCount || !id)
		{
			return false;
		}

		memcpy(id, members[index], kSelectionIdBytes);
		return true;
	}

	void Selection_Clear()
	{
		Selected const nothing = { SelectedKind::Nothing, 0, { 0 }, 0 };
		Selection_Set(nothing);
	}

	Selected Selection_Get()
	{
		return selected;
	}

	uint32_t Selection_Changes()
	{
		return changes;
	}

	void Selection_Words(const PickReport &report, char *words, size_t size)
	{
		if (!words || size == 0)
		{
			return;
		}

		char distance[32];
		Metres(report.distance, distance, sizeof(distance));

		switch (report.found)
		{
			case PickFound::Module:
			{
				// The name's bytes, up to its first 0, each one outside space to ~ shown as "?"
				char name[kSelectionModuleNameBytes + 1];
				size_t length = 0;

				while (length < kSelectionModuleNameBytes && report.moduleName[length] != 0)
				{
					uint8_t const letter = report.moduleName[length];
					name[length] = (letter >= ' ' && letter <= '~') ? (char)letter : '?';
					length++;
				}

				name[length] = '\0';
				_snprintf(words, size - 1, "Picked: module %d (%s), %s away", (int)report.moduleIndex, length ? name : "no name", distance);
				break;
			}
			case PickFound::Object:
			{
				char id[kSelectionIdBytes * 2 + 1];

				for (size_t i = 0; i < kSelectionIdBytes; i++)
				{
					_snprintf(id + i * 2, 3, "%02X", (unsigned)report.objectId[i]);
				}

				id[kSelectionIdBytes * 2] = '\0';
				_snprintf(words, size - 1, "Picked: object %s, %s away", id, distance);
				break;
			}
			case PickFound::Nothing:
				_snprintf(words, size - 1, "Nothing there to pick");
				break;
			case PickFound::NotInTheFile:
				_snprintf(words, size - 1, "Not picked: the game made that, and the level file doesn't name it");
				break;
			case PickFound::NumberRefused:
				_snprintf(words, size - 1, "Not picked: that module's number isn't in the level's list");
				break;
			default:
				_snprintf(words, size - 1, "Not picked: no line could be made from that click");
				break;
		}

		// _snprintf leaves out the 0 when the words fill the room, so the last place always gets one
		words[size - 1] = '\0';
	}

	const char *Selection_UsePickToggling(const PickReport &report)
	{
		if (report.found != PickFound::Object)
		{
			return Selection_UsePick(report);
		}

		Selected picked = { SelectedKind::Object, 0, { 0 } };
		memcpy(picked.objectId, report.objectId, kSelectionIdBytes);

		if (!Selection_Toggle(picked))
		{
			_snprintf(pickWords, sizeof(pickWords) - 1, "Not selected (the set holds %d objects at most, and an object needs a name)", (int)kSelectionSetMost);
		}
		else
		{
			_snprintf(pickWords, sizeof(pickWords) - 1, "%d object%s selected", (int)memberCount, memberCount == 1 ? "" : "s");
		}

		pickWords[sizeof(pickWords) - 1] = '\0';
		return pickWords;
	}

	const char *Selection_UsePick(const PickReport &report)
	{
		// What the words say: what was found, unless the selection refused it, and then why it wasn't picked, so the status line never
		// says "Picked" about something that isn't selected
		PickReport said = report;

		if (report.found == PickFound::Module)
		{
			Selected const picked = { SelectedKind::Module, report.moduleIndex, { 0 } };

			if (!Selection_Set(picked))
			{
				said.found = PickFound::NumberRefused;
			}
		}
		else if (report.found == PickFound::Object)
		{
			Selected picked = { SelectedKind::Object, 0, { 0 } };
			memcpy(picked.objectId, report.objectId, kSelectionIdBytes);

			if (!Selection_Set(picked))
			{
				said.found = PickFound::NotInTheFile;
			}
		}

		Selection_Words(said, pickWords, sizeof(pickWords));
		return pickWords;
	}
}
