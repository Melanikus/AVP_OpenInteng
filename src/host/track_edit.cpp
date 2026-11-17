// See track_edit.h. Written 2026-11-15 by Melanikus (Dietrich) for AVP_OpenInteng (P10). AVP_OpenInteng's own code: see License.txt,
// "AVP_OpenInteng's own code".

#include "track_edit.h"

#include <string.h>

#include <exception>
#include <string>
#include <vector>

#include "chunk.hpp"
#include "chunk_object.hpp"   // Object_Chunk (object_data)
#include "mishchnk.hpp"       // File_Chunk::list_objects
#include "environment_chunk.hpp"   // Sound_Directory_Chunk

#include "level_file.h"
#include "track_sections.h"

namespace host
{
	namespace
	{
		typedef std::vector<char> Bytes;

		size_t const HeadingBytes = 12;   // a chunk: its 8-letter name, then its size (which counts these 12 bytes too)
		size_t const NameAt = 60;         // in an object header's payload: the name starts here
		size_t const MostNameBytes = 63;

		struct Piece
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

		bool ChunkSizeAt(Bytes const &bytes, size_t at, size_t end, size_t *size)
		{
			if (end > bytes.size() || at > end || end - at < HeadingBytes)
			{
				return false;
			}

			int32_t const stored = IntAt(bytes, at + 8);

			if (stored < (int32_t)HeadingBytes || (size_t)stored > end - at)
			{
				return false;
			}

			*size = (size_t)stored;
			return true;
		}

		bool NameIs(Bytes const &bytes, size_t at, char const *name)
		{
			return memcmp(bytes.data() + at, name, 8) == 0;
		}

		TrackEditResult FromLoader(LevelOpenResult result)
		{
			switch (result)
			{
			case LevelOpenResult::Opened: return TrackEditResult::Done;
			case LevelOpenResult::NothingOpen: return TrackEditResult::NothingOpen;
			case LevelOpenResult::CantRead: return TrackEditResult::CantBuild;
			default: return TrackEditResult::LoaderRefused;
			}
		}

		bool NameOfObject(OpenLevelFile *file, uint8_t const *id, std::string *name)
		{
			File_Chunk *const level = file->Level();

			if (!level)
			{
				return false;
			}

			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (object && memcmp(&object->object_data.ID, id, 8) == 0)
				{
					size_t length = 0;

					while (length < sizeof(object->object_data.o_name) && object->object_data.o_name[length] != 0)
					{
						length++;
					}

					name->assign(object->object_data.o_name, length);
					return true;
				}
			}

			return false;
		}

		// The object's name from its OBJHEAD1, or false when it has none or it isn't well formed
		bool HeaderName(Bytes const &bytes, Piece const &object, std::string *name)
		{
			for (size_t at = object.at + HeadingBytes; at < object.at + object.size; )
			{
				size_t size = 0;

				if (!ChunkSizeAt(bytes, at, object.at + object.size, &size))
				{
					return false;
				}

				if (NameIs(bytes, at, "OBJHEAD1"))
				{
					size_t const length = size - HeadingBytes;

					if (length < NameAt + 1)
					{
						return false;
					}

					char const *const first = bytes.data() + at + HeadingBytes + NameAt;
					char const *const zero = (char const *)memchr(first, 0, length - NameAt);

					if (!zero || (size_t)(zero - first) > MostNameBytes)
					{
						return false;
					}

					name->assign(first, (size_t)(zero - first));
					return true;
				}

				at += size;
			}

			return false;
		}

		// The level as bytes, and the RBOBJECT of the object with this ID in it
		TrackEditResult Find(OpenLevelFile *file, uint8_t const *id, Bytes *image, Piece *object)
		{
			if (!file || !file->Level())
			{
				return TrackEditResult::NothingOpen;
			}

			bool allZero = true;

			for (size_t i = 0; id && i < 8; i++)
			{
				allZero = allZero && id[i] == 0;
			}

			if (!id || allZero)
			{
				return TrackEditResult::NoName;
			}

			std::string name;

			if (!NameOfObject(file, id, &name))
			{
				return TrackEditResult::NoSuchObject;
			}

			if (!file->BuildLevel(*image) || image->size() < HeadingBytes || !NameIs(*image, 0, "REBINFF2"))
			{
				return TrackEditResult::CantBuild;
			}

			for (size_t at = HeadingBytes; at < image->size(); )
			{
				size_t size = 0;

				if (!ChunkSizeAt(*image, at, image->size(), &size))
				{
					return TrackEditResult::CantBuild;
				}

				if (NameIs(*image, at, "RBOBJECT"))
				{
					Piece const whole = { at, size };
					std::string found;

					if (HeaderName(*image, whole, &found) && found == name)
					{
						*object = whole;
						return TrackEditResult::Done;
					}
				}

				at += size;
			}

			return TrackEditResult::NoSuchObject;
		}

		TrackEditResult Copy(OpenLevelFile *file, uint8_t const *id, TrackClip *clip)
		{
			Bytes image;
			Piece object = { 0, 0 };
			TrackEditResult const found = Find(file, id, &image, &object);

			if (found != TrackEditResult::Done)
			{
				return found;
			}

			TrackClip made;

			for (size_t at = object.at + HeadingBytes; at < object.at + object.size; )
			{
				size_t size = 0;

				if (!ChunkSizeAt(image, at, object.at + object.size, &size))
				{
					return TrackEditResult::CantBuild;
				}

				if (NameIs(image, at, "OBJTRAK2") && made.track.empty())
				{
					made.track.assign(image.begin() + (std::ptrdiff_t)at, image.begin() + (std::ptrdiff_t)(at + size));
				}
				else if (NameIs(image, at, "TRAKSOUN"))
				{
					made.sounds.push_back(Bytes(image.begin() + (std::ptrdiff_t)at, image.begin() + (std::ptrdiff_t)(at + size)));
				}

				at += size;
			}

			if (made.track.empty())
			{
				return TrackEditResult::NoTrack;
			}

			*clip = made;
			return TrackEditResult::Done;
		}

		// inteng's FUN_004f3330 asks first whether the object it pastes on has its level object (the record's +0x154, 0x4f335c-0x4f336f: "Please use Mode->Save and try again" when it has
		// none, a new object not yet saved), then for the clipboard (0x4f33c4, "Use copy track first"). Every object of a level open here is in the level's chunks, so the first refusal
		// can't come (NAMED, track_edit.h): the object only has to be there. It does NOT have to hold a track: its own, if it has one, is taken out (0x4f3431). (Fixed 2026-11-17 by
		// Frost: since 60ae1e5 a track was asked of it, read from 0x4f336f, and a paste onto an object with none was refused: OUR BUG, FIXED.)
		TrackEditResult PasteTargetCheck(OpenLevelFile *file, uint8_t const *id)
		{
			Bytes image;
			Piece object = { 0, 0 };

			return Find(file, id, &image, &object);
		}

		// The object's own track chunks replaced by the clip's (clip null: taken out, nothing put in: Delete Track)
		TrackEditResult Paste(OpenLevelFile *file, uint8_t const *id, TrackClip const *clipOrNull)
		{
			TrackClip const empty;
			TrackClip const &clip = clipOrNull ? *clipOrNull : empty;

			if (clipOrNull && (clip.track.size() < HeadingBytes || memcmp(clip.track.data(), "OBJTRAK2", 8) != 0))
			{
				return TrackEditResult::ClipboardEmpty;
			}

			Bytes image;
			Piece object = { 0, 0 };
			TrackEditResult const found = Find(file, id, &image, &object);

			if (found != TrackEditResult::Done)
			{
				return found;
			}

			// The object's chunks, with its own OBJTRAK2 and every TRAKSOUN left out, then the clipboard's: the track, the sounds in order
			Bytes inside;

			for (size_t at = object.at + HeadingBytes; at < object.at + object.size; )
			{
				size_t size = 0;

				if (!ChunkSizeAt(image, at, object.at + object.size, &size))
				{
					return TrackEditResult::CantBuild;
				}

				if (!NameIs(image, at, "OBJTRAK2") && !NameIs(image, at, "TRAKSOUN"))
				{
					inside.insert(inside.end(), image.begin() + (std::ptrdiff_t)at, image.begin() + (std::ptrdiff_t)(at + size));
				}

				at += size;
			}

			inside.insert(inside.end(), clip.track.begin(), clip.track.end());

			for (Bytes const &sound : clip.sounds)
			{
				inside.insert(inside.end(), sound.begin(), sound.end());
			}

			Bytes made(image.begin() + (std::ptrdiff_t)object.at, image.begin() + (std::ptrdiff_t)(object.at + HeadingBytes));

			made.insert(made.end(), inside.begin(), inside.end());

			int32_t const newSize = (int32_t)made.size();

			memcpy(made.data() + 8, &newSize, sizeof(newSize));
			image.erase(image.begin() + (std::ptrdiff_t)object.at, image.begin() + (std::ptrdiff_t)(object.at + object.size));
			image.insert(image.begin() + (std::ptrdiff_t)object.at, made.begin(), made.end());

			int32_t const levelSize = (int32_t)image.size();

			memcpy(image.data() + 8, &levelSize, sizeof(levelSize));
			return FromLoader(file->ReplaceLevel(image));
		}
	}

	char const *TrackEditWords(TrackEditResult result)
	{
		switch (result)
		{
		case TrackEditResult::Done: return "done";
		case TrackEditResult::NothingOpen: return "no level is open";
		case TrackEditResult::NoName: return "the object has no name to look for";
		case TrackEditResult::NoSuchObject: return "the level has no object with that name";
		case TrackEditResult::NoTrack: return "Object doesn't have a track";
		case TrackEditResult::ClipboardEmpty: return "Use copy track first";
		case TrackEditResult::CantBuild: return "the level could not be put together";
		case TrackEditResult::LoaderRefused: return "the game would not take the level that way, so nothing was changed";
		}

		return "the track could not be changed";
	}

	TrackEditResult TrackCopy(OpenLevelFile *file, uint8_t const *id, TrackClip *clip)
	{
		try
		{
			return clip ? Copy(file, id, clip) : TrackEditResult::ClipboardEmpty;
		}
		catch (std::exception const &)
		{
			return TrackEditResult::CantBuild;
		}
	}

	TrackEditResult TrackRemove(OpenLevelFile *file, uint8_t const *id)
	{
		try
		{
			return Paste(file, id, nullptr);
		}
		catch (std::exception const &)
		{
			return TrackEditResult::CantBuild;
		}
	}

	namespace
	{
		// The object's place and turn in the level (OBJHEAD1's location and orientation): the default track of an object that has none stands there
		bool PlaceOfObject(OpenLevelFile *file, uint8_t const *id, int32_t place[3], float quat[4])
		{
			File_Chunk *const level = file ? file->Level() : nullptr;

			if (!level || !id)
			{
				return false;
			}

			List<Object_Chunk *> objects;

			level->list_objects(&objects);

			for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
			{
				Object_Chunk *const object = each();

				if (object && memcmp(&object->object_data.ID, id, 8) == 0)
				{
					place[0] = object->object_data.location.x;
					place[1] = object->object_data.location.y;
					place[2] = object->object_data.location.z;
					quat[0] = object->object_data.orientation.x;
					quat[1] = object->object_data.orientation.y;
					quat[2] = object->object_data.orientation.z;
					quat[3] = object->object_data.orientation.w;
					return true;
				}
			}

			return false;
		}

		bool Mild(uint8_t const *bytes, size_t length)
		{
			return bytes && length >= HeadingBytes;
		}
	}

	bool TrackObjectPlace(OpenLevelFile *file, uint8_t const *id, int32_t place[3], float quat[4])
	{
		try
		{
			return PlaceOfObject(file, id, place, quat);
		}
		catch (std::exception const &)
		{
			return false;
		}
	}

	namespace
	{
		TrackRebaseFn rebaseFn = nullptr;
	}

	void TrackEdit_UseRebase(TrackRebaseFn rebase)
	{
		rebaseFn = rebase;
	}

	TrackEditResult TrackLoad(OpenLevelFile *file, uint8_t const *id, TrackLoaded *loaded)
	{
		try
		{
			TrackClip clip;
			TrackEditResult const copied = TrackCopy(file, id, &clip);
			TrackLoaded made;

			if (copied == TrackEditResult::NoTrack)
			{
				int32_t place[3];
				float quat[4];

				if (!PlaceOfObject(file, id, place, quat))
				{
					return TrackEditResult::NoSuchObject;
				}

				made.had = false;
				made.track = TrackData_Default(place, quat);
				*loaded = made;
				return TrackEditResult::Done;
			}

			if (copied != TrackEditResult::Done)
			{
				return copied;
			}

			if (!Mild((uint8_t const *)clip.track.data(), clip.track.size()) || !TrackData_Parse((uint8_t const *)clip.track.data() + HeadingBytes, clip.track.size() - HeadingBytes, &made.track))
			{
				return TrackEditResult::CantBuild;
			}

			made.had = true;

			// A track with no sections has nowhere to go (the game's own loader skips it: object_setup.cpp:257); inteng starts it as an object with no track
			if (made.track.sections.empty())
			{
				int32_t place[3];
				float quat[4];

				if (!PlaceOfObject(file, id, place, quat))
				{
					return TrackEditResult::NoSuchObject;
				}

				TrackData const flagsKept = made.track;

				made.track = TrackData_Default(place, quat);
				made.track.flags = flagsKept.flags;
				made.track.timerStart = flagsKept.timerStart;
				made.had = false;
			}

			for (Bytes const &chunk : clip.sounds)
			{
				TrackSound sound;

				if (Mild((uint8_t const *)chunk.data(), chunk.size()) && TrackSound_Parse((uint8_t const *)chunk.data() + HeadingBytes, chunk.size() - HeadingBytes, &sound))
				{
					made.sounds.push_back(sound);
				}
			}

			*loaded = made;
			return TrackEditResult::Done;
		}
		catch (std::exception const &)
		{
			return TrackEditResult::CantBuild;
		}
	}

	namespace
	{
		Bytes WholeChunk(char const *name, std::vector<uint8_t> const &payload)
		{
			Bytes chunk(HeadingBytes + payload.size());

			memcpy(chunk.data(), name, 8);

			int32_t const size = (int32_t)chunk.size();

			memcpy(chunk.data() + 8, &size, 4);

			if (!payload.empty())
			{
				memcpy(chunk.data() + HeadingBytes, payload.data(), payload.size());
			}

			return chunk;
		}
	}

	std::string TrackLevelSoundDirectory(OpenLevelFile *file)
	{
		File_Chunk *const level = file ? file->Level() : nullptr;

		if (!level)
		{
			return std::string();
		}

		Chunk *const env = level->lookup_single_child("REBENVDT");
		Chunk_With_Children *const envChildren = dynamic_cast<Chunk_With_Children *>(env);
		Chunk *const dir = envChildren ? envChildren->lookup_single_child("SOUNDDIR") : nullptr;
		Sound_Directory_Chunk *const directory = dynamic_cast<Sound_Directory_Chunk *>(dir);

		return directory && directory->directory ? std::string(directory->directory) : std::string();
	}

	std::vector<char> TrackChunkBytes(char const *name, std::vector<uint8_t> const &payload)
	{
		return WholeChunk(name, payload);
	}

	bool TrackRebasedOnObject(OpenLevelFile *file, uint8_t const *id, TrackData *track)
	{
		int32_t place[3];
		float quat[4];

		return rebaseFn && track && !track->sections.empty() && PlaceOfObject(file, id, place, quat) && rebaseFn(track, place, quat);
	}

	TrackEditResult TrackSetObjectPose(OpenLevelFile *file, uint8_t const *id, int32_t const place[3], float const quat[4])
	{
		try
		{
			Bytes image;
			Piece object = { 0, 0 };
			TrackEditResult const found = Find(file, id, &image, &object);

			if (found != TrackEditResult::Done)
			{
				return found;
			}

			for (size_t at = object.at + HeadingBytes; at < object.at + object.size; )
			{
				size_t size = 0;

				if (!ChunkSizeAt(image, at, object.at + object.size, &size))
				{
					return TrackEditResult::CantBuild;
				}

				if (NameIs(image, at, "OBJHEAD1"))
				{
					if (size < HeadingBytes + 48)
					{
						return TrackEditResult::CantBuild;
					}

					memcpy(image.data() + at + HeadingBytes + 20, place, 12);
					memcpy(image.data() + at + HeadingBytes + 32, quat, 16);
					return FromLoader(file->ReplaceLevel(image));
				}

				at += size;
			}

			return TrackEditResult::CantBuild;
		}
		catch (std::exception const &)
		{
			return TrackEditResult::CantBuild;
		}
	}

	TrackEditResult TrackStore(OpenLevelFile *file, uint8_t const *id, TrackLoaded const &loaded)
	{
		try
		{
			TrackClip clip;
			std::vector<uint8_t> payload;

			TrackData_Bytes(loaded.track, &payload);
			clip.track = WholeChunk("OBJTRAK2", payload);

			for (TrackSound const &sound : loaded.sounds)
			{
				std::vector<uint8_t> bytes;

				TrackSound_Bytes(sound, &bytes);
				clip.sounds.push_back(WholeChunk("TRAKSOUN", bytes));
			}

			return Paste(file, id, &clip);
		}
		catch (std::exception const &)
		{
			return TrackEditResult::CantBuild;
		}
	}

	bool TrackExists(OpenLevelFile *file, uint8_t const *id)
	{
		try
		{
			TrackClip scratch;

			return Copy(file, id, &scratch) == TrackEditResult::Done;   // the object holds an OBJTRAK2 (Copy Track would take it)
		}
		catch (std::exception const &)
		{
			return false;
		}
	}

	std::vector<TrackObjectId> TrackObjectsWithTracks(OpenLevelFile *file)
	{
		std::vector<TrackObjectId> found;
		File_Chunk *const level = file ? file->Level() : nullptr;

		if (!level)
		{
			return found;
		}

		List<Object_Chunk *> objects;

		level->list_objects(&objects);

		// FUN_004efd50 0x4efda4-0x4efe46: the list in its order; an object counts when its level object holds an OBJTRAK2 (FUN_00431a00 "OBJTRAK2" on it)
		for (LIF<Object_Chunk *> each(&objects); !each.done(); each.next())
		{
			Object_Chunk *const object = each();

			if (object && object->lookup_single_child("OBJTRAK2"))
			{
				TrackObjectId id;

				memcpy(id.bytes, &object->object_data.ID, sizeof(id.bytes));
				found.push_back(id);
			}
		}

		return found;
	}

	TrackEditResult TrackPasteRelative(OpenLevelFile *file, uint8_t const *id, TrackClip const &clip)
	{
		try
		{
			TrackEditResult const target = PasteTargetCheck(file, id);

			if (target != TrackEditResult::Done)
			{
				return target;
			}

			if (clip.track.size() < HeadingBytes || memcmp(clip.track.data(), "OBJTRAK2", 8) != 0)
			{
				return TrackEditResult::ClipboardEmpty;
			}

			TrackData track;
			int32_t place[3];
			float quat[4];

			if (!rebaseFn || !TrackData_Parse((uint8_t const *)clip.track.data() + HeadingBytes, clip.track.size() - HeadingBytes, &track) || track.sections.empty())
			{
				return TrackEditResult::CantBuild;
			}

			if (!PlaceOfObject(file, id, place, quat))
			{
				return TrackEditResult::NoSuchObject;
			}

			if (!rebaseFn(&track, place, quat))
			{
				return TrackEditResult::CantBuild;
			}

			TrackClip moved;
			std::vector<uint8_t> payload;

			TrackData_Bytes(track, &payload);
			moved.track = WholeChunk("OBJTRAK2", payload);
			moved.sounds = clip.sounds;
			return Paste(file, id, &moved);
		}
		catch (std::exception const &)
		{
			return TrackEditResult::CantBuild;
		}
	}

	TrackEditResult TrackPasteAbsolute(OpenLevelFile *file, uint8_t const *id, TrackClip const &clip)
	{
		try
		{
			TrackEditResult const target = PasteTargetCheck(file, id);

			if (target != TrackEditResult::Done)
			{
				return target;
			}

			return Paste(file, id, &clip);
		}
		catch (std::exception const &)
		{
			return TrackEditResult::CantBuild;
		}
	}
}
