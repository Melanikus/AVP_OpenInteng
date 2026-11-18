// write_c_file.cpp: see write_c_file.h. Written 2026-11-18 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".
#include <windows.h>
#include <string.h>

#include <string>
#include <vector>

#include "write_c_file.h"

#include "bitmap_names.hpp"
#include "chunk.hpp"
#include "chunk_object.hpp"
#include "chunk_shape.hpp"

#include "anim_edit.h"
#include "editor_shared.h"
#include "level_file.h"
#include "level_objects.h"
#include "level_view.h"
#include "logString.h"
#include "poly_edit.h"
#include "save_copy.h"
#include "save_folder.h"
#include "selection.h"
#include "shape_edit.h"
#include "uv_model.h"

namespace
{
	Object_Chunk *FirstWithId(File_Chunk *level, uint8_t const *id)
	{
		List<Object_Chunk *> objects;

		level->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
			{
				return each();
			}
		}

		return nullptr;
	}

	Object_Chunk *FirstNamed(File_Chunk *level, std::string const &name)
	{
		List<Object_Chunk *> objects;

		level->list_objects(&objects);

		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (object && std::string(object->object_data.o_name, strnlen(object->object_data.o_name, sizeof(object->object_data.o_name))) == name)
			{
				return object;
			}
		}

		return nullptr;
	}
}

namespace host
{
	bool WriteC_Source(OpenLevelFile *open, uint8_t const id[8], std::string const &name, model::CShapeSource *out, std::string *why)
	{
		*out = model::CShapeSource();
		out->name = name;

		File_Chunk *const level = open ? open->Level() : nullptr;
		Object_Chunk *const object = level ? FirstWithId(level, id) : nullptr;
		Shape_Chunk *const shape = object ? object->get_assoc_shape() : nullptr;

		if (!shape || ShapeEdit_Read(level, open, id, &out->shape, nullptr, why) != ShapeEditResult::Done)
		{
			if (why && why->empty())
			{
				*why = "the object has no shape the editor can read";
			}

			return false;
		}

		// SHPUVCRD: the lists in order (none when the shape has no chunk)
		if (Chunk *const uvChunk = shape->lookup_single_child("SHPUVCRD"))
		{
			std::vector<unsigned char> bytes;

			if (!open->PayloadOf(uvChunk, &bytes) || model::ReadUvLists(bytes.data(), bytes.size(), &out->uvLists) != model::UvModelResult::Ok)
			{
				*why = "the shape's lists of texture coordinates could not be read";
				return false;
			}
		}

		// TEXTANIM (FUN_00503da0 0x503e48: the shape's first)
		int32_t shapeNumber = -1;

		if (AnimEdit_Read(open, id, &shapeNumber, &out->anim) != AnimEditResult::Done)
		{
			out->anim = model::TextureAnim();
		}

		// the objects using the shape (FUN_0050e9f0: SHPHEAD1's list), each with its place
		std::vector<std::string> users;

		if (PolyEdit_ShapeUserNames(open, id, &users))
		{
			for (std::string const &user : users)
			{
				Object_Chunk *const one = FirstNamed(level, user);
				int32_t place[3] = { 0, 0, 0 };
				uint8_t userId[8];

				if (one)
				{
					memcpy(userId, &one->object_data.ID, 8);
					LevelObjects_PlaceOf(userId, place);
				}

				out->users.push_back({ user, place[0], place[1], place[2] });
			}
		}

		// REBENVDT (looked up on the level, 0x503eb1): ENVSDSCL's double (its working bytes: a field edit is in them) and BMPNAMES' names
		if (Chunk_With_Children *const environment = (Chunk_With_Children *)level->lookup_single_child("REBENVDT"))
		{
			if (Chunk *const scaleChunk = environment->lookup_single_child("ENVSDSCL"))
			{
				std::vector<unsigned char> bytes;
				double scale = 1.0;

				if (open->PayloadOf(scaleChunk, &bytes) && bytes.size() >= sizeof(scale))
				{
					memcpy(&scale, bytes.data(), sizeof(scale));
					out->scale = scale;   // the game's loader took the level, so it is a real number (avpx environment_chunk.cpp)
				}
			}

			if (Chunk_With_BMPs *const names = dynamic_cast<Chunk_With_BMPs *>(environment->lookup_single_child("BMPNAMES")))
			{
				for (LIF<BMP_Name> each(&names->bmps); !each.done(); each.next())
				{
					out->pictures.push_back({ each().index, each().filename ? std::string(each().filename) : std::string() });
				}
			}
		}

		return true;
	}
}

bool WriteC_TheObject(uint8_t id[8])
{
	// DAT_00631234 == 1 (0x445870): exactly one object in the set
	return host::Selection_SetCount() == 1 && host::Selection_SetMember(0, id);
}

// After Ok with a name (0x4458eb-0x44595f): the file written, or why not
void WriteTheC(std::string const &typed)
{
	OpenLevelFile *const open = LevelObjects_OpenForEdit();
	uint8_t id[8];
	char opened[MAX_PATH] = "";

	if (!open || !open->Level() || !LevelView_FilePath(opened, sizeof(opened)))
	{
		Answer("Write C File: no level is open");
		return;
	}

	if (!WriteC_TheObject(id))
	{
		Answer("Write C File: select one object");   // the selection changed while the question was open
		return;
	}

	model::CShapeSource source;
	std::string why;

	if (!host::WriteC_Source(open, id, typed, &source, &why))
	{
		Answer("Write C File: not written: " + why);
		return;
	}

	std::string path;
	std::string folder;
	char const *placeWhy = nullptr;

	if (!host::SaveExportPlace(opened, typed.c_str(), ".c", theCopiesFolder, theGameFolder, &path, &folder, &placeWhy))
	{
		Answer(std::string("Write C File: not written: ") + (placeWhy ? placeWhy : "no place for the file"));
		return;
	}

	std::string const baseNotUsable = MakeTheCopiesFolder();

	if (!baseNotUsable.empty())
	{
		Answer("Write C File: the folder for copies can't be used: " + baseNotUsable);
		return;
	}

	if (!CreateDirectoryA(folder.c_str(), NULL) && GetLastError() != ERROR_ALREADY_EXISTS)
	{
		Answer("Write C File: the editor couldn't make its folder for exports");
		return;
	}

	char checked[MAX_PATH];

	if (char const *const notUsable = SaveFolder_WhyNotUsable(folder.c_str(), checked, sizeof(checked)))
	{
		Answer(std::string("Write C File: the folder for exports can't be used: ") + notUsable);
		return;
	}

	// a file already there is the editor's own earlier export, replaced; a link or a folder is never written through
	DWORD const there = GetFileAttributesA(path.c_str());

	if (there != INVALID_FILE_ATTRIBUTES && (there & (FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_DIRECTORY)))
	{
		Answer("Write C File: not written: the name is a folder or a link");
		return;
	}

	std::string const text = model::CShapeText(source);

	// inteng's CreateFileA(name, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, FILE_FLAG_RANDOM_ACCESS) (0x503df3), then a WriteFile a line
	HANDLE const file = CreateFileA(path.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	if (file == INVALID_HANDLE_VALUE)
	{
		Answer("Write C File: the file couldn't be made");
		return;
	}

	DWORD written = 0;
	bool const whole = WriteFile(file, text.data(), (DWORD)text.size(), &written, NULL) && written == (DWORD)text.size();

	CloseHandle(file);

	if (!whole)
	{
		Answer("Write C File: the file couldn't be written whole: " + path);
		return;
	}

	savedWords = "Written " + path + " (the shape of one object: " + std::to_string(source.shape.points.size()) + " points, " +
		std::to_string(source.shape.polygons.size()) + " polygons)";
	statusWords = savedWords.c_str();
	LogString("write c: " + path);
}
