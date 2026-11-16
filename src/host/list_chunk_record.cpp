// See list_chunk_record.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "list_chunk_record.h"

#include <stdio.h>
#include <string.h>

#include <exception>
#include <vector>

#include "ai_fields.h"
#include "field_descriptor_from_model.h"
#include "field_panel_host.h"
#include "real_numbers.h"   // IsNaNOrEndless, by the bits: the editor is built with /fp:fast
#include "tracks_fields.h"
#include "tracks_readers.h"

namespace host
{
	namespace
	{
		// The words a refusal gives that aren't the model's own. Static, so a reader of *why can keep the pointer.
		char const NoRecordWords[] = "there's no record to fill";
		char const NoPayloadWords[] = "there's no chunk to show";
		char const NoPlaceForCountWords[] = "there's no place to put the count";
		char const UnknownChunkWords[] = "the editor doesn't know this kind of chunk";
		char const NoItemsWords[] = "this kind of chunk has no items: it's shown whole, not item by item";
		char const NotYetWords[] = "this kind of chunk's items aren't shown yet";
		char const MissingPartWords[] = "the editor's table for this chunk has no such part";
		char const NoSuchItemWords[] = "the chunk has no such item";
		char const TooManyFieldsWords[] = "the chunk has more fields than the panel shows";
		char const NoMemoryWords[] = "there isn't memory enough to show the chunk";

		bool Refuse(FieldRecord *record, char const **why, char const *words)
		{
			if (record)
			{
				EmptyRecord(record);
			}

			if (why)
			{
				*why = words;
			}

			return false;
		}

		// A refusal whose words are made here: kept in the record (which is empty otherwise), so *why stays good as long as the record.
		bool RefuseWith(FieldRecord *record, char const **why, std::string const &words)
		{
			EmptyRecord(record);
			record->text.push_back(words);

			if (why)
			{
				*why = record->text.back().c_str();
			}

			return false;
		}

		// Runs what builds a record, and refuses the record, with nothing shown, when there isn't memory enough: as any other mistake does.
		template <class Body>
		bool Guarded(FieldRecord *record, char const **why, Body const &body)
		{
			try
			{
				return body();
			}
			catch (std::exception const &)
			{
				return Refuse(record, why, NoMemoryWords);
			}
		}

		// A flags field's value as the 32 bits the panel reads its bits from. What the file holds is a whole number of 32 bits, signed or not,
		// so anything else (a fraction, "not a number", endless, a number too big) isn't a flags word: false. "Not a number" passes every
		// comparison, and the editor is built with /fp:fast (real_numbers.h says why that matters), so it and endless are refused first, by
		// their bits, before the number is turned into a whole one: only a number in the range is ever converted.
		bool WordOf(double number, uint32_t *word)
		{
			if (IsNaNOrEndless(number) || number < -2147483648.0 || number > 4294967295.0)
			{
				return false;
			}

			int64_t const whole = (int64_t)number;
			double const left = number - (double)whole;

			if (left > 0.0 || left < 0.0)
			{
				return false;
			}

			*word = (uint32_t)whole;
			return true;
		}

		// A flags field: one Bit for each of its named bits that has words, each holding the whole word as the panel takes it. Gives back false
		// when the field can't be read or isn't a flags word.
		bool AddBits(PartShown const &part, model::Field const &field, FieldRecord *record)
		{
			double number = 0;
			uint32_t word = 0;

			if (!part.reader->Number(field, &number) || !WordOf(number, &word))
			{
				return false;
			}

			char const *help = 0;
			bool helpKept = false;

			for (size_t i = 0; i < part.bitCount; i++)
			{
				PartBit const &named = part.bits[i];

				if (!named.field || strcmp(named.field, field.name) != 0)
				{
					continue;
				}

				std::string const label = SafeWords(named.label, ui::kFieldNameMost);

				if (label.empty())
				{
					continue;   // a bit nobody gave words isn't shown
				}

				if (!helpKept)
				{
					help = HelpOf(record, field);
					helpKept = true;
				}

				PushBit(record, Keep(record, label), help, named.bit, word);
			}

			return true;
		}

		// A field of three numbers: three Numbers, named for their axis. The label is cut two letters short so the axis fits.
		bool AddTriple(PartShown const &part, model::Field const &field, FieldRecord *record)
		{
			int32_t axes[3] = { 0, 0, 0 };

			for (int axis = 0; axis < 3; axis++)
			{
				if (!part.reader->Axis(field, axis, &axes[axis]))
				{
					return false;
				}
			}

			PushTriple(record, SafeWords(field.label, ui::kFieldNameMost - 2), HelpOf(record, field), field, axes);
			return true;
		}

		// Adds what one field shows, if anything. Gives back false when the field can't be shown: it can't be read from the payload, or it's one
		// this record has no way to show (an object, an orientation, a choice: none of the list chunks has one).
		bool AddField(PartShown const &part, model::Field const &field, FieldRecord *record)
		{
			// A flags field has no label of its own: each of its bits has one.
			if (field.unit == model::Unit::Flags)
			{
				return AddBits(part, field, record);
			}

			// A field the old editor showed no label for isn't shown, and its bytes are left alone.
			std::string const label = SafeWords(field.label, ui::kFieldNameMost);

			if (label.empty())
			{
				return true;
			}

			if (field.storage == model::Storage::ObjectId || field.storage == model::Storage::IntMatrix || field.unit == model::Unit::Choice)
			{
				return false;
			}

			if (field.storage == model::Storage::IntTriple)
			{
				return AddTriple(part, field, record);
			}

			double number = 0;

			if (!part.reader->Number(field, &number))
			{
				return false;
			}

			PushNumber(record, Keep(record, label), HelpOf(record, field), field, number);
			return true;
		}

		// Adds a part's fields in order. When one can't be shown, says which in *problem.
		bool AddPart(PartShown const &part, FieldRecord *record, std::string *problem)
		{
			for (size_t i = 0; i < part.fieldCount; i++)
			{
				if (!AddField(part, part.fields[i], record))
				{
					*problem = SafeWords(part.fields[i].name, ui::kFieldNameMost);
					return false;
				}
			}

			return true;
		}

		bool Assemble(std::string const &title, PartShown const *head, PartShown const *item, FieldRecord *record, char const **why)
		{
			EmptyRecord(record);
			record->title = title;

			std::string problem;

			if (head && !AddPart(*head, record, &problem))
			{
				return RefuseWith(record, why, "the chunk's own field " + problem + " couldn't be shown");
			}

			if (item && !AddPart(*item, record, &problem))
			{
				return RefuseWith(record, why, "the item's field " + problem + " couldn't be shown");
			}

			if (record->list.size() > kFieldPanelHostMostFields)
			{
				return Refuse(record, why, TooManyFieldsWords);
			}

			ui::FieldListProblem refused = { 0, ui::FieldListRefusal::None };

			if (!ui::CheckFieldList(record->list.data(), record->list.size(), &refused))
			{
				char reason[96];
				snprintf(reason, sizeof(reason), "the record didn't pass the panel's check (field %u)", (unsigned)refused.field);
				return RefuseWith(record, why, reason);
			}

			return true;
		}

		// The title: what the chunk is, its name, and which item of how many, counting from 1. The host reads no more than
		// kFieldPanelHostTitleMost (63) bytes of a title, and the item's number is what has to show: the tail is at most 37 bytes (a count is a
		// whole number of 31 bits, so 10 digits), and every label below is 26 letters or fewer, so the title always fits.
		std::string TitleOf(char const *label, char const *chunk, bool hasList, size_t count, size_t item)
		{
			std::string title = std::string(label) + ": " + SafeWords(chunk, 8);

			if (hasList)
			{
				title += count == 0 ? std::string(" (no items)")
				                    : " item " + std::to_string((unsigned long long)item + 1) + "/" + std::to_string((unsigned long long)count);
			}

			return title;
		}

		// ---- Tracks (OBJTRAK2)

		// The track's own numbers and a section's, read by the model's track readers
		class TrackOwnReader : public PartReader
		{
		public:
			TrackOwnReader(uint8_t const *payload, size_t length) : payload(payload), length(length) {}

			bool Number(model::Field const &field, double *value) const override
			{
				return model::ReadTrackField(field, payload, length, value);
			}

		private:
			uint8_t const *payload;
			size_t length;
		};

		class TrackSectionReader : public PartReader
		{
		public:
			TrackSectionReader(uint8_t const *payload, size_t length, size_t section) : payload(payload), length(length), section(section) {}

			// No field of a section that has a label is three numbers (its places and turns are drawn in the view, never labelled), so only one
			// number is read: PartReader's own refusal of a field of three would show the day one is added, saying which.
			bool Number(model::Field const &field, double *value) const override
			{
				return model::ReadTrackSectionField(field, payload, length, section, value);
			}

		private:
			uint8_t const *payload;
			size_t length;
			size_t section;
		};

		// ---- Alternate locations (ALTLOCAT)

		// The chunk's own numbers and a location's, read by the model's alternate location readers
		class LocationsOwnReader : public PartReader
		{
		public:
			LocationsOwnReader(uint8_t const *payload, size_t length) : payload(payload), length(length) {}

			bool Number(model::Field const &field, double *value) const override
			{
				return model::ReadAlternateLocationField(field, payload, length, value);
			}

		private:
			uint8_t const *payload;
			size_t length;
		};

		class LocationReader : public PartReader
		{
		public:
			LocationReader(uint8_t const *payload, size_t length, size_t location) : payload(payload), length(length), location(location) {}

			// No field of a location that has a label is three numbers (its place is drawn in the view), so only one number is read
			bool Number(model::Field const &field, double *value) const override
			{
				return model::ReadAlternateLocationEntryField(field, payload, length, location, value);
			}

		private:
			uint8_t const *payload;
			size_t length;
			size_t location;
		};

		// The end every chunk with a list shares: the item asked for against the items there are, then the record from the chunk's head and that
		// item. itemIsThere says whether the list holds the item (the chunk's own code asked its own place function). A list with no item shows
		// its head alone as item 0, and any other item number is refused. A chunk with no head says so with a part of no fields.
		bool ShowListed(char const *label, char const *chunk, size_t count, bool itemIsThere, size_t item, PartShown const &head,
		                PartShown const &itemPart, FieldRecord *record, char const **why)
		{
			bool const hasItem = count > 0;

			if (hasItem ? !itemIsThere : item != 0)
			{
				return Refuse(record, why, NoSuchItemWords);
			}

			return Assemble(TitleOf(label, chunk, true, count, item), &head, hasItem ? &itemPart : 0, record, why);
		}

		// A track: its own numbers, and section number item of its count. A track with no section shows its own numbers alone, as item 0.
		bool ShowTrack(char const *chunk, uint8_t const *payload, size_t length, size_t item, FieldRecord *record, char const **why)
		{
			size_t count = 0;

			if (!model::TrackSectionCount(payload, length, &count))
			{
				return Refuse(record, why, model::ChunkSizeCheckWords(model::CheckTrackSize(payload, length)));
			}

			size_t place = 0;
			bool const itemIsThere = model::TrackSectionPlace(payload, length, item, &place);
			std::vector<PartBit> bits;

			for (size_t i = 0; i < model::TrackBitCount; i++)
			{
				PartBit const bit = { model::TrackBits[i].field, model::TrackBits[i].bit, model::TrackBits[i].label };
				bits.push_back(bit);
			}

			TrackOwnReader const ownReader(payload, length);
			TrackSectionReader const sectionReader(payload, length, item);
			PartShown const head = { model::TrackFields, model::TrackFieldCount, bits.data(), bits.size(), &ownReader };
			PartShown const section = { model::TrackSectionFields, model::TrackSectionFieldCount, 0, 0, &sectionReader };

			return ShowListed("track", chunk, count, itemIsThere, item, head, section, record, why);
		}

		// An object's alternate locations: the group number, and location number item of its count. A chunk with no location shows the group
		// alone, as item 0.
		bool ShowAlternateLocations(char const *chunk, uint8_t const *payload, size_t length, size_t item, FieldRecord *record, char const **why)
		{
			size_t count = 0;

			if (!model::AlternateLocationCount(payload, length, &count))
			{
				return Refuse(record, why, model::ChunkSizeCheckWords(model::CheckAlternateLocationsSize(payload, length)));
			}

			size_t place = 0;
			bool const itemIsThere = model::AlternateLocationPlace(payload, length, item, &place);
			LocationsOwnReader const ownReader(payload, length);
			LocationReader const locationReader(payload, length, item);
			PartShown const head = { model::AlternateLocationFields, model::AlternateLocationFieldCount, 0, 0, &ownReader };
			PartShown const location = { model::AlternateLocationEntryFields, model::AlternateLocationEntryFieldCount, 0, 0, &locationReader };

			return ShowListed("alternate locations", chunk, count, itemIsThere, item, head, location, record, why);
		}

		// ---- The AI chunks: patrol paths, waypoints, AI modules, the modules a module sees, entry points and zones

		// A part of an AI chunk (ai_fields.h) read by the model's AI readers, at the place its origin gives
		class AiPartReader : public PartReader
		{
		public:
			AiPartReader(model::AiPart const &part, uint8_t const *payload, size_t length, size_t origin)
				: part(part), payload(payload), length(length), origin(origin) {}

			// No field of an AI part that has a label is three numbers (the paths and waypoints are drawn in the view), so only one number is read
			bool Number(model::Field const &field, double *value) const override
			{
				return model::ReadAiField(part, field, payload, length, origin, value, 0);
			}

		private:
			model::AiPart const &part;
			uint8_t const *payload;
			size_t length;
			size_t origin;
		};

		// The words of the bits of one AI part's flags fields, from the model's table of names
		std::vector<PartBit> BitsOfAiPart(model::AiPart const &part)
		{
			std::vector<PartBit> bits;

			for (size_t i = 0; i < model::AiBitCount; i++)
			{
				model::AiBitName const &named = model::AiBits[i];

				if (strncmp(named.chunk, part.chunk, 8) == 0 && strcmp(named.part, part.label) == 0)
				{
					PartBit const bit = { named.field, named.bit, named.label };
					bits.push_back(bit);
				}
			}

			return bits;
		}

		// One AI part ready to be shown: the model's fields, the words of their bits, and a reader at the part's origin. The part points into
		// itself (its reader and its bits are its own), so it's made where it's used and never copied.
		class AiPartShown
		{
		public:
			AiPartShown(model::AiPart const &part, uint8_t const *payload, size_t length, size_t origin)
				: reader(part, payload, length, origin), bits(BitsOfAiPart(part))
			{
				shown.fields = part.fields;
				shown.fieldCount = part.fieldCount;
				shown.bits = bits.data();
				shown.bitCount = bits.size();
				shown.reader = &reader;
			}

			AiPartShown(AiPartShown const &) = delete;
			AiPartShown &operator=(AiPartShown const &) = delete;

			PartShown const &Get() const
			{
				return shown;
			}

		private:
			AiPartReader reader;
			std::vector<PartBit> bits;
			PartShown shown;
		};

		// A chunk with no numbers at its head: a part with no fields, which shows nothing
		PartShown const NoPart = { 0, 0, 0, 0, 0 };

		// Whether the chunk is the one with that name (8 letters, or 7 and a zero: MODZONE)
		bool IsChunk(char const *chunk, char const *id)
		{
			return strncmp(chunk, id, 8) == 0;
		}

		// A patrol path: its own numbers (its flags), and point number item of its count. A path with no point shows its own numbers alone.
		bool ShowPath(char const *chunk, uint8_t const *payload, size_t length, size_t item, FieldRecord *record, char const **why)
		{
			model::PathParts parts = {};
			model::SizeCheck const check = model::CheckPathSize(payload, length, &parts);

			if (check != model::SizeCheck::Right)
			{
				return Refuse(record, why, model::AiSizeCheckWords(check));
			}

			model::AiPart const *const pathPart = model::FindAiPart(chunk, "path");
			model::AiPart const *const pointPart = model::FindAiPart(chunk, "path point");

			if (!pathPart || !pointPart)
			{
				return Refuse(record, why, MissingPartWords);
			}

			size_t origin = 0;
			bool const itemIsThere = model::PathPointOrigin(parts, item, &origin);
			AiPartShown const head(*pathPart, payload, length, parts.headAt);
			AiPartShown const point(*pointPart, payload, length, origin);

			return ShowListed("patrol path", chunk, parts.pointCount, itemIsThere, item, head.Get(), point.Get(), record, why);
		}

		// The waypoints of a module: waypoint number item of its count. A chunk has no numbers at its head. A waypoint's links and module links
		// aren't shown: they need an index of their own (the card under J in inteng-coverage.md).
		bool ShowWaypoints(char const *chunk, uint8_t const *payload, size_t length, size_t item, FieldRecord *record, char const **why)
		{
			model::WaypointParts parts = {};
			model::SizeCheck const check = model::CheckWaypointSize(payload, length, &parts);

			if (check != model::SizeCheck::Right)
			{
				return Refuse(record, why, model::AiSizeCheckWords(check));
			}

			model::AiPart const *const waypointPart = model::FindAiPart(chunk, "waypoint");

			if (!waypointPart)
			{
				return Refuse(record, why, MissingPartWords);
			}

			model::WaypointPlace place = {};
			bool const itemIsThere = model::WaypointOrigin(payload, length, item, &place);
			AiPartShown const waypoint(*waypointPart, payload, length, place.start);

			return ShowListed("waypoints", chunk, parts.count, itemIsThere, item, NoPart, waypoint.Get(), record, why);
		}

		// The modules a module can see (VMDARRAY): item number item of its count
		bool ShowVisibleModules(char const *chunk, uint8_t const *payload, size_t length, size_t item, FieldRecord *record, char const **why)
		{
			size_t count = 0;
			model::SizeCheck const check = model::CheckVModuleArraySize(payload, length, &count);

			if (check != model::SizeCheck::Right)
			{
				return Refuse(record, why, model::AiSizeCheckWords(check));
			}

			model::AiPart const *const modulePart = model::FindAiPart(chunk, "virtual module");

			if (!modulePart)
			{
				return Refuse(record, why, MissingPartWords);
			}

			size_t origin = 0;
			bool const itemIsThere = model::VModuleOrigin(count, item, &origin);
			AiPartShown const module(*modulePart, payload, length, origin);

			return ShowListed("visible modules", chunk, count, itemIsThere, item, NoPart, module.Get(), record, why);
		}

		// The entry points to adjacent modules (ADJMDLEP): item number item of its count
		bool ShowAdjacentModules(char const *chunk, uint8_t const *payload, size_t length, size_t item, FieldRecord *record, char const **why)
		{
			size_t count = 0;
			model::SizeCheck const check = model::CheckAdjacentModulesSize(payload, length, &count);

			if (check != model::SizeCheck::Right)
			{
				return Refuse(record, why, model::AiSizeCheckWords(check));
			}

			model::AiPart const *const modulePart = model::FindAiPart(chunk, "adjacent module");

			if (!modulePart)
			{
				return Refuse(record, why, MissingPartWords);
			}

			size_t origin = 0;
			bool const itemIsThere = model::AdjacentModuleOrigin(count, item, &origin);
			AiPartShown const module(*modulePart, payload, length, origin);

			return ShowListed("adjacent modules", chunk, count, itemIsThere, item, NoPart, module.Get(), record, why);
		}

		// A chunk that is a few fixed bytes and no list (AIMODMAS, AIMODSLA, MODZONE): its numbers, if the chunk has a part with labels, shown as
		// item 0, and any other item refused. partLabel names the chunk's part in the model's table, or is null when the chunk has none.
		bool ShowFlat(char const *label, char const *chunk, char const *partLabel, uint8_t const *payload, size_t length, size_t item,
		              FieldRecord *record, char const **why)
		{
			model::AiChunk const *const ai = model::FindAiChunk(chunk);

			if (!ai)
			{
				return Refuse(record, why, UnknownChunkWords);
			}

			model::SizeCheck const check = model::CheckFlatAiChunkSize(*ai, length);

			if (check != model::SizeCheck::Right)
			{
				return Refuse(record, why, model::AiSizeCheckWords(check));
			}

			if (item != 0)
			{
				return Refuse(record, why, NoSuchItemWords);
			}

			model::AiPart const *const part = partLabel ? model::FindAiPart(chunk, partLabel) : 0;

			if (partLabel && !part)
			{
				return Refuse(record, why, MissingPartWords);
			}

			if (!part)
			{
				return Assemble(TitleOf(label, chunk, false, 0, 0), &NoPart, 0, record, why);
			}

			AiPartShown const head(*part, payload, length, 0);

			return Assemble(TitleOf(label, chunk, false, 0, 0), &head.Get(), 0, record, why);
		}

		// Whichever AI chunk it is
		bool ShowAi(char const *chunk, uint8_t const *payload, size_t length, size_t item, FieldRecord *record, char const **why)
		{
			if (IsChunk(chunk, "AVPPATH2"))
			{
				return ShowPath(chunk, payload, length, item, record, why);
			}

			if (IsChunk(chunk, "WAYPOINT"))
			{
				return ShowWaypoints(chunk, payload, length, item, record, why);
			}

			if (IsChunk(chunk, "VMDARRAY"))
			{
				return ShowVisibleModules(chunk, payload, length, item, record, why);
			}

			if (IsChunk(chunk, "ADJMDLEP"))
			{
				return ShowAdjacentModules(chunk, payload, length, item, record, why);
			}

			if (IsChunk(chunk, "AIMODMAS"))
			{
				return ShowFlat("AI module master", chunk, 0, payload, length, item, record, why);
			}

			if (IsChunk(chunk, "AIMODSLA"))
			{
				return ShowFlat("AI module slave", chunk, "AI module slave", payload, length, item, record, why);
			}

			if (IsChunk(chunk, "MODZONE"))
			{
				return ShowFlat("module zone", chunk, "zone", payload, length, item, record, why);
			}

			// An AI chunk the registry names and this code doesn't know
			return Refuse(record, why, NotYetWords);
		}

		// Says why, when there's somewhere to say it, and gives back false
		bool Say(char const **why, char const *words)
		{
			if (why)
			{
				*why = words;
			}

			return false;
		}

		// How many items an AI chunk holds: the same size checks and counts the records use
		bool CountAiItems(char const *chunk, uint8_t const *payload, size_t length, size_t *count, char const **why)
		{
			model::SizeCheck check = model::SizeCheck::Right;
			size_t found = 0;

			if (IsChunk(chunk, "AVPPATH2"))
			{
				model::PathParts parts = {};

				check = model::CheckPathSize(payload, length, &parts);
				found = parts.pointCount;
			}
			else if (IsChunk(chunk, "WAYPOINT"))
			{
				model::WaypointParts parts = {};

				check = model::CheckWaypointSize(payload, length, &parts);
				found = parts.count;
			}
			else if (IsChunk(chunk, "VMDARRAY"))
			{
				check = model::CheckVModuleArraySize(payload, length, &found);
			}
			else if (IsChunk(chunk, "ADJMDLEP"))
			{
				check = model::CheckAdjacentModulesSize(payload, length, &found);
			}
			else
			{
				// The three small chunks have no list: their size is the game's, and they hold no items
				model::AiChunk const *const ai = model::FindAiChunk(chunk);

				if (!ai)
				{
					return Say(why, NotYetWords);
				}

				check = model::CheckFlatAiChunkSize(*ai, length);
			}

			if (check != model::SizeCheck::Right)
			{
				return Say(why, model::AiSizeCheckWords(check));
			}

			*count = found;
			return true;
		}

		bool CountOf(char const *chunk, uint8_t const *payload, size_t length, size_t *count, char const **why)
		{
			model::ChunkFamily const family = model::FindChunkFamily(chunk);

			if (family == model::ChunkFamily::Tracks)
			{
				return model::TrackSectionCount(payload, length, count) ||
				       Say(why, model::ChunkSizeCheckWords(model::CheckTrackSize(payload, length)));
			}

			if (family == model::ChunkFamily::AlternateLocations)
			{
				return model::AlternateLocationCount(payload, length, count) ||
				       Say(why, model::ChunkSizeCheckWords(model::CheckAlternateLocationsSize(payload, length)));
			}

			if (family == model::ChunkFamily::Ai)
			{
				return CountAiItems(chunk, payload, length, count, why);
			}

			return Say(why, family == model::ChunkFamily::None ? UnknownChunkWords : NoItemsWords);
		}

		bool ShowChunk(char const *chunk, uint8_t const *payload, size_t length, size_t item, FieldRecord *record, char const **why)
		{
			// FindChunkFamily takes no name as no chunk's
			model::ChunkFamily const family = model::FindChunkFamily(chunk);

			if (family == model::ChunkFamily::Tracks)
			{
				return ShowTrack(chunk, payload, length, item, record, why);
			}

			if (family == model::ChunkFamily::AlternateLocations)
			{
				return ShowAlternateLocations(chunk, payload, length, item, record, why);
			}

			if (family == model::ChunkFamily::Ai)
			{
				return ShowAi(chunk, payload, length, item, record, why);
			}

			if (family == model::ChunkFamily::Base || family == model::ChunkFamily::Acoustics)
			{
				return Refuse(record, why, NoItemsWords);
			}

			return Refuse(record, why, UnknownChunkWords);
		}
	}

	bool BuildRecordOfParts(std::string const &title, PartShown const *head, PartShown const *item, FieldRecord *record, char const **why)
	{
		if (!record)
		{
			return Refuse(0, why, NoRecordWords);
		}

		return Guarded(record, why, [&] { return Assemble(title, head, item, record, why); });
	}

	bool CountChunkItems(char const *chunk, uint8_t const *payload, size_t length, size_t *count, char const **why)
	{
		size_t found = 0;

		if (!count)
		{
			return Say(why, NoPlaceForCountWords);
		}

		if (!payload)
		{
			return Say(why, NoPayloadWords);
		}

		if (!CountOf(chunk, payload, length, &found, why))
		{
			return false;
		}

		*count = found;
		return true;
	}

	bool BuildChunkItemRecord(char const *chunk, uint8_t const *payload, size_t length, size_t item, FieldRecord *record, char const **why)
	{
		if (!record)
		{
			return Refuse(0, why, NoRecordWords);
		}

		if (!payload)
		{
			return Refuse(record, why, NoPayloadWords);
		}

		return Guarded(record, why, [&] { return ShowChunk(chunk, payload, length, item, record, why); });
	}
}
