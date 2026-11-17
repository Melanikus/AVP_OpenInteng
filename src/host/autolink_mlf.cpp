// See autolink_mlf.h. Written 2026-11-15 by Melanikus (Drake) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "autolink_mlf.h"

#include <string.h>

#include <windows.h>

#include <string>
#include <vector>

namespace host
{
	namespace
	{
		// A matrix number no turn can have: a 16.16 whole number of a rotation is at most 1.0 (0x10000); some room is left for a scaled one
		int32_t const kMostTurnNumber = 0x20000;

		int32_t ReadInt(uint8_t const *at)
		{
			int32_t value;

			memcpy(&value, at, 4);
			return value;
		}

		bool Refuse(std::string *why, size_t record, char const *words)
		{
			if (why)
			{
				*why = "record " + std::to_string(record) + ": " + words;
			}

			return false;
		}
	}

	std::string MlfPathFor(char const *openedPath)
	{
		if (!openedPath || !openedPath[0])
		{
			return std::string();
		}

		std::string path = openedPath;
		size_t const folderEnd = path.find_last_of("\\/");
		size_t const dot = path.find_last_of('.');

		if (dot != std::string::npos && (folderEnd == std::string::npos || dot > folderEnd))
		{
			path.erase(dot);
		}

		return path + ".mlf";
	}

	bool MlfReadFile(char const *path, std::vector<uint8_t> *bytes, std::string *why)
	{
		bytes->clear();

		if (!path || !path[0])
		{
			if (why)
			{
				*why = "no level file name to take the .mlf from";
			}

			return false;
		}

		HANDLE const file = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (file == INVALID_HANDLE_VALUE)
		{
			if (why)
			{
				*why = "there is no .mlf file next to the level (" + std::string(path) + ")";
			}

			return false;
		}

		LARGE_INTEGER size;
		bool good = GetFileSizeEx(file, &size) != 0;

		if (good && size.QuadPart <= 0)
		{
			if (why)
			{
				*why = "the .mlf file is empty";
			}

			CloseHandle(file);
			return false;
		}

		if (good && (unsigned long long)size.QuadPart > kMlfMostBytes)
		{
			if (why)
			{
				*why = "the .mlf file is bigger than " + std::to_string(kMlfMostBytes) + " bytes";
			}

			CloseHandle(file);
			return false;
		}

		if (good)
		{
			bytes->assign((size_t)size.QuadPart, 0);

			DWORD got = 0;

			good = ReadFile(file, bytes->data(), (DWORD)bytes->size(), &got, NULL) && got == bytes->size();
		}

		CloseHandle(file);

		if (!good)
		{
			bytes->clear();

			if (why)
			{
				*why = "the .mlf file could not be read whole";
			}
		}

		return good;
	}

	bool MlfParse(uint8_t const *bytes, size_t size, std::vector<MlfRecord> *records, std::string *why)
	{
		records->clear();

		if (!bytes || size == 0)
		{
			if (why)
			{
				*why = "the .mlf file is empty";
			}

			return false;
		}

		if (size > kMlfMostBytes)
		{
			if (why)
			{
				*why = "the .mlf file is bigger than " + std::to_string(kMlfMostBytes) + " bytes";
			}

			return false;
		}

		size_t at = 0;

		while (at < size)
		{
			size_t const number = records->size() + 1;

			if (records->size() >= kMlfMostRecords)
			{
				records->clear();
				return Refuse(why, number, "more records than the editor takes");
			}

			// the name: plain text up to a NUL, at most 63 bytes, padded to a multiple of 4 (including the NUL)
			size_t length = 0;

			while (at + length < size && bytes[at + length] != 0)
			{
				if (length >= kMlfMostName)
				{
					records->clear();
					return Refuse(why, number, "the module's name is longer than 63 bytes or has no end");
				}

				if (bytes[at + length] < 0x20 || bytes[at + length] > 0x7e)
				{
					records->clear();
					return Refuse(why, number, "the module's name has a letter that is not plain text");
				}

				length++;
			}

			if (at + length >= size)
			{
				records->clear();
				return Refuse(why, number, "the module's name has no end");
			}

			if (length == 0)
			{
				records->clear();
				return Refuse(why, number, "the module's name is empty");
			}

			size_t const padded = (length + 1 + 3) & ~(size_t)3;
			size_t const numbers = 15 * 4;

			if (padded > size - at || numbers > size - at - padded)
			{
				records->clear();
				return Refuse(why, number, "the record is cut short");
			}

			MlfRecord record;

			record.name.assign((char const *)bytes + at, length);

			uint8_t const *n = bytes + at + padded;

			for (int i = 0; i < 3; i++)
			{
				record.place[i] = ReadInt(n + 4 * i);
			}

			for (int i = 0; i < 9; i++)
			{
				record.turn[i] = ReadInt(n + 12 + 4 * i);

				if (record.turn[i] > kMostTurnNumber || record.turn[i] < -kMostTurnNumber)
				{
					records->clear();
					return Refuse(why, number, "a number of the view's matrix is more than a turn can have");
				}
			}

			for (int i = 0; i < 3; i++)
			{
				record.view[i] = ReadInt(n + 48 + 4 * i);
			}

			records->push_back(record);
			at += padded + numbers;
		}

		return true;
	}

	bool MlfFindModule(AutoLinkScene const &scene, MlfRecord const &record, int32_t *object, bool *moved)
	{
		if (object)
		{
			*object = -1;
		}

		if (moved)
		{
			*moved = false;
		}

		for (AutoLinkModule const &entry : scene.list)
		{
			std::map<int32_t, std::string>::const_iterator const name = scene.names.find(entry.number);

			if (name == scene.names.end() || name->second.size() != record.name.size() || _strnicmp(name->second.c_str(), record.name.c_str(), record.name.size()) != 0)
			{
				continue;
			}

			// the first module of that name decides (0x4c8e5a): within 1 unit on each axis, as the level's file numbers or as memory units
			std::map<int32_t, std::vector<int32_t> >::const_iterator const file = scene.fileLocations.find(entry.number);
			bool inMemory = true, inFile = file != scene.fileLocations.end() && file->second.size() >= 3;

			for (int axis = 0; axis < 3; axis++)
			{
				int64_t const a = (int64_t)record.place[axis] - entry.module.place[axis];

				inMemory = inMemory && a >= -1 && a <= 1;

				if (inFile)
				{
					int64_t const b = (int64_t)record.place[axis] - file->second[(size_t)axis];

					inFile = b >= -1 && b <= 1;
				}
			}

			if (inMemory || inFile)
			{
				if (object)
				{
					*object = entry.number;
				}
			}
			else if (moved)
			{
				*moved = true;
			}

			return true;
		}

		return false;
	}

	bool MlfView(MlfRecord const &record, AutoLinkView *view)
	{
		if (!view)
		{
			return false;
		}

		for (int axis = 0; axis < 3; axis++)
		{
			int64_t const value = (int64_t)record.view[axis] * 100;

			if (value < INT32_MIN || value > INT32_MAX)
			{
				return false;
			}

			view->place[axis] = (int32_t)value;
		}

		for (int r = 0; r < 3; r++)
		{
			for (int c = 0; c < 3; c++)
			{
				view->turn[r * 3 + c] = record.turn[c * 3 + r];
			}
		}

		view->step = 0;
		return true;
	}
}
