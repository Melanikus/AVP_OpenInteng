// See virtual_object.h. Written 2026-11-15 by Melanikus (Frost) for AVP_OpenInteng. AVP_OpenInteng's own code: see License.txt, "AVP_OpenInteng's own code".

#include "virtual_object.h"

#include <math.h>
#include <string.h>

#include <exception>
#include <string>
#include <vector>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk
#include "chunk_shape.hpp"    // Shape_Chunk
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "md5.h"
#include "field_model.h"
#include "level_file.h"
#include "level_image.h"

namespace host
{
	namespace
	{
		using namespace image;

		size_t const kVobjpropFixed = 12 + 4;   // location (3 numbers) and the size, before the name

		// The ID a VOBJPROP holds, from its payload; false when the chunk is too short
		bool IdOfProp(Bytes const &bytes, Span const &chunk, uint8_t *id)
		{
			size_t const payload = chunk.at + kHeadingBytes;
			size_t const length = chunk.size - kHeadingBytes;

			if (length < kVobjpropFixed)
			{
				return false;
			}

			char const *const zero = (char const *)memchr(bytes.data() + payload + kVobjpropFixed, 0, length - kVobjpropFixed);

			if (!zero)
			{
				return false;
			}

			size_t const nameBytes = (size_t)(zero - (bytes.data() + payload + kVobjpropFixed));
			size_t const at = kVobjpropFixed + ((nameBytes + 4) & ~(size_t)3);

			if (at + 8 > length)
			{
				return false;
			}

			memcpy(id, bytes.data() + payload + at, 8);
			return true;
		}
	}

	char const *VirtualObjectWords(VirtualObjectResult result)
	{
		switch (result)
		{
			case VirtualObjectResult::Done: return "done";
			case VirtualObjectResult::BadCall: return "no level to work on";
			case VirtualObjectResult::BadName: return "a name is 1 to 60 plain letters, digits, spaces, dots, dashes or underscores";
			case VirtualObjectResult::BadKind: return "no such kind of strategy";
			case VirtualObjectResult::NotFound: return "no virtual object has that ID";
			case VirtualObjectResult::NameUsed: return "Please try another name";   // inteng's words (0x5f0298)
			case VirtualObjectResult::NoEnvironment: return "the level has no environment data";
			case VirtualObjectResult::CantBuild: return "the level couldn't be put together from its bytes";
			case VirtualObjectResult::LoaderRefused: return "the game's own loader wouldn't take the changed level, so nothing was changed";
		}

		return "unknown answer";
	}

	void VirtualObject_Id(char const *name, uint8_t *id)
	{
		std::string const text = std::string("VirtOb") + (name ? name : "");
		char digest[16];

		md5_buffer(text.c_str(), (unsigned int)text.size(), digest);
		memcpy(id, digest, 8);
		id[7] = 0;
	}

	bool VirtualObject_Moved(int32_t command, int64_t const objectPlace[3], int64_t const min[3], int64_t const max[3], int64_t out[3])
	{
		for (int axis = 0; axis < 3; axis++)
		{
			out[axis] = objectPlace[axis];
		}

		int64_t const away = kVirtualObjectOffset;

		switch (command)
		{
			case 0x3a: out[1] = objectPlace[1] + min[1] - away; return true;
			case 0x3b: out[1] = objectPlace[1] + max[1] + away; return true;
			case 0x3c: out[0] = objectPlace[0] - away + min[0]; return true;
			case 0x3d: out[0] = objectPlace[0] + away + max[0]; return true;
			case 0x3e: out[2] = objectPlace[2] + away + max[2]; return true;
			case 0x3f: out[2] = objectPlace[2] - away + min[2]; return true;
			default: return false;
		}
	}

	namespace
	{
		// The SPECLOBJ of the image and its children; false when the level can't be walked. *specAt is top.size() when there is no REBENVDT or no SPECLOBJ.
		bool SpecialOf(Bytes const &image, std::vector<Span> const &top, size_t *envAt, std::vector<Span> *inside, size_t *specAt, std::vector<Span> *special)
		{
			*envAt = top.size();
			*specAt = 0;
			inside->clear();
			special->clear();

			for (size_t i = 0; i < top.size() && *envAt == top.size(); i++)
			{
				if (NameIs(image, top[i].at, "REBENVDT"))
				{
					*envAt = i;
				}
			}

			if (*envAt == top.size())
			{
				return true;
			}

			if (!Children(image, top[*envAt].at + kHeadingBytes, top[*envAt].at + top[*envAt].size, inside))
			{
				return false;
			}

			*specAt = FirstNamed(image, *inside, "SPECLOBJ");

			if (*specAt == inside->size())
			{
				return true;
			}

			return Children(image, (*inside)[*specAt].at + kHeadingBytes, (*inside)[*specAt].at + (*inside)[*specAt].size, special);
		}

		// What a VOBJPROP holds: false when it is too short
		bool ReadProp(Bytes const &image, Span const &chunk, VirtualObjectInfo *info)
		{
			size_t const payload = chunk.at + kHeadingBytes;
			size_t const length = chunk.size - kHeadingBytes;

			if (length < kVobjpropFixed || !IdOfProp(image, chunk, info->id))
			{
				return false;
			}

			info->place[0] = IntAt(image, payload);
			info->place[1] = IntAt(image, payload + 4);
			info->place[2] = IntAt(image, payload + 8);
			info->size = IntAt(image, payload + 12);

			char const *const text = (char const *)image.data() + payload + kVobjpropFixed;
			size_t shown = 0;

			while (shown < length - kVobjpropFixed && shown < 200 && text[shown] != 0)
			{
				shown++;
			}

			info->name.assign(text, shown);
			return true;
		}
	}

	bool VirtualObject_List(OpenLevelFile *file, std::vector<VirtualObjectInfo> *list)
	{
		list->clear();

		if (!file || !file->Level())
		{
			return false;
		}

		Bytes image;
		std::vector<Span> top, inside, special;
		size_t envAt = 0, specAt = 0;

		if (!Walk(file, &image, &top) || !SpecialOf(image, top, &envAt, &inside, &specAt, &special))
		{
			return false;
		}

		for (Span const &one : special)
		{
			std::vector<Span> parts;

			if (!NameIs(image, one.at, "VIOBJECT") || list->size() >= kVirtualObjectMost || !Children(image, one.at + kHeadingBytes, one.at + one.size, &parts))
			{
				continue;
			}

			size_t const propAt = FirstNamed(image, parts, "VOBJPROP");
			VirtualObjectInfo info;

			if (propAt != parts.size() && ReadProp(image, parts[propAt], &info))
			{
				info.hasStrategy = FirstNamed(image, parts, "AVPSTRAT") != parts.size();
				list->push_back(info);
			}
		}

		return true;
	}

	VirtualObjectResult VirtualObject_Destroy(OpenLevelFile *file, uint8_t const *id)
	{
		if (!file || !file->Level() || !id)
		{
			return VirtualObjectResult::BadCall;
		}

		Bytes image;
		std::vector<Span> top, inside, special;
		size_t envAt = 0, specAt = 0;

		if (!Walk(file, &image, &top) || !SpecialOf(image, top, &envAt, &inside, &specAt, &special))
		{
			return VirtualObjectResult::CantBuild;
		}

		size_t goes = special.size();

		for (size_t i = 0; i < special.size() && goes == special.size(); i++)
		{
			std::vector<Span> parts;

			if (!NameIs(image, special[i].at, "VIOBJECT") || !Children(image, special[i].at + kHeadingBytes, special[i].at + special[i].size, &parts))
			{
				continue;
			}

			size_t const propAt = FirstNamed(image, parts, "VOBJPROP");
			uint8_t held[8];

			if (propAt != parts.size() && IdOfProp(image, parts[propAt], held) && memcmp(held, id, 8) == 0)
			{
				goes = i;
			}
		}

		if (goes == special.size())
		{
			return VirtualObjectResult::NotFound;
		}

		// SPECLOBJ without it, REBENVDT with that SPECLOBJ, the level with that REBENVDT; every size right
		Bytes spec(image.begin() + (std::ptrdiff_t)inside[specAt].at, image.begin() + (std::ptrdiff_t)(inside[specAt].at + kHeadingBytes));

		for (size_t i = 0; i < special.size(); i++)
		{
			if (i != goes)
			{
				AddCopy(&spec, image, special[i]);
			}
		}

		PutIntAt(&spec, 8, (int32_t)spec.size());

		Span const env = top[envAt];
		Bytes newEnv(image.begin() + (std::ptrdiff_t)env.at, image.begin() + (std::ptrdiff_t)(env.at + kHeadingBytes));

		for (size_t i = 0; i < inside.size(); i++)
		{
			if (i == specAt)
			{
				newEnv.insert(newEnv.end(), spec.begin(), spec.end());
			}
			else
			{
				AddCopy(&newEnv, image, inside[i]);
			}
		}

		PutIntAt(&newEnv, 8, (int32_t)newEnv.size());

		Bytes out(image.begin(), image.begin() + (std::ptrdiff_t)env.at);

		out.insert(out.end(), newEnv.begin(), newEnv.end());
		out.insert(out.end(), image.begin() + (std::ptrdiff_t)(env.at + env.size), image.end());
		PutIntAt(&out, 8, (int32_t)out.size());

		LevelOpenResult const result = file->ReplaceLevel(out);

		if (result == LevelOpenResult::Opened)
		{
			return VirtualObjectResult::Done;
		}

		return result == LevelOpenResult::CantRead ? VirtualObjectResult::CantBuild : VirtualObjectResult::LoaderRefused;
	}

	VirtualObjectResult VirtualObject_AssignStrategy(OpenLevelFile *file, uint8_t const *id, int32_t kind)
	{
		if (!file || !file->Level() || !id)
		{
			return VirtualObjectResult::BadCall;
		}

		uint8_t payload[1024];
		size_t length = 0;

		if (!model::MakeNewStrategy(kind, payload, sizeof(payload), &length))
		{
			return VirtualObjectResult::BadKind;
		}

		Bytes image;
		std::vector<Span> top, inside, special;
		size_t envAt = 0, specAt = 0;

		if (!Walk(file, &image, &top) || !SpecialOf(image, top, &envAt, &inside, &specAt, &special))
		{
			return VirtualObjectResult::CantBuild;
		}

		size_t goes = special.size();
		std::vector<Span> parts;

		for (size_t i = 0; i < special.size() && goes == special.size(); i++)
		{
			std::vector<Span> mine;

			if (!NameIs(image, special[i].at, "VIOBJECT") || !Children(image, special[i].at + kHeadingBytes, special[i].at + special[i].size, &mine))
			{
				continue;
			}

			size_t const propAt = FirstNamed(image, mine, "VOBJPROP");
			uint8_t held[8];

			if (propAt != mine.size() && IdOfProp(image, mine[propAt], held) && memcmp(held, id, 8) == 0)
			{
				goes = i;
				parts = mine;
			}
		}

		if (goes == special.size())
		{
			return VirtualObjectResult::NotFound;
		}

		// the VIOBJECT made again: its children but an old AVPSTRAT, then the new one
		Bytes viobject(image.begin() + (std::ptrdiff_t)special[goes].at, image.begin() + (std::ptrdiff_t)(special[goes].at + kHeadingBytes));

		for (Span const &part : parts)
		{
			if (!NameIs(image, part.at, "AVPSTRAT"))
			{
				AddCopy(&viobject, image, part);
			}
		}

		Bytes const strategy = MakeChunk("AVPSTRAT", Bytes((char const *)payload, (char const *)payload + length));

		viobject.insert(viobject.end(), strategy.begin(), strategy.end());
		PutIntAt(&viobject, 8, (int32_t)viobject.size());

		Bytes spec(image.begin() + (std::ptrdiff_t)inside[specAt].at, image.begin() + (std::ptrdiff_t)(inside[specAt].at + kHeadingBytes));

		for (size_t i = 0; i < special.size(); i++)
		{
			if (i == goes)
			{
				spec.insert(spec.end(), viobject.begin(), viobject.end());
			}
			else
			{
				AddCopy(&spec, image, special[i]);
			}
		}

		PutIntAt(&spec, 8, (int32_t)spec.size());

		Span const env = top[envAt];
		Bytes newEnv(image.begin() + (std::ptrdiff_t)env.at, image.begin() + (std::ptrdiff_t)(env.at + kHeadingBytes));

		for (size_t i = 0; i < inside.size(); i++)
		{
			if (i == specAt)
			{
				newEnv.insert(newEnv.end(), spec.begin(), spec.end());
			}
			else
			{
				AddCopy(&newEnv, image, inside[i]);
			}
		}

		PutIntAt(&newEnv, 8, (int32_t)newEnv.size());

		Bytes out(image.begin(), image.begin() + (std::ptrdiff_t)env.at);

		out.insert(out.end(), newEnv.begin(), newEnv.end());
		out.insert(out.end(), image.begin() + (std::ptrdiff_t)(env.at + env.size), image.end());
		PutIntAt(&out, 8, (int32_t)out.size());

		LevelOpenResult const result = file->ReplaceLevel(out);

		if (result == LevelOpenResult::Opened)
		{
			return VirtualObjectResult::Done;
		}

		return result == LevelOpenResult::CantRead ? VirtualObjectResult::CantBuild : VirtualObjectResult::LoaderRefused;
	}

	bool VirtualObject_Geometry(File_Chunk *level, uint8_t const *id, int64_t place[3], int64_t min[3], int64_t max[3])
	{
		if (!level || !id)
		{
			return false;
		}

		try
		{
			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (!object || memcmp(&object->object_data.ID, id, 8) != 0)
				{
					continue;
				}

				Shape_Chunk *const shape = object->get_assoc_shape();

				if (!shape)
				{
					return false;
				}

				place[0] = object->object_data.location.x;
				place[1] = object->object_data.location.y;
				place[2] = object->object_data.location.z;
				min[0] = shape->shape_data.min.x;
				min[1] = shape->shape_data.min.y;
				min[2] = shape->shape_data.min.z;
				max[0] = shape->shape_data.max.x;
				max[1] = shape->shape_data.max.y;
				max[2] = shape->shape_data.max.z;
				return true;
			}
		}
		catch (std::exception const &)
		{
		}

		return false;
	}

	VirtualObjectResult VirtualObject_Create(OpenLevelFile *file, char const *name, int64_t const place[3], double scale)
	{
		if (!file || !file->Level() || !name || !place)
		{
			return VirtualObjectResult::BadCall;
		}

		size_t const length = strlen(name);

		if (length == 0 || length > kVirtualObjectMostName)
		{
			return VirtualObjectResult::BadName;
		}

		for (size_t i = 0; i < length; i++)
		{
			unsigned char const letter = (unsigned char)name[i];
			bool const plain = (letter >= 'A' && letter <= 'Z') || (letter >= 'a' && letter <= 'z') || (letter >= '0' && letter <= '9') || letter == ' ' || letter == '.' || letter == '-' || letter == '_';

			if (!plain)
			{
				return VirtualObjectResult::BadName;
			}
		}

		if (!(scale > 0.0) || !(scale < 1e12))
		{
			return VirtualObjectResult::BadCall;
		}

		uint8_t id[8];

		VirtualObject_Id(name, id);

		// An object that already holds the ID: "Please try another name"
		try
		{
			List<Object_Chunk *> objects;

			file->Level()->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				if (each() && memcmp(&each()->object_data.ID, id, 8) == 0)
				{
					return VirtualObjectResult::NameUsed;
				}
			}
		}
		catch (std::exception const &)
		{
			return VirtualObjectResult::CantBuild;
		}

		Bytes image;
		std::vector<Span> top, inside, specInside;

		if (!Walk(file, &image, &top))
		{
			return VirtualObjectResult::CantBuild;
		}

		size_t envAt = top.size();

		for (size_t i = 0; i < top.size() && envAt == top.size(); i++)
		{
			if (NameIs(image, top[i].at, "REBENVDT"))
			{
				envAt = i;
			}
		}

		if (envAt == top.size())
		{
			return VirtualObjectResult::NoEnvironment;
		}

		if (!Children(image, top[envAt].at + kHeadingBytes, top[envAt].at + top[envAt].size, &inside))
		{
			return VirtualObjectResult::CantBuild;
		}

		size_t const specAt = FirstNamed(image, inside, "SPECLOBJ");
		bool const haveSpec = specAt != inside.size();

		if (haveSpec && !Children(image, inside[specAt].at + kHeadingBytes, inside[specAt].at + inside[specAt].size, &specInside))
		{
			return VirtualObjectResult::CantBuild;
		}

		// the same ID in another virtual object
		for (Span const &one : specInside)
		{
			if (!NameIs(image, one.at, "VIOBJECT"))
			{
				continue;
			}

			std::vector<Span> parts;

			if (!Children(image, one.at + kHeadingBytes, one.at + one.size, &parts))
			{
				return VirtualObjectResult::CantBuild;
			}

			for (Span const &part : parts)
			{
				uint8_t held[8];

				if (NameIs(image, part.at, "VOBJPROP") && IdOfProp(image, part, held) && memcmp(held, id, 8) == 0)
				{
					return VirtualObjectResult::NameUsed;
				}
			}
		}

		// VOBJPROP: location, size, the name padded to 4 with its end, the ID, two spare numbers
		Bytes prop;

		for (int axis = 0; axis < 3; axis++)
		{
			double const file3 = (double)place[axis] / scale;
			double const cut = file3 < 0.0 ? -floor(-file3) : floor(file3);

			AddInt(&prop, cut > 2147483647.0 ? 2147483647 : (cut < -2147483647.0 ? -2147483647 : (int32_t)cut));
		}

		{
			double const size = (double)kVirtualObjectSize / scale;

			AddInt(&prop, size > 2147483647.0 ? 2147483647 : (int32_t)size);
		}

		prop.insert(prop.end(), name, name + length);
		prop.resize(kVobjpropFixed + ((length + 4) & ~(size_t)3), 0);
		prop.insert(prop.end(), id, id + 8);
		AddInt(&prop, 0);
		AddInt(&prop, 0);

		Bytes const propChunk = MakeChunk("VOBJPROP", prop);
		Bytes const viobject = MakeChunk("VIOBJECT", propChunk);

		// SPECLOBJ: its children, then the new one last; made (last in REBENVDT) when missing
		Bytes spec(kHeadingBytes, 0);

		memcpy(spec.data(), "SPECLOBJ", 8);

		for (Span const &child : specInside)
		{
			AddCopy(&spec, image, child);
		}

		spec.insert(spec.end(), viobject.begin(), viobject.end());
		PutIntAt(&spec, 8, (int32_t)spec.size());

		Span const env = top[envAt];
		Bytes newEnv;

		newEnv.insert(newEnv.end(), image.begin() + (std::ptrdiff_t)env.at, image.begin() + (std::ptrdiff_t)(env.at + kHeadingBytes));

		for (size_t i = 0; i < inside.size(); i++)
		{
			if (haveSpec && i == specAt)
			{
				newEnv.insert(newEnv.end(), spec.begin(), spec.end());
			}
			else
			{
				AddCopy(&newEnv, image, inside[i]);
			}
		}

		if (!haveSpec)
		{
			newEnv.insert(newEnv.end(), spec.begin(), spec.end());
		}

		size_t const kLargest = 0x7fffffff;

		if (newEnv.size() > kLargest)
		{
			return VirtualObjectResult::CantBuild;
		}

		PutIntAt(&newEnv, 8, (int32_t)newEnv.size());

		Bytes out(image.begin(), image.begin() + (std::ptrdiff_t)env.at);

		out.insert(out.end(), newEnv.begin(), newEnv.end());
		out.insert(out.end(), image.begin() + (std::ptrdiff_t)(env.at + env.size), image.end());

		if (out.size() > kLargest)
		{
			return VirtualObjectResult::CantBuild;
		}

		PutIntAt(&out, 8, (int32_t)out.size());

		LevelOpenResult const result = file->ReplaceLevel(out);

		if (result == LevelOpenResult::Opened)
		{
			return VirtualObjectResult::Done;
		}

		return result == LevelOpenResult::CantRead ? VirtualObjectResult::CantBuild : VirtualObjectResult::LoaderRefused;
	}
}
