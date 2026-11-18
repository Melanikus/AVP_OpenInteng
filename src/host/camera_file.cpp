// camera_file.cpp: see camera_file.h. Written 2026-11-16 by Melanikus (Dietrich) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "chunk.hpp"
#include "mishchnk.hpp"
#include "environment_chunk.hpp"

#include <windows.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <vector>

#include "camera_file.h"


namespace host
{
	namespace
	{
		int32_t Le(uint8_t const *at)
		{
			return (int32_t)((uint32_t)at[0] | (uint32_t)at[1] << 8 | (uint32_t)at[2] << 16 | (uint32_t)at[3] << 24);
		}

		void PutLe(std::vector<uint8_t> *bytes, int32_t value)
		{
			for (int i = 0; i < 4; i++)
			{
				bytes->push_back((uint8_t)((uint32_t)value >> (8 * i)));
			}
		}

		// The file's place (inteng's 1/100 mm, the offset added) as the editor's millimetres: the nearest, halves away from 0. Any 32-bit number plus any offset fits.
		int32_t FileToMm(int64_t inteng)
		{
			return (int32_t)(inteng >= 0 ? (inteng + 50) / 100 : -((-inteng + 50) / 100));
		}

		// The editor's millimetres as the file's number (x 100, the offset taken off), held to what 32 bits hold (never wrapped round to the other side)
		int32_t MmToFile(int32_t mm, int32_t offset)
		{
			int64_t const inteng = (int64_t)mm * 100 - offset;

			return inteng > INT32_MAX ? INT32_MAX : inteng < INT32_MIN ? INT32_MIN : (int32_t)inteng;
		}

		// The file's k-th matrix number is inteng's view matrix (+0x38), the game's VDB_Mat TURNED OVER (FUN_004c19a0, FUN_004b3710); the slot keeps the game's (camera_file.h)
		int SlotPlaceOfFileNumber(int k)
		{
			return (k % 3) * 3 + k / 3;
		}
	}

	bool CameraFile_Name(File_Chunk *level, std::string *name)
	{
		if (!level)
		{
			return false;
		}

		Chunk_With_Children *const envd = (Chunk_With_Children *)level->lookup_single_child("REBENVDT");
		RIF_Name_Chunk *const nameChunk = envd ? (RIF_Name_Chunk *)envd->lookup_single_child("RIFFNAME") : nullptr;

		if (!nameChunk || !nameChunk->rif_name)
		{
			return false;
		}

		// the last part of the name, kept to letters, digits and . - _ (the name is the file's own, hostile input: it never names a folder)
		std::string last;

		for (char const *p = nameChunk->rif_name; *p && last.size() < 4096; p++)
		{
			if (*p == '/' || *p == '\\')
			{
				last.clear();
			}
			else
			{
				last += *p;
			}
		}

		std::string clean;

		for (char letter : last)
		{
			bool const plain = (letter >= 'a' && letter <= 'z') || (letter >= 'A' && letter <= 'Z') || (letter >= '0' && letter <= '9') || letter == '.' || letter == '-' || letter == '_';

			clean += plain ? letter : '_';
		}

		if (clean.size() > 100)
		{
			clean.resize(100);
		}

		if (clean.empty() || clean == "." || clean == "..")
		{
			return false;
		}

		*name = clean + ".cam";
		return true;
	}

	void CameraFile_Offset(File_Chunk *level, int32_t offset[3])
	{
		offset[0] = offset[1] = offset[2] = 0;

		Chunk_With_Children *const envd = level ? (Chunk_With_Children *)level->lookup_single_child("REBENVDT") : nullptr;

		if (!envd)
		{
			return;
		}

		List<Chunk *> special;

		envd->lookup_child("SPECLOBJ", special);

		Chunk_With_Children *firstSpecial = nullptr;

		for (LIF<Chunk *> each(&special); !each.done() && !firstSpecial; each.next())
		{
			firstSpecial = dynamic_cast<Chunk_With_Children *>(each());
		}

		if (!firstSpecial)
		{
			return;
		}

		List<Chunk *> origins;

		firstSpecial->lookup_child("CAMORIGN", origins);

		Miscellaneous_Chunk *origin = nullptr;

		for (LIF<Chunk *> each(&origins); !each.done() && !origin; each.next())
		{
			origin = dynamic_cast<Miscellaneous_Chunk *>(each());
		}

		if (!origin || origin->data_size < 24 || !origin->data)
		{
			return;
		}

		double place[3];

		memcpy(place, origin->data, sizeof(place));

		// the level's scale, as ENVSDSCL gives it, times 100.0; 100.0 when the level has none
		Environment_Scale_Chunk const *const scaleChunk = dynamic_cast<Environment_Scale_Chunk const *>(envd->lookup_single_child("ENVSDSCL"));
		double const scale = scaleChunk ? scaleChunk->scale * 100.0 : 100.0;

		for (int i = 0; i < 3; i++)
		{
			double const moved = scale * place[i];

			// __ftol cuts toward zero; a place that is not a number or too far for a whole number is 0 (never undefined)
			offset[i] = (moved > -2147483000.0 && moved < 2147483000.0) ? (int32_t)moved : 0;
		}
	}

	bool CameraFile_Read(std::string const &path, int32_t const offset[3], CameraFileSlot slots[kCameraFileSlots])
	{
		HANDLE const file = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

		if (file == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		LARGE_INTEGER size;
		std::vector<uint8_t> bytes(kCameraFileBytes);
		DWORD got = 0;
		bool const whole = GetFileSizeEx(file, &size) && size.QuadPart == (LONGLONG)kCameraFileBytes && ReadFile(file, bytes.data(), (DWORD)kCameraFileBytes, &got, NULL) && got == kCameraFileBytes;

		CloseHandle(file);

		if (!whole)
		{
			return false;
		}

		for (size_t s = 0; s < kCameraFileSlots; s++)
		{
			CameraFileSlot slot;
			size_t const placeAt = s < 10 ? s * 12 : 0x78 + 0x168;
			size_t const matrixAt = s < 10 ? 0x78 + s * 36 : 0x78 + 0x168 + 0xc;

			for (int i = 0; i < 3; i++)
			{
				slot.place[i] = FileToMm((int64_t)Le(&bytes[placeAt + i * 4]) + offset[i]);
			}

			for (int k = 0; k < 9; k++)
			{
				slot.matrix[SlotPlaceOfFileNumber(k)] = Le(&bytes[matrixAt + k * 4]);
			}

			slots[s] = slot;
		}

		return true;
	}

	bool CameraFile_Write(std::string const &folder, std::string const &name, int32_t const offset[3], CameraFileSlot const slots[kCameraFileSlots])
	{
		std::vector<uint8_t> bytes;

		// the places of slots 0 to 9 (in inteng's 1/100 mm, offset taken off), their turns (inteng's form: turned over), then slot 10's place and its turn
		for (size_t s = 0; s < 10; s++)
		{
			for (int i = 0; i < 3; i++)
			{
				PutLe(&bytes, MmToFile(slots[s].place[i], offset[i]));
			}
		}

		for (size_t s = 0; s < 10; s++)
		{
			for (int k = 0; k < 9; k++)
			{
				PutLe(&bytes, slots[s].matrix[SlotPlaceOfFileNumber(k)]);
			}
		}

		for (int i = 0; i < 3; i++)
		{
			PutLe(&bytes, MmToFile(slots[10].place[i], offset[i]));
		}

		for (int k = 0; k < 9; k++)
		{
			PutLe(&bytes, slots[10].matrix[SlotPlaceOfFileNumber(k)]);
		}

		if (bytes.size() != kCameraFileBytes)
		{
			return false;
		}

		CreateDirectoryA(folder.c_str(), NULL);

		HANDLE const file = CreateFileA((folder + name).c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

		if (file == INVALID_HANDLE_VALUE)
		{
			return false;
		}

		DWORD put = 0;
		bool const ok = WriteFile(file, bytes.data(), (DWORD)bytes.size(), &put, NULL) && put == bytes.size();

		CloseHandle(file);
		return ok;
	}
}
