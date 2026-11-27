// fastfile_pack.cpp: a .FFL pack made from a list of files, and its strict checker (fastfile_pack.h says what a pack is, and why the checker is strict).
// Written 2026-11-27 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".
#include "fastfile_pack.h"

#include <string.h>

#include <algorithm>

#include "level_file_name.h"   // LevelFileName_WhyRefused: the game's own rule for a name from a level

namespace model
{
	namespace
	{
		size_t const kHeaderBytes = 20;   // "RFFL", the version, the count, the entry bytes, the data bytes

		// A letter made small as _stricmp makes it, by itself, so no locale can change it: only A to Z change (every name here is plain ASCII: the rule allows
		// nothing else)
		unsigned char Small(unsigned char c)
		{
			return c >= 'A' && c <= 'Z' ? (unsigned char)(c - 'A' + 'a') : c;
		}

		// _stricmp's order: below 0 when a comes first, 0 when the two are the same name to the game, above 0 when b comes first
		int CompareAsTheGame(std::string const &a, std::string const &b)
		{
			size_t const shorter = (std::min)(a.size(), b.size());

			for (size_t i = 0; i < shorter; i++)
			{
				int const x = Small((unsigned char)a[i]);
				int const y = Small((unsigned char)b[i]);

				if (x != y)
				{
					return x - y;
				}
			}

			return a.size() < b.size() ? -1 : a.size() > b.size() ? 1 : 0;
		}

		// An entry's bytes for a name of so many letters: its two numbers, the name, its zero, and zeros to a multiple of 4 (the step both readers take)
		uint64_t EntryBytes(size_t letters)
		{
			return 8 + ((letters + 4) & ~(uint64_t)3);
		}

		uint64_t RoundUpTo4(uint64_t n)
		{
			return (n + 3) & ~(uint64_t)3;
		}

		uint32_t ReadU32(uint8_t const *at)
		{
			return (uint32_t)at[0] | (uint32_t)at[1] << 8 | (uint32_t)at[2] << 16 | (uint32_t)at[3] << 24;
		}

		void AddU32(std::vector<uint8_t> *bytes, uint64_t value)
		{
			for (int k = 0; k < 4; k++)
			{
				bytes->push_back((uint8_t)(value >> (8 * k)));
			}
		}

		bool AllZero(uint8_t const *from, uint64_t count)
		{
			for (uint64_t i = 0; i < count; i++)
			{
				if (from[i] != 0)
				{
					return false;
				}
			}

			return true;
		}

		// Gives up: the checker's answer when something is wrong
		bool Wrong(std::vector<PackListed> *listed, std::string *why, std::string const &words)
		{
			listed->clear();
			*why = words;
			return false;
		}

		// A name from the pack, safe to put in words for the status line or a log (Tao 3: the pack's bytes come from anyone, so a refused name can hold
		// anything, even what would make one line look like two): cut to 210 letters, and any letter no game file's name has shown as '?'
		std::string Shown(std::string const &name)
		{
			char line[211];

			LevelFileName_ForTheLog(name.c_str(), line, sizeof(line));
			return line;
		}

		std::string Entry(size_t i, std::string const &name)
		{
			return "entry " + std::to_string(i + 1) + (name.empty() ? std::string() : " (" + Shown(name) + ")");
		}
	}

	char const *FastFilePack_RefusedWords(PackRefused refused)
	{
		switch (refused)
		{
		case PackRefused::None: return "the pack can be made";
		case PackRefused::NoFiles: return "there are no files to pack";
		case PackRefused::TooManyFiles: return "there are more files than a pack may hold (1,000)";
		case PackRefused::NameRefused: return "a name the game's rule for a file's name refuses (empty, a letter no game file's name has, or a part that goes up a folder or names a device)";
		case PackRefused::NameHasSlash: return "a name with a '/' (the game asks for its files with '\\', so it would never find it)";
		case PackRefused::NameTooLong: return "a name longer than a pack's names may be (199 letters)";
		case PackRefused::NameHasNul: return "a name with a zero inside it (the pack would end the name there)";
		case PackRefused::NameTwice: return "two files with the same name (capitals aside, the game sees one)";
		case PackRefused::FileTooBig: return "a file bigger than a pack's files may be (64 MB)";
		case PackRefused::PackTooBig: return "the pack would be bigger than a pack may be (64 MB)";
		}

		return "an answer this doesn't know";
	}

	PackRefused FastFilePack_WhyNameRefused(std::string const &name)
	{
		// Its zero first: everything after this reads the name as the game will, up to its first zero
		if (name.find('\0') != std::string::npos)
		{
			return PackRefused::NameHasNul;
		}

		if (name.size() > kPackMostNameLetters)
		{
			return PackRefused::NameTooLong;
		}

		if (name.find('/') != std::string::npos)
		{
			return PackRefused::NameHasSlash;
		}

		return LevelFileName_WhyRefused(name.c_str()) ? PackRefused::NameRefused : PackRefused::None;
	}

	bool FastFilePack_Build(std::vector<PackFile> const &files, std::vector<uint8_t> *pack, PackRefused *refused, size_t *which)
	{
		pack->clear();
		*refused = PackRefused::None;
		*which = 0;

		auto const no = [&](PackRefused why, size_t at) {
			pack->clear();
			*refused = why;
			*which = at;
			return false;
		};

		if (files.empty())
		{
			return no(PackRefused::NoFiles, 0);
		}

		if (files.size() > kPackMostFiles)
		{
			return no(PackRefused::TooManyFiles, kPackMostFiles);
		}

		for (size_t i = 0; i < files.size(); i++)
		{
			PackRefused const nameWhy = FastFilePack_WhyNameRefused(files[i].name);

			if (nameWhy != PackRefused::None)
			{
				return no(nameWhy, i);
			}

			if (files[i].bytes.size() > kPackMostFileBytes)
			{
				return no(PackRefused::FileTooBig, i);
			}
		}

		// The pack's order: the names as _stricmp orders them (AS STOCK'S ARE). Two that compare the same are one file to the game
		std::vector<size_t> order(files.size());

		for (size_t i = 0; i < order.size(); i++)
		{
			order[i] = i;
		}

		std::sort(order.begin(), order.end(), [&files](size_t a, size_t b) {
			int const c = CompareAsTheGame(files[a].name, files[b].name);
			return c < 0 || (c == 0 && a < b);
		});

		for (size_t k = 1; k < order.size(); k++)
		{
			if (CompareAsTheGame(files[order[k - 1]].name, files[order[k]].name) == 0)
			{
				return no(PackRefused::NameTwice, (std::max)(order[k - 1], order[k]));   // (std::max): a max macro from a Windows header can never reach it
			}
		}

		// The sizes, all worked out before a byte is written, in 64 bits (each part is held to its limit above, so none of this can wrap)
		uint64_t entryBytes = 0;

		for (size_t i : order)
		{
			entryBytes += EntryBytes(files[i].name.size());
		}

		uint64_t dataBytes = 0;

		for (size_t i : order)
		{
			dataBytes = RoundUpTo4(dataBytes) + files[i].bytes.size();

			if (kHeaderBytes + entryBytes + RoundUpTo4(dataBytes) > kPackMostBytes)
			{
				return no(PackRefused::PackTooBig, i);
			}
		}

		dataBytes = RoundUpTo4(dataBytes);
		pack->reserve((size_t)(kHeaderBytes + entryBytes + dataBytes));
		pack->insert(pack->end(), { 'R', 'F', 'F', 'L' });
		AddU32(pack, 0);
		AddU32(pack, files.size());
		AddU32(pack, entryBytes);
		AddU32(pack, dataBytes);

		uint64_t start = 0;

		for (size_t i : order)
		{
			std::string const &name = files[i].name;

			AddU32(pack, start);
			AddU32(pack, files[i].bytes.size());
			pack->insert(pack->end(), name.begin(), name.end());
			pack->insert(pack->end(), (size_t)(EntryBytes(name.size()) - 8 - name.size()), 0);   // its zero, and zeros to a multiple of 4
			start = RoundUpTo4(start + files[i].bytes.size());
		}

		for (size_t i : order)
		{
			std::vector<uint8_t> const &bytes = files[i].bytes;

			pack->insert(pack->end(), bytes.begin(), bytes.end());
			pack->insert(pack->end(), (size_t)(RoundUpTo4(bytes.size()) - bytes.size()), 0);
		}

		return true;
	}

	bool FastFilePack_Check(uint8_t const *bytes, size_t size, std::vector<PackListed> *listed, std::string *why)
	{
		listed->clear();
		why->clear();

		if (!bytes || size < kHeaderBytes)
		{
			return Wrong(listed, why, "the pack is shorter than its 20-byte header");
		}

		if (size > kPackMostBytes)
		{
			return Wrong(listed, why, "the pack is bigger than a pack may be (64 MB)");
		}

		if (memcmp(bytes, "RFFL", 4) != 0)
		{
			return Wrong(listed, why, "the pack doesn't start with RFFL");
		}

		uint32_t const version = ReadU32(bytes + 4);
		uint32_t const count = ReadU32(bytes + 8);
		uint64_t const entryBytes = ReadU32(bytes + 12);
		uint64_t const dataBytes = ReadU32(bytes + 16);

		if (version != 0)
		{
			return Wrong(listed, why, "the pack's version is " + std::to_string(version) + ", and both readers take only 0");
		}

		if (count == 0)
		{
			return Wrong(listed, why, "the pack holds no files");
		}

		if (count > kPackMostFiles)
		{
			return Wrong(listed, why, "the pack claims " + std::to_string(count) + " files, more files than a pack may hold (1,000)");
		}

		if (kHeaderBytes + entryBytes + dataBytes != size)
		{
			return Wrong(listed, why, "the sizes don't add up to the pack's bytes: 20 + " + std::to_string(entryBytes) + " + " + std::to_string(dataBytes) + " is not " +
				std::to_string(size));
		}

		if (dataBytes % 4 != 0)
		{
			return Wrong(listed, why, "the data bytes (" + std::to_string(dataBytes) + ") aren't a multiple of 4");
		}

		uint8_t const *const data = bytes + kHeaderBytes + entryBytes;
		uint64_t const entriesEnd = kHeaderBytes + entryBytes;
		uint64_t at = kHeaderBytes;
		uint64_t expectedStart = 0;   // where the next file must start: the last one's end, rounded up to 4
		uint64_t lastEnd = 0;
		std::vector<PackListed> found;

		found.reserve(count);

		for (uint32_t i = 0; i < count; i++)
		{
			if (at + 8 > entriesEnd)
			{
				return Wrong(listed, why, Entry(i, "") + " runs past the entries");
			}

			uint32_t const offset = ReadU32(bytes + at);
			uint32_t const length = ReadU32(bytes + at + 4);
			uint8_t const *const nameAt = bytes + at + 8;
			uint8_t const *const zero = (uint8_t const *)memchr(nameAt, 0, (size_t)(entriesEnd - (at + 8)));

			if (!zero)
			{
				return Wrong(listed, why, Entry(i, "") + "'s name has no zero inside the entries");
			}

			std::string const name((char const *)nameAt, (size_t)(zero - nameAt));
			uint64_t const step = EntryBytes(name.size());

			if (at + step > entriesEnd)
			{
				return Wrong(listed, why, Entry(i, name) + " runs past the entries");
			}

			PackRefused const nameWhy = FastFilePack_WhyNameRefused(name);

			if (nameWhy != PackRefused::None)
			{
				return Wrong(listed, why, Entry(i, name) + "'s name is refused: " + FastFilePack_RefusedWords(nameWhy));
			}

			if (!found.empty())
			{
				int const c = CompareAsTheGame(found.back().name, name);

				if (c == 0)
				{
					return Wrong(listed, why, Entry(i, name) + " is there twice (capitals aside)");
				}

				if (c > 0)
				{
					return Wrong(listed, why, Entry(i, name) + " is out of _stricmp's order: it comes before " + Shown(found.back().name));
				}
			}

			if (length > kPackMostFileBytes)
			{
				return Wrong(listed, why, Entry(i, name) + " is longer than a file in a pack may be (64 MB)");
			}

			if (offset % 4 != 0)
			{
				return Wrong(listed, why, Entry(i, name) + " doesn't start on a multiple of 4");
			}

			if (offset != expectedStart)
			{
				return Wrong(listed, why, Entry(i, name) + " doesn't start where the last one ended, rounded up to 4 (byte " + std::to_string(expectedStart) + " of the data)");
			}

			if ((uint64_t)offset + length > dataBytes)
			{
				return Wrong(listed, why, Entry(i, name) + " runs past the data");
			}

			// the zeros between the last file and this one
			if (!AllZero(data + lastEnd, offset - lastEnd))
			{
				return Wrong(listed, why, "the bytes before " + Entry(i, name) + "'s data aren't zeros");
			}

			PackListed one;

			one.name = name;
			one.offset = offset;
			one.length = length;
			found.push_back(one);
			lastEnd = (uint64_t)offset + length;
			expectedStart = RoundUpTo4(lastEnd);
			at += step;
		}

		if (at != entriesEnd)
		{
			return Wrong(listed, why, "the walk over the entries ends at byte " + std::to_string(at) + ", but they end at byte " + std::to_string(entriesEnd));
		}

		if (RoundUpTo4(lastEnd) != dataBytes)
		{
			return Wrong(listed, why, "the data bytes don't end where the last file does, rounded up to 4 (" + std::to_string(RoundUpTo4(lastEnd)) + ", not " +
				std::to_string(dataBytes) + ")");
		}

		if (!AllZero(data + lastEnd, dataBytes - lastEnd))
		{
			return Wrong(listed, why, "the bytes after the last file aren't zeros");
		}

		*listed = found;
		return true;
	}
}
