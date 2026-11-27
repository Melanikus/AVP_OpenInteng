// sound_dir_add.cpp: see sound_dir_add.h. Written 2026-11-27 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#include "sound_dir_add.h"

#include <stdint.h>
#include <string.h>

#include "level_file_name.h"   // LevelFileName_WhyRefused: the game's rule for a name from a level

namespace model
{
	namespace
	{
		size_t const kHeading = 12;            // a chunk's 8-letter name, then its size, which counts these 12 bytes too
		size_t const kLargest = 0x7fffffff;    // the most a size field holds

		struct Span
		{
			size_t at;
			size_t size;
		};

		int32_t IntAt(std::vector<char> const &bytes, size_t at)
		{
			int32_t value = 0;

			memcpy(&value, bytes.data() + at, sizeof(value));
			return value;
		}

		void PutIntAt(std::vector<char> *bytes, size_t at, int32_t value)
		{
			memcpy(bytes->data() + at, &value, sizeof(value));
		}

		bool NameIs(std::vector<char> const &bytes, size_t at, char const *name)
		{
			return at + 8 <= bytes.size() && memcmp(bytes.data() + at, name, 8) == 0;
		}

		// The chunks from `from` to `to`, each heading inside and each size at least a heading and at most what is left: false when they don't fill it exactly
		bool Children(std::vector<char> const &bytes, size_t from, size_t to, std::vector<Span> *out)
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

		SoundDirAddResult Refuse(SoundDirAddResult result, std::string const &words, std::string *why)
		{
			if (why)
			{
				*why = words;
			}

			return result;
		}
	}

	SoundDirAddResult SoundDirAdd_ToImage(std::vector<char> const &image, std::string const &folder, std::vector<char> *out, std::string *why)
	{
		if (!out)
		{
			return Refuse(SoundDirAddResult::BadCall, "no room for the answer", why);
		}

		out->clear();

		if (folder.empty() || folder.find('/') != std::string::npos || folder.find('\0') != std::string::npos)
		{
			return Refuse(SoundDirAddResult::BadCall, "a sound folder's name is not empty, and is spelled with backslashes", why);
		}

		if (char const *const refused = LevelFileName_WhyRefused(folder.c_str()))
		{
			return Refuse(SoundDirAddResult::BadCall, std::string("the sound folder's name is refused by the game's rule: ") + refused, why);
		}

		// THE FILE: REBINFF2, its size the whole of it, then its chunks
		std::vector<Span> top;

		if (image.size() < kHeading || image.size() > kLargest || !NameIs(image, 0, "REBINFF2") || (size_t)IntAt(image, 8) != image.size() ||
			!Children(image, kHeading, image.size(), &top))
		{
			return Refuse(SoundDirAddResult::CantWalk, "the level's bytes can't be walked chunk by chunk", why);
		}

		// THE FIRST REBENVDT, as the game's lookup_single_child gives it
		size_t environment = top.size();

		for (size_t i = 0; i < top.size() && environment == top.size(); i++)
		{
			environment = NameIs(image, top[i].at, "REBENVDT") ? i : environment;
		}

		if (environment == top.size())
		{
			return Refuse(SoundDirAddResult::NoEnvironment, "the level has no environment data (REBENVDT) to hold a sound folder", why);
		}

		Span const env = top[environment];
		std::vector<Span> inside;

		if (!Children(image, env.at + kHeading, env.at + env.size, &inside))
		{
			return Refuse(SoundDirAddResult::CantWalk, "the level's environment data (REBENVDT) can't be walked chunk by chunk", why);
		}

		for (Span const &chunk : inside)
		{
			if (NameIs(image, chunk.at, "SOUNDDIR"))
			{
				return Refuse(SoundDirAddResult::HasOne, "the level has a sound folder (SOUNDDIR) already", why);
			}
		}

		// THE NEW CHUNK, as Sound_Directory_Chunk writes it: the heading, the name, its zero, zeros to a multiple of 4
		size_t const payload = (folder.size() + 4) & ~(size_t)3;
		size_t const chunkSize = kHeading + payload;

		if (image.size() + chunkSize > kLargest || env.size + chunkSize > kLargest)
		{
			return Refuse(SoundDirAddResult::CantWalk, "the level would be too big for its size fields", why);
		}

		std::vector<char> chunk(chunkSize, 0);

		memcpy(chunk.data(), "SOUNDDIR", 8);
		PutIntAt(&chunk, 8, (int32_t)chunkSize);
		memcpy(chunk.data() + kHeading, folder.data(), folder.size());

		// AT THE END OF REBENVDT; then its size and the file's made right
		size_t const end = env.at + env.size;

		*out = image;
		out->insert(out->begin() + (ptrdiff_t)end, chunk.begin(), chunk.end());
		PutIntAt(out, env.at + 8, (int32_t)(env.size + chunkSize));
		PutIntAt(out, 8, (int32_t)out->size());
		return SoundDirAddResult::Added;
	}

	char const *SoundDirAddResultWords(SoundDirAddResult result)
	{
		switch (result)
		{
		case SoundDirAddResult::Added:
			return "added";
		case SoundDirAddResult::BadCall:
			return "not a call it takes";
		case SoundDirAddResult::CantWalk:
			return "the level's bytes can't be walked";
		case SoundDirAddResult::NoEnvironment:
			return "the level has no environment data";
		case SoundDirAddResult::HasOne:
			return "the level has a sound folder already";
		}

		return "an unknown result";
	}
}
