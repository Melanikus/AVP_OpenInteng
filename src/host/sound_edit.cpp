// See sound_edit.h. Written 2026-11-16 by Melanikus (Vasquez) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "sound_edit.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include <windows.h>

namespace host
{
	namespace
	{
		// ftol: toward 0, held inside the 32-bit range of the exe's words (a number out of it is the exe's undefined case: never made here)
		int32_t Ftol(double value)
		{
			if (!(value == value))
			{
				return 0;
			}

			if (value >= 2147483647.0)
			{
				return 2147483647;
			}

			if (value <= -2147483648.0)
			{
				return (int32_t)-2147483647 - 1;
			}

			return (int32_t)value;
		}

		struct Limit
		{
			float lowest;
			float highest;
		};

		// edobtype 0x10006830: DDV_MinMaxFloat after each DDX_Text (0x447a0000, 0x44fa0000, 0xc2400000 / 0x42400000, 0x42c80000)
		Limit const kLimits[4] = { { 0.0f, 1000.0f }, { 0.0f, 2000.0f }, { -48.0f, 48.0f }, { 0.0f, 100.0f } };

		// MFC's _AfxSimpleFloatParse: spaces and tabs, then _tcstod; a 0 read when the first letter is not '0' is no number; then spaces and tabs and the end
		bool SimpleFloatParse(std::string const &text, double *value)
		{
			size_t at = 0;

			while (at < text.size() && (text[at] == ' ' || text[at] == '\t'))
			{
				at++;
			}

			if (text.size() - at > 255)
			{
				return false;   // longer than the edit's window text could be (the DLL's buffers): never a number here
			}

			std::string const rest = text.substr(at);
			char const first = rest.empty() ? '\0' : rest[0];
			char *end = nullptr;
			double const read = strtod(rest.c_str(), &end);

			if (!end || (read == 0.0 && first != '0'))
			{
				return false;
			}

			while (*end == ' ' || *end == '\t')
			{
				end++;
			}

			if (*end != '\0' || !(read == read))
			{
				return false;
			}

			*value = read;
			return true;
		}
	}

	SoundEditRecord SoundEdit_Record(std::string const &name, uint32_t inner, uint32_t outer, int32_t pitch, int32_t volume, uint32_t flags, int32_t probability)
	{
		SoundEditRecord r;

		// 0x4ed112-0x4ed17d: each kept in a float
		r.name = name;
		r.inner = (float)((double)inner * 1e-05);
		r.outer = (float)((double)outer * 1e-05);
		r.pitch = (float)((double)pitch * 0.0078125);
		r.volume = (float)((double)volume * 100.0 * 0.007874016);
		r.flags = flags;
		r.mask = 3;
		r.probability = probability;
		return r;
	}

	std::string SoundEdit_FloatText(float value)
	{
		char text[64];

		_snprintf_s(text, sizeof(text), _TRUNCATE, "%.*g", 6, (double)value);
		return text;
	}

	SoundEditShown SoundEdit_Shown(SoundEditRecord const &record)
	{
		SoundEditShown s;

		// FUN_10006bd0 0x10006be5-0x10006c15: inner, outer, pitch as they are; the volume ftol(v + 0.5) kept in the float (0x1001a9a0)
		s.fields[0] = SoundEdit_FloatText(record.inner);
		s.fields[1] = SoundEdit_FloatText(record.outer);
		s.fields[2] = SoundEdit_FloatText(record.pitch);
		s.fields[3] = SoundEdit_FloatText((float)Ftol((double)record.volume + 0.5));

		// 0x10006c60-0x10006c98: a box is set from the flags only when its mask bit is; 0x10006b01-0x10006b43: a box whose mask bit is clear is disabled
		s.playingAtStartLive = (record.mask & 1) != 0;
		s.loopsLive = (record.mask & 2) != 0;
		s.playingAtStart = s.playingAtStartLive && (record.flags & 1) == 0;
		s.loops = s.loopsLive && (record.flags & 2) == 0;
		return s;
	}

	bool SoundEdit_ReadField(int field, std::string const &typed, float *value, std::string *message)
	{
		if (field < 0 || field > 3 || !value)
		{
			return false;
		}

		double read = 0;

		if (!SimpleFloatParse(typed, &read))
		{
			if (message)
			{
				*message = "Please enter a number.";   // AFX_IDP_PARSE_REAL (edobtype's string 61713)
			}

			return false;
		}

		float const kept = (float)read;

		if (kept < kLimits[field].lowest || kept > kLimits[field].highest)
		{
			if (message)
			{
				// AFX_IDP_PARSE_REAL_RANGE (61715), its %1 / %2 the limits as "%.*g"
				*message = "Please enter a number between " + SoundEdit_FloatText(kLimits[field].lowest) + " and " + SoundEdit_FloatText(kLimits[field].highest) + ".";
			}

			return false;
		}

		*value = kept;
		return true;
	}

	void SoundEdit_Ok(SoundEditRecord *record, std::string const &name, float const values[4], bool playingAtStart, bool loops)
	{
		if (!record || !values)
		{
			return;
		}

		// FUN_10006bd0 0x10006cb1-0x10006d76
		record->inner = values[0];
		record->outer = values[1];
		record->pitch = values[2];
		record->volume = (float)((double)values[3] + 0.3937007874015748);   // 0x1001a998: 50 / 127
		record->probability = 0;                                          // AvP: the window's probability is 0 (0x10006c56)
		record->name = name.size() > 255 ? name.substr(0, 255) : name;

		if (record->mask & 1)
		{
			record->flags = playingAtStart ? (record->flags & ~1u) : (record->flags | 1u);
		}

		if (record->mask & 2)
		{
			record->flags = loops ? (record->flags & ~2u) : (record->flags | 2u);
		}
	}

	void SoundEdit_Back(SoundEditRecord const &record, int32_t *inner, int32_t *outer, int32_t *pitch, int32_t *volume, uint32_t *flags, int32_t *probability)
	{
		// 0x4ed205-0x4ed290: ftol(f x 100000.0f), ftol(f x 128.0), ftol(f x 0.01 x 127.0)
		if (inner) *inner = Ftol((double)record.inner * 100000.0);
		if (outer) *outer = Ftol((double)record.outer * 100000.0);
		if (pitch) *pitch = Ftol((double)record.pitch * 128.0);
		if (volume) *volume = Ftol((double)record.volume * 0.01 * 127.0);
		if (flags) *flags = record.flags;
		if (probability) *probability = record.probability;
	}

	SoundEditRecord SoundEdit_IndexedRecord(std::string const &name, uint32_t inner, uint32_t outer, int32_t pitch, int32_t volume, int32_t recordFlags, bool loopsLive)
	{
		// FUN_004ebd60 0x4ebe12-0x4ebe91: the four numbers as the placed sound's; 0x4ebe2b: flags ((char)~bit 0 & 1) << 1; 0x4ebe3b: mask -(loops box != 0) & 2; 0x4ebe2e: probability 0
		SoundEditRecord r = SoundEdit_Record(name, inner, outer, pitch, volume, 0, 0);

		r.flags = ((~(uint32_t)recordFlags) & 1u) << 1;
		r.mask = loopsLive ? 2u : 0u;
		r.probability = 0;
		return r;
	}

	void SoundEdit_IndexedBack(SoundEditRecord const &record, int32_t recordFlags, int32_t *inner, int32_t *outer, int32_t *pitch, int32_t *volume, int32_t *newFlags)
	{
		SoundEdit_Back(record, inner, outer, pitch, volume, nullptr, nullptr);

		// 0x4ebf67-0x4ebf78: the window's bit 1 clear (Sound Loops ticked) sets the record's bit 0, set clears it
		if (newFlags)
		{
			*newFlags = (record.flags & 2u) == 0 ? (recordFlags | 1) : (recordFlags & ~1);
		}
	}

	int32_t SoundEdit_PlayFrequency(int32_t fileFrequency, float pitch)
	{
		// 0x100072df-0x10007364: only when the pitch is not 0; base x 2^(pitch x 0.0833333358 (0x1001a9b0))
		if (pitch == 0.0f)
		{
			return fileFrequency;
		}

		return Ftol((double)fileFrequency * pow(2.0, (double)pitch * (double)0.0833333358168602f));
	}

	int32_t SoundEdit_PlayVolume(float volume)
	{
		// 0x10007367-0x100073a9: v = volume when above 1 (0x1001ac78), else 1; ftol(ln(v x 0.01) x 2048)
		double const v = (double)volume > 1.0 ? (double)volume : 1.0;

		return Ftol(log(v * 0.01) * 2048.0);
	}

	namespace
	{
		// edobtype's finder (FUN_10001680 / FUN_10001a40 / FUN_10001c80, made with the "look in folders below" switch on, 0x10006a93): in a folder, first its matching FILES (not folders,
		// 0x10001cfd), then each folder under it (FindFirstFile "%s\*.*", a name not starting with '.', 0x10001af1) the same way, depth first, in the order Windows gives them; each name is
		// the path under the folder searched ("sub\x.wav"). Held here to 32 folders deep and to `most` names, and a link is never followed (a folder of links that loops can't run on).
		void Find(std::string const &folder, std::string const &under, size_t depth, size_t most, std::vector<std::string> *names)
		{
			if (depth > 32 || names->size() >= most || folder.size() + under.size() > MAX_PATH - 8)
			{
				return;
			}

			WIN32_FIND_DATAA found;
			HANDLE search = FindFirstFileA((folder + "\\" + under + "*.wav").c_str(), &found);

			if (search != INVALID_HANDLE_VALUE)
			{
				do
				{
					size_t const length = strnlen(found.cFileName, sizeof(found.cFileName));

					if (!(found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && length > 0 && under.size() + length < 256)
					{
						names->push_back(under + std::string(found.cFileName, length));
					}
				}
				while (names->size() < most && FindNextFileA(search, &found));

				FindClose(search);
			}

			search = FindFirstFileA((folder + "\\" + under + "*.*").c_str(), &found);

			if (search == INVALID_HANDLE_VALUE)
			{
				return;
			}

			do
			{
				size_t const length = strnlen(found.cFileName, sizeof(found.cFileName));

				if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && !(found.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) && length > 0 && found.cFileName[0] != '.')
				{
					Find(folder, under + std::string(found.cFileName, length) + "\\", depth + 1, most, names);
				}
			}
			while (names->size() < most && FindNextFileA(search, &found));

			FindClose(search);
		}
	}

	std::vector<std::string> SoundEdit_WavNames(std::string const &folder, size_t most)
	{
		std::vector<std::string> names;
		std::string root = folder;

		// "%s\*.wav" (edobtype 0x10006a68), with a folder that may already end with its backslash
		while (!root.empty() && (root.back() == '\\' || root.back() == '/'))
		{
			root.pop_back();
		}

		if (!root.empty())
		{
			Find(root, std::string(), 0, most, &names);
		}

		return names;
	}

	namespace
	{
		std::string Lower(std::string text)
		{
			for (char &letter : text)
			{
				letter = (letter >= 'A' && letter <= 'Z') ? (char)(letter - 'A' + 'a') : letter;
			}

			return text;
		}

		// NTFS keeps a folder's names in the order of their capitals (its upcase table; for the letters ' '..'~' that is plain toupper)
		int CompareInCapitals(std::string const &a, std::string const &b)
		{
			size_t const shorter = a.size() < b.size() ? a.size() : b.size();

			for (size_t i = 0; i < shorter; i++)
			{
				int const x = toupper((unsigned char)a[i]);
				int const y = toupper((unsigned char)b[i]);

				if (x != y)
				{
					return x < y ? -1 : 1;
				}
			}

			return a.size() == b.size() ? 0 : (a.size() < b.size() ? -1 : 1);
		}

		struct FoundName
		{
			std::vector<std::string> parts;   // the folders, then the file
			std::string name;                 // the parts with '\' between
		};

		// the finder's order: at the first part where two names differ, a file before a folder, else the parts in capitals
		bool FinderOrder(FoundName const &a, FoundName const &b)
		{
			size_t const shorter = a.parts.size() < b.parts.size() ? a.parts.size() : b.parts.size();

			for (size_t i = 0; i < shorter; i++)
			{
				bool const aFolder = i + 1 < a.parts.size();
				bool const bFolder = i + 1 < b.parts.size();

				if (aFolder != bFolder)
				{
					return !aFolder;
				}

				int const order = CompareInCapitals(a.parts[i], b.parts[i]);

				if (order != 0)
				{
					return order < 0;
				}
			}

			return a.parts.size() < b.parts.size();
		}
	}

	std::vector<std::string> SoundEdit_WavNamesFromFiles(std::string const &prefix, std::vector<std::string> const &files, size_t most)
	{
		std::string under = Lower(prefix);

		for (char &letter : under)
		{
			letter = letter == '\\' ? '/' : letter;
		}

		if (!under.empty() && under.back() != '/')
		{
			under += '/';
		}

		std::vector<FoundName> found;

		for (std::string const &file : files)
		{
			if (file.size() <= under.size() || Lower(file.substr(0, under.size())) != under)
			{
				continue;
			}

			std::string const rest = file.substr(under.size());

			if (rest.size() >= 256 || rest.size() < 4 || Lower(rest.substr(rest.size() - 4)) != ".wav")
			{
				continue;
			}

			FoundName one;
			std::string part;
			bool good = true;

			for (char const letter : rest)
			{
				if ((unsigned char)letter < 32 || (unsigned char)letter > 126 || strchr("<>:\"|?*\\", letter) != nullptr)
				{
					good = false;
					break;
				}

				if (letter == '/')
				{
					if (part.empty() || part[0] == '.')
					{
						good = false;   // an empty part ("a//b.wav"), or a folder the finder skips (".x", "..")
						break;
					}

					one.parts.push_back(part);
					part.clear();
					continue;
				}

				part += letter;
			}

			if (!good || part.empty() || one.parts.size() > 32)
			{
				continue;
			}

			one.parts.push_back(part);

			for (size_t i = 0; i < one.parts.size(); i++)
			{
				one.name += (i ? "\\" : "") + one.parts[i];
			}

			found.push_back(one);
		}

		std::sort(found.begin(), found.end(), FinderOrder);

		std::vector<std::string> names;

		for (FoundName const &one : found)
		{
			if (names.size() >= most)
			{
				break;
			}

			if (names.empty() || Lower(names.back()) != Lower(one.name))
			{
				names.push_back(one.name);
			}
		}

		return names;
	}

	std::vector<std::string> SoundEdit_MergeNames(std::vector<std::string> const &raw, std::vector<std::string> const &packed, size_t most)
	{
		std::vector<std::string> names;

		for (std::vector<std::string> const *list : { &raw, &packed })
		{
			for (std::string const &name : *list)
			{
				bool there = false;

				for (std::string const &have : names)
				{
					there = there || _stricmp(have.c_str(), name.c_str()) == 0;
				}

				if (!there && names.size() < most)
				{
					names.push_back(name);
				}
			}
		}

		return names;
	}
}
