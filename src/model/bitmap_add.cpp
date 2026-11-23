// bitmap_add.cpp: a new picture named in a level's BMPNAMES list, in the level's bytes (bitmap_add.h says what changes, the entry and what is refused).
// Written 2026-11-23 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "bitmap_add.h"

#include <string.h>

#include "level_file_name.h"   // LevelFileName_WhyRefused: the game's own rule for a name from a level

namespace model
{
	namespace
	{
		typedef std::vector<char> Bytes;

		size_t const kHeading = 12;                 // a chunk's 8-letter name, then its size, which counts these 12 bytes too
		size_t const kEntryNumbers = 20;            // an entry's five numbers, before its name
		size_t const kLargest = 0x7fffffff;         // the most a size field holds
		int32_t const kMostVersion = 0xfff;         // the version << 8 has to fit the entry's 20 bits
		int32_t const kGameMostIndex = 32767;       // the loader refuses an entry's index outside 0 to this (bitmap_names.cpp)

		struct Span
		{
			size_t at;
			size_t size;
		};

		int32_t IntAt(Bytes const &bytes, size_t at)
		{
			int32_t value;

			memcpy(&value, bytes.data() + at, sizeof(value));
			return value;
		}

		void PutIntAt(Bytes *bytes, size_t at, int32_t value)
		{
			memcpy(bytes->data() + at, &value, sizeof(value));
		}

		bool NameIs(Bytes const &bytes, size_t at, char const *name)
		{
			return memcmp(bytes.data() + at, name, 8) == 0;
		}

		// The chunks from `from` to `to`, each wholly inside: false when one isn't (hostile bytes are never walked past their end)
		bool Children(Bytes const &bytes, size_t from, size_t to, std::vector<Span> *out)
		{
			out->clear();

			if (to > bytes.size() || from > to)
			{
				return false;
			}

			for (size_t at = from; at < to; )
			{
				if (to - at < kHeading)
				{
					return false;
				}

				int32_t const stored = IntAt(bytes, at + 8);

				if (stored < (int32_t)kHeading || (size_t)stored > to - at)
				{
					return false;
				}

				out->push_back(Span{ at, (size_t)stored });
				at += (size_t)stored;
			}

			return true;
		}

		// The same name in any capitals, as the game compares two pictures' names (_stricmp: A to Z only)
		bool SameName(char const *a, size_t length, std::string const &b)
		{
			if (length != b.size())
			{
				return false;
			}

			for (size_t i = 0; i < length; i++)
			{
				char const x = (a[i] >= 'a' && a[i] <= 'z') ? (char)(a[i] - 'a' + 'A') : a[i];
				char const y = (b[i] >= 'a' && b[i] <= 'z') ? (char)(b[i] - 'a' + 'A') : b[i];

				if (x != y)
				{
					return false;
				}
			}

			return true;
		}

		BitmapAddResult Refuse(BitmapAddResult result, std::string const &words, std::string *why)
		{
			*why = words;
			return result;
		}
	}

	BitmapAddResult BitmapAdd_ToImage(std::vector<char> const &image, std::string const &name, uint32_t width, uint32_t height, std::vector<char> *out,
		int32_t *index, std::string *why)
	{
		std::string notSaid;

		why = why ? why : &notSaid;
		why->clear();

		if (!out || !index)
		{
			if (out)
			{
				out->clear();
			}

			if (index)
			{
				*index = -1;
			}

			return Refuse(BitmapAddResult::BadCall, "no room for the answer", why);
		}

		out->clear();
		*index = -1;

		if (width == 0 || height == 0 || width > 0xffff || height > 0xffff)
		{
			return Refuse(BitmapAddResult::BadCall, "a picture of " + std::to_string(width) + " x " + std::to_string(height) + ": each side 1 to 65535", why);
		}

		// a zero byte would end the name early, for the rule and for the game alike
		char const *const refused = name.find('\0') != std::string::npos ? kLevelNameCharacter : LevelFileName_WhyRefused(name.c_str());

		if (refused)
		{
			char shown[kLevelFileNameMost + 8];

			LevelFileName_ForTheLog(name.c_str(), shown, sizeof(shown));
			return Refuse(BitmapAddResult::BadCall, std::string("the game won't open ") + shown + ": " + refused, why);
		}

		// THE FILE: REBINFF2, its size the whole of it, then its chunks
		std::vector<Span> top;

		if (image.size() < kHeading || image.size() > kLargest || !NameIs(image, 0, "REBINFF2") || (size_t)IntAt(image, 8) != image.size() ||
			!Children(image, kHeading, image.size(), &top))
		{
			return Refuse(BitmapAddResult::CantWalk, "the level's bytes can't be walked chunk by chunk", why);
		}

		// THE FIRST REBENVDT, as the game's lookup_single_child gives it
		size_t environment = top.size();

		for (size_t i = 0; i < top.size() && environment == top.size(); i++)
		{
			environment = NameIs(image, top[i].at, "REBENVDT") ? i : environment;
		}

		if (environment == top.size())
		{
			return Refuse(BitmapAddResult::NoPictureList, "the level has no environment data (REBENVDT), so no list of pictures", why);
		}

		Span const env = top[environment];
		std::vector<Span> inside;

		if (!Children(image, env.at + kHeading, env.at + env.size, &inside))
		{
			return Refuse(BitmapAddResult::CantWalk, "the level's environment data (REBENVDT) can't be walked chunk by chunk", why);
		}

		size_t lists = 0, versions = 0;
		Span list = {}, version = {};

		for (Span const &chunk : inside)
		{
			if (NameIs(image, chunk.at, "BMPNAMES"))
			{
				list = chunk;
				lists++;
			}
			else if (NameIs(image, chunk.at, "BMNAMVER"))
			{
				version = chunk;
				versions++;
			}
		}

		if (lists == 0)
		{
			return Refuse(BitmapAddResult::NoPictureList, "the level has no list of pictures (BMPNAMES)", why);
		}

		if (lists > 1 || versions > 1)
		{
			return Refuse(BitmapAddResult::Unusual, "the level has " + std::to_string(lists) + " lists of pictures (BMPNAMES) and " + std::to_string(versions) +
				" of their version (BMNAMVER): a list this adds to has one of each, at most", why);
		}

		// THE VERSION: BMNAMVER's number, or 0 when there is none (as get_version_num gives it)
		int32_t listVersion = 0;

		if (versions == 1)
		{
			if (version.size < kHeading + 4)
			{
				return Refuse(BitmapAddResult::CantWalk, "the list's version (BMNAMVER) holds no number", why);
			}

			listVersion = IntAt(image, version.at + kHeading);
		}

		if (listVersion < 0 || listVersion > kMostVersion)
		{
			return Refuse(BitmapAddResult::ListFull, "the list's version is " + std::to_string(listVersion) + ": a new entry's version holds 0 to " +
				std::to_string(kMostVersion), why);
		}

		// THE LIST: its count word, then each entry walked as the game's loader walks it, its name looked for inside the chunk
		size_t const payload = list.at + kHeading;
		size_t const end = list.at + list.size;

		if (end - payload < 4)
		{
			return Refuse(BitmapAddResult::CantWalk, "the list of pictures (BMPNAMES) holds no count", why);
		}

		int32_t const word = IntAt(image, payload);
		int32_t const count = word & 0xffff;

		if ((word >> 16) & 0xffff)
		{
			return Refuse(BitmapAddResult::Unusual, "the list's count word has an old version in its top 16 bits (" + std::to_string((word >> 16) & 0xffff) +
				"), which the game would make the list's version: no stock level has one", why);
		}

		size_t at = payload + 4;
		int32_t mostIndex = 0;

		for (int32_t i = 0; i < count; i++)
		{
			if (end - at < kEntryNumbers + 4)
			{
				return Refuse(BitmapAddResult::CantWalk, "the list of pictures ends inside entry " + std::to_string(i + 1) + " of " + std::to_string(count), why);
			}

			int32_t const entryIndex = IntAt(image, at + 4);
			char const *const entryName = image.data() + at + kEntryNumbers;
			char const *const zero = (char const *)memchr(entryName, 0, end - at - kEntryNumbers);

			if (entryIndex < 0 || entryIndex > kGameMostIndex || !zero)
			{
				return Refuse(BitmapAddResult::CantWalk, "entry " + std::to_string(i + 1) + " of the list of pictures has an index the game refuses, or a name with no end",
					why);
			}

			size_t const length = (size_t)(zero - entryName);
			size_t const field = length + (4 - length % 4);   // its letters, its zero, and zeros to a multiple of 4 (fill_data_block's)

			if (field > end - at - kEntryNumbers)
			{
				return Refuse(BitmapAddResult::CantWalk, "entry " + std::to_string(i + 1) + " of the list of pictures runs past the list's end", why);
			}

			if (SameName(entryName, length, name))
			{
				return Refuse(BitmapAddResult::NameInList, "the level's list names " + name + " already (entry " + std::to_string(i + 1) + ", index " +
					std::to_string(entryIndex) + ")", why);
			}

			mostIndex = entryIndex > mostIndex ? entryIndex : mostIndex;
			at += kEntryNumbers + field;
		}

		size_t const entriesEnd = at;
		int32_t const newIndex = mostIndex + 1;   // never 0: every stock level's indices start at 1 or more

		if (newIndex > kBitmapAddMostIndex || count + 1 > 0xffff)
		{
			return Refuse(BitmapAddResult::ListFull, "the list's next index would be " + std::to_string(newIndex) + " (the editor writes at most " +
				std::to_string(kBitmapAddMostIndex) + ")", why);
		}

		// THE ENTRY, as fill_data_block writes one
		Bytes entry(kEntryNumbers, 0);

		PutIntAt(&entry, 0, kBitmapAddFlags);
		PutIntAt(&entry, 4, newIndex);
		PutIntAt(&entry, 8, (listVersion << 8) & 0x000fffff);   // the version; enum 0 in the top 12 bits
		PutIntAt(&entry, 12, kBitmapAddPriority);
		PutIntAt(&entry, 16, (int32_t)((height << 16) | width));
		entry.insert(entry.end(), name.begin(), name.end());
		entry.resize(kEntryNumbers + name.size() + (4 - name.size() % 4), 0);

		size_t const madeVersion = versions == 0 ? kHeading + 4 : 0;   // a BMNAMVER made when there is none

		if (image.size() + entry.size() + madeVersion > kLargest)
		{
			return Refuse(BitmapAddResult::ListFull, "the level would be larger than a level's size can say", why);
		}

		// THE NEW BYTES: everything before the list's end, the entry, then everything after it
		out->reserve(image.size() + entry.size() + madeVersion);
		out->insert(out->end(), image.begin(), image.begin() + (std::ptrdiff_t)entriesEnd);
		out->insert(out->end(), entry.begin(), entry.end());
		out->insert(out->end(), image.begin() + (std::ptrdiff_t)entriesEnd, image.end());

		PutIntAt(out, payload, count + 1);
		PutIntAt(out, list.at + 8, (int32_t)(list.size + entry.size()));

		if (versions == 1)
		{
			// one more, where it now is (after the list, it moved by the entry's size)
			size_t const versionAt = version.at > list.at ? version.at + entry.size() : version.at;

			PutIntAt(out, versionAt + kHeading, listVersion + 1);
		}
		else
		{
			// made at the end of REBENVDT, holding 1 (inc_version_num's new BMP_Names_Version_Chunk)
			Bytes made(kHeading + 4, 0);

			memcpy(made.data(), "BMNAMVER", 8);
			PutIntAt(&made, 8, (int32_t)made.size());
			PutIntAt(&made, kHeading, 1);
			out->insert(out->begin() + (std::ptrdiff_t)(env.at + env.size + entry.size()), made.begin(), made.end());
		}

		PutIntAt(out, env.at + 8, (int32_t)(env.size + entry.size() + madeVersion));
		PutIntAt(out, 8, (int32_t)out->size());

		*index = newIndex;
		return BitmapAddResult::Added;
	}

	char const *BitmapAddResultWords(BitmapAddResult result)
	{
		switch (result)
		{
		case BitmapAddResult::Added:
			return "added";
		case BitmapAddResult::BadCall:
			return "not a call it takes";
		case BitmapAddResult::CantWalk:
			return "the level's bytes can't be walked";
		case BitmapAddResult::NoPictureList:
			return "the level has no list of pictures";
		case BitmapAddResult::Unusual:
			return "the list isn't one this adds to";
		case BitmapAddResult::NameInList:
			return "the list names it already";
		case BitmapAddResult::ListFull:
			return "the list has no room";
		}

		return "unknown";
	}
}
